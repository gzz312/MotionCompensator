#pragma once
#include "capture.hpp"
#include <algorithm>

namespace mc {

class CaptureRecovery {
    double started_{};
    double nextRetry_{};
    double healthySince_{};
    unsigned attempt_{};
public:
    void begin(double now) {
        started_=now;
        nextRetry_=now+1;
        healthySince_=0;
        attempt_=0;
    }

    bool retrying() const { return attempt_!=0; }

    bool failed(const CaptureStats& stats,double now) const {
        if(stats.status=="Starting WGC"||stats.status=="Not started")return false;
        return !stats.running || (!stats.frames && now-started_>=8) ||
               (stats.readbackErrorSince>0 && now-stats.readbackErrorSince>=2);
    }

    bool ready(const CaptureStats& stats,double now) {
        if(stats.running&&stats.frames&&stats.readbackErrorSince==0){
            if(!healthySince_)healthySince_=now;
            if(now-healthySince_>=2)attempt_=0;
        } else {
            healthySince_=0;
        }
        return now>=nextRetry_&&failed(stats,now);
    }

    void restarting(double now) {
        nextRetry_=now+std::min(30.,double(1u<<std::min(attempt_,5u)));
        attempt_=std::min(attempt_+1,6u);
        started_=now;
        healthySince_=0;
    }
};

}
