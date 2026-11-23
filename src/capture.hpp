#pragma once
#include "common.hpp"
#include "core.hpp"
#include "appearance.hpp"
#include <atomic>
#include <thread>
#include <mutex>
#include <string>
#include <filesystem>
struct CaptureStats { uint64_t frames{},droppedCapture{},droppedAnalysis{},sourceReadbacks{},invalidPairs{}; double timestamp{}, received{}, ageMs{},readbackCallMs{},readbackErrorSince{}; int width{}, height{}; uint64_t brightnessUpdates=0;double brightnessMs=0;mc::BackgroundLight light{}; mc::MotionSample motion{}; double analysisMs{},analysisStartAgeMs{},observedCaptureFps{},targetAnalysisFps{}; uint64_t analyses{},flowResets{}; bool running{},chromaUnavailable{}; std::string borderlessState="not_requested"; std::string status="Not started"; };
class Capture {
    std::jthread thread_;
    std::atomic<bool> flowEnabled_{true},backgroundEnabled_{true},diagnosticRecording_{false},chromaEnabled_{false};
    mutable std::mutex mutex_;
    CaptureStats stats_;
    mc::ChromaInputHint chromaHint_;
    void run(std::stop_token stop, HWND window, bool diagnostic,std::filesystem::path debugDirectory);
public:
    void setDiagnosticRecording(bool enabled){diagnosticRecording_.store(enabled);}
    void setBackgroundEnabled(bool enabled){backgroundEnabled_.store(enabled);}
    void setFlowEnabled(bool enabled){flowEnabled_.store(enabled);}
    void setChromaEnabled(bool enabled){chromaEnabled_.store(enabled);}
    void setChromaHint(const mc::ChromaInputHint& hint){std::lock_guard lock(mutex_);chromaHint_=hint;}
    ~Capture() { stop(); }
    void start(HWND window, bool diagnostic=false,std::filesystem::path debugDirectory={});
    void stop();
    CaptureStats snapshot() const { std::lock_guard lock(mutex_); return stats_; }
};
