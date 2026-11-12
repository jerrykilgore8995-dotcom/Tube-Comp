#pragma once
// True JUCE-rendered front-panel studies for Tube Comp.
// Layout references: Chandler RS124 and 436B original hardware.
// No vendor logos or raster copies; paint everything with JUCE vector primitives.
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include <algorithm>

class TubeCompFaceplate final: public juce::Component {
public:
    enum class Mode { rs124, altec436b };
    void setMode(Mode m) {mode=m;repaint();}
    Mode getMode() const noexcept {return mode;}
    void setMeterDb(float db) noexcept {meterDb=juce::jlimit(0.f,30.f,db);repaint();}
    TubeCompFaceplate(){setSize(1600,350);}
    void paint(juce::Graphics& g) override {
        auto r=getLocalBounds().toFloat();
        g.fillAll(juce::Colour::fromRGB(19,22,25));
        // Stable 19-inch equivalent 2U aspect ratio; separate narrow mode rail.
        const auto panel=juce::Rectangle<float>(12,16,1576,280);
        if(mode==Mode::rs124)paintRS124(g,panel);
        else paintAltec(g,panel);
        drawModeRail(g);
    }
    void mouseDown(const juce::MouseEvent& e) override {
        if(e.position.y>308.f) {
            if(e.position.x>1178.f&&e.position.x<1590.f)
                setMode(mode==Mode::rs124?Mode::altec436b:Mode::rs124);
        }
    }
private:
    Mode mode=Mode::rs124;
    float meterDb=5.f;
    static constexpr float pi=3.14159265358979323846f;
    using C=juce::Colour;
    static juce::Font f(float px,bool heavy=false) {
        return juce::Font(juce::FontOptions(px,heavy?juce::Font::bold:juce::Font::plain));
    }
    static C rgb(unsigned r,unsigned g,unsigned b) {return C::fromRGB((juce::uint8)r,(juce::uint8)g,(juce::uint8)b);}
    static void label(juce::Graphics& g,juce::String str,float x,float y,float w,float h,
                      float size,C color,bool bold=true,
                      juce::Justification j=juce::Justification::centred) {
        g.setColour(color);g.setFont(f(size,bold));
        g.drawFittedText(str,juce::Rectangle<int>((int)x,(int)y,(int)w,(int)h),j,1);
    }
    static void screw(juce::Graphics& g,float x,float y,float radius=9) {
        g.setColour(rgb(16,19,21));g.fillEllipse(x-radius-1,y-radius-1,2*radius+2,2*radius+2);
        g.setGradientFill(juce::ColourGradient(rgb(198,201,201),x-radius,y-radius,
                            rgb(66,69,73),x+radius,y+radius,false));
        g.fillEllipse(x-radius,y-radius,2*radius,2*radius);
        g.setColour(rgb(26,27,28));g.drawLine(x-radius*.62f,y-radius*.32f,
                        x+radius*.62f,y+radius*.32f,2.2f);
    }
    static void mountSlots(juce::Graphics& g,const juce::Rectangle<float>& p) {
        const float xx[2]={p.getX()+19,p.getRight()-19};
        const float yy[2]={p.getY()+17,p.getBottom()-17};
        for(float x:xx)for(float y:yy) {
            g.setColour(rgb(31,33,34));g.fillRoundedRectangle(x-12,y-6,24,12,6);
            g.setColour(rgb(234,236,236));g.fillRoundedRectangle(x-9,y-4,18,8,4);
        }
        for(float x:{p.getX()+75,p.getRight()-75}) for(float y:{p.getY()+23,p.getBottom()-23})
            screw(g,x,y,7.f);
    }
    static void plate(juce::Graphics& g,juce::Rectangle<float> p,C top,C bottom,bool silver) {
        g.setGradientFill(juce::ColourGradient(top,p.getX(),p.getY(),bottom,
                           p.getRight(),p.getBottom(),false));
        g.fillRoundedRectangle(p,4.f);
        // Subtle brushed surface and engraved outer seam.
        for(int i=0;i<(int)p.getHeight();i+=3) {
            g.setColour(C::fromRGBA(255,255,255,(juce::uint8)(silver?12:6)));
            g.drawHorizontalLine((int)p.getY()+i,p.getX()+4,p.getRight()-4);
        }
        g.setColour(silver?rgb(63,67,68):rgb(9,14,12));
        g.drawRoundedRectangle(p.reduced(1),4.f,1.8f);
        g.setColour(C::fromRGBA(255,255,255,55));
        g.drawHorizontalLine((int)p.getY()+3,p.getX()+4,p.getRight()-4);
        mountSlots(g,p);
    }
    static void jewel(juce::Graphics& g,float x,float y) {
        g.setColour(rgb(37,35,33));g.fillEllipse(x-17,y-17,34,34);
        g.setGradientFill(juce::ColourGradient(rgb(241,246,245),x-13,y-13,
            rgb(73,77,80),x+14,y+14,false));
        g.fillEllipse(x-13,y-13,26,26);
        g.setColour(rgb(103,18,15));g.fillEllipse(x-9,y-9,18,18);
        g.setGradientFill(juce::ColourGradient(rgb(255,241,192),x-5,y-7,
            rgb(239,30,14),x+8,y+9,false));
        g.fillEllipse(x-7,y-7,14,14);
        g.setColour(C::fromRGBA(255,116,52,78));g.fillEllipse(x-14,y-14,28,28);
        g.setColour(C::fromRGBA(255,255,255,175));g.fillEllipse(x-4,y-7,5,5);
    }
    static void knob(juce::Graphics& g,float x,float y,float r,float val,
                     int ticks,bool metal=false) {
        const float a0=pi*.78f,a1=pi*2.22f;
        g.setColour(rgb(33,36,37));g.fillEllipse(x-r-9,y-r-9,2*r+18,2*r+18);
        g.setColour(rgb(115,118,119));g.drawEllipse(x-r-9,y-r-9,2*r+18,2*r+18,1.8f);
        // Knurled physical edge.
        for(int i=0;i<34;++i){
            const float a=2*pi*i/34.f;
            const float aX=x+std::cos(a)*r*.91f,aY=y+std::sin(a)*r*.91f;
            const float bX=x+std::cos(a)*r*1.07f,bY=y+std::sin(a)*r*1.07f;
            g.setColour(i%2?rgb(27,28,29):rgb(99,100,102));
            g.drawLine(aX,aY,bX,bY,2.f);
        }
        g.setGradientFill(juce::ColourGradient(
            metal?rgb(218,223,224):rgb(91,94,96),x-r,y-r,
            metal?rgb(86,93,94):rgb(17,19,21),x+r,y+r,false));
        g.fillEllipse(x-r,y-r,2*r,2*r);
        g.setColour(metal?rgb(90,94,96):rgb(9,11,12));
        g.drawEllipse(x-r+5,y-r+5,2*r-10,2*r-10,2.4f);
        // Molded finger grip and tapered marker.
        const float a=a0+(a1-a0)*juce::jlimit(0.f,1.f,val);
        const float dx=std::cos(a),dy=std::sin(a);
        g.setColour(rgb(240,241,230));
        g.drawLine(x+dx*r*.26f,y+dy*r*.26f,
                   x+dx*r*.86f,y+dy*r*.86f,3.f);
        g.setColour(C::fromRGBA(255,255,255,48));
        juce::Path highlight;
        highlight.addCentredArc(x,y,r-8.f,r-8.f,0.f,
                                pi*.90f,pi*1.50f,true);
        g.strokePath(highlight,juce::PathStrokeType(1.5f));
        // Control index ticks form period-correct stamped legends.
        for(int i=0;i<ticks;++i){
            const float t=float(i)/float(ticks-1);
            const float theta=a0+(a1-a0)*t;
            const float cx=std::cos(theta),cy=std::sin(theta);
            const float rr=r+15.f;
            g.setColour(rgb(41,42,42));
            g.drawLine(x+cx*rr,y+cy*rr,x+cx*(rr+4),y+cy*(rr+4),1.3f);
        }
    }
    static void knobNumbers(juce::Graphics& g,float x,float y,float r,int max,
                            C inkColor) {
        const float a0=pi*.78f,a1=pi*2.22f;
        for(int i=0;i<=max;++i){
            const float a=a0+(a1-a0)*i/max;
            const float rr=r+32;
            label(g,juce::String(i),x+std::cos(a)*rr-10,y+std::sin(a)*rr-7,
                  20,14,12,inkColor);
        }
    }
    static void dial(juce::Graphics& g,float x,float y,float r,float val,
                     C inkColor,bool numbered=true) {
        knob(g,x,y,r,val,11);
        if(numbered)knobNumbers(g,x,y,r,10,inkColor);
    }
    static void vu(juce::Graphics& g,juce::Rectangle<float> p,float db,bool rs) {
        g.setColour(rgb(20,23,24));g.fillRoundedRectangle(p.expanded(7),6.f);
        g.setGradientFill(juce::ColourGradient(rgb(59,59,58),p.getX(),p.getY(),
                             rgb(8,10,11),p.getRight(),p.getBottom(),false));
        g.fillRoundedRectangle(p.expanded(3),3.f);
        auto window=p.reduced(8);
        g.setGradientFill(juce::ColourGradient(rgb(255,253,240),
                             window.getX(),window.getY(),
                             rgb(220,216,197),window.getRight(),window.getBottom(),false));
        g.fillRoundedRectangle(window,2.f);
        const float cx=window.getCentreX(),base=window.getBottom()+13;
        const float sweep0=pi*1.22f,sweep1=pi*1.78f;
        for(int i=0;i<=6;++i){
            const float pos=(float)i/6.f;
            const float ang=sweep0+(sweep1-sweep0)*pos;
            const float x1=cx+std::cos(ang)*108,y1=base+std::sin(ang)*108;
            const float x2=cx+std::cos(ang)*117,y2=base+std::sin(ang)*117;
            g.setColour(rgb(58,57,55));g.drawLine(x1,y1,x2,y2,i%2?1.1f:1.8f);
            const int n=30-5*i;
            label(g,juce::String(n),cx+std::cos(ang)*95-11,
                  base+std::sin(ang)*95-7,22,14,11,rgb(43,43,42));
        }
        label(g,"dB  COMPRESSION",window.getX()+25,window.getY()+70,
              window.getWidth()-50,17,13,rgb(55,54,50));
        const float angle=sweep0+(sweep1-sweep0)*(1.f-db/30.f);
        const float tipX=cx+std::cos(angle)*121,tipY=base+std::sin(angle)*121;
        g.setColour(rgb(30,26,26));g.drawLine(cx,base,tipX,tipY,2.1f);
        g.setColour(rgb(11,13,14));g.fillEllipse(cx-7,base-7,14,14);
        g.setColour(C::fromRGBA(255,255,255,55));
        g.drawRoundedRectangle(window,2.f,1.0f);
        if(!rs) {
            // Classic Altec-style protruding bottom meter housing.
            g.setColour(rgb(19,22,21));
            g.fillRoundedRectangle(cx-34,p.getBottom()-8,68,44,11.f);
            screw(g,cx,p.getBottom()+17,6);
        }
    }
    static void smallSwitch(juce::Graphics& g,float x,float y,
                            juce::String labelText,bool up=true) {
        g.setColour(rgb(26,28,29));g.fillEllipse(x-18,y-18,36,36);
        g.setGradientFill(juce::ColourGradient(rgb(171,175,175),x-9,y-9,
                           rgb(58,59,60),x+8,y+8,false));
        g.fillEllipse(x-12,y-12,24,24);
        g.setColour(rgb(26,27,28));g.drawLine(x,y,
            x+(up?3.f:5.f),y+(up?-12.f:11.f),4.f);
        label(g,labelText,x-42,y+23,84,16,11,rgb(28,29,29));
    }
    void paintRS124(juce::Graphics& g,juce::Rectangle<float> p) {
        plate(g,p,rgb(173,172,169),rgb(125,127,127),true);
        const C ink=rgb(36,38,39);
        // Hardware controls in the approximate real Chandler 2U locations.
        label(g,"BAL.",126,146,83,22,14,rgb(250,250,246));
        screw(g,168,103,8);
        g.setGradientFill(juce::ColourGradient(rgb(227,234,235),161,189,
                           rgb(64,71,75),179,216,false));
        g.fillEllipse(155,191,34,34);
        g.setColour(rgb(21,23,25));g.drawEllipse(155,191,34,34,2.f);
        label(g,"INPUT",283,38,118,20,14,ink);
        label(g,"CONTROL",283,57,118,15,12,ink);
        dial(g,342,163,52,.56f,ink,true);
        label(g,"ATTACK",546,35,120,18,14,ink);
        knob(g,605,94,29,.36f,9);
        label(g,"RECOVERY",548,274,120,17,13,ink);
        knob(g,605,207,29,.52f,11);
        label(g,"SLOW",541,167,43,16,12,ink);
        label(g,"FAST",625,165,43,16,12,ink);
        label(g,"HOLD",581,238,46,15,11,rgb(175,36,31));
        vu(g,{728,67,250,163},meterDb,true);
        label(g,"OUTPUT",1029,37,130,21,14,ink);
        label(g,"ATTENUATOR  (dB)",1014,55,160,17,12,ink);
        dial(g,1094,164,50,.27f,ink,false);
        for(int i=0;i<=10;i+=2){
            const float a=pi*.78f+(pi*2.22f-pi*.78f)*float(i)/10.f;
            label(g,i==0?"0":("-"+juce::String(i)),
                  1094+std::cos(a)*83-13,164+std::sin(a)*83-8,
                  27,14,12,ink);
        }
        jewel(g,1250,93);
        label(g,"FUSE",1320,57,92,18,13,ink);
        g.setColour(rgb(23,24,25));g.fillEllipse(1347,87,38,38);
        g.setColour(rgb(123,126,127));g.drawEllipse(1347,87,38,38,2.f);
        label(g,"FUSE",1351,98,30,14,10,rgb(227,225,223));
        smallSwitch(g,1367,210,"POWER",true);
        label(g,"OFF",1323,169,33,16,10,ink);
        label(g,"ON",1393,169,33,16,10,ink);
        label(g,"COMPRESSOR",1435,137,130,19,13,ink);
        label(g,"TYPE RS124",1436,156,130,22,16,ink);
        label(g,"TUBE COMP",1437,181,126,16,11,ink);
    }
    void paintAltec(juce::Graphics& g,juce::Rectangle<float> p) {
        plate(g,p,rgb(56,69,62),rgb(29,41,37),false);
        const C bone=rgb(236,231,208);
        label(g,"A-436B",150,67,300,65,48,bone);
        label(g,"COMPRESSOR AMPLIFIER",150,120,300,24,18,bone);
        label(g,"INPUT",192,155,120,19,14,bone);
        label(g,"LEVEL",192,174,120,19,14,bone);
        dial(g,239,232,43,.64f,bone,true);
        vu(g,{655,65,272,161},meterDb,false);
        label(g,"GAIN REDUCTION",699,263,188,20,14,bone);
        jewel(g,1118,92);
        label(g,"POWER",1057,132,120,17,14,bone);
        smallSwitch(g,1122,193,"ON / OFF",true);
        label(g,"FUSE",1332,101,95,18,13,bone);
        g.setColour(rgb(16,22,18));g.fillEllipse(1355,131,48,48);
        g.setColour(rgb(132,138,131));g.drawEllipse(1355,131,48,48,2);
        label(g,"1/2A",1364,145,31,16,11,bone);
        label(g,"A-436B",1190,244,280,21,15,bone);
        label(g,"STOCK FIXED ATTACK & RECOVERY",1156,265,340,14,10,bone);
    }
    void drawModeRail(juce::Graphics& g) {
        auto rail=juce::Rectangle<float>(12,303,1576,33);
        g.setColour(rgb(29,32,35));g.fillRoundedRectangle(rail,2.f);
        g.setColour(rgb(71,78,84));g.drawRoundedRectangle(rail.reduced(.7f),2.f,1.3f);
        label(g,"MODEL / CIRCUIT SELECT",50,311,270,15,11,rgb(170,180,187),
              true,juce::Justification::centredLeft);
        label(g,"ALTEC 436B BASE + RS124 RECONSTRUCTION",355,311,570,15,11,rgb(136,145,150));
        auto t=juce::Rectangle<float>(1200,308,356,23);
        g.setColour(rgb(12,15,18));g.fillRoundedRectangle(t,6.f);
        auto left=juce::Rectangle<float>(1202,310,174,19);
        auto right=juce::Rectangle<float>(1377,310,177,19);
        g.setColour(mode==Mode::rs124?rgb(60,122,153):rgb(49,56,57));
        g.fillRoundedRectangle(left,5.f);
        g.setColour(mode==Mode::altec436b?rgb(53,104,66):rgb(49,56,57));
        g.fillRoundedRectangle(right,5.f);
        label(g,"RS124",1208,312,162,15,12,rgb(229,236,234));
        label(g,"A-436B",1383,312,167,15,12,rgb(229,236,234));
    }
};
