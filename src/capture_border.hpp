#pragma once
#include "common.hpp"
#include <roapi.h>
#include <asyncinfo.h>
#include <string>
#include <winstring.h>
#include <windows.graphics.capture.h>
#include <windows.security.authorization.appcapabilityaccess.h>
namespace mc {
// Minimal ABI for the SDK interface absent from the bundled MinGW capture
// header. IID/method ABI: Microsoft Windows metadata, windows-rs repository,
// Windows/Graphics/Capture/mod.rs. Requests only documented Borderless access.
using CaptureAccessStatus=ABI::Windows::Security::Authorization::AppCapabilityAccess::AppCapabilityAccessStatus;
using CaptureAccessOperation=ABI::Windows::Foundation::IAsyncOperation<CaptureAccessStatus>;
struct CaptureAccessStatics : IInspectable {
    virtual HRESULT STDMETHODCALLTYPE RequestAccessAsync(INT32 kind,CaptureAccessOperation** operation)=0;
};
class CaptureBorder {
    ComPtr<ABI::Windows::Graphics::Capture::IGraphicsCaptureSession3> session_;
    ComPtr<CaptureAccessOperation> operation_;
public:
    std::string state="unsupported";
    bool requested=false;
    void start(ABI::Windows::Graphics::Capture::IGraphicsCaptureSession* session){
        if(FAILED(session->QueryInterface(IID_PPV_ARGS(&session_))))return;
        constexpr IID iid{0x743ed370,0x06ec,0x5040,{0xa5,0x8a,0x90,0x1f,0x0f,0x75,0x70,0x95}};
        constexpr wchar_t name[]=L"Windows.Graphics.Capture.GraphicsCaptureAccess";
        HSTRING text{};HRESULT hr=WindowsCreateString(name,UINT32(std::size(name)-1),&text);
        ComPtr<CaptureAccessStatics> access;
        if(SUCCEEDED(hr))hr=RoGetActivationFactory(text,iid,reinterpret_cast<void**>(access.GetAddressOf()));
        WindowsDeleteString(text);
        if(FAILED(hr)){state="access_unavailable";return;}
        hr=access->RequestAccessAsync(0,&operation_);
        state=SUCCEEDED(hr)?"awaiting_system_access":"access_request_failed";poll();
    }
    void poll(){
        if(!operation_)return;
        ComPtr<IAsyncInfo> info;
        if(FAILED(operation_.As(&info))){state="access_query_failed";operation_.Reset();return;}
        AsyncStatus status{};
        if(FAILED(info->get_Status(&status))){state="access_query_failed";operation_.Reset();return;}
        if(status==Started)return;
        CaptureAccessStatus result{};HRESULT hr=operation_->GetResults(&result);operation_.Reset();
        if(FAILED(hr)){state="access_request_failed";return;}
        if(result!=ABI::Windows::Security::Authorization::AppCapabilityAccess::AppCapabilityAccessStatus_Allowed){state="system_access_denied";return;}
        hr=session_->put_IsBorderRequired(false);
        requested=SUCCEEDED(hr);state=requested?"borderless_requested":"borderless_request_failed";
        // Assignment success does not prove that another capturing app or
        // system policy won't still require the visible border.
    }
    ~CaptureBorder(){if(operation_){ComPtr<IAsyncInfo> info;if(SUCCEEDED(operation_.As(&info)))info->Cancel();}}
};
}
