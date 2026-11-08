#pragma once
#include "core.hpp"
#include <vector>
namespace mc {
MotionSample estimateTrackedFlow(const std::vector<float>& previous,const std::vector<float>& current,
    int width,int height,double dt,double timestamp,int sourceWidth,int sourceHeight);
}
