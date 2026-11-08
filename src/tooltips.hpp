#pragma once
#include "theme.hpp"
#include <commctrl.h>
#include <uxtheme.h>
#include <map>
#include <string>
namespace mc {
class ControlTips {
    HWND owner_{},window_{};HFONT font_{};int width_=0,height_=0;std::map<HWND,std::wstring> texts_;
    static LRESULT CALLBACK popupProc(HWND w,UINT m,WPARAM a,LPARAM b,UINT_PTR,DWORD_PTR ref){auto self=reinterpret_cast<ControlTips*>(ref);auto result=DefSubclassProc(w,m,a,b);if(m==WM_WINDOWPOSCHANGED){RECT r{};GetWindowRect(w,&r);int width=r.right-r.left,height=r.bottom-r.top;if(width!=self->width_||height!=self->height_){self->width_=width;self->height_=height;int radius=MulDiv(10,int(GetDpiForWindow(w)),96);auto region=CreateRoundRectRgn(0,0,width+1,height+1,radius,radius);if(!SetWindowRgn(w,region,TRUE))DeleteObject(region);}}if(m==WM_NCDESTROY)RemoveWindowSubclass(w,popupProc,9);return result;}
public:
    ~ControlTips(){if(window_&&IsWindow(window_))DestroyWindow(window_);if(font_)DeleteObject(font_);}
    void initialize(HWND owner){owner_=owner;window_=CreateWindowExW(WS_EX_TOPMOST,TOOLTIPS_CLASSW,nullptr,WS_POPUP|TTS_ALWAYSTIP|TTS_NOPREFIX,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,owner,nullptr,GetModuleHandleW(nullptr),nullptr);if(!window_)return;font_=CreateFontW(-MulDiv(14,int(GetDpiForWindow(owner)),96),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");SendMessageW(window_,WM_SETFONT,reinterpret_cast<WPARAM>(font_),FALSE);SetWindowSubclass(window_,popupProc,9,reinterpret_cast<DWORD_PTR>(this));SendMessageW(window_,TTM_SETMAXTIPWIDTH,0,MulDiv(460,int(GetDpiForWindow(owner)),96));SendMessageW(window_,TTM_SETDELAYTIME,TTDT_INITIAL,500);SendMessageW(window_,TTM_SETDELAYTIME,TTDT_RESHOW,100);SendMessageW(window_,TTM_SETDELAYTIME,TTDT_AUTOPOP,15000);applyTheme();}
    void applyTheme(){if(!window_)return;SendMessageW(window_,TTM_POP,0,0);SetWindowTheme(window_,L"",L"");SendMessageW(window_,TTM_SETTIPBKCOLOR,ui::palette.surface,0);SendMessageW(window_,TTM_SETTIPTEXTCOLOR,ui::palette.ink,0);RedrawWindow(window_,nullptr,nullptr,RDW_INVALIDATE|RDW_FRAME|RDW_UPDATENOW);}
    void set(HWND control,std::wstring text){if(!window_||!control)return;auto it=texts_.find(control);if(it!=texts_.end()&&it->second==text)return;bool added=it==texts_.end();auto& value=texts_[control];value=std::move(text);TOOLINFOW info{};info.cbSize=sizeof(info);info.uFlags=TTF_IDISHWND|TTF_SUBCLASS;info.hwnd=owner_;info.uId=reinterpret_cast<UINT_PTR>(control);info.lpszText=value.data();SendMessageW(window_,added?TTM_ADDTOOLW:TTM_UPDATETIPTEXTW,0,reinterpret_cast<LPARAM>(&info));}
    const std::wstring& text(HWND control) const{static const std::wstring empty;auto it=texts_.find(control);return it==texts_.end()?empty:it->second;}
    HWND window() const{return window_;}
};
}
