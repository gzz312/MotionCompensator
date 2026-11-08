#include "usage.hpp"
#include "ui_paint.hpp"
#include <iostream>
#include <cstdlib>
void require(bool b,const char* why){if(!b){std::cerr<<why;std::exit(1);}}
static int bodyPaints=0,bodyTextSets=0,bodyFontSets=0;
LRESULT CALLBACK countBody(HWND w,UINT m,WPARAM a,LPARAM b,UINT_PTR,DWORD_PTR){if(m==WM_PAINT)++bodyPaints;if(m==WM_SETTEXT)++bodyTextSets;if(m==WM_SETFONT)++bodyFontSets;return DefSubclassProc(w,m,a,b);}
int main(){
    auto normalized=mc::usageLines(L"one\ntwo\r\nthree\rfour");require(normalized==L"one\r\ntwo\r\nthree\r\nfour","mixed line endings normalize once");
    HWND owner=CreateWindowW(L"STATIC",L"Guide component test",WS_OVERLAPPEDWINDOW,0,0,800,700,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_BAR_CLASSES};InitCommonControlsEx(&controls);
    auto slider=CreateWindowW(TRACKBAR_CLASSW,L"",WS_CHILD|TBS_NOTICKS,0,0,300,30,owner,nullptr,GetModuleHandleW(nullptr),nullptr);mc::fluent::install(slider,mc::fluent::Kind::Slider);
    SendMessageW(slider,TBM_SETRANGE,TRUE,MAKELPARAM(1,24));SendMessageW(slider,TBM_SETPOS,TRUE,3);SendMessageW(slider,WM_KEYDOWN,VK_RIGHT,0);require(SendMessageW(slider,TBM_GETPOS,0,0)==4,"styled slider retains native keyboard increment");SendMessageW(slider,WM_KEYDOWN,VK_LEFT,0);require(SendMessageW(slider,TBM_GETPOS,0,0)==3,"styled slider retains native keyboard decrement");
    auto checkbox=CreateWindowW(L"BUTTON",L"Styled check",WS_CHILD|BS_AUTOCHECKBOX,0,40,200,26,owner,nullptr,GetModuleHandleW(nullptr),nullptr);mc::fluent::install(checkbox,mc::fluent::Kind::Check);SendMessageW(checkbox,BM_CLICK,0,0);require(SendMessageW(checkbox,BM_GETCHECK,0,0)==BST_CHECKED,"styled checkbox retains native toggle state");
    auto combo=CreateWindowW(L"COMBOBOX",L"",WS_CHILD|CBS_DROPDOWNLIST|CBS_OWNERDRAWFIXED|CBS_HASSTRINGS,0,80,300,200,owner,nullptr,GetModuleHandleW(nullptr),nullptr);mc::fluent::install(combo,mc::fluent::Kind::Combo);SendMessageW(combo,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"First"));SendMessageW(combo,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Second"));SendMessageW(combo,CB_SETCURSEL,1,0);require(SendMessageW(combo,CB_GETCURSEL,0,0)==1,"styled combo retains native selection state");
    auto window=mc::UsageWindow::show(owner,true);require(window!=nullptr,"guide created");auto body=GetDlgItem(GetDlgItem(window,5),1);require(body!=nullptr,"body exists, not a white empty window");
    RECT rect{};GetClientRect(body,&rect);require(rect.right>400&&rect.bottom>300,"body has a readable rectangle");
    std::wstring text(GetWindowTextLengthW(body)+1,L'\0');GetWindowTextW(body,text.data(),int(text.size()));require(text.find(L"开始使用")!=std::wstring::npos,"Chinese instructions loaded");
    require(text.find(L"BV1saKg6XEvg")!=std::wstring::npos&&text.find(L"设计参考")!=std::wstring::npos,"agreed credits are in the user guide");
    require(SendMessageW(body,EM_GETFIRSTVISIBLELINE,0,0)==0,"guide starts at the beginning");
    auto scrollbar=GetDlgItem(window,4);require(scrollbar!=nullptr,"document has a theme-colored scrollbar");
    SendMessageW(body,WM_VSCROLL,SB_BOTTOM,0);POINT bottom{};SendMessageW(body,EM_GETSCROLLPOS,0,reinterpret_cast<LPARAM>(&bottom));require(bottom.y>0,"document can scroll to bottom");RECT barRect{};GetClientRect(scrollbar,&barRect);SendMessageW(scrollbar,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(barRect.right/2,barRect.bottom-6));SendMessageW(scrollbar,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(barRect.right/2,0));SendMessageW(scrollbar,WM_LBUTTONUP,0,MAKELPARAM(barRect.right/2,0));POINT top{};SendMessageW(body,EM_GETSCROLLPOS,0,reinterpret_cast<LPARAM>(&top));require(top.y==0,"custom scrollbar drag restores document to top");

    RECT clipRect{},bodyRect{};auto viewport=GetDlgItem(window,5);GetClientRect(viewport,&clipRect);GetClientRect(body,&bodyRect);require(clipRect.right==bodyRect.right,"rich edit text client matches the clipping viewport");GetWindowRect(body,&bodyRect);RECT viewportRect{};GetWindowRect(viewport,&viewportRect);require(bodyRect.right>viewportRect.right,"native scrollbar lies entirely beyond the clipping viewport");
    require(!(GetWindowLongPtrW(window,GWL_EXSTYLE)&WS_EX_COMPOSITED),"guide avoids whole-tree composition repaint feedback");
    RedrawWindow(window,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN|RDW_UPDATENOW);ValidateRect(body,nullptr);SetWindowSubclass(body,countBody,77,0);
    for(int i=0;i<100;++i){SendMessageW(window,WM_SIZE,SIZE_RESTORED,0);UpdateWindow(window);UpdateWindow(body);}
    require(bodyPaints==0&&bodyTextSets==0&&bodyFontSets==0,"identical restore sizes do not repaint or reload guide text/font");
    RECT beforeDrag{};GetWindowRect(body,&beforeDrag);SendMessageW(window,WM_ENTERSIZEMOVE,0,0);RECT outer{};GetWindowRect(window,&outer);SetWindowPos(window,nullptr,0,0,outer.right-outer.left-50,outer.bottom-outer.top-40,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);RECT duringDrag{};GetWindowRect(body,&duringDrag);require(!EqualRect(&beforeDrag,&duringDrag),"live resize updates document geometry before release");SendMessageW(window,WM_EXITSIZEMOVE,0,0);RECT afterDrag{};GetWindowRect(body,&afterDrag);require(EqualRect(&duringDrag,&afterDrag),"resize release retains the already adapted document geometry");
    ShowWindow(window,SW_MINIMIZE);ShowWindow(window,SW_RESTORE);require(bodyTextSets==0&&bodyFontSets==0,"minimize/restore does not reload text or recreate document font");RemoveWindowSubclass(body,countBody,77);
    SendMessageW(window,WM_COMMAND,MAKEWPARAM(3,BN_CLICKED),0);require(GetWindowTextLengthW(body)>50000,"full embedded license loads beyond legacy edit capacity");
    text.assign(GetWindowTextLengthW(body)+1,L'\0');GetWindowTextW(body,text.data(),int(text.size()));require(text.find(L"LicenseRef-MotionCompensator-Integration-1.0")!=std::wstring::npos && text.find(L"Standalone sale")!=std::wstring::npos,"legal tab includes complete project license alongside third-party notices");
    require(SendMessageW(body,EM_GETFIRSTVISIBLELINE,0,0)==0,"licenses start at the beginning");
    require(mc::UsageWindow::show(owner,false)==window,"existing window reused");text.assign(GetWindowTextLengthW(body)+1,L'\0');GetWindowTextW(body,text.data(),int(text.size()));require(text.find(L"Getting started")!=std::wstring::npos,"English guide replaces license and remains readable");
    HDC dc=GetDC(window),image=CreateCompatibleDC(dc);HBITMAP bitmap=CreateCompatibleBitmap(dc,16,16);auto old=SelectObject(image,bitmap);RECT bounds{0,0,16,16};HBRUSH red=CreateSolidBrush(RGB(230,20,30));FillRect(image,&bounds,red);
    {UiPaintBuffer buffer(image,bounds);HBRUSH blue=CreateSolidBrush(RGB(10,40,220));FillRect(buffer.dc(),&bounds,blue);DeleteObject(blue);require(GetPixel(image,4,4)==RGB(230,20,30),"incomplete redraw never touches visible surface");buffer.present();require(GetPixel(image,4,4)==RGB(10,40,220),"complete frame is presented atomically");}
    for(int mode:{2,1}){
        mc::ui::setTheme(mode);mc::UsageWindow::applyTheme();
        for(int id:{2,3}){DRAWITEMSTRUCT draw{};draw.CtlType=ODT_BUTTON;draw.CtlID=id;draw.hwndItem=GetDlgItem(window,id);draw.hDC=image;draw.rcItem={0,0,160,36};SendMessageW(window,WM_DRAWITEM,id,reinterpret_cast<LPARAM>(&draw));GdiFlush();require(GetPixel(image,0,0)==mc::ui::palette.canvas,"guide tab corners match theme canvas instead of system white");}
    }
    DeleteObject(red);SelectObject(image,old);DeleteObject(bitmap);DeleteDC(image);ReleaseDC(window,dc);
    DestroyWindow(window);window=mc::UsageWindow::show(owner,true);require(window&&GetWindowTextLengthW(GetDlgItem(GetDlgItem(window,5),1))>1000,"close and reopen recreates content");DestroyWindow(window);DestroyWindow(owner);
    std::cout<<"PASS: native guide body, Chinese/English, large notices, reopen, line endings and buffered presentation\n";
}
