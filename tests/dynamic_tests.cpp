#include "dynamic.hpp"
#include "dots.hpp"
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
    require(std::hypot(offsets[0].x,offsets[0].y)<2 && driver.source()==mc::DriveSource::Ready,"held forward intent expires without visual evidence instead of inventing endless motion");
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
    require(driver.backgroundBlend()==0 && std::abs(driver.observedFlow()[8].x)<1,"held input does not replace a local zero with background model motion");
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
    // A road/sky frame may have only three reliable moving display regions.
    // Lack of whole-frame coverage must not discard those actual observations.
    driver.reset();flow={};input={};flow.analysisWidth=960;flow.analysisHeight=540;
    flow.features=324;flow.tracked=48;flow.observedCells=12;flow.issue=mc::MotionIssue::LowCoverage;
    flow.regionConfidence.fill(0);for(int r:{6,7,8}){flow.regionConfidence[r]=.95f;flow.velocity[r]={float(r-7)*180,180};}
    for(int i=0;i<40;++i){double now=60+i*.016;input.timestamp=now;flow.timestamp=now-.03;flow.completedTimestamp=now-.01;offsets=driver.update(flow,input,now,.016,true,true);}
    require(driver.source()==mc::DriveSource::Flow&&offsets[6].y<-30&&offsets[8].x<-30,"trustworthy road regions survive low whole-frame coverage without any input");
    require(driver.observedConfidence()[0]==0,"unobserved sky is not marked as measured flow");
    // A stationary sky/car fit must not flatten a broad measured road field.
    for(int r:{6,7,8})for(int row=r/3*3;row<r/3*3+3;++row)for(int col=r%3*3;col<r%3*3+3;++col){
        auto& c=flow.cells[row*9+col];c.confidence=.95f;c.velocity=c.smoothed=flow.velocity[r];}
    driver.reset();flow.globalConfidence=1;input.move={0,1};
    for(int i=0;i<60;++i){double now=62+i*.016;input.timestamp=now;flow.timestamp=now-.03;flow.completedTimestamp=now-.01;offsets=driver.update(flow,input,now,.016,true,true);}
    require(driver.backgroundBlend()<.01f&&driver.observedFlow()[6].y>170&&driver.observedFlow()[8].x>170,"stationary whole-frame model cannot overwrite distributed measured motion");
    require(std::hypot(offsets[3].x,offsets[3].y)<1&&std::hypot(offsets[5].x,offsets[5].y)<1&&driver.inferredRegions()==0,"unobserved side regions do not borrow road motion or retain indefinite keyboard intent");
    mc::RegionalDotField regional;mc::EdgeDotField edges;mc::VisualSettings display;display.edges=1;
    for(int i=0;i<40;++i){regional.update(offsets,1920,1080,122,.016,true,false,true,3,true);edges.update(offsets,1920,1080,display,.016);}
    require(regional.phaseMax()>5&&edges.phaseMax()>5,"both production dot fields continue rolling with the corrected road/input fusion");
    flow.timestamp=60;input.timestamp=64;driver.update(flow,input,64,.016,true,true);
    require(driver.source()==mc::DriveSource::Ready&&!driver.stationaryBackgroundRejected(),"old regional evidence and expired translation intent cannot manufacture fresh motion");
    // One isolated moving object cannot invalidate an otherwise stationary fit.
    driver.reset();flow.cells={};for(auto& c:flow.cells)c.confidence=.95f;
    flow.cells[80].velocity={180,180};flow.regionConfidence.fill(0);flow.regionConfidence[8]=.95f;flow.velocity[8]={180,180};
    for(int i=0;i<40;++i){double now=65+i*.016;input.timestamp=now;flow.timestamp=now-.03;flow.completedTimestamp=now-.01;driver.update(flow,input,now,.016,true,true);}
    require(!driver.stationaryBackgroundRejected()&&driver.backgroundBlend()==0&&driver.motionState()==mc::MotionState::Stopped,"isolated local animation does not override directly observed stillness");
    driver.reset();input={};flow.observedCells=0;flow.tracked=0;flow.globalConfidence=0;
    input.mouseRate={100,0};input.timestamp=64;flow.timestamp=63.97;flow.completedTimestamp=63.99;
    driver.update(flow,input,64,.016,true,true);
    require(driver.source()==mc::DriveSource::Input,"metadata without actual regional evidence cannot manufacture accepted flow");
    std::cout<<"PASS: continuous input, keyboard field, controller, explicit static, focus, fusion, timestamp, continuation, finite values and deadzone\n";
}
