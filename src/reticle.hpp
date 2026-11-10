#pragma once
#include "core.hpp"
namespace mc {
// Existing IDs 0..2 remain stable for saved settings.
inline constexpr int ReticleShapeCount=8;
inline const wchar_t* reticleName(int shape,bool chinese){
    static constexpr std::array zh={L"关闭",L"十字",L"菱十字",L"圆点",L"圆环",L"留空十字",L"T形",L"菱形"};
    static constexpr std::array en={L"Off",L"Cross",L"Diamond cross",L"Dot",L"Ring",L"Open cross",L"T-shape",L"Diamond"};
    return (chinese?zh:en)[std::clamp(shape,0,ReticleShapeCount-1)];
}
struct ReticleLine {Vec2 from,to;};
struct ReticleCircle {Vec2 center;float radius;bool filled;};
struct ReticleGeometry {
    std::array<ReticleLine,8> lines{};int lineCount=0;
    std::array<ReticleCircle,1> circles{};int circleCount=0;
};
inline ReticleGeometry buildReticle(int shape,Vec2 center,float size){
    ReticleGeometry result;
    if(shape<=0||shape>=ReticleShapeCount||!std::isfinite(size)||size<=0)return result;
    auto line=[&](float x,float y,float xx,float yy){result.lines[result.lineCount++]={{center.x+x,center.y+y},{center.x+xx,center.y+yy}};};
    auto cross=[&](float gap,bool top){line(-size,0,-gap,0);line(gap,0,size,0);if(top)line(0,-size,0,-gap);line(0,gap,0,size);};
    auto diamond=[&]{line(0,-size,size,0);line(size,0,0,size);line(0,size,-size,0);line(-size,0,0,-size);};
    float gap=std::min(2.f,size*.5f);
    switch(shape){
    case 1:cross(gap,true);break;
    case 2:cross(gap,true);diamond();break;
    case 3:result.circles[result.circleCount++]={center,std::max(1.f,size*.18f),true};break;
    case 4:result.circles[result.circleCount++]={center,size,false};break;
    case 5:cross(size*.38f,true);break;
    case 6:cross(size*.38f,false);break;
    case 7:diamond();break;
    }
    return result;
}
}
