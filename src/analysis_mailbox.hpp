#pragma once
#include <condition_variable>
#include <mutex>
#include <optional>
#include <stop_token>
#include <vector>
#include <cstdint>
namespace mc {
struct GrayFrame {
    std::vector<float> pixels;
    double timestamp=0;
    int sourceWidth=0,sourceHeight=0;
    uint64_t generation=0;
};
// One replaceable slot: slow analysis cannot build a FIFO of old frames.
class AnalysisMailbox {
    mutable std::mutex mutex_;
    std::condition_variable_any ready_;
    std::optional<GrayFrame> latest_;
    uint64_t generation_=0,dropped_=0;
public:
    void submit(GrayFrame frame){
        {std::lock_guard lock(mutex_);frame.generation=generation_;if(latest_)++dropped_;latest_=std::move(frame);}
        ready_.notify_one();
    }
    bool take(std::stop_token stop,GrayFrame& frame){
        std::unique_lock lock(mutex_);
        if(!ready_.wait(lock,stop,[&]{return latest_.has_value();}))return false;
        if(stop.stop_requested())return false;
        frame=std::move(*latest_);latest_.reset();return true;
    }
    void reset(){std::lock_guard lock(mutex_);++generation_;latest_.reset();}
    bool current(uint64_t generation)const{std::lock_guard lock(mutex_);return generation_==generation;}
    uint64_t dropped()const{std::lock_guard lock(mutex_);return dropped_;}
};
}
