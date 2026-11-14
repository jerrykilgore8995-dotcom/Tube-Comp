#include "PluginEditor.h"
#include <cmath>

TubeCompEditor::TubeCompEditor(TubeCompProcessor& p)
  : juce::AudioProcessorEditor(&p),proc(p) {
    setSize(1600,350);
    addAndMakeVisible(face);
    face.setBounds(0,0,1600,350);
    face.onModeSelected=[this](TubeCompFaceplate::Mode m) {
        setParameter("rs124",m==TubeCompFaceplate::Mode::rs124?1.f:0.f);
        setMode(m==TubeCompFaceplate::Mode::rs124);
    };
    face.onAltecAttackSelected=[this](int a) {
        setParameter("altecAttack",float(a));
    };
    face.onAltecRecoverySelected=[this](int pos) {
        setParameter("altecRecovery",float(pos));
    };
    face.onPowerSelected=[this](bool on) {
        setParameter("bypass",on?0.f:1.f);
    };
    for(const char* id: {"input","output","attack","recovery","balance"})
        makeDial(id);
    // Two independent, narrow controls mounted in the host strip, not faceplate.
    auto configureTrim=[this](juce::Slider& dial,juce::Label& label,
                              const char* title,const char* param) {
        dial.setSliderStyle(juce::Slider::LinearHorizontal);
        dial.setTextBoxStyle(juce::Slider::TextBoxRight,false,64,20);
        dial.setNumDecimalPlacesToDisplay(1);
        dial.setTextValueSuffix(" dB");
        dial.setColour(juce::Slider::trackColourId,juce::Colour::fromRGB(96,147,170));
        dial.setColour(juce::Slider::thumbColourId,juce::Colour::fromRGB(219,224,222));
        dial.setColour(juce::Slider::textBoxTextColourId,juce::Colours::white);
        dial.setColour(juce::Slider::textBoxBackgroundColourId,juce::Colour::fromRGB(20,24,26));
        dial.setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
        label.setText(title,juce::dontSendNotification);
        label.setFont(juce::Font(juce::FontOptions(11.5f,juce::Font::bold)));
        label.setColour(juce::Label::textColourId,juce::Colour::fromRGB(209,224,228));
        addAndMakeVisible(label);
        addAndMakeVisible(dial);
    };
    configureTrim(digitalInputTrim,inputTrimLabel,"Input","digitalInput");
    configureTrim(digitalOutputTrim,outputTrimLabel,"Output","digitalOutput");
    digitalInputAttach=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        p.state,"digitalInput",digitalInputTrim);
    digitalOutputAttach=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        p.state,"digitalOutput",digitalOutputTrim);
    altecModButton.setButtonText("MOD");
    altecModButton.setClickingTogglesState(true);
    bypass.setButtonText("BYPASS");
    bypass.setClickingTogglesState(true);
    superFuse.setButtonText("SUPERFUSE");
    superFuse.setClickingTogglesState(true);
    for(auto* b:{&bypass,&superFuse,&altecModButton}){
        b->setColour(juce::TextButton::buttonColourId,
                     juce::Colour::fromRGB(35,40,43));
        b->setColour(juce::TextButton::buttonOnColourId,
                     juce::Colour::fromRGB(79,105,116));
        b->setColour(juce::TextButton::textColourOffId,
                     juce::Colour::fromRGB(218,230,231));
        b->setColour(juce::TextButton::textColourOnId,
                     juce::Colours::white);
        addAndMakeVisible(*b);
    }
    bypassAttach=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        p.state,"bypass",bypass);
    fuseAttach=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        p.state,"superfuse",superFuse);
    altecModAttach=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        p.state,"altecMod",altecModButton);
    // Match the three RS124 host-strip boxes to the impedance selector's
    // neutral JUCE palette; retain a distinct illuminated toggle-on state.
    const auto boxBackground=outputLoad.findColour(juce::ComboBox::backgroundColourId);
    const auto boxOutline=outputLoad.findColour(juce::ComboBox::outlineColourId);
    const auto boxText=outputLoad.findColour(juce::ComboBox::textColourId);
    for(auto* b:{&superFuse,&bypass}){
        b->setColour(juce::TextButton::buttonColourId,boxBackground);
        b->setColour(juce::TextButton::textColourOffId,boxText);
        b->setColour(juce::ComboBox::outlineColourId,boxOutline);
    }
    outputLoad.setJustificationType(juce::Justification::centred);
    outputLoad.addItem("200 OHM",1);outputLoad.addItem("600 OHM",2);
    addAndMakeVisible(outputLoad);
    loadAttach=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        p.state,"impedance",outputLoad);
    setMode(proc.state.getRawParameterValue("rs124")->load()>.5f);
    resized();
    timerCallback();
    startTimerHz(30);
}
TubeCompEditor::~TubeCompEditor(){stopTimer();face.onModeSelected=nullptr;face.onPowerSelected=nullptr;face.onAltecAttackSelected=nullptr;face.onAltecRecoverySelected=nullptr;}
void TubeCompEditor::makeDial(const char* id) {
    Dial control;
    control.slider=std::make_unique<InvisibleDial>();
    addAndMakeVisible(*control.slider);
    control.attachment=
        std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            proc.state,id,*control.slider);
    dials.push_back(std::move(control));
}
void TubeCompEditor::setParameter(const char* key,float actual) {
    if(auto* p=proc.state.getParameter(key)){
        const auto normalized=p->convertTo0to1(actual);
        p->beginChangeGesture();
        p->setValueNotifyingHost(normalized);
        p->endChangeGesture();
    }
}
void TubeCompEditor::setMode(bool rs) {
    face.setMode(rs?TubeCompFaceplate::Mode::rs124:
                    TubeCompFaceplate::Mode::altec436b);
    // Real invisible host parameter controls follow the skinned physical knobs.
    dials[0].slider->setBounds(rs?297:165,rs?105:116,rs?118:138,rs?123:133);
    dials[1].slider->setBounds(1053,105,132,128);
    dials[2].slider->setBounds(581,67,74,77);
    dials[3].slider->setBounds(579,181,76,77);
    dials[4].slider->setBounds(147,183,48,47);
    dials[1].slider->setVisible(rs);
    dials[2].slider->setVisible(rs);
    dials[3].slider->setVisible(rs);
    dials[4].slider->setVisible(rs);
    superFuse.setVisible(rs);
    outputLoad.setVisible(rs);
    altecModButton.setVisible(!rs);
}
void TubeCompEditor::timerCallback() {
    const bool rs=proc.state.getRawParameterValue("rs124")->load()>.5f;
    if((face.getMode()==TubeCompFaceplate::Mode::rs124)!=rs)setMode(rs);
    const float in=proc.state.getRawParameterValue("input")->load();
    const float out=proc.state.getRawParameterValue("output")->load();
    const float at=proc.state.getRawParameterValue("attack")->load();
    const float rec=proc.state.getRawParameterValue("recovery")->load();
    face.setControlPositions(in*.1f,(out+10.f)*.1f,
                             (at-1.f)/8.f,rec*.1f);
    face.setMeterDb(proc.grMeter.load(std::memory_order_relaxed));
    face.setPowerOn(proc.state.getRawParameterValue("bypass")->load()<.5f);
    face.setAltecAttack(int(std::lround(proc.state.getRawParameterValue("altecAttack")->load())));
    face.setAltecRecovery(int(std::lround(proc.state.getRawParameterValue("altecRecovery")->load())));
    face.setAltecMod(!rs && proc.state.getRawParameterValue("altecMod")->load()>.5f);
}
void TubeCompEditor::resized() {
    face.setBounds(0,0,getWidth(),getHeight());
    superFuse.setBounds(539,312,124,23);
    outputLoad.setBounds(675,312,124,23);
    bypass.setBounds(811,312,124,23);
    altecModButton.setBounds(940,312,100,23);
    // Dedicated non-overlapping trim slots, with full-width readouts.
    // BYPASS follows impedance; digital trims stay next to the logo.
    inputTrimLabel.setBounds(168,313,42,21);
    digitalInputTrim.setBounds(214,313,126,21);
    outputTrimLabel.setBounds(348,313,48,21);
    digitalOutputTrim.setBounds(400,313,126,21);
}
