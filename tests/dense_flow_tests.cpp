#include "tracking.hpp"
#include "dynamic.hpp"
#include "dots.hpp"
#include <random>
#include <chrono>
#include <iostream>
#include <cstdlib>
#include <limits>
void require(bool ok,const char* why){if(!ok){std::cerr<<"FAIL: "<<why<<'\n';std::exit(1);}}
std::vector<float> texture(int w,int h,int seed){
    std::mt19937 rng(seed);std::vector<float> noise(w*h),image(w*h);
    for(auto& v:noise)v=float(rng()%256)/255;
    for(int y=1;y<h-1;++y)for(int x=1;x<w-1;++x){float sum=0;for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx)sum+=noise[(y+dy)*w+x+dx];image[y*w+x]=sum/9;}
    return image;
}
float sample(const std::vector<float>& image,int w,int h,float x,float y){
    int ix=int(std::floor(x)),iy=int(std::floor(y));if(ix<0||iy<0||ix+1>=w||iy+1>=h)return .5f;
    float u=x-ix,v=y-iy;
    return (1-v)*((1-u)*image[iy*w+ix]+u*image[iy*w+ix+1])+v*((1-u)*image[(iy+1)*w+ix]+u*image[(iy+1)*w+ix+1]);
}
int main(){
    constexpr int w=960,h=540;auto a=texture(w,h,47);std::vector<float> b(w*h);
    mc::DenseFlowWorkspace workspace;
    std::vector<double> denseTimes,sparseTimes;
    for(float shift:{2.f,12.f,32.f,48.f}){
        float dy=-shift*.4f;
        for(int y=0;y<h;++y)for(int x=0;x<w;++x)b[y*w+x]=sample(a,w,h,x-shift,y-dy);
        auto start=std::chrono::steady_clock::now();auto dense=mc::estimateDenseFlow(a,b,w,h,.05,10,1920,1080,&workspace);
        auto middle=std::chrono::steady_clock::now();auto sparse=mc::estimateTrackedFlow(a,b,w,h,.05,10,1920,1080,nullptr,4);
        auto end=std::chrono::steady_clock::now();
        denseTimes.push_back(std::chrono::duration<double,std::milli>(middle-start).count());sparseTimes.push_back(std::chrono::duration<double,std::milli>(end-middle).count());
        std::cout<<"translation shift="<<shift<<" dense_ms="<<denseTimes.back()<<" sparse_ms="<<sparseTimes.back()<<" dense_q="<<dense.confidence<<" sparse_q="<<sparse.confidence<<" measured_cells="<<dense.observedCells<<" v="<<dense.velocity[4].x<<":"<<dense.velocity[4].y<<'\n';
        require(dense.denseFlow&&dense.densePixels==w*h&&dense.features==1296&&dense.observations.size()==1296,"dense field covers analysis image with bounded audit observations");
        for(auto c:dense.cells)require(c.anchors==4&&c.samples==16,"all 81 cells retain four anchors and 16 uniform audits");
        require(dense.confidence>=.65f&&dense.observedCells>=60,"large displacement retains broad measured coverage");
        require(std::abs(dense.velocity[4].x-shift*40)<40&&std::abs(dense.velocity[4].y-dy*40)<40,"dense flow preserves source pixel units and direction");
    }
    for(auto [shift,interval]:{std::pair{16.f,1./60},std::pair{32.f,1./30}}){
        for(int y=0;y<h;++y)for(int x=0;x<w;++x)b[y*w+x]=sample(a,w,h,x-shift,y);
        auto adjacent=mc::estimateDenseFlow(a,b,w,h,interval,10.5,1920,1080,&workspace);
        require(adjacent.regionConfidence[4]>=.65f&&std::abs(adjacent.velocity[4].x-1920)<60,"shorter adjacent pairs retain physical velocity units rather than halving dot speed");
        require(adjacent.globalConfidence==0,"dense runtime does not recover matches, borrow or fit a background fill");
    }
    auto still=mc::estimateDenseFlow(a,a,w,h,.04,11,1920,1080,&workspace);
    require(still.confidence>=.65f,"textured still frames yield measured stillness");
    for(auto v:still.velocity)require(std::hypot(v.x,v.y)<2,"stillness does not fabricate motion");
    std::vector<float> flat(w*h,.5f);auto blank=mc::estimateDenseFlow(flat,flat,w,h,.04,12,1920,1080,&workspace);
    require(blank.tracked==0&&blank.confidence==0&&blank.rejectedTexture==1296,"flat dense output is unknown, not confident zero");
    auto cut=mc::estimateDenseFlow(a,texture(w,h,700),w,h,.04,13,1920,1080,&workspace);
    std::cout<<"cut q="<<cut.confidence<<" tracked="<<cut.tracked<<'\n';
    require(cut.confidence<.65f,"unrelated frames cannot become trustworthy motion");
    // Depth-dependent expansion, blank sky and a screen-fixed foreground.
    // This is an offline estimator regression, not a playable test scene.
    auto road=a;for(int y=0;y<int(h*.38f);++y)for(int x=0;x<w;++x)road[y*w+x]=.5f;
    for(float speed:{1.f,2.f,4.f}){
        for(int y=0;y<h;++y)for(int x=0;x<w;++x){
            float depth=std::max(0.f,(y-h*.38f)/(h*.62f));
            float dx=(x-w*.5f)*.035f*depth*speed,dy=10*depth*depth*speed;
            bool car=x>w*.38f&&x<w*.62f&&y>h*.64f;
            b[y*w+x]=car||y<h*.38f?road[y*w+x]:sample(road,w,h,x-dx,y-dy);
        }
        auto flow=mc::estimateDenseFlow(road,b,w,h,.05,20,1920,1080,&workspace);
        std::cout<<"road speed="<<speed<<" q="<<flow.confidence<<" cells="<<flow.observedCells<<" left="<<flow.velocity[6].x<<":"<<flow.velocity[6].y<<":"<<flow.regionConfidence[6]<<" right="<<flow.velocity[8].x<<":"<<flow.velocity[8].y<<":"<<flow.regionConfidence[8]<<'\n';
        require(flow.regionConfidence[6]>=.65f&&flow.regionConfidence[8]>=.65f,"fast expanding road retains measured lower corners");
        require(flow.velocity[6].x<-50*speed&&flow.velocity[8].x>50*speed&&flow.velocity[6].y>30*speed,"opposing expansion never cancels into zero motion");
        mc::DynamicDriver driver;mc::InputState input;input.move={0,1};std::array<mc::Vec2,9> output{};
        for(int frame=0;frame<60;++frame){double now=20+frame*.016;input.timestamp=now;flow.timestamp=now-.03;flow.completedTimestamp=now-.01;output=driver.update(flow,input,now,.016,true,true);}
        mc::RegionalDotField regional;mc::EdgeDotField edge;mc::VisualSettings settings;settings.style=1;
        for(int frame=0;frame<30;++frame){regional.update(output,1920,1080,122,.016,true,false,true,3,true);edge.update(output,1920,1080,settings,.016);}
        require(regional.phaseMax()>5&&edge.phaseMax()>5,"dense road estimates drive both production dot fields");
    }
    // Fixed foreground remains measured zero locally; background is separately
    // available for the existing fusion policy rather than editing raw data.
    for(int y=0;y<h;++y)for(int x=0;x<w;++x)b[y*w+x]=(x>w*.45f&&y>h*.5f)?a[y*w+x]:sample(a,w,h,x-7,y+3);
    auto occlusion=mc::estimateDenseFlow(a,b,w,h,.04,30,1920,1080,&workspace);
    require(std::hypot(occlusion.velocity[8].x,occlusion.velocity[8].y)<10&&occlusion.movingBackgroundConfidence==0,"stationary foreground is retained without an extrapolated background fill");
    b=a;
    for(int y=h/4;y<h/2;++y)for(int x=w*3/4;x<w*7/8;++x)b[y*w+x]=sample(a,w,h,x-6,y);
    auto object=mc::estimateDenseFlow(a,b,w,h,.04,30.1,1920,1080,&workspace);
    require(mc::spatialEvidence(object).stationary&&object.movingBackgroundConfidence==0,"one animated object cannot impersonate broad background motion");
    for(int y=0;y<h;++y)for(int x=0;x<w;++x)b[y*w+x]=sample(a,w,h,x+18,y-7);
    auto reverse=mc::estimateDenseFlow(a,b,w,h,.1,30.2,1920,1080,&workspace);
    require(reverse.confidence>=.65f&&std::abs(reverse.velocity[4].x+360)<25&&std::abs(reverse.velocity[4].y-140)<25,"opposite motion and actual longer frame interval preserve units");
    auto invalid=a;invalid[5]=std::numeric_limits<float>::quiet_NaN();
    require(mc::estimateDenseFlow(invalid,b,w,h,.04,31,1920,1080,&workspace).issue==mc::MotionIssue::InvalidInput,"nonfinite input rejected");
    require(mc::estimateDenseFlow(a,b,w,h,.4,31,1920,1080,&workspace).issue==mc::MotionIssue::InvalidInput,"long gaps rejected");
    workspace.reset();auto reset=mc::estimateDenseFlow(a,a,w,h,.04,32,1920,1080,&workspace);
    require(reset.confidence>=.65f&&std::hypot(reset.velocity[4].x,reset.velocity[4].y)<2,"reset never carries motion into a stationary pair");
    auto small=texture(320,180,9);auto resized=mc::estimateDenseFlow(small,small,320,180,.04,33,1920,1080,&workspace);
    require(resized.confidence>=.65f&&std::hypot(resized.velocity[4].x,resized.velocity[4].y)<2,"workspace safely adapts to resized analysis images");
    std::sort(denseTimes.begin(),denseTimes.end());std::sort(sparseTimes.begin(),sparseTimes.end());
    std::cout<<"offline paired timing median dense_ms="<<(denseTimes[1]+denseTimes[2])*.5<<" sparse_ms="<<(sparseTimes[1]+sparseTimes[2])*.5<<" (four displacements; not game latency)\nPASS: dense flow, fast motion, road expansion, stillness, ambiguity, occlusion and reset\n";
}
