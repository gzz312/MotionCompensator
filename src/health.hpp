#pragma once
#include "core.hpp"
namespace mc {
enum class Severity {Normal,Warning,Error};
enum class HealthReason {NoTarget,Starting,CaptureError,Paused,Static,Waiting,Observation,Ready};
struct Health {Severity severity=Severity::Normal;HealthReason reason=HealthReason::NoTarget;};
inline Health captureHealth(bool selected,bool running,bool foreground,bool dynamic,const std::string& status,const MotionSample& motion,double now){
    if(!selected)return {};
    bool failure=status.find("error")!=std::string::npos||status.find("HRESULT")!=std::string::npos;
    if(failure)return {Severity::Error,HealthReason::CaptureError};
    if(!running)return status=="Starting WGC"?Health{Severity::Normal,HealthReason::Starting}:Health{Severity::Error,HealthReason::CaptureError};
    if(!foreground)return {Severity::Normal,HealthReason::Paused};
    if(!dynamic)return {Severity::Normal,HealthReason::Static};
    if(motion.timestamp<=0)return {Severity::Warning,HealthReason::Waiting};
    if(!motionFresh(motion,now)||!std::isfinite(motion.confidence)||motion.confidence<.65f)return {Severity::Warning,HealthReason::Observation};
    return {Severity::Normal,HealthReason::Ready};
}
}
