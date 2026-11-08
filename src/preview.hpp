#pragma once
#include "dots.hpp"
#include "appearance.hpp"
namespace mc {
struct PreviewDot {Vec2 position;float radius,opacity;Color color{};};
struct PreviewLine {Vec2 from,to;};
struct PreviewScene {
    Color dotColor{},reticleColor{};float reticleOpacity=1;
    std::vector<PreviewDot> dots;
    std::vector<PreviewLine> reticle;
    int width=324,height=272;
    float contentLeft=0,contentTop=0,contentRight=324,contentBottom=272;
};
inline PreviewScene buildPreview(const VisualSettings& settings,int w=324,int h=272,int sourceW=1920,int sourceH=1080) {
    PreviewScene result;result.dotColor=settings.dots;result.reticleColor=settings.reticle;result.reticleOpacity=settings.reticleOpacity;
    result.width=std::max(0,w);result.height=std::max(0,h);
    if(w<=0 || h<=0 || sourceW<=0 || sourceH<=0)return result;
    float scale=std::min(float(w)/sourceW,float(h)/sourceH);
    float ox=(w-sourceW*scale)*.5f,oy=(h-sourceH*scale)*.5f;
    result.contentLeft=ox;result.contentTop=oy;result.contentRight=ox+sourceW*scale;result.contentBottom=oy+sourceH*scale;
    auto add=[&](const DotVisual& dot) {
        float radius=settings.radius*dot.radiusScale;
        float alpha=dot.opacity*settings.opacity*(settings.style==2?1.f:dotVisibility(dot.position.x,dot.position.y,float(sourceW),float(sourceH),radius,settings.holeDiameter));
        if(alpha>0 && dot.position.x>=0 && dot.position.y>=0 && dot.position.x<sourceW && dot.position.y<sourceH)
            for(auto layer:dotLayers(settings.dots,radius,alpha,settings.style==2&&dot.region==1,settings.appearance,dot.position.x/sourceW))
                result.dots.push_back({{ox+dot.position.x*scale,oy+dot.position.y*scale},std::max(.75f,layer.radius*scale),layer.opacity,layer.color});
    };
    std::array<Vec2,9> still{};
    if(settings.style==2){for(const auto& dot:staticDots(sourceW,sourceH,settings))add(dot);}
    else if(settings.style==1) {
        EdgeDotField field;
        for(const auto& dot:field.update(still,sourceW,sourceH,settings,0))add(dot);
    } else {
        RegionalDotField field;
        for(const auto& dot:field.update(still,sourceW,sourceH,settings.spacing,0,false,false,false,settings.radius,settings.staggered))add(dot);
    }
    if(settings.reticleShape>0 && settings.reticleOpacity>0) {
        float x=w*.5f,y=h*.5f,a=std::max(4.f,std::clamp(settings.reticleSize,float(ReticleSizeMin),float(ReticleSizeMax))*scale);
        result.reticle={{{x-a,y},{x-1,y}},{{x+1,y},{x+a,y}},{{x,y-a},{x,y-1}},{{x,y+1},{x,y+a}}};
        if(settings.reticleShape==2) {result.reticle.push_back({{x,y-a},{x+a,y}});result.reticle.push_back({{x+a,y},{x,y+a}});result.reticle.push_back({{x,y+a},{x-a,y}});result.reticle.push_back({{x-a,y},{x,y-a}});}
    }
    return result;
}
}
