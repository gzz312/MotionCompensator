#include "tracking.hpp"
#include "dynamic.hpp"
#include "dots.hpp"
#include <random>
#include <iostream>
#include <chrono>
#include <algorithm>
#include <cstdlib>
void require(bool ok,const char* why){if(!ok){std::cerr<<"FAIL: "<<why<<'\n';std::exit(1);}}
std::vector<float> texture(int w,int h,int seed){std::mt19937 rng(seed);std::vector<float> noise(w*h),image(w*h);for(auto& x:noise)x=float(rng()%256)/255;for(int y=1;y<h-1;++y)for(int x=1;x<w-1;++x){float sum=0;for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx)sum+=noise[(y+dy)*w+x+dx];image[y*w+x]=sum/9;}return image;}
float sample(const std::vector<float>& a,int w,int h,float x,float y){int xx=int(std::floor(x)),yy=int(std::floor(y));if(xx<0||xx+1>=w||yy<0||yy+1>=h)return 0;float u=x-xx,v=y-yy;return (1-v)*((1-u)*a[yy*w+xx]+u*a[yy*w+xx+1])+v*((1-u)*a[(yy+1)*w+xx]+u*a[(yy+1)*w+xx+1]);}
int main(){
    constexpr int w=960,h=540;auto a=texture(w,h,47);std::vector<float> b(w*h);
    for(int y=0;y<h;++y)for(int x=0;x<w;++x)b[y*w+x]=sample(a,w,h,x-5.4f,y+2.3f);
    auto translated=mc::estimateTrackedFlow(a,b,w,h,.04,10,1920,1080);
    require(translated.analysisWidth==960 && translated.analysisHeight==540 && translated.observations.size()==size_t(translated.features),"full sampling observations are retained at actual analysis resolution");
    std::cout<<"translation: features="<<translated.features<<", tracked="<<translated.tracked<<", cells="<<translated.observedCells<<", confidence="<<translated.confidence<<"\n";
    require(translated.features>=324 && translated.features<=972 && translated.observedCells>=70,"dense texture supports uniform 81-cell coverage");
    for(auto c:translated.cells)require(c.anchors==4 && c.samples>=4 && c.samples<=12,"every cell has four anchors plus up to eight texture points");
    require(translated.confidence>=.65f,"960x540 translation remains trustworthy");
    for(int r=0;r<9;++r)require(std::abs(translated.velocity[r].x-270)<18 && std::abs(translated.velocity[r].y+115)<18,"regional vectors retain source-pixel units and direction");
    std::vector<float> fastImage(w*h);for(int y=0;y<h;++y)for(int x=0;x<w;++x)fastImage[y*w+x]=sample(a,w,h,x-24.f,y+11.f);
    auto fast=mc::estimateTrackedFlow(a,fastImage,w,h,.04,10.5,1920,1080);
    require(fast.confidence>=.65f && std::abs(fast.velocity[4].x-1200)<40 && std::abs(fast.velocity[4].y+550)<40,"large inter-frame displacement survives the enlarged multiscale tracking path");
    // Independent regional motion: a single global affine model cannot explain
    // this entire field, but each local observation must survive.
    for(int y=0;y<h;++y)for(int x=0;x<w;++x){float dx=(1-x/(w/3))*6.f,dy=(1-y/(h/3))*4.f;b[y*w+x]=sample(a,w,h,x-dx,y-dy);}
    auto spatial=mc::estimateTrackedFlow(a,b,w,h,.04,11,1920,1080);
    std::cout<<"spatial confidence="<<spatial.confidence<<", cells="<<spatial.observedCells<<"\n";for(int r=0;r<9;++r)std::cout<<r<<":"<<spatial.velocity[r].x<<":"<<spatial.velocity[r].y<<":"<<spatial.regionConfidence[r]<<" ";std::cout<<"\n";
    require(spatial.confidence>=.65f && spatial.velocity[0].x>150 && spatial.velocity[0].y>100 && spatial.velocity[8].x<-150 && spatial.velocity[8].y<-100,"opposing local directions survive without being collapsed to a global average");
    require(spatial.cells[10].velocity.x>150 && spatial.cells[70].velocity.x<-150,"raw local data remain separate from aggregate output");
    auto still=mc::estimateTrackedFlow(a,a,w,h,.04,12,1920,1080);require(still.confidence>=.65f,"reliable zero flow remains a valid observation");
    for(auto v:still.velocity)require(std::hypot(v.x,v.y)<2,"stationary images do not create rolling motion");
    auto masked=a;for(int y=0;y<h/3;++y)for(int x=0;x<w/3;++x)masked[y*w+x]=.5f;
    auto hole=mc::estimateTrackedFlow(masked,masked,w,h,.04,13,1920,1080);
    require(hole.cells[10].anchors==4 && hole.cells[10].tracked==0 && hole.cells[10].confidence==0,"anchors in a featureless cell never become fabricated evidence");
    require(hole.confidence>=.65f && hole.regionConfidence[8]>=.65f,"one textureless region cannot invalidate reliable distant regions");
    std::vector<float> flat(w*h,.5f);auto blank=mc::estimateTrackedFlow(flat,flat,w,h,.04,14,1920,1080);
    require(blank.features==324 && blank.tracked==0 && blank.confidence==0,"uniform image preserves sampling quota but reports no measured motion");
    auto cut=texture(w,h,700);auto jumped=mc::estimateTrackedFlow(a,cut,w,h,.04,15,1920,1080);require(jumped.confidence<.65f,"unrelated frames are not accepted as reliable camera motion");
    auto occluded=b;
    for(int y=0;y<h;++y)for(int x=0;x<w;++x)occluded[y*w+x]=(x>w*.45f && y>h*.5f)?a[y*w+x]:sample(a,w,h,x-6.5f,y+3.5f);
    auto weapon=mc::estimateTrackedFlow(a,occluded,w,h,.04,16,1920,1080);
    std::cout<<"weapon test: local_R9="<<weapon.velocity[8].x<<","<<weapon.velocity[8].y<<", background_R9="<<weapon.movingBackgroundVelocity[8].x<<","<<weapon.movingBackgroundVelocity[8].y<<", confidence="<<weapon.movingBackgroundConfidence<<", support="<<weapon.movingBackgroundSupportCells<<"\n";
    require(std::hypot(weapon.velocity[8].x,weapon.velocity[8].y)<10,"stationary weapon-like foreground reproduces the original local zero-flow condition");
    require(weapon.movingBackgroundConfidence>=.65f && weapon.movingBackgroundVelocity[8].x>250 && weapon.movingBackgroundVelocity[8].y<-120,"broad moving background predicts motion behind stationary foreground without overwriting local observations");
    auto movingObject=a;for(int y=h/4;y<h/2;++y)for(int x=w*3/4;x<w*7/8;++x)movingObject[y*w+x]=sample(a,w,h,x-6,y);
    auto object=mc::estimateTrackedFlow(a,movingObject,w,h,.04,17,1920,1080);
    require(object.globalConfidence>=.65f && std::hypot(object.globalVelocity[4].x,object.globalVelocity[4].y)<10 && object.movingBackgroundConfidence<.65f,"a localized moving object cannot impersonate broad background motion");
    // Deterministic frame regression: featureless sky, depth-dependent road
    // expansion, and a fixed central foreground. Not a game/comfort test.
    auto road=a;std::vector<float> roadNext(w*h);
    for(int y=0;y<h;++y)for(int x=0;x<w;++x)if(y<h*.48f)road[y*w+x]=.5f;
    for(int y=0;y<h;++y)for(int x=0;x<w;++x){
        float depth=std::max(0.f,(y-h*.48f)/(h*.52f));
        float dx=(x-w*.5f)*.06f*depth,dy=14.f*depth*depth;
        bool car=x>w*.37f&&x<w*.63f&&y>h*.62f;
        roadNext[y*w+x]=car||y<h*.48f?road[y*w+x]:sample(road,w,h,x-dx,y-dy);
    }
    auto roadFlow=mc::estimateTrackedFlow(road,roadNext,w,h,.04,18,1920,1080);
    mc::DynamicDriver roadDriver;mc::InputState accelerator;accelerator.move={0,1};
    std::array<mc::Vec2,9> roadOutput{};
    for(int i=0;i<60;++i){double now=18+i*.016;accelerator.timestamp=now;roadFlow.timestamp=now-.03;roadFlow.completedTimestamp=now-.01;roadOutput=roadDriver.update(roadFlow,accelerator,now,.016,true,true);}
    std::cout<<"road fixture: confidence="<<roadFlow.confidence<<", cells="<<roadFlow.observedCells<<", rejected_stationary="<<roadDriver.stationaryBackgroundRejected()<<", bottom_left="<<roadDriver.observedFlow()[6].x<<":"<<roadDriver.observedFlow()[6].y<<"\n";
    require(roadFlow.regionConfidence[6]>=.65f&&roadFlow.regionConfidence[8]>=.65f,"road fixture retains measured lower corner evidence");
    require(roadDriver.observedFlow()[6].x < -50&&roadDriver.observedFlow()[8].x > 50,"opposite road expansion directions reach the production driver without cancellation");
    mc::RegionalDotField roadRegional;mc::EdgeDotField roadEdge;mc::VisualSettings roadSettings;
    for(int i=0;i<30;++i){roadRegional.update(roadOutput,1920,1080,122,.016,true,false,true,3,true);roadEdge.update(roadOutput,1920,1080,roadSettings,.016);}
    require(roadRegional.phaseMax()>5&&roadEdge.phaseMax()>5,"road frames drive both rolling dot render fields");
    std::vector<std::vector<float>> sequence(9,std::vector<float>(w*h));
    for(int frame=0;frame<9;++frame)for(int y=0;y<h;++y)for(int x=0;x<w;++x)sequence[frame][y*w+x]=sample(a,w,h,x-frame*.9f,y+frame*.5f);
    mc::TrackingWorkspace workspace;std::vector<double> baselineTimes,optimizedTimes;
    for(int frame=1;frame<9;++frame){
        auto start=std::chrono::steady_clock::now();auto baseline=mc::estimateTrackedFlow(sequence[frame-1],sequence[frame],w,h,.04,30+frame*.04,1920,1080);
        auto middle=std::chrono::steady_clock::now();auto optimized=mc::estimateTrackedFlow(sequence[frame-1],sequence[frame],w,h,.04,30+frame*.04,1920,1080,&workspace,4);
        auto end=std::chrono::steady_clock::now();
        require(optimized.pyramidReused==(frame>1),"only identical previous images reuse the worker pyramid");
        require(optimized.features>=324&&optimized.features<=648&&optimized.confidence>=.65f,"adaptive feature budget preserves measured coverage and four anchors per cell");
        for(auto cell:optimized.cells)require(cell.anchors==4&&cell.samples>=4&&cell.samples<=8,"automatic scheduling retains every cell's mandatory sampling positions");
        require(std::abs(optimized.velocity[4].x-45)<12&&std::abs(optimized.velocity[4].y+25)<12,"optimized path retains direction and source-pixel speed");
        if(frame>1){baselineTimes.push_back(std::chrono::duration<double,std::milli>(middle-start).count());optimizedTimes.push_back(std::chrono::duration<double,std::milli>(end-middle).count());}
    }
    auto unrelatedCache=mc::estimateTrackedFlow(a,b,w,h,.04,40,1920,1080,&workspace,4);require(!unrelatedCache.pyramidReused,"cache never substitutes a different previous frame");
    workspace.reset();auto resetCache=mc::estimateTrackedFlow(b,b,w,h,.04,40.04,1920,1080,&workspace,4);require(!resetCache.pyramidReused,"reset discards cached optical flow history");
    auto budgetRoad=mc::estimateTrackedFlow(road,roadNext,w,h,.04,41,1920,1080,&workspace,4);
    require(budgetRoad.regionConfidence[6]>=.65f&&budgetRoad.regionConfidence[8]>=.65f&&budgetRoad.velocity[6].x<-50&&budgetRoad.velocity[8].x>50,"adaptive sampling retains opposing road directions");
    std::sort(baselineTimes.begin(),baselineTimes.end());std::sort(optimizedTimes.begin(),optimizedTimes.end());
    std::cout<<"paired offline benchmark: baseline_median_ms="<<baselineTimes[baselineTimes.size()/2]<<", optimized_median_ms="<<optimizedTimes[optimizedTimes.size()/2]<<", optimized_features="<<resetCache.features<<"\n";
    std::vector<double> times;
    for(int i=0;i<7;++i){auto start=std::chrono::steady_clock::now();auto measured=mc::estimateTrackedFlow(a,b,w,h,.04,20+i,1920,1080);require(measured.features>=324,"benchmark runs full sampling workload");double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();if(i)times.push_back(ms);}
    std::sort(times.begin(),times.end());std::cout<<"960x540 offline CPU estimator: median_ms="<<times[times.size()/2]<<", max_ms="<<times.back()<<", features="<<translated.features<<", tracked="<<translated.tracked<<", observed_cells="<<translated.observedCells<<"\n";
    std::cout<<"PASS: high-resolution local flow, 81-cell quotas, signed parallax, holes, flat scene, cut, source units and retained observations\n";
}
