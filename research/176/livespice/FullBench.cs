using System.Diagnostics;
using Circuit;
using ComputerAlgebra;

internal static class FullBench
{
    public static void Run(int sampleRate, int blockSize, int oversample, double peak, string[] options)
    {
        var model = new FrontEnd();
        double Read(string name, double fallback)
        {
            string prefix = name + "=";
            string option = options.SingleOrDefault(value => value.StartsWith(prefix, StringComparison.Ordinal));
            return option == null ? fallback : double.Parse(option[prefix.Length..]);
        }

        var settings = new Settings(
            Input: Math.Pow(10, Read("input-db", 20) / 20),
            Output: Math.Pow(10, Read("output-db", 0) / 20),
            InputVernier: Read("input-vernier", 0),
            OutputVernier: Read("output-vernier", 0),
            Ratio: checked((int)Read("ratio", 2)),
            Attack: Read("attack", .5), Release: Read("release", .8),
            Limiting: !options.Contains("off"), Powered: options.Contains("powered"));
        Console.WriteLine($"Full loop {settings.Ratio}:1, {oversample}x, limiting {(settings.Limiting ? "on" : "off")}, PSU {(settings.Powered ? "rectified" : "averaged")}");
        peak *= Math.Pow(10, (Read("level", 4) - 4) / 20);
        const int toneHz = 1000;
        var elapsed = Stopwatch.StartNew();
        var analysis = model.Build(settings);
        Console.WriteLine($"Analysis assembled in {elapsed.Elapsed.TotalSeconds:F3} s");
        ILog log = Environment.GetEnvironmentVariable("UA176_TRACE") != null ? new ConsoleLog() : new NullLog();
        var solution = TransientSolution.Solve(analysis, (Real)1 / (sampleRate * oversample), log);
        Console.WriteLine($"Symbolic solution in {elapsed.Elapsed.TotalSeconds:F3} s");
        var outputs = new[] { model.Output, model.ControlVoltage, model.BPlus, model.Regulated, model.LinePlate };
        var solver = new Simulation(solution)
        {
            Input = new[] { model.Drive },
            Output = outputs,
            Iterations = 30,
            Oversample = oversample
        };
        var input = new double[blockSize];
        var result = outputs.Select(_ => new double[blockSize]).ToArray();
        var times = new List<double>();
        var window = sampleRate / 5;
        var traces = outputs.Select(_ => new double[window]).ToArray();
        var totalBlocks = (int)Math.Ceiling(5.2 * sampleRate / blockSize);
        for (int b = 0, n = 0; b < totalBlocks; b++, n += blockSize)
        {
            for (int i = 0; i < blockSize; i++)
                input[i] = peak * Math.Sin(2 * Math.PI * toneHz * (n + i) / sampleRate);
            var start = Stopwatch.GetTimestamp();
            solver.Run(input, result);
            var milliseconds = Stopwatch.GetElapsedTime(start).TotalMilliseconds;
            if (b == 0)
                Console.WriteLine($"First block ready at {elapsed.Elapsed.TotalSeconds:F3} s");
            if (b >= 64)
                times.Add(milliseconds);
            for (int i = 0; i < blockSize; i++)
                for (int j = 0; j < result.Length; j++)
                    traces[j][(n + i) % window] = result[j][i];
        }
        if (traces.Any(probe => probe.Any(value => !double.IsFinite(value))))
            throw new InvalidOperationException("Non-finite LiveSPICE output.");
        times.Sort();
        Console.WriteLine($"Mono: mean {times.Average():F3} ms, p99 {times[(int)Math.Ceiling(times.Count * .99) - 1]:F3} ms, max {times[^1]:F3} ms per {blockSize}");
        Console.WriteLine($"Blocks over deadline: {times.Count(t => t > blockSize * 1000.0 / sampleRate)} / {times.Count}");
        var fundamental = Harmonic(traces[0], toneHz, sampleRate);
        double power = Enumerable.Range(2, 19).Sum(k => Math.Pow(Harmonic(traces[0], k * toneHz, sampleRate), 2));
        double thd = 100 * Math.Sqrt(power) / fundamental;
        Console.WriteLine($"Output RMS {Math.Sqrt(traces[0].Average(v => v * v)):F6} V, THD K2–K20 {thd:F5}%, CV {traces[1].Average():F6} V");
        Console.WriteLine($"B+ {traces[2].Average():F6} V, regulated {traces[3].Average():F6} V, V3A plate {traces[4].Average():F6} V");
    }

    private static double Harmonic(double[] signal, double frequency, int rate)
    {
        double cosine = 0, sine = 0;
        for (int i = 0; i < signal.Length; i++)
        {
            double phase = 2 * Math.PI * frequency * i / rate;
            cosine += signal[i] * Math.Cos(phase);
            sine += signal[i] * Math.Sin(phase);
        }
        return Math.Sqrt(cosine * cosine + sine * sine) * 2 / signal.Length;
    }
}
