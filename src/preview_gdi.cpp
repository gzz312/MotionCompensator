#include "preview_gdi.hpp"
#include <gdiplus.h>
namespace {
struct PreviewGdiPlus {
    ULONG_PTR token=0;
    PreviewGdiPlus(){Gdiplus::GdiplusStartupInput input;Gdiplus::GdiplusStartup(&token,&input,nullptr);}
    ~PreviewGdiPlus(){if(token)Gdiplus::GdiplusShutdown(token);}
};
Gdiplus::Color color(mc::Color c,float alpha){
    auto byte=[](float v){return BYTE(std::lround(std::clamp(v,0.f,1.f)*255));};
    return {byte(alpha),byte(c.r),byte(c.g),byte(c.b)};
}
}
void paintPreview(HDC dc,POINT origin,const mc::PreviewScene& scene) {
    if(scene.width<=0||scene.height<=0)return;
    static PreviewGdiPlus runtime;if(!runtime.token)return;
    // Resolve logical UI mapping once. Subpixel radii keep scaled outlines
    // and halos separate, without sampling already-painted layers via GetPixel.
    POINT points[]={{origin.x,origin.y},{origin.x+scene.width,origin.y+scene.height}};
    LPtoDP(dc,points,2);int saved=SaveDC(dc);
    SetMapMode(dc,MM_TEXT);SetWindowOrgEx(dc,0,0,nullptr);SetViewportOrgEx(dc,0,0,nullptr);
    {
        Gdiplus::Graphics g(dc);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.TranslateTransform(float(points[0].x),float(points[0].y));
        g.ScaleTransform(float(points[1].x-points[0].x)/scene.width,float(points[1].y-points[0].y)/scene.height);
        float left=std::ceil(scene.contentLeft),top=std::ceil(scene.contentTop);
        g.SetClip(Gdiplus::RectF(left,top,std::max(0.f,std::floor(scene.contentRight)-left),std::max(0.f,std::floor(scene.contentBottom)-top)));
        for(const auto& dot:scene.dots){Gdiplus::SolidBrush brush(color(dot.color,dot.opacity));
            g.FillEllipse(&brush,dot.position.x-dot.radius,dot.position.y-dot.radius,2*dot.radius,2*dot.radius);}
        Gdiplus::Pen pen(color(scene.reticleColor,scene.reticleOpacity),1.f);
        for(auto line:scene.reticle)g.DrawLine(&pen,line.from.x,line.from.y,line.to.x,line.to.y);
        Gdiplus::SolidBrush brush(color(scene.reticleColor,scene.reticleOpacity));
        for(auto circle:scene.reticleCircles){auto p=circle.center;float r=circle.radius;
            if(circle.filled)g.FillEllipse(&brush,p.x-r,p.y-r,2*r,2*r);
            else g.DrawEllipse(&pen,p.x-r,p.y-r,2*r,2*r);}
        g.Flush(Gdiplus::FlushIntentionSync);
    }
    RestoreDC(dc,saved);
}
