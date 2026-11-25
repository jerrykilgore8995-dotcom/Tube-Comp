using Circuit;
using ComputerAlgebra;

internal sealed record Settings(double Input = 10, double Output = 1,
    double InputVernier = 0, double OutputVernier = 0, int Ratio = 2,
    double Attack = .5, double Release = .8, bool Limiting = true, bool Powered = false);

internal sealed partial class FrontEnd
{
    private string outputReturn = "T3_N";
    public Expression Output => Node("OUT_P").V - Node(outputReturn).V;
    public Expression ControlVoltage => Node("CV").V;
    public Expression BPlus => Node("T3_CT").V;
    public Expression Regulated => Node("REG").V;
    public Expression LinePlate => Node("V3PA").V;

    public Analysis Build(Settings settings) => Build(0, settings);

    private void V(string name, string a, string b, Expression value) =>
        VoltageSource.Analyze(analysis, name, Node(a), Node(b), value);

    private void Pad(string a, string b, string c, string d, string name, double ratio)
    {
        var arm = Math.Max(.01, 300 * (ratio - 1) / (ratio + 1));
        AdjustableR(a, name + "_P", arm, name + "Arm");
        AdjustableR(b, name + "_N", arm, name + "Arm");
        AdjustableR(name + "_P", c, arm, name + "Arm");
        AdjustableR(name + "_N", d, arm, name + "Arm");
        AdjustableR(name + "_P", name + "_N",
            1200 * ratio / Math.Max(1e-9, ratio * ratio - 1), name + "Shunt");
    }

    private static Expression PositivePow(Expression x, double power) =>
        Call.If(x > 0, x ^ power, 0);

    private static Expression Softplus(Expression x) =>
        Call.If(x > 50, x, Call.Ln(1 + Call.Exp(x)));

    private void Current(string name, string a, string b, Expression law,
                         params (Expression Symbol, string Positive, string Negative)[] ports)
    {
        if (!export)
        {
            analysis.AddPassiveComponent(Node(a), Node(b), analysis.AddUnknownEqualTo(name, law));
            return;
        }
        var current = Variable.New(name);
        analysis.AddPassiveComponent(Node(a), Node(b), current);
        var variables = ports.Select((_, i) => (Expression)Variable.New("v" + i)).ToArray();
        law = law.Substitute(ports.Select((p, i) => Arrow.New(p.Symbol, variables[i])));
        devices.Add(new Device(current, ports.Select(p => (Expression)(Node(p.Positive).V - Node(p.Negative).V)).ToArray(),
            law));
    }

    private void GridDiode(string name, string a, string b, double ideality)
    {
        if (!export)
        {
            Diode.Analyze(analysis, "D_" + name, Node(a), Node(b), 1e-9, ideality);
            return;
        }
        var v = (Expression)Variable.New("v0");
        Current("ID_" + name, a, b, 1e-9 * Component.LinExpm1(v / (ideality * (Expression)Component.VT)),
            (v, a, b));
    }

    private void LineTube(string p, string g, string k, bool ax7)
    {
        Expression vp = Node(p).V - Node(k).V;
        Expression vg = Node(g).V - Node(k).V;
        if (!export)
        {
            vp = analysis.AddUnknownEqualTo("VPK_" + p, vp);
            vg = analysis.AddUnknownEqualTo("VGK_" + p, vg);
        }
        else
        {
            vp = Variable.New("v0");
            vg = Variable.New("v1");
        }
        var kp = ax7 ? 600 : 185.4;
        var mu = ax7 ? 100 : 18.02;
        var kvb = ax7 ? 300 : 1247.69;
        var x = kp * (1 / mu + (vg - (ax7 ? 0 : .6206)) * Binary.Power(kvb + vp * vp, -.5));
        var exponent = ax7 ? 1.4 : 1.181;
        var gain = 2 / (ax7 ? 1060 : 945.75);
        Expression plateLaw = export
            ? Call.New("plate", vp, vg, kp / mu, kp, kp, kvb, ax7 ? 0 : -.6206, gain, exponent)
            : gain * PositivePow(vp * Softplus(x) / kp, exponent);
        Current("I_" + p, p, k, plateLaw, (vp, p, k), (vg, g, k));
        R(p, k, 1e9);
        C(g, k, ax7 ? 2.3e-12 : 2.9e-12);
        C(g, p, ax7 ? 2.4e-12 : 2.5e-12);
        C(p, k, ax7 ? .9e-12 : .5e-12);
        if (ax7)
        {
            var junction = g + "_D";
            R(g, junction, 2001);
            GridDiode(g, junction, k, 1);
            C(junction, k, 10e-12);
        }
        else
        {
            Expression denominator = 6.624 + vp;
            Current("IG_" + p, g, k,
                (.00082 + .162 / Call.If(denominator > 1, denominator, 1)) *
                (2 * mu / 945.75) * PositivePow(vg + 1.03, 1.3), (vp, p, k), (vg, g, k));
        }
    }

    private void Supply(bool powered)
    {
        if (powered)
        {
            // Compare with the upstream rectifier/choke topology in a separate bench.
            Expression mains = 460 * Call.Sin(2 * Math.PI * 60 * Component.t);
            V("T4A", "HV_A_OC", "0", mains);
            V("T4B", "0", "HV_B_OC", mains);
            R("HV_A_OC", "HV_A", 100);
            R("HV_B_OC", "HV_B", 100);
            foreach (var a in new[] { "HV_A", "HV_B" })
                Current("I_" + a, a, "RECT", .00331 * PositivePow(Node(a).V - Node("RECT").V, 1.5));
            C("RECT", "0", 20e-6);
            L("RECT", "CHOKE_END", 10);
            R("CHOKE_END", "FILTER", 150);
            C("FILTER", "0", 60e-6);
            Current("I_REG", "REG", "0", PositivePow(Node("REG").V - 107.55, 1) / 33);
        }
        else
        {
            // Warm, averaged replacement for T4, rectifier and choke. The effective
            // upstream source resistance retains load-dependent B+ and sag.
            // Fit to the warmed ngspice supply at 55, 60 and 65 mA.
            V("FILTER_AVERAGE", "SUPPLY_DC", "0", 439.417);
            R("SUPPLY_DC", "FILTER", 678.671);
            C("FILTER", "0", 60e-6);
            V("REG_REFERENCE", "REG_DC", "0", 107.55);
            R("REG", "REG_DC", 33);
        }
        R("FILTER", "T3_CT", 1000);
        C("T3_CT", "0", 30e-6); // C8B and C9B share this node
        R("T3_CT", "REG", 10e3);
    }

    private void LineStage(Settings settings)
    {
        R("NEXT_P", "0", 20e3);
        R("NEXT_N", "0", 20e3);
        LineTube("V2PA", "NEXT_P", "V2KA", true);
        LineTube("V2PB", "NEXT_N", "V2KB", true);
        R("V2KA", "V2_RETURN", 2200);
        R("V2KB", "V2_RETURN", 2200);
        R("V2_RETURN", "0", 2200);
        R("T3_CT", "V2_SUPPLY", 47e3);
        C("V2_SUPPLY", "0", 10e-6);
        foreach (var side in new[] { "A", "B" })
        {
            R("V3P" + side, "FEEDBACK_" + side, 180e3);
            R("FEEDBACK_" + side, "0", 180e3);
            C("FEEDBACK_" + side, "V2K" + side, .47e-6);
            R("V2P" + side, "V2_SUPPLY", 220e3);
            C("V2P" + side, "V3G" + side, .1e-6);
            R("V3G" + side, "0", 1.2e6);
            LineTube("V3P" + side, "V3G" + side, "V3K", false);
        }
        R("V3K", "0", 330);

        R("V3PA", "PLEAKA", 525);
        R("V3PB", "PLEAKB", 525);
        L("PLEAKA", "HALF_A", .05);
        L("PLEAKB", "HALF_B", .05);
        var plus = new[] { "HALF_A", "T3_CT", "OUT_RAW" };
        var minus = new[] { "T3_CT", "HALF_B", "T3_N" };
        var inductances = new[] { 62.5, 62.5, 250.0 / 81 };
        var currents = inductances.Select((_, i) => analysis.AddUnknown("IT3_" + i)).ToArray();
        for (var i = 0; i < currents.Length; i++)
        {
            analysis.AddPassiveComponent(Node(plus[i]), Node(minus[i]), currents[i]);
            Expression flux = 0;
            for (var j = 0; j < currents.Length; j++)
                flux += (i == j ? inductances[i] : .99999 * Math.Sqrt(inductances[i] * inductances[j])) *
                    Call.D(currents[j], Component.t);
            analysis.AddEquation(Node(plus[i]).V - Node(minus[i]).V, flux);
        }
        R("HALF_A", "HALF_B", 500e3);
        R("OUT_RAW", "T3_P", 1);
        C("T3_P", "T3_N", 80e-12);
        AdjustableR("T3_P", "OUT_FEED", Math.Max(.01, 300 * settings.OutputVernier), "outVernier");
        if (settings.Output > 1 || export)
        {
            outputReturn = "OUT_N";
            Pad("OUT_FEED", "T3_N", "OUT_P", outputReturn, "OUTPUT", settings.Output);
        }
        else
            R("OUT_FEED", "OUT_P", .01);
        R("OUT_P", outputReturn, 600);
        V("OUT_REF", "T3_N", "0", 0);
    }

    private void Detector(Settings settings)
    {
        var (tap, bias) = settings.Ratio switch
        {
            2 => (.12, 1000.0),
            4 => (.29, 6800.0),
            8 => (.58, 20e3),
            12 => (1.0, 47e3),
            _ => throw new ArgumentOutOfRangeException(nameof(settings.Ratio))
        };
        Expression selectedTap = export ? Parameter("tap") : tap;
        foreach (var side in new[] { "A", "B" })
        {
            var plate = "V3P" + side;
            var terminal = "TAP_" + side;
            var k = "DET_K" + side;
            var current = analysis.AddUnknown("ITAP_" + side);
            analysis.AddPassiveComponent(Node(terminal), Node("T3_CT"), current);
            analysis.AddPassiveComponent(Node(plate), Node("T3_CT"), -selectedTap * current);
            analysis.AddEquation(Node(terminal).V - Node("T3_CT").V, selectedTap * (Node(plate).V - Node("T3_CT").V));
            C(terminal, k, .5e-6);
            R(k, "DET_BIAS", 220e3);
            DetectorDiode(side, k);
        }
        R("REG", "DET_BIAS", 82e3);
        AdjustableR("DET_BIAS", "0", bias, "ratioBias");
        C("DET_BIAS", "0", 20e-6);
        AdjustableR("CV", "ATK_IN", settings.Limiting ? .001 : 1e12, "limitOn");
        AdjustableR("CV", "0", settings.Limiting ? 1e12 : .001, "limitOff");
        R("ATK_IN", "DET_A", 10e3);
        AdjustableR("ATK_IN", "DET_A", Math.Max(.001, 25e3 * (1 - settings.Attack)), "attack");
        AdjustableR("DET_A", "REL_WIPER", Math.Max(.001, 5e6 * (1 - settings.Release)), "releaseUpper");
        AdjustableR("REL_WIPER", "0", Math.Max(.001, 5e6 * settings.Release), "releaseLower");
        R("REL_WIPER", "0", 270e3);
    }

    private void DetectorDiode(string name, string k)
    {
        if (export)
        {
            var v = (Expression)Variable.New("v0");
            Current("ID_" + name, "DET_A", k, Call.New("detector", v), (v, "DET_A", k));
            C("DET_A", k, 3.2e-12);
            return;
        }
        // The voltage across the exponential junction is the Newton unknown.
        // This parametrization avoids the reverse-current logarithm singularity.
        var vj = analysis.AddUnknown("VD_" + name);
        Expression scaled = vj / .081471;
        var expm1 = Call.If(scaled < 40, Call.Exp(scaled) - 1, Math.Exp(40) * (1 + scaled - 40) - 1);
        Expression law = 66e-12 * expm1 + (vj - .867) / 12.7e9;
        var current = analysis.AddUnknownEqualTo("ID_" + name, law);
        var x = law / .0032;
        var drop = Call.If(x > 0, ((x + 1e-9) ^ .775) - Math.Pow(1e-9, .775), 0);
        analysis.AddEquation(Node("DET_A").V - Node(k).V, vj - .867 + drop);
        analysis.AddPassiveComponent(Node("DET_A"), Node(k), current);
        C("DET_A", k, 3.2e-12);
    }
}
