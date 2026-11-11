#include "motion_continuity.hpp"

#include "dynamic.hpp"
#include <iostream>
#include <cstdlib>
#include <limits>
void require(bool ok,const char* why){if(!ok){std::cerr<<"FAIL: "<<why<<'\n';std::exit(1);}}
int main(){
    mc::MotionContinuity continuity;std::array<mc::Vec2,9> field{};std::array<float,9> q{};q.fill(1);field.fill({100,0});
    auto out=continuity.update(field,q,9.95,10,.04f);require(out.state==mc::MotionState::Moving,"measured field establishes movement");
    q.fill(0);out=continuity.update({},q,9.95,10.16,.04f);
    require(out.available[0]&&out.held[0]&&out.velocity[0].x>90,"short missing observation retains measured speed without immediate decay");
    out=continuity.update({},q,9.95,10.4,.04f);require(!out.available[0],"continuation expires using original observation age");
    continuity.reset();q.fill(1);continuity.update(field,q,19.95,20,.04f);field={};
    out=continuity.update(field,q,20.02,20.07,0);require(out.state==mc::MotionState::SuspectedStop&&out.held[0],"single quiet zero frame does not immediately erase motion");
    for(int i=0;i<5;++i)out=continuity.update(field,q,20.02,20.08+i*.01,0);
    require(out.state==mc::MotionState::SuspectedStop&&out.held[0],"repeated render ticks are not independent stop evidence");
    out=continuity.update(field,q,20.10,20.15,0);require(out.state==mc::MotionState::Stopped&&out.velocity[0].x==0&&!out.held[0],"independent quiet zero frames confirm a stop");
    continuity.reset();field.fill({100,0});continuity.update(field,q,29.95,30,.04f);
    out=continuity.update({},q,30.02,30.07,.08f);require(out.held[0]&&out.state==mc::MotionState::Unknown,"large pixel changes plus zero flow remain ambiguous");
    continuity.reset();for(int r=0;r<9;++r)field[r]={float(r%3-1)*100,float(r/3-1)*100};
    out=continuity.update(field,q,39.95,40,.04f);require(out.state==mc::MotionState::Moving,"opposing expansion vectors cannot cancel the movement state");
    continuity.reset();q.fill(0);out=continuity.update({},q,40,40.02,0);require(!out.available[0],"reset removes remembered motion");
    mc::DynamicDriver driver;mc::InputState input;mc::MotionSample flow;
    flow.confidence=1;flow.velocity.fill({100,0});flow.timestamp=49.95;flow.completedTimestamp=49.99;input.timestamp=50;
    driver.update(flow,input,50,.016,true,true);input.move={0,1};input.timestamp=50.16;
    auto speed=driver.update(flow,input,50.16,.016,true,true);
    require(driver.source()==mc::DriveSource::Coast&&driver.heldRegions()==9&&driver.fused()[0].x>90,"driver bridges missing flow before switching to unrelated input mapping");
    input.move={};input.mouseRate={100,0};input.timestamp=50.18;driver.update(flow,input,50.18,.016,true,true);
    require(driver.fused()[0].x<0&&driver.source()==mc::DriveSource::Input,"opposite look input immediately cancels obsolete continuation");
    driver.update(flow,input,50.18,.016,true,false);require(driver.heldRegions()==0,"focus loss clears the continuity state");
    driver.reset();input={};input.move={0,1};flow={};flow.confidence=1;flow.velocity.fill({200,0});flow.meanDelta=.04f;
    int continuationTicks=0;
    for(int i=0;i<200;++i){double now=60+i*.016;input.timestamp=now;
        if(i%5==0){flow.timestamp=now-.17;flow.completedTimestamp=now-.01;}
        driver.update(flow,input,now,.016,true,true);
        if(i>20){require(driver.fused()[0].x>190&&std::abs(driver.fused()[0].y)<1,"170ms-old periodic flow gaps do not alternate with the keyboard expansion field");continuationTicks+=driver.source()==mc::DriveSource::Coast;}
    }
    require(continuationTicks>50,"periodic trace actually exercises expiration and continuation");
    flow.velocity={};flow.meanDelta=0;
    for(int i=0;i<30;++i){double now=64+i*.016;input.timestamp=now;if(i%5==0){flow.timestamp=now-.08;flow.completedTimestamp=now-.01;}driver.update(flow,input,now,.016,true,true);}
    require(driver.motionState()==mc::MotionState::Stopped&&std::hypot(driver.fused()[0].x,driver.fused()[0].y)<1,"broad repeated static observations stop the field despite a held movement key");
    std::cout<<"PASS: bounded continuation, distinct stop observations, expansion energy, reversal, reset\n";
}
