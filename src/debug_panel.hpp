#pragma once
#include "core.hpp"
#include "motion_continuity.hpp"
#include <sstream>
#include <iomanip>
namespace mc {
struct DebugSnapshot {
    std::array<Vec2,9> input{},fused{},compensation{},selectedFlow{};
    std::array<float,9> selectedConfidence{};
    float backgroundBlend=0;
    bool hasSelectedFlow=false;
    MotionState motionState=MotionState::Unknown;int heldRegions=0;
    MotionSample flow{};
    double now=0,captureAgeMs=-1,analysisMs=-1,fusionMs=-1,submitMs=-1,tickMs=-1;
    double captureFps=-1,analysisFps=-1,renderFps=-1;
    bool staggered=false;
    int style=0;
};
inline std::wstring formatDebug(const DebugSnapshot& d) {
    std::wostringstream s;s<<std::fixed<<std::setprecision(1);
    auto metric=[&](double value){if(std::isfinite(value)&&value>=0)s<<value;else s<<L"N/A";};
    s<<L"MotionCompensator | "<<(d.style==0?L"Regional":d.style==1?L"Screen-edge":L"Static")<<L" | "<<(d.staggered?L"Staggered":L"Square")<<L"\n";
    s<<L"FPS capture/analysis/submit: ";metric(d.captureFps);s<<L" / ";metric(d.analysisFps);s<<L" / ";metric(d.renderFps);
    s<<L" | Tick ms: ";metric(d.tickMs);s<<L"\n";
    s<<L"Frame age at receipt ms: ";metric(d.captureAgeMs);s<<L" | Current frame age ms: ";
    metric(d.flow.timestamp>0 && d.now>=d.flow.timestamp?(d.now-d.flow.timestamp)*1000:-1);s<<L"\n";
    s<<L"CPU ms - flow: ";metric(d.analysisMs);s<<L" | fusion: ";metric(d.fusionMs);s<<L" | previous draw/submit: ";metric(d.submitMs);s<<L"\n";
    s<<L"Motion state: "<<motionStateName(d.motionState,false)<<L" | Held regions: "<<d.heldRegions<<L"\n";
    s<<L"R   Input prediction     Optical flow          Fused                Compensation         Conf\n";
    for(int r=0;r<9;++r) {
        s<<r+1<<L" ";
        auto vec=[&](Vec2 v,bool valid=true){if(valid && std::isfinite(v.x)&&std::isfinite(v.y))s<<L"("<<std::setw(7)<<v.x<<L","<<std::setw(7)<<v.y<<L") ";else s<<L"       N/A          ";};
        vec(d.input[r]);vec(d.flow.velocity[r],regionMotionTrusted(d.flow,r,d.now));vec(d.fused[r]);vec(d.compensation[r]);
        if(regionMotionTrusted(d.flow,r,d.now))s<<std::setprecision(2)<<d.flow.regionConfidence[r]<<std::setprecision(1);else s<<L"N/A";
        s<<L"\n";
    }
    s<<L"Vectors: px/s; input is a provisional screen-motion mapping.\n";
    if(d.style==1)s<<L"Edge sources: left=R4 right=R6 top=R2 bottom=R8.\n";
    if(d.style==2)s<<L"Static: compensation is zero.\n";
    return s.str();
}
}
