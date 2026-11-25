using System.Diagnostics;
using System.Globalization;
using Circuit;
using ComputerAlgebra;

CultureInfo.CurrentCulture = CultureInfo.InvariantCulture;
const int sampleRate = 44100;
const int blockSize = 512;
const int toneHz = 1000;
const int warmupBlocks = 40;
const int measuredBlocks = 256;
const int window = sampleRate / 5;
var peak = 2 * .775 * Math.Sqrt(2) * Math.Pow(10, 4.0 / 20); // +4 dBu into a matched 600 ohm load
var oversample = args.Length == 0 ? 1 : int.Parse(args[0]);
if (oversample is < 1 or > 4)
    throw new ArgumentOutOfRangeException(nameof(oversample));

if (args.Length > 1 && args[1] == "full")
{
    FullBench.Run(sampleRate, blockSize, oversample, peak, args.Skip(2).ToArray());
    return;
}

foreach (var bias in new double[] { 0, -2, -4 })
{
    var timeStep = (Expression)((Real)1 / (sampleRate * oversample));
    var fixture = new FrontEnd();
    var setup = Stopwatch.StartNew();
    var analysis = fixture.Build(bias);
    var outputs = new[] { fixture.Plates, fixture.Following, fixture.PlateA, fixture.CathodeA, fixture.GridA };
    ILog log = Environment.GetEnvironmentVariable("UA176_TRACE") != null ? new ConsoleLog() : new NullLog();
    var solution = TransientSolution.Solve(analysis, timeStep, log);
    var solver = new Simulation(solution)
    {
        Input = new[] { fixture.Drive },
        Output = outputs,
        Iterations = 20,
        Oversample = oversample
    };
    var tone = new double[blockSize];
    var buffer = outputs.Select(_ => new double[blockSize]).ToArray();
    var traces = outputs.Select(_ => new double[window]).ToArray();
    var times = new double[measuredBlocks];
    var index = 0;
    for (var block = -warmupBlocks; block < measuredBlocks; block++)
    {
        for (var sample = 0; sample < blockSize; sample++)
            tone[sample] = peak * Math.Sin(2 * Math.PI * toneHz * (index + sample) / sampleRate);
        var start = Stopwatch.GetTimestamp();
        solver.Run(tone, buffer);
        var elapsed = Stopwatch.GetElapsedTime(start).TotalMilliseconds;
        if (block == -warmupBlocks)
            Console.WriteLine($"CV {bias:F0} V, {oversample}x, setup and first block {setup.Elapsed.TotalSeconds:F3} s");
        if (block >= 0)
            times[block] = elapsed;
        for (var sample = 0; sample < blockSize; sample++)
            for (var probe = 0; probe < buffer.Length; probe++)
                traces[probe][(index + sample) % window] = buffer[probe][sample];
        index += blockSize;
    }
    if (traces.Any(values => values.Any(value => !double.IsFinite(value))))
        throw new InvalidOperationException("Non-finite LiveSPICE output.");
    Array.Sort(times);
    Console.WriteLine($"Mono 512 samples: mean {times.Average():F3} ms, p99 {times[253]:F3} ms, max {times[^1]:F3} ms");
    Console.WriteLine($"DC PA {traces[2].Average():F6} V, KA {traces[3].Average():F6} V, GP {traces[4].Average():F6} V");
    for (var probe = 0; probe < 2; probe++)
    {
        var fundamental = Harmonic(traces[probe], 1);
        var thd = Math.Sqrt(Enumerable.Range(2, 19).Sum(k => Math.Pow(Harmonic(traces[probe], k), 2))) / fundamental;
        Console.WriteLine($"{(probe == 0 ? "PB-PA" : "NEXT_N-NEXT_P")}: {20 * Math.Log10(fundamental / peak):F4} dB, THD K2–K20 {100 * thd:F6}%");
    }
}

static double Harmonic(double[] values, int k)
{
    var cosine = 0.0;
    var sine = 0.0;
    for (var i = 0; i < values.Length; i++)
    {
        var angle = 2 * Math.PI * k * toneHz * i / sampleRate;
        cosine += values[i] * Math.Cos(angle);
        sine += values[i] * Math.Sin(angle);
    }
    return 2 * Math.Sqrt(cosine * cosine + sine * sine) / values.Length;
}
