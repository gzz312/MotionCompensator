#include "capture.hpp"
#include "capture_border.hpp"
#include "downsample.hpp"
#include "flow.hpp"
#include "analysis_pair_mailbox.hpp"
#include "capture_cadence.hpp"
#ifdef MC_HAS_OPENCV
#include "tracking.hpp"
#endif
#include <fstream>
#include <d3d11.h>
#include <dxgi.h>
#include <roapi.h>
#include <winstring.h>
#include <windows.graphics.capture.h>
#include <windows.graphics.capture.interop.h>
#include <chrono>
namespace cap = ABI::Windows::Graphics::Capture;
namespace gfx = ABI::Windows::Graphics;
namespace dx = ABI::Windows::Graphics::DirectX;
namespace foundation = ABI::Windows::Foundation;
template<class T> ComPtr<T> factory(const wchar_t* name) {
    HSTRING text{}; check(WindowsCreateString(name, static_cast<UINT32>(wcslen(name)), &text), "WindowsCreateString");
    ComPtr<T> result; HRESULT hr=RoGetActivationFactory(text, __uuidof(T), reinterpret_cast<void**>(result.GetAddressOf()));
    WindowsDeleteString(text); check(hr, "RoGetActivationFactory"); return result;
}
template<class T> void closeObject(ComPtr<T>& object) {
    if (object) { ComPtr<foundation::IClosable> close; if (SUCCEEDED(object.As(&close))) close->Close(); object.Reset(); }
}
void Capture::stop() { if(thread_.joinable()) { thread_.request_stop(); thread_.join(); } }
void Capture::start(HWND window,bool diagnostic,std::filesystem::path debugDirectory) {
    stop(); { std::lock_guard lock(mutex_); stats_={}; stats_.status="Starting WGC"; }
    thread_=std::jthread([this,window,diagnostic,debugDirectory](std::stop_token stop){run(stop,window,diagnostic,debugDirectory);});
}
void Capture::run(std::stop_token stop, HWND window,bool diagnostic,std::filesystem::path debugDirectory) {
    HRESULT apartment=RoInitialize(RO_INIT_MULTITHREADED);
    ComPtr<cap::IDirect3D11CaptureFramePool> pool;
    ComPtr<cap::IGraphicsCaptureSession> session;
    try {
        check(apartment,"RoInitialize");
        boolean supported{}; check(factory<cap::IGraphicsCaptureSessionStatics>(L"Windows.Graphics.Capture.GraphicsCaptureSession")->IsSupported(&supported),"IsSupported");
        if(!supported) throw std::runtime_error("Windows Graphics Capture unavailable; static overlay remains available");
        ComPtr<ID3D11Device> device;
        check(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,nullptr),"Capture D3D11 device");
        ComPtr<IDXGIDevice> dxgi; check(device.As(&dxgi),"DXGI device");
        using CreateDevice = HRESULT (WINAPI*)(IDXGIDevice*, IInspectable**);
        auto create = loadFunction<CreateDevice>(GetModuleHandleW(L"d3d11.dll"),"CreateDirect3D11DeviceFromDXGIDevice");
        if(!create) throw std::runtime_error("D3D11 WinRT interop unavailable");
        ComPtr<IInspectable> inspectable; check(create(dxgi.Get(),&inspectable),"WinRT D3D11 device");
        ComPtr<dx::Direct3D11::IDirect3DDevice> runtimeDevice; check(inspectable.As(&runtimeDevice),"IDirect3DDevice");
        ComPtr<cap::IGraphicsCaptureItem> item;
        check(factory<IGraphicsCaptureItemInterop>(L"Windows.Graphics.Capture.GraphicsCaptureItem")->CreateForWindow(window,__uuidof(cap::IGraphicsCaptureItem),reinterpret_cast<void**>(item.GetAddressOf())),"CreateForWindow");
        gfx::SizeInt32 size{}; check(item->get_Size(&size),"Capture size");
        auto format=dx::DirectXPixelFormat_B8G8R8A8UIntNormalized;
        check(factory<cap::IDirect3D11CaptureFramePoolStatics2>(L"Windows.Graphics.Capture.Direct3D11CaptureFramePool")->CreateFreeThreaded(runtimeDevice.Get(),format,2,size,&pool),"CreateFreeThreaded");
        check(pool->CreateCaptureSession(item.Get(),&session),"CreateCaptureSession");
        mc::CaptureBorder border;border.start(session.Get());
        check(session->StartCapture(),"StartCapture");
        { std::lock_guard lock(mutex_); stats_.running=true; stats_.status="WGC active / waiting for analysis"; }
        Downsample analysis; bool analysisReady=false;
        std::vector<float> pixels;double lastAnalysis=0;bool wasActive=false,wasFlow=flowEnabled_.load();
        mc::CaptureCadence cadence;
        mc::AnalysisPairMailbox mailbox(1./30);
        std::jthread processor([&](std::stop_token token){
            mc::AnalysisPair pair;int recorded=0;double lastRecord=0;uint64_t workerGeneration=~uint64_t(0);
#ifdef MC_HAS_OPENCV
            mc::DenseFlowWorkspace workspace;
#endif
            while(mailbox.take(token,pair)){
                if(!flowEnabled_.load()){
#ifdef MC_HAS_OPENCV
                    workspace.reset();
#endif
                    continue;}
                const auto& previous=*pair.previous;const auto& current=*pair.current;
                if(workerGeneration!=current.generation){workerGeneration=current.generation;
#ifdef MC_HAS_OPENCV
                    workspace.reset();
#endif
                }
                double begin=qpcSeconds(),dt=current.timestamp-previous.timestamp;
                if(previous.sourceWidth!=current.sourceWidth||previous.sourceHeight!=current.sourceHeight||!std::isfinite(dt)||dt<.008||dt>.15){std::lock_guard lock(mutex_);++stats_.invalidPairs;continue;}
                mc::MotionSample motion;
                try {
                    if(previous.sourceWidth==current.sourceWidth && previous.sourceHeight==current.sourceHeight && dt>0){
#ifdef MC_HAS_OPENCV
                        motion=mc::estimateDenseFlow(previous.pixels,current.pixels,Downsample::W,Downsample::H,dt,current.timestamp,current.sourceWidth,current.sourceHeight,&workspace);
#else
                        motion=mc::estimateFlow(previous.pixels,current.pixels,Downsample::W,Downsample::H,dt,current.timestamp,current.sourceWidth,current.sourceHeight);
#endif
                    }
                    if(diagnosticRecording_.load() && !debugDirectory.empty() && recorded<16 && begin-lastRecord>.15 && previous.pixels.size()==current.pixels.size() && dt>0 && dt<.15 && motion.meanDelta>.003f){
                        std::filesystem::create_directories(debugDirectory);
                        std::string name="pair-"+std::to_string(int64_t(current.timestamp*1000));
                        auto saveGray=[&](const std::filesystem::path& file,const std::vector<float>& data){
                            std::ofstream stream(file,std::ios::binary);stream<<"P5\n"<<Downsample::W<<' '<<Downsample::H<<"\n255\n";
                            for(float v:data){auto byte=static_cast<unsigned char>(std::clamp(v,0.f,1.f)*255);stream.write(reinterpret_cast<char*>(&byte),1);}
                        };
                        saveGray(debugDirectory/(name+"-a.pgm"),previous.pixels);saveGray(debugDirectory/(name+"-b.pgm"),current.pixels);
                        std::ofstream meta(debugDirectory/(name+".txt"));meta<<dt<<' '<<current.timestamp<<' '<<current.sourceWidth<<' '<<current.sourceHeight<<' '<<motion.confidence<<'\n';
                        ++recorded;lastRecord=begin;
                    }
                    motion.completedTimestamp=qpcSeconds();
                    std::lock_guard lock(mutex_);
                    if(mailbox.current(current.generation)){
                        stats_.motion=std::move(motion);++stats_.analyses;stats_.analysisMs=(qpcSeconds()-begin)*1000;
                        stats_.analysisStartAgeMs=(begin-current.timestamp)*1000;stats_.droppedAnalysis=mailbox.dropped();
                        stats_.status="WGC / adjacent capture pairs / 30Hz analysis / DIS";
                    }
                }catch(const std::exception& e){
                    std::lock_guard lock(mutex_);
                    if(mailbox.current(current.generation)){stats_.motion={};stats_.status=std::string("Analysis error: ")+e.what();}
                }
            }
        });
        auto resetHistory=[&]{
            analysis.resetHistory();mailbox.reset();cadence.reset();
            std::lock_guard lock(mutex_);stats_.motion={};stats_.light={};++stats_.flowResets;
        };
        while(!stop.stop_requested() && IsWindow(window)) {
            border.poll();{std::lock_guard lock(mutex_);stats_.borderlessState=border.state;}
            bool active=diagnostic || (GetForegroundWindow()==window && !IsIconic(window));
            bool flow=flowEnabled_.load();
            if(active!=wasActive||flow!=wasFlow) {resetHistory();wasActive=active;wasFlow=flow;}
            ComPtr<cap::IDirect3D11CaptureFrame> frame;
            uint64_t receivedFrames=0;
            // Drain the bounded WGC queue before copying. Processing and GPU
            // readback always start from the newest frame, never a FIFO tail.
            for(int i=0;i<8;++i){
                ComPtr<cap::IDirect3D11CaptureFrame> next;
                check(pool->TryGetNextFrame(&next),"TryGetNextFrame");if(!next)break;
                closeObject(frame);frame=std::move(next);++receivedFrames;
            }
            if(frame) {
                foundation::TimeSpan stamp{}; gfx::SizeInt32 content{};
                check(frame->get_SystemRelativeTime(&stamp),"Frame timestamp");
                check(frame->get_ContentSize(&content),"Frame content size");
                ComPtr<dx::Direct3D11::IDirect3DSurface> surface; check(frame->get_Surface(&surface),"Frame GPU surface");
                double now=qpcSeconds(), timestamp=double(stamp.Duration)/1e7;
                { std::lock_guard lock(mutex_); stats_.frames+=receivedFrames;stats_.droppedCapture+=receivedFrames>0?receivedFrames-1:0; stats_.timestamp=timestamp; stats_.received=now;
                  stats_.ageMs=(now-timestamp)*1000; stats_.width=content.Width; stats_.height=content.Height; }
                bool resized=content.Width!=size.Width || content.Height!=size.Height;
                if(resized) resetHistory();
                if(active && (flowEnabled_.load()||backgroundEnabled_.load()) && !resized && content.Width>0 && content.Height>0){
                    try {
                        if(!analysisReady){analysis.initialize(device.Get());analysisReady=true;}
                        ComPtr<SurfaceAccess> access;check(surface.As(&access),"Surface access");
                        ComPtr<ID3D11Texture2D> texture;check(access->GetInterface(IID_PPV_ARGS(&texture)),"Capture texture");
                        double stamp=0;int frameW=0,frameH=0;bool submitted=false;
                        if(flow)cadence.observe(timestamp,receivedFrames);
                        double readBegin=qpcSeconds();
                        std::vector<Downsample::Readback> preceding;
                        bool ready=analysis.read(texture.Get(),pixels,timestamp,stamp,frameW,frameH,flow?true:now-lastAnalysis>=.1,submitted,&preceding);
                        {std::lock_guard lock(mutex_);stats_.readbackCallMs=(qpcSeconds()-readBegin)*1000;if(ready&&flow)stats_.sourceReadbacks+=preceding.size()+1;}
                        if(submitted)lastAnalysis=now;
                        {std::lock_guard lock(mutex_);stats_.observedCaptureFps=flow?cadence.captureFps():0;stats_.targetAnalysisFps=flow?cadence.targetFps():0;}
                        if(ready){
                            {std::lock_guard lock(mutex_);if(backgroundEnabled_.load() && stamp-stats_.light.timestamp>=.09){double begin=qpcSeconds();stats_.light.update(pixels,Downsample::W,Downsample::H,stamp);stats_.brightnessMs=(qpcSeconds()-begin)*1000;++stats_.brightnessUpdates;}}
                            if(flowEnabled_.load()){
                                std::vector<mc::GrayFrame> completed;completed.reserve(preceding.size()+1);
                                for(auto& earlier:preceding)completed.push_back({std::move(earlier.pixels),earlier.timestamp,earlier.width,earlier.height,0});
                                completed.push_back({std::move(pixels),stamp,frameW,frameH,0});mailbox.submitBatch(std::move(completed));
                            }
                            else {mailbox.reset();std::lock_guard lock(mutex_);stats_.motion={};stats_.status="WGC / background brightness only";}
                        }
                    }catch(const std::exception& e){
                        resetHistory();analysisReady=false;
                        std::lock_guard lock(mutex_);stats_.status=std::string("Capture readback error: ")+e.what();
                    }
                }
                surface.Reset(); closeObject(frame);
                if(content.Width>0 && content.Height>0 && (content.Width!=size.Width || content.Height!=size.Height)) {
                    size=content; check(pool->Recreate(runtimeDevice.Get(),format,2,size),"Resize capture pool");
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(8));
        }
        processor.request_stop();processor.join();
        { std::lock_guard lock(mutex_); stats_.status="Capture stopped"; }
    } catch(const std::exception& e) { std::lock_guard lock(mutex_); stats_.status=e.what(); }
    closeObject(session); closeObject(pool);
    { std::lock_guard lock(mutex_); stats_.running=false; }
    if(SUCCEEDED(apartment)) RoUninitialize();
}
