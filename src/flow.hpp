#pragma once
#include "core.hpp"
#include <vector>
#include <limits>
namespace mc {
namespace flow_detail {
struct Image {
    int w{},h{}; const std::vector<float>& pixels;
    bool contains(int x,int y,int r) const {return x-r>=0 && y-r>=0 && x+r<w && y+r<h;}
    float at(int x,int y) const {return pixels[size_t(y)*w+x];}
};
struct Match {int dx{},dy{};float error=1,quality=0;};
inline float mean(const Image& im,int x,int y,int r) {
    float sum=0;for(int yy=-r;yy<=r;++yy) for(int xx=-r;xx<=r;++xx) sum+=im.at(x+xx,y+yy);
    return sum/float((2*r+1)*(2*r+1));
}
inline Match search(const Image& a,const Image& b,int x,int y,int gx,int gy,int range,int r) {
    Match result;if(!a.contains(x,y,r)) return result;
    float av=mean(a,x,y,r),variance=0;int count=(2*r+1)*(2*r+1);
    for(int yy=-r;yy<=r;++yy) for(int xx=-r;xx<=r;++xx) {float v=a.at(x+xx,y+yy)-av;variance+=v*v;}
    if(variance/count<.0007f) return result;
    float best=std::numeric_limits<float>::max(),second=best;
    for(int dy=gy-range;dy<=gy+range;++dy) for(int dx=gx-range;dx<=gx+range;++dx) {
        if(!b.contains(x+dx,y+dy,r)) continue;
        float bv=mean(b,x+dx,y+dy,r),cost=0;
        for(int yy=-r;yy<=r;++yy) for(int xx=-r;xx<=r;++xx)
            cost+=std::abs((a.at(x+xx,y+yy)-av)-(b.at(x+dx+xx,y+dy+yy)-bv));
        cost/=float(count);
        if(cost<best) {second=best;best=cost;result.dx=dx;result.dy=dy;} else second=std::min(second,cost);
    }
    result.error=best;
    if(best>.065f || !std::isfinite(second) || second-best<.001f || best>second*.88f) return result;
    if(std::abs(result.dx-gx)==range || std::abs(result.dy-gy)==range) return result;
    result.quality=std::clamp((second-best)/std::max(second,.001f),0.f,1.f);return result;
}
inline std::vector<float> half(const Image& image) {
    std::vector<float> out(size_t(image.w/2)*(image.h/2));
    for(int y=0;y<image.h/2;++y) for(int x=0;x<image.w/2;++x)
        out[size_t(y)*(image.w/2)+x]=(image.at(x*2,y*2)+image.at(x*2+1,y*2)+image.at(x*2,y*2+1)+image.at(x*2+1,y*2+1))*.25f;
    return out;
}
inline Match pyramidal(const Image& a,const Image& b,const Image& smallA,const Image& smallB,int x,int y) {
    auto coarse=search(smallA,smallB,x/2,y/2,0,0,9,3);
    auto fine=search(a,b,x,y,coarse.quality>0?coarse.dx*2:0,coarse.quality>0?coarse.dy*2:0,coarse.quality>0?3:6,5);
    if(fine.quality<=0) return fine;
    auto reverse=search(b,a,x+fine.dx,y+fine.dy,-fine.dx,-fine.dy,3,5);
    if(reverse.quality<=0 || std::abs(fine.dx+reverse.dx)>1 || std::abs(fine.dy+reverse.dy)>1) fine.quality=0;
    return fine;
}
inline float median(std::vector<float> values) {
    auto middle=values.begin()+values.size()/2;std::nth_element(values.begin(),middle,values.end());return *middle;
}
}
// Two-scale sparse optical motion, not camera pose or dense optical flow.
// Each region requires at least two agreeing tracks. Dominant translation
// consensus rejects independently moving regions; it cannot identify semantics.
inline MotionSample estimateFlow(const std::vector<float>& a,const std::vector<float>& b,
    int w,int h,double dt,double timestamp,int sourceW,int sourceH) {
    MotionSample s;s.timestamp=timestamp;s.regionConfidence.fill(0);
    if(w<120 || h<84 || a.size()!=size_t(w)*h || b.size()!=a.size() || !std::isfinite(dt)
        || !std::isfinite(timestamp) || dt<.015 || dt>.15 || sourceW<=0 || sourceH<=0) return s;
    for(size_t i=0;i<a.size();++i) if(!std::isfinite(a[i]) || !std::isfinite(b[i])) return s;
    using namespace flow_detail;
    Image fullA{w,h,a},fullB{w,h,b};auto aa=half(fullA),bb=half(fullB);
    Image smallA{w/2,h/2,aa},smallB{w/2,h/2,bb};
    std::array<Vec2,9> region{};std::vector<float> globalX,globalY;
    for(int r=0;r<9;++r) {
        int cx=(r%3*2+1)*w/6,cy=(r/3*2+1)*h/6;std::vector<float> xs,ys;
        for(int offset:{-8,0,8}) {
            auto match=pyramidal(fullA,fullB,smallA,smallB,cx+offset,cy);
            if(match.quality>0) {xs.push_back(float(match.dx));ys.push_back(float(match.dy));}
        }
        if(xs.size()<2) continue;
        float mx=median(xs),my=median(ys);int support=0;
        for(size_t i=0;i<xs.size();++i) if(std::abs(xs[i]-mx)<=1.5f && std::abs(ys[i]-my)<=1.5f) ++support;
        if(support<2) continue;
        region[r]={mx,my};s.regionConfidence[r]=support/3.f;globalX.push_back(mx);globalY.push_back(my);
    }
    if(globalX.size()<5) {s.regionConfidence.fill(0);return s;}
    float mx=median(globalX),my=median(globalY);std::vector<float> distances;
    for(size_t i=0;i<globalX.size();++i) distances.push_back(std::hypot(globalX[i]-mx,globalY[i]-my));
    float limit=std::max(3.f,median(distances)*2.5f);int valid=0;
    for(int r=0;r<9;++r) {
        if(s.regionConfidence[r]<=0) continue;
        if(std::hypot(region[r].x-mx,region[r].y-my)>limit) {s.regionConfidence[r]=0;continue;}
        s.velocity[r]={float(region[r].x*sourceW/w/dt),float(region[r].y*sourceH/h/dt)};++valid;
    }
    s.confidence=valid/9.f;return s;
}
}
