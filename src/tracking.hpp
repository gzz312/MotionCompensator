#pragma once
#include "core.hpp"
#include <vector>
#include <memory>
namespace mc {
class TrackingWorkspace;
MotionSample estimateTrackedFlow(const std::vector<float>& previous,const std::vector<float>& current,
    int width,int height,double dt,double timestamp,int sourceWidth,int sourceHeight,
    TrackingWorkspace* workspace=nullptr,int extraFeaturesPerCell=8);
class TrackingWorkspace {
    struct Data;std::unique_ptr<Data> data_;
    friend MotionSample estimateTrackedFlow(const std::vector<float>&,const std::vector<float>&,int,int,double,double,int,int,TrackingWorkspace*,int);
public:
    TrackingWorkspace();~TrackingWorkspace();
    TrackingWorkspace(const TrackingWorkspace&)=delete;
    TrackingWorkspace& operator=(const TrackingWorkspace&)=delete;
    void reset();
};
// Shared spatial aggregation keeps sparse regression and dense production
// results in exactly the same source-pixel units and nine display regions.
namespace detail {
MotionSample aggregateFlow(MotionSample result,int width,int height,double dt,
    int sourceWidth,int sourceHeight,float maxRoundTrip);

}
class DenseFlowWorkspace {
    struct Data;std::unique_ptr<Data> data_;
    friend MotionSample estimateDenseFlow(const std::vector<float>&,const std::vector<float>&,int,int,double,double,int,int,DenseFlowWorkspace*);
public:
    DenseFlowWorkspace();~DenseFlowWorkspace();
    DenseFlowWorkspace(const DenseFlowWorkspace&)=delete;
    DenseFlowWorkspace& operator=(const DenseFlowWorkspace&)=delete;
    void reset();
};
MotionSample estimateDenseFlow(const std::vector<float>& previous,const std::vector<float>& current,
    int width,int height,double dt,double timestamp,int sourceWidth,int sourceHeight,
    DenseFlowWorkspace* workspace=nullptr);

}
