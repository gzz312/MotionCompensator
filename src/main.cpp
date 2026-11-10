#include "capture.hpp"
#include "overlay.hpp"
#include "input.hpp"
#include "preferences.hpp"
#include "preview_gdi.hpp"
#include "debug_panel.hpp"
#include "usage.hpp"
#include "release_version.hpp"
#include "embedded.hpp"
#include "ui_paint.hpp"
#include "fluent.hpp"
#include "window_frame.hpp"
#include "session_log.hpp"
#include "tooltips.hpp"
#include "health.hpp"
#include <commctrl.h>
#include <filesystem>
#include <fstream>
#include <vector>
#include <cwctype>
#include <iomanip>
#include <memory>
#include <psapi.h>
#include <commdlg.h>
#include <windowsx.h>
struct WindowEntry { HWND handle{}; DWORD pid{}; std::wstring exe,title; };
static WindowEntry describe(HWND hwnd) {
    WindowEntry w; w.handle=hwnd; GetWindowThreadProcessId(hwnd,&w.pid);
    wchar_t title[512]{}; GetWindowTextW(hwnd,title,512); w.title=title;
    // Query image name only. Never request VM_READ, VM_WRITE or process injection rights.
    HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,w.pid);
    if(process) { wchar_t path[32768]{}; DWORD n=32768;
        if(QueryFullProcessImageNameW(process,0,path,&n)) w.exe=std::filesystem::path(path).filename().wstring();
        CloseHandle(process); }
    std::transform(w.exe.begin(),w.exe.end(),w.exe.begin(),[](wchar_t c){return std::towlower(c);}); return w;
}
enum { List=101, Refresh, Attach, Auto, Mode, Visible, Stop, Radius, Opacity, Spacing, Language, DotColor, ReticleColor, ReticleOpacity, ReticleSize, ReticleShape, Hole, Staggered, BoundaryFade, TabRegional, TabEdge, TabStatic, Gain, Invert, Edges, DebugParameters, Instructions, Appearance, CaptionMinimize, CaptionClose, Theme, RecordLog, ReticleFirst=200 };
class App {
public:
    HWND hwnd{},list{},status{},autoBox{},visibleBox{},radius{},opacity{},spacing{};
    std::vector<WindowEntry> windows;
    WindowEntry selected;
    Capture capture;
    Overlay overlay;
    bool automatic=true,dynamic=true,visible=true,smoke=false,failed=false;
    bool captureOnly=false,overlayOnly=false,wasOverlayVisible=false;
    int captureRetries=0;double captureRetryAfter=0;
    double started=qpcSeconds(),lastLog=started,drawMs=0,drawMax=0;
    uint64_t draws=0,previousFrames=0,rawEvents=0;
    double rawTimestamp=0;
    double previousCpu=0;
    std::string previousCaptureStatus;
    mc::SessionLog log;
    std::filesystem::path logDirectory;
    bool diagnosticRecordLog=false;
    HWND logBox{},themeButton{},shortcutsLabel{};
    mc::ControlTips tips;
    mc::Health health;
    std::string captureDetails;
    std::array<bool,3> hotkeys{};
    double nextHotkeyRetry=0;
    std::wstring healthMessage;
    std::wstring error;
    bool chinese=true,flowDebug=false,showDebug=false;
    double lastDebug=0,fusionMs=0;
    uint64_t debugFrames=0,debugAnalyses=0,debugDraws=0;
    HWND debugButton{};
    mc::Preferences preferences;
    std::filesystem::path settingsPath;
    bool settingsDirty=false,controlsReady=false;
    double saveAfter=0;
    std::string settingsError;
    HWND appearanceBox{},captionMinimize{},captionClose{};
    float uiScale=1;int uiOffsetX=0,uiOffsetY=0;
    bool liveSizing=false;int layoutWidth=0,layoutHeight=0,layoutDpi=0,fontScalePixels=0;
    struct ControlRect{HWND window;int x,y,w,h;};
    std::vector<ControlRect> controlRects;
    HWND boundaryBox{},gain{},gainLabel{},invertBox{},edgesBox{};
    std::array<HWND,3> tabs{};
    std::array<HWND,mc::ReticleShapeCount> reticleButtons{};
    HFONT smallFont{};
    std::filesystem::path debugDirectory;
    mc::DynamicDriver driver; InputObserver inputs; double lastTick=qpcSeconds();
    std::vector<std::pair<HWND,std::wstring>> labels;
    const wchar_t* tr(const std::wstring& en) {
        if(!chinese) return en.c_str();
        static const std::vector<std::pair<std::wstring,std::wstring>> words={
            {L"MotionCompensator 0.2 | Capture + experimental motion estimation",L"MotionCompensator 0.2 | 外部采集与实验运动估计"},
            {L"Refresh windows",L"刷新窗口"},{L"Attach selected",L"选择此窗口"},{L"Stop capture",L"停止采集"},
            {L"Auto attach foreground known game",L"自动识别前台游戏"},
            {L"Show dots when selected game is foreground",L"游戏在前台时显示点阵"},
            {L"Dot radius",L"点大小"},{L"Opacity",L"透明度"},{L"Dot spacing",L"点间距"},
            {L"Waiting for a game. Select a window or launch PAYDAY 2.",L"等待游戏。可手动选择窗口，或自动识别前台游戏。"},
            {L"Ctrl+Alt+F8: next style    Ctrl+Alt+F9: show/hide\nCtrl+Alt+F10: stop",L"Ctrl+Alt+F8：切换方案    Ctrl+Alt+F9：显示/隐藏\nCtrl+Alt+F10：停止采集"}};
        static const std::vector<std::pair<std::wstring,std::wstring>> extra={
            {L"Theme",L"主题"},{L"Log",L"日志"},
            {L"Visual comfort, in your control.",L"让视野更舒适，让设置更直观。"},
            {L"Game connection",L"游戏连接"},{L"Center reticle",L"中央准星"},
            {L"Regional dots",L"分区点阵"},{L"Screen-edge dots",L"屏边点阵"},{L"Static dots",L"静态点阵"},
            {L"Boundary fade",L"边界淡化"},{L"Motion strength",L"运动强度"},{L"Reverse direction",L"反转运动方向"},
            {L"Appearance preview",L"外观预览"},{L"Connection & activity",L"连接与运行状态"},
            {L"Show overlay",L"显示浮层"},{L"Staggered dots",L"点阵交错"},{L"Show debug parameters",L"显示调试参数"},{L"User Guide",L"使用说明"},
            {L"Choose dot color",L"选择点阵颜色"},{L"Choose reticle color",L"选择准星颜色"},
            {L"Transparency",L"透明度"},{L"Clear center (%)",L"中央留空（%）"},
            {L"Background",L"背景适配"},
            {L"Reticle shape",L"准星形状"},{L"Reticle size",L"准星大小"}};
        for(const auto& pair:extra) if(pair.first==en) return pair.second.c_str();
        for(const auto& pair:words) if(pair.first==en) return pair.second.c_str();
        return en.c_str();
    }
    HFONT font{},titleFont{};
    HWND reticleOpacity{},reticleSize{},hole{},staggerBox{};
    mc::Color dotColor{},crossColor{.2f,1.f,.45f};
    HBRUSH background=CreateSolidBrush(mc::ui::palette.canvas),card=CreateSolidBrush(mc::ui::palette.surface);
    mc::VisualSettings visualSettings() {
        auto v=mc::visualSettingsFromPreferences(preferences);
        if(!controlsReady)return v;
        v.style=preferences.selected;
        auto profile=preferences.profiles[preferences.selected];
        v.edges=profile.edges;v.edgeWidth=profile.edgeWidth;
        if(appearanceBox)v.appearance=std::max(0,int(SendMessageW(appearanceBox,CB_GETCURSEL,0,0)));
        if(staggerBox) v.staggered=SendMessageW(staggerBox,BM_GETCHECK,0,0)==BST_CHECKED;
        if(boundaryBox) v.boundaryFade=SendMessageW(boundaryBox,BM_GETCHECK,0,0)==BST_CHECKED;
        if(edgesBox) v.edges=std::max(0,int(SendMessageW(edgesBox,CB_GETCURSEL,0,0)));
        v.radius=float(SendMessageW(radius,TBM_GETPOS,0,0));
        v.opacity=1-float(SendMessageW(opacity,TBM_GETPOS,0,0))/100;
        v.spacing=float(SendMessageW(spacing,TBM_GETPOS,0,0));
        v.dots=dotColor;v.reticle=crossColor;
        v.reticleOpacity=1-float(SendMessageW(reticleOpacity,TBM_GETPOS,0,0))/100;
        v.reticleSize=float(SendMessageW(reticleSize,TBM_GETPOS,0,0));
        v.holeDiameter=profile.hole;
        if(v.style==1) v.edgeWidth=float(SendMessageW(hole,TBM_GETPOS,0,0));
        else v.holeDiameter=float(SendMessageW(hole,TBM_GETPOS,0,0))/100;
        v.reticleShape=preferences.reticleShape;return v;
    }
    static void setText(HWND control,const std::wstring& value){
        int n=GetWindowTextLengthW(control);std::wstring old(n+1,L'\0');GetWindowTextW(control,old.data(),n+1);old.resize(n);
        if(old!=value)SetWindowTextW(control,value.c_str());
    }
    void invalidatePreview(){RECT r{uiOffsetX+int(600*uiScale),uiOffsetY+int(100*uiScale),uiOffsetX+int(964*uiScale),uiOffsetY+int(986*uiScale)};InvalidateRect(hwnd,&r,FALSE);}
    void refreshValueLabels() {
        if(!radius || !reticleSize) return;
        auto v=visualSettings();int transparencyLabel=0;
        for(const auto& label:labels) {
            std::wstring text=tr(label.second),value;
            if(label.second==L"Dot radius") value=std::to_wstring(int(v.radius))+L" px";
            if(label.second==L"Dot spacing"){if(v.style==2){text=chinese?L"排布展开":L"Pattern spread";value=std::to_wstring(int(std::round((v.spacing-60)/180*100)))+L"%";}else value=std::to_wstring(int(v.spacing))+L" px";}
            if(label.second==L"Reticle size") value=std::to_wstring(int(v.reticleSize))+L" px";
            if(label.second==L"Clear center (%)") {
                text=preferences.selected==1?(chinese?L"边缘带宽":L"Edge width"):(chinese?L"中央留空":L"Clear center");
                value=preferences.selected==1?std::to_wstring(int(v.edgeWidth))+L" px":std::to_wstring(int(std::round(v.holeDiameter*100)))+L"%";
            }
            if(label.second==L"Motion strength") {if(!chinese)text=L"Strength";value=std::to_wstring(int(SendMessageW(gain,TBM_GETPOS,0,0)))+L"%";}
            if(label.second==L"Transparency") value=std::to_wstring(int(std::round((1-(transparencyLabel++==0?v.opacity:v.reticleOpacity))*100)))+L"%";
            if(!value.empty()) setText(label.first,text+L"  "+value);
        }
    }
    const wchar_t* styleName(int style) {
        if(chinese) return style==0?L"分区点阵":style==1?L"屏边点阵":L"静态点阵";
        return style==0?L"Regional dots":style==1?L"Screen-edge dots":L"Static dots";
    }
    void populateEdges(int index) {
        mc::fluent::setComboOptions(edgesBox,chinese?std::array{L"左右两侧",L"上下两侧",L"四周边缘"}:std::array{L"Left and right",L"Top and bottom",L"All four edges"},index);
    }
    void collectSettings() {
        if(!controlsReady)return;
        auto v=visualSettings();auto& p=preferences.profiles[preferences.selected];
        p.radius=v.radius;p.opacity=v.opacity;p.spacing=v.spacing;p.color=v.dots;p.hole=v.holeDiameter;p.edgeWidth=v.edgeWidth;
        p.appearance=v.appearance;p.staggered=v.staggered;p.boundaryFade=v.boundaryFade;p.edges=v.edges;
        p.gain=float(SendMessageW(gain,TBM_GETPOS,0,0))/100;
        p.inverted=SendMessageW(invertBox,BM_GETCHECK,0,0)==BST_CHECKED;
        preferences.reticleShape=v.reticleShape;preferences.reticleSize=v.reticleSize;preferences.reticleOpacity=v.reticleOpacity;preferences.reticleColor=v.reticle;
        preferences.chinese=chinese;preferences.visible=visible;preferences.automatic=automatic;
        mc::sanitize(preferences);
    }
    void markSettings() {collectSettings();settingsDirty=true;saveAfter=qpcSeconds()+.5;invalidatePreview();updateTips();}
    void flushSettings() {
        if(!settingsDirty || smoke)return;
        collectSettings();
        if(mc::saveUserPreferences(preferences,settingsError)) settingsDirty=false;
        else saveAfter=qpcSeconds()+5;
    }
    void populateAppearance(int index){
        mc::fluent::setComboOptions(appearanceBox,chinese?std::array{L"固定颜色",L"同色相明暗",L"同色相双层描边",L"明暗 + 双层描边"}:std::array{L"Fixed color",L"Adaptive brightness",L"Two-tone outline",L"Brightness + outline"},index);
    }
    void loadProfileControls() {
        const auto& p=preferences.profiles[preferences.selected];dotColor=p.color;
        SendMessageW(radius,TBM_SETRANGE,TRUE,MAKELPARAM(1,mc::dotRadiusMax(preferences.selected)));
        SendMessageW(radius,TBM_SETPOS,TRUE,int(p.radius));SendMessageW(opacity,TBM_SETPOS,TRUE,int(std::round((1-p.opacity)*100)));
        SendMessageW(spacing,TBM_SETPOS,TRUE,int(p.spacing));
        SendMessageW(hole,TBM_SETRANGE,TRUE,preferences.selected==1?MAKELPARAM(40,320):MAKELPARAM(5,25));
        SendMessageW(hole,TBM_SETPOS,TRUE,preferences.selected==1?int(p.edgeWidth):int(std::round(p.hole*100)));
        SendMessageW(staggerBox,BM_SETCHECK,p.staggered?BST_CHECKED:BST_UNCHECKED,0);
        SendMessageW(boundaryBox,BM_SETCHECK,p.boundaryFade?BST_CHECKED:BST_UNCHECKED,0);
        SendMessageW(gain,TBM_SETPOS,TRUE,int(std::round(p.gain*100)));
        SendMessageW(invertBox,BM_SETCHECK,p.inverted?BST_CHECKED:BST_UNCHECKED,0);
        populateEdges(p.edges);populateAppearance(p.appearance);
        dynamic=preferences.selected!=2;
        ShowWindow(staggerBox,SW_SHOW);ShowWindow(boundaryBox,preferences.selected==0?SW_SHOW:SW_HIDE);
        ShowWindow(edgesBox,preferences.selected==1?SW_SHOW:SW_HIDE);
        for(HWND c:{gain,gainLabel,invertBox}) ShowWindow(c,dynamic?SW_SHOW:SW_HIDE);
        refreshValueLabels();if(controlsReady)updateTips();for(auto tab:tabs) InvalidateRect(tab,nullptr,FALSE);
    }
    void selectStyle(int style) {
        collectSettings();preferences.selected=std::clamp(style,0,2);loadProfileControls();
        driver.reset();overlay.resetMotion();markSettings();lastLog=qpcSeconds()-1;
    }
    void populateShapes(int index=0) {
        for(int i=0;i<mc::ReticleShapeCount;++i)if(reticleButtons[i]){
            SetWindowTextW(reticleButtons[i],mc::reticleName(i,chinese));
            InvalidateRect(reticleButtons[i],nullptr,FALSE);
        }
        (void)index;
    }
    static COLORREF rgb(mc::Color c) {return RGB(int(c.r*255),int(c.g*255),int(c.b*255));}
    void chooseColor(mc::Color& color) {
        static COLORREF custom[16]{};CHOOSECOLORW cc{};cc.lStructSize=sizeof(cc);cc.hwndOwner=hwnd;cc.rgbResult=rgb(color);cc.lpCustColors=custom;cc.Flags=CC_FULLOPEN|CC_RGBINIT;
        if(ChooseColorW(&cc)) color={GetRValue(cc.rgbResult)/255.f,GetGValue(cc.rgbResult)/255.f,GetBValue(cc.rgbResult)/255.f};
        InvalidateRect(hwnd,nullptr,FALSE);
    }
    void layout(){
        if(!controlsReady||IsIconic(hwnd))return;
        RECT client{};GetClientRect(hwnd,&client);int dpi=int(GetDpiForWindow(hwnd));
        if(client.right<=0||client.bottom<=0||(layoutWidth==client.right&&layoutHeight==client.bottom&&layoutDpi==dpi))return;
        layoutWidth=client.right;layoutHeight=client.bottom;layoutDpi=dpi;
        int caption=mc::captionHeight(hwnd);uiScale=std::min(client.right/984.f,std::max(1L,client.bottom-caption)/1050.f);
        uiOffsetX=int((client.right-984*uiScale)*.5f);uiOffsetY=caption+int((client.bottom-caption-1050*uiScale)*.5f);
        UiLayoutBatch batch(hwnd,int(controlRects.size())+2);
        int captionWidth=mc::captionButtonWidth(hwnd);
        if(captionMinimize)batch.place(captionMinimize,client.right-2*captionWidth,0,captionWidth,caption);
        if(captionClose)batch.place(captionClose,client.right-captionWidth,0,captionWidth,caption);
        for(auto c:controlRects)batch.place(c.window,uiOffsetX+int(c.x*uiScale),uiOffsetY+int(c.y*uiScale),int(c.w*uiScale),int(c.h*uiScale));
        int scalePixels=std::max(1,int(18*uiScale))*10000+std::max(1,int(14*uiScale))*100+std::max(1,int(30*uiScale));
        if(fontScalePixels!=scalePixels){
            fontScalePixels=scalePixels;HFONT oldFont=font,oldSmall=smallFont,oldTitle=titleFont;
            auto make=[&](int size,int weight){return CreateFontW(-std::max(1,int(size*uiScale)),0,0,0,weight,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI Variable Text");};
            font=make(18,FW_NORMAL);smallFont=make(14,FW_NORMAL);titleFont=make(30,FW_SEMIBOLD);
            for(auto c:controlRects)SendMessageW(c.window,WM_SETFONT,reinterpret_cast<WPARAM>(c.y==20?titleFont:c.window==shortcutsLabel?smallFont:font),FALSE);
            DeleteObject(oldFont);DeleteObject(oldSmall);DeleteObject(oldTitle);
        }
        batch.commit();
        for(HWND combo:{list,edgesBox,appearanceBox})mc::fluent::comboHeight(combo,int(30*uiScale));
        RedrawWindow(hwnd,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN);
    }
    void paint() {
        PAINTSTRUCT paint{};HDC screen=BeginPaint(hwnd,&paint);RECT client{};GetClientRect(hwnd,&client);UiPaintBuffer buffer(screen,client);HDC dc=buffer.dc();FillRect(dc,&client,background);
        HFONT captionFont=CreateFontW(-MulDiv(13,int(GetDpiForWindow(hwnd)),96),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        RECT captionRect{MulDiv(12,int(GetDpiForWindow(hwnd)),96),0,client.right-2*mc::captionButtonWidth(hwnd),mc::captionHeight(hwnd)};mc::fluent::text(dc,captionRect,L"MotionCompensator",captionFont,mc::fluent::ink);DeleteObject(captionFont);
        SetMapMode(dc,MM_ANISOTROPIC);SetWindowExtEx(dc,984,1050,nullptr);SetViewportExtEx(dc,int(984*uiScale),int(1050*uiScale),nullptr);SetViewportOrgEx(dc,uiOffsetX,uiOffsetY,nullptr);
        HPEN border=CreatePen(PS_SOLID,1,mc::ui::palette.line);auto savedPen=SelectObject(dc,border);SelectObject(dc,card);
        for(RECT r:std::array{RECT{24,100,580,292},RECT{24,310,580,726},RECT{24,744,580,986},RECT{600,100,964,986}})
            RoundRect(dc,r.left,r.top,r.right,r.bottom,20,20);
        SelectObject(dc,savedPen);DeleteObject(border);
        // A settings preview, not a simulated game. Uses the same mask as the overlay.
        RECT preview{620,158,944,430};HBRUSH dark=CreateSolidBrush(RGB(20,29,44));FillRect(dc,&preview,dark);DeleteObject(dark);
        auto v=visualSettings();
        int sourceW=1920,sourceH=1080;RECT targetSize{};
        if(selected.handle && GetClientRect(selected.handle,&targetSize) && targetSize.right>0 && targetSize.bottom>0){sourceW=targetSize.right;sourceH=targetSize.bottom;}
        auto scene=mc::buildPreview(v,324,272,sourceW,sourceH);
        for(int x=int(std::ceil(scene.contentLeft));x<int(scene.contentRight);++x){int shade=24+int(196.f*(x-scene.contentLeft)/std::max(1.f,scene.contentRight-scene.contentLeft));HBRUSH bg=CreateSolidBrush(RGB(shade,shade,shade));RECT strip{620+x,158+int(std::ceil(scene.contentTop)),621+x,158+int(std::floor(scene.contentBottom))};FillRect(dc,&strip,bg);DeleteObject(bg);}
        paintPreview(dc,{620,158},scene);
        SetBkMode(dc,TRANSPARENT);SetTextColor(dc,mc::fluent::muted);HFONT paintFont=CreateFontW(-18,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");auto savedFont=SelectObject(dc,paintFont);
        auto text=[&](int x,int y,std::wstring value){TextOutW(dc,x,y,value.c_str(),int(value.size()));};
        text(620,454,styleName(preferences.selected));
        text(620,480,chinese?L"调整设置即时预览，切回游戏应用。":L"Live preview. Return to the game to apply.");
        text(620,520,chinese?L"明暗背景对照预览":L"Light / dark background comparison");
        if(!dynamic)text(620,548,chinese?L"4 × 4 · 外列柔边外圈":L"4 x 4 / soft halos on outer columns");
        RECT swatch{620,670,640,686};HBRUSH b=CreateSolidBrush(rgb(v.dots));FillRect(dc,&swatch,b);DeleteObject(b);text(648,664,chinese?L"点阵":L"Dots");
        swatch={770,670,790,686};b=CreateSolidBrush(rgb(v.reticle));FillRect(dc,&swatch,b);DeleteObject(b);text(798,664,chinese?L"准星":L"Reticle");
        SelectObject(dc,savedFont);DeleteObject(paintFont);buffer.present();EndPaint(hwnd,&paint);
    }

    ~App() { if(smallFont) DeleteObject(smallFont);if(font) DeleteObject(font);if(titleFont) DeleteObject(titleFont);DeleteObject(background);DeleteObject(card); }
    bool keysMissing() const{return !hotkeys[0]||!hotkeys[1]||!hotkeys[2];}
    void registerKeys(){
        for(int i=0;i<3;++i)if(!hotkeys[i])hotkeys[i]=RegisterHotKey(hwnd,i+1,MOD_CONTROL|MOD_ALT|MOD_NOREPEAT,VK_F8+i)!=FALSE;
        nextHotkeyRetry=qpcSeconds()+3;
    }
    void populateTheme(){
        if(!themeButton)return;
        const auto name=chinese?std::array{L"跟随系统",L"浅色",L"深色"}:std::array{L"Follow system",L"Light",L"Dark"};
        SetWindowTextW(themeButton,((chinese?std::wstring(L"主题："):std::wstring(L"Theme: "))+name[preferences.theme]).c_str());
        InvalidateRect(themeButton,nullptr,FALSE);
    }
    void applyTheme(bool force=false){
        bool changed=mc::ui::setTheme(preferences.theme);if(!changed&&!force)return;
        bool visiblePanel=IsWindowVisible(hwnd);if(visiblePanel)SendMessageW(hwnd,WM_SETREDRAW,FALSE,0);
        DeleteObject(background);DeleteObject(card);background=CreateSolidBrush(mc::fluent::canvas);card=CreateSolidBrush(mc::fluent::surface);
        mc::ui::setNativeTheme(hwnd);
        for(HWND combo:{list,edgesBox,appearanceBox})if(combo){COMBOBOXINFO info{};info.cbSize=sizeof(info);if(GetComboBoxInfo(combo,&info)&&info.hwndList)RedrawWindow(info.hwndList,nullptr,nullptr,RDW_INVALIDATE|RDW_FRAME|RDW_UPDATENOW);}
        tips.applyTheme();mc::UsageWindow::applyTheme();
        if(visiblePanel)SendMessageW(hwnd,WM_SETREDRAW,TRUE,0);
        RedrawWindow(hwnd,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN|RDW_FRAME|RDW_UPDATENOW);
    }
    std::wstring hint(int id) const{
        auto choose=[&](const wchar_t* cn,const wchar_t* en){return std::wstring(chinese?cn:en);};
        if(id>=ReticleFirst&&id<ReticleFirst+mc::ReticleShapeCount){
            return std::wstring(mc::reticleName(id-ReticleFirst,chinese))+(chinese?L"：点击切换准星。颜色、透明度和大小由下方控件调整，三种点阵方案共用。":L": select this reticle. Color, transparency and size are shared by all dot styles.");
        }
        switch(id){
        case List:return choose(L"列出可手动连接的窗口。列表中的候选窗口不一定是当前自动连接的游戏；实际目标见右侧状态。",L"Choose a window for manual attachment. The active auto-attached target is shown in the status panel.");
        case Refresh:return choose(L"刷新可选择的窗口列表，不改变当前连接。",L"Refresh the window list without changing the active connection.");
        case Attach:return choose(L"连接列表中的窗口，并关闭自动识别；切回该窗口后显示浮层。",L"Attach the selected window and turn off auto detection. Return to that window to show the overlay.");
        case Auto:return choose(L"自动连接前台已识别游戏。切出游戏时隐藏浮层，切回后恢复。",L"Auto-attach a recognized foreground game. The overlay hides when you switch away.");
        case Stop:return choose(L"停止采集并关闭自动连接。快捷键：Ctrl+Alt+F10。",L"Stop capture and auto attachment. Shortcut: Ctrl+Alt+F10.");
        case Visible:return choose(L"控制游戏前台浮层显示，不改变当前点阵方案。快捷键：Ctrl+Alt+F9。",L"Show or hide the foreground overlay without changing style. Shortcut: Ctrl+Alt+F9.");
        case TabRegional:return choose(L"九宫格分别持续滚动，使用本方案独立设置。Ctrl+Alt+F8依次切换方案。",L"Nine regions scroll continuously with their own saved settings. Ctrl+Alt+F8 cycles styles.");
        case TabEdge:return choose(L"在选定屏边显示渐隐点阵，使用本方案独立设置。Ctrl+Alt+F8依次切换方案。",L"Fading dots on selected edges with independently saved settings. Ctrl+Alt+F8 cycles styles.");
        case TabStatic:return choose(L"固定4×4共16点，左右外列带柔边外圈；仅手动选择进入。",L"A fixed 4x4 reference with halos on the outer columns. Entered only by manual selection.");
        case Radius:return choose(L"调整圆点参考大小；屏边方案保留相邻点大小差异。可拖动、按方向键或滚轮微调。",L"Set reference dot size. Edge dots retain alternating sizes. Drag, use arrow keys or scroll to adjust.");
        case Opacity:case ReticleOpacity:return choose(L"透明度0%为完全可见，100%为完全透明。点阵与准星分别调整。",L"0% transparency is fully visible; 100% is hidden. Dots and reticle are independent.");
        case Spacing:return preferences.selected==2?choose(L"调整16个静态点的整体展开范围，点数保持不变。",L"Adjust the spread of the fixed 16 dots without changing their number."):choose(L"调整点阵间距；数值越大，点越稀疏。",L"Increase spacing for fewer, more widely separated dots.");
        case Hole:return preferences.selected==1?choose(L"调整点阵所在的边缘带宽，不影响中央准星。",L"Set the width of the edge strip without changing the reticle."):choose(L"调整中央圆形留空的直径比例，避开游戏准星区域。",L"Set the clear-center diameter as a percentage of the shorter screen dimension.");
        case Staggered:return choose(L"相邻行错开半个点距，减弱规则方块感，不改变动态滚动速度。",L"Offset alternate rows to soften the square-grid appearance without changing scrolling speed.");
        case BoundaryFade:return choose(L"轻度淡化分区交界处的圆点，不限制累计滚动位移。",L"Gently fade dots at regional boundaries without limiting accumulated travel.");
        case Gain:return choose(L"调整当前动态方案的点阵运动强度；各方案独立保存。",L"Adjust motion strength for this dynamic style. Saved independently.");
        case Invert:return choose(L"反转当前动态方案的运动方向，方便个人对照。",L"Reverse the direction of this dynamic style for comparison.");
        case Edges:return choose(L"选择左右、上下或全部屏边显示点阵。",L"Choose left/right, top/bottom or all edges.");
        case Appearance:return choose(L"固定颜色、同色相明暗适配、双层描边或组合。亮度适配不会抵消原有渐隐透明度。",L"Choose fixed color, adaptive brightness, two-tone outlines or both. Adaptation preserves alpha fading.");
        case DotColor:case ReticleColor:return choose(L"打开Windows颜色选择器；所选游戏浮层颜色不随软件明暗主题改变。",L"Open the Windows color picker. Overlay colors stay independent of the app theme.");
        case ReticleShape:return choose(L"点击图标选择准星；准星设置三种方案共用。",L"Click a shape icon to select a reticle. Settings are shared by all dot styles.");
        case ReticleSize:return choose(L"准星大小是中心到端点的长度，完整跨度约为该值的两倍。",L"Size is the center-to-tip length; the full span is approximately twice this value.");
        case DebugParameters:return choose(L"仅控制游戏浮层中的调试可视化，与日志开关独立。",L"Toggle diagnostic visuals in the overlay. This does not enable logging.");
        case RecordLog:return choose(L"每次启动默认关闭。勾选才记录；取消后立即停止并保留已有文件。单文件约10MB、总量100MB、最多20份，超限清理最旧记录。",L"Off at every launch. Check to record; uncheck to stop and keep existing files. About 10 MB per file, 100 MB total, up to 20 files; oldest logs are removed.");
        case Theme:return choose(L"点击切换：跟随系统 → 浅色 → 深色。\n当前：",L"Click to cycle: system → light → dark.\nCurrent: ")+(chinese?std::array{L"跟随系统",L"浅色",L"深色"}:std::array{L"Follow system",L"Light",L"Dark"})[preferences.theme];
        case Language:return choose(L"切换中文和英文界面，不改变视觉参数。",L"Switch between Chinese and English without changing visual parameters.");
        case Instructions:return choose(L"打开使用说明与第三方许可。",L"Open the user guide and third-party notices.");
        case CaptionMinimize:return choose(L"最小化设置窗口，游戏浮层继续按原设置工作。",L"Minimize the panel while the overlay keeps working.");
        case CaptionClose:return choose(L"退出软件并停止采集，自动保存视觉设置。",L"Exit, stop capture and save visual settings.");
        default:return L"";
        }
    }
    void updateTips(){
        if(!controlsReady)return;
        for(const auto& item:labels){int id=GetDlgCtrlID(item.first);auto value=hint(id);if(value.empty()&&!item.second.empty())value=std::wstring(tr(item.second));
            if(id==Attach&&health.severity==mc::Severity::Error)value+=(chinese?L"\n当前采集异常：":L"\nCapture problem: ")+std::wstring(captureDetails.begin(),captureDetails.end());
            bool conflict=((id>=TabRegional&&id<=TabStatic)&&!hotkeys[0])||(id==Visible&&!hotkeys[1])||(id==Stop&&!hotkeys[2]);
            if(conflict)value+=chinese?L"\n快捷键被占用，按钮仍可使用。请退出其他版本或占用快捷键的软件；程序会自动重试。":L"\nShortcut unavailable. The button still works. Exit other versions or the app using it; registration is retried automatically.";
            if(id==RecordLog&&!log.error().empty())value+=(chinese?L"\n记录失败：":L"\nRecording failed: ")+std::wstring(log.error().begin(),log.error().end());
            tips.set(item.first,std::move(value));
            mc::Severity severity=conflict?mc::Severity::Warning:mc::Severity::Normal;
            if(id==Attach&&health.severity==mc::Severity::Error)severity=mc::Severity::Error;
            if(id==RecordLog&&!log.error().empty())severity=mc::Severity::Error;
            mc::fluent::setSeverity(item.first,severity);
        }
        tips.set(captionMinimize,hint(CaptionMinimize));tips.set(captionClose,hint(CaptionClose));
        auto statusTip=chinese?std::wstring(L"显示当前连接目标、画面观测和输入来源；此处fps不是游戏实际帧率。"):std::wstring(L"Active target, observation quality and input source. This rate is not the game's FPS.");
        if(!healthMessage.empty())statusTip+=L"\n"+healthMessage;
        if(health.reason==mc::HealthReason::CaptureError)statusTip+=L"\n"+std::wstring(captureDetails.begin(),captureDetails.end());
        if(!settingsError.empty())statusTip+=(chinese?L"\n设置问题：":L"\nSettings problem: ")+std::wstring(settingsError.begin(),settingsError.end());
        if(!inputs.rawReady)statusTip+=chinese?L"\n键鼠注册失败；可靠画面观测仍可使用，请重新启动软件重试。":L"\nRaw input registration failed. Reliable image observations can still be used; restart the app to retry.";
        tips.set(status,statusTip);
        tips.set(shortcutsLabel,keysMissing()?(chinese?L"部分快捷键被占用。退出其他版本或冲突程序后会自动重试；窗口按钮仍可使用。":L"Some shortcuts are occupied. They are retried after the conflicting app exits. Panel buttons still work."):(chinese?L"Ctrl+Alt+F8切换方案；Ctrl+Alt+F9显示或隐藏；Ctrl+Alt+F10停止采集。":L"Ctrl+Alt+F8 cycles styles; F9 shows/hides; F10 stops capture (all with Ctrl+Alt)."));
    }
    void setLogging(bool enabled){
        if(enabled){
            log.start(logDirectory);
            if(log.enabled()){
                std::ostringstream configuration;mc::writePreferences(configuration,preferences);
                log<<"# MotionCompensator "<<mc::ReleaseVersion<<" recording started\n# app_elapsed_s="<<qpcSeconds()-started<<'\n'<<"# settings_snapshot\n";
                std::istringstream lines(configuration.str());std::string line;while(std::getline(lines,line))log<<"# "<<line<<'\n';log.flush();
            }
        }else log.stop();
        SendMessageW(logBox,BM_SETCHECK,log.enabled()?BST_CHECKED:BST_UNCHECKED,0);capture.setDiagnosticRecording(log.enabled());
        updateTips();InvalidateRect(logBox,nullptr,FALSE);
    }
    void refresh() {
        windows.clear(); SendMessageW(list,CB_RESETCONTENT,0,0);
        EnumWindows([](HWND w,LPARAM p)->BOOL {
            auto self=reinterpret_cast<App*>(p); auto entry=describe(w);
            if(IsWindowVisible(w) && !entry.title.empty() && entry.pid!=GetCurrentProcessId() && !(GetWindowLongPtrW(w,GWL_EXSTYLE)&WS_EX_TOOLWINDOW)) {
                std::wstring label=entry.exe+L" ["+std::to_wstring(entry.pid)+L"] "+entry.title;
                self->windows.push_back(entry); SendMessageW(self->list,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label.c_str()));
            } return TRUE;
        },reinterpret_cast<LPARAM>(this));
        if(!windows.empty()) SendMessageW(list,CB_SETCURSEL,0,0);
    }
    void attach(WindowEntry entry) {
        lastDebug=0;driver.reset();inputs.clear();overlay.hide(); capture.stop(); selected=entry; previousFrames=0;
        captureRetries=0;captureRetryAfter=qpcSeconds()+1;
        log<<"# attach_pid="<<entry.pid<<", hwnd="<<reinterpret_cast<uintptr_t>(entry.handle)<<'\n'; log.flush();
        capture.setFlowEnabled(dynamic);
        if(!overlayOnly) capture.start(entry.handle,smoke,debugDirectory);
    }
    void stop() { overlay.hide(); capture.stop(); selected={};driver.reset();inputs.setTarget(nullptr,false); }
    void create(HINSTANCE instance) {
        wchar_t path[MAX_PATH]{};GetModuleFileNameW(nullptr,path,MAX_PATH);settingsPath=std::filesystem::path(path).parent_path()/L"settings.ini";
        auto seed=mc::embeddedText(101);std::istringstream initial(seed);mc::readPreferences(initial,preferences);
        bool found=false;mc::loadUserPreferences(preferences,found,settingsError);
        if(!found && settingsError.empty()){
            if(std::filesystem::exists(settingsPath))mc::loadPreferences(settingsPath,preferences,settingsError);
            settingsDirty=true;saveAfter=qpcSeconds()+.5;
        }
        if(smoke)for(auto& profile:preferences.profiles)profile.appearance=1;
        mc::ui::setTheme(preferences.theme);applyTheme(true);
        chinese=preferences.chinese;visible=preferences.visible;automatic=preferences.automatic;dynamic=preferences.selected!=2;
        crossColor=preferences.reticleColor;
        smallFont=CreateFontW(-14,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        font=CreateFontW(-18,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        auto child=[&](const wchar_t* cls,const wchar_t* text,DWORD style,int x,int y,int w,int h,int id)->HWND {
            if(wcscmp(cls,L"BUTTON")==0 || wcscmp(cls,L"COMBOBOX")==0) style|=WS_TABSTOP;
            if(wcscmp(cls,L"COMBOBOX")==0)style|=CBS_OWNERDRAWFIXED|CBS_HASSTRINGS;
            if(wcscmp(cls,L"STATIC")==0)style|=SS_NOTIFY;
            HWND c=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,x,y,w,h,hwnd,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),instance,nullptr);
            if(wcscmp(cls,L"STATIC")==0)mc::fluent::install(c,mc::fluent::Kind::Label);
            else if(wcscmp(cls,L"COMBOBOX")==0)mc::fluent::install(c,mc::fluent::Kind::Combo);
            else if(wcscmp(cls,TRACKBAR_CLASSW)==0)mc::fluent::install(c,mc::fluent::Kind::Slider);
            else if(wcscmp(cls,L"BUTTON")==0)mc::fluent::install(c,(style&BS_TYPEMASK)==BS_AUTOCHECKBOX?mc::fluent::Kind::Check:mc::fluent::Kind::Button);
            controlRects.push_back({c,x,y,w,h});
            labels.push_back({c,text});SetWindowTextW(c,tr(labels.back().second));
            SendMessageW(c,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE); return c;
        };
        titleFont=CreateFontW(-30,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        auto title=child(L"STATIC",mc::DisplayName,0,28,20,530,38,0);SendMessageW(title,WM_SETFONT,reinterpret_cast<WPARAM>(titleFont),TRUE);
        child(L"STATIC",L"Visual comfort, in your control.",0,30,64,300,25,0);
        themeButton=child(L"BUTTON",L"",BS_OWNERDRAW,560,32,44,36,Theme);populateTheme();
        child(L"BUTTON",L"User Guide",BS_OWNERDRAW,620,32,156,36,Instructions);
        child(L"BUTTON",L"中文 / English",BS_OWNERDRAW,804,32,156,36,Language);
        child(L"STATIC",L"Game connection",0,44,118,400,26,0);
        list=child(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,44,154,516,320,List);
        child(L"BUTTON",L"Refresh windows",BS_OWNERDRAW,44,195,158,34,Refresh);
        child(L"BUTTON",L"Attach selected",BS_OWNERDRAW,214,195,174,34,Attach);
        child(L"BUTTON",L"Stop capture",BS_OWNERDRAW,400,195,160,34,Stop);
        autoBox=child(L"BUTTON",L"Auto attach foreground known game",BS_AUTOCHECKBOX,44,246,480,26,Auto);
        tabs[0]=child(L"BUTTON",L"Regional dots",BS_OWNERDRAW,44,326,166,36,TabRegional);
        tabs[1]=child(L"BUTTON",L"Screen-edge dots",BS_OWNERDRAW,216,326,166,36,TabEdge);
        tabs[2]=child(L"BUTTON",L"Static dots",BS_OWNERDRAW,388,326,172,36,TabStatic);
        visibleBox=child(L"BUTTON",L"Show overlay",BS_AUTOCHECKBOX,44,372,150,26,Visible);
        staggerBox=child(L"BUTTON",L"Staggered dots",BS_AUTOCHECKBOX,208,372,162,26,Staggered);
        boundaryBox=child(L"BUTTON",L"Boundary fade",BS_AUTOCHECKBOX,382,372,178,26,BoundaryFade);
        edgesBox=child(L"COMBOBOX",L"",CBS_DROPDOWNLIST,382,372,178,150,Edges);
        SendMessageW(autoBox,BM_SETCHECK,automatic?BST_CHECKED:BST_UNCHECKED,0);
        SendMessageW(visibleBox,BM_SETCHECK,visible?BST_CHECKED:BST_UNCHECKED,0);
        auto slider=[&](const wchar_t* label,int y,int id,int low,int high,int initial) {
            child(L"STATIC",label,0,44,y+4,180,26,0);
            HWND control=child(TRACKBAR_CLASSW,L"",TBS_NOTICKS|WS_TABSTOP,232,y,328,30,id);
            SendMessageW(control,TBM_SETRANGE,TRUE,MAKELPARAM(low,high));SendMessageW(control,TBM_SETPOS,TRUE,initial);return control;
        };
        radius=slider(L"Dot radius",406,Radius,1,24,3);
        opacity=slider(L"Transparency",446,Opacity,0,100,0);
        spacing=slider(L"Dot spacing",486,Spacing,60,240,122);
        hole=slider(L"Clear center (%)",526,Hole,5,25,10);
        child(L"BUTTON",L"Choose dot color",BS_OWNERDRAW,44,562,516,28,DotColor);
        child(L"STATIC",L"Background",0,44,646,180,26,0);
        appearanceBox=child(L"COMBOBOX",L"",CBS_DROPDOWNLIST,232,640,328,180,Appearance);
        child(L"STATIC",L"Center reticle",0,44,762,400,26,0);
        for(int i=0;i<mc::ReticleShapeCount;++i)
            reticleButtons[i]=child(L"BUTTON",L"",BS_OWNERDRAW,44+i*65,794,61,44,ReticleFirst+i);
        populateShapes(preferences.reticleShape);
        reticleOpacity=slider(L"Transparency",844,ReticleOpacity,0,100,0);
        reticleSize=slider(L"Reticle size",886,ReticleSize,mc::ReticleSizeMin,mc::ReticleSizeMax,10);
        child(L"BUTTON",L"Choose reticle color",BS_OWNERDRAW,44,936,516,32,ReticleColor);
        child(L"STATIC",L"Appearance preview",0,620,120,324,28,0);
        gainLabel=child(L"STATIC",L"Motion strength",0,44,604,180,26,0);
        gain=child(TRACKBAR_CLASSW,L"",TBS_NOTICKS|WS_TABSTOP,232,598,328,30,Gain);SendMessageW(gain,TBM_SETRANGE,TRUE,MAKELPARAM(25,200));
        invertBox=child(L"BUTTON",L"Reverse direction",BS_AUTOCHECKBOX,44,684,516,26,Invert);
        child(L"STATIC",L"Connection & activity",0,620,744,324,26,0);
        status=child(L"STATIC",L"Waiting for a game. Select a window or launch PAYDAY 2.",0,620,782,324,184,0);
        shortcutsLabel=child(L"STATIC",L"Ctrl+Alt+F8: next style    Ctrl+Alt+F9: show/hide\nCtrl+Alt+F10: stop",0,30,1000,500,46,0);
        SendMessageW(shortcutsLabel,WM_SETFONT,reinterpret_cast<WPARAM>(smallFont),TRUE);
        logBox=child(L"BUTTON",L"Log",BS_AUTOCHECKBOX,620,1005,90,28,RecordLog);
        debugButton=child(L"BUTTON",L"Show debug parameters",BS_OWNERDRAW,730,1002,230,32,DebugParameters);
        registerKeys();
        inputs.initialize(hwnd);
        if(!inputs.rawReady) error+=L" Raw Input unavailable.";
        SendMessageW(reticleOpacity,TBM_SETPOS,TRUE,int(std::round((1-preferences.reticleOpacity)*100)));
        SendMessageW(reticleSize,TBM_SETPOS,TRUE,int(preferences.reticleSize));
        captionMinimize=CreateWindowW(L"BUTTON",L"最小化 / Minimize",WS_CHILD|WS_VISIBLE|BS_OWNERDRAW,0,0,1,1,hwnd,reinterpret_cast<HMENU>(CaptionMinimize),instance,nullptr);
        captionClose=CreateWindowW(L"BUTTON",L"关闭 / Close",WS_CHILD|WS_VISIBLE|BS_OWNERDRAW,0,0,1,1,hwnd,reinterpret_cast<HMENU>(CaptionClose),instance,nullptr);
        mc::fluent::install(captionMinimize,mc::fluent::Kind::Button);mc::fluent::install(captionClose,mc::fluent::Kind::Button);
        loadProfileControls();controlsReady=true;
        layout();
        overlay.initialize(instance);overlay.invalidate();populateShapes(preferences.reticleShape);refresh();refreshValueLabels();
        wchar_t exe[MAX_PATH]{}; GetModuleFileNameW(nullptr,exe,MAX_PATH);
        logDirectory=std::filesystem::path(exe).parent_path()/L"logs";
        if(flowDebug)debugDirectory=logDirectory/(L"flow-debug-"+std::to_wstring(GetCurrentProcessId()));
        log.setHeader("elapsed_s,capture_frames,capture_fps,frame_age_at_receive_ms,latest_frame_age_ms,width,height,last_render_submit_ms,render_max_ms,render_count,raw_events,raw_timestamp_qpc_s,requested_dynamic,manual_static,capture_running,cpu_one_core_pct,working_set_mb\n");
        tips.initialize(hwnd);updateTips();
        if(diagnosticRecordLog)setLogging(true);
        FILETIME c{},e{},k{},u{}; GetProcessTimes(GetCurrentProcess(),&c,&e,&k,&u);
        previousCpu=(double((uint64_t(k.dwHighDateTime)<<32)|k.dwLowDateTime)+double((uint64_t(u.dwHighDateTime)<<32)|u.dwLowDateTime))/1e7;
        InvalidateRect(hwnd,nullptr,FALSE);
        SetTimer(hwnd,1,16,nullptr);
    }
    void tick() {
        double tickTime=qpcSeconds();float dt=float(tickTime-lastTick);lastTick=tickTime;
        if(settingsDirty && tickTime>=saveAfter) flushSettings();
        if(keysMissing()&&tickTime>=nextHotkeyRetry){registerKeys();updateTips();InvalidateRect(shortcutsLabel,nullptr,FALSE);}
        if(SendMessageW(logBox,BM_GETCHECK,0,0)==BST_CHECKED&&!log.enabled()){SendMessageW(logBox,BM_SETCHECK,BST_UNCHECKED,0);updateTips();}
        capture.setDiagnosticRecording(log.enabled());
        auto sample=capture.snapshot();
        HWND foreground=GetForegroundWindow();
        if(automatic && !smoke && foreground && foreground!=hwnd) {
            auto entry=describe(foreground);
            // Do not switch the capture item to an owned crash/error dialog.
            if(mc::knownGame(entry.exe) && !GetWindow(foreground,GW_OWNER)
                && (selected.handle!=entry.handle || selected.pid!=entry.pid)) attach(entry);
        }
        DWORD pid{}; if(selected.handle) GetWindowThreadProcessId(selected.handle,&pid);
        if(selected.handle && (!IsWindow(selected.handle)||pid!=selected.pid)) stop();
        // Some games expose an HWND before WGC can create its capture item.
        // Retry completed failures only, never interrupt an initializing worker.
        if(selected.handle && foreground==selected.handle && !overlayOnly && !sample.running
            && sample.status!="Starting WGC" && captureRetries<3 && tickTime>=captureRetryAfter) {
            ++captureRetries;captureRetryAfter=tickTime+(1<<captureRetries);
            capture.start(selected.handle,smoke,debugDirectory);
            log<<"# capture_retry="<<captureRetries<<'\n';log.flush();
        }
        capture.setFlowEnabled(dynamic);capture.setBackgroundEnabled(preferences.profiles[preferences.selected].appearance==1||preferences.profiles[preferences.selected].appearance==3);
        auto fresh=capture.snapshot();overlay.setBackground(fresh.running?fresh.light:mc::BackgroundLight{});
        bool inputForeground=selected.handle && foreground==selected.handle && !IsIconic(selected.handle);
        inputs.setTarget(selected.handle,inputForeground);
        auto input=inputs.poll(tickTime,dt);
        rawEvents=inputs.mouseEvents+inputs.keyEvents;rawTimestamp=inputs.lastEvent;
        double fusionStart=qpcSeconds();
        auto velocity=driver.update(fresh.motion,input,tickTime,dt,dynamic,inputForeground);
        fusionMs=(qpcSeconds()-fusionStart)*1000;
        const auto& profile=preferences.profiles[preferences.selected];
        float motionGain=profile.gain*(profile.inverted?-1.f:1.f);
        for(auto& v:velocity) {v.x*=motionGain;v.y*=motionGain;}
        bool dynamicActive=dynamic && inputForeground;
        if(!captureOnly && selected.handle && visible && !IsIconic(selected.handle) && (foreground==selected.handle || smoke)) {
            RECT rect{}; GetClientRect(selected.handle,&rect); POINT origin{}; ClientToScreen(selected.handle,&origin); OffsetRect(&rect,origin.x,origin.y);
            auto settings=visualSettings();
            if(showDebug && tickTime-lastDebug>=.1) {
                mc::DebugSnapshot d;d.input=driver.prediction();d.fused=driver.fused();d.compensation=velocity;d.flow=fresh.motion;d.selectedFlow=driver.observedFlow();d.selectedConfidence=driver.observedConfidence();d.hasSelectedFlow=true;d.backgroundBlend=driver.backgroundBlend();
                for(auto& c:d.compensation){c.x=std::clamp(c.x,-1200.f,1200.f);c.y=std::clamp(c.y,-1200.f,1200.f);}
                d.now=tickTime;d.captureAgeMs=fresh.frames?fresh.ageMs:-1;d.analysisMs=fresh.analyses?fresh.analysisMs:-1;
                d.fusionMs=fusionMs;d.submitMs=draws?drawMs:-1;d.tickMs=dt*1000;d.staggered=settings.staggered;d.style=settings.style;
                if(lastDebug>0 && tickTime>lastDebug && fresh.frames>=debugFrames && fresh.analyses>=debugAnalyses && draws>=debugDraws) {
                    double interval=tickTime-lastDebug;d.captureFps=(fresh.frames-debugFrames)/interval;d.analysisFps=(fresh.analyses-debugAnalyses)/interval;d.renderFps=(draws-debugDraws)/interval;
                }
                debugFrames=fresh.frames;debugAnalyses=fresh.analyses;debugDraws=draws;
                overlay.setDebugSnapshot(d,chinese);lastDebug=tickTime;
            }
            double before=qpcSeconds();
            if(overlay.draw(rect,settings,velocity,dt,dynamic)) { drawMs=(qpcSeconds()-before)*1000.; drawMax=std::max(drawMax,drawMs); ++draws; }
        } else {overlay.hide();lastDebug=0;}
        bool shown=IsWindowVisible(overlay.window())!=FALSE;
        if(shown!=wasOverlayVisible) { log<<"# overlay_visible="<<shown<<", elapsed_s="<<qpcSeconds()-started<<'\n'; log.flush(); wasOverlayVisible=shown; }
        double now=qpcSeconds();
        if(now-lastLog>=1.) {
            auto s=capture.snapshot();captureDetails=s.status;health=mc::captureHealth(selected.handle!=nullptr,s.running,inputForeground,dynamic,s.status,s.motion,now);
            switch(health.reason){
                case mc::HealthReason::NoTarget:healthMessage=chinese?L"等待连接游戏":L"Waiting for a game";break;
                case mc::HealthReason::Starting:healthMessage=chinese?L"正在启动采集":L"Starting capture";break;
                case mc::HealthReason::CaptureError:healthMessage=chinese?L"采集异常，请悬停查看详情":L"Capture problem; hover for details";break;
                case mc::HealthReason::Paused:healthMessage=chinese?L"切回目标窗口后恢复浮层":L"Return to the target to resume the overlay";break;
                case mc::HealthReason::Static:healthMessage=chinese?L"静态方案已连接":L"Static reference connected";break;
                case mc::HealthReason::Waiting:healthMessage=chinese?L"等待有效画面观测":L"Waiting for visual observations";break;
                case mc::HealthReason::Observation:healthMessage=chinese?L"画面观测不足或已过期，动态方案保持":L"Visual observations low or expired; style unchanged";break;
                default:healthMessage=chinese?L"画面观测正常":L"Visual observations available";break;
            }
            if(!settingsError.empty()&&health.severity!=mc::Severity::Error){health.severity=mc::Severity::Warning;healthMessage=chinese?L"设置保存或读取异常，请悬停查看详情":L"Settings problem; hover for details";}
            if(!inputs.rawReady&&health.severity!=mc::Severity::Error){health.severity=mc::Severity::Warning;healthMessage=chinese?L"键鼠观察不可用，请悬停查看详情":L"Input observation unavailable; hover for details";}
            updateTips();InvalidateRect(status,nullptr,FALSE);
            double fps=double(s.frames>=previousFrames?s.frames-previousFrames:0)/(now-lastLog);
            double age=s.timestamp>0?(now-s.timestamp)*1000:-1;
            FILETIME c{},e{},k{},u{}; GetProcessTimes(GetCurrentProcess(),&c,&e,&k,&u);
            double cpu=(double((uint64_t(k.dwHighDateTime)<<32)|k.dwLowDateTime)+double((uint64_t(u.dwHighDateTime)<<32)|u.dwLowDateTime))/1e7;
            PROCESS_MEMORY_COUNTERS memory{}; GetProcessMemoryInfo(GetCurrentProcess(),&memory,sizeof(memory));
            log<<std::fixed<<std::setprecision(3)<<now-started<<','<<s.frames<<','<<fps<<','<<s.ageMs<<','<<age<<','<<s.width<<','<<s.height<<','<<drawMs<<','<<drawMax<<','<<draws<<','<<rawEvents<<','<<rawTimestamp<<','<<dynamic<<','<<!dynamic<<','<<s.running<<','<<100*(cpu-previousCpu)/(now-lastLog)<<','<<memory.WorkingSetSize/(1024.*1024.)<<'\n';
            if(s.status!=previousCaptureStatus) { log<<"# capture_status="<<s.status<<'\n'; previousCaptureStatus=s.status; }
            log.flush(); previousCpu=cpu;
            std::wstring captureStatus(s.status.begin(),s.status.end());
            bool effective=dynamicActive;
            const wchar_t* reason=chinese?L"输入待命":L"input ready";
            switch(driver.source()) {
                case mc::DriveSource::Flow: reason=chinese?L"光流驱动":L"flow driven";break;
                case mc::DriveSource::Fused: reason=chinese?L"光流 + 输入":L"flow + input";break;
                case mc::DriveSource::Input: reason=chinese?L"输入预测":L"input prediction";break;
                case mc::DriveSource::Coast: reason=chinese?L"运动延续":L"motion continuation";break;
                case mc::DriveSource::Suspended: reason=chinese?L"游戏不在前台":L"game not foreground";break;
                default: break;
            }
            log<<"# vector_stages_qpc_s="<<tickTime<<", source_flow_qpc_s="<<fresh.motion.timestamp<<", fusion_ms="<<fusionMs<<", regions=";
            for(int r=0;r<9;++r){auto i=driver.prediction()[r],f=driver.fused()[r],c=velocity[r];
                log<<r<<":input:"<<i.x<<':'<<i.y<<":fused:"<<f.x<<':'<<f.y<<":dots:"<<std::clamp(c.x,-1200.f,1200.f)<<':'<<std::clamp(c.y,-1200.f,1200.f)<<' ';}
            log<<'\n';
            float offsetMax=0;for(auto v:velocity) offsetMax=std::max(offsetMax,std::hypot(v.x,v.y));
            log<<"# analysis_grid=9x9, analysis_width="<<s.motion.analysisWidth<<", analysis_height="<<s.motion.analysisHeight<<", observed_cells="<<s.motion.observedCells<<", observations="<<s.motion.observations.size()<<", cells=";
            for(int cell=0;cell<81;++cell){const auto& c=s.motion.cells[cell];log<<cell<<':'<<c.samples<<':'<<c.tracked<<':'<<c.inliers<<':'<<c.confidence<<':'<<c.velocity.x<<':'<<c.velocity.y<<':'<<c.smoothed.x<<':'<<c.smoothed.y<<':'<<c.inferred<<' ';}log<<'\n';
            log<<"# brightness_qpc_s="<<s.light.timestamp<<", brightness_updates="<<s.brightnessUpdates<<", brightness_cpu_ms="<<s.brightnessMs<<", brightness_age_ms="<<(s.light.timestamp>0?(now-s.light.timestamp)*1000:-1)<<", appearance="<<visualSettings().appearance<<'\n';
            log<<"# pipeline_completed_qpc_s="<<s.motion.completedTimestamp<<", completed_age_ms="<<(s.motion.completedTimestamp>0?(now-s.motion.completedTimestamp)*1000:-1)<<", flow_fresh="<<mc::motionFresh(s.motion,now)<<", analysis_start_age_ms="<<s.analysisStartAgeMs<<", capture_dropped="<<s.droppedCapture<<", analysis_replaced="<<s.droppedAnalysis<<'\n';
            log<<"# background_blend="<<driver.backgroundBlend()<<", global_confidence="<<s.motion.globalConfidence<<", global_support_cells="<<s.motion.globalSupportCells<<", moving_background_confidence="<<s.motion.movingBackgroundConfidence<<", moving_background_cells="<<s.motion.movingBackgroundSupportCells<<", chosen_flow=";
            for(int r=0;r<9;++r){auto v=driver.observedFlow()[r];log<<r<<':'<<v.x<<':'<<v.y<<':'<<driver.observedConfidence()[r]<<' ';}log<<'\n';
            log<<"# flow_confidence="<<s.motion.confidence<<", analysis_ms="<<s.analysisMs<<", analyses="<<s.analyses<<", history_resets="<<s.flowResets<<", features="<<s.motion.features<<", tracked="<<s.motion.tracked<<", inliers="<<s.motion.inliers<<", issue="<<int(s.motion.issue)<<", mean_delta="<<s.motion.meanDelta<<", dynamic_active="<<effective<<'\n';
            log<<"# input_mouse_events="<<inputs.mouseEvents<<", input_key_events="<<inputs.keyEvents<<", pad_changes="<<inputs.padChanges
               <<", mouse_rate_x="<<input.mouseRate.x<<", mouse_rate_y="<<input.mouseRate.y<<", move_x="<<input.move.x<<", move_y="<<input.move.y
               <<", look_x="<<input.look.x<<", look_y="<<input.look.y<<", pad_connected="<<input.pad<<", drive_source="<<int(driver.source())<<", raw_ready="<<inputs.rawReady
               <<", mouse_buttons="<<input.mouseButtons<<", wheel="<<input.wheel<<", pad_buttons="<<input.padButtons<<", trigger_l="<<input.triggers.x<<", trigger_r="<<input.triggers.y<<", xinput_available="<<inputs.padApiAvailable()<<'\n';
            log<<"# motion_qpc_s="<<s.motion.timestamp<<", motion_age_ms="<<(s.motion.timestamp>0?(now-s.motion.timestamp)*1000:-1)<<", regions=";
            for(size_t r=0;r<9;++r) log<<r<<':'<<s.motion.velocity[r].x<<':'<<s.motion.velocity[r].y<<':'<<s.motion.regionConfidence[r]<<' ';
            log<<", field_speed_max_px_s="<<offsetMax<<", dot_count="<<overlay.dotCount()<<", phase_max_px="<<overlay.phaseMax()<<", style="<<preferences.selected<<", enhancement="<<visualSettings().enhancement<<", boundary_fade="<<visualSettings().boundaryFade<<'\n';log.flush();
            if(chinese) {
                if(s.status=="Not started") captureStatus=L"等待选择窗口";
                else if(s.status=="Starting WGC") captureStatus=L"正在启动画面采集";
                else if(s.status=="Capture stopped") captureStatus=L"采集已停止";
                else if(s.status=="WGC active / waiting for analysis") captureStatus=L"画面采集中，等待分析";
                else if(s.status=="WGC / asynchronous latest-frame local flow") captureStatus=L"异步最新帧 / 9×9 局部光流";
                else if(s.status=="WGC / OpenCV pyramidal LK + robust spatial motion") captureStatus=L"灰度 960×540 / 9×9 局部光流";
                else if(s.status=="WGC / experimental robust pyramid flow") captureStatus=L"画面采集 / 实验多尺度运动估计";
                else captureStatus=L"诊断信息："+captureStatus;
            }
            if(overlayOnly) captureStatus=L"DIAGNOSTIC: overlay only / WGC disabled";
            if(captureOnly) captureStatus=L"DIAGNOSTIC: capture only / dots disabled | "+captureStatus;
            std::wostringstream text;
            text<<(chinese?L"目标：":L"Target: ")<<(selected.handle?selected.exe:L"—")<<L"\n"
                <<styleName(preferences.selected)
                <<(dynamic&&selected.handle?L" · ":L"")<<(dynamic&&selected.handle?reason:L"")
                <<L"\n"<<std::fixed<<std::setprecision(1)<<fps<<L" fps | "<<(chinese?L"光流 ":L"Flow ")<<s.motion.confidence
                <<L"\n";
            if(selected.handle && !s.running) text<<captureStatus;
            else text<<(chinese?L"分析 ":L"Analysis ")<<s.analysisMs<<L" ms | "<<(chinese?L"速度 ":L"Speed ")<<offsetMax<<L" px/s";
            text<<L"\n"<<(chinese?L"输入：":L"Input: ")
                <<(inputs.rawReady?(chinese?L"键鼠已接入":L"KB/mouse ready"):(chinese?L"键鼠注册失败":L"raw registration failed"))
                <<(input.pad?(chinese?L" | 手柄已连接":L" | pad connected"):(inputs.padApiAvailable()?(chinese?L" | 无 XInput 手柄":L" | no XInput pad"):(chinese?L" | XInput 不可用":L" | XInput unavailable")));
            if(!settingsError.empty())text<<L"\n"<<(chinese?L"设置保存/读取失败":L"Settings could not be loaded/saved");
            if(health.severity!=mc::Severity::Normal||health.reason==mc::HealthReason::NoTarget)text<<L"\n"<<healthMessage;
            setText(status,text.str()); previousFrames=s.frames; lastLog=now;
        }
        if(smoke && now-started>6) {
            auto s=capture.snapshot(); failed=s.frames==0 || draws==0 || !s.running || (dynamic?s.analyses==0:(s.analyses!=0||s.brightnessUpdates==0));
            log<<"# smoke_result="<<(failed?"FAIL":"PASS")<<", frames="<<s.frames<<", draws="<<draws<<", analyses="<<s.analyses<<", brightness_updates="<<s.brightnessUpdates<<", status="<<s.status<<'\n';
            DestroyWindow(hwnd);
        }
    }
    void command(int id) {
        if(id==RecordLog){setLogging(SendMessageW(logBox,BM_GETCHECK,0,0)==BST_CHECKED);return;}
        if(id==Theme){preferences.theme=(preferences.theme+1)%3;populateTheme();applyTheme();updateTips();markSettings();return;}
        if(id==CaptionMinimize){ShowWindow(hwnd,SW_MINIMIZE);return;}
        if(id==CaptionClose){SendMessageW(hwnd,WM_CLOSE,0,0);return;}
        if(id==Instructions) {mc::UsageWindow::show(hwnd,chinese);return;}
        if(id==DebugParameters) {
            showDebug=!showDebug;lastDebug=0;overlay.setDebugEnabled(showDebug);
            SetWindowTextW(debugButton,showDebug?(chinese?L"隐藏调试参数":L"Hide debug parameters"):tr(L"Show debug parameters"));
            InvalidateRect(debugButton,nullptr,FALSE);return;
        }
        if(id>=ReticleFirst && id<ReticleFirst+mc::ReticleShapeCount && controlsReady){
            preferences.reticleShape=id-ReticleFirst;populateShapes(preferences.reticleShape);
            overlay.invalidate();markSettings();updateTips();return;
        }
        if(id>=TabRegional && id<=TabStatic) {selectStyle(id-TabRegional);return;}
        if(id==DotColor) chooseColor(dotColor);
        if(id==ReticleColor) chooseColor(crossColor);
        if(id==Staggered)overlay.invalidate();
        if(id==Language) {chinese=!chinese;populateShapes(preferences.reticleShape);for(const auto& label:labels) if(!label.second.empty()) SetWindowTextW(label.first,tr(label.second));populateTheme();populateEdges(preferences.profiles[preferences.selected].edges);populateAppearance(preferences.profiles[preferences.selected].appearance);SetWindowTextW(debugButton,showDebug?(chinese?L"隐藏调试参数":L"Hide debug parameters"):tr(L"Show debug parameters"));refreshValueLabels();InvalidateRect(hwnd,nullptr,FALSE);lastLog=qpcSeconds()-1;}
        if(id==Refresh) refresh();
        if(id==Attach) { auto index=SendMessageW(list,CB_GETCURSEL,0,0); if(index>=0 && static_cast<size_t>(index)<windows.size()) {
            automatic=false; SendMessageW(autoBox,BM_SETCHECK,BST_UNCHECKED,0); attach(windows[index]); } }
        if(id==Auto) automatic=SendMessageW(autoBox,BM_GETCHECK,0,0)==BST_CHECKED;
        if(id==Visible) visible=SendMessageW(visibleBox,BM_GETCHECK,0,0)==BST_CHECKED;
        if(id==Stop) {automatic=false; SendMessageW(autoBox,BM_SETCHECK,BST_UNCHECKED,0); stop();}
        if(id!=Refresh) markSettings();
    }
};
static LRESULT CALLBACK proc(HWND w,UINT m,WPARAM a,LPARAM b) {
    auto app=reinterpret_cast<App*>(GetWindowLongPtrW(w,GWLP_USERDATA));
    if(m==WM_NCCREATE) { app=static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(b)->lpCreateParams); app->hwnd=w; SetWindowLongPtrW(w,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(app)); }
    if(!app) return DefWindowProcW(w,m,a,b);
    try {
        if(m==WM_NCCALCSIZE)return 0;
        if(m==WM_NCPAINT)return 0;
        if(m==WM_NCACTIVATE){InvalidateRect(w,nullptr,FALSE);return TRUE;}
        if(m==WM_DPICHANGED){auto r=reinterpret_cast<RECT*>(b);SetWindowPos(w,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);return 0;}
        if(m==WM_NCHITTEST){auto hit=DefWindowProcW(w,m,a,b);if(hit!=HTCLIENT)return hit;POINT point{int(short(LOWORD(b))),int(short(HIWORD(b)))};ScreenToClient(w,&point);RECT client{};GetClientRect(w,&client);auto edge=mc::panelResizeHit(point,client,GetDpiForWindow(w));if(edge!=HTCLIENT)return edge;if(point.y>=0&&point.y<mc::captionHeight(w)&&point.x<client.right-2*mc::captionButtonWidth(w))return HTCAPTION;return HTCLIENT;}
        if(m==WM_SETTINGCHANGE||m==WM_THEMECHANGED||m==WM_SYSCOLORCHANGE){if(app->preferences.theme==0)app->applyTheme();return 0;}
        if(m==WM_SYSCOMMAND && (a&0xfff0)==SC_MAXIMIZE)return 0;
        if(m==WM_NCLBUTTONDBLCLK && a==HTCAPTION)return 0;
        if(m==WM_ENTERSIZEMOVE){app->liveSizing=true;return 0;}
        if(m==WM_EXITSIZEMOVE){app->liveSizing=false;app->layout();RedrawWindow(w,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN|RDW_UPDATENOW);return 0;}
        if(m==WM_SIZE){if(a==SIZE_MINIMIZED)return 0;app->layout();InvalidateRect(w,nullptr,FALSE);return 0;}
        if(m==WM_GETMINMAXINFO){auto limits=reinterpret_cast<MINMAXINFO*>(b);limits->ptMinTrackSize={600,640};return 0;}
        if(m==WM_PAINT) {app->paint();return 0;}
        if(m==WM_ERASEBKGND)return 1;
        if(m==WM_HSCROLL) {app->markSettings();app->refreshValueLabels();return 0;}
        if(m==WM_CTLCOLORLISTBOX){auto dc=reinterpret_cast<HDC>(a);SetBkColor(dc,mc::fluent::surface);SetTextColor(dc,mc::fluent::ink);return reinterpret_cast<LRESULT>(mc::ui::surfaceBrush());}
        if(m==WM_CTLCOLORSTATIC || m==WM_CTLCOLORBTN) {
            HDC dc=reinterpret_cast<HDC>(a);SetTextColor(dc,mc::fluent::ink);
            if(reinterpret_cast<HWND>(b)==app->status)SetTextColor(dc,mc::fluent::severityColor(app->health.severity));
            if(reinterpret_cast<HWND>(b)==app->shortcutsLabel&&app->keysMissing())SetTextColor(dc,mc::ui::palette.warning);
            SetBkMode(dc,TRANSPARENT);
            RECT r{};GetWindowRect(reinterpret_cast<HWND>(b),&r);MapWindowPoints(nullptr,w,reinterpret_cast<POINT*>(&r),2);
            return reinterpret_cast<LRESULT>(r.top>=app->uiOffsetY+int(100*app->uiScale) && r.top<app->uiOffsetY+int(986*app->uiScale) ? app->card:app->background);
        }
        if(m==WM_MEASUREITEM){auto item=reinterpret_cast<MEASUREITEMSTRUCT*>(b);if(item->CtlType==ODT_COMBOBOX){item->itemHeight=36;return TRUE;}}
        if(m==WM_DRAWITEM) {
            auto d=reinterpret_cast<DRAWITEMSTRUCT*>(b);
            if(d->CtlID==CaptionMinimize||d->CtlID==CaptionClose){UiPaintBuffer buffer(d->hDC,d->rcItem);auto item=*d;item.hDC=buffer.dc();mc::paintCaptionButton(item,d->CtlID==CaptionClose);buffer.present();return TRUE;}
            if(d->CtlID>=ReticleFirst&&d->CtlID<ReticleFirst+mc::ReticleShapeCount){int shape=int(d->CtlID)-ReticleFirst;mc::fluent::reticleButton(*d,shape==app->preferences.reticleShape,shape,app->chinese);return TRUE;}
            if(d->CtlType==ODT_COMBOBOX){mc::fluent::comboItem(*d);return TRUE;}
            bool selected=(d->CtlID==DebugParameters && app->showDebug)||d->CtlID==Attach||(d->CtlID>=TabRegional&&d->CtlID<=TabStatic&&int(d->CtlID-TabRegional)==app->preferences.selected);
            mc::fluent::button(*d,selected,d->CtlID==Theme?app->preferences.theme:-1);return TRUE;
        }
        if(m==WM_TIMER) { app->tick(); return 0; }
        if(m==WM_COMMAND) {
            int id=LOWORD(a),notification=HIWORD(a);
            bool combo=id==List||id==Edges||id==Appearance;
            if(id==0||id==List || (combo?notification!=CBN_SELCHANGE:notification!=BN_CLICKED))return 0;
            app->command(id);return 0;
        }
        if(m==WM_HOTKEY) {
            if(a==1) app->selectStyle((app->preferences.selected+1)%3);
            if(a==2) {app->visible=!app->visible; SendMessageW(app->visibleBox,BM_SETCHECK,app->visible?BST_CHECKED:BST_UNCHECKED,0);app->markSettings();}
            if(a==3) app->command(Stop);
            return 0;
        }
        if(m==WM_INPUT) {
            app->inputs.onRaw(reinterpret_cast<HRAWINPUT>(b));
            return DefWindowProcW(w,m,a,b); // Required foreground Raw Input cleanup.
        }
        if(m==WM_INPUT_DEVICE_CHANGE) {app->inputs.clear();return 0;}
        if(m==WM_DESTROY) {
            app->flushSettings();KillTimer(w,1); app->stop(); for(int id=1;id<=3;++id) UnregisterHotKey(w,id); PostQuitMessage(app->failed?2:0); return 0;
        }
    } catch(const std::exception& e) {
        app->failed=true; app->overlay.hide(); app->error=std::wstring(e.what(),e.what()+strlen(e.what()));
        app->capture.stop(); app->log<<"# fatal_render_error="<<e.what()<<'\n'; app->log.flush();
        SetWindowTextW(app->status,app->error.c_str()); KillTimer(w,1);
        if(app->smoke) DestroyWindow(w);
    }
    return DefWindowProcW(w,m,a,b);
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR command,int show) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    HRESULT apartment=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    int result=1;
    try {
        mc::Preferences startup;bool found=false;std::string startupError;mc::loadUserPreferences(startup,found,startupError);mc::ui::setTheme(startup.theme);
        App app;app.flowDebug=wcsstr(command,L"--flow-debug")!=nullptr; app.smoke=wcsstr(command,L"--smoke-test")!=nullptr;app.diagnosticRecordLog=wcsstr(command,L"--diagnostic-record-log")!=nullptr;
        app.captureOnly=wcsstr(command,L"--capture-only")!=nullptr;
        app.overlayOnly=wcsstr(command,L"--overlay-only")!=nullptr;
        if(app.captureOnly && app.overlayOnly) throw std::runtime_error("Choose one diagnostic isolation mode");
        INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_BAR_CLASSES}; InitCommonControlsEx(&controls);
        WNDCLASSW wc{}; wc.hInstance=instance; wc.lpfnWndProc=proc; wc.lpszClassName=L"MotionCompensatorControl";
        wc.hCursor=LoadCursorW(nullptr,IDC_ARROW); wc.hbrBackground=nullptr; RegisterClassW(&wc);
        POINT cursor{};GetCursorPos(&cursor);RECT desktop=mc::monitorWorkArea(MonitorFromPoint(cursor,MONITOR_DEFAULTTOPRIMARY));
        RECT initial=mc::centeredPanelRect(desktop,GetDpiForSystem());
        HWND window=CreateWindowExW(0,wc.lpszClassName,L"MotionCompensator",mc::PanelWindowStyle,
            initial.left,initial.top,initial.right-initial.left,initial.bottom-initial.top,nullptr,nullptr,instance,&app);
        if(!window) throw std::runtime_error("Control window creation failed");
        using DwmSet=HRESULT(WINAPI*)(HWND,DWORD,LPCVOID,DWORD);
        if(auto dwm=LoadLibraryExW(L"dwmapi.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32)){auto set=loadFunction<DwmSet>(dwm,"DwmSetWindowAttribute");if(set){DWORD rounded=2;set(window,33,&rounded,sizeof(rounded));}FreeLibrary(dwm);}
        DeleteMenu(GetSystemMenu(window,FALSE),SC_MAXIMIZE,MF_BYCOMMAND);
        // The hidden window now has its actual monitor DPI; correct the initial fit before showing.
        RECT centered=mc::centeredPanelRect(desktop,GetDpiForWindow(window));
        SetWindowPos(window,nullptr,centered.left,centered.top,centered.right-centered.left,centered.bottom-centered.top,SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);
        app.create(instance);
        if(wcsstr(command,L"--diagnostic-static"))app.selectStyle(2);
        if(wcsstr(command,L"--diagnostic-regional"))app.selectStyle(0);
        if(wcsstr(command,L"--diagnostic-edge"))app.selectStyle(1);
        if(wcsstr(command,L"--diagnostic-visible-overlay")) {
            if(!SetWindowDisplayAffinity(app.overlay.window(),WDA_NONE)) throw std::runtime_error("Diagnostic capture visibility failed");
            app.log<<"# diagnostic_overlay_capture_visible=1\n"; app.log.flush();
        }
        app.log<<"# isolation_mode="<<(app.captureOnly?"capture-only":app.overlayOnly?"overlay-only":"combined")<<'\n';app.log.flush();
        ShowWindow(window,show); RedrawWindow(window,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN|RDW_FRAME|RDW_UPDATENOW);
        if(app.smoke || wcsstr(command,L"--diagnostic-self-target")) app.attach(describe(window));
        MSG msg{}; while(GetMessageW(&msg,nullptr,0,0)>0) {TranslateMessage(&msg); DispatchMessageW(&msg);} result=static_cast<int>(msg.wParam);
    } catch(const std::exception& e) { MessageBoxA(nullptr,e.what(),"MotionCompensator startup failed",MB_ICONERROR); }
    if(SUCCEEDED(apartment)) CoUninitialize();
    return result;
}
