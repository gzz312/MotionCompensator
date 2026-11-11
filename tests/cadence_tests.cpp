#include "capture_cadence.hpp"
#include "analysis_pair_mailbox.hpp"
#include <thread>
#include <iostream>
#include <cstdlib>
#include <limits>
void require(bool ok,const char* why){if(!ok){std::cerr<<"FAIL: "<<why<<'\n';std::exit(1);}}
int main(){
    for(double fps:{24.,30.,60.,90.,120.,144.,240.}){
        mc::CaptureCadence cadence;int samples=0;double previous=0;
        int total=int(fps*4);
        for(int i=0;i<total;++i){double stamp=10+(i+.01*std::sin(i*.7))/fps;cadence.observe(stamp,1);
            if(cadence.due()){if(previous>0)require(stamp-previous>=.008-1e-6,"source pairs respect the estimator minimum without duplicate frames");previous=stamp;++samples;cadence.submitted();}}
        int expected=int(std::min(fps,30.)*4);
        require(std::abs(samples-expected)<=1&&cadence.targetFps()==30,"fixed 30Hz target does not scale with source FPS");
    }
    mc::CaptureCadence cadence;
    for(int i=0;i<60;++i){cadence.observe(20+i/60.,1);if(cadence.due())cadence.submitted();}
    int samples=0;for(int i=0;i<120;++i){cadence.observe(21+i/120.,1);if(cadence.due()){++samples;cadence.submitted();}}
    require(std::abs(samples-30)<=1&&cadence.targetFps()==30,"source rate changes do not change the requested 30Hz analysis rate");
    cadence.reset();cadence.observe(30,1);require(cadence.due(),"first source frame starts the cadence");cadence.submitted();
    cadence.observe(30,1);cadence.observe(std::numeric_limits<double>::quiet_NaN(),1);
    require(!cadence.due(),"duplicate or invalid timestamps do not create work");
    cadence.observe(30.016,1);require(!cadence.due(),"a 60Hz source frame before the next deadline is skipped");
    cadence.observe(30.033334,1);require(cadence.due(),"the next 30Hz deadline accepts a real frame");cadence.submitted();
    cadence.observe(31,1);require(cadence.due(),"a capture gap restarts scheduling without catch-up work");cadence.submitted();
    cadence.observe(31.001,2);require(!cadence.due(),"drained frames cannot accelerate fixed-rate analysis");
    cadence.reset();cadence.observe(40,1);cadence.submitted();cadence.observe(40.0666666,1);require(cadence.due(),"a delayed source may replace a missed deadline");cadence.submitted();
    cadence.observe(40.0833333,1);require(!cadence.due(),"late submissions do not produce a catch-up burst near floating-point deadline boundaries");
    mc::AnalysisPairMailbox mailbox;
    mailbox.submit({{1},50,960,540,0});mailbox.submit({{2},50.033,960,540,0});
    mc::AnalysisPair first;std::stop_source stop;require(mailbox.take(stop.get_token(),first),"first distinct sampled pair is available");
    for(int i=3;i<=100;++i)mailbox.submit({{float(i)},50+(i-1)*.033,960,540,0});
    mc::AnalysisPair latest;require(mailbox.take(stop.get_token(),latest),"latest pair replaces obsolete pairs");
    require(latest.previous->pixels[0]==99&&latest.current->pixels[0]==100,"slow analysis receives adjacent latest sampled frames rather than its old previous frame");
    require(std::abs(latest.current->timestamp-latest.previous->timestamp-.033)<1e-8&&mailbox.dropped()==97,"backpressure preserves short source-pair spacing and records replacements");
    require(first.previous->pixels[0]==1&&first.current->pixels[0]==2,"producer replacement cannot mutate in-flight input images");
    mailbox.reset();require(!mailbox.current(latest.current->generation),"reset invalidates in-flight publication");
    std::jthread waiting([&](std::stop_token token){mc::AnalysisPair pair;require(!mailbox.take(token,pair),"shutdown wakes a worker waiting for a pair");});waiting.request_stop();waiting.join();
    mc::AnalysisPairMailbox paced(1./30);paced.submit({{1},70,960,540,0});paced.submit({{2},70+1./60,960,540,0});
    mc::AnalysisPair before,after;require(paced.take(stop.get_token(),before),"paced worker receives its initial source pair");
    auto begin=std::chrono::steady_clock::now();
    for(int i=3;i<=6;++i)paced.submit({{float(i)},70+(i-1)/60.,960,540,0});
    require(paced.take(stop.get_token(),after),"paced worker receives replacement after its dispatch deadline");
    require(std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count()>=.025,"analysis dispatch is paced separately from 60Hz source input");
    require(after.previous->pixels[0]==5&&after.current->pixels[0]==6&&std::abs(after.current->timestamp-after.previous->timestamp-1./60)<1e-8,"30Hz analysis uses adjacent 60Hz source frames instead of 30Hz-spaced frames");
    paced.submit({{99},70+5./60,960,540,0});paced.submit({{7},70+6./60,960,540,0});
    mc::AnalysisPair distinct;require(paced.take(stop.get_token(),distinct)&&distinct.previous->pixels[0]==6,"duplicate timestamp cannot poison source pairing");
    paced.reset();paced.submit({{8},80,960,540,0});paced.submit({{9},80+1./60,960,540,0});
    require(paced.take(stop.get_token(),distinct)&&distinct.previous->pixels[0]==8,"reset clears dispatch history and old frame references");
    mailbox.reset();mailbox.submitBatch({{{10},90,960,540,0},{{11},90+1./60,960,540,0},{{12},90+2./60,960,540,0}});
    require(mailbox.take(stop.get_token(),distinct)&&distinct.previous->pixels[0]==11&&distinct.current->pixels[0]==12,"multiple ready GPU buffers publish the newest adjacent pair atomically");
    std::cout<<"PASS: fixed dispatch cadence, adjacent source pairs, duplicate protection, newest immutable inputs, reset and bounded backpressure\n";
}
