#pragma once
#include "core.hpp"
namespace mc {
struct SpatialEvidence {
    bool spatial=false,broadMotion=false,stationary=false;
    int measuredCells=0,quietCells=0,movingCells=0;
    int directCells=0,quietRegions=0;bool motionVeto=false;
    std::array<bool,81> connected{};
    std::array<int,9> regionalMotion{};
};
inline SpatialEvidence spatialEvidence(const MotionSample& flow) {
    SpatialEvidence out;out.spatial=flow.analysisWidth>0&&flow.analysisHeight>0;
    std::array<bool,9> occupied{};std::array<bool,3> columns{},rows{};
    if(out.spatial){
        for(int i=0;i<81;++i){const auto& c=flow.cells[i];
            if(c.confidence<.65f||!std::isfinite(c.confidence)||!std::isfinite(c.velocity.x)||!std::isfinite(c.velocity.y))continue;
            ++out.measuredCells;float speed=std::hypot(c.velocity.x,c.velocity.y);
            bool direct=flow.directEvidence?c.directConfidence>=.65f:true;
            auto dv=flow.directEvidence?c.directVelocity:c.velocity;
            if(direct){++out.directCells;if(std::hypot(dv.x,dv.y)<12)++out.quietCells;}
            if(speed<=40)continue;
            int neighbours=0;
            for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){
                int x=i%9+dx,y=i/9+dy;if(x<0||y<0||x>=9||y>=9)continue;
                const auto& n=flow.cells[y*9+x];float ns=std::hypot(n.velocity.x,n.velocity.y);
                if(n.confidence>=.65f&&std::isfinite(ns)&&ns>40
                    &&(c.velocity.x*n.velocity.x+c.velocity.y*n.velocity.y)>.25f*speed*ns
                    &&std::min(speed,ns)>.10f*std::max(speed,ns))++neighbours;
            }
            if(neighbours>=3){out.connected[i]=true;++out.movingCells;int r=i/9/3*3+i%9/3;
                ++out.regionalMotion[r];occupied[r]=true;columns[r%3]=true;rows[r/3]=true;}
        }
        int regions=int(std::count(occupied.begin(),occupied.end(),true));
        int cols=int(std::count(columns.begin(),columns.end(),true)),rs=int(std::count(rows.begin(),rows.end(),true));
        out.broadMotion=out.movingCells>=8&&regions>=3&&(cols==3||rs==3||(cols>=2&&rs>=2));
        // Static evidence needs measured texture across the image. A large
        // coherent road field vetoes static foreground/sky majority votes.
        std::array<int,9> quietByRegion{};
        for(int i=0;i<81;++i){const auto& c=flow.cells[i];float q=flow.directEvidence?c.directConfidence:c.confidence;auto v=flow.directEvidence?c.directVelocity:c.velocity;
            if(q>=.65f&&std::isfinite(v.x)&&std::isfinite(v.y)&&std::hypot(v.x,v.y)<12)++quietByRegion[i/9/3*3+i%9/3];}
        out.quietRegions=int(std::count_if(quietByRegion.begin(),quietByRegion.end(),[](int n){return n>=2;}));
        out.motionVeto=out.broadMotion||(out.movingCells>=6&&regions>=2&&(cols>=2||rs>=2));
        std::array<bool,3> quietRows{},quietColumns{};for(int r=0;r<9;++r)if(quietByRegion[r]>=2){quietRows[r/3]=true;quietColumns[r%3]=true;}
        out.stationary=std::count(quietRows.begin(),quietRows.end(),true)==3&&std::count(quietColumns.begin(),quietColumns.end(),true)==3&&!out.motionVeto&&out.directCells>=32&&out.quietCells>=32
            &&out.quietCells>=out.directCells*.8f&&out.quietRegions>=6;
    }else{
        int valid=0,quiet=0;
        for(int r=0;r<9;++r){auto v=flow.velocity[r];if(!std::isfinite(flow.confidence)||!std::isfinite(flow.regionConfidence[r])||flow.confidence<.65f||flow.regionConfidence[r]<.65f||!std::isfinite(v.x)||!std::isfinite(v.y))continue;
            ++valid;float speed=std::hypot(v.x,v.y);if(speed<12)++quiet;
            if(speed>40){out.regionalMotion[r]=1;occupied[r]=true;columns[r%3]=true;rows[r/3]=true;}}
        out.broadMotion=std::count(occupied.begin(),occupied.end(),true)>=3
            &&(std::count(columns.begin(),columns.end(),true)==3||std::count(rows.begin(),rows.end(),true)==3);
        out.stationary=!out.broadMotion&&valid>=6&&quiet>=valid-1&&flow.meanDelta<.012f;
    }
    return out;
}
// Count actual model inliers near a display region, not its whole-image score.
inline int nearbyModelSupport(const std::array<int,9>& support,int region){
    int count=0;for(int r=0;r<9;++r)if(std::abs(r%3-region%3)<=1&&std::abs(r/3-region/3)<=1)count+=support[r];
    return count;
}
inline bool compatibleMotion(Vec2 a,Vec2 b){
    float as=std::hypot(a.x,a.y),bs=std::hypot(b.x,b.y);
    if(as<12&&bs<12)return true;
    if(std::min(as,bs)<std::max(12.f,std::max(as,bs)*.35f))return false;
    return a.x*b.x+a.y*b.y>.5f*as*bs;
}
}
