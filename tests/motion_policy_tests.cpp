#include "dynamic.hpp"
#include <iostream>
#include <cstdlib>
void require(bool ok,const char* why){if(!ok){std::cerr<<"FAIL: "<<why<<'\n';std::exit(1);}}
mc::MotionSample base(){
    mc::MotionSample f;f.analysisWidth=960;f.analysisHeight=540;f.features=1296;f.tracked=1000;f.observedCells=81;
    f.confidence=1;f.regionConfidence.fill(.95f);f.issue=mc::MotionIssue::Valid;f.meanDelta=.04f;
    for(auto& c:f.cells){c.confidence=.95f;c.samples=16;c.tracked=c.inliers=12;}
    return f;
}
void road(mc::MotionSample& f){
    for(int i=0;i<81;++i){int row=i/9,col=i%9;
        if(row>=3&&row<=5){auto v=mc::Vec2{float(col-4)*270,120};f.cells[i].velocity=f.cells[i].smoothed=v;}}
    for(int r=0;r<9;++r)if(r>=3&&r<=5)f.velocity[r]={float(r%3-1)*1060,116};
}
void run(mc::DynamicDriver& d,mc::MotionSample& f,mc::InputState& input,double begin,int frames=60){
    for(int i=0;i<frames;++i){double now=begin+i*.016;f.timestamp=now-.06;f.completedTimestamp=now-.015;input.timestamp=now;d.update(f,input,now,.016,true,true);}
}
int main(){
    mc::DynamicDriver driver;auto f=base();road(f);mc::InputState input;input.move={0,1};
    // Regression for the observed log failure: q~.83 background speed~11
    // replaced local right-region speed~1066 with q~.83, age~70 ms.
    f.globalConfidence=.94f;f.movingBackgroundConfidence=.84f;
    f.globalVelocity.fill({15,7});f.movingBackgroundVelocity.fill({15,7});f.globalRegionSupport.fill(4);f.movingRegionSupport.fill(4);
    run(driver,f,input,10);
    require(driver.evidence().broadMotion&&!driver.evidence().stationary,"distributed road motion vetoes the static foreground majority");
    require(driver.observedFlow()[5].x>1000&&driver.observedFlow()[3].x<-1000,"near-zero model cannot erase reliable opposing road motion");
    // Even a moderately moving wrong model must not collapse large motion.
    f.movingBackgroundVelocity.fill({90,25});run(driver,f,input,11);
    require(driver.observedFlow()[5].x>1000&&driver.observedFlow()[3].x<-1000,"nonzero conflicting background models are also region-gated");
    input.move={};run(driver,f,input,12);
    require(driver.source()==mc::DriveSource::Flow&&std::hypot(driver.fused()[5].x,driver.fused()[5].y)>1000,"coasting motion does not require a held accelerator");
    // Entire textured wall; key remains held. Animation changes pixels but
    // not a distributed background. It must not prevent a measured stop.
    f=base();f.globalConfidence=1;f.globalRegionSupport.fill(6);input.move={0,1};
    for(int i:{60,61,69})f.cells[i].velocity={500,100};f.velocity[6]={500,100};
    run(driver,f,input,13);
    require(driver.motionState()==mc::MotionState::Stopped,"localized animation cannot block a spatially confirmed wall stop");
    for(auto v:driver.fused())require(std::hypot(v.x,v.y)<1,"forward intent cannot override observed stationary wall");
    input.move={1,0};input.timestamp=14;
    driver.update(f,input,14,.016,true,true);
    for(auto v:driver.fused())require(std::hypot(v.x,v.y)<1,"new movement intent cannot reanimate a confirmed stationary wall during source alignment");
    // Unknown texture is not a confirmed stop and not an endless W drive.
    driver.reset();f={};f.regionConfidence.fill(0);
    run(driver,f,input,15);
    require(driver.motionState()==mc::MotionState::Unknown&&driver.source()==mc::DriveSource::Ready,"unknown stays distinct from stopped after initial intent expires");
    for(auto v:driver.fused())require(std::hypot(v.x,v.y)<1,"unknown wall does not retain indefinite translation");
    // Short gaps retain measured road motion, then expire even with W held.
    driver.reset();f=base();road(f);run(driver,f,input,17);
    auto old=f;input.timestamp=18.12;driver.update(old,input,18.12,.016,true,true);
    require(driver.source()==mc::DriveSource::Coast,"short source-age gap preserves the measured field");
    input.timestamp=18.5;driver.update(old,input,18.5,.016,true,true);
    require(driver.source()==mc::DriveSource::Ready,"expired observation is not extended by held W");
    // Model fit must have nearby actual support before filling missing data.
    driver.reset();f=base();road(f);f.velocity[0]={};f.regionConfidence[0]=0;
    for(int i=27;i<36;++i)f.cells[i].velocity=f.cells[i].smoothed={};
    f.movingBackgroundConfidence=1;f.movingBackgroundVelocity.fill({600,100});f.movingRegionSupport.fill(0);
    run(driver,f,input,20);require(driver.observedConfidence()[0]==0,"whole-image confidence alone cannot manufacture local model support");
    f.movingRegionSupport[1]=6;run(driver,f,input,21);
    require(driver.observedConfidence()[0]==0&&driver.inferredRegions()==0,"missing regions remain unmeasured even with nearby model support");
    std::cout<<"PASS: logged suppression regression, wall stop, animation, coasting, unknown, bounded input and supported inference\n";
}
