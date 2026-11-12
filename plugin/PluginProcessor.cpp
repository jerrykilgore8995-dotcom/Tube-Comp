#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace {
using Layout=juce::AudioProcessorValueTreeState::ParameterLayout;
void addFloat(Layout& p,const char* id,const char* name,
              float min,float max,float step,float def) {
    p.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{id,1},name,
        juce::NormalisableRange<float>(min,max,step),def));
}
void addBool(Layout& p,const char* id,const char* name,bool def) {
    p.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{id,1},name,def));
}
float v(const juce::AudioProcessorValueTreeState& p,const char* key) noexcept {
    const auto* a=p.getRawParameterValue(key);
    return a==nullptr?0.f:a->load(std::memory_order_relaxed);
}
}
juce::AudioProcessorValueTreeState::ParameterLayout
TubeCompProcessor::createParameters() {
    Layout p;
    addBool(p,"rs124","RS124 / A-436B mode",true);
    addFloat(p,"input","Input",0.f,10.f,.01f,5.f);
    addFloat(p,"output","Output attenuator (dB)",-10.f,0.f,.01f,0.f);
    addFloat(p,"attack","RS124 attack position 1-9",1.f,9.f,1.f,4.f);
    // Physical recovery dial: 0=#6 slow, 1=HOLD ... 10=#1 fast.
    addFloat(p,"recovery","RS124 recovery selector",0.f,10.f,1.f,6.f);
    addFloat(p,"balance","Push-pull balance",-1.f,1.f,.005f,0.f);
    addBool(p,"superfuse","SuperFuse (reconstruction)",false);
    addFloat(p,"wet","Parallel blend",0.f,1.f,.001f,1.f);
    addBool(p,"bypass","Bypass",false);
    addFloat(p,"link","Stereo link",0.f,1.f,.01f,1.f);
    p.add(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID{"impedance",1},"Output impedance",juce::StringArray{
        "200 ohm","600 ohm"},1));
    return p;
}
TubeCompProcessor::TubeCompProcessor()
 : AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true)
                                  .withOutput("Output",juce::AudioChannelSet::stereo(),true)),
   state(*this,nullptr,"TubeCompState",createParameters()) {}
tubecomp::Parameters TubeCompProcessor::parametersFromState() const noexcept {
    tubecomp::Parameters p;
    p.rs124=v(state,"rs124")>.5f;
    p.input=v(state,"input");
    p.output=v(state,"output");
    p.attack=std::clamp(int(std::lround(v(state,"attack")))-1,0,8);
    p.recovery=std::clamp(int(std::lround(v(state,"recovery"))),0,10);
    p.balance=v(state,"balance");
    p.superfuse=v(state,"superfuse")>.5f;
    p.wet=v(state,"wet");
    p.bypass=v(state,"bypass")>.5f;
    p.stereoLink=v(state,"link");
    p.outputOhms=v(state,"impedance")>.5f?600.f:200.f;
    return p;
}
void TubeCompProcessor::prepareToPlay(double sampleRate,int samplesPerBlock) {
    engine.prepare((float)sampleRate,samplesPerBlock);
    engine.setParameters(parametersFromState(),true);
    // Accurate SPICE phase FIR has a fixed 16-sample group-delay offset.
    // Report this to the DAW for sample-accurate plugin delay compensation.
    setLatencySamples(tubecomp::phasefit::kLatency);
    grMeter.store(0.f);inputMeter.store(0.f);outputMeter.store(0.f);
}
void TubeCompProcessor::processBlock(juce::AudioBuffer<float>& audio,
                                     juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals guard;
    midi.clear();
    const int size=audio.getNumSamples();
    const int channels=audio.getNumChannels();
    if(channels<1)return;
    engine.setParameters(parametersFromState());
    auto* l=audio.getWritePointer(0);
    auto* r=channels>1?audio.getWritePointer(1):nullptr;
    float inputPeak=0,outputPeak=0;
    for(int i=0;i<size;++i) {
        float a=l[i],b=r?r[i]:a;
        inputPeak=std::max(inputPeak,std::max(std::fabs(a),std::fabs(b)));
        engine.process(a,b);
        l[i]=a;
        if(r)r[i]=b;
        outputPeak=std::max(outputPeak,std::max(std::fabs(a),std::fabs(b)));
    }
    grMeter.store(engine.meters().gainReduction,std::memory_order_relaxed);
    inputMeter.store(inputPeak,std::memory_order_relaxed);
    outputMeter.store(outputPeak,std::memory_order_relaxed);
}
juce::AudioProcessorEditor* TubeCompProcessor::createEditor() {
    return new TubeCompEditor(*this);
}
void TubeCompProcessor::getStateInformation(juce::MemoryBlock& dst) {
    std::unique_ptr<juce::XmlElement> xml(state.copyState().createXml());
    if(xml)copyXmlToBinary(*xml,dst);
}
void TubeCompProcessor::setStateInformation(const void* data,int length) {
    auto xml=getXmlFromBinary(data,length);
    if(xml&&xml->hasTagName(state.state.getType()))
        state.replaceState(juce::ValueTree::fromXml(*xml));
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new TubeCompProcessor();
}
