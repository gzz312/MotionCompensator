#include "reticle.hpp"
#include "overlay.hpp"
#include "debug_visual_d2d.hpp"
#include <d2d1_1helper.h>
static LRESULT CALLBACK overlayProc(HWND w,UINT m,WPARAM a,LPARAM b) {
    if(m==WM_NCHITTEST) return HTTRANSPARENT;
    if(m==WM_MOUSEACTIVATE) return MA_NOACTIVATE;
    return DefWindowProcW(w,m,a,b);
}
Overlay::~Overlay() {
    if(window_) DestroyWindow(window_);
    visual_.Reset(); target_.Reset(); composition_.Reset();
    if(compositionDll_) FreeLibrary(compositionDll_);
}
void Overlay::initialize(HINSTANCE instance) {
    WNDCLASSW wc{}; wc.hInstance=instance; wc.lpfnWndProc=overlayProc; wc.lpszClassName=L"MotionCompensatorOverlay";
    RegisterClassW(&wc);
    window_=CreateWindowExW(WS_EX_TOPMOST|WS_EX_TRANSPARENT|WS_EX_LAYERED|WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW|WS_EX_NOREDIRECTIONBITMAP,
        wc.lpszClassName,L"MotionCompensator overlay",WS_POPUP,0,0,1,1,nullptr,nullptr,instance,nullptr);
    if(!window_) throw std::runtime_error("Overlay window creation failed");
    if(!SetLayeredWindowAttributes(window_,0,255,LWA_ALPHA)) throw std::runtime_error("Overlay click-through setup failed");
    SetWindowDisplayAffinity(window_,0x11); // Best effort WDA_EXCLUDEFROMCAPTURE; never suppress the WGC border.
    check(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,&device_,nullptr,nullptr),"Overlay D3D11 device");
    ComPtr<IDXGIDevice> dxgi; check(device_.As(&dxgi),"Overlay DXGI device");
    ComPtr<IDXGIAdapter> adapter; check(dxgi->GetAdapter(&adapter),"DXGI adapter");
    ComPtr<IDXGIFactory2> factory; check(adapter->GetParent(IID_PPV_ARGS(&factory)),"DXGI factory");
    DXGI_SWAP_CHAIN_DESC1 desc{}; desc.Width=1; desc.Height=1; desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count=1; desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT; desc.BufferCount=2;
    desc.Scaling=DXGI_SCALING_STRETCH; desc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL; desc.AlphaMode=DXGI_ALPHA_MODE_PREMULTIPLIED;
    check(factory->CreateSwapChainForComposition(device_.Get(),&desc,nullptr,&swap_),"Composition swap chain");
    ComPtr<ID2D1Factory1> d2d; D2D1_FACTORY_OPTIONS options{};
    check(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,__uuidof(ID2D1Factory1),&options,reinterpret_cast<void**>(d2d.GetAddressOf())),"D2D factory");
    ComPtr<ID2D1Device> d2dDevice; check(d2d->CreateDevice(dxgi.Get(),&d2dDevice),"D2D device");
    check(d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE,&context_),"D2D context");
    check(context_->CreateSolidColorBrush(D2D1::ColorF(1.f,1.f,1.f,.45f),&brush_),"Dot brush");
    check(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),reinterpret_cast<IUnknown**>(writeFactory_.GetAddressOf())),"Debug text factory");
    check(writeFactory_->CreateTextFormat(L"Segoe UI",nullptr,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,13.f,L"en-us",&debugFont_),"Debug text format");
    debugFont_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    compositionDll_=LoadLibraryExW(L"dcomp.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!compositionDll_) throw std::runtime_error("DirectComposition unavailable");
    using CreateComposition=HRESULT(WINAPI*)(IDXGIDevice*,REFIID,void**);
    auto create=loadFunction<CreateComposition>(compositionDll_,"DCompositionCreateDevice");
    if(!create) throw std::runtime_error("DirectComposition entry point unavailable");
    check(create(dxgi.Get(),__uuidof(IDCompositionDevice),reinterpret_cast<void**>(composition_.GetAddressOf())),"DCompositionCreateDevice");
    check(composition_->CreateTargetForHwnd(window_,TRUE,&target_),"Composition HWND target");
    check(composition_->CreateVisual(&visual_),"Composition visual");
    check(visual_->SetContent(swap_.Get()),"Composition content"); check(target_->SetRoot(visual_.Get()),"Composition root");
    check(composition_->Commit(),"Composition commit"); bindSurface();
}
void Overlay::bindSurface() {
    ComPtr<IDXGISurface> surface; check(swap_->GetBuffer(0,IID_PPV_ARGS(&surface)),"Swap surface");
    auto props=D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET|D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED),96,96);
    check(context_->CreateBitmapFromDxgiSurface(surface.Get(),&props,&bitmap_),"D2D swap bitmap"); context_->SetTarget(bitmap_.Get());
}
bool Overlay::draw(RECT rect,const mc::VisualSettings& settings,const std::array<mc::Vec2,9>& velocity,float dt,bool dynamic) {
    int w=rect.right-rect.left,h=rect.bottom-rect.top; if(w<1 || h<1) {hide(); return false;}
    // A fullscreen/borderless target can reorder itself without moving. Keep
    // z-order maintenance independent of dirty drawing; never activate it.
    double now=qpcSeconds();
    if(!IsWindowVisible(window_) || !EqualRect(&rect,&lastRect_) || now-lastRaise_>.5) {
        if(!SetWindowPos(window_,HWND_TOPMOST,rect.left,rect.top,w,h,SWP_NOACTIVATE|SWP_SHOWWINDOW))
            throw std::runtime_error("Overlay positioning failed");
        lastRaise_=now;
    }
    if(activeStyle_!=settings.style) {resetMotion();activeStyle_=settings.style;}
    auto fixed=settings.style==2?mc::staticDots(w,h,settings):std::vector<mc::DotVisual>{};
    const auto& dots=settings.style==2?fixed:settings.style==1?edgeDots_.update(velocity,w,h,settings,dt):dots_.update(velocity,w,h,settings.spacing,dt,dynamic,settings.enhancement,settings.boundaryFade,settings.radius,settings.staggered);
    bool moved=dots.size()!=lastDots_.size();
    if(!moved) for(size_t i=0;i<dots.size();++i) {
        if(std::abs(dots[i].position.x-lastDots_[i].position.x)>.1f || std::abs(dots[i].position.y-lastDots_[i].position.y)>.1f || std::abs(dots[i].opacity-lastDots_[i].opacity)>.005f || std::abs(dots[i].radiusScale-lastDots_[i].radiusScale)>.002f || std::abs(dots[i].blendWeight-lastDots_[i].blendWeight)>.005f) {moved=true;break;}
    }
    if(!moved && painted_ && IsWindowVisible(window_) && EqualRect(&rect,&lastRect_) && settings==lastSettings_) return false;
    if(w!=width_ || h!=height_) {
        context_->SetTarget(nullptr); bitmap_.Reset();
        check(swap_->ResizeBuffers(2,w,h,DXGI_FORMAT_B8G8R8A8_UNORM,0),"Resize overlay"); width_=w; height_=h; bindSurface();
    }
    context_->BeginDraw(); context_->Clear(D2D1::ColorF(0.f,0.f,0.f,0.f));
    // Regional lattices keep their original clipping and spacing. Boundary
    // softness only attenuates the crossing dots by a conservative amount.
    visibleDots_=0;
    int clippedRegion=-1;
    for(const auto& dot:dots) {
        float px=dot.position.x,py=dot.position.y;
        float dotRadius=settings.radius*dot.radiusScale;
        float alpha=dot.opacity*settings.opacity*(settings.style==2?1.f:mc::dotVisibility(px,py,float(w),float(h),dotRadius,settings.holeDiameter));
        if(alpha<=0 || px<-dotRadius || py<-dotRadius || px>w+dotRadius || py>h+dotRadius) continue;
        if(dynamic && settings.style==0 && clippedRegion!=dot.region) {
            if(clippedRegion>=0) context_->PopAxisAlignedClip();
            clippedRegion=dot.region;int col=dot.region%3,row=dot.region/3;
            context_->PushAxisAlignedClip(D2D1::RectF(col*w/3.f,row*h/3.f,(col+1)*w/3.f,(row+1)*h/3.f),D2D1_ANTIALIAS_MODE_ALIASED);
        }
        if((!dynamic || settings.style!=0) || (px+dotRadius>(dot.region%3)*w/3.f && px-dotRadius<(dot.region%3+1)*w/3.f && py+dotRadius>(dot.region/3)*h/3.f && py-dotRadius<(dot.region/3+1)*h/3.f))++visibleDots_;
        float background=light_.timestamp>0?light_.sample(px,py,float(w),float(h)):.5f;
        for(auto layer:mc::dotLayers(settings.dots,dotRadius,alpha,settings.style==2&&dot.region==1,settings.appearance,background)){
            brush_->SetColor(D2D1::ColorF(layer.color.r,layer.color.g,layer.color.b,layer.opacity));
            context_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(px,py),layer.radius,layer.radius),brush_.Get());
        }
    }
    if(clippedRegion>=0) context_->PopAxisAlignedClip();
    if(settings.reticleShape) {
        auto geometry=mc::buildReticle(settings.reticleShape,{w*.5f,h*.5f},settings.reticleSize);
        brush_->SetColor(D2D1::ColorF(settings.reticle.r,settings.reticle.g,settings.reticle.b,settings.reticleOpacity));
        for(int i=0;i<geometry.lineCount;++i){auto line=geometry.lines[i];context_->DrawLine(D2D1::Point2F(line.from.x,line.from.y),D2D1::Point2F(line.to.x,line.to.y),brush_.Get(),1.3f);}
        for(int i=0;i<geometry.circleCount;++i){auto circle=geometry.circles[i];auto ellipse=D2D1::Ellipse(D2D1::Point2F(circle.center.x,circle.center.y),circle.radius,circle.radius);if(circle.filled)context_->FillEllipse(ellipse,brush_.Get());else context_->DrawEllipse(ellipse,brush_.Get(),1.3f);}
    }
    if(debugEnabled_)drawDebugVisual(w,h);
    check(context_->EndDraw(),"Draw overlay");
    HRESULT present=swap_->Present(0,DXGI_PRESENT_DO_NOT_WAIT);
    if(present!=DXGI_ERROR_WAS_STILL_DRAWING) check(present,"Present overlay");
    painted_=present!=DXGI_ERROR_WAS_STILL_DRAWING; lastRect_=rect; lastSettings_=settings;if(painted_) lastDots_=dots;
    return painted_;
}

void Overlay::drawDebugVisual(int w,int h) {
    const auto scene=mc::buildDebugScene(debugHistory_,debugChinese_,visibleDots_);
    if(scene.rects.empty())return;
    float scale=std::min({1.25f,(float(w)*.38f)/mc::DebugScene::width,(float(h)*.47f)/mc::DebugScene::height});
    if(scale<=0)return;
    D2D1_MATRIX_3X2_F saved;context_->GetTransform(&saved);
    context_->SetTransform(D2D1::Matrix3x2F::Scale(scale,scale)*D2D1::Matrix3x2F::Translation(12,12));
    paintDebugScene(context_.Get(),brush_.Get(),debugFont_.Get(),scene);
    context_->SetTransform(saved);
}
