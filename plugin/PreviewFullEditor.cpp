#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <iostream>
int main(){
 juce::ScopedJuceInitialiser_GUI init;
 TubeCompProcessor processor;
 processor.prepareToPlay(44100.0,256);
 auto* mode=processor.state.getParameter("rs124");
 mode->setValueNotifyingHost(mode->convertTo0to1(1.f));
 TubeCompEditor editor(processor);
 editor.setVisible(true);
 editor.resized();
 const auto shot=editor.createComponentSnapshot(editor.getLocalBounds(),false,1.f);
 if(!shot.isValid()||shot.getWidth()!=1600||shot.getHeight()!=350)return 2;
 auto file=juce::File::getCurrentWorkingDirectory().getChildFile("TubeComp_RS124_Full_Editor.png");
 file.deleteFile();
 auto output=file.createOutputStream();
 if(!output)return 3;
 juce::PNGImageFormat png;
 if(!png.writeImageToStream(shot,*output))return 4;
 output->flush();
 std::cout<<"Exact RS124 plugin-editor screenshot saved"<<std::endl;
 return 0;
}
