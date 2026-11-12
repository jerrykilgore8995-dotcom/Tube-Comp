#pragma once
#include "PluginProcessor.h"
#include "../gui/TubeCompFaceplate.h"
#include <memory>
#include <vector>

class TubeCompEditor final : public juce::AudioProcessorEditor,private juce::Timer {
public:
    explicit TubeCompEditor(TubeCompProcessor&);
    ~TubeCompEditor() override;
    void resized() override;
private:
    class InvisibleDial : public juce::Slider {
    public:
        InvisibleDial() {
            setSliderStyle(juce::Slider::RotaryVerticalDrag);
            setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
            setMouseDragSensitivity(280);
        }
        void paint(juce::Graphics&) override {}
    };
    struct Dial {
        std::unique_ptr<InvisibleDial> slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
    TubeCompProcessor& proc;
    TubeCompFaceplate face;
    std::vector<Dial> dials;
    juce::TextButton bypass,superFuse;
    juce::ComboBox outputLoad;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttach,fuseAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> loadAttach;
    void timerCallback() override;
    void setMode(bool rs);
    void makeDial(const char* parameter);
    void setParameter(const char* key,float actual);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TubeCompEditor)
};
