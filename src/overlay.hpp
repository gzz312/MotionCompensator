#pragma once
#include "common.hpp"
#include "visual_backend.hpp"
#include "triangle_mesh.hpp"
#include "appearance.hpp"
#include "debug_visual.hpp"
#include <d3d11.h>
#include <dxgi1_2.h>
#include <d2d1_1.h>
#include <dcomp.h>
#include <dwrite.h>
class Overlay {
    HWND window_{};
    HMODULE compositionDll_{};
    ComPtr<ID3D11Device> device_;
    ComPtr<IDXGISwapChain1> swap_;
    ComPtr<ID2D1DeviceContext> context_;
    ComPtr<ID2D1Bitmap1> bitmap_;
    ComPtr<ID2D1SolidColorBrush> brush_;
    ComPtr<IDCompositionDevice> composition_;
    ComPtr<IDCompositionTarget> target_;
    ComPtr<IDCompositionVisual> visual_;
    ComPtr<IDWriteFactory> writeFactory_;
    ComPtr<IDWriteTextFormat> debugFont_;
    bool debugEnabled_=false,debugChinese_=true;
    mc::DebugHistory debugHistory_;
    void drawDebugVisual(int w,int h);
    size_t visibleDots_=0;
    int width_=1, height_=1;
    RECT lastRect_{};
    mc::VisualSettings lastSettings_{};
    mc::BackgroundLight light_;
    mc::LegacyVisualBackend visualBackend_;
    mc::LiveTriangleBackend meshBackend_;
    bool meshEdgesVisible_=false;
    uint64_t drawnMeshRevision_=0;
    std::vector<mc::DotVisual> lastDots_;
    bool painted_=false;
    double lastRaise_=0;
    void bindSurface();
public:
    ~Overlay();
    void initialize(HINSTANCE instance);
    void setDebugEnabled(bool enabled) {debugEnabled_=enabled;debugHistory_.clear();painted_=false;}
    void setDebugSnapshot(const mc::DebugSnapshot& value,bool chinese) {debugHistory_.push(value);debugChinese_=chinese;painted_=false;}
    void setBackground(const mc::BackgroundLight& light){if(light.timestamp!=light_.timestamp){light_=light;painted_=false;}}
    void invalidate() {painted_=false;}
    void setMeshEdgesVisible(bool visible) {meshEdgesVisible_=visible;painted_=false;}
    void resetMotion() {visualBackend_.reset();meshBackend_.reset();painted_=false;}
    void hide() {
        if(window_&&IsWindowVisible(window_)){
            debugHistory_.clear();visualBackend_.reset();meshBackend_.suspend();visibleDots_=0;
            ShowWindow(window_,SW_HIDE);painted_=false;
        }
    }
    bool draw(RECT rect, const mc::VisualSettings& settings, const mc::RegionalVisualMotion& motion,
              const mc::MotionField* field=nullptr,float gain=1.f,bool fieldMoving=false,float appearanceElapsed=0.f);
    size_t dotCount() const {return lastSettings_.style==mc::TriangleStyle?visibleDots_:visualBackend_.dotCount();}
    float phaseMax() const {return lastSettings_.style==mc::TriangleStyle?0.f:visualBackend_.phaseMax();}
    const mc::LiveTriangleBackend& liveMesh() const {return meshBackend_;}
    HWND window() const { return window_; }
};
