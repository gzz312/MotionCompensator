#include "preview_gdi.hpp"
static COLORREF previewColor(mc::Color c,float opacity,COLORREF bg=RGB(20,29,44)) {
    float alpha=std::clamp(opacity,0.f,1.f);
    return RGB(int(GetRValue(bg)+(c.r*255-GetRValue(bg))*alpha),int(GetGValue(bg)+(c.g*255-GetGValue(bg))*alpha),int(GetBValue(bg)+(c.b*255-GetBValue(bg))*alpha));
}
void paintPreview(HDC dc,POINT origin,const mc::PreviewScene& scene) {
    int state=SaveDC(dc);
    IntersectClipRect(dc,origin.x,origin.y,origin.x+scene.width,origin.y+scene.height);
    IntersectClipRect(dc,origin.x+int(std::ceil(scene.contentLeft)),origin.y+int(std::ceil(scene.contentTop)),origin.x+int(std::floor(scene.contentRight)),origin.y+int(std::floor(scene.contentBottom)));
    SelectObject(dc,GetStockObject(NULL_PEN));
    for(auto dot:scene.dots) {
        HBRUSH brush=CreateSolidBrush(previewColor(dot.color,dot.opacity,GetPixel(dc,origin.x+int(dot.position.x),origin.y+int(dot.position.y))));auto old=SelectObject(dc,brush);
        int x=origin.x+int(std::lround(dot.position.x)),y=origin.y+int(std::lround(dot.position.y)),r=std::max(1,int(std::lround(dot.radius)));
        if(r==1){RECT pixel{x-1,y-1,x+1,y+1};FillRect(dc,&pixel,brush);}
        else Ellipse(dc,x-r,y-r,x+r+1,y+r+1);
        SelectObject(dc,old);DeleteObject(brush);
    }
    HPEN pen=CreatePen(PS_SOLID,1,previewColor(scene.reticleColor,scene.reticleOpacity));auto old=SelectObject(dc,pen);
    for(auto line:scene.reticle) {MoveToEx(dc,origin.x+int(std::lround(line.from.x)),origin.y+int(std::lround(line.from.y)),nullptr);LineTo(dc,origin.x+int(std::lround(line.to.x)),origin.y+int(std::lround(line.to.y)));}
    SelectObject(dc,old);DeleteObject(pen);RestoreDC(dc,state);
}
