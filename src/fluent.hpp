#pragma once
#include "common.hpp"
#include "ui_paint.hpp"
#include "theme.hpp"
#include "health.hpp"
#include "reticle.hpp"
#include <commctrl.h>
#include <uxtheme.h>
#include <gdiplus.h>
#include <algorithm>
#include <string>
#include <array>
namespace mc::fluent {
inline void initialize(){struct Runtime{ULONG_PTR token{};Runtime(){Gdiplus::GdiplusStartupInput input;Gdiplus::GdiplusStartup(&token,&input,nullptr);}~Runtime(){if(token)Gdiplus::GdiplusShutdown(token);}};static Runtime runtime;}
inline Gdiplus::Color color(COLORREF value,int alpha=255){return Gdiplus::Color(BYTE(alpha),GetRValue(value),GetGValue(value),GetBValue(value));}
inline COLORREF &accent=ui::palette.accent,&ink=ui::palette.ink,&muted=ui::palette.muted,&line=ui::palette.line,&surface=ui::palette.surface,&canvas=ui::palette.canvas;
inline COLORREF severityColor(Severity severity){return severity==Severity::Error?ui::palette.error:severity==Severity::Warning?ui::palette.warning:ink;}
inline void rounded(Gdiplus::Graphics& g,float x,float y,float w,float h,float radius,COLORREF fill,COLORREF border=CLR_INVALID){
    if(w<=0||h<=0)return;
    radius=std::min({radius,w*.5f,h*.5f});float d=radius*2;
    Gdiplus::GraphicsPath path;path.AddArc(x,y,d,d,180,90);path.AddArc(x+w-d,y,d,d,270,90);path.AddArc(x+w-d,y+h-d,d,d,0,90);path.AddArc(x,y+h-d,d,d,90,90);path.CloseFigure();
    Gdiplus::SolidBrush brush(color(fill));g.FillPath(&brush,&path);if(border!=CLR_INVALID){Gdiplus::Pen pen(color(border),1);g.DrawPath(&pen,&path);}
}
inline std::wstring label(HWND window){int n=GetWindowTextLengthW(window);std::wstring s(n+1,L'\0');GetWindowTextW(window,s.data(),n+1);s.resize(n);return s;}
inline void text(HDC dc,RECT r,const std::wstring& s,HFONT font,COLORREF tint,UINT flags=DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS){auto old=SelectObject(dc,font);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,tint);DrawTextW(dc,s.c_str(),int(s.size()),&r,flags);SelectObject(dc,old);}
enum class Kind{Button,Check,Combo,Slider,Label};
struct State{Kind kind;bool hot=false,tracking=false,dragging=false,redraw=true;int wheel=0;Severity severity=Severity::Normal;};
inline LRESULT CALLBACK proc(HWND,UINT,WPARAM,LPARAM,UINT_PTR,DWORD_PTR);
inline bool hovered(HWND window){DWORD_PTR ref=0;return GetWindowSubclass(window,proc,1,&ref)&&reinterpret_cast<State*>(ref)->hot;}
inline bool redrawEnabled(HWND w){DWORD_PTR ref=0;return !GetWindowSubclass(w,proc,1,&ref)||reinterpret_cast<State*>(ref)->redraw;}
inline Severity severity(HWND w){DWORD_PTR ref=0;return GetWindowSubclass(w,proc,1,&ref)?reinterpret_cast<State*>(ref)->severity:Severity::Normal;}
inline void setSeverity(HWND w,Severity value){DWORD_PTR ref=0;if(w&&GetWindowSubclass(w,proc,1,&ref)){auto state=reinterpret_cast<State*>(ref);if(state->severity!=value){state->severity=value;InvalidateRect(w,nullptr,FALSE);}}}
inline void buttonContent(const DRAWITEMSTRUCT& d,bool selected,int themeIcon=-1,int reticleIcon=-1,bool chinese=true){
    initialize();HDC dc=d.hDC;Gdiplus::Graphics g(dc);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    float h=float(d.rcItem.bottom-d.rcItem.top),scale=h/36.f;bool hot=hovered(d.hwndItem),pressed=(d.itemState&ODS_SELECTED)!=0;
    auto bg=reinterpret_cast<HBRUSH>(SendMessageW(GetParent(d.hwndItem),WM_CTLCOLORBTN,reinterpret_cast<WPARAM>(dc),reinterpret_cast<LPARAM>(d.hwndItem)));FillRect(dc,&d.rcItem,bg?bg:ui::surfaceBrush());
    COLORREF fill=selected?accent:(pressed?ui::palette.pressed:hot?ui::palette.hover:surface);
    rounded(g,float(d.rcItem.left)+.5f,float(d.rcItem.top)+.5f,float(d.rcItem.right-d.rcItem.left)-1,h-1,6*scale,fill,severity(d.hwndItem)!=Severity::Normal?severityColor(severity(d.hwndItem)):selected?fill:line);
    if(reticleIcon>=0){
        float width=float(d.rcItem.right-d.rcItem.left),x=(d.rcItem.left+d.rcItem.right)*.5f,y=d.rcItem.top+h*.34f,a=h*.21f;
        auto tint=selected?ui::palette.onAccent:ink;
        Gdiplus::Pen pen(color(tint),std::max(1.f,h/30.f));
        auto geometry=buildReticle(reticleIcon,{x,y},a);
        if(reticleIcon==0){g.DrawEllipse(&pen,x-a,y-a,a*2,a*2);g.DrawLine(&pen,x-a*.7f,y+a*.7f,x+a*.7f,y-a*.7f);}
        for(int i=0;i<geometry.lineCount;++i){auto l=geometry.lines[i];g.DrawLine(&pen,l.from.x,l.from.y,l.to.x,l.to.y);}
        for(int i=0;i<geometry.circleCount;++i){auto c=geometry.circles[i];Gdiplus::SolidBrush brush(color(tint));if(c.filled)g.FillEllipse(&brush,c.center.x-c.radius,c.center.y-c.radius,c.radius*2,c.radius*2);else g.DrawEllipse(&pen,c.center.x-c.radius,c.center.y-c.radius,c.radius*2,c.radius*2);}
        g.Flush();
        const wchar_t* caption=reticleName(reticleIcon,chinese);
        if(!chinese&&reticleIcon==2)caption=L"Dia. +";
        if(!chinese&&reticleIcon==5)caption=L"Open +";
        HFONT iconFont=CreateFontW(-std::max(9,int(h*.24f)),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        RECT labelRect{d.rcItem.left+1,LONG(d.rcItem.top+h*.61f),LONG(d.rcItem.left+width-1),d.rcItem.bottom-1};
        text(dc,labelRect,caption,iconFont,tint,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);DeleteObject(iconFont);
    }else if(themeIcon<0){RECT r=d.rcItem;InflateRect(&r,-int(8*scale),0);text(dc,r,label(d.hwndItem),reinterpret_cast<HFONT>(SendMessageW(d.hwndItem,WM_GETFONT,0,0)),IsWindowEnabled(d.hwndItem)?(selected?ui::palette.onAccent:severityColor(severity(d.hwndItem))):muted,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
    }else{
        // Vector icons stay crisp at any scale, independent of emoji fonts.
        Gdiplus::Pen pen(color(ink),1.7f*scale);pen.SetStartCap(Gdiplus::LineCapRound);pen.SetEndCap(Gdiplus::LineCapRound);
        auto sun=[&](float x,float y,float radius){g.DrawEllipse(&pen,x-radius,y-radius,radius*2,radius*2);for(int i=0;i<8;++i){float a=i*3.14159265f/4;g.DrawLine(&pen,x+std::cos(a)*(radius+2*scale),y+std::sin(a)*(radius+2*scale),x+std::cos(a)*(radius+4*scale),y+std::sin(a)*(radius+4*scale));}};
        auto moon=[&](float x,float y,float radius){Gdiplus::SolidBrush body(color(ink)),cutout(color(fill));g.FillEllipse(&body,x-radius,y-radius,2*radius,2*radius);g.FillEllipse(&cutout,x-radius+4*scale,y-radius-2*scale,2*radius,2*radius);};
        float x=(d.rcItem.left+d.rcItem.right)*.5f,y=(d.rcItem.top+d.rcItem.bottom)*.5f;
        if(themeIcon==1)sun(x,y,5*scale);
        else if(themeIcon==2)moon(x,y,9*scale);
        else {sun(x-6*scale,y-2*scale,3*scale);moon(x+5*scale,y+3*scale,6*scale);}
    }
    if((d.itemState&ODS_FOCUS) && !(SendMessageW(d.hwndItem,WM_QUERYUISTATE,0,0)&UISF_HIDEFOCUS)){Gdiplus::Pen focus(color(selected?ui::palette.onAccent:accent),1);g.DrawRectangle(&focus,3.f,3.f,float(d.rcItem.right)-6,h-6);}
}
inline void button(const DRAWITEMSTRUCT& d,bool selected,int themeIcon=-1){
    UiPaintBuffer frame(d.hDC,d.rcItem);auto content=d;content.hDC=frame.dc();buttonContent(content,selected,themeIcon);frame.present();
}
inline void reticleButton(const DRAWITEMSTRUCT& d,bool selected,int shape,bool chinese){
    UiPaintBuffer frame(d.hDC,d.rcItem);auto content=d;content.hDC=frame.dc();buttonContent(content,selected,-1,shape,chinese);frame.present();
}
inline void navigation(const DRAWITEMSTRUCT& d,bool selected){
    UiPaintBuffer frame(d.hDC,d.rcItem);HDC dc=frame.dc();FillRect(dc,&d.rcItem,ui::surfaceBrush());
    Gdiplus::Graphics g(dc);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);float h=float(d.rcItem.bottom),scale=h/36.f;
    if(hovered(d.hwndItem))rounded(g,0,0,float(d.rcItem.right),h,4*scale,ui::palette.hover);
    HFONT font=CreateFontW(-std::max(12,int(20*scale)),0,0,0,selected?FW_SEMIBOLD:FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI Variable Text");
    RECT r=d.rcItem;r.left+=int(12*scale);text(dc,r,label(d.hwndItem),font,selected?ink:muted);DeleteObject(font);
    if(selected)rounded(g,12*scale,h-3*scale,24*scale,3*scale,1.5f*scale,accent);
    if((d.itemState&ODS_FOCUS)&&!(SendMessageW(d.hwndItem,WM_QUERYUISTATE,0,0)&UISF_HIDEFOCUS)){RECT focus=d.rcItem;InflateRect(&focus,-3,-3);DrawFocusRect(dc,&focus);}frame.present();
}
inline void segment(const DRAWITEMSTRUCT& d,bool selected){
    UiPaintBuffer frame(d.hDC,d.rcItem);HDC dc=frame.dc();HBRUSH bg=CreateSolidBrush(ui::palette.hover);FillRect(dc,&d.rcItem,bg);DeleteObject(bg);
    Gdiplus::Graphics g(dc);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);float h=float(d.rcItem.bottom),width=float(d.rcItem.right);
    if(selected||hovered(d.hwndItem))rounded(g,1,2,width-2,h-4,4,selected?ui::palette.selected:ui::palette.pressed,selected?line:CLR_INVALID);
    RECT r=d.rcItem;InflateRect(&r,-6,0);text(dc,r,label(d.hwndItem),reinterpret_cast<HFONT>(SendMessageW(d.hwndItem,WM_GETFONT,0,0)),selected?ink:muted,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
    if((d.itemState&ODS_FOCUS)&&!(SendMessageW(d.hwndItem,WM_QUERYUISTATE,0,0)&UISF_HIDEFOCUS)){RECT focus=d.rcItem;InflateRect(&focus,-3,-3);DrawFocusRect(dc,&focus);}frame.present();
}
inline void comboItem(const DRAWITEMSTRUCT& d){
    initialize();if(d.itemState&ODS_COMBOBOXEDIT){InvalidateRect(d.hwndItem,nullptr,FALSE);return;}HBRUSH white=CreateSolidBrush(surface);FillRect(d.hDC,&d.rcItem,white);DeleteObject(white);if(d.itemID==UINT(-1))return;
    std::wstring value(size_t(SendMessageW(d.hwndItem,CB_GETLBTEXTLEN,d.itemID,0))+1,L'\0');SendMessageW(d.hwndItem,CB_GETLBTEXT,d.itemID,reinterpret_cast<LPARAM>(value.data()));value.resize(wcslen(value.c_str()));
    bool selected=(d.itemState&ODS_SELECTED)!=0;float s=(d.rcItem.bottom-d.rcItem.top)/36.f;
    if(selected){Gdiplus::Graphics g(d.hDC);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);rounded(g,float(d.rcItem.left)+4*s,float(d.rcItem.top)+2*s,float(d.rcItem.right-d.rcItem.left)-8*s,float(d.rcItem.bottom-d.rcItem.top)-4*s,5*s,ui::palette.selected);Gdiplus::Pen p(color(accent),3*s);g.DrawLine(&p,d.rcItem.left+6*s,d.rcItem.top+10*s,d.rcItem.left+6*s,d.rcItem.bottom-10*s);}
    RECT r=d.rcItem;r.left+=int(18*s);r.right-=int(12*s);text(d.hDC,r,value,reinterpret_cast<HFONT>(SendMessageW(d.hwndItem,WM_GETFONT,0,0)),ink);
}
struct SliderGeometry {float left,right,center,y,scale;};
inline SliderGeometry sliderGeometry(HWND w,RECT r){
    float scale=std::max(1.f,float(r.bottom))/30.f,left=12*scale,right=std::max(left,float(r.right)-12*scale);
    int low=int(SendMessageW(w,TBM_GETRANGEMIN,0,0)),high=int(SendMessageW(w,TBM_GETRANGEMAX,0,0)),value=int(SendMessageW(w,TBM_GETPOS,0,0));
    float ratio=high>low?std::clamp(float(value-low)/(high-low),0.f,1.f):0;
    return {left,right,std::lerp(left,right,ratio),r.bottom*.5f,scale};
}
inline void notifySlider(HWND w,int code){int value=int(SendMessageW(w,TBM_GETPOS,0,0));SendMessageW(GetParent(w),WM_HSCROLL,MAKEWPARAM(code,value),reinterpret_cast<LPARAM>(w));}
inline void slideTo(HWND w,int x){
    RECT r{};GetClientRect(w,&r);auto geometry=sliderGeometry(w,r);
    int low=int(SendMessageW(w,TBM_GETRANGEMIN,0,0)),high=int(SendMessageW(w,TBM_GETRANGEMAX,0,0));
    float ratio=geometry.right>geometry.left?std::clamp((x-geometry.left)/(geometry.right-geometry.left),0.f,1.f):0;
    int value=low+int(std::lround((high-low)*ratio));
    if(value==SendMessageW(w,TBM_GETPOS,0,0))return;
    SendMessageW(w,TBM_SETPOS,FALSE,value);
    // Present only the small slider surface before notifying the settings page.
    RedrawWindow(w,nullptr,nullptr,RDW_INVALIDATE|RDW_UPDATENOW);
    notifySlider(w,TB_THUMBTRACK);
}
inline void renderContent(HWND window,HDC dc,RECT r,State& state){
    initialize();HBRUSH background=ui::surfaceBrush();COLORREF foreground=ink;
    if(state.kind==Kind::Check||state.kind==Kind::Label){auto parent=reinterpret_cast<HBRUSH>(SendMessageW(GetParent(window),state.kind==Kind::Label?WM_CTLCOLORSTATIC:WM_CTLCOLORBTN,reinterpret_cast<WPARAM>(dc),reinterpret_cast<LPARAM>(window)));if(parent)background=parent;foreground=GetTextColor(dc);}
    FillRect(dc,&r,background);
    if(state.kind==Kind::Label){auto value=label(window);auto font=reinterpret_cast<HFONT>(SendMessageW(window,WM_GETFONT,0,0));UINT flags=DT_LEFT|DT_NOPREFIX;flags|=value.find(L'\n')!=std::wstring::npos?DT_TOP|DT_WORDBREAK:DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS;text(dc,r,value,font,foreground,flags);return;}
    {Gdiplus::Graphics g(dc);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);float w=float(r.right),h=float(r.bottom);bool focus=GetFocus()==window;auto font=reinterpret_cast<HFONT>(SendMessageW(window,WM_GETFONT,0,0));
    if(state.kind==Kind::Combo){
        float s=h/36;rounded(g,.5f,.5f,w-1,h-1,6*s,state.hot?ui::palette.hover:surface,focus?accent:line);
        Gdiplus::Pen bottom(color(focus?accent:ui::palette.bottom),focus?2.f:1.f);g.DrawLine(&bottom,6*s,h-1,w-6*s,h-1);
        int index=int(SendMessageW(window,CB_GETCURSEL,0,0));std::wstring value;if(index>=0){int n=int(SendMessageW(window,CB_GETLBTEXTLEN,index,0));value.resize(n+1);SendMessageW(window,CB_GETLBTEXT,index,reinterpret_cast<LPARAM>(value.data()));value.resize(n);}
        RECT content{LONG(12*s),0,LONG(w-36*s),r.bottom};text(dc,content,value,font,IsWindowEnabled(window)?ink:muted);
        Gdiplus::Pen chevron(color(muted),1.5f*s);chevron.SetStartCap(Gdiplus::LineCapRound);chevron.SetEndCap(Gdiplus::LineCapRound);g.DrawLine(&chevron,w-23*s,h*.46f,w-19*s,h*.57f);g.DrawLine(&chevron,w-19*s,h*.57f,w-15*s,h*.46f);
    }else if(state.kind==Kind::Check){
        float size=std::min(h-4,20.f*h/26),s=size/20,x=1,y=(h-size)*.5f;bool checked=SendMessageW(window,BM_GETCHECK,0,0)==BST_CHECKED;
        rounded(g,x,y,size,size,4*s,checked?accent:state.hot?ui::palette.hover:surface,state.severity!=Severity::Normal?severityColor(state.severity):checked?accent:focus?accent:ui::palette.bottom);
        if(checked){Gdiplus::Pen tick(color(ui::palette.onAccent),1.9f*s);tick.SetStartCap(Gdiplus::LineCapRound);tick.SetEndCap(Gdiplus::LineCapRound);g.DrawLine(&tick,x+5*s,y+10*s,x+9*s,y+14*s);g.DrawLine(&tick,x+9*s,y+14*s,x+16*s,y+6*s);}
        RECT label{LONG(size+10*s),0,r.right,r.bottom};text(dc,label,fluent::label(window),font,IsWindowEnabled(window)?severityColor(state.severity):muted);
    }else if(state.kind==Kind::Slider){
        auto geometry=sliderGeometry(window,r);float y=geometry.y,left=geometry.left,right=geometry.right,x=geometry.center,s=geometry.scale,rad=9*s;
        Gdiplus::Pen track(color(ui::palette.track),4*s);track.SetStartCap(Gdiplus::LineCapRound);track.SetEndCap(Gdiplus::LineCapRound);g.DrawLine(&track,left,y,right,y);
        Gdiplus::Pen progress(color(accent),4*s);progress.SetStartCap(Gdiplus::LineCapRound);progress.SetEndCap(Gdiplus::LineCapRound);g.DrawLine(&progress,left,y,std::clamp(x,left,right),y);
        Gdiplus::SolidBrush shadow(color(ui::palette.shadow,22));g.FillEllipse(&shadow,x-rad-1,y-rad+1,2*rad+2,2*rad+2);
        Gdiplus::SolidBrush body(color(surface));g.FillEllipse(&body,x-rad,y-rad,2*rad,2*rad);Gdiplus::Pen rim(color(focus?accent:line),1);g.DrawEllipse(&rim,x-rad,y-rad,2*rad,2*rad);
        float dot=(GetCapture()==window?4:state.hot?6.4f:5.5f)*s;Gdiplus::SolidBrush center(color(accent));g.FillEllipse(&center,x-dot,y-dot,2*dot,2*dot);
    }}
}
inline void paint(HWND window,State& state){PAINTSTRUCT ps{};HDC target=BeginPaint(window,&ps);RECT r{};GetClientRect(window,&r);UiPaintBuffer buffer(target,r);renderContent(window,buffer.dc(),r,state);buffer.present();EndPaint(window,&ps);
}
inline LRESULT CALLBACK proc(HWND w,UINT m,WPARAM a,LPARAM b,UINT_PTR,DWORD_PTR ref){
    auto state=reinterpret_cast<State*>(ref);
    if(m==WM_CTLCOLORLISTBOX){auto dc=reinterpret_cast<HDC>(a);SetBkColor(dc,surface);SetTextColor(dc,ink);return reinterpret_cast<LRESULT>(ui::surfaceBrush());}
    if(m==WM_NCDESTROY){RemoveWindowSubclass(w,proc,1);delete state;return DefSubclassProc(w,m,a,b);}
    if(m==WM_MOUSEMOVE&&!state->tracking){state->hot=state->tracking=true;TRACKMOUSEEVENT track{sizeof(track),TME_LEAVE,w,0};TrackMouseEvent(&track);InvalidateRect(w,nullptr,FALSE);}
    if(m==WM_MOUSELEAVE){state->hot=state->tracking=false;InvalidateRect(w,nullptr,FALSE);}
    if(m==WM_SETTEXT&&state->kind==Kind::Check){
        // Native checkbox text updates paint directly in the system theme.
        // Keep the native label storage, then redraw with our themed renderer.
        bool visible=(GetWindowLongPtrW(w,GWL_STYLE)&WS_VISIBLE)!=0;
        if(visible)DefSubclassProc(w,WM_SETREDRAW,FALSE,0);
        auto result=DefSubclassProc(w,m,a,b);
        if(visible)DefSubclassProc(w,WM_SETREDRAW,TRUE,0);
        InvalidateRect(w,nullptr,FALSE);return result;
    }
    if(state->kind==Kind::Combo){
        if(m==WM_SETREDRAW){state->redraw=a!=FALSE;return DefSubclassProc(w,m,a,b);}
        // Native combo data messages can draw directly, bypassing WM_PAINT.
        // Suspend that path while retaining the native selection/data model.
        if(m==CB_RESETCONTENT||m==CB_ADDSTRING||m==CB_INSERTSTRING||m==CB_DELETESTRING||m==CB_SETCURSEL||m==CB_SETITEMHEIGHT){
            bool suspend=state->redraw&&(GetWindowLongPtrW(w,GWL_STYLE)&WS_VISIBLE);
            if(suspend)DefSubclassProc(w,WM_SETREDRAW,FALSE,0);
            auto result=DefSubclassProc(w,m,a,b);
            if(suspend)DefSubclassProc(w,WM_SETREDRAW,TRUE,0);
            if(state->redraw)InvalidateRect(w,nullptr,FALSE);
            return result;
        }
    }
    if(m==WM_ERASEBKGND)return 1;
    if(m==WM_PRINTCLIENT && state->kind!=Kind::Button){RECT rect{};GetClientRect(w,&rect);renderContent(w,reinterpret_cast<HDC>(a),rect,*state);return 0;}
    if(m==WM_PAINT && state->kind!=Kind::Button){paint(w,*state);return 0;}
    if(state->kind==Kind::Slider){
        // Trackbar's fast thumb path paints directly outside WM_PAINT. Do not
        // invoke that path: retain its value/accessibility storage, but own the
        // pointer/keyboard interaction and invalidate the entire custom frame.
        if(m==TBM_SETPOS||m==TBM_SETRANGE||m==TBM_SETRANGEMIN||m==TBM_SETRANGEMAX){auto result=DefSubclassProc(w,m,FALSE,b);InvalidateRect(w,nullptr,FALSE);return result;}
        if(m==WM_SETFOCUS||m==WM_KILLFOCUS){InvalidateRect(w,nullptr,FALSE);return 0;}
        if(m==WM_LBUTTONDOWN||m==WM_LBUTTONDBLCLK){if(!IsWindowEnabled(w))return 0;SetFocus(w);state->dragging=true;SetCapture(w);slideTo(w,int(short(LOWORD(b))));return 0;}
        if(m==WM_MOUSEMOVE){if(state->dragging)slideTo(w,int(short(LOWORD(b))));return 0;}
        if(m==WM_LBUTTONUP){if(state->dragging){slideTo(w,int(short(LOWORD(b))));state->dragging=false;ReleaseCapture();notifySlider(w,TB_THUMBPOSITION);notifySlider(w,TB_ENDTRACK);}InvalidateRect(w,nullptr,FALSE);return 0;}
        if(m==WM_CAPTURECHANGED||m==WM_CANCELMODE){if(state->dragging){state->dragging=false;if(GetCapture()==w)ReleaseCapture();notifySlider(w,TB_ENDTRACK);}InvalidateRect(w,nullptr,FALSE);return 0;}
        if(m==WM_MOUSEWHEEL){state->wheel+=int(short(HIWORD(a)));int steps=state->wheel/WHEEL_DELTA;state->wheel%=WHEEL_DELTA;
            if(steps){int value=int(SendMessageW(w,TBM_GETPOS,0,0)),low=int(SendMessageW(w,TBM_GETRANGEMIN,0,0)),high=int(SendMessageW(w,TBM_GETRANGEMAX,0,0));SendMessageW(w,TBM_SETPOS,FALSE,std::clamp(value+steps,low,high));notifySlider(w,TB_THUMBPOSITION);notifySlider(w,TB_ENDTRACK);}return 0;}
        if(m==WM_KEYUP)return 0;
        if(m==WM_KEYDOWN){
            int value=int(SendMessageW(w,TBM_GETPOS,0,0)),low=int(SendMessageW(w,TBM_GETRANGEMIN,0,0)),high=int(SendMessageW(w,TBM_GETRANGEMAX,0,0)),page=std::max(1,int(SendMessageW(w,TBM_GETPAGESIZE,0,0))),code=TB_THUMBPOSITION;
            switch(a){case VK_LEFT:case VK_DOWN:value--;code=TB_LINEUP;break;case VK_RIGHT:case VK_UP:value++;code=TB_LINEDOWN;break;case VK_PRIOR:value-=page;code=TB_PAGEUP;break;case VK_NEXT:value+=page;code=TB_PAGEDOWN;break;case VK_HOME:value=low;code=TB_TOP;break;case VK_END:value=high;code=TB_BOTTOM;break;default:return DefSubclassProc(w,m,a,b);}
            SendMessageW(w,TBM_SETPOS,FALSE,std::clamp(value,low,high));notifySlider(w,code);notifySlider(w,TB_ENDTRACK);return 0;
        }
    }
    auto result=DefSubclassProc(w,m,a,b);
    if(m==WM_SETFOCUS||m==WM_KILLFOCUS||m==BM_SETCHECK||m==CB_SETCURSEL||m==WM_ENABLE||m==WM_KEYUP||m==WM_LBUTTONUP||(state->kind==Kind::Combo&&(m==WM_COMMAND||m==WM_MOUSEMOVE)))InvalidateRect(w,nullptr,FALSE);
    return result;
}
struct PopupState{int width=0,height=0,dpi=0;};
inline LRESULT CALLBACK popupProc(HWND w,UINT m,WPARAM a,LPARAM b,UINT_PTR,DWORD_PTR ref){
    auto state=reinterpret_cast<PopupState*>(ref);auto result=DefSubclassProc(w,m,a,b);
    if(m==WM_WINDOWPOSCHANGED){RECT r{};GetWindowRect(w,&r);int width=r.right-r.left,height=r.bottom-r.top,dpi=int(GetDpiForWindow(w));
        if(width!=state->width||height!=state->height||dpi!=state->dpi){state->width=width;state->height=height;state->dpi=dpi;int radius=MulDiv(12,dpi,96);auto region=CreateRoundRectRgn(0,0,width+1,height+1,radius,radius);if(!SetWindowRgn(w,region,TRUE))DeleteObject(region);}
    }
    if(m==WM_NCDESTROY){RemoveWindowSubclass(w,popupProc,2);delete state;}
    return result;
}
inline void install(HWND w,Kind kind){initialize();auto value=new State{kind};if(!SetWindowSubclass(w,proc,1,reinterpret_cast<DWORD_PTR>(value)))delete value;SetWindowTheme(w,L"Explorer",nullptr);if(kind==Kind::Combo){COMBOBOXINFO info{};info.cbSize=sizeof(info);if(GetComboBoxInfo(w,&info)&&info.hwndList){auto popup=new PopupState;if(!SetWindowSubclass(info.hwndList,popupProc,2,reinterpret_cast<DWORD_PTR>(popup)))delete popup;}}}
template<size_t N>bool setComboOptions(HWND w,const std::array<const wchar_t*,N>& values,int selected){
    bool rebuild=SendMessageW(w,CB_GETCOUNT,0,0)!=LRESULT(N);
    if(!rebuild)for(size_t i=0;i<N;++i){auto size=SendMessageW(w,CB_GETLBTEXTLEN,i,0);if(size==CB_ERR){rebuild=true;break;}std::wstring text(size_t(size)+1,L'\0');SendMessageW(w,CB_GETLBTEXT,i,reinterpret_cast<LPARAM>(text.data()));text.resize(size_t(size));if(text!=values[i]){rebuild=true;break;}}
    if(!rebuild&&SendMessageW(w,CB_GETCURSEL,0,0)==selected)return false;
    bool visible=(GetWindowLongPtrW(w,GWL_STYLE)&WS_VISIBLE)!=0&&redrawEnabled(w);
    if(visible)SendMessageW(w,WM_SETREDRAW,FALSE,0);
    if(rebuild){SendMessageW(w,CB_RESETCONTENT,0,0);for(auto value:values)SendMessageW(w,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(value));}
    if(SendMessageW(w,CB_GETCURSEL,0,0)!=selected)SendMessageW(w,CB_SETCURSEL,selected,0);
    if(visible){SendMessageW(w,WM_SETREDRAW,TRUE,0);InvalidateRect(w,nullptr,FALSE);}
    return rebuild;
}
inline void comboHeight(HWND w,int height){SendMessageW(w,CB_SETITEMHEIGHT,WPARAM(-1),height);SendMessageW(w,CB_SETITEMHEIGHT,0,height+4);}
}
