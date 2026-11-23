#pragma once
#include "common.hpp"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <vector>
#include <array>
#include <cstdint>
#ifdef _MSC_VER
struct __declspec(uuid("A9B3D012-3DF2-4EE3-B8D1-8695F457D3C1")) SurfaceAccess
#else
struct SurfaceAccess
#endif
 : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetInterface(REFIID,void**)=0;
};
#ifdef __MINGW32__
__CRT_UUID_DECL(SurfaceAccess,0xa9b3d012,0x3df2,0x4ee3,0xb8,0xd1,0x86,0x95,0xf4,0x57,0xd3,0xc1)
#endif
class Downsample {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<ID3D11Texture2D> small,copy;
    struct Slot {ComPtr<ID3D11Texture2D> texture;bool pending=false;double stamp=0;int width=0,height=0;};
    std::array<Slot,3> slots;
    ComPtr<ID3D11ShaderResourceView> source;
    ComPtr<ID3D11RenderTargetView> target;
    ComPtr<ID3D11VertexShader> vs;
    ComPtr<ID3D11PixelShader> ps;
    ComPtr<ID3D11SamplerState> sampler;
    bool chroma_=false;
public:
    struct Readback {std::vector<float> pixels;double timestamp=0;int width=0,height=0;std::vector<uint16_t> chroma;};
#ifdef MC_HAS_OPENCV
    static constexpr int W=960,H=540;
#else
    static constexpr int W=160,H=90;
#endif
    void resetHistory() {for(auto& slot:slots)slot.pending=false;}
    void initialize(ID3D11Device* d,bool chroma=false) {
        chroma_=chroma;
        device=d;d->GetImmediateContext(&context);
        const char* shader=R"(
struct V { float4 p:SV_POSITION; float2 uv:TEXCOORD; };
V vertex(uint id:SV_VertexID) { V o; o.uv=float2((id<<1)&2,id&2);o.p=float4(o.uv*float2(2,-2)+float2(-1,1),0,1);return o; }
Texture2D image:register(t0);SamplerState linearSampler:register(s0);
float pixel(V i):SV_TARGET {return dot(image.Sample(linearSampler,i.uv).rgb,float3(.299,.587,.114));}
uint pixelChroma(V i):SV_TARGET {
    float3 rgb=image.Sample(linearSampler,i.uv).rgb;
    float y=dot(rgb,float3(.299,.587,.114));
    uint luma=(uint)round(saturate(y)*65535);
    uint cb=(uint)round(saturate(.5+(rgb.b-y)*.5643)*255);
    uint cr=(uint)round(saturate(.5+(rgb.r-y)*.7133)*255);
    return luma | (cb<<16) | (cr<<24);
}
)";
        ComPtr<ID3DBlob> v,p,e;
        check(D3DCompile(shader,strlen(shader),nullptr,nullptr,nullptr,"vertex","vs_4_0",0,0,&v,&e),"Downsample vertex shader");
        check(D3DCompile(shader,strlen(shader),nullptr,nullptr,nullptr,chroma?"pixelChroma":"pixel","ps_4_0",0,0,&p,&e),"Downsample pixel shader");
        check(d->CreateVertexShader(v->GetBufferPointer(),v->GetBufferSize(),nullptr,&vs),"Create vertex shader");
        check(d->CreatePixelShader(p->GetBufferPointer(),p->GetBufferSize(),nullptr,&ps),"Create pixel shader");
        D3D11_TEXTURE2D_DESC td{};td.Width=W;td.Height=H;td.MipLevels=td.ArraySize=1;
        td.Format=chroma?DXGI_FORMAT_R32_UINT:DXGI_FORMAT_R32_FLOAT;
        td.SampleDesc.Count=1;td.BindFlags=D3D11_BIND_RENDER_TARGET;
        check(d->CreateTexture2D(&td,nullptr,&small),"Small texture");check(d->CreateRenderTargetView(small.Get(),nullptr,&target),"Small target");
        td.BindFlags=0;td.Usage=D3D11_USAGE_STAGING;td.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        for(auto& slot:slots)check(d->CreateTexture2D(&td,nullptr,&slot.texture),"Readback texture");
        D3D11_SAMPLER_DESC sd{};sd.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;sd.AddressU=sd.AddressV=sd.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;sd.MaxLOD=D3D11_FLOAT32_MAX;
        check(d->CreateSamplerState(&sd,&sampler),"Linear sampler");
    }
    bool read(ID3D11Texture2D* input,std::vector<float>& pixels,std::vector<uint16_t>& chroma,double stamp,double& outputStamp,
              int& outputWidth,int& outputHeight,bool submit,bool& submitted,std::vector<Readback>* preceding=nullptr) {
        submitted=false;
        bool ready=false;
        if(preceding)preceding->clear();
        for(int count=0;count<3;++count){
            Slot* oldest=nullptr;for(auto& slot:slots)if(slot.pending&&(!oldest||slot.stamp<oldest->stamp))oldest=&slot;
            if(!oldest)break;
            D3D11_MAPPED_SUBRESOURCE mapped{};
            HRESULT hr=context->Map(oldest->texture.Get(),0,D3D11_MAP_READ,D3D11_MAP_FLAG_DO_NOT_WAIT,&mapped);
            if(hr==DXGI_ERROR_WAS_STILL_DRAWING)break;
            check(hr,"Analysis readback");
            struct Mapping {
                ID3D11DeviceContext* context;
                ID3D11Texture2D* texture;
                ~Mapping(){context->Unmap(texture,0);}
            } mapping{context.Get(),oldest->texture.Get()};
            if(ready&&preceding)preceding->push_back({std::move(pixels),outputStamp,outputWidth,outputHeight,std::move(chroma)});
            pixels.resize(W*H);
            if(chroma_)chroma.resize(W*H);
            else chroma.clear();
            for(int y=0;y<H;++y){
                const auto* row=static_cast<const char*>(mapped.pData)+y*mapped.RowPitch;
                if(chroma_){
                    const auto* packed=reinterpret_cast<const uint32_t*>(row);
                    for(int x=0;x<W;++x){
                        pixels[y*W+x]=float(packed[x]&0xffffu)*(1.f/65535.f);
                        chroma[y*W+x]=uint16_t(packed[x]>>16);
                    }
                }else memcpy(pixels.data()+y*W,row,W*sizeof(float));
            }
            outputStamp=oldest->stamp;
            outputWidth=oldest->width;outputHeight=oldest->height;ready=true;
            oldest->pending=false;
        }
        // Poll the previous submission on every captured frame, independent of
        // the analysis submission budget. Never queue an unbounded backlog.
        if(!submit) return ready;
        Slot* free=nullptr;for(auto& slot:slots)if(!slot.pending){free=&slot;break;}
        if(!free)return ready; // Fixed capacity; never wait for a busy GPU.
        D3D11_TEXTURE2D_DESC td{},old{};input->GetDesc(&td);if(copy) copy->GetDesc(&old);
        if(!copy || old.Width!=td.Width || old.Height!=td.Height || old.Format!=td.Format) {
            source.Reset();copy.Reset();td.BindFlags=D3D11_BIND_SHADER_RESOURCE;td.Usage=D3D11_USAGE_DEFAULT;td.CPUAccessFlags=td.MiscFlags=0;
            check(device->CreateTexture2D(&td,nullptr,&copy),"Analysis copy texture");
            check(device->CreateShaderResourceView(copy.Get(),nullptr,&source),"Analysis source view");
        }
        context->CopyResource(copy.Get(),input);
        D3D11_VIEWPORT vp{0,0,float(W),float(H),0,1};context->RSSetViewports(1,&vp);
        context->OMSetRenderTargets(1,target.GetAddressOf(),nullptr);context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->VSSetShader(vs.Get(),nullptr,0);context->PSSetShader(ps.Get(),nullptr,0);
        context->PSSetShaderResources(0,1,source.GetAddressOf());context->PSSetSamplers(0,1,sampler.GetAddressOf());context->Draw(3,0);
        ID3D11ShaderResourceView* none=nullptr;context->PSSetShaderResources(0,1,&none);context->OMSetRenderTargets(0,nullptr,nullptr);
        context->CopyResource(free->texture.Get(),small.Get());context->Flush();
        free->pending=true;submitted=true;free->stamp=stamp;free->width=int(td.Width);free->height=int(td.Height);return ready;
    }
};

