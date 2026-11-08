#include "tracking.hpp"
#include "flow.hpp"
#include <random>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstdlib>
void require(bool ok,const char* reason) {if(!ok) {std::cerr<<"FAIL: "<<reason<<'\n';std::exit(1);}}
static std::vector<float> readPGM(const std::filesystem::path& path,int& w,int& h) {
    std::ifstream file(path,std::ios::binary);std::string magic;int range=0;file>>magic>>w>>h>>range;file.get();
    if(magic!="P5" || range!=255 || w<1 || h<1) throw std::runtime_error("Invalid diagnostic PGM");
    std::vector<float> image(size_t(w)*h);for(auto& value:image) {unsigned char byte{};file.read(reinterpret_cast<char*>(&byte),1);value=byte/255.f;}return image;
}
int main(int argc,char** argv) {
    if(argc==2) {
        std::cout<<"pair,old_conf,new_conf,features,tracked,inliers,issue,vx,vy\n";
        for(auto file:std::filesystem::directory_iterator(argv[1])) if(file.path().extension()==".txt") {
            double dt=0,stamp=0,oldConf=0;int sw=0,sh=0,w=0,h=0;
            std::ifstream(file.path())>>dt>>stamp>>sw>>sh>>oldConf;
            auto base=file.path().parent_path()/file.path().stem();auto a=readPGM(base.string()+"-a.pgm",w,h),b=readPGM(base.string()+"-b.pgm",w,h);
            auto old=mc::estimateFlow(a,b,w,h,dt,stamp,sw,sh),sample=mc::estimateTrackedFlow(a,b,w,h,dt,stamp,sw,sh);
            std::cout<<file.path().stem().string()<<','<<old.confidence<<','<<sample.confidence<<','<<sample.features<<','<<sample.tracked<<','<<sample.inliers<<','<<int(sample.issue)<<','<<sample.velocity[4].x<<','<<sample.velocity[4].y<<'\n';
        }
        return 0;
    }
    constexpr int w=320,h=180;std::mt19937 rng(17);std::vector<float> image(w*h);
    // Smooth texture resembles spatially filtered capture and makes fractional
    // displacement testable. No rendering or independent game scene is used.
    std::vector<float> noise(w*h);for(auto& v:noise) v=float(rng()%256)/255;
    for(int y=1;y<h-1;++y) for(int x=1;x<w-1;++x) {
        float sum=0;for(int yy=-1;yy<=1;++yy) for(int xx=-1;xx<=1;++xx) sum+=noise[(y+yy)*w+x+xx];image[y*w+x]=sum/9;
    }
    auto sampleAt=[&](float x,float y) {
        int xx=int(std::floor(x)),yy=int(std::floor(y));if(xx<0 || yy<0 || xx+1>=w || yy+1>=h) return 0.f;
        float a=x-xx,b=y-yy;return (1-b)*((1-a)*image[yy*w+xx]+a*image[yy*w+xx+1])+b*((1-a)*image[(yy+1)*w+xx]+a*image[(yy+1)*w+xx+1]);
    };
    auto translate=[&](float dx,float dy) {std::vector<float> out(w*h);for(int y=0;y<h;++y) for(int x=0;x<w;++x) out[y*w+x]=sampleAt(x-dx,y-dy);return out;};
    auto fractional=translate(.45f,-.7f);auto sub=mc::estimateTrackedFlow(image,fractional,w,h,.04,10,3200,1800);
    require(sub.confidence>=.65f,"subpixel tracking confidence");
    require(std::abs(sub.velocity[4].x*.04f/10-.45f)<.15f && std::abs(sub.velocity[4].y*.04f/10+.7f)<.15f,"subpixel motion direction and scale");
    mc::MotionFilter filter;
    auto offsets=filter.update(sub,10.05,.016f,true);
    require(offsets[4].x<-.1f && offsets[4].y>.1f,"tracked sample reaches reverse overlay displacement");
    auto previousOffset=std::hypot(offsets[4].x,offsets[4].y);
    offsets=filter.update(sub,10.2,.016f,true);
    require(std::hypot(offsets[4].x,offsets[4].y)<previousOffset,"expired tracking sample decays");
    auto moved=translate(9,-4);auto fast=mc::estimateTrackedFlow(image,moved,w,h,.04,10,3200,1800);
    require(fast.confidence>=.65f,"larger translation confidence");
    require(std::abs(fast.velocity[4].x*.04/10-9)<.2 && std::abs(fast.velocity[4].y*.04/10+4)<.2,"larger motion direction");
    std::vector<float> flat(w*h,.5f);require(mc::estimateTrackedFlow(flat,flat,w,h,.04,10,3200,1800).confidence==0,"flat scene rejected");
    auto cut=image;for(auto& v:cut) v=float(rng()%256)/255;
    require(mc::estimateTrackedFlow(image,cut,w,h,.04,10,3200,1800).confidence==0,"scene cut rejected");
    auto stationary=mc::estimateTrackedFlow(image,image,w,h,.04,10,3200,1800);
    require(stationary.confidence>=.65f && std::abs(stationary.velocity[4].x)<1 && std::abs(stationary.velocity[4].y)<1,"still image remains still");
    require(mc::estimateTrackedFlow(image,moved,w,h,.4,10,3200,1800).confidence==0,"long gap rejected");
    std::cout<<"PASS: subpixel, translation, stillness, flat scene, cut and gap\n";
}
