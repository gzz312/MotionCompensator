#include "dots.hpp"
#include "dynamic.hpp"
#include <iostream>
#include <limits>
#include <cstdlib>
void require(bool ok,const char* why) {if(!ok){std::cerr<<"FAIL: "<<why<<'\n';std::exit(1);}}
int main() {
    mc::RegionalDotField plain,enhanced;std::array<mc::Vec2,9> field{};
    field.fill({170,35});int moving=0;float previous=0;std::vector<mc::DotVisual> dots;
    for(int i=0;i<3600;++i) {
        plain.update(field,1000,600,122,1.f/60,true,false);
        dots=enhanced.update(field,1000,600,122,1.f/60,true,true);
        for(int r=0;r<9;++r) {
            require(plain.phases()[r].x==enhanced.phases()[r].x && plain.phases()[r].y==enhanced.phases()[r].y,"enhancement cannot change rolling speed, direction or phase");
            require(std::abs(plain.phases()[r].x)<122 && std::abs(plain.phases()[r].y)<122,"rolling phase remains bounded after many cycles");
        }
        if(i>3000 && std::abs(enhanced.phases()[4].x-previous)>1) ++moving;
        previous=enhanced.phases()[4].x;
    }
    require(moving==599 && enhanced.phaseMax()>6,"constant motion keeps rolling beyond rejected six-pixel design");
    for(auto d:dots) require(std::isfinite(d.position.x)&&std::isfinite(d.position.y)&&d.radiusScale>=.9f&&d.radiusScale<=1&&d.opacity>=.85f&&d.opacity<=1&&d.blendWeight>=0&&d.blendWeight<=1,"enhanced appearance stays finite and within slider upper limits");
    for(auto d:plain.dots()) {
        require(d.opacity==1 && d.radiusScale==1,"disabled enhancement restores unmodified dot appearance");
        float x=std::remainder(d.position.x-61-plain.phases()[d.region].x,122),y=std::remainder(d.position.y-61-plain.phases()[d.region].y,122);
        require(std::abs(x)<.001 && std::abs(y)<.001,"baseline dots retain original 0.6 regional lattice");
    }
    // A forward-input field has opposing local directions. Do not collapse it
    // into one global median as in the rejected 0.8 variant.
    for(int r=0;r<9;++r) field[r]={float(1-r%3)*91,float(1-r/3)*91};
    enhanced.reset();enhanced.update(field,1000,600,122,.016f,true);
    require(enhanced.phases()[0].x>0 && enhanced.phases()[0].y>0 && enhanced.phases()[8].x<0 && enhanced.phases()[8].y<0,"radial movement retains distinct regional directions");
    auto phase=enhanced.phases();
    enhanced.update(field,1000,600,122,0,true,false);
    require(enhanced.phases()[0].x==phase[0].x && enhanced.phases()[8].y==phase[8].y,"toggling appearance does not reset accumulated motion");
    require(mc::conservativeBoundaryFade(1000.f/3,100,0,1000,600,7.32f)==.75f,"boundary opacity never becomes an empty separator");
    require(mc::conservativeBoundaryFade(1000.f/3-8,100,0,1000,600,7.32f)==1,"narrow fade leaves region interior unchanged");
    float previousFade=1;
    for(int i=10;i>=0;--i) {float f=mc::conservativeBoundaryFade(1000.f/3-i,100,0,1000,600,7.32f);require(f<=previousFade && f>=.75f,"boundary fade approaches conservatively and monotonically");previousFade=f;}
    mc::RegionalDotField fadeOff,fadeOn;
    auto off=fadeOff.update(field,1000,600,122,.016f,true,true,false,3);
    auto on=fadeOn.update(field,1000,600,122,.016f,true,true,true,3);
    require(on.size()==off.size(),"boundary fade adds no overlapping dot populations");
    for(size_t i=0;i<on.size();++i)require(on[i].position.x==off[i].position.x && on[i].position.y==off[i].position.y && on[i].radiusScale==off[i].radiusScale && on[i].opacity>=off[i].opacity*.75f,"boundary treatment only attenuates opacity without changing motion or radius");
    enhanced.reset();field.fill({500,0});
    for(int i=0;i<120;++i) dots=enhanced.update(field,1000,600,122,.016f,true);
    float minSize=1,maxSize=0;
    for(auto d:dots) if(d.position.x>=0 && d.position.x<=1000 && d.position.y>=0 && d.position.y<=600) {minSize=std::min(minSize,d.radiusScale);maxSize=std::max(maxSize,d.radiusScale);}
    require(maxSize-minSize>.03f,"rolling layer also applies directional size modulation");
    dots=enhanced.update(field,1000,600,122,.016f,false);
    require(enhanced.phaseMax()==0,"manual static clears phase");
    for(auto d:dots) require(d.opacity==1 && d.radiusScale==1,"manual static ignores enhancement and moving input");
    field.fill({std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()});
    dots=enhanced.update(field,1000,600,122,.016f,true);
    for(auto d:dots) require(std::isfinite(d.position.x)&&std::isfinite(d.position.y)&&std::isfinite(d.opacity),"invalid velocity cannot corrupt lattice");
    require(enhanced.update(field,0,0,122,.016f,true).empty(),"invalid viewport clears dots");
    mc::DynamicDriver driver;mc::InputState input;mc::MotionSample invalid;
    enhanced.reset();
    for(int i=0;i<600;++i) {double now=10+i*.016;input.timestamp=now;input.mouseRate={300,0};auto v=driver.update(invalid,input,now,.016,true,true);enhanced.update(v,1000,600,122,.016,true);}
    require(driver.source()==mc::DriveSource::Input && enhanced.phaseMax()>0,"missing optical flow still drives the rolling dots from input");
    require(mc::dotVisibility(500,300,1000,600,3*.9f,.1f)==0,"size enhancement retains central exclusion");
    std::cout<<"PASS: original rolling lattice, phase equivalence, continuous motion, radial field, enhancement toggle, conservative boundary opacity, appearance, manual static, input fallback and finite values\n";
}
