#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace mc {
// Source-timestamp scheduling at a fixed 30Hz target. Missing frames are
// never repeated; delayed submissions skip deadlines instead of queuing work.
class CaptureCadence {
    double previous_=0,lastSubmitted_=0,period_=0,nextDue_=0;
    static constexpr double Step=1./30;
public:
    void reset(){previous_=lastSubmitted_=period_=nextDue_=0;}
    void observe(double timestamp,uint64_t received){
        if(!std::isfinite(timestamp)||timestamp<=0||received==0||timestamp<=previous_)return;
        if(previous_>0){double interval=timestamp-previous_;
            if(interval>.25){nextDue_=0;period_=0;}
            else {double sample=interval/double(received);period_=period_>0?std::lerp(period_,sample,.15):sample;}}
        previous_=timestamp;
    }
    bool due()const{
        double tolerance=std::min(.001,period_*.1);
        return previous_>0&&(lastSubmitted_==0||previous_-lastSubmitted_>=.008-1e-6)
            &&(nextDue_==0||previous_+tolerance>=nextDue_);
    }
    void submitted(){
        lastSubmitted_=previous_;
        if(nextDue_==0)nextDue_=previous_+Step;
        else {nextDue_+=Step;if(nextDue_<=previous_+1e-6)nextDue_+=Step*(1+std::floor(std::max(0.,previous_-nextDue_)/Step));}
    }
    double captureFps()const{return period_>0?1/period_:0;}
    double targetFps()const{return 30;}
};
}
