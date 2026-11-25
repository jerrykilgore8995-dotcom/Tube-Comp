using Circuit;
using ComputerAlgebra;

internal sealed partial class FrontEnd
{
    private readonly Dictionary<string, Node> nodes = new();
    private readonly Analysis analysis = new();
    private readonly bool export;
    public FrontEnd(bool export = false) => this.export = export;
    private readonly List<Device> devices = new();
    private readonly List<Expression> parameters = new();
    private sealed record Device(Expression Current, Expression[] Voltages, Expression Law);

    public readonly Expression Drive = Component.DependentVariable("drive", Component.t);
    public Expression Plates => Node("PB").V - Node("PA").V;
    public Expression Following => Node("NEXT_N").V - Node("NEXT_P").V;
    public Expression PlateA => Node("PA").V;
    public Expression CathodeA => Node("KA").V;
    public Expression GridA => Node("GP").V;

    private Node Node(string name)
    {
        if (nodes.TryGetValue(name, out var node))
            return node;
        node = new Node { Name = name == "0" ? "GND" : name };
        nodes.Add(name, node);
        analysis.DeclNodes(node);
        return node;
    }

    private void R(string a, string b, double value) =>
        Resistor.Analyze(analysis, Node(a), Node(b), value);
    private void AdjustableR(string a, string b, double resistance, string name)
    {
        if (!export)
        {
            R(a, b, resistance);
            return;
        }
        var conductance = Parameter(name);
        analysis.AddPassiveComponent(Node(a), Node(b), conductance * (Node(a).V - Node(b).V));
    }

    private Expression Parameter(string name)
    {
        var value = Variable.New(name);
        if (!parameters.Contains(value))
            parameters.Add(value);
        return value;
    }

    private void C(string a, string b, double value)
    {
        var va = a == "0" ? (Expression)0 : Node(a).V;
        var vb = b == "0" ? (Expression)0 : Node(b).V;
        analysis.AddPassiveComponent(Node(a), Node(b),
            value * (Call.D(va, Component.t) - Call.D(vb, Component.t)));
    }
    private void L(string a, string b, double value) =>
        Inductor.Analyze(analysis, Node(a), Node(b), value);

    private void Transformer(string ip, string im, string op, string om, string name,
                             double n, double rp, double rs, double lm, double ll,
                             double rc, double cs, double cw)
    {
        string p = name + "_P";
        string m = name + "_M";
        string pp = name + "_PP";
        string sp = name + "_SP";
        string sm = name + "_SM";
        R(ip, p, rp / 2);
        R(im, m, rp / 2);
        L(p, pp, ll);
        L(pp, m, lm);
        R(pp, m, rc);

        // Equal and opposite winding power, including reflected loading.
        var i = analysis.AddUnknown(name + "_I");
        analysis.AddPassiveComponent(Node(sp), Node(sm), i);
        analysis.AddPassiveComponent(Node(pp), Node(m), -n * i);
        analysis.AddEquation(Node(sp).V - Node(sm).V, n * (Node(pp).V - Node(m).V));

        R(sp, op, rs / 2);
        R(sm, om, rs / 2);
        C(op, om, cs);
        C(pp, op, cw);
        C(m, om, cw);
    }

    private void Tube6BC8(string plate, string grid, string cathode, bool fullCircuit)
    {
        Expression vp = Node(plate).V - Node(cathode).V;
        Expression vg = Node(grid).V - Node(cathode).V;
        if (fullCircuit && !export)
        {
            vp = analysis.AddUnknownEqualTo("VPK_" + plate, vp);
            vg = analysis.AddUnknownEqualTo("VGK_" + plate, vg);
        }
        if (export)
        {
            vp = Variable.New("v0");
            vg = Variable.New("v1");
        }
        var x = 1.794225622 * (1 + 54.16463004 * vg * Binary.Power(922.2704089 + vp * vp, -.5));
        var softplus = Call.If(x > 50, x, Call.Ln(1 + Call.Exp(x)));
        var e = vp * softplus / 1.794225622;
        var law = export
            ? Call.New("plate", vp, vg, 1.794225622, 1.794225622 * 54.16463004,
                1.794225622, 922.2704089, 0, 20.19891536e-6, 1.439822572)
            : Call.If(e > 0, 20.19891536e-6 * (e ^ 1.439822572), 0);
        Current("I_" + plate, plate, cathode, law, (vp, plate, cathode), (vg, grid, cathode));
        R(plate, cathode, 1e9);
        C(grid, cathode, 3.9e-12);
        C(grid, plate, 3.7e-12);
        C(plate, cathode, 1.8e-12);

        // LiveSPICE's Shockley diode, with the deck's external series resistance
        // and a fixed zero-bias junction capacitance for this first bench.
        var junction = grid + "_D";
        R(grid, junction, 2000);
        GridDiode(grid, junction, cathode, 1.5);
        C(junction, cathode, 1e-12);
    }

    public Analysis Build(double bias) => Build(bias, null);

    private Analysis Build(double bias, Settings settings)
    {
        analysis.PushContext(null);
        Ground.Analyze(analysis, Node("0"));
        VoltageSource.Analyze(analysis, "VDRIVE", Node("DRIVEP"), Node("DRIVEN"), Drive,
            Arrow.New(Drive.Evaluate(Component.t, 0), 0));
        if (settings == null)
        {
            V("VBPLUS", "REG", "0", 108);
            V("VCV", "CV", "0", bias);
        }
        else
        {
            Supply(settings.Powered);
        }
        R("DRIVEN", "0", 1e9);
        R("DRIVEP", "INP", 300);
        R("DRIVEN", "INN", 300);
        Pad("INP", "INN", export ? "TP" : "PAD_P", "TN", "INPUT", settings?.Input ?? 10);
        if (!export)
            R("PAD_P", "TP", settings == null ? 1e-6 : Math.Max(.01, 300 * settings.InputVernier));
        Transformer("TP", "TN", "SEC_P", "SEC_N", "T1", 10, 52, 3900, 3.3, .002, 100e3, 92e-12, 15e-12);

        R("SEC_P", "TAP_P", 10e3);
        R("TAP_P", "CV", 6800);
        R("CV", "TAP_N", 6800);
        R("TAP_N", "SEC_N", 10e3);
        R("SEC_P", "GP", .001);
        R("SEC_N", "GN", .001);
        R("TAP_P", "GP", 1e12);
        R("TAP_N", "GN", 1e12);
        C("CV", "0", .1e-6);

        Tube6BC8("PA", "GP", "KA", settings != null);
        Tube6BC8("PB", "GN", "KB", settings != null);
        R("KA", "KRETURN", 50);
        R("KB", "KRETURN", 50);
        R("KA", "KRETURN", 47);
        R("KB", "KRETURN", 47);
        R("KRETURN", "0", 330);
        R("KRETURN", "METER", 500);
        R("METER", "R11TOP", 500);
        R("R11TOP", "0", 330);

        C("REG", "0", .1e-6);

        R("PA", "PLATE_BAL_A", 4700);
        R("PLATE_BAL_A", "REG", 500);
        R("REG", "PLATE_BAL_B", 500);
        R("PLATE_BAL_B", "PB", 4700);
        Transformer("PA", "PB", "NEXT_P", "NEXT_N", "T2", 1.0 / 2.3, 10e3, 2900,
                    346, .10, 400e3, 60e-12, 15e-12);

        if (settings == null)
        {
            R("NEXT_P", "NEXT_RETURN", 20e3);
            R("NEXT_N", "NEXT_RETURN", 20e3);
            C("NEXT_P", "NEXT_RETURN", 7e-12);
            C("NEXT_N", "NEXT_RETURN", 7e-12);
            R("NEXT_RETURN", "0", .001);
        }
        else
        {
            LineStage(settings);
            Detector(settings);
        }
        analysis.PopContext();
        return analysis;
    }
}
