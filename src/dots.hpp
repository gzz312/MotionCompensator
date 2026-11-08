#pragma once
#include "core.hpp"
#include "lattice.hpp"
#include <vector>
namespace mc {
struct DotVisual {Vec2 position;float opacity=1,radiusScale=1;int region=0;float blendWeight=1;};
// Fixed 4x4 reference. Outer columns carry halos; inner columns stay compact.
inline std::vector<DotVisual> staticDots(int w,int h,const VisualSettings& settings){
    std::vector<DotVisual> result;
    if(w<=0||h<=0)return result;
    float margin=std::min(float(std::min(w,h))*.2f,settings.radius*3+2);
    float t=std::clamp((settings.spacing-60)/180,0.f,1.f);
    float dx=std::min(w*std::lerp(.50f,.72f,t)*.5f,w*.5f-margin),dy=std::min(h*std::lerp(.60f,.84f,t)*.5f,h*.5f-margin);
    // Maintain the central clearance without removing any of the 16 dots.
    dx=std::max(dx,std::min(w*.5f-margin,std::min(w,h)*settings.holeDiameter*1.6f));
    for(int row=0;row<4;++row)for(int col=0;col<4;++col){
        float x=w*.5f+(col/3.f*2-1)*dx;
        if(settings.staggered&&(row&1))x+=dx/6;
        float y=h*.5f+(row/3.f*2-1)*dy;
        // Enforce clearance after stagger: retain all sixteen points and row
        // alignment, moving only an encroaching column away from the center.
        float outer=settings.radius*(col==0||col==3?2.73f:1.f);
        float required=std::min(w,h)*settings.holeDiameter*.5f+outer+1;
        float vertical=y-h*.5f;
        float clearX=std::sqrt(std::max(0.f,required*required-vertical*vertical));
        if(std::abs(x-w*.5f)<clearX)x=w*.5f+(col<2?-clearX:clearX);
        x=std::clamp(x,margin,w-margin);
        result.push_back({{x,y},1,1,col==0||col==3?1:0});
    }
    return result;
}
inline float smoothUnit(float x) {x=std::clamp(x,0.f,1.f);return x*x*(3-2*x);}
inline float conservativeBoundaryFade(float x,float y,int region,float w,float h,float band) {
    if(band<=0) return 1;
    int col=region%3,row=region/3;float distance=band;
    if(col>0) distance=std::min(distance,x-col*w/3);
    if(col<2) distance=std::min(distance,(col+1)*w/3-x);
    if(row>0) distance=std::min(distance,y-row*h/3);
    if(row<2) distance=std::min(distance,(row+1)*h/3-y);
    return .75f+.25f*smoothUnit(distance/band);
}
inline Vec2 sampleCue(const std::array<Vec2,9>& field,float x,float y,float w,float h) {
    float u=std::clamp(x/w,0.f,1.f)*3-.5f,v=std::clamp(y/h,0.f,1.f)*3-.5f;
    int col=std::clamp(int(std::floor(u)),0,1),row=std::clamp(int(std::floor(v)),0,1);
    float a=std::clamp(u-col,0.f,1.f),b=std::clamp(v-row,0.f,1.f);
    auto mix=[](Vec2 p,Vec2 q,float t){return Vec2{std::lerp(p.x,q.x,t),std::lerp(p.y,q.y,t)};};
    return mix(mix(field[row*3+col],field[row*3+col+1],a),mix(field[(row+1)*3+col],field[(row+1)*3+col+1],a),b);
}
class RegionalDotField {
    std::array<Vec2,9> phase_{},cue_{};
    std::vector<DotVisual> dots_;
    int width_=0,height_=0;float spacing_=0;bool dynamic_=false;
public:
    void reset() {phase_={};cue_={};dots_.clear();width_=height_=0;}
    const auto& dots() const {return dots_;}
    const auto& phases() const {return phase_;}
    float phaseMax() const {float value=0;for(auto p:phase_) value=std::max(value,std::hypot(p.x,p.y));return value;}
    const std::vector<DotVisual>& update(const std::array<Vec2,9>& velocity,int w,int h,float spacing,float dt,bool dynamic,bool enhanced=true,bool boundaryFade=false,float radius=3,bool staggered=false) {
        if(w<=0 || h<=0) {reset();return dots_;}
        spacing=std::clamp(std::isfinite(spacing)?spacing:122.f,60.f,240.f);
        if(w!=width_ || h!=height_ || spacing!=spacing_ || dynamic!=dynamic_) {
            reset();width_=w;height_=h;spacing_=spacing;dynamic_=dynamic;
        }
        float seconds=std::clamp(std::isfinite(dt)?dt:0.f,0.f,.05f),alpha=1-std::exp(-seconds/.06f);
        for(int r=0;r<9;++r) {
            auto finite=[](float x){return std::isfinite(x)?std::clamp(x,-1200.f,1200.f):0.f;};
            Vec2 speed{dynamic?finite(velocity[r].x):0,dynamic?finite(velocity[r].y):0};
            // Original regional scrolling is retained; appearance never alters phase.
            // Integrate continuously without the 0.8 six-pixel position cap.
            phase_[r].x=std::fmod(phase_[r].x+speed.x*seconds,spacing);
            phase_[r].y=std::fmod(phase_[r].y+speed.y*seconds,verticalPeriod(spacing,staggered));
            float length=std::hypot(speed.x,speed.y);Vec2 target{};
            if(length>2) {float strength=std::clamp((length-2)/240.f,0.f,1.f);target={speed.x/length*strength,speed.y/length*strength};}
            cue_[r].x+=alpha*(target.x-cue_[r].x);cue_[r].y+=alpha*(target.y-cue_[r].y);
            if(!dynamic) {phase_[r]={};cue_[r]={};}
        }
        dots_.clear();
        if(!dynamic) {
            visitLattice(0,0,float(w),float(h),spacing,spacing,spacing*.5f,spacing*.5f,0,0,staggered,[&](float x,float y){
                if(x>=0 && y>=0 && x<w && y<h)dots_.push_back({{x,y},1,1,0});
            });
            return dots_;
        }
        float band=std::min(std::clamp(radius,1.f,24.f)*2.5f,spacing*.06f);
        for(int r=0;r<9;++r) {
            int col=r%3,row=r/3;float left=col*w/3.f,right=(col+1)*w/3.f,top=row*h/3.f,bottom=(row+1)*h/3.f;
            float pad=0;
            auto phase=phase_[r];
            visitLattice(left-pad,top-pad,right+pad,bottom+pad,spacing,spacing,spacing*.5f,spacing*.5f,phase.x,phase.y,staggered,[&](float px,float py) {
                float opacity=1,size=1,weight=1;
                if(enhanced) {
                    auto cue=sampleCue(cue_,px,py,float(w),float(h));
                    float strength=std::min(1.f,std::hypot(cue.x,cue.y)),denominator=std::abs(cue.x)+std::abs(cue.y);
                    float x=std::clamp((px-w*.5f)/(w*.5f),-1.f,1.f),y=std::clamp((py-h*.5f)/(h*.5f),-1.f,1.f);
                    float direction=denominator>0?(x*cue.x+y*cue.y)/denominator:0;
                    float attenuation=strength*(1-direction)*.5f;
                    size=1-.10f*attenuation;opacity=1-.15f*attenuation;
                }
                if(boundaryFade) opacity*=conservativeBoundaryFade(px,py,r,float(w),float(h),band);
                dots_.push_back({{px,py},opacity,size,r,weight});
            });
        }
        return dots_;
    }
};
struct EdgeAppearance {float opacity=0,radiusScale=1;};
inline EdgeAppearance edgeAppearance(float x,float y,float width,float height,float sx,float sy,int parity,bool enhanced=true){
    float fadeX=std::min(sx*.9f,width*.46f),fadeY=std::min(sy*.9f,height*.46f);
    float dx=std::min(x,width-x),dy=std::min(y,height-y);
    float opacity=smoothUnit(dx/std::max(1.f,fadeX))*smoothUnit(dy/std::max(1.f,fadeY));
    if(opacity<=0)return {};
    if(!enhanced)return {opacity,1};
    // Keep the body of the journey at a stable size. Only the last, already
    // faint part shrinks gently; alpha is responsible for disappearing.
    float tail=smoothUnit((.22f-opacity)/.22f);
    float variation=parity%2!=0?.72f:1.f;
    return {opacity,1.20f*(1-.15f*tail)*variation};
}
class EdgeDotField {
    std::array<Vec2,4> phase_{};
    std::vector<DotVisual> dots_;
    int w_=0,h_=0;float spacing_=0,width_=0;int edges_=-1;
public:
    void reset() {phase_={};dots_.clear();w_=h_=0;}
    const auto& dots() const {return dots_;}
    float phaseMax() const {float result=0;for(auto p:phase_)result=std::max(result,std::hypot(p.x,p.y));return result;}
    const std::vector<DotVisual>& update(const std::array<Vec2,9>& field,int w,int h,const VisualSettings& settings,float dt) {
        if(w<=0 || h<=0) {reset();return dots_;}
        float spacing=std::clamp(settings.spacing,8.f,240.f);
        float band=std::min(settings.edgeWidth,float(std::min(w,h))*.22f);
        if(w!=w_ || h!=h_ || spacing!=spacing_ || band!=width_ || settings.edges!=edges_) {reset();w_=w;h_=h;spacing_=spacing;width_=band;edges_=settings.edges;}
        float seconds=std::clamp(std::isfinite(dt)?dt:0.f,0.f,.05f);dots_.clear();
        for(int side=0;side<4;++side) {
            bool vertical=side<2;
            if((settings.edges==0 && !vertical) || (settings.edges==1 && vertical)) continue;
            float left=side==1?w-band:0,top=side==3?h-band:0;
            float width=vertical?band:float(w),height=vertical?float(h):band;
            // Reserve corners for the vertical strips when all four sides show.
            if(!vertical && settings.edges==2) {left=band;width=w-2*band;}
            int index=side==0?3:side==1?5:side==2?1:7;auto speed=field[index];
            if(!std::isfinite(speed.x))speed.x=0;
            if(!std::isfinite(speed.y))speed.y=0;
            int cols=std::max(1,int(std::floor(width/(vertical?spacing*.6f:spacing))));
            int rows=std::max(1,int(std::floor(height/(vertical?spacing:spacing*.6f))));
            // Keep a second lane visible while the first wraps through its
            // fade, rather than pulsing an entire single-column strip off.
            if(vertical) cols=std::max(2,cols);else rows=std::max(2,rows);
            float sx=width/cols,sy=height/rows;
            phase_[side].x=std::fmod(phase_[side].x+std::clamp(speed.x,-1200.f,1200.f)*seconds,2*sx);
            phase_[side].y=std::fmod(phase_[side].y+std::clamp(speed.y,-1200.f,1200.f)*seconds,2*sy);
            // Appearance is anchored to position and lattice identity, never
            // to the velocity sign. Reversing input only reverses integration.
            visitLatticeIndexed(left,top,left+width,top+height,sx,sy,left+sx*.5f,top+sy*.5f,phase_[side].x,phase_[side].y,settings.staggered,[&](float x,float y,int row,int col) {
                auto appearance=edgeAppearance(x-left,y-top,width,height,sx,sy,row+col+side,settings.enhancement);
                if(appearance.opacity<=0)return;
                dots_.push_back({{x,y},appearance.opacity,appearance.radiusScale,side,1});
            });
        }
        return dots_;
    }
};

}
