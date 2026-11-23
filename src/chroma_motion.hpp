#pragma once
#include "core.hpp"
#include <span>

namespace mc::detail {
void recoverChromaMotion(MotionSample& sample,int width,int height,float roundTripLimit,Vec2 pixelsPerSecond,
                         std::span<const uint16_t> previous,std::span<const uint16_t> current,const ChromaInputHint& input);
}
