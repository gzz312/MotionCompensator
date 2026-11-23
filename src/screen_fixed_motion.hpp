#pragma once
#include "camera_support.hpp"
#include "motion_evidence.hpp"

namespace mc {

struct BackgroundCorrection {
    Vec2 velocity{};
    float confidence{};
};

// Keep raw observations available for diagnostics and stationary detection.
// This state follows positions in the captured image, independent of dot mode.
class ScreenFixedMotion {
    std::array<float,81> quietFor_{};
    std::array<BackgroundCorrection,81> cells_{};
    std::array<BackgroundCorrection,9> regions_{};
    double timestamp_{};
    int width_{},height_{},correctedCells_{};

    struct Neighbor {int index{};float distance{},weight{};};
    static float speed(Vec2 v){return std::hypot(v.x,v.y);}
    static bool supports(Vec2 observed,Vec2 predicted){
        const float magnitude=predicted.x*predicted.x+predicted.y*predicted.y;
        const float dot=observed.x*predicted.x+observed.y*predicted.y;
        if(magnitude<38.f*38.f||dot<=0)return false;
        const float observedMagnitude=observed.x*observed.x+observed.y*observed.y;
        if(dot*dot<=.78f*.78f*observedMagnitude*magnitude)return false;
        const float dx=observed.x-predicted.x,dy=observed.y-predicted.y,tolerance=30.f+.50f*std::sqrt(magnitude);
        return dx*dx+dy*dy<=tolerance*tolerance;
    }
    static BackgroundCorrection estimate(const MotionSample& flow,int cell,const std::array<bool,81>& moving){
        std::array<Neighbor,48> nearby{};
        const int cx=cell%9,cy=cell/9;
        size_t count=0;
        for(int y=std::max(0,cy-3);y<=std::min(8,cy+3);++y)
            for(int x=std::max(0,cx-3);x<=std::min(8,cx+3);++x){
                const int i=y*9+x;
                if(!moving[i]||i==cell)continue;
                const int dx=x-cx,dy=y-cy;
                const float distance=float(dx*dx+dy*dy);
                nearby[count++]={i,distance,cellQuality(flow,i)/(1.f+.28f*distance)};
            }
        if(count<3)return {};

        const auto position=cellPoint(cell);
        float bestScore=0;
        BackgroundCorrection result;
        for(const auto* model:{&flow.cameraModel,&flow.movingCameraModel}){
            if(model->confidence<.76f)continue;
            const auto predicted=model->at(position.x,position.y);
            if(!finiteMotion(predicted)||speed(predicted)<42||speed(predicted)>3200)continue;
            int supported=0,close=0;
            float score=0,contrary=0,ratioSum=0,speedSum=0;
            for(size_t n=0;n<count;++n){
                const auto& vote=nearby[n];const auto at=cellPoint(vote.index);
                const auto original=cellVelocity(flow,vote.index),local=model->at(at.x,at.y);
                if(backgroundDirectionAgrees(original,local)){
                    ++supported;close+=vote.distance<=5;score+=vote.weight;
                    const float measured=speed(original);
                    ratioSum+=vote.weight*std::clamp(measured/speed(local),.25f,2.2f);
                    speedSum+=vote.weight*measured;
                }else if(vote.distance<=5&&speed(local)>=38)contrary+=vote.weight;
            }
            if(supported>=3&&close>0&&score>=1.25f*contrary&&score>bestScore){
                const float scale=.86f*std::min(ratioSum/score,1.32f*speedSum/(score*speed(predicted)));
                bestScore=score;result={{predicted.x*scale,predicted.y*scale},.76f};
            }
        }
        if(result.confidence)return result;

        // If a global fit fails at this position, accept one nearby coherent
        // direction only. Opposing flows alone are insufficient evidence.
        float total=0;
        for(size_t n=0;n<count;++n)total+=nearby[n].weight;
        const size_t seeds=std::min<size_t>(count,8);
        std::partial_sort(nearby.begin(),nearby.begin()+seeds,nearby.begin()+count,
                          [](const auto& a,const auto& b){return a.weight>b.weight;});
        for(size_t n=0;n<seeds;++n){
            const auto reference=cellVelocity(flow,nearby[n].index);
            Vec2 sum{};float weight=0;int support=0,close=0;
            for(size_t j=0;j<count;++j){
                const auto& vote=nearby[j];const auto v=cellVelocity(flow,vote.index);
                if(!supports(v,reference))continue;
                sum.x+=v.x*vote.weight;sum.y+=v.y*vote.weight;weight+=vote.weight;
                ++support;close+=vote.distance<=5;
            }
            if(support>=3&&close>=2&&weight>=.73f*total&&weight>bestScore){
                bestScore=weight;result={{.74f*sum.x/weight,.74f*sum.y/weight},.69f};
            }
        }
        return result;
    }

public:
    void reset(){quietFor_={};cells_={};regions_={};timestamp_=0;width_=height_=correctedCells_=0;}
    const auto& cells() const {return cells_;}
    const auto& regions() const {return regions_;}
    int correctedCells() const {return correctedCells_;}

    void update(const MotionSample& flow,const SpatialEvidence& evidence,bool fresh){
        if(!fresh||!flow.directEvidence||evidence.stationary){reset();return;}
        if(width_!=flow.sourceWidth||height_!=flow.sourceHeight||flow.timestamp<timestamp_||flow.timestamp-timestamp_>.12)reset();
        if(flow.timestamp==timestamp_)return;
        const float elapsed=timestamp_>0?float(flow.timestamp-timestamp_):0.f;
        timestamp_=flow.timestamp;width_=flow.sourceWidth;height_=flow.sourceHeight;
        cells_={};regions_={};correctedCells_=0;
        std::array<bool,81> moving{};
        for(int i=0;i<81;++i){
            const auto v=cellVelocity(flow,i);
            const bool valid=cellQuality(flow,i)>=.65f&&finiteMotion(v);
            quietFor_[i]=valid&&speed(v)<22.f?std::min(.35f,quietFor_[i]+elapsed):0.f;
            moving[i]=valid&&speed(v)>=45&&evidence.connected[i];
        }
        if(!evidence.broadMotion)return;
        for(int i=0;i<81;++i){
            if(quietFor_[i]<.075f)continue;
            cells_[i]=estimate(flow,i,moving);
            correctedCells_+=cells_[i].confidence>0;
        }
        for(int r=0;r<9;++r){
            std::array<BackgroundCorrection,9> corrections{};
            int count=0;
            for(int dy=0;dy<3;++dy)for(int dx=0;dx<3;++dx){
                const auto& item=cells_[(r/3*3+dy)*9+r%3*3+dx];
                if(item.confidence>0)corrections[count++]=item;
            }
            int best=0;
            for(int j=0;j<count;++j){
                Vec2 sum{};float weight=0;int matches=0;
                for(int k=0;k<count;++k){
                    const auto& c=corrections[k];
                    if(k!=j&&!supports(c.velocity,corrections[j].velocity))continue;
                    sum.x+=c.velocity.x*c.confidence;sum.y+=c.velocity.y*c.confidence;
                    weight+=c.confidence;++matches;
                }
                if(matches>=2&&matches*3>=count*2&&matches>best){
                    best=matches;regions_[r]={{sum.x/weight,sum.y/weight},weight/matches};
                }
            }
        }
    }
};

}
