#pragma once
#include "core.hpp"
#include <deque>
namespace mc {
struct InputState {
    double timestamp{};
    Vec2 mouseRate{}; // Relative counts/second; never screen cursor coordinates.
    Vec2 look{},move{}; // Normalized pad look (down-positive Y), strafe/forward.
    bool mouse=false,keyboard=false,pad=false;
    uint16_t mouseButtons=0,padButtons=0;
    Vec2 triggers{};
    float wheel=0;
    bool sprint=false,jump=false,crouch=false;
};
inline float stickAxis(int value,int deadzone) {
    float magnitude=std::abs(float(value));
    return magnitude<=deadzone?0.f:std::copysign(std::min(1.f,(magnitude-deadzone)/(32767.f-deadzone)),float(value));
}
enum class DriveSource { Suspended, Static, Flow, Fused, Input, Coast, Ready };
class DynamicDriver {
    struct TimedField {double timestamp;std::array<Vec2,9> velocity;};
    std::deque<TimedField> history_;
    std::array<Vec2,9> velocity_{};
    MotionSample lastFlow_{};
    std::array<Vec2,9> prediction_{},fused_{};
    DriveSource source_=DriveSource::Ready;
    double lastMotionInput_=-1;
    float backgroundBlend_=0;
    std::array<Vec2,9> observedFlow_{};
    std::array<float,9> observedConfidence_{};
public:
    const auto& prediction() const {return prediction_;}
    const auto& fused() const {return fused_;}
    float backgroundBlend()const{return backgroundBlend_;}
    const auto& observedFlow()const{return observedFlow_;}
    const auto& observedConfidence()const{return observedConfidence_;}
    DriveSource source() const {return source_;}
    void reset() {history_.clear();velocity_={};lastFlow_={};prediction_={};fused_={};source_=DriveSource::Ready;lastMotionInput_=-1;backgroundBlend_=0;observedFlow_={};observedConfidence_={};}
    std::array<Vec2,9> update(const MotionSample& flow,const InputState& input,double now,float dt,bool dynamic,bool foreground) {
        if(!foreground || !dynamic || !std::isfinite(now)) {
            reset();source_=foreground?DriveSource::Static:DriveSource::Suspended;return {};
        }
        auto finite=[](float v){return std::isfinite(v)?v:0.f;};
        bool freshInput=std::isfinite(input.timestamp) && now>=input.timestamp && now-input.timestamp<=.1;
        std::array<Vec2,9> prediction{};
        for(int r=0;r<9;++r) if(freshInput) {
            float x=float(r%3-1),y=float(r/3-1);
            // Provisional FPS mapping, not a calibrated camera angle. Forward
            // motion expands the peripheral field; no artificial idle animation.
            prediction[r]={std::clamp(-finite(input.mouseRate.x)*1.4f-finite(input.look.x)*1100.f-finite(input.move.x)*320.f+x*finite(input.move.y)*260.f,-2400.f,2400.f),
                           std::clamp(-finite(input.mouseRate.y)*1.4f-finite(input.look.y)*1100.f+y*finite(input.move.y)*260.f,-2400.f,2400.f)};
        }
        prediction_=prediction;
        bool inputMoving=false;for(auto v:prediction) inputMoving|=std::hypot(v.x,v.y)>2;
        history_.push_back({now,prediction});while(!history_.empty() && now-history_.front().timestamp>.4) history_.pop_front();
        bool valid=std::isfinite(flow.confidence) && flow.confidence>=.65f && motionFresh(flow,now);
        bool motionIntent=inputMoving || (freshInput && (input.jump || input.crouch));
        if(motionIntent)lastMotionInput_=now;
        bool holdIntent=lastMotionInput_>=0 && now-lastMotionInput_<.18;
        bool fresh=motionFresh(flow,now);
        auto trustedModel=[](float confidence,const auto& field){return std::isfinite(confidence) && confidence>=.65f && std::all_of(field.begin(),field.end(),[](Vec2 v){return std::isfinite(v.x)&&std::isfinite(v.y);});};
        bool useMoving=(motionIntent||holdIntent||backgroundBlend_>.001f) && fresh && trustedModel(flow.movingBackgroundConfidence,flow.movingBackgroundVelocity);
        bool global=fresh && (useMoving || trustedModel(flow.globalConfidence,flow.globalVelocity));
        const auto& globalField=useMoving?flow.movingBackgroundVelocity:flow.globalVelocity;
        float globalQuality=useMoving?flow.movingBackgroundConfidence:flow.globalConfidence;
        float blendAlpha=1-std::exp(-std::clamp(std::isfinite(dt)?dt:0.f,0.f,.05f)/.08f);
        backgroundBlend_+=blendAlpha*((global && (motionIntent||holdIntent)?1.f:0.f)-backgroundBlend_);
        observedFlow_={};observedConfidence_={};
        std::array<Vec2,9> capturedInput{};bool aligned=false;
        if(valid || global) {
            // Use the nearest input sample to the capture timestamp. Only add
            // the change since capture, avoiding flow + full-input double count.
            double nearest=.026;
            for(const auto& entry:history_) if(std::abs(entry.timestamp-flow.timestamp)<nearest) {
                nearest=std::abs(entry.timestamp-flow.timestamp);capturedInput=entry.velocity;aligned=true;
            }
        }
        MotionSample drive;drive.timestamp=now;drive.confidence=1;
        bool usedFlow=false;
        for(int r=0;r<9;++r) {
            auto v=flow.velocity[r];
            bool local=valid && std::isfinite(flow.regionConfidence[r]) && flow.regionConfidence[r]>=.65f && std::isfinite(v.x) && std::isfinite(v.y);
            float weight=global?backgroundBlend_:0;
            bool region=local || (global && weight>.01f);
            if(region) {
                if(global && weight>.01f){
                    auto g=globalField[r];
                    if(std::isfinite(g.x)&&std::isfinite(g.y)){
                        if(local){v.x=std::lerp(v.x,g.x,weight);v.y=std::lerp(v.y,g.y,weight);}
                        else v=g;
                    }
                }
                usedFlow=true;drive.velocity[r]=v;
                observedFlow_[r]=v;observedConfidence_[r]=global && weight>.01f?globalQuality:flow.regionConfidence[r];
                if(aligned && inputMoving) {
                    drive.velocity[r].x+=std::clamp(prediction[r].x-capturedInput[r].x,-600.f,600.f);
                    drive.velocity[r].y+=std::clamp(prediction[r].y-capturedInput[r].y,-600.f,600.f);
                }
                lastFlow_.velocity[r]=v;lastFlow_.regionConfidence[r]=1;
            } else if(inputMoving) drive.velocity[r]=prediction[r];
            else if(lastFlow_.timestamp>0 && now>=lastFlow_.timestamp && now-lastFlow_.timestamp<.25) {
                float decay=float(std::exp(-(now-lastFlow_.timestamp)/.09));
                drive.velocity[r]={lastFlow_.velocity[r].x*decay,lastFlow_.velocity[r].y*decay};
            }
        }
        if(usedFlow) {
            lastFlow_.timestamp=flow.timestamp;
            for(int r=0;r<9;++r) if(observedConfidence_[r]<.65f) lastFlow_.velocity[r]={};
        }
        source_=usedFlow?(inputMoving?DriveSource::Fused:DriveSource::Flow):inputMoving?DriveSource::Input:
            (lastFlow_.timestamp>0 && now-lastFlow_.timestamp<.25?DriveSource::Coast:DriveSource::Ready);
        fused_=drive.velocity;
        float seconds=std::clamp(std::isfinite(dt)?dt:0.f,0.f,.05f);
        float alpha=1-std::exp(-seconds/.08f);
        for(int r=0;r<9;++r) {
            // Publish the original smoothed field for the rolling lattice.
            velocity_[r].x+=alpha*(-std::clamp(drive.velocity[r].x,-2400.f,2400.f)*.35f-velocity_[r].x);
            velocity_[r].y+=alpha*(-std::clamp(drive.velocity[r].y,-2400.f,2400.f)*.35f-velocity_[r].y);
        }
        return velocity_;
    }
};
}
