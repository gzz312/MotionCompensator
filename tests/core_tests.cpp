#include "core.hpp"
#include "flow.hpp"
#include <random>
#include <iostream>
#include <limits>
#include <cstdlib>
void require(bool b,const char* reason) { if(!b) { std::cerr<<reason<<'\n'; std::exit(1); } }
int main() {
    require(mc::dotVisibility(500,300,1000,600,3,.1f)==0,"center exclusion");
    require(mc::dotVisibility(531,300,1000,600,3,.1f)==0,"exclude entire dot radius");
    require(mc::dotVisibility(990,300,1000,600,3,.1f)>mc::dotVisibility(600,300,1000,600,3,.1f),"peripheral opacity gradient");

    std::vector<float> image(160*90),shifted(image.size());std::mt19937 random(42);
    for(auto& v:image) v=float(random()%256)/255.f;
    for(int y=0;y<90;++y) for(int x=0;x<160;++x) shifted[y*160+x]=(x>=3 && y>=2)?image[(y-2)*160+x-3]:0;
    auto flow=mc::estimateFlow(image,shifted,160,90,.05,10,1600,900);
    require(flow.confidence>.9f,"translated texture confidence");
    require(std::abs(flow.velocity[4].x-600)<1 && std::abs(flow.velocity[4].y-400)<1,"flow direction, timestamp and resolution units");
    require(mc::estimateFlow(image,shifted,160,90,.5,10,1600,900).confidence==0,"flow gap rejection");
    std::vector<float> flat(image.size(),.5f);
    require(mc::estimateFlow(flat,flat,160,90,.05,10,1600,900).confidence==0,"textureless rejection");
    for(auto& v:shifted) v=float(random()%256)/255.f;
    require(mc::estimateFlow(image,shifted,160,90,.05,10,1600,900).confidence==0,"scene cut rejection");

    auto translate=[&](int dx,int dy,float exposure=0) {
        std::vector<float> out(image.size(),0);
        for(int y=0;y<90;++y) for(int x=0;x<160;++x)
            if(x-dx>=0 && x-dx<160 && y-dy>=0 && y-dy<90) out[y*160+x]=image[(y-dy)*160+x-dx]+exposure;
        return out;
    };
    auto fast=translate(10,-4);
    auto quick=mc::estimateFlow(image,fast,160,90,.05,10,1600,900);
    require(quick.confidence>=.65f,"pyramid fast translation confidence");
    require(std::abs(quick.velocity[4].x-2000)<1 && std::abs(quick.velocity[4].y+800)<1,"pyramid fast translation direction");
    auto bright=translate(2,2,.1f);
    require(mc::estimateFlow(image,bright,160,90,.05,10,1600,900).confidence>=.65f,"uniform exposure tolerance");
    auto occlusion=translate(2,0);
    for(int y=37;y<=53;++y) for(int x=62;x<=98;++x) occlusion[y*160+x]=float(random()%256)/255;
    auto partial=mc::estimateFlow(image,occlusion,160,90,.05,10,1600,900);
    require(partial.confidence>=.65f && partial.regionConfidence[4]==0,"occlusion rejects one region without disabling background");
    auto independent=translate(2,0);
    for(int y=34;y<=56;++y) for(int x=58;x<=102;++x) independent[y*160+x]=image[y*160+x+6];
    auto object=mc::estimateFlow(image,independent,160,90,.05,10,1600,900);
    require(object.confidence>=.65f && object.regionConfidence[4]==0,"independent motion consensus rejection");
    auto invalid=image;invalid[0]=std::numeric_limits<float>::quiet_NaN();
    require(mc::estimateFlow(image,invalid,160,90,.05,10,1600,900).confidence==0,"nonfinite image rejection");
    require(mc::estimateFlow(image,image,160,90,.05,10,1600,900).confidence>=.65f,"textured still frame stays stable");
    require(mc::estimateFlow(image,fast,160,90,-.05,10,1600,900).confidence==0,"nonmonotonic time rejection");

    mc::MotionFilter f; mc::MotionSample s; s.timestamp=10.; s.confidence=.9f;
    for(auto& v:s.velocity) v={1000.f,-1000.f};
    auto result=f.update(s,10.02,.016f,true);
    require(result[0].x<0 && result[0].y>0,"compensation sign");
    require(std::abs(result[0].x)<24,"amplitude clamp and smoothing");
    float previous=std::abs(result[0].x);
    result=f.update(s,11.,.016f,true); require(std::abs(result[0].x)<previous,"stale observation decay");
    previous=std::abs(result[0].x); s.timestamp=12;
    result=f.update(s,11.,.016f,true); require(std::abs(result[0].x)<previous,"future timestamp rejection");
    s.timestamp=11; s.confidence=.2f; previous=std::abs(result[0].x);
    result=f.update(s,11.,.016f,true); require(std::abs(result[0].x)<previous,"low confidence decay");
    s.confidence=.9f; s.velocity[0].x=std::numeric_limits<float>::quiet_NaN();
    result=f.update(s,11.,.016f,true); require(std::isfinite(result[0].x),"NaN rejection");
    s.velocity[1]={1000,0};s.regionConfidence[1]=0;
    previous=std::abs(result[1].x);
    result=f.update(s,11.,.016f,true);require(std::abs(result[1].x)<previous,"per region invalid motion decays independently");
    for(int i=0;i<120;++i) result=f.update(s,11.,.016f,false);
    require(std::abs(result[0].x)<.001f,"static mode settles"); f.reset();
    require(mc::knownGame(L"payday2_win64_release.exe"),"PAYDAY detection");
    require(!mc::knownGame(L"explorer.exe"),"do not auto-target desktop");
    require(mc::knownGame(L"sb-win64-shipping.exe"),"Stellar Blade detection");
    require(mc::knownGame(L"forzahorizon6.exe"),"Forza Horizon 6 detection");
    std::cout<<"PASS: sign, clamp, smoothing, stale/future/low-confidence/NaN rejection, static settling, game allowlist\n";
}
