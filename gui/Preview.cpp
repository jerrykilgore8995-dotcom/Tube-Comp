// Capture both authentic JUCE-painted GUI modes on GitHub Windows runner.
#include "TubeCompFaceplate.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <iostream>
int main() {
    juce::ScopedJuceInitialiser_GUI init;
    TubeCompFaceplate panel;
    panel.setVisible(true);
    panel.setMeterDb(8.f);
    const auto capture=[&](const juce::String& name) {
        const auto shot=panel.createComponentSnapshot(panel.getLocalBounds(),false,1.f);
        if(!shot.isValid()||shot.getWidth()!=1600||shot.getHeight()!=350){
            std::cerr<<"Invalid JUCE preview screenshot\n";return false;
        }
        auto file=juce::File::getCurrentWorkingDirectory().getChildFile(name);
        file.deleteFile();
        auto out=file.createOutputStream();
        if(!out)return false;
        juce::PNGImageFormat png;
        if(!png.writeImageToStream(shot,*out))return false;
        out->flush();
        std::cout<<name<<": actual JUCE snapshot captured\n";
        return true;
    };
    panel.setMode(TubeCompFaceplate::Mode::rs124);
    if(!capture("TubeComp_RS124_Actual_JUCE.png"))return 1;
    panel.setMode(TubeCompFaceplate::Mode::altec436b);
    if(!capture("TubeComp_A-436B_Actual_JUCE.png"))return 2;
    return 0;
}
