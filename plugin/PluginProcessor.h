#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "TubeCompDSP.h"
#include <atomic>

class TubeCompProcessor final : public juce::AudioProcessor {
public:
    TubeCompProcessor();
    ~TubeCompProcessor() override = default;
    const juce::String getName() const override {return "Tube Comp RS124 / A-436B";}
    void prepareToPlay(double sampleRate,int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override {
        const auto in=layouts.getMainInputChannelSet();
        const auto out=layouts.getMainOutputChannelSet();
        return in==out && (in==juce::AudioChannelSet::mono()||
                           in==juce::AudioChannelSet::stereo());
    }
    bool acceptsMidi() const override {return false;}
    bool producesMidi() const override {return false;}
    bool isMidiEffect() const override {return false;}
    double getTailLengthSeconds() const override {return 0.0;}
    int getNumPrograms() override {return 1;}
    int getCurrentProgram() override {return 0;}
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override {return "Default";}
    void changeProgramName(int,const juce::String&) override {}
    bool hasEditor() const override {return true;}
    juce::AudioProcessorEditor* createEditor() override;
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*,int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameters();
    juce::AudioProcessorValueTreeState state;
    std::atomic<float> grMeter{0.f},inputMeter{0.f},outputMeter{0.f};

private:
    tubecomp::Engine engine;
    tubecomp::Parameters parametersFromState() const noexcept;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TubeCompProcessor)
};
