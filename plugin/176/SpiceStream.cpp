#include "SpiceStream.h"
#include "UA176Deck.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <sstream>
#include <thread>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace ua176 {
namespace {
struct VectorValue { char* name; double real, imag; bool scale, complex; };
struct VectorValues { int count, index; VectorValue** values; };
using TextCallback = int(char*, int, void*);
using ExitCallback = int(int, bool, bool, int, void*);
using DataCallback = int(VectorValues*, int, int, void*);
using InitCallback = int(void*, int, void*);
using BackgroundCallback = int(bool, int, void*);
using SourceCallback = int(double*, double, char*, int, void*);
using SyncCallback = int(double, double*, double, int, int, int, void*);
constexpr double warmup = .15;
constexpr auto ringSize = std::uint64_t{1} << 16;
using std::memory_order_acquire;
using std::memory_order_release;
using std::memory_order_relaxed;

template<class Pointer>
Pointer symbol(void* module, const char* name)
{
#ifdef _WIN32
    return reinterpret_cast<Pointer>(GetProcAddress(static_cast<HMODULE>(module), name));
#else
    return reinterpret_cast<Pointer>(dlsym(module, name));
#endif
}
}

struct SpiceStream::Impl {
    struct Frame { float input; Controls controls; };
    struct Result { float output, cv; };
    using Init = int(*)(TextCallback*, TextCallback*, ExitCallback*, DataCallback*, InitCallback*,
                        BackgroundCallback*, void*);
    using Sync = int(*)(SourceCallback*, SourceCallback*, SyncCallback*, int*, void*);
    using Command = int(*)(char*);
    using Circuit = int(*)(char**);
    using Running = bool(*)();

    std::vector<Frame> input{ringSize};
    std::vector<Result> output{ringSize};
    std::atomic<std::uint64_t> queued{0}, consumed{0}, rendered{0}, readThrough{0};
    std::atomic<bool> ending{false}, problem{false}, warmed{false};
    double sampleRate = 44100;
    double previousTime = 0, previousOutput = 0, previousCv = 0;
    void* module = nullptr;
    Command command = nullptr;
    Running running = nullptr;
    std::filesystem::path temporary;

    static int message(char* value, int, void* context)
    {
        if (std::strstr(value, "Error:") || std::strstr(value, "ERROR:") || std::strstr(value, "aborted")
            || std::strstr(value, "Timestep too small"))
            static_cast<Impl*>(context)->problem.store(true, memory_order_release);
        return 0;
    }

    static int exit(int, bool, bool quit, int, void* context)
    {
        if (!quit) static_cast<Impl*>(context)->problem.store(true, memory_order_release);
        return 0;
    }

    double position(double time) const { return (time - warmup) * sampleRate; }

    static int voltage(double* result, double time, char* source, int, void* context)
    {
        auto& s = *static_cast<Impl*>(context);
        const double position = s.position(time);
        Controls controls;
        double inputVoltage = 0;
        if (position > 0) {
            auto index = static_cast<std::uint64_t>(position);
            auto next = index + 1;
            s.warmed.store(true, memory_order_release);
            while (s.queued.load(memory_order_acquire) <= next && !s.ending.load(memory_order_acquire))
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            if (!s.ending.load(memory_order_relaxed)) {
                const auto& left = s.input[index % ringSize];
                const auto& right = s.input[next % ringSize];
                controls = left.controls;
                inputVoltage = (static_cast<double>(left.input) + (right.input - left.input) * (position - index)) * std::min(1., position / 220.);
            }
        }
        const auto equal = [source](const char* name) { return std::strcmp(source, name) == 0; };
        if (equal("vdrive")) *result = 2 * fullScalePeak * inputVoltage;
        else if (equal("vpin")) *result = controls.input;
        else if (equal("vpout")) *result = controls.output;
        else if (equal("vpinvernier")) *result = controls.inputVernier;
        else if (equal("vpoutvernier")) *result = controls.outputVernier;
        else if (equal("vrs")) *result = controls.tap;
        else if (equal("vrb")) *result = controls.biasResistance;
        else if (equal("vatt")) *result = controls.attack;
        else if (equal("vrel")) *result = controls.release;
        else if (equal("vlimit"))
            *result = 1. + (controls.limiting - 1.) * std::clamp(position / 220., 0., 1.);
        else *result = 0;
        return 0;
    }

    static int initializeVectors(void*, int, void*) { return 0; }

    static int receive(VectorValues* data, int count, int, void* context)
    {
        auto& s = *static_cast<Impl*>(context);
        double time = 0, audio = 0, cv = 0;
        for (int i = 0; i < count; ++i) {
            auto& value = *data->values[i];
            if (value.scale || std::strcmp(value.name, "time") == 0) time = value.real;
            else if (std::strcmp(value.name, "audio") == 0) audio = value.real;
            else if (std::strcmp(value.name, "cv") == 0) cv = value.real;
        }
        if (!std::isfinite(audio) || !std::isfinite(cv)) {
            s.problem.store(true, memory_order_release);
            return 0;
        }
        auto next = s.rendered.load(memory_order_relaxed);
        const auto position = s.position(time);
        while (position >= static_cast<double>(next) && !s.ending.load(memory_order_relaxed)) {
            while (next >= s.readThrough.load(memory_order_acquire) + ringSize
                   && !s.ending.load(memory_order_relaxed))
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            if (s.ending.load(memory_order_relaxed)) break;
            const double target = warmup + next / s.sampleRate;
            const double dt = time - s.previousTime;
            const double weight = dt > 0 ? std::clamp((target - s.previousTime) / dt, 0., 1.) : 1.;
            s.output[next % ringSize] = {
                static_cast<float>((s.previousOutput + weight * (audio - s.previousOutput)) / fullScalePeak),
                static_cast<float>(s.previousCv + weight * (cv - s.previousCv))
            };
            s.rendered.store(++next, memory_order_release);
        }
        s.previousTime = time;
        s.previousOutput = audio;
        s.previousCv = cv;
        if (position > 32)
            s.consumed.store(static_cast<std::uint64_t>(position) - 32, memory_order_release);
        return 0;
    }

    bool open(const std::filesystem::path& library)
    {
        // ngspice owns global state. A distinct mapped file gives each channel
        // and instance its own simulator and callback state.
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        temporary = std::filesystem::temp_directory_path() /
            ("176-spice-" + std::to_string(stamp) + "-" + std::to_string(reinterpret_cast<std::uintptr_t>(this)));
        std::filesystem::create_directory(temporary);
        const auto copy = temporary / library.filename();
        std::filesystem::copy_file(library, copy);
#ifdef _WIN32
        const auto directory = library.parent_path();
        for (const auto& item : std::filesystem::directory_iterator(directory))
            if (item.path().extension() == ".dll" && item.path().filename() != library.filename())
                std::filesystem::copy_file(item.path(), temporary / item.path().filename());
        module = LoadLibraryExW(copy.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
#else
        module = dlopen(copy.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
        if (!module) return false;
        auto init = symbol<Init>(module, "ngSpice_Init");
        auto sync = symbol<Sync>(module, "ngSpice_Init_Sync");
        auto circuit = symbol<Circuit>(module, "ngSpice_Circ");
        command = symbol<Command>(module, "ngSpice_Command");
        running = symbol<Running>(module, "ngSpice_running");
        if (!init || !sync || !circuit || !command || !running) return false;
        if (init(message, nullptr, exit, receive, initializeVectors, nullptr, this) != 0
            || sync(voltage, nullptr, nullptr, nullptr, this) != 0)
            return false;

        std::vector<std::string> lines(std::begin(poweredDeckLines), std::end(poweredDeckLines));
        lines.pop_back();
        const auto step = 1 / (sampleRate * 2);
        std::ostringstream transient;
        transient.precision(16);
        transient << ".tran " << step << " 86400 0 " << step;
        lines.push_back(transient.str());
        lines.emplace_back(".end");
        std::vector<char*> pointers;
        for (auto& line : lines) pointers.push_back(line.data());
        pointers.push_back(nullptr);
        if (circuit(pointers.data()) != 0 || problem.load(memory_order_acquire)) return false;
        char run[] = "bg_run";
        return command(run) == 0;
    }

    ~Impl()
    {
        ending.store(true, memory_order_release);
        if (command && running) {
            char halt[] = "bg_halt";
            if (running()) command(halt);
            // Do not unload callback code until the solver leaves its worker.
            while (running()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
            char reset[] = "destroy all";
            command(reset);
            char remove[] = "remcirc";
            command(remove);
            char stopMessages[] = "bg_pstop";
            command(stopMessages);
        }
        if (module) {
#ifdef _WIN32
            FreeLibrary(static_cast<HMODULE>(module));
#else
            dlclose(module);
#endif
        }
        std::error_code error;
        if (!temporary.empty()) std::filesystem::remove_all(temporary, error);
    }
};

SpiceStream::SpiceStream() = default;
SpiceStream::~SpiceStream() = default;

bool SpiceStream::start(const std::filesystem::path& library, double rate)
{
    stop();
    try {
        state = std::make_unique<Impl>();
        state->sampleRate = rate;
        if (state->open(library)) return true;
    } catch (const std::exception&) {}
    stop();
    return false;
}

void SpiceStream::stop() { state.reset(); }
bool SpiceStream::ready() const noexcept { return state && state->warmed.load(memory_order_acquire); }
bool SpiceStream::failed() const noexcept { return !state || state->problem.load(memory_order_acquire); }
std::uint64_t SpiceStream::framesProduced() const noexcept
{
    return state ? state->rendered.load(memory_order_acquire) : 0;
}

bool SpiceStream::submit(float input, const Controls& controls) noexcept
{
    if (!state) return false;
    auto index = state->queued.load(memory_order_relaxed);
    if (index >= state->consumed.load(memory_order_acquire) + ringSize) return false;
    state->input[index % ringSize] = {input, controls};
    state->queued.store(index + 1, memory_order_release);
    return true;
}

bool SpiceStream::read(std::uint64_t index, float& value, float& cv) noexcept
{
    if (!state) return false;
    const auto produced = framesProduced();
    if (index < state->readThrough.load(memory_order_relaxed) || index >= produced) return false;
    const auto row = state->output[index % ringSize];
    value = row.output;
    cv = row.cv;
    state->readThrough.store(index + 1, memory_order_release);
    return true;
}
}
