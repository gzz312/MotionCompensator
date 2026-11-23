#pragma once
#include "motion_field.hpp"
#include "dynamic.hpp"

namespace mc {

enum class FieldOrigin { Missing, Measured, CameraModel, Background, Neighbors, Held, Input, Stationary };

struct ScreenFieldStats {
    int measured{},modeled{},held{},input{},stationary{},boundaries{},backgroundSupport{},foregroundSuppressed{},independentCells{},preservedMotionCells{};
    int candidateMovingCells{},candidateDirectionCells{},candidateSpeedMatches{},boundedBackgroundCells{};
    int unobservedCells{},quietObservedCells{},neighborFilledCells{},slowDirectCells{};
    std::array<int,3> unobservedByBand{},quietByBand{},neighborByBand{};
    bool backgroundActive{},backgroundHeld{};
    bool moving{};
};

// Owned by the UI thread. update() consumes an immutable capture snapshot.
class ScreenMotionField final : public MotionField {
    struct Cell { Vec2 velocity{}; float confidence{}; FieldOrigin origin{}; uint8_t motionRegion{}; Vec2 localCorrection{}; float backgroundScale{},motionWeight{}; };
    struct Remembered { Vec2 velocity{}; float confidence{}; double stamp{}; };
    std::array<Cell,81> cells_{};
    std::array<std::array<bool,4>,81> compatibleNeighbors_{};
    std::array<Remembered,81> previous_{};
    std::array<uint8_t,81> independentRegions_{},independentVotes_{};
    std::array<float,81> backgroundScales_{};
    std::array<BackgroundCorrection,81> neighborFill_{};
    int backgroundSupport_{};
    int candidateMovingCells_{},candidateDirectionCells_{},candidateSpeedMatches_{};
    MotionFieldFrame frame_{};
    ScreenFieldStats stats_{};
    AffineFlow camera_{},background_{};
    Vec2 backgroundLook_{};
    float backgroundConfidence_{};
    double backgroundStamp_{};
    double depthStamp_{};
    float depthDemand_{};
    double lastSourceTimestamp_{};
    bool backgroundPriority_{};
    bool neighborUsesBackground_{};
    void classifyBackground(const MotionSample& source);
    void fillNearMissing(const MotionSample& source,bool useBackground);
public:
    void reset();
    void update(const MotionSample& source,const FieldIntent& intent,MotionFieldFrame mapping,double now,
                bool enabled,bool stationary,bool backgroundPriority=false,const ScreenFixedMotion* screenFixed=nullptr,
                bool depthEnabled=false);
    const MotionFieldFrame& frame() const override {return frame_;}
    FieldSample sample(Vec2 targetPosition) const override;
    const ScreenFieldStats& stats() const {return stats_;}
    const AffineFlow& cameraModel() const {return camera_;}
};

}
