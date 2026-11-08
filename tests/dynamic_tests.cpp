#include "dynamic.hpp"
#include <iostream>
#include <limits>
#include <cstdlib>
void require(bool ok,const char* text) {if(!ok) {std::cerr<<"FAIL: "<<text<<'\n';std::exit(1);}}
int main() {
    mc::DynamicDriver driver;mc::MotionSample unavailable;mc::InputState input;
    std::array<mc::Vec2,9> offsets{};
    int lateMovingFrames=0;
    for(int i=0;i<600;++i) {
        double now=10+i*.016;input.timestamp=now;input.mouseRate={300,0};
        offsets=driver.update(unavailable,input,now,.016,true,true);
        if(i>300 && offsets[4].x>100) ++lateMovingFrames;
    }
    require(driver.source()==mc::DriveSource::Input && offsets[4].x>10,"rejected flow never stops continuous mouse-driven motion");
    require(lateMovingFrames==299,"constant input keeps moving after old displacement would saturate");
    driver.reset();
    input.timestamp=20;input.mouseRate={};input.move={0,1};
    for(int i=0;i<40;++i) {input.timestamp=20+i*.016;offsets=driver.update(unavailable,input,input.timestamp,.016,true,true);}
    require(offsets[0].x>5 && offsets[0].y>5 && offsets[8].x<-5 && offsets[8].y<-5,"forward input produces peripheral reverse expansion");
    driver.reset();input={};input.look={1,0};input.timestamp=22;
    for(int i=0;i<40;++i) {input.timestamp=22+i*.016;offsets=driver.update(unavailable,input,input.timestamp,.016,true,true);}
    require(offsets[4].x>20,"controller look drives missing-flow frame");
    offsets=driver.update(unavailable,input,input.timestamp,.016,false,true);
    require(driver.source()==mc::DriveSource::Static && offsets[4].x==0,"only explicit static produces static mode");
    driver.update(unavailable,input,input.timestamp,.016,true,true);
    offsets=driver.update(unavailable,input,input.timestamp,.016,true,false);
    require(driver.source()==mc::DriveSource::Suspended && offsets[4].x==0,"focus loss clears motion");
    driver.reset();input={};input.timestamp=30;input.mouseRate={100,0};
    driver.update(unavailable,input,30,.016,true,true);
    mc::MotionSample flow;flow.timestamp=30;flow.confidence=1;flow.velocity.fill({-140,0});
    for(int i=1;i<=5;++i) {input.timestamp=30+i*.016;offsets=driver.update(flow,input,input.timestamp,.016,true,true);}
    require(driver.source()==mc::DriveSource::Fused && offsets[4].x<50.f,"steady aligned input is not counted twice with measured flow");
    flow.timestamp=1;flow.velocity.fill({2000,0});input.timestamp=31;
    offsets=driver.update(flow,input,31,.016,true,true);
    require(driver.source()==mc::DriveSource::Input && offsets[4].x>0,"expired flow cannot override input prediction");
    driver.reset();input={};input.timestamp=40;flow.timestamp=40;flow.confidence=1;flow.velocity.fill({100,0});
    driver.update(flow,input,40,.016,true,true);input.timestamp=40.15;
    driver.update(unavailable,input,40.15,.016,true,true);
    require(driver.source()==mc::DriveSource::Coast,"brief input-free gap uses bounded continuation");
    input.timestamp=41;offsets=driver.update(unavailable,input,41,.016,true,true);
    require(driver.source()==mc::DriveSource::Ready,"no input remains dynamic-ready rather than auto-static");
    driver.reset();input.timestamp=42;input.mouseRate.x=std::numeric_limits<float>::quiet_NaN();
    offsets=driver.update(unavailable,input,42,.016,true,true);
    require(std::isfinite(offsets[4].x),"invalid input cannot poison rendering");
    require(mc::stickAxis(7000,7849)==0,"controller deadzone rejects drift");
    require(mc::stickAxis(-32768,7849)==-1 && mc::stickAxis(32767,7849)==1,"signed stick endpoints normalize safely");
    driver.reset();input={};input.mouseRate={100,0};
    flow={};flow.confidence=1;flow.regionConfidence.fill(1);flow.velocity.fill({100,0});flow.velocity[7]={};flow.velocity[8]={};
    flow.globalConfidence=1;flow.globalVelocity.fill({100,0});flow.movingBackgroundConfidence=1;flow.movingBackgroundVelocity.fill({100,0});
    for(int i=0;i<60;++i){double now=50+i*.016;input.timestamp=now;flow.timestamp=now-.03;flow.completedTimestamp=now-.01;driver.update(flow,input,now,.016,true,true);}
    require(driver.backgroundBlend()>.99f && driver.observedFlow()[8].x>99,"active input uses background flow even when a weapon makes local flow zero");
    input.mouseRate={};
    for(int i=0;i<100;++i){double now=51+i*.016;input.timestamp=now;flow.timestamp=now-.03;flow.completedTimestamp=now-.01;driver.update(flow,input,now,.016,true,true);}
    require(driver.backgroundBlend()<.001f && std::abs(driver.observedFlow()[8].x)<1,"without input the same retained local measurements smoothly regain control");
    driver.reset();input.move={0,1};flow.velocity={};flow.globalVelocity={};flow.movingBackgroundConfidence=0;
    for(int i=0;i<60;++i){double now=54+i*.016;input.timestamp=now;flow.timestamp=now-.03;flow.completedTimestamp=now-.01;driver.update(flow,input,now,.016,true,true);}
    require(std::hypot(driver.fused()[8].x,driver.fused()[8].y)<1,"holding a movement key cannot force motion against a reliably stationary global background");
    driver.reset();input.move={};input.mouseRate={100,0};flow.globalConfidence=0;flow.velocity.fill({90,0});
    for(int i=0;i<60;++i){double now=56+i*.016;input.timestamp=now;flow.timestamp=now-.03;flow.completedTimestamp=now-.01;driver.update(flow,input,now,.016,true,true);}
    require(driver.backgroundBlend()==0 && driver.observedFlow()[8].x==90,"missing background model preserves reliable local flow");
    flow.globalConfidence=1;flow.globalVelocity[0].x=std::numeric_limits<float>::quiet_NaN();
    input.timestamp=57;flow.timestamp=56.97;flow.completedTimestamp=56.99;driver.update(flow,input,57,.016,true,true);
    require(driver.observedFlow()[8].x==90 && driver.backgroundBlend()==0,"invalid whole-field model is rejected without corrupting valid local observations");
    std::cout<<"PASS: continuous input, keyboard field, controller, explicit static, focus, fusion, timestamp, continuation, finite values and deadzone\n";
}
