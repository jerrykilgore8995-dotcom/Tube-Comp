using Circuit;
using System.Globalization;
using ComputerAlgebra;
using Expr = ComputerAlgebra.Expression;

internal sealed partial class FrontEnd
{
    private static string Number(double value) => value.ToString("R", CultureInfo.InvariantCulture);

    private static string Cxx(Expr expression, IReadOnlyDictionary<Expr, string> symbols)
    {
        if (symbols.TryGetValue(expression, out var symbol))
            return symbol;
        if (expression is Constant constant)
            return Number((double)constant);
        if (expression is Power power)
        {
            var a = Cxx(power.Left, symbols);
            return power.Right.Equals(-.5) ? $"(1/std::sqrt({a}))"
                : power.Right.Equals(.5) ? $"std::sqrt({a})"
                : power.Right.Equals(2) ? $"square({a})"
                : $"std::pow({a},{Cxx(power.Right, symbols)})";
        }
        if (expression is Sum sum)
            return "(" + string.Join("+", sum.Terms.Select(term => Cxx(term, symbols))) + ")";
        if (expression is Product product)
            return "(" + string.Join("*", product.Terms.Select(term => Cxx(term, symbols))) + ")";
        if (expression is Unary unary && unary.Operator == Operator.Negate)
            return $"(-{Cxx(unary.Operand, symbols)})";
        if (expression is Binary binary)
        {
            var op = binary.Operator switch
            {
                Operator.Greater => ">", Operator.GreaterEqual => ">=", Operator.Less => "<",
                Operator.LessEqual => "<=", Operator.Divide => "/", Operator.Subtract => "-",
                Operator.Add => "+", Operator.Multiply => "*", _ => throw new NotSupportedException(binary.ToString())
            };
            return $"({Cxx(binary.Left, symbols)}{op}{Cxx(binary.Right, symbols)})";
        }
        if (expression is Call call)
        {
            var args = call.Arguments.Select(arg => Cxx(arg, symbols)).ToArray();
            return call.Target.Name switch
            {
                "If" => $"(({args[0]})?({args[1]}):({args[2]}))",
                "Ln" => $"std::log({args[0]})",
                "Exp" => $"std::exp({args[0]})",
                _ => throw new NotSupportedException(call.ToString())
            };
        }
        throw new NotSupportedException(expression.ToString());
    }

    public void Export(string file)
    {
        var compiled = Build(new Settings(Output: 3.16227766));
        var x = compiled.Unknowns.Distinct().ToArray();
        var derivatives = x.Select(v => Call.D(v, Component.t)).ToArray();
        var input = devices.Select(d => d.Current).Prepend(Drive).ToArray();
        var basis = x.Concat(derivatives).Concat(input).ToArray();
        var equations = compiled.Equations.Select(eq => LinearCombination.New(basis, eq.Left - eq.Right)).ToArray();
        if (equations.Length != x.Length)
            throw new InvalidOperationException($"Expected square circuit matrix, saw {equations.Length}x{x.Length}.");
        var controlSymbols = parameters.ToDictionary(p => p, p => $"p.{p}");
        using var writer = new StreamWriter(file);
        writer.WriteLine("// Generated from the LiveSPICE 176 circuit. Do not edit.");
        writer.WriteLine($"constexpr int nodeCount = {x.Length}, deviceCount = {devices.Count};");
        var states = Enumerable.Range(0, x.Length).Where(i => equations.Any(eq => !eq[derivatives[i]].EqualsZero())).ToArray();
        writer.WriteLine($"constexpr int stateCount = {states.Length};");
        writer.WriteLine($"constexpr int stateIndices[] = {{ {string.Join(", ", states)} }};");
        writer.WriteLine("struct ElectricalControls {");
        foreach (var p in parameters)
            writer.WriteLine($"    double {p};");
        writer.WriteLine("};");
        writer.WriteLine("void stamp(const ElectricalControls& p, double* a, double* d, double* b) {");
        for (int i = 0; i < equations.Length; i++)
        {
            var eq = equations[i];
            for (int j = 0; j < x.Length; j++)
                if (!eq[x[j]].EqualsZero())
                    writer.WriteLine($"    a[{i} * nodeCount + {j}] = {Cxx(eq[x[j]], controlSymbols)};");
            for (int j = 0; j < states.Length; j++)
                if (!eq[derivatives[states[j]]].EqualsZero())
                    writer.WriteLine($"    d[{i} * stateCount + {j}] = {Cxx(eq[derivatives[states[j]]], controlSymbols)};");
            for (int j = 0; j < input.Length; j++)
                if (!eq[input[j]].EqualsZero())
                    writer.WriteLine($"    b[{i} * (deviceCount + 2) + {j + 1}] = {Cxx(-eq[input[j]], controlSymbols)};");
            if (!eq[1].EqualsZero())
                writer.WriteLine($"    b[{i} * (deviceCount + 2)] = {Cxx(-eq[1], controlSymbols)};");
        }
        writer.WriteLine("}");
        var voltages = devices.SelectMany(d => d.Voltages).Concat(new[] { Output, ControlVoltage, BPlus, Regulated }).ToArray();
        writer.WriteLine($"constexpr int portCount = {voltages.Length};");
        writer.WriteLine("void ports(double* z) {");
        for (int i = 0; i < voltages.Length; i++)
        {
            var port = LinearCombination.New(x, voltages[i]);
            for (int j = 0; j < x.Length; j++)
                if (!port[x[j]].EqualsZero())
                    writer.WriteLine($"    z[{i} * nodeCount + {j}] = {Cxx(port[x[j]], controlSymbols)};");
        }
        writer.WriteLine("}");
        var index = 0;
        writer.WriteLine("template<bool derivative> void devicesAt(const double* v, double* out) {");
        foreach (var (device, row) in devices.Select((d, i) => (d, i)))
        {
            var symbols = device.Voltages.Select((_, j) => (Expr)Variable.New("v" + j)).ToArray();
            var replacements = symbols.Select((s, j) => (s, v: $"p{index + j}")).ToDictionary(p => p.s, p => p.v);
            for (int j = 0; j < symbols.Length; j++)
                writer.WriteLine($"    const double p{index + j} = v[{index + j}];");
            if (device.Law is Call call && call.Target.Name == "detector")
            {
                writer.WriteLine($"    double a{row}, b{row}; detector(p{index}, a{row}, b{row});");
                writer.WriteLine($"    if constexpr (derivative) out[{index}] = b{row}; else out[{row}] = a{row};");
            }
            else if (device.Law is Call plate && plate.Target.Name == "plate")
            {
                var args = string.Join(", ", plate.Arguments.Select(a => Cxx(a, replacements)));
                writer.WriteLine($"    plateLaw<derivative>({args}, out + (derivative ? {index} : {row}));");
            }
            else
            {
                writer.WriteLine("    if constexpr (derivative) {");
                for (int j = 0; j < symbols.Length; j++)
                    writer.WriteLine($"        out[{index + j}] = {Cxx(device.Law.Differentiate(symbols[j]), replacements)};");
                writer.WriteLine($"    }} else out[{row}] = {Cxx(device.Law, replacements)};");
            }
            index += symbols.Length;
        }
        writer.WriteLine("}");
        writer.WriteLine("constexpr int deviceOffsets[] = { 0, " +
            string.Join(", ", devices.Select((_, i) => devices.Take(i + 1).Sum(d => d.Voltages.Length))) + " };");
    }
}
