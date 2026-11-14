#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <iostream>
int main(){
 juce::ScopedJuceInitialiser_GUI init;
 TubeCompProcessor processor;
 processor.prepareToPlay(44100.0,256);
 auto set=[&](const char* key,float value){
   auto* p=processor.state.getParameter(key);
   p->setValueNotifyingHost(p->convertTo0to1(value));
 };
 auto capture=[&](bool rs,bool mod,const char* name){
   set("rs124",rs?1.f:0.f);set("altecMod",mod?1.f:0.f);
   TubeCompEditor editor(processor);
   editor.setVisible(true);editor.resized();
   const auto shot=editor.createComponentSnapshot(editor.getLocalBounds(),false,1.f);
   if(!shot.isValid()||shot.getWidth()!=1600||shot.getHeight()!=350)return false;
   auto file=juce::File::getCurrentWorkingDirectory().getChildFile(name);
   file.deleteFile();auto output=file.createOutputStream();
   if(!output)return false;
   juce::PNGImageFormat png;
   if(!png.writeImageToStream(shot,*output))return false;
   output->flush();std::cout<<name<<" captured from complete JUCE editor"<<std::endl;
   return true;
 };
 return capture(true,false,"RS124.png") &&
        capture(false,false,"A436B_Mod_OFF.png") &&
        capture(false,true,"A436B_Mod_ON.png") ? 0 : 1;
}
