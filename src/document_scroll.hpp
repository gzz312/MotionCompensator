#pragma once
#include "fluent.hpp"
#include <richedit.h>
namespace mc {
// The Rich Edit extends one scrollbar width beyond its clipping viewport.
// Its native scrollbar/model stays intact but cannot paint into the visible UI.
// This separate sibling owns the visible scrollbar; wheel/keys remain native.
class DocumentViewport {
    static LRESULT CALLBACK proc(HWND w,UINT m,WPARAM a,LPARAM b){
        if(m==WM_COMMAND||m==WM_NOTIFY||m==WM_CTLCOLOREDIT||m==WM_CTLCOLORSTATIC)return SendMessageW(GetParent(w),m,a,b);
        if(m==WM_ERASEBKGND)return 1;
        if(m==WM_PAINT){PAINTSTRUCT ps{};auto dc=BeginPaint(w,&ps);RECT r{};GetClientRect(w,&r);FillRect(dc,&r,ui::surfaceBrush());EndPaint(w,&ps);return 0;}
        return DefWindowProcW(w,m,a,b);
    }
public:
    static HWND create(HWND parent,HINSTANCE instance){WNDCLASSW wc{};wc.hInstance=instance;wc.lpfnWndProc=proc;wc.lpszClassName=L"MotionCompensatorDocumentViewport";RegisterClassW(&wc);return CreateWindowW(wc.lpszClassName,L"",WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN|WS_CLIPSIBLINGS,0,0,1,1,parent,reinterpret_cast<HMENU>(5),instance,nullptr);}
};
class DocumentScroll {
    struct State{HWND body{};bool dragging=false,hot=false;float grab=0;};
    struct Geometry{int minimum=0,maximum=0,position=0,page=0;float top=0,height=0,travel=0;};
    static Geometry geometry(HWND w,State* state){RECT rect{};GetClientRect(w,&rect);SCROLLINFO info{};info.cbSize=sizeof(info);info.fMask=SIF_ALL;GetScrollInfo(state->body,SB_VERT,&info);int range=std::max(1,info.nMax-info.nMin+1);int maximum=std::max(info.nMin,info.nMax-int(info.nPage)+1);float h=float(rect.bottom);float thumb=maximum>info.nMin?std::clamp(h*info.nPage/range,std::min(h,24.f*GetDpiForWindow(w)/96.f),h):h;float travel=h-thumb;float top=maximum>info.nMin?travel*(info.nPos-info.nMin)/(maximum-info.nMin):0;return {info.nMin,maximum,info.nPos,int(info.nPage),top,thumb,travel};}
    static void move(HWND w,State* state,int y){auto g=geometry(w,state);if(g.travel<=0)return;float t=std::clamp((y-state->grab)/g.travel,0.f,1.f);POINT point{};SendMessageW(state->body,EM_GETSCROLLPOS,0,reinterpret_cast<LPARAM>(&point));point.y=g.minimum+LONG(std::lround(t*(g.maximum-g.minimum)));SendMessageW(state->body,EM_SETSCROLLPOS,0,reinterpret_cast<LPARAM>(&point));InvalidateRect(w,nullptr,FALSE);}
    static void render(HWND w,HDC dc,State* state){RECT r{};GetClientRect(w,&r);HBRUSH brush=CreateSolidBrush(fluent::surface);FillRect(dc,&r,brush);DeleteObject(brush);auto geom=geometry(w,state);if(geom.maximum<=geom.minimum)return;fluent::initialize();Gdiplus::Graphics g(dc);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);float margin=state->hot||state->dragging?3.f:5.f;float width=std::max(3.f,float(r.right)-2*margin);fluent::rounded(g,margin,geom.top+2,width,std::max(4.f,geom.height-4),width*.5f,state->hot||state->dragging?ui::palette.muted:ui::palette.bottom);}
    static LRESULT CALLBACK proc(HWND w,UINT m,WPARAM a,LPARAM b){auto state=reinterpret_cast<State*>(GetWindowLongPtrW(w,GWLP_USERDATA));if(m==WM_NCCREATE){state=new State{static_cast<HWND>(reinterpret_cast<CREATESTRUCTW*>(b)->lpCreateParams)};SetWindowLongPtrW(w,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(state));}if(!state)return DefWindowProcW(w,m,a,b);
        if(m==WM_NCDESTROY){delete state;SetWindowLongPtrW(w,GWLP_USERDATA,0);return DefWindowProcW(w,m,a,b);}
        if(m==WM_ERASEBKGND)return 1;
        if(m==WM_PAINT){PAINTSTRUCT ps{};auto dc=BeginPaint(w,&ps);RECT r{};GetClientRect(w,&r);UiPaintBuffer buffer(dc,r);render(w,buffer.dc(),state);buffer.present();EndPaint(w,&ps);return 0;}
        if(m==WM_PRINTCLIENT){render(w,reinterpret_cast<HDC>(a),state);return 0;}
        if(m==WM_LBUTTONDOWN){SetFocus(state->body);auto g=geometry(w,state);int y=int(short(HIWORD(b)));if(y>=g.top&&y<=g.top+g.height){state->grab=y-g.top;state->dragging=true;SetCapture(w);}else SendMessageW(state->body,WM_VSCROLL,y<g.top?SB_PAGEUP:SB_PAGEDOWN,0);InvalidateRect(w,nullptr,FALSE);return 0;}
        if(m==WM_MOUSEMOVE){if(!state->hot){state->hot=true;TRACKMOUSEEVENT tracking{sizeof(tracking),TME_LEAVE,w,0};TrackMouseEvent(&tracking);}if(state->dragging)move(w,state,int(short(HIWORD(b))));InvalidateRect(w,nullptr,FALSE);return 0;}
        if(m==WM_MOUSELEAVE){state->hot=false;InvalidateRect(w,nullptr,FALSE);return 0;}
        if(m==WM_LBUTTONUP){if(state->dragging){move(w,state,int(short(HIWORD(b))));state->dragging=false;ReleaseCapture();}InvalidateRect(w,nullptr,FALSE);return 0;}
        if(m==WM_CAPTURECHANGED||m==WM_CANCELMODE){state->dragging=false;InvalidateRect(w,nullptr,FALSE);return 0;}
        if(m==WM_MOUSEWHEEL){SendMessageW(state->body,m,a,b);InvalidateRect(w,nullptr,FALSE);return 0;}
        return DefWindowProcW(w,m,a,b);
    }
public:
    static HWND create(HWND parent,HWND body,HINSTANCE instance){WNDCLASSW wc{};wc.hInstance=instance;wc.lpfnWndProc=proc;wc.lpszClassName=L"MotionCompensatorDocumentScroll";wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);RegisterClassW(&wc);return CreateWindowW(wc.lpszClassName,L"Scroll document",WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS,0,0,1,1,parent,reinterpret_cast<HMENU>(4),instance,body);}
};
}
