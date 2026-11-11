#pragma once
#include "analysis_mailbox.hpp"
#include <memory>
#include <chrono>
#include <algorithm>
namespace mc {
struct AnalysisPair {
    std::shared_ptr<const GrayFrame> previous,current;
};
// Two latest sampled images plus one replaceable pair. An in-flight worker
// keeps immutable references; producer updates never mutate its inputs.
class AnalysisPairMailbox {
    mutable std::mutex mutex_;
    std::condition_variable_any ready_;
    std::shared_ptr<const GrayFrame> latest_;
    std::optional<AnalysisPair> pending_;
    uint64_t generation_=0,dropped_=0;
    std::chrono::steady_clock::duration interval_{};
    std::chrono::steady_clock::time_point nextStart_{};
public:
    explicit AnalysisPairMailbox(double intervalSeconds=0):interval_(std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(std::max(0.,intervalSeconds)))){}
    void submit(GrayFrame frame){
        {std::lock_guard lock(mutex_);frame.generation=generation_;
            if(latest_&&frame.timestamp<=latest_->timestamp)return;
            auto current=std::make_shared<const GrayFrame>(std::move(frame));
            if(latest_&&current->timestamp>latest_->timestamp){
                if(pending_)++dropped_;
                pending_=AnalysisPair{latest_,current};
            }
            latest_=std::move(current);
        }
        ready_.notify_one();
    }
    bool take(std::stop_token stop,AnalysisPair& pair){
        std::unique_lock lock(mutex_);
        for(;;){
            if(!ready_.wait(lock,stop,[&]{return pending_.has_value();})||stop.stop_requested())return false;
            if(std::chrono::steady_clock::now()>=nextStart_)break;
            auto deadline=nextStart_;
            ready_.wait_until(lock,stop,deadline,[&]{return !pending_.has_value();});
            if(stop.stop_requested())return false;
        }
        nextStart_=std::chrono::steady_clock::now()+interval_;
        pair=std::move(*pending_);pending_.reset();return true;
    }
    void submitBatch(std::vector<GrayFrame> frames){
        {std::lock_guard lock(mutex_);for(auto& frame:frames){frame.generation=generation_;
            if(latest_&&frame.timestamp<=latest_->timestamp)continue;
            auto current=std::make_shared<const GrayFrame>(std::move(frame));
            if(latest_){if(pending_)++dropped_;pending_=AnalysisPair{latest_,current};}
            latest_=std::move(current);}}
        ready_.notify_one();
    }
    void reset(){{std::lock_guard lock(mutex_);++generation_;latest_.reset();pending_.reset();nextStart_={};}ready_.notify_all();}
    bool current(uint64_t generation)const{std::lock_guard lock(mutex_);return generation_==generation;}
    uint64_t dropped()const{std::lock_guard lock(mutex_);return dropped_;}
};
}
