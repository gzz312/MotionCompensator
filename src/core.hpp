#pragma once
#include <algorithm>
#include <cmath>
#include <array>
#include <cstdint>
#include <string>
#include <vector>
namespace mc {
inline constexpr int ReticleSizeMin=4,ReticleSizeMax=64;
inline constexpr int dotRadiusMax(int style){return style==1?64:24;}
struct Vec2 { float x{}, y{}; };
enum class MotionIssue { Warming, Valid, InvalidInput, LowTexture, TrackingFailed, Inconsistent, LowCoverage };
struct FlowObservation {
    Vec2 position{},displacement{}; // Analysis pixels, before output scaling.
    float error=0,roundTripError=0;
    int cell=0;
    bool valid=false,anchor=false;
    Vec2 originalDisplacement{};
    bool originalValid=false;
    uint8_t failure=0; // Rejection bits: texture/photo/consistency.
};
struct FlowCell {
    Vec2 velocity{},smoothed{}; // Source pixels/second; raw and neighbourhood result.
    float confidence=0;
    int samples=0,anchors=0,tracked=0,inliers=0;
    bool inferred=false;
    Vec2 directVelocity{};
    float directConfidence=0;
    int directInliers=0;
    int rejectedTexture=0,rejectedPhoto=0,rejectedConsistency=0;

};
struct MotionSample {
    std::array<Vec2, 9> velocity{}; double timestamp{},completedTimestamp{}; float confidence{};
    std::array<float,9> regionConfidence{1,1,1,1,1,1,1,1,1};
    MotionIssue issue=MotionIssue::Warming;
    int features=0,tracked=0,inliers=0;
    float meanDelta=0;
    std::array<FlowCell,81> cells{};
    std::vector<FlowObservation> observations;
    int analysisWidth=0,analysisHeight=0,observedCells=0;
    std::array<Vec2,9> globalVelocity{},movingBackgroundVelocity{};
    float globalConfidence=0,movingBackgroundConfidence=0;
    int globalSupportCells=0,movingBackgroundSupportCells=0;
    std::array<int,9> globalRegionSupport{},movingRegionSupport{};
    int trackingExtraFeatures=0;
    bool pyramidReused=false;
    bool denseFlow=false;
    int densePixels=0,rejectedTexture=0,rejectedPhotometric=0,rejectedConsistency=0;
    double pairIntervalMs=0;
    bool directEvidence=false;

};
inline bool motionFresh(const MotionSample& sample,double now) {
    if(!std::isfinite(now)||!std::isfinite(sample.timestamp)||sample.timestamp<=0||now<sample.timestamp)return false;
    // Legacy/test samples have no completion stamp. Live results must satisfy
    // both a strict source-age cap and a short time since computation finished.
    if(sample.completedTimestamp==0)return now-sample.timestamp<=.1;
    return std::isfinite(sample.completedTimestamp) && sample.completedTimestamp>=sample.timestamp && now>=sample.completedTimestamp
        && now-sample.timestamp<=.2 && now-sample.completedTimestamp<=.12;
}
inline bool regionalFlowEvidence(const MotionSample& sample) {
    // Legacy producers report one whole-frame quality. The spatial tracker
    // additionally publishes measured regional evidence even at low coverage.
    return (std::isfinite(sample.confidence)&&sample.confidence>=.65f)
        || (sample.analysisWidth>0&&sample.analysisHeight>0&&sample.features>=sample.tracked
            &&sample.tracked>0&&sample.observedCells>0
            &&sample.issue!=MotionIssue::InvalidInput&&sample.issue!=MotionIssue::Warming);
}
inline bool regionMotionTrusted(const MotionSample& sample,int region,double now) {
    if(region<0||region>=9||!motionFresh(sample,now)||!regionalFlowEvidence(sample))return false;
    auto v=sample.velocity[region];float q=sample.regionConfidence[region];
    return std::isfinite(q)&&q>=.65f&&std::isfinite(v.x)&&std::isfinite(v.y);
}
inline bool anyRegionMotionTrusted(const MotionSample& sample,double now) {
    for(int region=0;region<9;++region)if(regionMotionTrusted(sample,region,now))return true;
    return false;
}
// All timestamps use QPC seconds. Future estimators publish the latest sample only.
class MotionFilter {
    std::array<Vec2, 9> offsets_{};
public:
    const auto& update(const MotionSample& sample, double now, float dt, bool dynamic) {
        bool valid = dynamic && std::isfinite(sample.confidence) && sample.confidence >= .65f
            && motionFresh(sample,now);
        float alpha = 1.f - std::exp(-std::clamp(std::isfinite(dt) ? dt : 0.f, 0.f, .1f) / .08f);
        for (size_t i=0; i<offsets_.size(); ++i) {
            auto v = sample.velocity[i];
            Vec2 target{};
            if (valid && std::isfinite(v.x) && std::isfinite(v.y)
                && std::isfinite(sample.regionConfidence[i]) && sample.regionConfidence[i]>=.65f)
                target = {std::clamp(-v.x * .04f, -24.f, 24.f), std::clamp(-v.y * .04f, -24.f, 24.f)};
            offsets_[i].x += alpha * (target.x - offsets_[i].x);
            offsets_[i].y += alpha * (target.y - offsets_[i].y);
        }
        return offsets_;
    }
    void reset() { offsets_ = {}; }
};
struct Color { float r=1,g=1,b=1; bool operator==(const Color&) const = default; };
struct VisualSettings {
    bool enhancement=true;
    bool staggered=false;
    bool boundaryFade=true;
    int appearance=1; // fixed, brightness, outline, combined
    int style=0,edges=0; // regional, screen-edge, static; left/right, top/bottom, all
    float edgeWidth=140;
    float radius=3.f,opacity=1.f,spacing=122.f;
    Color dots{},reticle{.2f,1.f,.45f};
    float reticleOpacity=1.f,reticleSize=10.f,holeDiameter=.10f;
    int reticleShape=1; // stable shape IDs are defined in reticle.hpp
    bool operator==(const VisualSettings&) const = default;
};
inline float dotVisibility(float x,float y,float w,float h,float radius,float holeDiameter) {
    float dx=x-w*.5f,dy=y-h*.5f;
    float hole=std::min(w,h)*holeDiameter*.5f;
    float distance=std::sqrt(dx*dx+dy*dy);
    if(distance<=hole+radius) return 0;
    float fade=std::clamp((distance-hole-radius)/(std::min(w,h)*.08f),0.f,1.f);
    fade=fade*fade*(3-2*fade);
    float edge=std::clamp(std::sqrt(dx*dx/(w*w*.25f)+dy*dy/(h*h*.25f)),0.f,1.f);
    return fade*(.12f+.88f*edge*edge);
}
inline bool knownGame(const std::wstring& name) {
    return name==L"payday2_win32_release.exe" || name==L"payday2_win64_release.exe" || name==L"payday2.exe"
        || name==L"minecraft.windows.exe" || name==L"portal.exe" || name==L"portal2.exe"
        || name==L"sb-win64-shipping.exe" || name==L"forzahorizon6.exe";
}
}
