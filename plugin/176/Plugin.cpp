#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include <cmath>
#include <thread>
#include "SpiceStream.h"
#include "UA176Art.h"
#ifdef UA176_LIVE
#include "../176-live/Live176.h"
#endif

namespace {
using State = juce::AudioProcessorValueTreeState;
#ifdef UA176_LIVE
using EngineControls = ua176::LiveControls;
constexpr std::array<const char*, 7> controlIds{
    "input", "ratio", "mix", "attack", "sidechainHpf", "output", "release"
};
constexpr std::array<const char*, 7> controlNames{
    "Input", "Compression ratio", "Mix (dry %)", "Attack", "Sidechain HPF (Hz)", "Output", "Release"
};
#else
using EngineControls = ua176::Controls;
constexpr std::array<const char*, 7> controlIds{
    "input", "ratio", "inputVernier", "attack", "outputVernier", "output", "release"
};
constexpr std::array<const char*, 7> controlNames{
    "Input", "Compression ratio", "Input vernier", "Attack", "Output vernier", "Output", "Release"
};
#endif
constexpr std::array<float, 4> tap{.12f, .29f, .58f, 1.f};
constexpr std::array<float, 4> bias{1000, 6800, 20000, 47000};

State::ParameterLayout parameters()
{
    State::ParameterLayout result;
    const auto add = [&result](int i, juce::NormalisableRange<float> range, float initial) {
        result.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{controlIds[static_cast<std::size_t>(i)], 1},
            controlNames[static_cast<std::size_t>(i)], range, initial));
    };
    add(0, {0, 40, 2}, 20);
    result.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"ratio", 1},
        "Compression ratio", juce::StringArray{"2:1", "4:1", "8:1", "12:1"}, 1));
#ifdef UA176_LIVE
    add(2, {0, 100, 1}, 0);
#else
    add(2, {0, 1}, 0);
#endif
    add(3, {0, 1}, .5f);
#ifdef UA176_LIVE
    add(4, {0, 350, 1}, 0);
#else
    add(4, {0, 1}, 0);
#endif
    add(5, {0, 40, 2}, 30);
    add(6, {0, 1}, .8f);
    result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"on", 1}, "Power", true));
    result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"limiting", 1}, "Limiting action", true));
    result.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"meter", 1},
        "Meter", juce::StringArray{"Gain reduction", "Input", "Output"}, 0));
#ifdef UA176_LIVE
    result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"double", 1}, "Double", false));
#endif
    return result;
}

class Processor final : public juce::AudioProcessor {
public:
    Processor() : AudioProcessor(BusesProperties().withInput("Input", defaultChannels(), true)
                                 .withOutput("Output", defaultChannels(), true)),
                  state(*this, nullptr, "UA176State", parameters()) {}

    static juce::AudioChannelSet defaultChannels()
    {
#ifdef UA176_LIVE
        return juce::AudioChannelSet::mono();
#else
        return juce::AudioChannelSet::stereo();
#endif
    }

    bool isBusesLayoutSupported(const BusesLayout& layout) const override
    {
        const auto in = layout.getMainInputChannelSet();
#ifdef UA176_LIVE
        return in == layout.getMainOutputChannelSet() && in == juce::AudioChannelSet::mono();
#else
        return in == layout.getMainOutputChannelSet()
            && (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
#endif
    }

#ifdef UA176_LIVE
    void prepareToPlay(double sampleRate, int) override
    {
        liveLatency = ua176::Live176::latency(sampleRate);
        setLatencySamples(liveLatency);
        liveDry = {};
        liveDryPosition = {};
        mix.reset(sampleRate, .010);
        mix.setCurrentAndTargetValue(get("mix") * .01f);
        const auto controls = readControls();
        good.fill(false);
        for (int c = 0; c < getTotalNumInputChannels(); ++c)
            good[static_cast<std::size_t>(c)] = simulators[static_cast<std::size_t>(c)].prepare(sampleRate, controls);
        status.store(good[0] ? 1 : 2);
    }

    void releaseResources() override {}

    void processBlock(juce::AudioBuffer<float>& block, juce::MidiBuffer& midi) override
    {
        processLiveBlock(block, midi, get("on") > .5f);
    }

    void processBlockBypassed(juce::AudioBuffer<float>& block, juce::MidiBuffer& midi) override
    {
        processLiveBlock(block, midi, false);
    }

    void processLiveBlock(juce::AudioBuffer<float>& block, juce::MidiBuffer& midi, bool on)
    {
        juce::ScopedNoDenormals guard;
        midi.clear();
        const auto controls = readControls();
        float inputPeak = 0, outputPeak = 0, reduction = 0;
        bool failed = false;
        constexpr double scale = ua176::SpiceStream::fullScalePeak;
        mix.setTargetValue(get("mix") * .01f);
        for (int c = 0; c < getTotalNumInputChannels(); ++c) {
            auto& simulator = simulators[static_cast<std::size_t>(c)];
            const bool ready = good[static_cast<std::size_t>(c)] && simulator.setControls(controls);
            auto* samples = block.getWritePointer(c);
            auto& delay = liveDry[static_cast<std::size_t>(c)];
            auto& position = liveDryPosition[static_cast<std::size_t>(c)];
            for (int i = 0; i < block.getNumSamples(); ++i) {
                const float drySample = samples[i];
                inputPeak = std::max(inputPeak, std::abs(drySample));
                const float dryAmount = mix.getNextValue();
                double volts = 0, cv = 0;
                const bool solved = ready && simulator.step(2 * scale * drySample, volts, cv);
                float alignedDry = drySample;
                if (liveLatency > 0) {
                    alignedDry = delay[position];
                    delay[position] = drySample;
                    position = (position + 1) % delay.size();
                }
                samples[i] = alignedDry;
                if (solved && on) {
                    samples[i] = ua176::dryMix(static_cast<float>(volts / scale), alignedDry, dryAmount);
                    reduction = std::max(reduction, static_cast<float>(-cv * 3.5));
                }
                failed |= !solved;
                outputPeak = std::max(outputPeak, std::abs(samples[i]));
            }
        }
        levelIn.store(inputPeak);
        levelOut.store(outputPeak);
        gr.store(reduction);
        status.store(failed ? 2 : 1);
    }
#else
    void prepareToPlay(double sampleRate, int) override
    {
        releaseResources();
        position = 0;
        dry = {};
        underruns.store(0);
        bad = false;
        setLatencySamples(ua176::SpiceStream::latency);
        auto directory = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                             .getParentDirectory().getSiblingFile("Resources").getChildFile("ngspice");
#ifdef _WIN32
        auto library = directory.getChildFile("ngspice.dll");
#else
        auto library = directory.getChildFile("libngspice.so.0");
        if (!library.existsAsFile()) library = juce::File("/usr/lib/x86_64-linux-gnu/libngspice.so.0");
#endif
        if (!library.existsAsFile()) {
            status.store(2);
            bad = true;
            return;
        }
        const auto path = std::filesystem::u8path(library.getFullPathName().toStdString());
        status.store(0);
        for (int c = 0; c < getTotalNumInputChannels(); ++c)
            if (!simulators[static_cast<std::size_t>(c)].start(path, sampleRate)) {
                status.store(2);
                bad = true;
                break;
            }
    }

    void releaseResources() override
    {
        for (auto& stream : simulators) stream.stop();
    }

    void processBlock(juce::AudioBuffer<float>& block, juce::MidiBuffer& midi) override
    {
        juce::ScopedNoDenormals guard;
        midi.clear();
        const bool on = get("on") > .5f;
        const bool offline = isNonRealtime();
        const auto controls = readControls();
        const int channels = getTotalNumInputChannels();
        const int count = block.getNumSamples();
        auto* const* write = block.getArrayOfWritePointers();
        float inputPeak = 0, outputPeak = 0, reduction = 0;
        for (int i = 0; i < count; ++i, ++position) {
            const auto slot = position % ua176::SpiceStream::latency;
            for (int c = 0; c < channels; ++c) {
                auto& delayed = dry[static_cast<std::size_t>(c)][slot];
                auto& simulator = simulators[static_cast<std::size_t>(c)];
                const float input = write[c][i];
                float output = 0;
                inputPeak = std::max(inputPeak, std::abs(input));
                if (!bad && (simulator.failed() || !simulator.submit(input, controls))) {
                    bad = true;
                    status.store(2);
                }
                if (position >= ua176::SpiceStream::latency) {
                    output = delayed;
                    if (!bad) {
                        const auto index = position - ua176::SpiceStream::latency;
                        if (offline) {
                            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
                            while (simulator.framesProduced() <= index && !simulator.failed()
                                   && std::chrono::steady_clock::now() < deadline)
                                std::this_thread::sleep_for(std::chrono::microseconds(100));
                        }
                        float wet = 0, cv = 0;
                        if (on && simulator.read(index, wet, cv)) {
                            output = wet;
                            // Display the simulated CV, scaled to approximate the meter movement.
                            reduction = std::max(reduction, -cv * 3.5f);
                            status.store(1);
                        } else if (on) {
                            underruns.fetch_add(1);
                            status.store(3);
                        } else {
                            simulator.read(index, wet, cv);
                        }
                    }
                }
                delayed = input;
                write[c][i] = output;
                outputPeak = std::max(outputPeak, std::abs(output));
            }
        }
        levelIn.store(inputPeak);
        levelOut.store(outputPeak);
        gr.store(reduction);
    }

#endif

    EngineControls readControls() const noexcept
    {
        const auto r = static_cast<std::size_t>(juce::jlimit(0, 3, static_cast<int>(get("ratio"))));
        EngineControls controls;
        controls.input = std::pow(10.f, (40 - get("input")) / 20);
        controls.output = std::pow(10.f, (40 - get("output")) / 20);
#ifdef UA176_LIVE
        controls.sidechainHz = get("sidechainHpf");
#else
        controls.inputVernier = get("inputVernier");
        controls.outputVernier = get("outputVernier");
#endif
        controls.tap = tap[r];
        controls.biasResistance = bias[r];
        controls.attack = get("attack");
#ifdef UA176_LIVE
        controls.doubleTiming = get("double") > .5f;
#endif
        controls.release = get("release");
        controls.limiting = get("limiting");
        return controls;
    }

    float get(const char* id) const noexcept { return state.getRawParameterValue(id)->load(); }
    void set(const char* id, float normal)
    {
        if (auto* p = state.getParameter(id)) {
            p->beginChangeGesture();
            p->setValueNotifyingHost(normal);
            p->endChangeGesture();
        }
    }

    const juce::String getName() const override
    {
#ifdef UA176_LIVE
        return "176 Live";
#else
        return "176";
#endif
    }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 1.5; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;
    void getStateInformation(juce::MemoryBlock& destination) override
    {
        if (auto xml = state.copyState().createXml()) copyXmlToBinary(*xml, destination);
    }
    void setStateInformation(const void* data, int size) override
    {
        if (auto xml = getXmlFromBinary(data, size); xml && xml->hasTagName(state.state.getType()))
            state.replaceState(juce::ValueTree::fromXml(*xml));
    }

    State state;
    std::atomic<float> levelIn{0}, levelOut{0}, gr{0};
    std::atomic<int> status{0};
private:
#ifdef UA176_LIVE
    std::array<ua176::Live176, 2> simulators;
    std::array<bool, 2> good{};
    std::array<std::array<float, ua176::Live176::phaseLatency>, 2> liveDry{};
    std::array<std::size_t, 2> liveDryPosition{};
    int liveLatency = 0;
    juce::SmoothedValue<float> mix;
#else
    std::array<ua176::SpiceStream, 2> simulators;
    std::array<std::array<float, ua176::SpiceStream::latency>, 2> dry{};
    std::uint64_t position = 0;
    std::atomic<std::uint64_t> underruns{0};
    bool bad = false;
#endif
};

struct PinLook final : juce::LookAndFeel_V4 {
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float value, float start, float end, juce::Slider&) override
    {
        const float radius = static_cast<float>(std::min(width, height)) * .5f;
        const float cx = static_cast<float>(x) + width * .5f;
        const float cy = static_cast<float>(y) + height * .5f;
        const float a = start + value * (end - start);
        const auto dx = std::sin(a), dy = -std::cos(a);
        g.setColour(juce::Colour(0xfff3e1ba));
        g.drawLine(cx + radius * .68f * dx, cy + radius * .68f * dy,
                   cx + radius * .92f * dx, cy + radius * .92f * dy, 1.8f);
    }
};

class Editor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit Editor(Processor& p) : AudioProcessorEditor(p), processor(p)
    {
        auto xml = juce::parseXML(juce::String::fromUTF8(UA176Art::Faceplate_svg, UA176Art::Faceplate_svgSize));
        if (xml) panel = juce::Drawable::createFromSVG(*xml);
        for (std::size_t i = 0; i < knobs.size(); ++i) {
            auto& knob = knobs[i];
            knob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            knob.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
            knob.setRotaryParameters(juce::degreesToRadians(i == 1 ? -68.f : -132.f),
                                     juce::degreesToRadians(i == 1 ? 64.f : 132.f), true);
            knob.setName(controlNames[i]);
            knob.setTooltip(controlNames[i]);
            knob.setLookAndFeel(&pins);
            addAndMakeVisible(knob);
            attachments[i] = std::make_unique<State::SliderAttachment>(processor.state, controlIds[i], knob);
        }
#ifdef UA176_LIVE
        knobs[2].onValueChange = [this] {
            knobs[2].setTooltip(juce::String(knobs[2].getValue(), 0) + "% dry");
        };
        knobs[4].onValueChange = [this] {
            const auto hz = knobs[4].getValue();
            knobs[4].setTooltip(hz == 0 ? juce::String("Sidechain HPF off")
                                      : juce::String(hz, 0) + " Hz sidechain HPF");
        };
        knobs[2].onValueChange();
        knobs[4].onValueChange();
#endif
#ifdef UA176_LIVE
        doubleSwitch.setLookAndFeel(&doubleLook);
        doubleSwitch.setClickingTogglesState(true);
        doubleSwitch.setTooltip("Double timing");
        addAndMakeVisible(doubleSwitch);
        doubleAttachment = std::make_unique<State::ButtonAttachment>(processor.state, "double", doubleSwitch);
#endif
        setSize(1140, 218);
        startTimerHz(30);
    }

    ~Editor() override
    {
        stopTimer();
#ifdef UA176_LIVE
        doubleSwitch.setLookAndFeel(nullptr);
#endif
        for (auto& knob : knobs) knob.setLookAndFeel(nullptr);
    }

    void resized() override
    {
#ifdef UA176_LIVE
        doubleSwitch.setBounds(811, 48, 41, 38);
#endif
        constexpr std::array<int, 7> x{237, 370, 370, 772, 772, 904, 1035};
        constexpr std::array<int, 7> y{121, 67, 166, 67, 166, 121, 67};
        for (std::size_t i = 0; i < knobs.size(); ++i) {
            const int r = i == 0 || i == 5 ? 49 : 24;
            knobs[i].setBounds(x[i] - r, y[i] - r, 2*r, 2*r);
        }
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff403939));
        if (panel) panel->draw(g, 1.f);
        const float angle = juce::degreesToRadians(-53.f + 106.f * needle);
        g.setColour(juce::Colour(0xff39261c));
        g.drawLine(565, 126, 565 + 65 * std::sin(angle), 126 - 65 * std::cos(angle), 1.4f);
        if (processor.get("on") < .5f) {
            g.setColour(juce::Colours::black.withAlpha(.5f));
            g.fillEllipse(987, 160, 16, 16);
        }
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        const auto p = event.getPosition();
        if (juce::Rectangle<int>{1048, 142, 42, 48}.contains(p))
            processor.set("on", processor.get("on") > .5f ? 0.f : 1.f);
        else if (juce::Rectangle<int>{466, 29, 207, 116}.contains(p))
            processor.set("meter", std::fmod(processor.get("meter") + 1, 3.f) / 2.f);
        else if (juce::Rectangle<int>{751, 19, 42, 18}.contains(p))
            processor.set("limiting", processor.get("limiting") > .5f ? 0.f : 1.f);
    }

private:
    void timerCallback() override
    {
        const auto meter = static_cast<int>(processor.get("meter"));
        float value = 1.f - processor.gr.load() / 22.f;
        if (meter != 0) {
            const float peak = meter == 1 ? processor.levelIn.load() : processor.levelOut.load();
            value = (juce::Decibels::gainToDecibels(peak, -60.f) + 35.f) / 23.f;
        }
        needle += .24f * (juce::jlimit(0.f, 1.f, value) - needle);
        const auto solver = processor.status.load();
        setTooltipText(solver);
        repaint();
    }

    void setTooltipText(int solver)
    {
        if (solver == lastStatus) return;
        lastStatus = solver;
        setTitle(solver == 2 ? "176 - SPICE unavailable"
                 : solver == 3 ? "176 - SPICE cannot keep up with playback"
                 : "176");
    }

#ifdef UA176_LIVE
    struct DoubleLook final : juce::LookAndFeel_V4 {
        void drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                  bool hovered, bool pressed) override
        {
            const auto dot = juce::Rectangle<float>(14, 14).withCentre({b.getWidth() * .5f, 11});
            g.setColour(juce::Colour(0xff100e0d));
            g.fillEllipse(dot);
            g.setColour(juce::Colour(b.getToggleState() ? 0xffdbb66e : 0xff766656));
            g.drawEllipse(dot, b.getToggleState() ? 1.5f : 1.f);
            if (hovered || pressed)
                g.drawEllipse(dot.reduced(3), .65f);
        }
        void drawButtonText(juce::Graphics& g, juce::TextButton& b, bool, bool) override
        {
            g.setColour(juce::Colour(0xffe3d4ad));
            g.setFont(juce::FontOptions(9.f));
            g.drawText(b.getButtonText(), 0, 20, b.getWidth(), 13, juce::Justification::centred);
        }
    };
    DoubleLook doubleLook;
    juce::TextButton doubleSwitch{"Double"};
    std::unique_ptr<State::ButtonAttachment> doubleAttachment;
#endif
    Processor& processor;
    PinLook pins;
    std::unique_ptr<juce::Drawable> panel;
    std::array<juce::Slider, 7> knobs;
    std::array<std::unique_ptr<State::SliderAttachment>, 7> attachments;
    float needle = .78f;
    int lastStatus = -1;
};

juce::AudioProcessorEditor* Processor::createEditor() { return new Editor(*this); }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new Processor; }
