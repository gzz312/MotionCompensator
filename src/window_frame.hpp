#pragma once
#include "fluent.hpp"
namespace mc {
// Only the app's own panel uses this caption. Keep minimize and close without
// Windows reserving a disabled maximize button between them.
inline constexpr DWORD PanelWindowStyle=WS_POPUP|WS_THICKFRAME|WS_SYSMENU|WS_MINIMIZEBOX|WS_CLIPCHILDREN;
// Fit the 984x1050 settings design uniformly, leaving the taskbar/work-area clear.
inline RECT centeredPanelRect(RECT work,UINT dpi){
    float scale=std::max(1u,dpi)/96.f;int margin=std::max(12,int(24*scale)),caption=int(32*scale);
    float fit=std::min({scale,(work.right-work.left-2*margin)/984.f,(work.bottom-work.top-2*margin-caption)/1050.f});
    fit=std::max(.1f,fit);int width=int(984*fit),height=int(1050*fit)+caption;
    int x=work.left+(work.right-work.left-width)/2,y=work.top+(work.bottom-work.top-height)/2;
    return {x,y,x+width,y+height};
}
inline RECT monitorWorkArea(HMONITOR monitor){MONITORINFO info{};info.cbSize=sizeof(info);if(GetMonitorInfoW(monitor,&info))return info.rcWork;RECT result{};SystemParametersInfoW(SPI_GETWORKAREA,0,&result,0);return result;}
inline LRESULT panelResizeHit(POINT p,RECT client,UINT dpi){
    int edge=MulDiv(6,int(dpi),96);bool left=p.x<edge,right=p.x>=client.right-edge,top=p.y<edge,bottom=p.y>=client.bottom-edge;
    if(top)return left?HTTOPLEFT:right?HTTOPRIGHT:HTTOP;
    if(bottom)return left?HTBOTTOMLEFT:right?HTBOTTOMRIGHT:HTBOTTOM;
    return left?HTLEFT:right?HTRIGHT:HTCLIENT;
}
inline int captionHeight(HWND w){return MulDiv(32,int(GetDpiForWindow(w)),96);}
inline int captionButtonWidth(HWND w){return MulDiv(46,int(GetDpiForWindow(w)),96);}
inline void paintCaptionButton(const DRAWITEMSTRUCT& d,bool close){
    fluent::initialize();bool hot=fluent::hovered(d.hwndItem),pressed=(d.itemState&ODS_SELECTED)!=0;
    COLORREF fill=close&&hot?(pressed?RGB(190,20,35):RGB(196,43,28)):hot?ui::palette.pressed:fluent::canvas;
    HBRUSH bg=CreateSolidBrush(fill);FillRect(d.hDC,&d.rcItem,bg);DeleteObject(bg);
    Gdiplus::Graphics g(d.hDC);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);float scale=(d.rcItem.bottom-d.rcItem.top)/32.f;
    float x=(d.rcItem.left+d.rcItem.right)*.5f,y=(d.rcItem.top+d.rcItem.bottom)*.5f,a=5*scale;Gdiplus::Pen pen(fluent::color(close&&hot?RGB(255,255,255):fluent::ink),1.2f*scale);
    if(close){g.DrawLine(&pen,x-a,y-a,x+a,y+a);g.DrawLine(&pen,x-a,y+a,x+a,y-a);}else g.DrawLine(&pen,x-a,y+2*scale,x+a,y+2*scale);
}
}
