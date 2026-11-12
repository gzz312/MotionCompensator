#include "window_frame.hpp"
#include <iostream>
#include <cstdlib>
#include <vector>
static int notifications=0,ends=0;
void require(bool ok,const char* why){if(!ok){std::cerr<<"FAIL: "<<why<<'\n';std::exit(1);}}
LRESULT CALLBACK ownerProc(HWND w,UINT m,WPARAM a,LPARAM b){if(m==WM_NCCALCSIZE)return 0;if(m==WM_CTLCOLORSTATIC||m==WM_CTLCOLORBTN){SetTextColor(reinterpret_cast<HDC>(a),mc::ui::palette.ink);return reinterpret_cast<LRESULT>(mc::ui::surfaceBrush());}if(m==WM_HSCROLL){++notifications;if(LOWORD(a)==TB_ENDTRACK)++ends;return 0;}return DefWindowProcW(w,m,a,b);}
int main(){
    for(auto work:{RECT{0,0,1920,1040},RECT{-2560,-200,0,1240},RECT{1200,100,2566,828}})for(UINT dpi:{96u,144u,192u}){
        auto r=mc::centeredPanelRect(work,dpi);int width=r.right-r.left,height=r.bottom-r.top,caption=MulDiv(32,int(dpi),96);
        require(r.left>=work.left&&r.right<=work.right&&r.top>=work.top&&r.bottom<=work.bottom,"startup fits selected monitor work area including negative origins");
        require(std::abs((r.left+r.right)-(work.left+work.right))<=1&&std::abs((r.top+r.bottom)-(work.top+work.bottom))<=1,"startup centered within one pixel");
        require(std::abs(width/1120.f-(height-caption)/940.f)<.002f,"startup preserves content aspect ratio at all tested DPIs");
    }
    require(mc::panelResizeHit({0,0},{0,0,600,400},96)==HTTOPLEFT&&mc::panelResizeHit({599,399},{0,0,600,400},96)==HTBOTTOMRIGHT&&mc::panelResizeHit({300,200},{0,0,600,400},96)==HTCLIENT,"custom frame preserves corner sizing and client hit testing");
    INITCOMMONCONTROLSEX init{sizeof(init),ICC_BAR_CLASSES};InitCommonControlsEx(&init);
    WNDCLASSW wc{};wc.hInstance=GetModuleHandleW(nullptr);wc.lpfnWndProc=ownerProc;wc.lpszClassName=L"FluentRenderTestOwner";RegisterClassW(&wc);
    HWND owner=CreateWindowW(wc.lpszClassName,L"Render component checks",mc::PanelWindowStyle,0,0,600,400,nullptr,nullptr,wc.hInstance,nullptr);
    require(owner!=nullptr,"panel window exists");SetWindowPos(owner,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);RECT outer{},client{};GetWindowRect(owner,&outer);GetClientRect(owner,&client);require(outer.right-outer.left==client.right&&outer.bottom-outer.top==client.bottom,"custom frame has no system-painted nonclient strip");auto style=GetWindowLongPtrW(owner,GWL_STYLE);require(!(style&WS_MAXIMIZEBOX)&&!(style&WS_CAPTION)&&(style&WS_MINIMIZEBOX),"native maximize slot removed while minimize is retained");
    HWND slider=CreateWindowW(TRACKBAR_CLASSW,L"",WS_CHILD|WS_VISIBLE|TBS_NOTICKS,0,0,360,36,owner,reinterpret_cast<HMENU>(1),wc.hInstance,nullptr);mc::fluent::install(slider,mc::fluent::Kind::Slider);
    RECT bounds{};GetClientRect(slider,&bounds);SendMessageW(slider,TBM_SETRANGE,TRUE,MAKELPARAM(1,24));
    ShowWindow(owner,SW_SHOWNOACTIVATE);UpdateWindow(owner);
    auto full=[&]{RECT dirty{};require(GetUpdateRect(slider,&dirty,FALSE)&&EqualRect(&dirty,&bounds),"every slider update invalidates the full custom surface");};
    for(int i=0;i<100;++i){ValidateRect(slider,nullptr);SendMessageW(slider,TBM_SETPOS,TRUE,i%2?3:22);full();}
    require(notifications==0,"profile loads do not generate user movement notifications");
    SendMessageW(slider,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(0,18));require(SendMessageW(slider,TBM_GETPOS,0,0)==1,"drag left clamps to minimum");
    for(int i=0;i<60;++i){ValidateRect(slider,nullptr);SendMessageW(slider,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(i%2?110:310,18));require(!GetUpdateRect(slider,nullptr,FALSE),"pointer movement presents the slider before returning to the message loop");}
    int countBeforeDuplicate=notifications;
    for(int i=0;i<120;++i)SendMessageW(slider,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(110,18));
    require(notifications==countBeforeDuplicate,"identical quantized positions do not notify settings repeatedly");
    SendMessageW(slider,WM_LBUTTONUP,0,MAKELPARAM(500,18));require(SendMessageW(slider,TBM_GETPOS,0,0)==24&&GetCapture()!=slider&&ends==1,"outside release clamps maximum and ends capture once");
    SendMessageW(slider,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(110,18));SendMessageW(slider,WM_CANCELMODE,0,0);auto before=SendMessageW(slider,TBM_GETPOS,0,0);SendMessageW(slider,WM_MOUSEMOVE,0,MAKELPARAM(310,18));require(SendMessageW(slider,TBM_GETPOS,0,0)==before&&GetCapture()!=slider,"cancelled drag cannot leave a stuck knob");
    SendMessageW(slider,TBM_SETPOS,TRUE,3);SendMessageW(slider,WM_KEYDOWN,VK_RIGHT,0);require(SendMessageW(slider,TBM_GETPOS,0,0)==4,"keyboard increment retained");SendMessageW(slider,WM_KEYDOWN,VK_LEFT,0);require(SendMessageW(slider,TBM_GETPOS,0,0)==3,"keyboard decrement retained");
    SendMessageW(slider,WM_MOUSEWHEEL,MAKEWPARAM(0,WHEEL_DELTA),0);require(SendMessageW(slider,TBM_GETPOS,0,0)==4,"wheel increment uses the custom update path");SendMessageW(slider,WM_MOUSEWHEEL,MAKEWPARAM(0,-WHEEL_DELTA),0);require(SendMessageW(slider,TBM_GETPOS,0,0)==3,"wheel decrement uses the custom update path");
    HDC screen=GetDC(nullptr),dc=CreateCompatibleDC(screen);BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=360;info.bmiHeader.biHeight=-36;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;void* pixels=nullptr;auto bitmap=CreateDIBSection(screen,&info,DIB_RGB_COLORS,&pixels,nullptr,0);auto old=SelectObject(dc,bitmap);
    for(int value:{24,1,23,2,24,1}){SendMessageW(slider,TBM_SETPOS,TRUE,value);SendMessageW(slider,WM_PRINTCLIENT,reinterpret_cast<WPARAM>(dc),PRF_CLIENT);GdiFlush();auto geometry=mc::fluent::sliderGeometry(slider,bounds);
        for(int x=int(geometry.center+15);x<int(geometry.right-3);++x){auto c=GetPixel(dc,x,18);require(!(GetRValue(c)<100&&GetGValue(c)<160&&GetBValue(c)>140),"old blue progress never remains to the right of the knob");}
    }
    mc::ui::setTheme(2);auto button=CreateWindowW(L"BUTTON",L"Theme switch render test",WS_CHILD|BS_OWNERDRAW,0,0,160,36,owner,nullptr,wc.hInstance,nullptr);mc::fluent::install(button,mc::fluent::Kind::Button);
    HBRUSH red=CreateSolidBrush(RGB(255,0,0));RECT dirty{0,0,160,36};FillRect(dc,&dirty,red);SendMessageW(button,WM_ERASEBKGND,reinterpret_cast<WPARAM>(dc),0);require(GetPixel(dc,20,18)==RGB(255,0,0),"owner-drawn button never invokes native white background erase");DeleteObject(red);DestroyWindow(button);mc::ui::setTheme(1);
    SendMessageW(slider,WM_PRINTCLIENT,reinterpret_cast<WPARAM>(dc),PRF_CLIENT);
    require(GetPixel(dc,300,18)!=mc::fluent::accent,"minimum position clears prior maximum progress");
    auto label=CreateWindowW(L"STATIC",L"A long label to clear",WS_CHILD|WS_VISIBLE,0,0,360,36,owner,nullptr,wc.hInstance,nullptr);mc::fluent::install(label,mc::fluent::Kind::Label);
    for(int mode:{1,2}){mc::ui::setTheme(mode);for(int i=0;i<20;++i){SetWindowTextW(label,L"Changing label text");SendMessageW(label,WM_PRINTCLIENT,reinterpret_cast<WPARAM>(dc),PRF_CLIENT);SetWindowTextW(label,L"");SendMessageW(label,WM_PRINTCLIENT,reinterpret_cast<WPARAM>(dc),PRF_CLIENT);GdiFlush();for(int x=0;x<360;x+=13)for(int y=0;y<36;y+=5)require(GetPixel(dc,x,y)==mc::ui::palette.surface,"buffered label clears long-to-empty transitions in light and dark without native background leftovers");}}
    DestroyWindow(label);
    auto combo=CreateWindowW(L"COMBOBOX",L"",WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST|CBS_OWNERDRAWFIXED|CBS_HASSTRINGS,0,0,360,180,owner,nullptr,wc.hInstance,nullptr);mc::fluent::install(combo,mc::fluent::Kind::Combo);mc::fluent::comboHeight(combo,30);
    auto options=std::array{L"Fixed",L"Brightness",L"Outline",L"Combined"};require(mc::fluent::setComboOptions(combo,options,1),"initial options are populated");
    for(int mode:{1,2}){mc::ui::setTheme(mode);for(int i=0;i<60;++i){require(!mc::fluent::setComboOptions(combo,options,i%4),"profile changes retain the existing option table");require(SendMessageW(combo,CB_GETCURSEL,0,0)==i%4&&SendMessageW(combo,CB_GETCOUNT,0,0)==4,"profile selection and options remain valid");SendMessageW(combo,WM_PRINTCLIENT,reinterpret_cast<WPARAM>(dc),PRF_CLIENT);GdiFlush();require(GetPixel(dc,300,15)==mc::ui::palette.surface,"collapsed combo background matches the active theme after profile changes");}}
    auto translated=std::array{L"固定",L"明暗",L"描边",L"组合"};require(mc::fluent::setComboOptions(combo,translated,2),"language changes still rebuild translated options");
    ShowWindow(combo,SW_HIDE);mc::fluent::setComboOptions(combo,options,0);require(!(GetWindowLongPtrW(combo,GWL_STYLE)&WS_VISIBLE),"updating hidden combos never makes them visible");ShowWindow(combo,SW_SHOW);SendMessageW(combo,WM_SETREDRAW,FALSE,0);SendMessageW(combo,CB_SETCURSEL,3,0);require(!mc::fluent::redrawEnabled(combo),"nested selection respects caller redraw suspension");mc::fluent::setComboOptions(combo,translated,1);require(!mc::fluent::redrawEnabled(combo),"option replacement respects caller redraw suspension");SendMessageW(combo,WM_SETREDRAW,TRUE,0);DestroyWindow(combo);
    auto check=CreateWindowW(L"BUTTON",L"Old label",WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,0,0,300,26,owner,nullptr,wc.hInstance,nullptr);mc::fluent::install(check,mc::fluent::Kind::Check);
    for(int mode:{1,2}){mc::ui::setTheme(mode);for(auto name:{L"New English label",L"中文标签"}){
        ValidateRect(check,nullptr);SetWindowTextW(check,name);RECT pending{};
        require(GetUpdateRect(check,&pending,FALSE),"changing a checkbox label schedules a complete themed repaint");
        require(mc::fluent::label(check)==name,"checkbox native text remains available to accessibility");
        SendMessageW(check,WM_PRINTCLIENT,reinterpret_cast<WPARAM>(dc),PRF_CLIENT);GdiFlush();
        require(GetPixel(dc,290,13)==mc::ui::palette.surface,"translated checkbox retains the correct themed background");
    }}DestroyWindow(check);
    auto reticleButton=CreateWindowW(L"BUTTON",L"Reticle",WS_CHILD|BS_OWNERDRAW,0,0,61,44,owner,nullptr,wc.hInstance,nullptr);mc::fluent::install(reticleButton,mc::fluent::Kind::Button);
    for(int mode:{1,2}){mc::ui::setTheme(mode);for(int shape=0;shape<mc::ReticleShapeCount;++shape)for(bool selected:{false,true}){
        DRAWITEMSTRUCT item{};item.hwndItem=reticleButton;item.hDC=dc;item.rcItem={0,0,61,36};
        mc::fluent::reticleButton(item,selected,shape,true);GdiFlush();
        require(GetPixel(dc,0,0)==mc::ui::palette.surface,"reticle button corners match light/dark surface");
        require(GetPixel(dc,30,2)==(selected?mc::ui::palette.accent:mc::ui::palette.surface),"reticle selected state has a filled blue background");
    }}DestroyWindow(reticleButton);
    SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);ReleaseDC(nullptr,screen);DestroyWindow(owner);
    std::cout<<"PASS: 100 profile changes, 60 alternating drag updates, full invalidation, pointer capture, keyboard and rendered progress clearing\n";
}
