#pragma once
#include "core.hpp"
namespace mc {
// Small immutable luminance snapshots; capture owns temporal filtering.
struct BackgroundLight {
    static constexpr int W=64,H=36;
    std::array<float,W*H> values{};
    double timestamp=0;
    float sample(float x,float y,float w,float h) const {
        float u=std::clamp(x/std::max(1.f,w)*W-.5f,0.f,float(W-1));
        float v=std::clamp(y/std::max(1.f,h)*H-.5f,0.f,float(H-1));
        int a=int(u),b=int(v),c=std::min(a+1,W-1),d=std::min(b+1,H-1);
        return std::lerp(std::lerp(values[b*W+a],values[b*W+c],u-a),std::lerp(values[d*W+a],values[d*W+c],u-a),v-b);
    }
    void update(const std::vector<float>& pixels,int width,int height,double stamp) {
        if(width<1||height<1||pixels.size()!=size_t(width*height))return;
        float dt=timestamp>0?float(std::clamp(stamp-timestamp,0.,.2)):0;
        for(int y=0;y<H;++y)for(int x=0;x<W;++x){
            float sum=0;int n=0;
            for(int j=y*height/H;j<(y+1)*height/H;++j)for(int i=x*width/W;i<(x+1)*width/W;++i){sum+=pixels[j*width+i];++n;}
            float target=n?sum/n:.5f;auto& value=values[y*W+x];
            if(timestamp==0)value=target;
            else if(std::abs(target-value)>.035f)value+=std::clamp((target-value)*(1-std::exp(-dt/.35f)),-dt*.65f,dt*.65f);
        }
        timestamp=stamp;
    }
};
inline Color tintLightness(Color c,float light){
    // RGB interpolation toward black/white preserves the selected hue.
    float base=(std::max({c.r,c.g,c.b})+std::min({c.r,c.g,c.b}))*.5f;
    if(light>base){float t=(light-base)/std::max(.001f,1-base);return {std::lerp(c.r,1.f,t),std::lerp(c.g,1.f,t),std::lerp(c.b,1.f,t)};}
    float t=light/std::max(.001f,base);return {c.r*t,c.g*t,c.b*t};
}
struct DotLayer {float radius,opacity;Color color;};
struct DotLayers {
    std::array<DotLayer,9> layers{};size_t count=0;
    void push_back(DotLayer layer){layers[count++]=layer;}
    const DotLayer* begin() const{return layers.data();}
    const DotLayer* end() const{return layers.data()+count;}
};
inline DotLayers dotLayers(Color color,float radius,float opacity,bool halo,int appearance,float background){
    DotLayers out;
    if(appearance==1||appearance==3)color=tintLightness(color,std::lerp(.82f,.28f,std::clamp(background,0.f,1.f)));
    if(halo)for(int i=6;i>=1;--i)out.push_back({radius*(1.35f+i*.23f),opacity*.025f,color});
    if(appearance>=2){out.push_back({radius+std::max(1.f,radius*.18f),opacity*.65f,tintLightness(color,.16f)});out.push_back({radius+std::max(.5f,radius*.08f),opacity*.8f,tintLightness(color,.88f)});}
    out.push_back({radius,opacity,color});return out;
}
}
