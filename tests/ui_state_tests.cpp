#include "theme.hpp"
#include "health.hpp"
#include "tooltips.hpp"
#include <cmath>
#include <iostream>
#include <cstdlib>
void require(bool b,const char* why){if(!b){std::cerr<<"FAIL: "<<why<<'\n';std::exit(1);}}
double lum(COLORREF c){auto v=[](int n){double s=n/255.;return s<=.04045?s/12.92:std::pow((s+.055)/1.055,2.4);};return .2126*v(GetRValue(c))+.7152*v(GetGValue(c))+.0722*v(GetBValue(c));}
double contrast(COLORREF a,COLORREF b){auto x=lum(a),y=lum(b);return (std::max(x,y)+.05)/(std::min(x,y)+.05);}
int main(){
    require(!mc::ui::chooseDark(1,true)&&mc::ui::chooseDark(2,false)&&mc::ui::chooseDark(0,true)&&!mc::ui::chooseDark(0,false),"explicit light/dark and follow-system resolution");
    for(auto p:{mc::ui::lightPalette(),mc::ui::darkPalette()}){require(contrast(p.ink,p.surface)>=4.5&&contrast(p.muted,p.surface)>=4.5&&contrast(p.onAccent,p.accent)>=4.5,"normal, secondary and selected text retain readable contrast");require(contrast(p.warning,p.surface)>=4.5&&contrast(p.error,p.surface)>=4.5,"warnings/errors remain readable in both themes");}
    mc::MotionSample motion;auto health=[&](bool selected,bool running,bool foreground,bool dynamic,std::string status){return mc::captureHealth(selected,running,foreground,dynamic,status,motion,10);};
    require(health(false,false,false,true,"Not started").severity==mc::Severity::Normal,"no target is not an error");require(health(true,false,true,true,"Starting WGC").reason==mc::HealthReason::Starting,"initialization is not reported as failure");
    require(health(true,true,true,true,"Capture readback error: failed").severity==mc::Severity::Error,"capture errors remain visible even while thread runs");require(health(true,true,false,true,"WGC active").severity==mc::Severity::Normal,"switching away intentionally pauses without warning");require(health(true,true,true,false,"WGC active").severity==mc::Severity::Normal,"static mode does not require optical flow");
    require(health(true,true,true,true,"WGC active").severity==mc::Severity::Warning,"waiting observation is amber rather than a crash");motion.timestamp=9.98;motion.confidence=.9;require(health(true,true,true,true,"WGC active").severity==mc::Severity::Normal,"recovery returns to normal");motion.confidence=.2;require(health(true,true,true,true,"WGC active").severity==mc::Severity::Warning,"low confidence shows warning without changing style");
    INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_WIN95_CLASSES};InitCommonControlsEx(&controls);auto instance=GetModuleHandleW(nullptr);HWND owner=CreateWindowW(L"STATIC",L"Tooltip component test",WS_OVERLAPPEDWINDOW,0,0,600,400,nullptr,nullptr,instance,nullptr);HWND button=CreateWindowW(L"BUTTON",L"Hint",WS_CHILD,0,0,200,36,owner,nullptr,instance,nullptr);
    {mc::ControlTips tips;tips.initialize(owner);require(tips.window()!=nullptr,"tooltip window exists");tips.set(button,L"Normal explanation");require(tips.text(button)==L"Normal explanation","normal control hint stored");tips.set(button,L"Conflict: retry automatically");require(tips.text(button).find(L"Conflict")!=std::wstring::npos,"error hint replaces normal text");mc::ui::setTheme(2);tips.applyTheme();require(SendMessageW(tips.window(),TTM_GETTIPTEXTCOLOR,0,0)==mc::ui::palette.ink,"tooltip follows dark text palette");mc::ui::setTheme(1);tips.applyTheme();}
    DestroyWindow(owner);std::cout<<"PASS: theme selection/contrast, status precedence and recovery, normal/error tooltips\n";
}
