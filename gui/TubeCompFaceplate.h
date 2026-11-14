#pragma once
// TUBE COMP / hardware-faithful dual faceplate.
// Geometry traced from photographic Chandler RS124 reference and original
// Altec 436B specification / original-unit imagery, not a generic rack concept.
// Original brand logos have intentionally been substituted by RS124 / A-436B.
//
// Pure JUCE vector artwork: movable meter, real physical-looking pointer knobs,
// numbered switch legends, painted anodized chassis, indicator and fuse.
// These faceplates are still GUI studies, not the VST3 compression engine.
#include <juce_gui_basics/juce_gui_basics.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>

class TubeCompFaceplate final : public juce::Component
{
public:
    enum class Mode { rs124, altec436b };

    TubeCompFaceplate() { setSize(1600, 350); }
    std::function<void(Mode)> onModeSelected;
    std::function<void(bool)> onPowerSelected;
    std::function<void(int)> onAltecAttackSelected;
    std::function<void(int)> onAltecRecoverySelected;
    void setPowerOn(bool isOn) {powerOn=isOn;repaint();}
    void setAltecAttack(int position) {altecAttack=juce::jlimit(0,2,position);repaint();}
    void setAltecMod(bool on) {altecMod=on;repaint();}
    void setAltecRecovery(int position) {altecRecovery=juce::jlimit(0,4,position);repaint();}
    void setControlPositions(float input,float output,float attack,float recovery) {
        inPosition=juce::jlimit(0.f,1.f,input);
        outPosition=juce::jlimit(0.f,1.f,output);
        attackPosition=juce::jlimit(0.f,1.f,attack);
        recPosition=juce::jlimit(0.f,1.f,recovery);
        repaint();
    }
    void setMode(Mode m) { mode = m; repaint(); }
    Mode getMode() const noexcept { return mode; }
    void setMeterDb(float db) { meterDb = juce::jlimit(0.0f, 30.0f, db); repaint(); }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(col(17, 19, 22));
        if (mode == Mode::rs124) renderRS124(g);
        else render436B(g);
        renderChooser(g);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        // The painted physical power switch is an actual DAW bypass control.
        const auto point=e.position;
        if(mode==Mode::altec436b && altecMod && point.x>=1095 && point.x<=1192
           && point.y>=179 && point.y<=271) {
            const int selected=point.y<210?0:(point.y<240?1:2);
            setAltecAttack(selected);
            if(onAltecAttackSelected)onAltecAttackSelected(selected);
            return;
        }
        if(mode==Mode::altec436b && altecMod && point.x>=952 && point.x<=1053
           && point.y>=175 && point.y<=277) {
            const int selected=juce::jlimit(0,4,int((point.y-175)/20));
            setAltecRecovery(selected);
            if(onAltecRecoverySelected)onAltecRecoverySelected(selected);
            return;
        }
        const bool touchedPower=(mode==Mode::rs124)
            ? (point.x>1360&&point.x<1451&&point.y>185&&point.y<263)
            : (point.x>1180&&point.x<1263&&point.y>176&&point.y<251);
        if(touchedPower) {
            powerOn=!powerOn;
            if(onPowerSelected)onPowerSelected(powerOn);
            repaint();
            return;
        }
        if (e.position.y >= 313.0f) {
            if (e.position.x >= 1292.0f && e.position.x < 1440.0f) {
                setMode(Mode::rs124);
                if (onModeSelected) onModeSelected(mode);
            } else if (e.position.x >= 1440.0f && e.position.x <= 1582.0f) {
                setMode(Mode::altec436b);
                if (onModeSelected) onModeSelected(mode);
            }
        }
    }

private:
    using C = juce::Colour;
    static constexpr float pi = 3.14159265358979323846f;
    Mode mode = Mode::rs124;
    float meterDb = 8.0f;
    bool powerOn=true;
    int altecAttack=2;
    bool altecMod=false;
    int altecRecovery=0;
    float inPosition=.50f,outPosition=1.f,attackPosition=.375f,recPosition=.60f;

    static C col(int r, int g, int b) {
        return C::fromRGB((juce::uint8) r,(juce::uint8) g,(juce::uint8) b);
    }
    static juce::Font font(float size, bool bold = false) {
        return juce::Font(juce::FontOptions(size,
                   bold ? juce::Font::bold : juce::Font::plain));
    }
    static void stamp(juce::Graphics& g,const juce::String& content,
                      float x,float y,float w,float h,float size,C colour,
                      bool bold=false,juce::Justification justify=juce::Justification::centred)
    {
        g.setColour(colour);g.setFont(font(size,bold));
        g.drawFittedText(content,juce::Rectangle<int>((int)std::round(x),
                         (int)std::round(y),(int)std::round(w),
                         (int)std::round(h)),justify,1);
    }
    static void line(juce::Graphics& g,float x1,float y1,float x2,float y2,
                     C c,float thick=1.0f) {
        g.setColour(c);g.drawLine(x1,y1,x2,y2,thick);
    }
    static void metal(juce::Graphics& g,float x,float y,float w,float h,
                      bool green)
    {
        C start=green?col(51,65,52):col(166,170,172);
        C middle=green?col(42,57,46):col(176,177,176);
        C end=green?col(30,43,35):col(134,139,139);
        g.setGradientFill(juce::ColourGradient(start,x,y,end,x,y+h,false));
        g.fillRoundedRectangle(x,y,w,h,2.0f);
        g.setGradientFill(juce::ColourGradient(start,x,y,
                           middle,x+w*.85f,y+h*.5f,false));
        g.fillRoundedRectangle(x+2,y+2,w-4,h-4,1.0f);
        for (int k=0;k<(int)h;k+=2) {
            const float yy=y+k+1;
            g.setColour(C::fromRGBA(250,250,250,(juce::uint8)(green?4:7)));
            g.drawHorizontalLine((int)yy,x+3,x+w-3);
        }
        line(g,x+3,y+3,x+w-3,y+3,C::fromRGBA(248,249,246,56));
        line(g,x+3,y+h-3,x+w-3,y+h-3,
             green?col(20,29,22):col(93,99,102),1.5f);
        g.setColour(green?col(14,23,16):col(73,78,81));
        g.drawRoundedRectangle(x+1,y+1,w-2,h-2,2,1.4f);
    }

    static void panHead(juce::Graphics& g,float cx,float cy,float r=7.0f)
    {
        g.setColour(col(27,29,30));g.fillEllipse(cx-r-1,cy-r-1,2*r+2,2*r+2);
        g.setGradientFill(juce::ColourGradient(col(221,222,220),cx-r,cy-r,
                                             col(78,82,84),cx+r,cy+r,false));
        g.fillEllipse(cx-r,cy-r,2*r,2*r);
        line(g,cx-r*.60f,cy-r*.17f,cx+r*.60f,cy+r*.17f,col(31,33,32),1.8f);
    }
    static void ovalHole(juce::Graphics& g,float x,float y,bool green)
    {
        g.setColour(green?col(9,16,10):col(39,41,42));
        g.fillRoundedRectangle(x-13,y-7,26,14,6.0f);
        g.setColour(green?col(96,104,95):col(231,229,221));
        g.drawRoundedRectangle(x-12,y-6,24,12,5.0f,2.1f);
        g.setColour(col(23,26,25));
        g.fillRoundedRectangle(x-7,y-3,14,6,3.0f);
    }
    static void fittings(juce::Graphics& g,bool green)
    {
        for(float x : {31.0f,1569.0f})
            for(float y : {43.0f,279.0f})ovalHole(g,x,y,green);
        // On the original RS124 four countersunk screws are visible by the ears.
        for(float x : {91.0f,1509.0f})
            for(float y : {43.0f,279.0f})panHead(g,x,y,7.5f);
    }
    static void pilot(juce::Graphics& g,float cx,float cy)
    {
        g.setColour(col(18,21,23));g.fillEllipse(cx-23,cy-23,46,46);
        g.setGradientFill(juce::ColourGradient(col(233,235,229),cx-19,cy-19,
                    col(72,75,76),cx+18,cy+18,false));
        g.fillEllipse(cx-18,cy-18,36,36);
        g.setColour(col(79,17,10));g.fillEllipse(cx-13,cy-13,26,26);
        g.setGradientFill(juce::ColourGradient(col(255,216,175),cx-9,cy-10,
                                    col(225,28,13),cx+11,cy+11,false));
        g.fillEllipse(cx-11,cy-11,22,22);
        g.setColour(C::fromRGBA(255,106,58,75));g.fillEllipse(cx-16,cy-16,32,32);
        g.setColour(C::fromRGBA(255,249,225,186));g.fillEllipse(cx-6,cy-9,7,6);
    }
    static void fuse(juce::Graphics& g,float cx,float cy,bool green)
    {
        g.setColour(col(11,13,13));g.fillEllipse(cx-26,cy-26,52,52);
        g.setGradientFill(juce::ColourGradient(col(124,127,123),cx-22,cy-23,
                                             col(35,37,38),cx+23,cy+24,false));
        g.fillEllipse(cx-22,cy-22,44,44);
        g.setColour(col(18,19,20));g.fillEllipse(cx-18,cy-18,36,36);
        g.setColour(col(74,74,73));g.drawEllipse(cx-18,cy-18,36,36,1.2f);
        stamp(g,green?"1/2 A":"FUSE",cx-17,cy-7,34,14,9.5f,
              green?col(231,230,219):col(226,225,220),true);
    }
    static void powerToggle(juce::Graphics& g,float cx,float cy,bool isOn)
    {
        g.setColour(col(24,26,26));g.fillEllipse(cx-20,cy-20,40,40);
        g.setGradientFill(juce::ColourGradient(col(209,213,212),cx-16,cy-17,
                                             col(58,61,63),cx+16,cy+17,false));
        g.fillEllipse(cx-16,cy-16,32,32);
        g.setColour(col(24,26,27));g.fillEllipse(cx-9,cy-9,18,18);
        juce::Graphics::ScopedSaveState save(g);
        g.addTransform(juce::AffineTransform::rotation(isOn?-.48f:.48f,cx,cy));
        juce::Path arm;
        arm.addRoundedRectangle(cx-5.4f,cy-23.0f,11.0f,28.0f,3.5f);
        g.setGradientFill(juce::ColourGradient(col(30,31,31),cx-6,cy,
                                             col(88,88,85),cx+6,cy-22,false));
        g.fillPath(arm);
        g.setColour(col(10,11,12));g.strokePath(arm,juce::PathStrokeType(1.2f));
    }
    static void powerRotary(juce::Graphics& g,float cx,float cy,bool isOn)
    {
        g.setColour(col(18,21,18));g.fillEllipse(cx-25,cy-25,50,50);
        g.setGradientFill(juce::ColourGradient(col(86,92,85),cx-20,cy-20,
                                           col(13,17,16),cx+20,cy+20,false));
        g.fillEllipse(cx-21,cy-21,42,42);
        for(int i=0;i<17;++i) {
            float a=2*pi*i/17.f;
            line(g,cx+std::cos(a)*18,cy+std::sin(a)*18,
                    cx+std::cos(a)*23,cy+std::sin(a)*23,
                    i%2?col(20,22,20):col(99,101,95),1.6f);
        }
        g.setColour(col(23,24,22));
        juce::Path tab;
        tab.addRoundedRectangle(cx-6,cy-28,12,26,2);
        if(!isOn) {
            juce::Graphics::ScopedSaveState rotate(g);
            g.addTransform(juce::AffineTransform::rotation(pi*.65f,cx,cy));
            g.fillPath(tab);
        }
        g.fillPath(tab);
        line(g,cx-2,cy-19,cx-2,cy-7,col(147,146,138),1.7f);
    }
    static void escutcheon(juce::Graphics& g,float cx,float cy,float radius)
    {
        g.setColour(col(30,31,32));g.fillEllipse(cx-radius-4,cy-radius-4,
                                                 2*radius+8,2*radius+8);
        g.setGradientFill(juce::ColourGradient(col(120,124,122),
                         cx-radius,cy-radius,col(30,33,34),
                         cx+radius,cy+radius,false));
        g.fillEllipse(cx-radius,cy-radius,2*radius,2*radius);
        g.setColour(C::fromRGBA(255,255,255,78));
        g.drawEllipse(cx-radius+3,cy-radius+3,2*radius-6,2*radius-6,1.1f);
    }
    static void pointerKnob(juce::Graphics& g,float cx,float cy,float radius,float normalized)
    {
        // The RS124 uses tall, dark phenolic pointer/levers on input/output,
        // not a generic round aluminum potentiometer.
        escutcheon(g,cx,cy,radius*.73f);
        g.setColour(col(11,12,13));
        g.fillEllipse(cx-radius*.73f,cy-radius*.73f,1.46f*radius,1.46f*radius);
        const float angle=(-2.25f+normalized*4.50f);
        {
            juce::Graphics::ScopedSaveState save(g);
            g.addTransform(juce::AffineTransform::rotation(angle,cx,cy));
            juce::Path body;
            // tapered body / projecting ivory-ended lever pointing UP
            body.startNewSubPath(cx-13,cy+radius*.67f);
            body.cubicTo(cx-24,cy+radius*.31f,cx-27,cy-radius*.18f,cx-19,cy-radius*.55f);
            body.lineTo(cx-14,cy-radius*1.10f);
            body.quadraticTo(cx,cy-radius*1.30f,cx+14,cy-radius*1.10f);
            body.lineTo(cx+19,cy-radius*.55f);
            body.cubicTo(cx+27,cy-radius*.19f,cx+24,cy+radius*.31f,
                         cx+13,cy+radius*.67f);
            body.quadraticTo(cx,cy+radius*.88f,cx-13,cy+radius*.67f);
            body.closeSubPath();
            g.setColour(C::fromRGBA(3,4,6,135));
            g.fillPath(body,juce::AffineTransform::translation(3,4));
            g.setGradientFill(juce::ColourGradient(col(117,119,113),
                  cx-19,cy-radius*.95f,col(10,11,13),cx+23,cy+radius*.7f,false));
            g.fillPath(body);
            g.setColour(col(12,13,14));
            g.strokePath(body,juce::PathStrokeType(2.5f));
            line(g,cx-12,cy-radius*.93f,cx-8,cy+radius*.36f,
                 C::fromRGBA(237,239,233,112),1.5f);
            line(g,cx+7,cy-radius*.97f,cx+11,cy-radius*.34f,
                 C::fromRGBA(0,0,0,150),2);
            // engraved indicator in bright gray on pointed tip.
            line(g,cx,cy-radius*1.13f,cx,cy-radius*.80f,
                 col(233,233,226),2.8f);
        }
        g.setColour(C::fromRGBA(245,245,242,28));
        g.drawEllipse(cx-radius*.57f,cy-radius*.60f,1.1f*radius,1.1f*radius,1.1f);
    }
    static void selectKnob(juce::Graphics& g,float cx,float cy,float radius,float normalized)
    {
        escutcheon(g,cx,cy,radius*.88f);
        const float angle=-2.45f+4.9f*normalized;
        juce::Graphics::ScopedSaveState save(g);
        g.addTransform(juce::AffineTransform::rotation(angle,cx,cy));
        juce::Path shape;
        shape.startNewSubPath(cx-8,cy+radius*.58f);
        shape.quadraticTo(cx-19,cy+radius*.10f,cx-11,cy-radius*.5f);
        shape.lineTo(cx-6,cy-radius*1.28f);
        shape.quadraticTo(cx,cy-radius*1.55f,cx+6,cy-radius*1.28f);
        shape.lineTo(cx+11,cy-radius*.50f);
        shape.quadraticTo(cx+19,cy+radius*.10f,cx+8,cy+radius*.58f);
        shape.closeSubPath();
        g.setGradientFill(juce::ColourGradient(col(95,98,96),
                   cx-13,cy-radius,col(12,13,14),cx+12,cy+radius,false));
        g.fillPath(shape);
        g.setColour(col(12,12,13));g.strokePath(shape,juce::PathStrokeType(2.4f));
        line(g,cx-3,cy-radius*1.24f,cx-3,cy-radius*.79f,
             col(238,240,228),2.2f);
    }
    static void dialScale(juce::Graphics& g,float cx,float cy,float radius,
                          const std::array<juce::String,11>& labels,
                          C ink,float textSize=13.5f)
    {
        // Unlike the old GUI: values are individually stamped around the
        // circumference, with correct period control sequence and no overlaps.
        constexpr float a0=-2.50f,a1=2.50f;
        for(int i=0;i<11;++i){
            const float a=a0+(a1-a0)*i/10.f;
            const float rr=radius+21;
            const float tx=cx+std::sin(a)*rr,ty=cy-std::cos(a)*rr;
            line(g,cx+std::sin(a)*(radius+5),cy-std::cos(a)*(radius+5),
                    cx+std::sin(a)*(radius+11),cy-std::cos(a)*(radius+11),
                    ink,1.2f);
            stamp(g,labels[(size_t)i],tx-16,ty-10,32,20,textSize,ink,true);
        }
    }
    static void attackMarks(juce::Graphics& g,float cx,float cy,C ink)
    {
        const std::array<juce::String,9> nums={
            "1","2","3","4","5","6","7","8","9"};
        for(int i=0;i<9;++i){
            float a=-2.48f+4.96f*i/8.f;
            float x=cx+std::sin(a)*64,y=cy-std::cos(a)*59;
            stamp(g,nums[(size_t)i],x-13,y-9,26,18,11.3f,ink,true);
        }
    }
    static void recoveryMarks(juce::Graphics& g,float cx,float cy,C ink)
    {
        const std::array<juce::String,11> marks={
            "6","H","5","H","4","H","3","H","2","H","1"};
        for(int i=0;i<11;++i){
            float a=-2.38f+4.76f*i/10.f;
            float x=cx+std::sin(a)*57,y=cy-std::cos(a)*58;
            stamp(g,marks[(size_t)i],x-12,y-8,24,18,10.8f,
                  i%2?col(165,42,43):ink,true);
        }
    }
    void meterRS124(juce::Graphics& g,float x,float y)
    {
        const float w=257,h=171;
        g.setColour(col(20,20,21));g.fillRoundedRectangle(x-10,y-11,w+20,h+22,5);
        g.setGradientFill(juce::ColourGradient(col(33,35,35),x,y,
                           col(6,7,8),x+w,y+h,false));
        g.fillRoundedRectangle(x-4,y-4,w+8,h+8,2);
        g.setColour(col(89,91,89));g.drawRoundedRectangle(x-2,y-2,w+4,h+4,2,1.8f);
        g.setGradientFill(juce::ColourGradient(col(252,248,238),x+6,y+5,
                           col(220,215,204),x+w-5,y+h-9,false));
        g.fillRoundedRectangle(x+7,y+7,w-14,h-14,2);
        const float c=x+w*.5f, pivotY=y+h-17;
        const std::array<int,7> values={30,25,20,15,10,5,0};
        for(size_t i=0;i<values.size();++i){
            // right-to-left zero compression scale as in the hardware.
            const float a=pi*1.20f+(pi*.60f)*i/(values.size()-1);
            const float px=c+std::cos(a)*133,py=pivotY+std::sin(a)*133;
            const float qx=c+std::cos(a)*144,qy=pivotY+std::sin(a)*144;
            line(g,px,py,qx,qy,col(45,44,43),i%2?1.5f:2.2f);
            stamp(g,juce::String(values[i]),c+std::cos(a)*114-15,
                  pivotY+std::sin(a)*114-11,30,18,11.4f,col(44,44,41),true);
        }
        // The needle direction is left with GR; zero lives on the right.
        float needle=pi*1.80f-(pi*.60f)*meterDb/30.f;
        float tx=c+std::cos(needle)*151,ty=pivotY+std::sin(needle)*151;
        line(g,c,pivotY,tx,ty,col(27,25,22),2.2f);
        line(g,c+2,pivotY,tx+2,ty+1,C::fromRGBA(40,34,26,70));
        stamp(g,"dB",x+78,y+102,100,17,12,col(44,44,41));
        stamp(g,"COMPRESSION",x+43,y+119,170,16,11.8f,col(39,39,37));
        g.setColour(col(20,22,22));g.fillEllipse(c-7,pivotY-7,14,14);
        // Lower-right chrome screw for the black meter frame.
        panHead(g,x+w-12,y+h-12,5);
        g.setColour(C::fromRGBA(255,255,255,80));
        g.drawRoundedRectangle(x+7,y+7,w-14,h-14,2,1);
    }
    void meterAltec(juce::Graphics& g,float x,float y)
    {
        // Distinctive original Altec 6049 round-topped Bakelite meter,
        // NOT the modern rectangular VU housing.
        const float w=235,h=195;
        juce::Path housing;
        housing.startNewSubPath(x+28,y);
        housing.lineTo(x+w-28,y);
        housing.quadraticTo(x+w+8,y+9,x+w+7,y+46);
        housing.lineTo(x+w+5,y+h-43);
        housing.quadraticTo(x+w-2,y+h+4,x+w-42,y+h);
        housing.lineTo(x+42,y+h);
        housing.quadraticTo(x-4,y+h+4,x-5,y+h-43);
        housing.lineTo(x-7,y+46);
        housing.quadraticTo(x-8,y+9,x+28,y);
        housing.closeSubPath();
        g.setColour(col(10,13,12));g.fillPath(housing,juce::AffineTransform::translation(3,5));
        g.setGradientFill(juce::ColourGradient(col(28,31,28),x,y,
                                             col(5,8,7),x+w,y+h,false));
        g.fillPath(housing);
        g.setColour(col(91,92,82));g.strokePath(housing,juce::PathStrokeType(2));
        auto paper=juce::Rectangle<float>(x+12,y+10,w-24,h-60);
        g.setGradientFill(juce::ColourGradient(col(252,251,242),paper.getX(),paper.getY(),
                         col(216,212,198),paper.getRight(),paper.getBottom(),false));
        g.fillRoundedRectangle(paper,32);
        g.setColour(col(49,53,49));g.drawRoundedRectangle(paper,32,1.4f);
        const float cx=x+w/2,py=y+h-26;
        const std::array<int,5> ticks={30,20,15,10,5};
        for(size_t i=0;i<ticks.size();++i){
            const float a=pi*1.17f+pi*.63f*i/(ticks.size()-1);
            float xx=cx+std::cos(a)*116,yy=py+std::sin(a)*117;
            float tx=cx+std::cos(a)*103,ty=py+std::sin(a)*103;
            line(g,xx,yy,tx,ty,col(46,45,39),1.4f);
            stamp(g,juce::String(ticks[i]),cx+std::cos(a)*89-13,
                  py+std::sin(a)*92-8,26,17,11,col(51,49,42));
        }
        stamp(g,"DB COMPRESSION",x+30,y+93,w-60,15,10.3f,col(55,50,46));
        stamp(g,"A-436B",x+48,y+112,w-96,20,13.5f,col(57,60,51),true);
        const float angle=pi*1.80f-pi*.63f*meterDb/30.f;
        line(g,cx,py,cx+std::cos(angle)*114,py+std::sin(angle)*114,
             col(42,38,33),1.8f);
        g.setColour(col(13,15,14));g.fillEllipse(cx-7,py-7,14,14);
        g.setGradientFill(juce::ColourGradient(col(33,35,32),x+48,y+h-53,
                                             col(10,12,11),x+w-47,y+h-2,false));
        g.fillRoundedRectangle(x+47,y+h-56,w-94,53,15);
        panHead(g,cx,y+h-25,7);
    }

    void renderRS124(juce::Graphics& g)
    {
        metal(g,13,17,1574,283,false);
        fittings(g,false);
        const C ink=col(37,39,40);
        // Panel layout follows Chandler RS124 frontal photo, with only
        // names/identity adapted. No extra front-panel controls are inserted.
        panHead(g,159,81,8); // balance-adjust trimmer screw
        stamp(g,"BAL.",118,143,88,21,13.5f,ink,true);
        g.setGradientFill(juce::ColourGradient(col(239,242,238),152,182,
                                             col(108,115,118),188,218,false));
        g.fillEllipse(150,185,39,39); 
        g.setColour(col(42,45,46));g.drawEllipse(150,185,39,39,1.5f);
        // Tiny depressed central balancing button.
        g.setColour(col(215,216,212));g.fillEllipse(161,196,17,17);

        stamp(g,"I N P U T",280,29,160,19,13,ink,true);
        stamp(g,"C O N T R O L",270,46,180,17,11.5f,ink,true);
        const std::array<juce::String,11> ints={"0","1","2","3","4","5","6","7","8","9","10"};
        dialScale(g,356,166,48,ints,ink,12.4f);
        pointerKnob(g,356,166,46,inPosition);

        stamp(g,"A T T A C K",541,29,152,18,14,ink,true);
        attackMarks(g,617,105,ink);
        selectKnob(g,617,105,28,attackPosition);
        stamp(g,"60070B",519,57,62,15,9.6f,col(166,39,34),true);
        stamp(g,"60050A",651,57,66,15,9.6f,col(166,39,34),true);

        stamp(g,"S L O W",504,231,60,15,10.7f,ink,true);
        stamp(g,"F A S T",678,231,62,15,10.7f,ink,true);
        recoveryMarks(g,617,217,ink);
        selectKnob(g,617,217,27,recPosition);
        stamp(g,"R E C O V E R Y",542,274,148,17,11.7f,ink,true);

        meterRS124(g,744,66);

        stamp(g,"O U T P U T",1048,29,159,19,13.5f,ink,true);
        stamp(g,"A T T E N U A T O R  (dB)",1004,48,255,17,11.7f,ink,true);
        const std::array<juce::String,11> out={"-10","-9","-8","-7","-6","-5","-4","-3","-2","-1","0"};
        dialScale(g,1119,169,46,out,ink,12.f);
        pointerKnob(g,1119,169,45,outPosition);
        pilot(g,1262,101);
        stamp(g,"FUSE",1376,56,91,17,13,ink,true);
        fuse(g,1421,101,false);
        stamp(g,"OFF",1330,168,57,17,11,ink,true);
        stamp(g,"ON",1449,168,48,17,11,ink,true);
        powerToggle(g,1406,225,powerOn);
        stamp(g,"C O M P R E S S O R",1453,192,118,17,10.1f,ink,true);
        stamp(g,"T Y P E   R S 1 2 4",1453,211,118,16,9.8f,ink,true);
        stamp(g,"T U B E   C O M P",1453,232,118,15,9.4f,ink,true);
    }
    void render436B(juce::Graphics& g)
    {
        metal(g,13,17,1574,283,true);
        fittings(g,true);
        const C cream=col(235,237,216),thin=col(193,207,188);
        stamp(g,"V O L U M E",129,58,203,22,14,cream,true);
        // The original 436B has only one user variable input gain knob.
        const std::array<juce::String,11> ints={"0","1","2","3","4","5","6","7","8","9","10"};
        dialScale(g,234,181,49,ints,cream,12.6f);
        // Vintage Altec black fluted skirt, bright chickenhead pointer.
        pointerKnob(g,234,181,49,inPosition);

        // Replaces original brand name in its correct left-middle location.
        stamp(g,"A-436B",390,105,258,53,42,cream,true);
        line(g,404,158,636,158,thin,1.2f);
        stamp(g,"COMPRESSOR",400,164,244,21,16,cream,true);
        stamp(g,"AMPLIFIER",413,184,219,19,12.6f,cream);

        meterAltec(g,669,59);

        stamp(g,"436B",967,84,116,26,22,cream,true);
        stamp(g,"COMPRESSOR",965,114,181,22,14,cream,true);
        stamp(g,"AMPLIFIER",965,138,181,22,14,cream,true);
        pilot(g,1218,111);
        stamp(g,"P O W E R",1170,154,102,20,13,cream,true);
        powerRotary(g,1224,211,powerOn);
        stamp(g,"ON",1273,202,45,17,10.7f,cream);
        stamp(g,"OFF",1131,202,51,17,10.7f,cream);
        stamp(g,"FUSE",1377,76,103,21,13,cream,true);
        fuse(g,1430,133,true);
        stamp(g,".5 AMP",1375,170,110,18,10.5f,cream);
        // A-436B-only reversible mods: unmodified faceplate when Mod is OFF.
        if(altecMod) {
            const float ax=1118.f, ay=225.f;
            stamp(g,"ATK",1065,211,45,22,14,cream,true);
            g.setColour(col(13,20,15));g.fillRoundedRectangle(ax-15,ay-48,30,96,7);
            g.setColour(col(144,157,139));g.drawRoundedRectangle(ax-15,ay-48,30,96,7,1.4f);
            g.setColour(col(12,15,12));g.fillRoundedRectangle(ax-6,ay-34,12,68,5);
            const float hy=ay-27.f+27.f*altecAttack;
            g.setGradientFill(juce::ColourGradient(col(220,223,216),ax-9,hy-10,
                                                   col(64,74,69),ax+10,hy+11,false));
            g.fillRoundedRectangle(ax-9,hy-10,18,20,4);
            const char* anames[]={"FAST","MED","SLOW"};
            for(int k=0;k<3;++k)
                stamp(g,anames[k],1140,189.f+27.f*k,50,20,11.5f,
                      k==altecAttack?cream:col(137,155,137),k==altecAttack);

            const float rx=968.f, ry=225.f;
            stamp(g,"REC",925,211,38,22,13,cream,true);
            g.setColour(col(13,20,15));g.fillRoundedRectangle(rx-14,ry-51,28,102,7);
            g.setColour(col(144,157,139));g.drawRoundedRectangle(rx-14,ry-51,28,102,7,1.3f);
            g.setColour(col(12,15,12));g.fillRoundedRectangle(rx-5,ry-40,10,80,5);
            const float rhy=ry-40.f+20.f*altecRecovery;
            g.setGradientFill(juce::ColourGradient(col(220,223,216),rx-8,rhy-7,
                                                   col(64,74,69),rx+8,rhy+8,false));
            g.fillRoundedRectangle(rx-8,rhy-8,16,16,3);
            const char* rnames[]={"1 SEC","500","333","200","91"};
            for(int k=0;k<5;++k)
                stamp(g,rnames[k],990,176.f+20.f*k,65,18,11.2f,
                      k==altecRecovery?cream:col(137,155,137),k==altecRecovery);
            stamp(g,"REC ms",990,277,65,13,9,cream);
        }
        // Bottom-right factory model identity, not invented release controls.
        stamp(g,"A-436B",1363,250,135,23,14.3f,cream,true);
    }
    void renderChooser(juce::Graphics& g)
    {
        // Separate host UI strip OUTSIDE the historical faceplate geometry.
        // The actual 19-inch rack remains a complete self-contained panel.
        const float y=309.0f;
        g.setColour(col(15,18,20));g.fillRoundedRectangle(13,y,1574,30,3);
        g.setColour(col(55,65,69));g.drawRoundedRectangle(13,y,1574,30,3,1.0f);
        stamp(g,"TUBE COMP  /  CIRCUIT",40,y+4,300,21,11.5f,col(177,188,192),
              true,juce::Justification::centredLeft);
        const float l=1291.f,w=145.f;
        g.setColour(mode==Mode::rs124?col(76,108,124):col(48,53,56));
        g.fillRoundedRectangle(l,y+3,w,24,3);
        g.setColour(mode==Mode::altec436b?col(66,103,78):col(48,53,56));
        g.fillRoundedRectangle(l+w+3,y+3,w,24,3);
        stamp(g,"RS124",l+5,y+6,w-10,17,12,col(237,240,239),true);
        stamp(g,"A-436B",l+w+8,y+6,w-10,17,12,col(237,240,239),true);
    }
};
