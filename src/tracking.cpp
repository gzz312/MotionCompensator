#include "tracking.hpp"
#include "motion_evidence.hpp"
#include <opencv2/imgproc.hpp>
#include <opencv2/video/tracking.hpp>
#include <random>
#include <numeric>
#include <cstring>
namespace mc {
namespace {
struct Track {cv::Point2f p,q;int region;};
using Model=std::array<double,6>;
cv::Point2f project(const Model& m,cv::Point2f p) {return {float(m[0]*p.x+m[1]*p.y+m[2]),float(m[3]*p.x+m[4]*p.y+m[5])};}
bool solve3(double a[3][4],std::array<double,3>& out) {
    for(int k=0;k<3;++k) {
        int row=k;for(int j=k+1;j<3;++j) if(std::abs(a[j][k])>std::abs(a[row][k])) row=j;
        if(std::abs(a[row][k])<1e-8) return false;
        for(int j=k;j<4;++j) std::swap(a[k][j],a[row][j]);
        double pivot=a[k][k];for(int j=k;j<4;++j) a[k][j]/=pivot;
        for(int i=0;i<3;++i) if(i!=k) {double v=a[i][k];for(int j=k;j<4;++j) a[i][j]-=v*a[k][j];}
    }
    for(int i=0;i<3;++i) out[i]=a[i][3];
    return true;
}
bool fit(const std::vector<Track>& tracks,const std::vector<int>& selected,Model& model) {
    double x[3][4]{},y[3][4]{};
    for(int i:selected) {
        auto t=tracks[i];double basis[]{t.p.x,t.p.y,1};
        for(int r=0;r<3;++r) {for(int c=0;c<3;++c) x[r][c]+=basis[r]*basis[c];x[r][3]+=basis[r]*t.q.x;y[r][3]+=basis[r]*t.q.y;}
    }
    for(int r=0;r<3;++r) for(int c=0;c<3;++c) y[r][c]=x[r][c];
    std::array<double,3> a{},b{};if(!solve3(x,a) || !solve3(y,b)) return false;
    model={a[0],a[1],a[2],b[0],b[1],b[2]};return true;
}
}
struct TrackingWorkspace::Data {cv::Mat previousImage;std::vector<cv::Mat> pyramid;int window=0;};
TrackingWorkspace::TrackingWorkspace():data_(std::make_unique<Data>()){}
TrackingWorkspace::~TrackingWorkspace()=default;
void TrackingWorkspace::reset(){*data_=Data{};}
MotionSample estimateTrackedFlow(const std::vector<float>& previous,const std::vector<float>& current,
    int w,int h,double dt,double timestamp,int sourceW,int sourceH,TrackingWorkspace* workspace,int extraFeaturesPerCell) {
    MotionSample result;result.timestamp=timestamp;result.regionConfidence.fill(0);
    result.issue=MotionIssue::InvalidInput;
    if(w<80 || h<60 || previous.size()!=size_t(w)*h || current.size()!=previous.size()
        || !std::isfinite(dt) || !std::isfinite(timestamp) || dt<.008 || dt>.15 || sourceW<=0 || sourceH<=0) return result;
    for(size_t i=0;i<previous.size();++i) {
        if(!std::isfinite(previous[i]) || !std::isfinite(current[i])) return result;
        result.meanDelta+=std::abs(previous[i]-current[i]);
    }
    result.meanDelta/=float(previous.size());
    // One worker, no OpenCV thread pool competing with the game's renderer.
    static const bool configured=[] {cv::setNumThreads(1);return true;}();(void)configured;
    cv::Mat a,b;
    cv::Mat(h,w,CV_32F,const_cast<float*>(previous.data())).convertTo(a,CV_8U,255);
    cv::Mat(h,w,CV_32F,const_cast<float*>(current.data())).convertTo(b,CV_8U,255);

    result.analysisWidth=w;result.analysisHeight=h;
    int extra=std::clamp(extraFeaturesPerCell,4,8);result.trackingExtraFeatures=extra;
    const float scale=std::max(1.f,w/320.f);
    std::vector<cv::Point2f> points;std::vector<int> pointCells;std::vector<bool> anchors;
    auto add=[&](cv::Point2f p,int cell,bool anchor){
        p.x=std::clamp(p.x,3.f,float(w-4));p.y=std::clamp(p.y,3.f,float(h-4));
        points.push_back(p);pointCells.push_back(cell);anchors.push_back(anchor);
        ++result.cells[cell].samples;if(anchor)++result.cells[cell].anchors;
    };
    // Every cell owns four sampling positions, even when texture is absent.
    // Failed anchors remain explicitly invalid; quotas never invent evidence.
    for(int cell=0;cell<81;++cell){
        int col=cell%9,row=cell/9;
        int left=col*w/9,right=(col+1)*w/9,top=row*h/9,bottom=(row+1)*h/9;
        for(float y:{.25f,.75f})for(float x:{.25f,.75f})add({left+x*(right-left),top+y*(bottom-top)},cell,true);
        std::vector<cv::Point2f> candidates;
        if(right-left>=5 && bottom-top>=5)cv::goodFeaturesToTrack(a(cv::Rect(left,top,right-left,bottom-top)),candidates,extra+4,.012,std::max(3.f,2.f*scale),cv::noArray(),5);
        int added=0;
        for(auto local:candidates){
            cv::Point2f p=local+cv::Point2f(float(left),float(top));
            if(p.x<3 || p.y<3 || p.x>=w-3 || p.y>=h-3)continue;
            bool close=false;for(size_t i=points.size()-size_t(4+added);i<points.size();++i)if(cv::norm(points[i]-p)<std::max(2.f,1.5f*scale)){close=true;break;}
            if(close)continue;
            add(p,cell,false);if(++added==extra)break;
        }
    }
    result.features=int(points.size());result.issue=MotionIssue::LowTexture;
    std::vector<cv::Point2f> forward,backward;std::vector<unsigned char> ok,backOk;std::vector<float> err,backErr;
    int window=std::clamp(int(std::round(15*scale))|1,21,41);
    auto criteria=cv::TermCriteria(cv::TermCriteria::COUNT|cv::TermCriteria::EPS,25,.01);
    // Share pyramids across forward/backward validation instead of building
    // both images twice. Three scales retain context without mixing the
    // entire screen into a local tracking window.
    std::vector<cv::Mat> pyramidA,pyramidB;
    auto cache=workspace?workspace->data_.get():nullptr;
    bool reuse=cache&&cache->window==window&&cache->previousImage.size()==a.size()
        &&cache->previousImage.type()==a.type()&&cache->previousImage.isContinuous()&&a.isContinuous()
        &&!cache->pyramid.empty()&&std::memcmp(cache->previousImage.data,a.data,a.total()*a.elemSize())==0;
    if(reuse){pyramidA=cache->pyramid;result.pyramidReused=true;}
    else cv::buildOpticalFlowPyramid(a,pyramidA,cv::Size(window,window),2);
    cv::buildOpticalFlowPyramid(b,pyramidB,cv::Size(window,window),2);
    if(cache){cache->previousImage=b;cache->pyramid=pyramidB;cache->window=window;}
    // Low-resolution phase correlation supplies only a search starting point
    // for fast coherent pans. Local LK and round-trip checks still decide
    // every vector; the seed never becomes an observation by itself.
    int coarseW=std::min(w,320),coarseH=std::min(h,180);
    cv::Mat coarseA,coarseB;static thread_local cv::Mat hanning;
    cv::resize(cv::Mat(h,w,CV_32F,const_cast<float*>(previous.data())),coarseA,cv::Size(coarseW,coarseH),0,0,cv::INTER_AREA);
    cv::resize(cv::Mat(h,w,CV_32F,const_cast<float*>(current.data())),coarseB,cv::Size(coarseW,coarseH),0,0,cv::INTER_AREA);
    if(hanning.size()!=coarseA.size())cv::createHanningWindow(hanning,coarseA.size(),CV_32F);
    double response=0;
    auto shift=cv::phaseCorrelate(coarseA,coarseB,hanning,&response);
    bool seeded=response>.18 && std::isfinite(shift.x) && std::isfinite(shift.y) && std::abs(shift.x)<coarseW*.25 && std::abs(shift.y)<coarseH*.25;
    if(seeded){for(auto point:points)forward.push_back(point+cv::Point2f(float(shift.x*w/coarseW),float(shift.y*h/coarseH)));backward=points;}
    int flags=seeded?cv::OPTFLOW_USE_INITIAL_FLOW:0;
    cv::calcOpticalFlowPyrLK(pyramidA,pyramidB,points,forward,ok,err,cv::Size(window,window),2,criteria,flags,1e-4);
    cv::calcOpticalFlowPyrLK(pyramidB,pyramidA,forward,backward,backOk,backErr,cv::Size(window,window),2,criteria,flags,1e-4);
    std::vector<Track> tracks;std::array<std::vector<int>,81> cellTracks;
    float maxRoundTrip=.8f*scale;
    for(size_t i=0;i<points.size();++i){
        auto p=points[i],q=forward[i];float roundTrip=float(cv::norm(backward[i]-p));
        bool valid=ok[i] && backOk[i] && std::isfinite(q.x) && std::isfinite(q.y) && std::isfinite(roundTrip) && std::isfinite(err[i]) && err[i]<=22 && roundTrip<=maxRoundTrip && q.x>=3 && q.x<w-3 && q.y>=3 && q.y<h-3;
        result.observations.push_back({{p.x,p.y},{q.x-p.x,q.y-p.y},std::isfinite(err[i])?err[i]:0,std::isfinite(roundTrip)?roundTrip:0,pointCells[i],valid,anchors[i]});
        if(!valid)continue;
        cellTracks[pointCells[i]].push_back(int(i));++result.cells[pointCells[i]].tracked;
        tracks.push_back({p,q,pointCells[i]/9/3*3+(pointCells[i]%9)/3});
    }
    result.tracked=int(tracks.size());
    return detail::aggregateFlow(std::move(result),w,h,dt,sourceW,sourceH,maxRoundTrip);
}
namespace detail {
MotionSample aggregateFlow(MotionSample result,int w,int h,double dt,int sourceW,int sourceH,float maxRoundTrip) {
    std::array<std::vector<int>,81> cellTracks;
    for(size_t i=0;i<result.observations.size();++i)if(result.observations[i].valid)
        cellTracks[result.observations[i].cell].push_back(int(i));
    const float scale=std::max(1.f,w/320.f);
    const float sx=float(sourceW)/w/float(dt),sy=float(sourceH)/h/float(dt);
    for(int cell=0;cell<81;++cell){
        auto& out=result.cells[cell];const auto& ids=cellTracks[cell];if(ids.size()<2)continue;
        // Dominant local cluster, not a full-image affine constraint. A moving
        // foreground object in one cell cannot invalidate every other cell.
        int best=-1;float bestScore=-1;std::vector<int> members;
        for(int index:ids){
            auto v=result.observations[index].displacement;float tolerance=1.25f*scale+.08f*std::hypot(v.x,v.y),score=0;
            for(int other:ids){auto u=result.observations[other].displacement;if(std::hypot(u.x-v.x,u.y-v.y)<=tolerance)score+=1;}
            if(score>bestScore){bestScore=score;best=index;}
        }
        auto center=result.observations[best].displacement;float tolerance=1.25f*scale+.08f*std::hypot(center.x,center.y);
        Vec2 sum{};float quality=0;
        for(int i:ids){const auto& obs=result.observations[i];if(std::hypot(obs.displacement.x-center.x,obs.displacement.y-center.y)>tolerance)continue;
            members.push_back(i);sum.x+=obs.displacement.x;sum.y+=obs.displacement.y;quality+=1-.2f*std::min(1.f,obs.roundTripError/maxRoundTrip);
        }
        out.inliers=int(members.size());if(out.inliers<2)continue;
        out.velocity={sum.x/out.inliers*sx,sum.y/out.inliers*sy};out.smoothed=out.velocity;
        out.confidence=std::min(1.f,out.inliers/4.f)*(float(out.inliers)/ids.size())*(quality/out.inliers);
        result.inliers+=out.inliers;
        if(result.directEvidence){
            out.directVelocity=out.velocity;out.directConfidence=out.confidence;out.directInliers=out.inliers;
        }
    }
    result.observedCells=0;for(const auto& cell:result.cells)if(cell.confidence>=.65f)++result.observedCells;
    // Fit whole-background fields from one vote per reliable analysis cell.
    // Texture-rich weapons/HUD must not gain extra votes from more keypoints.
    std::vector<Track> balanced,moving;
    for(int cell=0;cell<81&&!result.denseFlow;++cell){const auto& c=result.cells[cell];if(c.confidence<.65f)continue;
        cv::Point2f p((cell%9+.5f)*w/9,(cell/9+.5f)*h/9),v(c.velocity.x/sx,c.velocity.y/sy);
        Track track{p,p+v,(cell/9/3)*3+(cell%9)/3};balanced.push_back(track);
        if(cv::norm(v)>.18) moving.push_back(track);
    }
    auto fitBackground=[&](const std::vector<Track>& candidates,Model& fitted,int& support,std::array<int,9>& regionalSupport){
        if(candidates.size()<12)return 0.f;
        std::mt19937 random(7251);std::vector<int> best;float bestScore=-1;
        for(int attempt=0;attempt<160;++attempt){
            std::vector<int> selected;while(selected.size()<3){int i=int(random()%candidates.size());if(std::find(selected.begin(),selected.end(),i)==selected.end())selected.push_back(i);}
            Model candidate{};if(!fit(candidates,selected,candidate))continue;
            std::vector<int> inliers;std::array<bool,9> regions{};float minX=float(w),maxX=0,minY=float(h),maxY=0;
            for(size_t i=0;i<candidates.size();++i)if(cv::norm(project(candidate,candidates[i].p)-candidates[i].q)<=.9*scale){
                inliers.push_back(int(i));auto t=candidates[i];regions[t.region]=true;minX=std::min(minX,t.p.x);maxX=std::max(maxX,t.p.x);minY=std::min(minY,t.p.y);maxY=std::max(maxY,t.p.y);
            }
            int coverage=int(std::count(regions.begin(),regions.end(),true));
            if(coverage<5 || maxX-minX<w*.45f || maxY-minY<h*.35f)continue;
            float score=float(inliers.size())+coverage*.5f;
            if(score>bestScore){bestScore=score;best=std::move(inliers);}
        }
        if(best.size()<12 || !fit(candidates,best,fitted))return 0.f;
        double determinant=fitted[0]*fitted[4]-fitted[1]*fitted[3];if(determinant<.65 || determinant>1.5)return 0.f;
        support=int(best.size());for(int i:best)++regionalSupport[candidates[i].region];
        return std::min(1.f,support/18.f)*std::min(1.f,float(support)/candidates.size()/.6f);
    };
    Model model{},movingModel{};
    result.globalConfidence=fitBackground(balanced,model,result.globalSupportCells,result.globalRegionSupport);
    result.movingBackgroundConfidence=fitBackground(moving,movingModel,result.movingBackgroundSupportCells,result.movingRegionSupport);
    bool globalValid=result.globalConfidence>=.65f;
    for(int r=0;r<9;++r){
        cv::Point2f p((r%3+.5f)*w/3,(r/3+.5f)*h/3);
        if(globalValid){auto v=project(model,p)-p;result.globalVelocity[r]={v.x*sx,v.y*sy};}
        if(result.movingBackgroundConfidence>=.60f){auto v=project(movingModel,p)-p;result.movingBackgroundVelocity[r]={v.x*sx,v.y*sy};}
    }
    auto distance=[](Vec2 p,Vec2 q){return std::hypot(p.x-q.x,p.y-q.y);};
    for(int cell=0;cell<81;++cell){
        auto& out=result.cells[cell];Vec2 sum{};float weight=0;bool own=out.confidence>=.65f;
        if(result.denseFlow&&!own)continue;
        if(own){sum=out.velocity;weight=1;}
        for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){
            if(dx==0 && dy==0)continue;
            int x=cell%9+dx,y=cell/9+dy;if(x<0||x>=9||y<0||y>=9)continue;
            const auto& other=result.cells[y*9+x];if(other.confidence<.65f)continue;
            float affinity=1;
            if(own){float tolerance=std::max(60.f,.2f*(std::hypot(out.velocity.x,out.velocity.y)+std::hypot(other.velocity.x,other.velocity.y)));float delta=distance(out.velocity,other.velocity);if(delta>tolerance*2)continue;affinity=std::exp(-delta*delta/(tolerance*tolerance));}
            float factor=(own?.06f:1.f)*other.confidence*affinity/(dx*dx+dy*dy);sum.x+=other.velocity.x*factor;sum.y+=other.velocity.y*factor;weight+=factor;
        }
        if(weight>0){out.smoothed={sum.x/weight,sum.y/weight};if(!own)out.inferred=true;}
        else if(globalValid){auto p=cv::Point2f((cell%9+.5f)*w/9,(cell/9+.5f)*h/9);auto v=project(model,p)-p;out.smoothed={v.x*sx,v.y*sy};out.inferred=true;}
    }
    const auto evidence=spatialEvidence(result);
    std::vector<float> qualities;
    for(int region=0;region<9;++region){
        int col=region%3,row=region/3;std::vector<float> xs,ys;
        for(int y=row*3;y<row*3+3;++y)for(int x=col*3;x<col*3+3;++x){const auto& c=result.cells[y*9+x];if(c.confidence>=.65f){xs.push_back(c.smoothed.x);ys.push_back(c.smoothed.y);}}
        if(xs.empty())continue;
        // Keep a spatially supported moving layer when fixed foreground
        // dominates a region's median; do not choose arbitrary fastest points.
        bool movingLayer=evidence.broadMotion&&evidence.regionalMotion[region]>=3;
        if(movingLayer){xs.clear();ys.clear();
            for(int y=row*3;y<row*3+3;++y)for(int x=col*3;x<col*3+3;++x)if(evidence.connected[y*9+x]){
                xs.push_back(result.cells[y*9+x].smoothed.x);ys.push_back(result.cells[y*9+x].smoothed.y);}}
        auto median=[](std::vector<float> values){std::sort(values.begin(),values.end());return values[values.size()/2];};
        Vec2 reference{median(xs),median(ys)},sum{};float weight=0,ownWeight=0,quality=0;int ownCells=0;
        // Main 3x3 group plus a soft one-cell border into adjacent regions.
        for(int y=std::max(0,row*3-1);y<std::min(9,row*3+4);++y)for(int x=std::max(0,col*3-1);x<std::min(9,col*3+4);++x){
            const auto& c=result.cells[y*9+x];bool inside=x>=col*3&&x<col*3+3&&y>=row*3&&y<row*3+3;if(c.confidence<.65f)continue;
            float tolerance=std::max(90.f,.45f*std::hypot(reference.x,reference.y));float delta=distance(reference,c.smoothed);float robust=1/(1+delta*delta/(tolerance*tolerance));
            float factor=c.confidence*robust*(inside?1.f:.12f);
            if(movingLayer&&!evidence.connected[y*9+x])factor*=.03f;
            sum.x+=c.smoothed.x*factor;sum.y+=c.smoothed.y*factor;weight+=factor;
            if(inside){quality+=c.confidence*factor;ownWeight+=factor;++ownCells;}
        }
        if(weight>0 && ownWeight>0){result.velocity[region]={sum.x/weight,sum.y/weight};result.regionConfidence[region]=quality/ownWeight*std::min(1.f,ownCells/4.f);if(result.regionConfidence[region]>=.65f)qualities.push_back(result.regionConfidence[region]);}
    }
    if(qualities.size()>=4){std::sort(qualities.begin(),qualities.end());result.confidence=qualities[qualities.size()/2];result.issue=MotionIssue::Valid;}
    else result.issue=result.tracked<12?MotionIssue::LowTexture:MotionIssue::LowCoverage;
    return result;
}
}
}
