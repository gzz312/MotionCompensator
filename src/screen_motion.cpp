#include "screen_motion.hpp"

namespace mc {
namespace {
Vec2 cap(Vec2 velocity) {
    return {std::clamp(velocity.x,-3200.f,3200.f),std::clamp(velocity.y,-3200.f,3200.f)};
}
bool fitsBackground(Vec2 measured,Vec2 expected) {
    const float speed=std::hypot(expected.x,expected.y);
    const float tolerance=20.f+.42f*speed;
    const float dx=measured.x-expected.x,dy=measured.y-expected.y;
    return dx*dx+dy*dy<=tolerance*tolerance;
}

float radialDepth(const MotionSample& source,const AffineFlow& model) {
    // Compare derivatives in source pixels, accounting for the aspect ratio.
    if(model.confidence<.70f)return 0;
    const float xRate=model.slopeX.x/source.sourceWidth;
    const float yRate=model.slopeY.y/source.sourceHeight;
    const float rate=(xRate+yRate)*.5f;
    const float magnitude=std::abs(rate);
    const float crossRate=std::hypot(model.slopeY.x/source.sourceHeight,model.slopeX.y/source.sourceWidth);
    if(!std::isfinite(magnitude)||!std::isfinite(crossRate)||xRate*yRate<=0||magnitude<.020f||
       std::abs(xRate-yRate)>.035f+.85f*magnitude||crossRate>.035f+.75f*magnitude)return 0;

    // Demand comes from dispersed observed background cells. Remove camera
    // translation before asking the cells to corroborate the radial component.
    const float direction=std::copysign(1.f,rate);
    const float minRadius=.16f*std::min(source.sourceWidth,source.sourceHeight);
    int left=0,right=0;
    for(int i=0;i<81;++i){
        if(!model.support[i]||cellQuality(source,i)<.65f)continue;
        const auto p=cellPoint(i),observed=cellVelocity(source,i);
        if(!finiteMotion(observed))continue;
        const Vec2 offset{(p.x-.5f)*source.sourceWidth,(p.y-.5f)*source.sourceHeight};
        const float distance=std::hypot(offset.x,offset.y);
        if(distance<minRadius)continue;
        const Vec2 residual{observed.x-model.center.x,observed.y-model.center.y};
        const float radial=direction*(residual.x*offset.x+residual.y*offset.y)/distance;
        if(radial<std::max(3.f,.30f*magnitude*distance)||radial<.55f*std::hypot(residual.x,residual.y))continue;
        if(p.x<.40f)++left;
        else if(p.x>.60f)++right;
    }
    if(left<2||right<2||left+right<6)return 0;
    return direction*std::lerp(.48f,1.f,std::clamp((magnitude-.020f)/.15f,0.f,1.f));
}

bool alignedMotion(Vec2 a,Vec2 b) {
    const float dot=a.x*b.x+a.y*b.y;
    return dot>0&&dot*dot>=.83f*.83f*(a.x*a.x+a.y*a.y)*(b.x*b.x+b.y*b.y);
}

float directMotionWeight(const MotionSample& source,int cell,Vec2 measured,float quality,Vec2 baseline,float baselineQuality) {
    const float localSpeed=std::hypot(measured.x,measured.y);
    const float baseSpeed=std::hypot(baseline.x,baseline.y);
    if(localSpeed<35||localSpeed*quality<=baseSpeed*baselineQuality*1.15f+6.f)return 0;

    const bool aligned=baseline.x*measured.x+baseline.y*measured.y>.73f*baseSpeed*localSpeed;
    const bool weakBase=baseSpeed<.52f*localSpeed;
    if(!aligned&&!weakBase)return 0;
    const int required=aligned?2:3;
    const int x=cell%9,y=cell/9;
    int neighbors=0;
    for(int yy=std::max(0,y-1);yy<=std::min(8,y+1);++yy)
        for(int xx=std::max(0,x-1);xx<=std::min(8,x+1);++xx){
            const int i=yy*9+xx;
            if(i==cell||cellQuality(source,i)<.65f)continue;
            const auto v=cellVelocity(source,i);
            const float speed=std::hypot(v.x,v.y);
            if(speed<std::max(25.f,.32f*localSpeed)||v.x*measured.x+v.y*measured.y<=.70f*speed*localSpeed)continue;
            if(finiteMotion(v)&&++neighbors>=required)return aligned?.85f:.72f;
        }
    return 0;
}
}

void ScreenMotionField::reset() {
    cells_={};compatibleNeighbors_={};previous_={};frame_={};stats_={};camera_={};lastSourceTimestamp_=0;
    background_={};backgroundLook_={};backgroundConfidence_=0;backgroundStamp_=0;
    depthStamp_=0;depthDemand_=0;
    independentRegions_={};independentVotes_={};backgroundSupport_=0;backgroundPriority_=false;
    backgroundScales_={};candidateMovingCells_=candidateDirectionCells_=candidateSpeedMatches_=0;
    neighborFill_={};neighborUsesBackground_=false;
}

void ScreenMotionField::classifyBackground(const MotionSample& source) {
    std::array<bool,81> movingCells{},visited{};
    std::array<Vec2,81> observations{};
    int movingCount=0;
    for(int i=0;i<81;++i){
        observations[i]=cellVelocity(source,i);
        if(cellQuality(source,i)<.65f||!finiteMotion(observations[i]))continue;
        if(std::hypot(observations[i].x,observations[i].y)>=25){movingCells[i]=true;++movingCount;}
    }
    struct Alignment {
        std::array<bool,81> matching{};
        int directions{},speedMatches{};
        bool usable{};
    };
    const auto assess=[&](const AffineFlow& model){
        Alignment result;
        if(model.confidence<.76f)return result;
        std::array<bool,9> zones{};
        int fitted=0,minX=8,maxX=0,minY=8,maxY=0;
        for(int i=0;i<81;++i){
            if(!movingCells[i])continue;
            const auto p=cellPoint(i),expected=model.at(p.x,p.y);
            if(!finiteMotion(expected)||!backgroundDirectionAgrees(observations[i],expected))continue;
            result.matching[i]=true;++result.directions;
            result.speedMatches+=fitsBackground(observations[i],expected);
            fitted+=model.support[i];
            const int x=i%9,y=i/9;
            zones[y/3*3+x/3]=true;
            minX=std::min(minX,x);maxX=std::max(maxX,x);minY=std::min(minY,y);maxY=std::max(maxY,y);
        }
        // Confirm direction over a broad area. Depth-dependent speed
        // differences belong to local calibration, not the direction vote.
        result.usable=result.directions>=12&&fitted>=8&&result.directions>=.53f*movingCount&&
                      std::count(zones.begin(),zones.end(),true)>=4&&maxX-minX>=4&&maxY-minY>=3;
        return result;
    };
    const auto all=assess(source.cameraModel),moving=assess(source.movingCameraModel);
    const bool preferMoving=moving.usable!=all.usable?moving.usable:
        moving.directions>all.directions||(moving.directions==all.directions&&moving.speedMatches>all.speedMatches);
    const auto& chosen=preferMoving?moving:all;
    candidateMovingCells_=movingCount;candidateDirectionCells_=chosen.directions;candidateSpeedMatches_=chosen.speedMatches;
    if(!chosen.usable)return;
    background_=preferMoving?source.movingCameraModel:source.cameraModel;
    backgroundSupport_=chosen.directions;
    backgroundStamp_=source.timestamp;

    std::array<float,81> ratios{},speeds{};
    for(int i=0;i<81;++i)if(chosen.matching[i]){
        const auto p=cellPoint(i),expected=background_.at(p.x,p.y);
        speeds[i]=std::hypot(observations[i].x,observations[i].y);
        ratios[i]=std::clamp(speeds[i]/std::hypot(expected.x,expected.y),.25f,2.2f);
    }
    backgroundScales_={};
    for(int i=0;i<81;++i){
        const auto p=cellPoint(i),expected=background_.at(p.x,p.y);
        const float predicted=std::hypot(expected.x,expected.y);
        if(!std::isfinite(predicted)||predicted<1)continue;
        struct Rate {float ratio{},weight{};};
        std::array<Rate,49> rates{};
        const int x=i%9,y=i/9;
        int count=0,close=0;
        float sum=0,nearSpeed=0;
        for(int yy=std::max(0,y-3);yy<=std::min(8,y+3);++yy)
            for(int xx=std::max(0,x-3);xx<=std::min(8,x+3);++xx){
                const int j=yy*9+xx;
                if(!chosen.matching[j])continue;
                const int dx=xx-x,dy=yy-y,distance=dx*dx+dy*dy;
                const float weight=cellQuality(source,j)/(1.f+.42f*distance);
                rates[count++]={ratios[j],weight};close+=distance<=5;
                sum+=weight;nearSpeed+=weight*speeds[j];
            }
        if(count<3||close==0)continue;
        std::sort(rates.begin(),rates.begin()+count,[](auto a,auto b){return a.ratio<b.ratio;});
        float cumulative=0;
        for(int j=0;j<count;++j){
            cumulative+=rates[j].weight;
            if(cumulative>=sum*.5f){
                backgroundScales_[i]=std::min(rates[j].ratio,1.32f*nearSpeed/(sum*predicted));
                break;
            }
        }
    }

    std::array<bool,81> independent{};
    std::array<uint8_t,81> candidateRegions{};
    std::array<bool,83> reserved{},assigned{};
    for(auto region:independentRegions_)reserved[region]=true;
    for(int i=0;i<81;++i){
        if(!movingCells[i])continue;
        const auto p=cellPoint(i);
        const auto expected=background_.at(p.x,p.y);
        independent[i]=std::hypot(expected.x,expected.y)>=28&&!backgroundDirectionAgrees(observations[i],expected);
    }

    constexpr std::array stepX{-1,1,0,0},stepY{0,0,-1,1};
    for(int start=0;start<81;++start){
        if(!independent[start]||visited[start])continue;
        std::array<int,81> component{start};
        std::array<bool,9> zones{};
        visited[start]=true;
        int count=1;
        for(int n=0;n<count;++n){
            const int i=component[n],x=i%9,y=i/9;
            zones[y/3*3+x/3]=true;
            for(size_t d=0;d<stepX.size();++d){
                const int xx=x+stepX[d],yy=y+stepY[d];
                if(xx<0||xx>=9||yy<0||yy>=9)continue;
                const int j=yy*9+xx;
                if(independent[j]&&!visited[j]&&sameLocalMotion(observations[i],observations[j])){
                    visited[j]=true;component[count++]=j;
                }
            }
        }
        if(count<12||std::count(zones.begin(),zones.end(),true)<3)continue;
        std::array<int,83> overlap{};
        for(int n=0;n<count;++n)++overlap[independentRegions_[component[n]]];
        uint8_t region=0;
        for(uint8_t id=2;id<assigned.size();++id){
            if(assigned[id]||!overlap[id])continue;
            if(region==0||overlap[id]>overlap[region])region=id;
        }
        if(!region){
            for(uint8_t id=2;id<assigned.size();++id)if(!assigned[id]&&!reserved[id]){region=id;break;}
        }
        if(!region)continue;
        assigned[region]=true;
        for(int n=0;n<count;++n)candidateRegions[component[n]]=region;
    }

    for(int i=0;i<81;++i){
        auto& votes=independentVotes_[i];
        votes=candidateRegions[i]?std::min<int>(votes+1,3):0;
        independentRegions_[i]=votes>=3?candidateRegions[i]:0;
    }
}

void ScreenMotionField::fillNearMissing(const MotionSample& source,bool useBackground) {
    neighborFill_={};neighborUsesBackground_=useBackground;
    std::array<Vec2,81> motion{};
    std::array<float,81> speeds{};
    std::array<bool,9> areas{};
    int count=0;
    const float movingFloor=std::max(7.f,.007f*std::min(source.sourceWidth,source.sourceHeight));
    for(int i=0;i<81;++i){
        if(cellQuality(source,i)<.65f||independentRegions_[i])continue;
        const auto v=cellVelocity(source,i);
        const float speed=std::hypot(v.x,v.y);
        if(!std::isfinite(speed)||speed<movingFloor)continue;
        if(useBackground){
            const auto p=cellPoint(i),expected=background_.at(p.x,p.y);
            if(std::hypot(expected.x,expected.y)<movingFloor*.5f||!alignedMotion(v,expected))continue;
        }
        motion[i]=v;speeds[i]=speed;++count;areas[i/27*3+i%9/3]=true;
    }
    if(count<6||std::count(areas.begin(),areas.end(),true)<3)return;
    for(int i=0;i<81;++i){
        if(cellQuality(source,i)>=.65f||(useBackground&&backgroundScales_[i]>0))continue;
        const int x=i%9,y=i/9;
        struct Vote {int cell{},distance{};float weight{};};
        std::array<Vote,28> votes{}; // Non-center positions within squared distance 9.
        int found=0;
        float total=0;
        for(int yy=std::max(0,y-3);yy<=std::min(8,y+3);++yy)
            for(int xx=std::max(0,x-3);xx<=std::min(8,x+3);++xx){
                const int j=yy*9+xx,dx=xx-x,dy=yy-y,distance=dx*dx+dy*dy;
                if(!distance||distance>9||speeds[j]==0)continue;
                const float weight=cellQuality(source,j)/(1.f+.5f*distance);
                votes[found++]={j,distance,weight};total+=weight;
            }
        if(found<2)continue;
        const auto at=cellPoint(i);
        const auto expected=useBackground?background_.at(at.x,at.y):Vec2{};
        const float predicted=std::hypot(expected.x,expected.y);
        if(useBackground&&(!std::isfinite(predicted)||predicted<movingFloor*.5f))continue;
        float best=0;
        for(int n=0;n<(useBackground?1:found);++n){
            const auto reference=useBackground?expected:motion[votes[n].cell];
            float weight=0,speedSum=0;
            int support=0,nearest=10;
            Vec2 direction{};
            for(int v=0;v<found;++v){
                const auto vote=votes[v];const auto observed=motion[vote.cell];
                if(!useBackground&&!alignedMotion(observed,reference))continue;
                ++support;nearest=std::min(nearest,vote.distance);weight+=vote.weight;
                speedSum+=speeds[vote.cell]*vote.weight;
                direction.x+=observed.x*vote.weight;direction.y+=observed.y*vote.weight;
            }
            if(support<2||nearest>5||weight<total*.72f||weight<=best)continue;
            const auto axis=useBackground?expected:direction;
            const float length=std::hypot(axis.x,axis.y);
            if(length<=0)continue;
            const float fade=1.f-.16f*std::sqrt(float(nearest));
            const float limited=fade*(useBackground?std::min(predicted,speedSum/weight):speedSum/weight);
            neighborFill_[i]={{axis.x*limited/length,axis.y*limited/length},.74f};
            best=weight;
        }
    }
}

void ScreenMotionField::update(const MotionSample& source,const FieldIntent& intent,MotionFieldFrame mapping,
                               double now,bool enabled,bool stationary,bool backgroundPriority,const ScreenFixedMotion* screenFixed,
                               bool depthEnabled) {
    if(!enabled||!std::isfinite(now)||mapping.sourceWidth<=0||mapping.sourceHeight<=0||
       mapping.targetWidth<=0||mapping.targetHeight<=0){if(frame_.targetWidth)reset();return;}
    if(frame_.sourceWidth!=mapping.sourceWidth||frame_.sourceHeight!=mapping.sourceHeight||
       frame_.viewX!=mapping.viewX||frame_.viewY!=mapping.viewY||
       frame_.viewWidth!=mapping.viewWidth||frame_.viewHeight!=mapping.viewHeight||backgroundPriority_!=backgroundPriority)reset();

    frame_=mapping;
    backgroundPriority_=backgroundPriority;
    frame_.analysisWidth=source.analysisWidth;frame_.analysisHeight=source.analysisHeight;
    const bool fresh=motionFresh(source,now)&&source.sourceWidth==frame_.sourceWidth&&source.sourceHeight==frame_.sourceHeight;
    const bool newFrame=fresh&&source.timestamp>lastSourceTimestamp_+1e-6;
    if(newFrame)lastSourceTimestamp_=source.timestamp;
    frame_.timestamp=now;
    frame_.observationTimestamp=lastSourceTimestamp_;
    camera_=fresh?source.cameraModel:AffineFlow{};
    if(backgroundPriority&&newFrame&&!stationary)classifyBackground(source);
    const bool measuredBackground=fresh&&backgroundStamp_>0&&backgroundStamp_==source.timestamp;
    if(depthEnabled&&fresh&&!stationary){
        if(depthStamp_!=source.timestamp){
            const auto& model=measuredBackground?background_:source.movingCameraModel;
            depthDemand_=radialDepth(source,model);
            if(depthDemand_==0)depthDemand_=radialDepth(source,source.cameraModel);
            depthStamp_=source.timestamp;
        }
        frame_.depthDemand=depthDemand_;
    }
    const double backgroundAge=now-backgroundStamp_;
    if(stationary||(!measuredBackground&&backgroundAge>=.17)){
        backgroundStamp_=0;backgroundSupport_=0;independentRegions_={};independentVotes_={};
    }
    if(!fresh||stationary)candidateMovingCells_=candidateDirectionCells_=candidateSpeedMatches_=0;
    const bool useBackground=backgroundStamp_>0;
    if(backgroundPriority&&fresh&&!stationary&&(newFrame||neighborUsesBackground_!=useBackground))fillNearMissing(source,useBackground);
    backgroundConfidence_=measuredBackground?.80f:.80f*float(1-std::clamp((backgroundAge-.085)/.085,0.,1.));
    backgroundLook_={};
    if(useBackground){
        camera_=background_;
        if(measuredBackground&&intent.aligned&&now-source.timestamp<=.11&&std::hypot(intent.look.x,intent.look.y)>2){
            backgroundLook_={.20f*std::clamp(intent.look.x-intent.capturedLook.x,-420.f,420.f),
                             .20f*std::clamp(intent.look.y-intent.capturedLook.y,-420.f,420.f)};
        }
    }
    stats_={};
    stats_.backgroundSupport=backgroundSupport_;
    stats_.candidateMovingCells=candidateMovingCells_;stats_.candidateDirectionCells=candidateDirectionCells_;
    stats_.candidateSpeedMatches=candidateSpeedMatches_;
    stats_.backgroundActive=useBackground;stats_.backgroundHeld=useBackground&&!measuredBackground;
    for(int i=0;i<81;++i){
        auto& cell=cells_[i];cell={};
        auto& held=previous_[i];
        const auto p=cellPoint(i);
        const auto measured=cellVelocity(source,i);
        const float q=cellQuality(source,i);
        const auto fixed=fresh&&screenFixed?screenFixed->cells()[i]:BackgroundCorrection{};
        const bool directlyMeasured=fresh&&q>=.65f&&finiteMotion(measured);
        if(fresh){
            const bool quiet=directlyMeasured&&std::hypot(measured.x,measured.y)<22;
            stats_.unobservedCells+=!directlyMeasured;stats_.unobservedByBand[i/27]+=!directlyMeasured;
            stats_.quietObservedCells+=quiet;stats_.quietByBand[i/27]+=quiet;
        }
        const bool corroboratedLocal=measuredBackground||(held.stamp>0&&source.timestamp>=held.stamp&&source.timestamp-held.stamp<.17&&
                                     sameLocalMotion(held.velocity,measured)&&std::hypot(measured.x,measured.y)>=25);
        const auto region=directlyMeasured&&corroboratedLocal?independentRegions_[i]:uint8_t{};
        float measuredShare=0,heldMotionWeight=-1;
        if(fresh&&stationary){
            held={};
            cell={{},.9f,FieldOrigin::Stationary};
        }else if(useBackground&&!region&&backgroundScales_[i]>0){
            const auto expected=background_.at(p.x,p.y);
            const float scale=backgroundScales_[i];
            cell={cap({expected.x*scale+backgroundLook_.x,expected.y*scale+backgroundLook_.y}),backgroundConfidence_,
                  FieldOrigin::Background,1};
            cell.backgroundScale=scale;
            if(!directlyMeasured||std::hypot(measured.x,measured.y)<25)++stats_.boundedBackgroundCells;
            if(directlyMeasured){
                const auto base=cell.velocity;
                const auto local=cap(measured);
                const float minorSpeed=std::hypot(local.x,local.y);
                const bool aligned=backgroundDirectionAgrees(local,expected)||
                    (minorSpeed>=8&&minorSpeed<25&&std::hypot(expected.x,expected.y)>=8&&alignedMotion(local,expected));
                const float weight=aligned?.85f:directMotionWeight(source,i,local,q,base,backgroundConfidence_);
                if(weight>0){
                    cell.localCorrection={(local.x-base.x)*weight,(local.y-base.y)*weight};
                    cell.velocity={base.x+cell.localCorrection.x,base.y+cell.localCorrection.y};
                    cell.confidence=std::lerp(backgroundConfidence_,q,weight);
                    if(aligned)measuredShare=weight;
                    ++stats_.preservedMotionCells;
                }else if(!fitsBackground(measured,expected))++stats_.foregroundSuppressed;
            }
            held={};
        }else if(fixed.confidence>0){
            cell={cap(fixed.velocity),fixed.confidence,FieldOrigin::CameraModel};
            if(newFrame)held={cell.velocity,cell.confidence,source.timestamp};
        }else if(directlyMeasured){
            cell={cap(measured),q,FieldOrigin::Measured,region};
            measuredShare=1.f;
            if(region)++stats_.independentCells;
            if(newFrame)held={cell.velocity,cell.confidence,source.timestamp};
        }else if(fresh&&backgroundPriority&&neighborFill_[i].confidence>0){
            const auto fill=neighborFill_[i];
            cell={cap(fill.velocity),fill.confidence,FieldOrigin::Neighbors};
            if(useBackground){
                const auto expected=background_.at(p.x,p.y);
                const float length=std::hypot(expected.x,expected.y);
                if(length>0){
                    cell.motionRegion=1;
                    cell.backgroundScale=std::hypot(cell.velocity.x,cell.velocity.y)/length;
                    cell.localCorrection={-backgroundLook_.x,-backgroundLook_.y};
                }
                if(!measuredBackground)cell.confidence*=backgroundConfidence_/.80f;
            }
        }else{
            Vec2 model{};
            const bool modelValid=fresh&&!useBackground&&cameraFill(source,i,model);
            const bool canHold=held.stamp>0&&now>=held.stamp&&now-held.stamp<.19;
            const bool reversing=canHold&&std::hypot(intent.look.x,intent.look.y)>60&&
                held.velocity.x*intent.look.x+held.velocity.y*intent.look.y<0;
            if(reversing)held={};
            if(modelValid&&(!canHold||reversing||sameLocalMotion(held.velocity,model))){
                cell={cap(model),.57f,FieldOrigin::CameraModel};
            }else if(canHold&&!reversing){
                const float weight=float(1-std::clamp((now-held.stamp-.07)/.12,0.,1.));
                cell={{held.velocity.x*weight,held.velocity.y*weight},held.confidence*.67f*weight,FieldOrigin::Held};
                heldMotionWeight=held.confidence*.67f;
            }else if(intent.timestamp>0&&now>=intent.timestamp&&now-intent.timestamp<.10){
                const auto predicted=intent.startup?intent.at(p.x,p.y):intent.look;
                if(std::hypot(predicted.x,predicted.y)>7)
                    cell={{predicted.x*.62f,predicted.y*.62f},.38f,FieldOrigin::Input};
            }
        }
        if(fresh&&intent.aligned&&now-source.timestamp<=.11&&cell.origin==FieldOrigin::Measured&&!region){
            const auto current=intent.at(p.x,p.y),captured=intent.atCapture(p.x,p.y);
            if(std::hypot(current.x,current.y)>2){
                cell.velocity.x+=std::clamp(current.x-captured.x,-420.f,420.f)*.60f;
                cell.velocity.y+=std::clamp(current.y-captured.y,-420.f,420.f)*.60f;
                cell.velocity=cap(cell.velocity);
            }
        }
        switch(cell.origin){
            case FieldOrigin::Measured:++stats_.measured;break;
            case FieldOrigin::CameraModel:++stats_.modeled;break;
            case FieldOrigin::Background:++stats_.modeled;break;
            case FieldOrigin::Neighbors:++stats_.neighborFilledCells;++stats_.neighborByBand[i/27];break;
            case FieldOrigin::Held:++stats_.held;break;
            case FieldOrigin::Input:++stats_.input;break;
            case FieldOrigin::Stationary:++stats_.stationary;break;
            default:break;
        }
        cell.motionWeight=cell.confidence;
        if(backgroundPriority&&heldMotionWeight>=0)cell.motionWeight=heldMotionWeight;
        if(backgroundPriority&&measuredShare>0){
            const float speed=std::hypot(measured.x,measured.y);
            const float unit=std::max(1.f,std::min(source.sourceWidth,source.sourceHeight)/1080.f);
            const float blend=1.f-std::clamp((speed/unit-65.f)/95.f,0.f,1.f);
            cell.motionWeight=std::lerp(cell.confidence,1.f,measuredShare*blend);
            if(blend>.5f&&speed>=2)++stats_.slowDirectCells;
        }
        const float speedSquared=cell.velocity.x*cell.velocity.x+cell.velocity.y*cell.velocity.y;
        const float threshold=backgroundPriority?2.25f:9.f;
        stats_.moving|=cell.confidence>0&&speedSquared*cell.motionWeight*cell.motionWeight>threshold;
    }
    // Cache local compatibility once per snapshot. Thousands of mesh node and
    // coverage queries can then reuse it without rechecking the same vectors.
    constexpr std::array dx{1,-1,0,1},dy{0,1,1,1};
    for(int y=0;y<9;++y)for(int x=0;x<9;++x){
        const int i=y*9+x;
        const auto& a=cells_[i];
        for(size_t n=0;n<dx.size();++n){
            const int xx=x+dx[n],yy=y+dy[n];
            if(xx<0||xx>=9||yy>=9)continue;
            const auto& b=cells_[yy*9+xx];
            const bool available=a.confidence>0&&b.confidence>0;
            compatibleNeighbors_[i][n]=available&&((a.motionRegion||b.motionRegion)?a.motionRegion==b.motionRegion:
                                                    sameLocalMotion(a.velocity,b.velocity));
            if(available&&!compatibleNeighbors_[i][n]&&(n==0||n==2))++stats_.boundaries;
        }
    }
}

FieldSample ScreenMotionField::sample(Vec2 p) const {
    if(frame_.targetWidth<=0||frame_.targetHeight<=0||!finiteMotion(p))return {};
    const float cropW=frame_.viewWidth>0?frame_.viewWidth:frame_.sourceWidth;
    const float cropH=frame_.viewHeight>0?frame_.viewHeight:frame_.sourceHeight;
    const float u=(frame_.viewX+p.x*cropW/frame_.targetWidth)/frame_.sourceWidth;
    const float v=(frame_.viewY+p.y*cropH/frame_.targetHeight)/frame_.sourceHeight;
    const float geometry=frame_.mappingKnown?1.f:.78f;
    const auto backgroundSample=[&](float scale,Vec2 correction,float confidence,float motionWeight,bool boundary)->FieldSample{
        const auto expected=background_.at(std::clamp(u,0.f,1.f),std::clamp(v,0.f,1.f));
        const auto motion=cap({expected.x*scale+backgroundLook_.x+correction.x,expected.y*scale+backgroundLook_.y+correction.y});
        return {{motion.x*frame_.targetWidth/cropW,motion.y*frame_.targetHeight/cropH},
                confidence*geometry,true,boundary,1,motionWeight*geometry};
    };
    const float gx=std::clamp(9*u-.5f,0.f,8.f),gy=std::clamp(9*v-.5f,0.f,8.f);
    const int x=int(gx),y=int(gy),right=std::min(x+1,8),bottom=std::min(y+1,8);
    const float dx=gx-x,dy=gy-y;
    const std::array ids{y*9+x,y*9+right,bottom*9+x,bottom*9+right};
    const std::array weights{(1-dx)*(1-dy),dx*(1-dy),(1-dx)*dy,dx*dy};
    int anchor=-1;
    float best=0;
    for(size_t i=0;i<ids.size();++i){
        const auto& cell=cells_[ids[i]];
        if(cell.confidence>0&&weights[i]>best){anchor=ids[i];best=weights[i];}
    }
    if(anchor<0)return {};
    const auto& closest=cells_[anchor];
    const bool background=closest.motionRegion==1;
    Vec2 sum{};float support=0,spatial=0,scale=0,motionWeight=0;bool boundary=false;
    for(size_t i=0;i<ids.size();++i){
        const auto& cell=cells_[ids[i]];
        if(weights[i]<=0||cell.confidence<=0)continue;
        const int offset=std::abs(anchor-ids[i]);
        if(offset&&!compatibleNeighbors_[std::min(anchor,ids[i])][offset==1?0:offset-7]){boundary=true;continue;}
        const float w=weights[i]*cell.confidence;
        const auto motion=background?cell.localCorrection:cell.velocity;
        sum.x+=w*motion.x;sum.y+=w*motion.y;support+=w;spatial+=weights[i];
        scale+=w*cell.backgroundScale;
        motionWeight+=w*cell.motionWeight;
    }
    if(support<=0)return {};
    if(background){
        if(boundary)return backgroundSample(closest.backgroundScale,closest.localCorrection,closest.confidence,closest.motionWeight,true);
        return backgroundSample(scale/support,{sum.x/support,sum.y/support},std::clamp(support/spatial,0.f,1.f),motionWeight/support,false);
    }
    if(boundary) return {{closest.velocity.x*frame_.targetWidth/cropW,
                          closest.velocity.y*frame_.targetHeight/cropH},closest.confidence*geometry,true,true,closest.motionRegion,closest.motionWeight*geometry};
    return {{sum.x/support*frame_.targetWidth/cropW,sum.y/support*frame_.targetHeight/cropH},
            std::clamp(support/spatial,0.f,1.f)*geometry,true,false,closest.motionRegion,(motionWeight/support)*geometry};
}

}
