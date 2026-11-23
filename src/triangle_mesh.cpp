#include "triangle_mesh.hpp"
#include <chrono>
#include <tuple>

namespace mc {
namespace {
using Clock=std::chrono::steady_clock;
constexpr float fadeInSeconds=.34f,fadeOutSeconds=.20f;

float stablePhase(uint64_t key) {
    return float((key*11400714819323198485ull)>>48)/65535.f;
}

double elapsedMs(Clock::time_point start) {
    return std::chrono::duration<double,std::milli>(Clock::now()-start).count();
}

float distanceSquared(Vec2 a,Vec2 b) {
    const float x=a.x-b.x,y=a.y-b.y;return x*x+y*y;
}

bool joinsMesh(const MeshNode& node) {
    return node.life==MeshLife::Active||node.life==MeshLife::Entering;
}

Vec2 fieldVelocity(const MotionField& field,Vec2 position) {
    return meshVelocity(field.sample(position),field.frame().targetHeight);
}

bool sameRegion(uint8_t a,uint8_t b) {return !a||!b||a==b;}

Vec2 localAreaRates(const MotionField& field,Vec2 position,float step,FieldSample center,bool live) {
    const int height=field.frame().targetHeight;
    const auto middle=meshVelocity(center,height);
    const auto axis=[&](Vec2 offset,float Vec2::*component){
        const auto low=field.sample({position.x-offset.x,position.y-offset.y});
        const auto high=field.sample({position.x+offset.x,position.y+offset.y});
        const auto a=meshVelocity(low,height),b=meshVelocity(high,height);
        if(!live)return (b.*component-a.*component)/(2*step);
        if(!center.valid)return 0.f;
        const auto usable=[&](FieldSample neighbor,Vec2 motion){
            if(!neighbor.valid||!sameRegion(center.motionRegion,neighbor.motionRegion))return false;
            if(center.motionRegion&&neighbor.motionRegion)return true;
            return !(center.boundary||neighbor.boundary)||distanceSquared(middle,motion)<22.f*22.f;
        };
        const bool takeLow=usable(low,a),takeHigh=usable(high,b);
        if(takeLow&&takeHigh)return (b.*component-a.*component)/(2*step);
        if(takeLow)return (middle.*component-a.*component)/step;
        if(takeHigh)return (b.*component-middle.*component)/step;
        return 0.f;
    };
    return {axis({step,0},&Vec2::x),axis({0,step},&Vec2::y)};
}

bool occupiesSpace(const MeshNode& node) {
    return joinsMesh(node)||(node.life==MeshLife::Leaving&&node.opacity>.12f);
}

bool approachesViewport(Vec2 p,Vec2 velocity,int width,int height,float spacing,float* approach=nullptr) {
    auto requiredTime=[&](float position,float speed,float high){
        if(position<0)return speed>4?-position/speed:-1.f;
        if(position>high)return speed< -4?(high-position)/speed:-1.f;
        return 0.f;
    };
    const float tx=requiredTime(p.x,velocity.x,float(width));
    const float ty=requiredTime(p.y,velocity.y,float(height));
    if(tx<0||ty<0)return false;
    const float speed=std::hypot(velocity.x,velocity.y);
    if(speed<5)return false;
    const float time=std::max(tx,ty);
    if(time<fadeInSeconds*.62f||time>fadeInSeconds+1.25f*spacing/speed)return false;
    const Vec2 arrival{p.x+velocity.x*time,p.y+velocity.y*time};
    const float tolerance=spacing*.1f;
    if(arrival.x< -tolerance||arrival.x>width+tolerance||
       arrival.y< -tolerance||arrival.y>height+tolerance)return false;
    if(approach)*approach=time;
    return true;
}


// A small, deterministic perturbation resolves cocircular choices. Only
// topology sees it; rendered node positions come directly from integration.
Vec2 topologyPoint(const MeshNode& node,float spacing) {
    const uint64_t bits=node.id*11400714819323198485ull;
    return {node.position.x+(float(bits>>56)/255.f-.5f)*spacing*.002f,
            node.position.y+(float((bits>>48)&255)/255.f-.5f)*spacing*.002f};
}

struct Circle {Vec2 center;float radiusSquared;};
Circle circumcircle(Vec2 a,Vec2 b,Vec2 c,double minArea) {
    const double bx=b.x-a.x,by=b.y-a.y,cx=c.x-a.x,cy=c.y-a.y;
    const double determinant=bx*cy-by*cx;
    if(std::abs(determinant)<minArea)return {{},-1};
    const double b2=bx*bx+by*by,c2=cx*cx+cy*cy;
    const double x=(cy*b2-by*c2)/(2*determinant),y=(bx*c2-cx*b2)/(2*determinant);
    return {{float(a.x+x),float(a.y+y)},float(x*x+y*y)};
}
}

bool TriangleMesh::linkCompatible(Vec2 a,Vec2 b,bool existing) const {
    const float difference=distanceSquared(a,b);
    if(!liveTracking_)return difference<70.f*70.f||a.x*b.x+a.y*b.y>=0;
    const float speedSquared=std::max(distanceSquared(a,{}),distanceSquared(b,{}));
    // Rebuilding a connection requires closer velocity agreement than keeping it.
    const float floor=existing?22.f:14.f,ratio=existing?.60f:.43f;
    return difference<=floor*floor+ratio*ratio*speedSquared;
}

float meshCenterClearRadius(int width,int height) {return float(std::min(width,height))*.045f;}

float meshCenterOpacity(Vec2 p,int width,int height) {
    const float shortSide=float(std::min(width,height));
    if(shortSide<=0)return 1;
    const float radius=std::hypot(p.x-width*.5f,p.y-height*.5f);
    const float t=std::clamp((radius-meshCenterClearRadius(width,height))/(shortSide*.12f),0.f,1.f);
    return t*t*(3-2*t);
}

void TriangleMesh::seed(int width,int height,float spacing) {
    nodes_.clear();triangles_.clear();edges_.clear();free_.clear();newAnchors_.clear();cells_.clear();probes_.clear();
    stats_={};topologyTime_=0;seconds_=0;translationPhase_={};referencePhaseValid_=true;faceCursor_=probeCursor_=0;nextId_=0;nodeLimit_=0;
    width_=width;height_=height;spacing_=spacing;
    const auto lattice=triangleLattice(width,height,spacing);
    const int columns=lattice.columns,rows=lattice.rows;
    if(!columns||!rows)return;
    const float rowStep=lattice.rowStep();
    if((columns-3)*spacing<width||(rows-3)*rowStep<height)return;
    buffer_=std::max(3.2f*spacing,std::max(240.f,height*.45f)*fadeInSeconds+spacing);
    origin_={-buffer_,-buffer_};
    columns_=int(std::ceil((width+2*buffer_)/spacing))+1;
    rows_=int(std::ceil((height+2*buffer_)/spacing))+1;
    cells_.resize(size_t(columns_)*rows_);
    nodeLimit_=size_t(rows)*columns*2;
    nodes_.reserve(nodeLimit_);
    triangles_.reserve(nodeLimit_*2);
    edges_.reserve(nodeLimit_*3);

    latticeOrigin_=lattice.origin;
    for(int row=0;row<rows;++row)for(int column=0;column<columns;++column)
        nodes_.push_back({lattice.point(column,row),
                          {},++nextId_,1,1,0,MeshLife::Active});
    for(int row=0;row<rows-1;++row)for(int column=0;column<columns-1;++column){
        const auto a=uint32_t(row*columns+column),b=a+1,c=a+columns,d=c+1;
        if(row%2){triangles_.push_back({{a,b,d}});triangles_.push_back({{a,c,d}});}
        else {triangles_.push_back({{a,b,c}});triangles_.push_back({{b,c,d}});}
    }
    // Probes measure persistent coverage gaps; new points use a local lattice basis.
    const float probeStep=spacing*.60f;
    for(int row=0;row*probeStep*.8660254f<height;++row){
        const float y=(row+.5f)*probeStep*.8660254f;
        if(y>=height)break;
        for(float x=(row%2?.75f:.25f)*probeStep;x<width;x+=probeStep)
            probes_.push_back({{x,y},-1});
    }
    if(edgesEnabled_)updateEdges();
    countNodes();
}

int TriangleMesh::cellAt(float coordinate,float start,int count) const {
    return std::clamp(int((coordinate-start)/spacing_),0,count-1);
}

void TriangleMesh::indexNode(uint32_t index) {
    const auto p=nodes_[index].position;
    cells_[size_t(cellAt(p.y,origin_.y,rows_))*columns_+cellAt(p.x,origin_.x,columns_)].push_back(index);
}

void TriangleMesh::rebuildIndex() {
    for(auto& cell:cells_)cell.clear();
    for(uint32_t index=0;index<nodes_.size();++index)
        if(occupiesSpace(nodes_[index]))indexNode(index);
}

bool TriangleMesh::add(Vec2 position,const MotionField& field,BirthSite site,bool expanding) {
    if(position.x< -buffer_||position.y< -buffer_||
       position.x>width_+buffer_||position.y>height_+buffer_)return false;
    uint32_t index{};
    if(!free_.empty()){index=free_.back();free_.pop_back();}
    else if(nodes_.size()<nodeLimit_){index=uint32_t(nodes_.size());nodes_.push_back({});}
    else return false;
    const float variation=site==BirthSite::Inlet?.025f:.075f;
    const auto id=++nextId_;
    // Expansion can open many sites together. Reserve positions immediately,
    // then feather in visibility over independently paced intervals.
    const float phase=stablePhase(id);
    const float delay=expanding?.010f+.085f*phase:variation*phase;
    const float duration=expanding?.48f+.24f*stablePhase(id+7):fadeInSeconds;
    const auto motion=field.sample(position);
    nodes_[index]={position,meshVelocity(motion,height_),id,0,0,0,MeshLife::Entering,delay,duration,motion.motionRegion};
    indexNode(index);
    newAnchors_.push_back(index);
    if(site==BirthSite::Inlet)++stats_.inletBirths;
    else if(site==BirthSite::Center)++stats_.centerBirths;
    else ++stats_.interiorBirths;
    return true;
}

void TriangleMesh::retire(MeshNode& node) {
    node.opacity=0;node.life=MeshLife::Retired;
    ++stats_.retired;
}

bool TriangleMesh::hasRoom(Vec2 position,float distance,uint8_t motionRegion) const {
    bool free=true;
    const float squared=distance*distance;
    nearby(position,distance,[&](uint32_t i){
        const auto& node=nodes_[i];
        const float limit=sameRegion(motionRegion,node.motionRegion)?squared:std::min(squared,.22f*spacing_*spacing_);
        if(free&&occupiesSpace(node)&&distanceSquared(position,node.position)<limit)
            free=false;
    });
    return free;
}

bool TriangleMesh::findAlignedSite(Vec2 target,const MotionField& field,BirthSite site,Vec2& result) const {
    // Extend a nearby pair's lattice basis by multiples of 60 degrees. A
    // predicted vacancy may nominate an area, but never becomes a point.
    const auto region=liveTracking_?field.sample(target).motionRegion:uint8_t{};
    std::array<std::pair<float,uint32_t>,4> anchors{};
    for(auto& item:anchors)item.first=4*spacing_*spacing_;
    nearby(target,2*spacing_,[&](uint32_t i){
        const auto& node=nodes_[i];
        if(!joinsMesh(node)||node.opacity<.35f||!sameRegion(node.motionRegion,region))return;
        const auto d=distanceSquared(node.position,target);
        const auto at=std::lower_bound(anchors.begin(),anchors.end(),d,[](auto item,float value){
            return item.first<value;
        });
        if(at==anchors.end()||d>=4*spacing_*spacing_)return;
        std::move_backward(at,anchors.end()-1,anchors.end());
        *at={d,i};
    });
    const float d2=spacing_*spacing_;
    float best=.90f*.90f*d2;
    const float minimum=site==BirthSite::Interior?.67f:.76f;
    bool found=false;
    for(const auto& [distance,index]:anchors){
        if(distance>=4*d2)break;
        const auto& anchor=nodes_[index];
        float peerError=1.6f*d2;
        Vec2 nearestAxis{};
        std::array<Vec2,16> offsets{};
        size_t count=0;
        float sumCos=0,sumSin=0,sumWeight=0,sumLength=0;
        nearby(anchor.position,spacing_*1.62f,[&](uint32_t j){
            const auto& peer=nodes_[j];
            if(j==index||!joinsMesh(peer)||peer.opacity<.35f||!sameRegion(anchor.motionRegion,peer.motionRegion)||
               !linkCompatible(anchor.velocity,peer.velocity))return;
            const Vec2 offset{peer.position.x-anchor.position.x,peer.position.y-anchor.position.y};
            const float squared=distanceSquared(anchor.position,peer.position);
            if(squared<.52f*.52f*d2||squared>1.58f*1.58f*d2)return;
            const float length=std::sqrt(squared);
            const float error=std::abs(squared-d2);
            if(error<peerError){nearestAxis=offset;peerError=error;}
            if(count>=offsets.size())return;
            offsets[count++]=offset;
            // Fold all six directions of the triangular lattice onto the
            // same orientation. A single diagonal cannot steer the fit.
            const float angle=6*std::atan2(offset.y,offset.x);
            const float weight=1.f/(1.f+3*std::abs(length/spacing_-1));
            sumCos+=weight*std::cos(angle);sumSin+=weight*std::sin(angle);
            sumWeight+=weight;sumLength+=weight*length;
        });
        const float nearestLength=std::hypot(nearestAxis.x,nearestAxis.y);
        if(nearestLength<=0)continue;
        const bool coherent=count>=2&&std::hypot(sumCos,sumSin)>.58f*sumWeight;
        const float step=std::clamp(coherent?sumLength/sumWeight:nearestLength,
            spacing_*(site==BirthSite::Inlet?.80f:.90f),spacing_*1.12f);
        Vec2 axis{nearestAxis.x*step/nearestLength,nearestAxis.y*step/nearestLength};
        Vec2 origin=anchor.position;
        if(coherent){
            const float angle=std::atan2(sumSin,sumCos)/6;
            const float x=std::cos(angle),y=std::sin(angle);
            axis={x*step,y*step};
            // Average the phase error of neighbors that agree with the fitted
            // rows. Keep the correction small and only use it for new points.
            Vec2 phase{};
            int phaseSamples=2;
            for(size_t n=0;n<count;++n){
                const auto p=offsets[n];
                const float u=p.x*x+p.y*y,v=p.y*x-p.x*y;
                const float row=std::round(v/(.8660254f*step));
                const float column=std::round(u/step-row*.5f);
                const float du=u-step*(column+row*.5f),dv=v-step*row*.8660254f;
                if(du*du+dv*dv>.24f*.24f*d2)continue;
                phase.x+=du*x-dv*y;phase.y+=du*y+dv*x;
                ++phaseSamples;
            }
            phase={phase.x/phaseSamples,phase.y/phaseSamples};
            const float phaseLength=std::hypot(phase.x,phase.y);
            const float scale=phaseLength>spacing_*.13f?spacing_*.13f/phaseLength:1.f;
            origin.x+=phase.x*scale;origin.y+=phase.y*scale;
        }

        for(int direction=0;direction<6;++direction){
            const Vec2 p{origin.x+axis.x,origin.y+axis.y};
            axis={.5f*axis.x-.8660254f*axis.y,.8660254f*axis.x+.5f*axis.y};
            const bool inside=p.x>=0&&p.x<=width_&&p.y>=0&&p.y<=height_;
            const float score=distanceSquared(p,target);
            if(score>=best||!hasRoom(p,minimum*spacing_,region))continue;
            const auto sample=field.sample(p);
            if(!sameRegion(region,sample.motionRegion))continue;
            const auto speed=meshVelocity(sample,height_);
            if(!linkCompatible(anchor.velocity,speed))continue;
            if(site==BirthSite::Inlet){
                if(inside||!approachesViewport(p,speed,width_,height_,spacing_))continue;
            } else if(!inside)continue;
            result=p;best=score;found=true;
        }
    }
    return found;
}

void TriangleMesh::recycleCrowding(const MotionField& field,float sinkRadius,float elapsed) {
    const Vec2 center{width_*.5f,height_*.5f};
    const float d2=spacing_*spacing_;
    const float horizon=fadeOutSeconds+.04f;
    auto projected=[&](const MeshNode& node){
        return Vec2{node.position.x+node.velocity.x*horizon,node.position.y+node.velocity.y*horizon};
    };
    int centerBudget=8;
    if(sinkRadius>0){
        const auto centerRegion=field.sample(center).motionRegion;
        for(auto& node:nodes_){
            if(!centerBudget)break;
            if(node.life!=MeshLife::Active||distanceSquared(node.position,center)>sinkRadius*sinkRadius||
               !sameRegion(node.motionRegion,centerRegion))continue;
            node.life=MeshLife::Leaving;--centerBudget;++stats_.centerExits;
        }
    }

    struct Candidate {uint32_t node,keeper;float distance,priority;};
    std::vector<Candidate> candidates;
    candidates.reserve(32);
    for(uint32_t i=0;i<nodes_.size();++i){
        auto& node=nodes_[i];
        if(node.life!=MeshLife::Active)continue;
        const Vec2 ahead=projected(node);
        float best=.59f*.59f*d2;
        uint32_t keeper=uint32_t(nodes_.size());
        nearby(node.position,spacing_*.85f,[&](uint32_t j){
            const auto& peer=nodes_[j];
            if(j==i||peer.life!=MeshLife::Active||peer.id>=node.id||
               !sameRegion(node.motionRegion,peer.motionRegion)||!linkCompatible(node.velocity,peer.velocity))return;
            if(distanceSquared(node.position,peer.position)>.68f*.68f*d2)return;
            const auto d=distanceSquared(ahead,projected(peer));
            if(d<best){best=d;keeper=j;}
        });
        if(keeper==nodes_.size()){node.crowdedFor=0;continue;}
        node.crowdedFor+=elapsed;
        const auto rates=localAreaRates(field,node.position,spacing_*.55f,field.sample(node.position),liveTracking_);
        const float rate=rates.x+rates.y;
        if(node.crowdedFor>=(rate<-.12f?.09f:.19f)||best<.34f*.34f*d2)
            candidates.push_back({i,keeper,best,best+std::clamp(rate,-.8f,.8f)*.035f*d2});
    }
    std::sort(candidates.begin(),candidates.end(),[](auto a,auto b){return a.priority<b.priority;});
    int remaining=8;
    for(auto option:candidates){
        if(!remaining)break;
        auto& node=nodes_[option.node];
        if(node.life!=MeshLife::Active||nodes_[option.keeper].life!=MeshLife::Active)continue;
        // A close pair can still be on the rim of a hole. Protect any side
        // that would have no surviving same-flow neighbor after this fade.
        bool covered=true;
        Vec2 direction{spacing_*.60f,0};
        for(int sample=0;sample<6&&covered;++sample){
            const Vec2 p{node.position.x+direction.x,node.position.y+direction.y};
            direction={.5f*direction.x-.8660254f*direction.y,.8660254f*direction.x+.5f*direction.y};
            if(p.x<0||p.x>width_||p.y<0||p.y>height_||
               (sinkRadius>0&&distanceSquared(p,center)<sinkRadius*sinkRadius))continue;
            if(node.motionRegion&&!sameRegion(node.motionRegion,field.sample(p).motionRegion))continue;
            bool kept=false;
            nearby(p,spacing_*.90f,[&](uint32_t j){
                if(j!=option.node&&nodes_[j].life==MeshLife::Active&&
                   sameRegion(node.motionRegion,nodes_[j].motionRegion)&&linkCompatible(node.velocity,nodes_[j].velocity)&&
                   distanceSquared(p,nodes_[j].position)<.81f*d2)
                    kept=true;
            });
            covered=kept;
        }
        if(!covered&&option.distance>.30f*.30f*d2)continue;
        node.life=MeshLife::Leaving;--remaining;++stats_.merged;
    }
}


void TriangleMesh::replenish(const MotionField& field,bool alignedTranslation,float radialRate,float sinkRadius) {
    const float step=spacing_*.8660254f;
    const float reach=buffer_-spacing_*.3f;
    const Vec2 phase=alignedTranslation?translationPhase_:Vec2{};
    const Vec2 base{latticeOrigin_.x+phase.x,latticeOrigin_.y+phase.y};
    const int firstRow=int(std::floor((-reach-base.y)/step));
    const int lastRow=int(std::ceil((height_+reach-base.y)/step));
    const Vec2 center{width_*.5f,height_*.5f};
    const auto centerRegion=field.sample(center).motionRegion;

    struct Inlet {Vec2 p;float approach;};
    std::vector<Inlet> inlet;
    inlet.reserve(64);
    for(int row=firstRow;row<=lastRow;++row){
        const float y=base.y+row*step;
        const float x0=base.x+((row%2)!=0?spacing_*.5f:0);
        const int first=int(std::ceil((-reach-x0)/spacing_));
        const int last=int(std::floor((width_+reach-x0)/spacing_));
        for(int col=first;col<=last;++col){
            const Vec2 p{x0+col*spacing_,y};
            if(p.x>=0&&p.x<=width_&&p.y>=0&&p.y<=height_)continue;
            const auto motion=field.sample(p);
            const auto speed=meshVelocity(motion,height_);
            float approach{};
            if(approachesViewport(p,speed,width_,height_,spacing_,&approach)&&hasRoom(p,spacing_*.73f,motion.motionRegion))
                inlet.push_back({p,approach});
        }
    }
    std::stable_sort(inlet.begin(),inlet.end(),[](const auto& a,const auto& b){return std::abs(a.approach-fadeInSeconds)<std::abs(b.approach-fadeInSeconds);});
    int inletBudget=8;
    for(const auto& site:inlet){
        if(!inletBudget)break;
        Vec2 p=site.p;
        if(!alignedTranslation&&!findAlignedSite(site.p,field,BirthSite::Inlet,p)){
            // Keep the reference row as a seed if the incoming band lost
            // its nearby anchors. Leave extra space around existing rows.
            if(!hasRoom(p,spacing_*.90f,field.sample(p).motionRegion))continue;
        }
        if(!hasRoom(p,spacing_*.73f,field.sample(p).motionRegion))continue;
        if(add(p,field,BirthSite::Inlet))--inletBudget;
    }

    if(radialRate>.10f){
        int centerBudget=6;
        for(int row=-2;row<=2&&centerBudget;++row){
            for(int col=-2;col<=2&&centerBudget;++col){
                const Vec2 p{center.x+(col+((row%2)!=0?.5f:0)-.25f)*spacing_,
                             center.y+(row+.5f)*step};
                const float radius=distanceSquared(p,center);
                if(radius<spacing_*spacing_*.31f||radius>spacing_*spacing_*1.9f*1.9f)continue;
                const auto region=field.sample(p).motionRegion;
                const auto slot=uint64_t((row+2)*5+col+3);
                const float threshold=.73f+.05f*stablePhase(slot);
                if(sameRegion(region,centerRegion)&&hasRoom(p,threshold*spacing_,region)&&add(p,field,BirthSite::Center,true))--centerBudget;
            }
        }
    }

    // Contraction too can leave local holes. Keep the small dedicated sink
    // clear, but let persistent gaps anywhere else compete for coverage.
    if(alignedTranslation||probes_.empty())return;
    struct Gap {Vec2 point;float distance,rate;bool predicted;uint8_t motionRegion;};
    std::vector<Gap> gaps;
    gaps.reserve(96);
    const float d2=spacing_*spacing_;
    const size_t scan=std::min<size_t>(probes_.size(),192);
    for(size_t n=0;n<scan;++n){
        const auto slot=probeCursor_;
        auto& probe=probes_[slot];
        probeCursor_=(probeCursor_+1)%probes_.size();
        const auto p=probe.position;
        const auto motion=field.sample(p);
        if(probe.motionRegion!=motion.motionRegion){probe.emptySince=-1;probe.motionRegion=motion.motionRegion;}
        if(sinkRadius>0&&distanceSquared(p,center)<sinkRadius*sinkRadius&&sameRegion(centerRegion,motion.motionRegion)){
            probe.emptySince=-1;
            continue;
        }
        float nearest=2.2f*2.2f*d2,predictedNearest=nearest;
        const auto rates=localAreaRates(field,p,spacing_*.55f,motion,liveTracking_);
        const float rate=rates.x+rates.y;
        const auto v=rate>.12f?meshVelocity(motion,height_):Vec2{};
        // Vary when similarly sized holes in the same expanding lattice start
        // qualifying. The current-distance guard still protects spacing.
        const float phase=stablePhase(slot+1);
        const float lookAhead=.30f+.30f*phase;
        const Vec2 ahead{p.x+v.x*lookAhead,p.y+v.y*lookAhead};
        nearby(p,2.2f*spacing_,[&](uint32_t i){
            const auto& node=nodes_[i];
            if(!occupiesSpace(node)||!sameRegion(node.motionRegion,motion.motionRegion))return;
            nearest=std::min(nearest,distanceSquared(p,node.position));
            if(rate>.12f)
                predictedNearest=std::min(predictedNearest,distanceSquared(ahead,
                    {node.position.x+node.velocity.x*lookAhead,node.position.y+node.velocity.y*lookAhead}));
        });
        const float threshold=rate>.12f?.80f:rate<-.12f?.90f:.88f;
        const bool open=nearest>=threshold*threshold*d2;
        const float earlyClearance=.68f+.032f*(1-phase);
        const bool opening=rate>.12f&&nearest>=earlyClearance*earlyClearance*d2&&
                           predictedNearest>=threshold*threshold*d2;
        if(!open&&!opening){probe.emptySince=-1;continue;}
        if(probe.emptySince<0)probe.emptySince=seconds_;
        const double delay=rate>.12f?.035+.065*stablePhase(slot+5):rate<-.12f?.17:.21;
        if(seconds_-probe.emptySince>=delay)gaps.push_back({p,nearest,rate,!open,motion.motionRegion});
    }
    std::stable_sort(gaps.begin(),gaps.end(),[](auto a,auto b){return a.distance>b.distance;});
    auto onScreen=[&](Vec2 p,uint8_t region){
        return p.x>=0&&p.x<=width_&&p.y>=0&&p.y<=height_&&
               (!sinkRadius||!sameRegion(centerRegion,region)||distanceSquared(p,center)>=sinkRadius*sinkRadius);
    };
    int interiorBudget=9;
    for(const auto& gap:gaps){
        if(!interiorBudget||free_.size()+nodeLimit_-nodes_.size()<22)break;
        const bool expanding=gap.rate>.12f;
        const float minGap=gap.predicted?.68f:expanding?.75f:.84f;
        if(!hasRoom(gap.point,minGap*spacing_,gap.motionRegion))continue;
        Vec2 candidate;
        if(findAlignedSite(gap.point,field,BirthSite::Interior,candidate)&&onScreen(candidate,gap.motionRegion)&&
           add(candidate,field,BirthSite::Interior,expanding)){
            --interiorBudget;
            continue;
        }
        if(gap.predicted||gap.distance<.98f*.98f*d2)continue;

        // Large gaps can lack a good pair of anchors. Continue in whole
        // triangular rows around the nearest reference site rather than
        // placing a point directly at each coverage probe.
        const int nearRow=int(std::lround((gap.point.y-latticeOrigin_.y)/step));
        for(int row=nearRow-1;row<=nearRow+1&&interiorBudget;++row){
            const float origin=latticeOrigin_.x+(row%2?.5f*spacing_:0);
            const int nearCol=int(std::lround((gap.point.x-origin)/spacing_));
            const Vec2 p{origin+nearCol*spacing_,latticeOrigin_.y+row*step};
            if(onScreen(p,gap.motionRegion)&&distanceSquared(p,gap.point)<1.12f*d2&&hasRoom(p,.73f*spacing_,gap.motionRegion)&&
               sameRegion(gap.motionRegion,field.sample(p).motionRegion)&&add(p,field,BirthSite::Interior,expanding))--interiorBudget;
        }
    }
}


void TriangleMesh::reconnect() {
    const float maxEdge=spacing_*meshEdgeReach,maxRadius=spacing_*1.49f;
    const float minArea=spacing_*spacing_*.045f;
    auto admissible=[&](const std::array<uint32_t,3>& ids,bool existing){
        const auto& a=nodes_[ids[0]];const auto& b=nodes_[ids[1]];const auto& c=nodes_[ids[2]];
        if(!joinsMesh(a)||!joinsMesh(b)||!joinsMesh(c))return false;
        if(!sameRegion(a.motionRegion,b.motionRegion)||!sameRegion(b.motionRegion,c.motionRegion)||
           !sameRegion(c.motionRegion,a.motionRegion))return false;
        if(!linkCompatible(a.velocity,b.velocity,existing)||!linkCompatible(b.velocity,c.velocity,existing)||
           !linkCompatible(c.velocity,a.velocity,existing))return false;
        const float limit=maxEdge*maxEdge;
        if(distanceSquared(a.position,b.position)>limit||distanceSquared(b.position,c.position)>limit||
           distanceSquared(c.position,a.position)>limit)return false;
        const auto circle=circumcircle(topologyPoint(a,spacing_),topologyPoint(b,spacing_),topologyPoint(c,spacing_),minArea);
        if(circle.radiusSquared<0){if(existing)++stats_.degenerate;return false;}
        if(circle.radiusSquared>maxRadius*maxRadius)return false;
        bool empty=true;
        nearby(circle.center,std::sqrt(circle.radiusSquared)+spacing_*.004f,[&](uint32_t index){
            const auto& neighbor=nodes_[index];
            if(empty&&index!=ids[0]&&index!=ids[1]&&index!=ids[2]&&joinsMesh(neighbor)&&
               sameRegion(a.motionRegion,neighbor.motionRegion)&&sameRegion(b.motionRegion,neighbor.motionRegion)&&
               sameRegion(c.motionRegion,neighbor.motionRegion)&&
               distanceSquared(topologyPoint(neighbor,spacing_),circle.center)<circle.radiusSquared-spacing_*spacing_*1.e-7f)
                empty=false;
        });
        return empty;
    };

    // Keep valid faces. Reconstruct only the holes whose local empty-circle
    // relation changed. When a quadrilateral changes its valid diagonal the
    // new adjacent faces implement the edge flip without moving any point.
    stats_.degenerate=0;
    const auto removed=std::erase_if(triangles_,[&](const auto& triangle){return !admissible(triangle.node,true);});
    std::vector<std::array<uint32_t,3>> present;
    present.reserve(triangles_.size()+96);
    for(const auto& triangle:triangles_)present.push_back(triangle.node);
    std::sort(present.begin(),present.end());
    std::vector<std::pair<float,uint32_t>> neighbors;
    neighbors.reserve(24);
    const size_t faceBudget=std::min<size_t>(96,nodeLimit_*2-triangles_.size());
    size_t added=0;
    auto rebuildAround=[&](uint32_t i,size_t budget){
        if(!joinsMesh(nodes_[i]))return;
        neighbors.clear();
        nearby(nodes_[i].position,maxEdge,[&](uint32_t j){
            const auto d=distanceSquared(nodes_[i].position,nodes_[j].position);
            if(i!=j&&joinsMesh(nodes_[j])&&d<=maxEdge*maxEdge&&sameRegion(nodes_[i].motionRegion,nodes_[j].motionRegion))
                neighbors.emplace_back(d,j);
        });
        constexpr size_t maxNeighbors=12;
        if(neighbors.size()>maxNeighbors){
            std::nth_element(neighbors.begin(),neighbors.begin()+maxNeighbors,neighbors.end());
            neighbors.resize(maxNeighbors);
        }
        for(size_t a=0;a<neighbors.size()&&added<budget;++a)
            for(size_t b=a+1;b<neighbors.size()&&added<budget;++b){
                std::array ids{i,neighbors[a].second,neighbors[b].second};
                std::sort(ids.begin(),ids.end());
                const auto at=std::lower_bound(present.begin(),present.end(),ids);
                if((at!=present.end()&&*at==ids)||!admissible(ids,false))continue;
                present.insert(at,ids);triangles_.push_back({ids});++added;
            }
    };
    const size_t priorityBudget=std::min<size_t>(56,faceBudget);
    for(auto i:newAnchors_){
        if(added>=priorityBudget)break;
        rebuildAround(i,priorityBudget);
    }
    newAnchors_.clear();
    for(size_t n=0;n<std::min<size_t>(128,nodes_.size())&&added<faceBudget;++n){
        const auto i=uint32_t(faceCursor_);
        faceCursor_=(faceCursor_+1)%nodes_.size();
        rebuildAround(i,faceBudget);
    }
    stats_.rewired+=removed+added;
    if(edgesEnabled_&&(removed||added))updateEdges();
    for(uint32_t i=0;i<nodes_.size();++i)if(nodes_[i].life==MeshLife::Retired){
        nodes_[i].life=MeshLife::Empty;
        free_.push_back(i);
    }
}

void TriangleMesh::setEdgesEnabled(bool enabled) {
    if(edgesEnabled_==enabled)return;
    edgesEnabled_=enabled;
    if(enabled)updateEdges();else edges_.clear();
}

void TriangleMesh::updateEdges() {
    edges_.clear();
    for(const auto& triangle:triangles_){
        const auto& n=triangle.node;
        edges_.push_back({n[0],n[1]});edges_.push_back({n[0],n[2]});edges_.push_back({n[1],n[2]});
    }
    std::sort(edges_.begin(),edges_.end(),[](auto a,auto b){return std::tie(a.first,a.second)<std::tie(b.first,b.second);});
    edges_.erase(std::unique(edges_.begin(),edges_.end(),[](auto a,auto b){
        return a.first==b.first&&a.second==b.second;
    }),edges_.end());
}

void TriangleMesh::countNodes() {
    stats_.triangles=triangles_.size();
    stats_.live=stats_.entering=stats_.leaving=0;
    for(const auto& node:nodes_){
        if(node.life==MeshLife::Active)++stats_.live;
        if(node.life==MeshLife::Entering)++stats_.entering;
        if(node.life==MeshLife::Leaving)++stats_.leaving;
    }
}

void TriangleMesh::advance(const MotionField& field,float seconds) {
    const auto& frame=field.frame();
    if(!nodeLimit_||frame.targetWidth!=width_||frame.targetHeight!=height_||!std::isfinite(seconds)||seconds<=0)return;
    const float elapsed=std::min(seconds,.05f),margin=buffer_;
    seconds_+=elapsed;
    auto timer=Clock::now();
    const float smoothing=1-std::exp(-elapsed/.048f);
    const float braking=1-std::exp(-elapsed/.030f);
    for(auto& node:nodes_){
        if(node.life==MeshLife::Empty||node.life==MeshLife::Retired)continue;
        const auto p=node.position,first=liveTracking_?node.velocity:fieldVelocity(field,p);
        auto sampled=field.sample({p.x+first.x*elapsed*.5f,p.y+first.y*elapsed*.5f});
        if(liveTracking_&&(sampled.boundary||sampled.motionRegion!=node.motionRegion)){
            const auto atNode=field.sample(p);
            if(atNode.motionRegion!=sampled.motionRegion||atNode.boundary)sampled=atNode;
        }
        node.motionRegion=sampled.motionRegion;
        const auto target=meshVelocity(sampled,height_);
        if(liveTracking_){
            const bool slowing=distanceSquared(target,{})<.12f*distanceSquared(first,{})||target.x*first.x+target.y*first.y<0;
            const float blend=slowing?braking:smoothing;
            node.velocity={std::lerp(first.x,target.x,blend),std::lerp(first.y,target.y,blend)};
            if(distanceSquared(node.velocity,{})<.0225f&&distanceSquared(target,{})<.0025f)node.velocity={};
        }else node.velocity=target;
        node.position={p.x+node.velocity.x*elapsed,p.y+node.velocity.y*elapsed};
        node.age+=elapsed;
        if(node.position.x<-margin||node.position.x>width_+margin||
           node.position.y<-margin||node.position.y>height_+margin){++stats_.edgeExits;retire(node);continue;}
        if(node.life==MeshLife::Entering){
            const float progress=std::clamp((node.age-node.fadeDelay)/node.fadeDuration,0.f,1.f);
            node.opacity=progress*progress*(3-2*progress);
            if(progress>=1)node.life=MeshLife::Active;
        } else if(node.life==MeshLife::Leaving){
            node.opacity=std::max(0.f,node.opacity-elapsed/fadeOutSeconds);
            if(node.opacity<=0)retire(node);
        }
    }
    stats_.movementMs=elapsedMs(timer);
    const Vec2 center{width_*.5f,height_*.5f};
    const auto centerSample=field.sample(center);
    const auto middle=meshVelocity(centerSample,height_);
    const auto [xRate,yRate]=localAreaRates(field,center,spacing_,centerSample,liveTracking_);
    // Detect simultaneous radial expansion or contraction. Pure shear and
    // rotation have no radial source/sink. The field itself still moves nodes.
    const bool radial=xRate*yRate>0&&std::abs(xRate-yRate)<.18f&&
        distanceSquared(middle,{})<32.f*32.f;
    const float radialRate=radial?(xRate+yRate)*.5f:0;
    const auto consistent=[&](Vec2 p){return distanceSquared(fieldVelocity(field,p),middle)<.25f;};
    if(referencePhaseValid_&&(!consistent({0,0})||!consistent({float(width_),0})||
       !consistent({0,float(height_)})||!consistent({float(width_),float(height_)})||
       !consistent({center.x-spacing_,center.y})||!consistent({center.x,center.y-spacing_})))
        referencePhaseValid_=false;
    if(referencePhaseValid_){
        translationPhase_.x=std::fmod(translationPhase_.x+middle.x*elapsed,spacing_);
        translationPhase_.y=std::fmod(translationPhase_.y+middle.y*elapsed,spacing_*1.7320508f);
    }

    timer=Clock::now();rebuildIndex();stats_.indexMs=elapsedMs(timer);
    const float clearRadius=centerClearRadius_>=0?centerClearRadius_:meshCenterClearRadius(width_,height_);
    const float sinkRadius=radialRate<-.10f?(centerFade_?std::max(4.f,clearRadius):4.f):0;
    timer=Clock::now();recycleCrowding(field,sinkRadius,elapsed);stats_.recycleMs=elapsedMs(timer);
    timer=Clock::now();replenish(field,referencePhaseValid_,radialRate,sinkRadius);stats_.birthMs=elapsedMs(timer);
    topologyTime_+=elapsed;stats_.topologyMs=0;
    if(topologyTime_>=.065f){
        timer=Clock::now();reconnect();stats_.topologyMs=elapsedMs(timer);
        topologyTime_-=.065f;
    }
    countNodes();
}

void TriangleMesh::stopVelocity() {
    for(auto& node:nodes_)node.velocity={};
    restartMotionChecks();
}

void TriangleMesh::restartMotionChecks() {
    for(auto& node:nodes_)node.crowdedFor=0;
    for(auto& probe:probes_)probe.emptySince=-1;
    topologyTime_=.065f;
}

namespace {
class CompensationField final : public MotionField {
    const MotionField* source_;
    MotionFieldFrame frame_{};
    float gain_;
public:
    CompensationField(const MotionField* source,int width,int height,float gain)
        : source_(source),gain_(gain) {
        if(source_&&source_->frame().targetWidth==width&&source_->frame().targetHeight==height)frame_=source_->frame();
        else source_=nullptr;
        frame_.targetWidth=width;frame_.targetHeight=height;
    }
    const MotionFieldFrame& frame() const override {return frame_;}
    FieldSample sample(Vec2 position) const override {
        if(!source_)return {};
        return compensateMeshSample(source_->sample(position),gain_);
    }
};
}

void LiveTriangleBackend::reset() {
    mesh_.seed(0,0,0);dots_.clear();depthCue_=0;
    width_=height_=0;spacing_=settle_=updateMs_=0;advancing_=false;
}

void LiveTriangleBackend::suspend() {
    if(!advancing_)return;
    mesh_.stopVelocity();
    advancing_=false;
}

const std::vector<DotVisual>& LiveTriangleBackend::update(int width,int height,const VisualSettings& settings,
                                                        const MotionField* source,float elapsed,float signedGain,bool moving,
                                                        float appearanceElapsed) {
    const auto started=Clock::now();
    const float spacing=liveMeshSpacing(width,height,settings.spacing);
    const bool reseed=width!=width_||height!=height_||spacing!=spacing_;
    if(reseed){
        width_=width;height_=height;spacing_=spacing;settle_=0;
        mesh_.seed(width,height,spacing);
    }
    const float dt=std::clamp(std::isfinite(elapsed)?elapsed:0.f,0.f,.05f);
    if(dt==0)suspend();
    const bool advance=!reseed&&dt>0&&(moving||settle_>0);
    if(advance){
        advancing_=true;
        settle_=moving?1.f:std::max(0.f,settle_-dt);
        mesh_.setCenterClearRadius(std::min(width,height)*settings.holeDiameter*.5f);
        // The field describes scene motion. Compensation uses the same sign
        // and baseline gain as the other dynamic styles.
        const CompensationField compensation(source,width,height,signedGain);
        mesh_.advance(compensation,dt);
    }
    const bool mapped=source&&source->frame().targetWidth==width&&source->frame().targetHeight==height;
    const float target=settings.meshDepth&&mapped?source->frame().depthDemand*settings.meshDepthStrength:0.f;
    const float previousDepth=depthCue_;
    if(!settings.meshDepth||std::abs(target-previousDepth)<=.001f)depthCue_=target;
    else {
        const float dt=std::clamp(std::isfinite(appearanceElapsed)?appearanceElapsed:0.f,0.f,.05f);
        const bool growing=target*previousDepth>=0&&std::abs(target)>std::abs(previousDepth);
        depthCue_=std::lerp(previousDepth,target,1.f-std::exp(-dt/(growing?.12f:.22f)));
    }
    const bool depthChanged=depthCue_!=previousDepth;
    const float depthAmount=std::min(1.f,1.3f*std::abs(depthCue_));
    if(!reseed&&!advance&&!depthChanged){updateMs_=0;return dots_;}
    dots_.clear();
    constexpr float margin=dotRadiusMax(TriangleStyle)*2;
    for(const auto& node:mesh_.nodes()){
        const auto p=node.position;
        if(node.opacity<=0||p.x<-margin||p.x>width+margin||p.y<-margin||p.y>height+margin)continue;
        DotVisual dot{p,node.opacity};
        if(depthAmount>0){
            // Aspect-normalized bands keep the neutral belt between the center
            // and all four edges. Evaluate the same envelope for every node.
            const float x=p.x*(2.f/width)-1.f,y=p.y*(2.f/height)-1.f;
            const float radiusSquared=x*x+y*y;
            const float inner=1.f-smoothUnit((radiusSquared-.30f*.30f)/(.49f*.49f-.30f*.30f));
            const float outer=smoothUnit((radiusSquared-.63f*.63f)/(.91f*.91f-.63f*.63f));
            dot.depthAmount=depthAmount;
            dot.depthCue=std::copysign(depthAmount,depthCue_)*(outer-inner);
            const float sizeStrength=dot.depthCue<0?.28f:.46f;
            dot.radiusScale=1.f+sizeStrength*dot.depthCue;
        }
        dots_.push_back(dot);
    }
    updateMs_=float(elapsedMs(started));
    return dots_;
}

}
