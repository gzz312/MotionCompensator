#include "appearance.hpp"
#include "dots.hpp"
#include <cstdlib>
#include <iostream>
void require(bool b,const char* why){if(!b){std::cerr<<why;std::exit(1);}}
int main(){
    mc::VisualSettings v;v.style=2;
    for(auto size:std::array{mc::Vec2{1920,1080},mc::Vec2{2560,1600},mc::Vec2{800,600}})for(float radius:{1.f,3.f,24.f})for(float spacing:{60.f,240.f})for(float hole:{.05f,.10f,.25f})for(bool stagger:{false,true}){
        v.radius=radius;v.spacing=spacing;v.staggered=stagger;v.holeDiameter=hole;auto dots=mc::staticDots(int(size.x),int(size.y),v);
        require(dots.size()==16,"static count must always be 16");int halos=0;
        for(auto d:dots){halos+=d.region==1;float outer=radius*(d.region==1?2.73f:1.f);require(d.position.x>=outer && d.position.y>=outer && d.position.x+outer<=size.x && d.position.y+outer<=size.y,"static halos remain inside viewport");}
        for(auto d:dots){float outer=radius*(d.region==1?2.73f:1.f);require(std::hypot(d.position.x-size.x*.5f,d.position.y-size.y*.5f)-outer>=std::min(size.x,size.y)*hole*.5f-.01f,"staggered static dots and halos remain outside the central clearance");}
        require(halos==8,"only outer columns carry halos");
    }
    mc::BackgroundLight light;std::vector<float> image(128*72,.1f);light.update(image,128,72,1);
    require(std::abs(light.sample(50,50,100,100)-.1f)<.001,"dark patch sampled");
    std::fill(image.begin(),image.end(),1.f);light.update(image,128,72,1.1);
    require(light.values[0]>.1 && light.values[0]<=.166f,"flash response is rate limited");
    image.assign(128*72,.16f);float before=light.values[0];light.update(image,128,72,1.2);require(before==light.values[0],"small differences stay inside hysteresis");
    auto dark=mc::dotLayers({0,1,0},10,.25,false,1,0),bright=mc::dotLayers({0,1,0},10,.25,false,1,1);
    require(dark.count==1&&dark.layers[0].color.g>bright.layers[0].color.g,"dark backgrounds brighten and bright backgrounds darken selected hue");
    require(dark.layers[0].opacity==.25f && bright.layers[0].opacity==.25f,"adaptation never cancels alpha fading");
    auto outline=mc::dotLayers({0,1,0},10,0,true,3,.5);
    require(outline.count==9,"combined outer dot has halo, two outlines and core");
    for(auto layer:outline)require(layer.opacity==0,"invisible dots cannot be resurrected");
    std::cout<<"PASS: fixed count/halo placement, brightness sampling, temporal response, hue adaptation, alpha preservation\n";
}
