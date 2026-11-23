#pragma once
#include "core.hpp"
#include "motion_continuity.hpp"
#include "camera_support.hpp"
#include "screen_fixed_motion.hpp"
#include <deque>
namespace mc {
struct InputState {
    double timestamp{};
    Vec2 mouseRate{}; // Relative counts/second; never screen cursor coordinates.
    Vec2 look{},move{}; // Normalized pad look (down-positive Y), strafe/forward.
    bool mouse=false,keyboard=false,pad=false;
    uint16_t mouseButtons=0,padButtons=0;
    Vec2 triggers{};
    bool sprint=false,jump=false,crouch=false,prone=false;
};
inline float stickAxis(int value,int deadzone) {
    float magnitude=std::abs(float(value));
    return magnitude<=deadzone?0.f:std::copysign(std::min(1.f,(magnitude-deadzone)/(32767.f-deadzone)),float(value));
}
struct FieldIntent {
    Vec2 look{},move{},capturedLook{},capturedMove{};
    double timestamp{};
    bool aligned{},startup{};
    static Vec2 project(Vec2 look,Vec2 move,float u,float v) {
        return {look.x-move.x*320.f+(u-.5f)*780.f*move.y,look.y+(v-.5f)*780.f*move.y};
    }
    Vec2 at(float u,float v) const {return project(look,move,u,v);}
    Vec2 atCapture(float u,float v) const {return project(capturedLook,capturedMove,u,v);}
};
enum class DriveSource { Suspended, Static, Flow, Fused, Input, Coast, Ready };
class DynamicDriver {
    struct TimedField {double timestamp;std::array<Vec2,9> velocity;Vec2 look,move;};
    std::deque<TimedField> history_;
    std::array<Vec2,9> velocity_{};
    MotionContinuity continuity_;
    MotionState motionState_=MotionState::Unknown;
    int heldRegions_=0;
    std::array<Vec2,9> prediction_{},fused_{};
    DriveSource source_=DriveSource::Ready;
    float backgroundBlend_=0;
    std::array<Vec2,9> observedFlow_{};
    std::array<float,9> observedConfidence_{};
    bool stationaryBackgroundRejected_=false;
    double intentBegan_=-1;
    bool intentHeld_=false;
    int protectedRegions_=0,inferredRegions_=0;
    SpatialEvidence evidence_{};
    FieldIntent intent_{};
    ScreenFixedMotion screenFixed_;
    int correctedRegions_{};
public:
    const ScreenFixedMotion& screenFixed() const {return screenFixed_;}
    int correctedRegions() const {return correctedRegions_;}
    const FieldIntent& fieldIntent() const {return intent_;}
    const auto& prediction() const {return prediction_;}
    const auto& fused() const {return fused_;}
    float backgroundBlend()const{return backgroundBlend_;}
    const auto& observedFlow()const{return observedFlow_;}
    const auto& observedConfidence()const{return observedConfidence_;}
    bool stationaryBackgroundRejected()const{return stationaryBackgroundRejected_;}
    MotionState motionState()const{return motionState_;}
    int heldRegions()const{return heldRegions_;}
    DriveSource source() const {return source_;}
    int protectedRegions()const{return protectedRegions_;}
    int inferredRegions()const{return inferredRegions_;}
    const auto& evidence()const{return evidence_;}
    void reset() {intent_={};history_.clear();velocity_={};continuity_.reset();motionState_=MotionState::Unknown;heldRegions_=0;prediction_={};fused_={};source_=DriveSource::Ready;backgroundBlend_=0;observedFlow_={};observedConfidence_={};stationaryBackgroundRejected_=false;intentBegan_=-1;intentHeld_=false;protectedRegions_=inferredRegions_=correctedRegions_=0;evidence_={};screenFixed_.reset();}
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
        bool translationIntent=freshInput&&(std::hypot(input.move.x,input.move.y)>.01f||input.jump||input.crouch||input.prone);
        if(translationIntent&&!intentHeld_)intentBegan_=now;
        intentHeld_=translationIntent;
        bool shortTranslation=translationIntent&&intentBegan_>=0&&now-intentBegan_<=.18;
        const Vec2 lookPrediction{freshInput?-finite(input.mouseRate.x)*1.4f-finite(input.look.x)*1100.f:0,
                                  freshInput?-finite(input.mouseRate.y)*1.4f-finite(input.look.y)*1100.f:0};
        const Vec2 move=freshInput?Vec2{finite(input.move.x),finite(input.move.y)}:Vec2{};
        intent_={lookPrediction,move,{},{},freshInput?input.timestamp:0,false,shortTranslation};
        prediction_=prediction;
        bool inputMoving=false;for(auto v:prediction) inputMoving|=std::hypot(v.x,v.y)>2;
        history_.push_back({now,prediction,lookPrediction,move});while(!history_.empty() && now-history_.front().timestamp>.4) history_.pop_front();
        bool valid=anyRegionMotionTrusted(flow,now);
        bool fresh=motionFresh(flow,now);
        evidence_=fresh?spatialEvidence(flow):SpatialEvidence{};
        screenFixed_.update(flow,evidence_,fresh);
        correctedRegions_=0;
        // Reliable regional observations and temporal continuation take priority.
        backgroundBlend_=0;stationaryBackgroundRejected_=false;
        protectedRegions_=inferredRegions_=0;
        observedFlow_={};observedConfidence_={};
        std::array<Vec2,9> capturedInput{};Vec2 capturedLook{};bool aligned=false;
        if(fresh&&(valid||flow.observedCells>0)) {
            // Use the nearest input sample to the capture timestamp. Only add
            // the change since capture, avoiding flow + full-input double count.
            double nearest=.026;
            for(const auto& entry:history_) if(std::abs(entry.timestamp-flow.timestamp)<nearest) {
                nearest=std::abs(entry.timestamp-flow.timestamp);capturedInput=entry.velocity;capturedLook=entry.look;aligned=true;
                intent_.capturedLook=entry.look;intent_.capturedMove=entry.move;intent_.aligned=true;
            }
        }
        MotionSample drive;drive.timestamp=now;drive.confidence=1;
        std::array<Vec2,9> selected{};std::array<float,9> quality{};
        for(int r=0;r<9;++r)if(regionMotionTrusted(flow,r,now)){
            selected[r]=flow.velocity[r];quality[r]=flow.regionConfidence[r];
        }
        for(int r=0;r<9;++r){
            const auto& correction=screenFixed_.regions()[r];
            if(correction.confidence<=0)continue;
            const auto old=selected[r],background=correction.velocity;
            const float oldSpeed=std::hypot(old.x,old.y),backgroundSpeed=std::hypot(background.x,background.y);
            const bool weakAligned=oldSpeed<std::min(52.f,.43f*backgroundSpeed)&&old.x*background.x+old.y*background.y>0;
            if(quality[r]>0&&oldSpeed>=22&&!weakAligned)continue;
            selected[r]=background;quality[r]=correction.confidence;++correctedRegions_;
        }
        // Stop evidence comes from raw spatial observations, never from a
        // model-smoothed near-zero field. Isolated animation cannot veto it.
        if(evidence_.stationary)selected={};
        auto continued=continuity_.update(selected,quality,flow.timestamp,now,flow.meanDelta,&evidence_);
        motionState_=continued.state;heldRegions_=0;
        bool usedFlow=false,usedCoast=false,usedInput=false;

        float lookSpeed=std::hypot(lookPrediction.x,lookPrediction.y);
        for(int r=0;r<9;++r){
            if(continued.available[r]){
                auto v=continued.velocity[r];float speed=std::hypot(v.x,v.y);
                bool reversed=continued.held[r]&&lookSpeed>40&&(speed<8||v.x*lookPrediction.x+v.y*lookPrediction.y<-.2f*speed*lookSpeed);
                if(reversed){continuity_.invalidateRegion(r);drive.velocity[r]=shortTranslation&&!evidence_.stationary?prediction[r]:lookPrediction;usedInput|=inputMoving;continue;}
                drive.velocity[r]=v;
                if(continued.held[r]){++heldRegions_;usedCoast=true;}
                else {
                    usedFlow=true;observedFlow_[r]=selected[r];observedConfidence_[r]=quality[r];
                    if(aligned&&inputMoving){
                        auto delta=evidence_.stationary?Vec2{lookPrediction.x-capturedLook.x,lookPrediction.y-capturedLook.y}
                            :Vec2{prediction[r].x-capturedInput[r].x,prediction[r].y-capturedInput[r].y};
                        drive.velocity[r].x+=std::clamp(delta.x,-600.f,600.f);
                        drive.velocity[r].y+=std::clamp(delta.y,-600.f,600.f);
                    }
                }
            }else {
                // Only a missing region can borrow the camera model. Sample
                // its missing cells independently to avoid painting over HUD
                // or neighboring regions with different measured motion.
                if(fresh&&!evidence_.stationary&&flow.cameraModel.confidence>=.76f){
                    Vec2 sum{};int ready=0;
                    for(int dy=0;dy<3;++dy)for(int dx=0;dx<3;++dx){
                        const int i=(r/3*3+dy)*9+r%3*3+dx;
                        Vec2 value{};
                        if(cameraFill(flow,i,value)){sum.x+=value.x;sum.y+=value.y;++ready;}
                    }
                    if(ready>=3){
                        drive.velocity[r]={sum.x/ready,sum.y/ready};
                        ++inferredRegions_;usedFlow=true;
                        continue;
                    }
                }
                if(!inputMoving)continue;
                // Camera look may predict the next image. Translation is only
                // an initial burst, not a permanent claim that W means motion.
                drive.velocity[r]=shortTranslation&&!evidence_.stationary?prediction[r]:lookPrediction;
                usedInput|=std::hypot(drive.velocity[r].x,drive.velocity[r].y)>2;
            }
        }
        source_=usedFlow?(inputMoving?DriveSource::Fused:DriveSource::Flow):usedCoast?DriveSource::Coast:usedInput?DriveSource::Input:DriveSource::Ready;
        fused_=drive.velocity;
        float seconds=std::clamp(std::isfinite(dt)?dt:0.f,0.f,.05f);
        for(int r=0;r<9;++r) {
            bool reversing=velocity_[r].x*(-drive.velocity[r].x)+velocity_[r].y*(-drive.velocity[r].y)<-1;
            float alpha=1-std::exp(-seconds/((reversing||motionState_==MotionState::Stopped)?.035f:.08f));
            // Publish the original smoothed field for the rolling lattice.
            velocity_[r].x+=alpha*(-std::clamp(drive.velocity[r].x,-2400.f,2400.f)*.35f-velocity_[r].x);
            velocity_[r].y+=alpha*(-std::clamp(drive.velocity[r].y,-2400.f,2400.f)*.35f-velocity_[r].y);
        }
        return velocity_;
    }
};
}
