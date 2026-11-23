#pragma once
#include "core.hpp"

namespace mc {

struct MotionFieldFrame {
    double timestamp{};
    int sourceWidth{},sourceHeight{};
    int targetWidth{},targetHeight{};
    int viewX{},viewY{},viewWidth{},viewHeight{};
    int analysisWidth{},analysisHeight{};
    bool mappingKnown=true;
    double observationTimestamp{};
    // Signed strength of visually confirmed expansion (+) or contraction (-).
    float depthDemand{};
};

struct FieldSample {
    // Target pixels per second, in the direction of scene motion. +X right, +Y down.
    Vec2 velocity{};
    float confidence{};
    bool valid{};
    bool boundary{};
    // Zero indicates a field without an assigned motion region.
    uint8_t motionRegion{};
    // A negative value keeps the original confidence-based scaling.
    float motionWeight=-1.f;
};

class MotionField {
public:
    virtual ~MotionField() = default;
    virtual const MotionFieldFrame& frame() const = 0;
    virtual FieldSample sample(Vec2 targetPosition) const = 0;
};

}
