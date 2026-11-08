#include "common.hpp"
#include "debug_visual_d2d.hpp"
#include <wincodec.h>
#include <iostream>
#include <cstdlib>
void require(bool ok,const char* why){if(!ok){std::cerr<<"FAIL: "<<why<<'\n';std::exit(1);}}
int main(){
    mc::DebugHistory history;mc::DebugSnapshot d;
    require(mc::buildDebugScene(history,true,0).rects.empty(),"disabled/empty history generates no panel");
    for(int i=0;i<=200;++i){d.now=2+i*.1;history.push(d);}
    require(history.samples().size()<=101 && history.samples().back().now-history.samples().front().now<=10.0001,"history has a bounded ten-second window");
    d.now+=3;history.push(d);require(history.samples().size()==1,"long pauses cannot connect unrelated graph segments");
    d.now-=1;history.push(d);require(history.samples().size()==1,"backward timestamps clear history");
    d.style=1;d.now+=.1;history.push(d);require(history.samples().size()==1,"style change clears graph history");
    history.clear();
    for(int i=0;i<=100;++i){
        d={};d.now=2+i*.1;d.flow.timestamp=d.now-.02;d.flow.confidence=.85f;d.flow.features=80;d.staggered=true;
        d.captureFps=60+std::sin(i*.2);d.analysisFps=58+std::sin(i*.2);d.renderFps=59+std::sin(i*.2);
        d.captureAgeMs=6;d.tickMs=16.7;d.analysisMs=2+std::sin(i*.1);d.fusionMs=.04;d.submitMs=.5+std::sin(i*.1)*.2;
        if(i==40)d.analysisMs=-1;
        for(int r=0;r<9;++r){float x=float(r%3-1),y=float(r/3-1);d.input[r]={x*180,y*180};d.flow.velocity[r]={x*220,y*200};d.fused[r]={x*210,y*190};d.compensation[r]={-x*80,-y*70};d.flow.regionConfidence[r]=.8f;}
        d.flow.regionConfidence[8]=.2f;history.push(d);
    }
    auto scene=mc::buildDebugScene(history,true,120);
    require(scene.rects.size()>20 && scene.strokes.size()>400,"panel contains confidence bars, vector arrows and real history curves");
    for(auto l:scene.strokes)require(std::isfinite(l.a.x)&&std::isfinite(l.a.y)&&std::isfinite(l.b.x)&&std::isfinite(l.b.y)&&l.a.x>=0&&l.b.x<=460&&l.a.y>=0&&l.b.y<=mc::DebugScene::height,"all vector and trend strokes stay finite and inside panel");
    for(auto label:scene.labels)require(label.x>=0 && label.y+18<=mc::DebugScene::height,"labels fit inside panel");
    auto english=mc::buildDebugScene(history,false,120);require(english.labels[0].text.find(L"Motion view")!=std::wstring::npos,"visual panel follows language choice");
    HRESULT apartment=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);require(SUCCEEDED(apartment),"COM initialized");
    {
        ComPtr<IWICImagingFactory> wic;check(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&wic)),"WIC factory");
        ComPtr<IWICBitmap> bitmap;check(wic->CreateBitmap(460,int(mc::DebugScene::height),GUID_WICPixelFormat32bppPBGRA,WICBitmapCacheOnLoad,&bitmap),"WIC bitmap");
        ComPtr<ID2D1Factory> factory;check(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,factory.GetAddressOf()),"D2D factory");
        ComPtr<ID2D1RenderTarget> target;auto props=D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE,D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED),96,96);
        check(factory->CreateWicBitmapRenderTarget(bitmap.Get(),props,&target),"offscreen D2D target");
        ComPtr<IDWriteFactory> write;check(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),reinterpret_cast<IUnknown**>(write.GetAddressOf())),"DWrite factory");
        ComPtr<IDWriteTextFormat> font;check(write->CreateTextFormat(L"Segoe UI",nullptr,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,13,L"en-us",&font),"debug font");font->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        ComPtr<ID2D1SolidColorBrush> brush;check(target->CreateSolidColorBrush(D2D1::ColorF(1,1,1),&brush),"brush");
        target->BeginDraw();target->Clear(D2D1::ColorF(.08f,.1f,.14f,1));paintDebugScene(target.Get(),brush.Get(),font.Get(),scene);check(target->EndDraw(),"shared panel renderer");
        {
            ComPtr<IWICBitmapLock> lock;WICRect rect{0,0,460,int(mc::DebugScene::height)};check(bitmap->Lock(&rect,WICBitmapLockRead,&lock),"bitmap readback");UINT size=0,stride=0;BYTE* bytes=nullptr;check(lock->GetDataPointer(&size,&bytes),"readback bytes");check(lock->GetStride(&stride),"stride");
            int colored=0;for(int y=75;y<270;++y)for(int x=12;x<450;++x){auto p=bytes+y*stride+x*4;if(p[0]>140||p[1]>140||p[2]>140)++colored;}
            require(colored>1000,"actual renderer draws visible arrows, labels and confidence bars");
        }
        ComPtr<IWICStream> stream;check(wic->CreateStream(&stream),"PNG stream");check(stream->InitializeFromFilename(L"debug-visual-sample.png",GENERIC_WRITE),"PNG output");
        ComPtr<IWICBitmapEncoder> encoder;check(wic->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder),"PNG encoder");check(encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache),"PNG initialize");
        ComPtr<IWICBitmapFrameEncode> frame;ComPtr<IPropertyBag2> options;check(encoder->CreateNewFrame(&frame,&options),"PNG frame");check(frame->Initialize(options.Get()),"frame initialize");check(frame->WriteSource(bitmap.Get(),nullptr),"PNG pixels");check(frame->Commit(),"PNG frame commit");check(encoder->Commit(),"PNG commit");
    }
    CoUninitialize();std::cout<<"PASS: visual geometry, real D2D/WIC pixels, bounded history, invalid values, language and shared renderer PNG\n";
}
