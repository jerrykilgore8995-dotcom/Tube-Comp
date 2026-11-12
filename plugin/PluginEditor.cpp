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
    for(const char* id: {"input","output","attack","recovery","balance"})
        makeDial(id);
    bypass.setButtonText("BYPASS");
    bypass.setClickingTogglesState(true);
    superFuse.setButtonText("SUPERFUSE");
    superFuse.setClickingTogglesState(true);
    for(auto* b:{&bypass,&superFuse}){
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
    outputLoad.addItem("200 OHM",1);outputLoad.addItem("600 OHM",2);
    addAndMakeVisible(outputLoad);
    loadAttach=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        p.state,"impedance",outputLoad);
    setMode(proc.state.getRawParameterValue("rs124")->load()>.5f);
    resized();
    timerCallback();
    startTimerHz(30);
}
TubeCompEditor::~TubeCompEditor(){stopTimer();face.onModeSelected=nullptr;}
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
}
void TubeCompEditor::resized() {
    face.setBounds(0,0,getWidth(),getHeight());
    bypass.setBounds(421,312,95,23);
    superFuse.setBounds(539,312,124,23);
    outputLoad.setBounds(694,312,113,23);
}
