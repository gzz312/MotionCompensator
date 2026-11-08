#pragma once
#include "common.hpp"
#include <algorithm>
namespace mc::ui {
struct Palette {
    COLORREF canvas,surface,ink,muted,line,accent,onAccent,hover,pressed,selected,track,shadow,warning,error,bottom;
};
inline Palette lightPalette(){return {RGB(245,247,250),RGB(255,255,255),RGB(32,36,43),RGB(99,107,119),RGB(218,222,229),RGB(0,95,184),RGB(255,255,255),RGB(246,248,251),RGB(232,235,240),RGB(234,242,253),RGB(215,221,230),RGB(30,40,55),RGB(147,82,0),RGB(182,27,43),RGB(172,179,189)};}
inline Palette darkPalette(){return {RGB(25,28,34),RGB(34,38,45),RGB(239,242,248),RGB(177,186,201),RGB(68,76,89),RGB(108,182,255),RGB(12,25,40),RGB(43,49,59),RGB(54,63,75),RGB(42,61,82),RGB(79,88,104),RGB(0,0,0),RGB(255,205,118),RGB(255,151,164),RGB(116,129,148)};}
inline Palette palette=lightPalette();inline bool dark=false;
inline bool systemDark(){DWORD value=1,bytes=sizeof(value);return RegGetValueW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",L"AppsUseLightTheme",RRF_RT_REG_DWORD,nullptr,&value,&bytes)==ERROR_SUCCESS&&value==0;}
inline bool chooseDark(int mode,bool system){return mode==2||(mode==0&&system);}
inline bool setTheme(int mode){bool next=chooseDark(std::clamp(mode,0,2),systemDark());bool changed=next!=dark;dark=next;palette=dark?darkPalette():lightPalette();return changed;}
inline HBRUSH surfaceBrush(){static HBRUSH brush{};static COLORREF old=CLR_INVALID;if(!brush||old!=palette.surface){if(brush)DeleteObject(brush);brush=CreateSolidBrush(palette.surface);old=palette.surface;}return brush;}
inline void setNativeTheme(HWND window){using Set=HRESULT(WINAPI*)(HWND,DWORD,LPCVOID,DWORD);if(auto module=LoadLibraryExW(L"dwmapi.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32)){auto set=loadFunction<Set>(module,"DwmSetWindowAttribute");if(set){BOOL value=dark;set(window,20,&value,sizeof(value));COLORREF border=0xFFFFFFFE,caption=palette.canvas,text=palette.ink;set(window,34,&border,sizeof(border));set(window,35,&caption,sizeof(caption));set(window,36,&text,sizeof(text));}FreeLibrary(module);}}
}
