#include "tracking.hpp"
#include <opencv2/imgproc.hpp>
#include <opencv2/video/tracking.hpp>
namespace mc {
namespace {
cv::Ptr<cv::DISOpticalFlow> makeEstimator() {
    auto estimator=cv::DISOpticalFlow::create(cv::DISOpticalFlow::PRESET_FAST);
    estimator->setUseMeanNormalization(true);
    estimator->setUseSpatialPropagation(true);
    return estimator;
}
float pixel(const cv::Mat& image,float x,float y) {
    int ix=int(x),iy=int(y);float u=x-ix,v=y-iy;
    return (1-v)*((1-u)*image.at<unsigned char>(iy,ix)+u*image.at<unsigned char>(iy,ix+1))
        +v*((1-u)*image.at<unsigned char>(iy+1,ix)+u*image.at<unsigned char>(iy+1,ix+1));
}
cv::Vec2f vectorAt(const cv::Mat& field,float x,float y) {
    int ix=int(x),iy=int(y);float u=x-ix,v=y-iy;
    return (1-v)*((1-u)*field.at<cv::Vec2f>(iy,ix)+u*field.at<cv::Vec2f>(iy,ix+1))
        +v*((1-u)*field.at<cv::Vec2f>(iy+1,ix)+u*field.at<cv::Vec2f>(iy+1,ix+1));
}
}
struct DenseFlowWorkspace::Data {
    cv::Ptr<cv::DISOpticalFlow> forward=makeEstimator(),backward=makeEstimator();
};
DenseFlowWorkspace::DenseFlowWorkspace():data_(std::make_unique<Data>()){}
DenseFlowWorkspace::~DenseFlowWorkspace()=default;
// OpenCV 4.12 DIS collectGarbage() clears its refinement processors; reusing
// that instance can dereference an empty vector. Recreate on capture resets.
void DenseFlowWorkspace::reset(){data_=std::make_unique<Data>();}
MotionSample estimateDenseFlow(const std::vector<float>& previous,const std::vector<float>& current,
    int w,int h,double dt,double timestamp,int sourceW,int sourceH,DenseFlowWorkspace* workspace) {
    MotionSample result;result.timestamp=timestamp;result.regionConfidence.fill(0);
    result.issue=MotionIssue::InvalidInput;result.denseFlow=true;
    if(std::isfinite(dt)&&dt>0)result.pairIntervalMs=dt*1000;
    if(w<80||h<60||previous.size()!=size_t(w)*h||current.size()!=previous.size()
        ||!std::isfinite(dt)||!std::isfinite(timestamp)||dt<.008||dt>.15||sourceW<=0||sourceH<=0)return result;
    for(size_t i=0;i<previous.size();++i){
        if(!std::isfinite(previous[i])||!std::isfinite(current[i]))return result;
        result.meanDelta+=std::abs(previous[i]-current[i]);
    }
    result.meanDelta/=float(previous.size());
    result.analysisWidth=w;result.analysisHeight=h;result.issue=MotionIssue::LowTexture;
    // Bounded, single-threaded CPU estimation. These are dense fields, not
    // propagated sparse observations. Fresh empty outputs avoid stale seeds.
    static const bool configured=[] {cv::setNumThreads(1);return true;}();(void)configured;
    cv::Mat a,b,forward,backward;
    cv::Mat(h,w,CV_32F,const_cast<float*>(previous.data())).convertTo(a,CV_8U,255);
    cv::Mat(h,w,CV_32F,const_cast<float*>(current.data())).convertTo(b,CV_8U,255);
    std::unique_ptr<DenseFlowWorkspace> local;
    if(!workspace){local=std::make_unique<DenseFlowWorkspace>();workspace=local.get();}
    workspace->data_->forward->calc(a,b,forward);
    workspace->data_->backward->calc(b,a,backward);
    result.densePixels=w*h;
    const float maxRoundTrip=std::max(1.5f,.8f*w/320.f);
    // Uniformly audit 16 locations per cell after dense estimation. Four
    // anchors remain explicit; no feature-count competition with HUD/vehicles.
    // Dense output alone is not evidence: flat patches and failed matching
    // remain invalid rather than becoming confident zero motion.
    for(int cell=0;cell<81;++cell){
        int left=cell%9*w/9,right=(cell%9+1)*w/9,top=cell/9*h/9,bottom=(cell/9+1)*h/9;
        for(int row=0;row<4;++row)for(int col=0;col<4;++col){
            int x=std::clamp(int(left+(col+.5f)*(right-left)/4),4,w-5);
            int y=std::clamp(int(top+(row+.5f)*(bottom-top)/4),4,h-5);
            bool anchor=(row==0||row==3)&&(col==0||col==3);
            auto& stats=result.cells[cell];++stats.samples;if(anchor)++stats.anchors;
            cv::Vec2f v=forward.at<cv::Vec2f>(y,x);float qx=x+v[0],qy=y+v[1];
            bool finite=std::isfinite(qx)&&std::isfinite(qy);
            bool inside=finite&&qx>=4&&qy>=4&&qx<w-5&&qy<h-5;
            float sum=0,squared=0,error=0,roundTrip=0;
            for(int dy=-2;dy<=2;++dy)for(int dx=-2;dx<=2;++dx){
                float value=a.at<unsigned char>(y+dy,x+dx);sum+=value;squared+=value*value;
                if(inside)error+=std::abs(value-pixel(b,qx+dx,qy+dy));
            }
            float variance=squared/25-(sum/25)*(sum/25);error/=25;
            bool textured=variance>=16;
            if(inside){auto reverse=vectorAt(backward,qx,qy);roundTrip=float(cv::norm(v+reverse));}
            bool photo=inside&&std::isfinite(error)&&error<=18;
            bool consistent=inside&&std::isfinite(roundTrip)&&roundTrip<=maxRoundTrip;
            bool valid=textured&&photo&&consistent;
            if(!textured)++result.rejectedTexture;
            else if(!photo)++result.rejectedPhotometric;
            else if(!consistent)++result.rejectedConsistency;
            // Invalid nonfinite values never escape into debug geometry.
            result.observations.push_back({{float(x),float(y)},finite?Vec2{v[0],v[1]}:Vec2{},
                inside?error:0,inside&&std::isfinite(roundTrip)?roundTrip:0,cell,valid,anchor});
            auto& observation=result.observations.back();observation.originalDisplacement=observation.displacement;observation.originalValid=valid;
            observation.failure=uint8_t((textured?0:1)|(photo?0:2)|(consistent?0:4));
            if(!textured)++stats.rejectedTexture;else if(!photo)++stats.rejectedPhoto;else if(!consistent)++stats.rejectedConsistency;
            ++result.features;if(valid){++result.tracked;++stats.tracked;}
        }
    }
    // Direct local observations only; no wider matching or parent borrowing.
    result.directEvidence=true;
    result=detail::aggregateFlow(std::move(result),w,h,dt,sourceW,sourceH,maxRoundTrip);
    return result;
}
}
