#include "chroma_motion.hpp"
#include <chrono>
#include <limits>

namespace mc::detail {
namespace {
constexpr int patchRadius=4,patchSamples=25,searchRadius=24,attemptBudget=54;

struct Image {
    std::span<const uint16_t> pixels;
    int width,height;
    bool containsPatch(int x,int y) const {
        return x>=patchRadius&&y>=patchRadius&&x<width-patchRadius&&y<height-patchRadius;
    }
    uint16_t at(int x,int y) const {return pixels[size_t(y)*width+x];}
};

int cb(uint16_t pixel){return pixel&255;}
int cr(uint16_t pixel){return pixel>>8;}

float texture(const Image& image,int x,int y) {
    if(!image.containsPatch(x,y))return 0;
    int xx=0,xy=0,yy=0;
    for(int dy=-4;dy<4;dy+=2)for(int dx=-4;dx<4;dx+=2){
        const auto a=image.at(x+dx,y+dy),right=image.at(x+dx+2,y+dy),bottom=image.at(x+dx,y+dy+2);
        const int bx=cb(right)-cb(a),by=cb(bottom)-cb(a),rx=cr(right)-cr(a),ry=cr(bottom)-cr(a);
        xx+=bx*bx+rx*rx;xy+=bx*by+rx*ry;yy+=by*by+ry*ry;
    }
    const float trace=float(xx+yy);
    return trace>0?std::max(0.f,(float(xx)*yy-float(xy)*xy)/trace):0;
}

struct Match {int dx{},dy{},cost=std::numeric_limits<int>::max();bool valid{},strong{};};

Match match(const Image& from,const Image& to,int x,int y,int hintX,int hintY) {
    std::array<uint16_t,patchSamples> pattern{};
    size_t n=0;
    for(int dy=-4;dy<=4;dy+=2)for(int dx=-4;dx<=4;dx+=2)pattern[n++]=from.at(x+dx,y+dy);

    // 169 coarse, up to 50 around optical-flow and color seeds, and 49 fine.
    std::array<Match,269> candidates;
    size_t count=0;
    Match best;
    const auto score=[&](int dx,int dy){
        if(!to.containsPatch(x+dx,y+dy))return;
        int cost=0;
        size_t i=0;
        for(int py=-4;py<=4;py+=2)for(int px=-4;px<=4;px+=2){
            const auto a=pattern[i++],b=to.at(x+dx+px,y+dy+py);
            cost+=std::abs(cb(a)-cb(b))+std::abs(cr(a)-cr(b));
        }
        const Match candidate{dx,dy,cost,true};
        candidates[count++]=candidate;
        if(cost<best.cost)best=candidate;
    };
    for(int dy=-searchRadius;dy<=searchRadius;dy+=4)
        for(int dx=-searchRadius;dx<=searchRadius;dx+=4)score(dx,dy);
    if(hintX||hintY){
        for(int dy=-8;dy<=8;dy+=4)for(int dx=-8;dx<=8;dx+=4)score(hintX+dx,hintY+dy);
        for(int dy=-2;dy<=2;++dy)for(int dx=-2;dx<=2;++dx)score(hintX+dx,hintY+dy);
    }
    if(!best.valid)return {};
    const int centerX=best.dx,centerY=best.dy;
    for(int dy=-3;dy<=3;++dy)for(int dx=-3;dx<=3;++dx)score(centerX+dx,centerY+dy);

    int runner=std::numeric_limits<int>::max();
    for(size_t i=0;i<count;++i){
        const auto& candidate=candidates[i];
        const int dx=candidate.dx-best.dx,dy=candidate.dy-best.dy;
        if(dx*dx+dy*dy>=9)runner=std::min(runner,candidate.cost);
    }
    if(best.cost>10*patchSamples||runner==std::numeric_limits<int>::max()||runner-best.cost<24||runner<best.cost*1.10f)return {};
    best.strong=best.cost<=8*patchSamples&&runner-best.cost>=34&&runner>=best.cost*1.16f;
    return best;
}

bool agrees(Vec2 a,Vec2 b) {
    return std::hypot(a.x-b.x,a.y-b.y)<=1.5f+.08f*std::hypot(a.x,a.y);
}
bool directionAgrees(Vec2 a,Vec2 b) {
    const float speed=std::hypot(a.x,a.y),reference=std::hypot(b.x,b.y);
    return speed>=35&&reference>=25&&a.x*b.x+a.y*b.y>.75f*speed*reference;
}
}

void recoverChromaMotion(MotionSample& sample,int width,int height,float roundTripLimit,Vec2 pixelsPerSecond,
                         std::span<const uint16_t> previous,std::span<const uint16_t> current,const ChromaInputHint& input) {
    if(previous.size()!=size_t(width)*height||current.size()!=previous.size()||sample.observations.size()!=81*16)return;
    sample.chromaActive=true;
    using Clock=std::chrono::steady_clock;
    const auto deadline=Clock::now()+std::chrono::milliseconds(3);
    const Image from{previous,width,height},to{current,width,height};
    const auto zone=[](int cell){return cell/27*3+cell%9/3;};
    const auto scaled=[&](Vec2 v){return Vec2{v.x*pixelsPerSecond.x,v.y*pixelsPerSecond.y};};
    const auto quiet=[&](Vec2 v){const auto speed=scaled(v);return std::hypot(speed.x,speed.y)<25;};
    std::array<int,81> quietTracks{},movingTracks{};
    std::array<Vec2,81> grayDirections{};
    std::array<bool,9> quietZones{},movingZones{};
    int quietCells=0,measuredCells=0,broadMovingCells=0;
    for(int i=0;i<81;++i){
        for(int j=i*16;j<(i+1)*16;++j){
            const auto& observation=sample.observations[j];
            if(!observation.originalValid)continue;
            if(quiet(observation.originalDisplacement)){++quietTracks[i];continue;}
            const auto speed=scaled(observation.originalDisplacement);
            grayDirections[i].x+=speed.x;grayDirections[i].y+=speed.y;++movingTracks[i];
        }
        const int tracked=sample.cells[i].tracked;
        measuredCells+=tracked>=4;
        if(quietTracks[i]>=4&&quietTracks[i]*5>=tracked*4){++quietCells;quietZones[zone(i)]=true;}
        if(movingTracks[i]>=3){
            auto& mean=grayDirections[i];mean={mean.x/movingTracks[i],mean.y/movingTracks[i]};
            if(std::hypot(mean.x,mean.y)>=35){++broadMovingCells;movingZones[zone(i)]=true;}
        }
    }
    // Strong, spatially distributed stationary evidence vetoes color assist.
    if(quietCells>=32&&quietCells*5>=measuredCells*4&&std::count(quietZones.begin(),quietZones.end(),true)>=6&&
       !(broadMovingCells>=8&&std::count(movingZones.begin(),movingZones.end(),true)>=3))return;

    struct Site {size_t index{};float strength{};};
    struct Cell {int index{};float strength{};std::array<Site,16> sites{};size_t count{};bool mostlyQuiet{};};
    std::array<Cell,81> ready{};
    size_t cellCount=0;
    for(int i=0;i<81;++i){
        const int tracked=sample.cells[i].tracked;
        const bool mostlyQuiet=quietTracks[i]>=2&&quietTracks[i]*5>=tracked*3;
        if(tracked>=4&&!mostlyQuiet)continue;
        Cell cell;cell.index=i;cell.mostlyQuiet=mostlyQuiet;
        for(size_t j=size_t(i)*16;j<size_t(i+1)*16;++j){
            const auto& observation=sample.observations[j];
            if(observation.valid&&(!mostlyQuiet||!quiet(observation.originalDisplacement)))continue;
            const float strength=texture(from,int(observation.position.x),int(observation.position.y));
            if(strength>=28)cell.sites[cell.count++]={j,strength};
        }
        if(cell.count+sample.cells[i].tracked<3||cell.count<2)continue;
        std::stable_sort(cell.sites.begin(),cell.sites.begin()+cell.count,[](const auto& a,const auto& b){return a.strength>b.strength;});
        cell.strength=cell.sites[1].strength;
        ready[cellCount++]=cell;
    }
    std::stable_sort(ready.begin(),ready.begin()+cellCount,[](const auto& a,const auto& b){return a.strength>b.strength;});
    // Interleave the strongest sites from each screen region, so a bright HUD
    // or colorful sky cannot monopolize the search budget.
    std::array<int,9> positions{};
    std::array<int,81> order{};
    std::array<int,81> round{};
    for(size_t i=0;i<cellCount;++i){order[i]=int(i);round[i]=positions[zone(ready[i].index)]++;}
    std::stable_sort(order.begin(),order.begin()+cellCount,[&](int a,int b){return round[a]<round[b];});
    const bool recentInput=input.timestamp>0&&std::abs(input.timestamp-sample.timestamp)<.075;

    for(size_t i=0;i<cellCount;++i){
        const auto& cell=ready[order[i]];
        struct Candidate {size_t index{};Vec2 motion{};float cost{},roundTrip{};bool strong{};};
        std::array<Candidate,6> recovered{};
        size_t count=0;
        for(size_t j=0;j<std::min<size_t>(cell.count,recovered.size());++j){
            if(sample.chromaAttempts>=attemptBudget||Clock::now()>=deadline){sample.chromaBudgetLimited=true;break;}
            const auto index=cell.sites[j].index;
            const auto& observation=sample.observations[index];
            const int x=int(observation.position.x),y=int(observation.position.y);
            const auto hint=observation.displacement;
            const int hx=int(std::round(std::clamp(hint.x,-72.f,72.f))),hy=int(std::round(std::clamp(hint.y,-72.f,72.f)));
            ++sample.chromaAttempts;
            const auto forward=match(from,to,x,y,hx,hy);
            if(!forward.valid)continue;
            if(texture(to,x+forward.dx,y+forward.dy)<28)continue;
            const auto backward=match(to,from,x+forward.dx,y+forward.dy,-forward.dx,-forward.dy);
            if(!backward.valid)continue;
            const float roundTrip=std::hypot(float(forward.dx+backward.dx),float(forward.dy+backward.dy));
            if(roundTrip>std::min(2.f,roundTripLimit))continue;
            const Vec2 motion{float(forward.dx),float(forward.dy)};
            if(cell.mostlyQuiet&&(quiet(motion)||std::hypot(motion.x,motion.y)<1.4f))continue;
            recovered[count++]={index,motion,float(forward.cost)/patchSamples,roundTrip,forward.strong&&backward.strong};
        }

        // A single chroma match cannot add a vector. Confirm nearby motion
        // with multiple color patches, optionally supported by existing luma.
        size_t best=count;
        int bestSupport=0;bool bestCorroborated=false;
        for(size_t j=0;j<count;++j){
            const auto motion=recovered[j].motion;
            int colorSupport=0,strongColor=0,graySupport=0,neighbors=0;
            for(size_t k=0;k<count;++k)if(agrees(motion,recovered[k].motion)){++colorSupport;strongColor+=recovered[k].strong;}
            for(int k=cell.index*16;k<(cell.index+1)*16;++k){
                const auto& o=sample.observations[k];
                graySupport+=o.originalValid&&(!cell.mostlyQuiet||!quiet(o.originalDisplacement))&&agrees(motion,o.originalDisplacement);
            }
            const auto speed=scaled(motion);
            const int x=cell.index%9,y=cell.index/9;
            for(int yy=std::max(0,y-1);yy<=std::min(8,y+1);++yy)
                for(int xx=std::max(0,x-1);xx<=std::min(8,x+1);++xx){
                    const int index=yy*9+xx;
                    if(index!=cell.index&&movingTracks[index]>=3&&directionAgrees(speed,grayDirections[index]))++neighbors;
                }
            const int support=colorSupport+graySupport;
            const bool strong=strongColor>=2&&strongColor+graySupport>=3;
            const bool corroborated=colorSupport>=3&&
                (graySupport>=2||neighbors>=2||(recentInput&&directionAgrees(speed,input.directions[zone(cell.index)])));
            if((strong||corroborated)&&support>bestSupport){best=j;bestSupport=support;bestCorroborated=!strong;}
        }
        if(best<count){
            ++sample.chromaCells;
            sample.chromaCorroboratedCells+=bestCorroborated;
            sample.chromaQuietCells+=cell.mostlyQuiet;
            sample.cells[cell.index].chromaReplacesQuiet=cell.mostlyQuiet;
            for(size_t j=0;j<count;++j){
                const auto& candidate=recovered[j];
                if((!bestCorroborated&&!candidate.strong)||!agrees(recovered[best].motion,candidate.motion))continue;
                auto& observation=sample.observations[candidate.index];
                if(!observation.valid){++sample.tracked;++sample.cells[cell.index].tracked;}
                observation.displacement=candidate.motion;observation.error=candidate.cost;
                observation.roundTripError=candidate.roundTrip;observation.valid=true;observation.chromaMatch=true;
                ++sample.chromaAccepted;
            }
        }
        if(sample.chromaBudgetLimited)break;
    }
}
}
