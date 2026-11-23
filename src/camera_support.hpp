#pragma once
#include "core.hpp"

namespace mc {

inline Vec2 cellPoint(int cell) { return {(cell%9+.5f)/9,(cell/9+.5f)/9}; }

inline float cellQuality(const MotionSample& flow,int cell) {
    const auto& value=flow.cells[cell];
    return flow.directEvidence?value.directConfidence:value.confidence;
}

inline Vec2 cellVelocity(const MotionSample& flow,int cell) {
    const auto& value=flow.cells[cell];
    return flow.directEvidence?value.directVelocity:value.velocity;
}

inline bool finiteMotion(Vec2 v) {return std::isfinite(v.x)&&std::isfinite(v.y);}

inline bool backgroundDirectionAgrees(Vec2 measured,Vec2 expected) {
    const float a=measured.x*measured.x+measured.y*measured.y;
    const float b=expected.x*expected.x+expected.y*expected.y;
    const float dot=measured.x*expected.x+measured.y*expected.y;
    return a>=25.f*25.f&&b>=28.f*28.f&&dot>0&&dot*dot>=.80f*.80f*a*b;
}

// Do not interpolate across sharply different, independently observed vectors.
inline bool sameLocalMotion(Vec2 a,Vec2 b) {
    const float x=std::hypot(a.x,a.y),y=std::hypot(b.x,b.y);
    const float delta=std::hypot(a.x-b.x,a.y-b.y);
    if(delta<=24+.14f*std::max(x,y))return true;
    return a.x*b.x+a.y*b.y>.60f*x*y&&std::min(x,y)>.38f*std::max(x,y);
}

inline bool cameraFill(const MotionSample& flow,int cell,Vec2& out) {
    const auto& model=flow.cameraModel;
    if(model.confidence<.76f||cellQuality(flow,cell)>=.65f)return false;
    const auto p=cellPoint(cell);
    auto velocity=model.at(p.x,p.y);
    const auto strength=std::hypot(velocity.x,velocity.y);
    if(!finiteMotion(velocity)||strength<28||strength>4000)return false;
    int supporters=0,immediate=0;
    const int x=cell%9,y=cell/9;
    for(int yy=std::max(0,y-2);yy<=std::min(8,y+2);++yy)
        for(int xx=std::max(0,x-2);xx<=std::min(8,x+2);++xx){
            const int neighbor=yy*9+xx;
            if(neighbor==cell)continue;
            const bool close=std::abs(xx-x)<=1&&std::abs(yy-y)<=1;
            supporters+=model.support[neighbor]?1:0;
            immediate+=close&&model.support[neighbor]?1:0;
            if(!close||cellQuality(flow,neighbor)<.65f)continue;
            const auto q=cellPoint(neighbor);
            const auto observed=cellVelocity(flow,neighbor);
            const auto expected=model.at(q.x,q.y);
            if(!finiteMotion(observed))continue;
            // Protect real local motion, including reliably quiet pixels.
            if(!sameLocalMotion(observed,expected))return false;
        }
    if(supporters<3||immediate<1)return false;
    out={velocity.x*.72f,velocity.y*.72f};
    return true;
}

}
