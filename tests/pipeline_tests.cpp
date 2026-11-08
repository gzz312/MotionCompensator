#include "analysis_mailbox.hpp"
#include "dynamic.hpp"
#include "debug_visual.hpp"
#include <thread>
#include <latch>
#include <iostream>
#include <cstdlib>
void require(bool ok,const char* why){if(!ok){std::cerr<<"FAIL: "<<why<<'\n';std::exit(1);}}
int main(){
    mc::AnalysisMailbox box;std::latch first(1),release(1);mc::GrayFrame firstFrame,lastFrame;
    box.submit({{1},1,960,540,0});
    std::jthread worker([&](std::stop_token token){require(box.take(token,firstFrame),"first frame available");first.count_down();release.wait();require(box.take(token,lastFrame),"latest frame available");});
    first.wait();for(int i=2;i<=100;++i)box.submit({{float(i)},double(i),960,540,0});release.count_down();worker.join();
    require(firstFrame.timestamp==1 && lastFrame.timestamp==100 && box.dropped()==98,"slow consumer receives newest frame, never an accumulated FIFO");
    require(box.current(lastFrame.generation),"current generation may publish");box.reset();require(!box.current(lastFrame.generation),"focus/reset invalidates in-flight results");
    std::jthread waiting([&](std::stop_token token){mc::GrayFrame f;require(!box.take(token,f),"stop wakes an empty mailbox");});waiting.request_stop();waiting.join();
    mc::MotionSample sample;sample.timestamp=10;sample.completedTimestamp=10.09;sample.confidence=1;sample.velocity.fill({100,0});
    require(mc::motionFresh(sample,10.15),"freshly completed real work is not rejected solely for exceeding legacy 100ms source cutoff");
    require(!mc::motionFresh(sample,10.23),"stale completion or old source remains rejected");
    sample.completedTimestamp=10.21;require(!mc::motionFresh(sample,10.22),"new completion cannot rescue an over-age source");
    sample.completedTimestamp=10.09;mc::DynamicDriver driver;mc::InputState input;input.timestamp=10.15;driver.update(sample,input,10.15,.016,true,true);
    require(driver.source()==mc::DriveSource::Flow && driver.fused()[4].x==100,"actual driver consumes fresh asynchronous flow without keyboard or mouse");
    mc::DebugSnapshot d;d.now=10.15;d.flow=sample;mc::DebugHistory history;history.push(d);auto scene=mc::buildDebugScene(history,true,20);
    bool rawLabel=false,ready=false;for(auto t:scene.labels){rawLabel|=t.text.find(L"原始光流速度")!=std::wstring::npos;ready|=t.text.find(L"可用于补偿")!=std::wstring::npos;}
    require(rawLabel && ready,"panel exposes actual flow speed and the same readiness decision as driver");
    sample.confidence=0;d.flow=sample;d.now=10.16;history.push(d);scene=mc::buildDebugScene(history,true,20);bool low=false;for(auto t:scene.labels)low|=t.text.find(L"可信度/覆盖不足")!=std::wstring::npos;require(low,"low-confidence observations are distinguished from missing capture");
    d.now=11;history.push(d);scene=mc::buildDebugScene(history,true,20);bool expired=false;for(auto t:scene.labels)expired|=t.text==L"结果过期";require(expired,"expired data is labelled rather than silently disappearing");
    std::cout<<"PASS: latest-slot backpressure, reset generation, shutdown, asynchronous timestamp policy, flow-only drive and honest visual state\n";
}
