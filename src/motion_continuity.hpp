#pragma once
#include "core.hpp"
#include "motion_evidence.hpp"
namespace mc {
enum class MotionState {Unknown,Moving,SuspectedStop,Stopped};
inline const wchar_t* motionStateName(MotionState state,bool chinese){
    switch(state){
    case MotionState::Moving:return chinese?L"运动中":L"Moving";
    case MotionState::SuspectedStop:return chinese?L"疑似停止":L"Checking stop";
    case MotionState::Stopped:return chinese?L"已停止":L"Stopped";
    default:return chinese?L"观测不足":L"Observation missing";
    }
}
struct ContinuedMotion {
    std::array<Vec2,9> velocity{};
    std::array<bool,9> available{},held{};
    MotionState state=MotionState::Unknown;
};
// Measured motion is kept distinct from intent. Repeated display ticks are
// not additional observations and cannot confirm stopping or extend a hold.
class MotionContinuity {
    struct Region {Vec2 velocity{};double stamp=-1,accepted=-1,zeroSince=-1;int zeros=0;};
    std::array<Region,9> regions_{};
    double lastFrame_=-1,stopSince_=-1;int stopFrames_=0;
public:
    void reset(){regions_={};lastFrame_=stopSince_=-1;stopFrames_=0;}
    void invalidateRegion(int region){if(region>=0&&region<9)regions_[region]={};}
    ContinuedMotion update(const std::array<Vec2,9>& field,const std::array<float,9>& confidence,
        double stamp,double now,float meanDelta,const SpatialEvidence* evidence=nullptr){
        ContinuedMotion out;if(!std::isfinite(now))return out;
        bool newFrame=std::isfinite(stamp)&&stamp>lastFrame_+1e-6&&stamp>0&&now>=stamp;
        std::array<bool,9> trusted{};int count=0,moving=0;double energy=0;
        for(int r=0;r<9;++r){auto v=field[r];trusted[r]=stamp>0&&now>=stamp&&now-stamp<=.2&&std::isfinite(confidence[r])&&confidence[r]>=.65f&&std::isfinite(v.x)&&std::isfinite(v.y);
            if(trusted[r]){++count;float speed=std::hypot(v.x,v.y);energy+=speed*speed;if(speed>12)++moving;}}
        // RMS preserves opposing expansion directions instead of cancelling
        // them. Quiet pixels are required before treating broad zero flow as a stop.
        bool quiet=std::isfinite(meanDelta)&&meanDelta>=0&&meanDelta<.012f;
        bool stopEvidence=evidence?evidence->stationary:count>=5&&moving==0&&energy/count<64&&quiet;
        if(newFrame){lastFrame_=stamp;if(stopEvidence){if(stopFrames_==0)stopSince_=stamp;++stopFrames_;}else {stopFrames_=0;stopSince_=-1;}}
        bool stopped=stopEvidence&&stopFrames_>=2&&stamp-stopSince_>=.07;
        out.state=stopped?MotionState::Stopped:stopEvidence?MotionState::SuspectedStop:moving>0?MotionState::Moving:MotionState::Unknown;
        for(int r=0;r<9;++r){auto& memory=regions_[r];bool use=false;
            if(trusted[r]){
                auto v=field[r];bool small=std::hypot(v.x,v.y)<8;
                if(newFrame){
                    if(small){if(memory.zeros==0)memory.zeroSince=stamp;++memory.zeros;}
                    else {memory.zeros=0;memory.zeroSince=-1;}
                }
                bool confirmedZero=stopped||((quiet||moving>=2)&&memory.zeros>=2&&stamp-memory.zeroSince>=.07)
                    ||(memory.zeros>=3&&stamp-memory.zeroSince>=.18);
                bool rememberedMotion=memory.stamp>0&&std::hypot(memory.velocity.x,memory.velocity.y)>=8;
                use=!small||!rememberedMotion||confirmedZero;
                if(use){out.velocity[r]=v;out.available[r]=true;
                    if(newFrame){memory.velocity=v;memory.stamp=stamp;memory.accepted=now;}}
            }
            if(!use&&memory.stamp>0&&now>=memory.stamp&&now>=memory.accepted){
                double sourceAge=now-memory.stamp,gap=now-memory.accepted;
                if(sourceAge<=.35&&gap<=.22){
                    float fade=1.f-float(std::clamp((sourceAge-.27)/.08,0.,1.));
                    out.velocity[r]={memory.velocity.x*fade,memory.velocity.y*fade};
                    out.available[r]=true;out.held[r]=true;
                }
            }
            if(stopped){out.velocity[r]={};out.available[r]=true;out.held[r]=false;memory.velocity={};}
        }
        return out;
    }
};
}
