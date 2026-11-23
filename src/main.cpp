#include "capture.hpp"
#include "overlay.hpp"
#include "input.hpp"
#include "preferences.hpp"
#include "game_registry.hpp"
#include "game_session.hpp"
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
#include "workbench_layout.hpp"
#include "triangle_mesh.hpp"
#include "screen_motion.hpp"
#include <commctrl.h>
#include <dwmapi.h>
#include <filesystem>
#include <fstream>
#include <vector>
#include <cwctype>
#include <iomanip>
#include <memory>
#include <psapi.h>
#include <commdlg.h>
#include <windowsx.h>
struct WindowEntry { HWND handle{}; DWORD pid{}; std::wstring exe,title,path; };
static WindowEntry describe(HWND hwnd) {
    WindowEntry w; w.handle=hwnd; GetWindowThreadProcessId(hwnd,&w.pid);
    wchar_t title[512]{}; GetWindowTextW(hwnd,title,512); w.title=title;
    // Query image name only. Never request VM_READ, VM_WRITE or process injection rights.
    HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,w.pid);
    if(process) { wchar_t path[32768]{}; DWORD n=32768;
        if(QueryFullProcessImageNameW(process,0,path,&n)) {
            const auto image=std::filesystem::path(std::wstring(path,n)).lexically_normal();
            w.path=image.wstring(); w.exe=image.filename().wstring();
        }
        CloseHandle(process); }
    std::transform(w.exe.begin(),w.exe.end(),w.exe.begin(),[](wchar_t c){return std::towlower(c);}); return w;
}
static bool candidateWindow(HWND window) {
    if(!window||!IsWindowVisible(window)||GetAncestor(window,GA_ROOT)!=window||
       (GetWindowLongPtrW(window,GWL_EXSTYLE)&WS_EX_TOOLWINDOW))return false;
    DWORD pid{};
    GetWindowThreadProcessId(window,&pid);
    return pid!=0&&pid!=GetCurrentProcessId();
}
static mc::MotionFieldFrame captureFieldMapping(HWND window, RECT client, const CaptureStats& capture) {
    mc::MotionFieldFrame frame{};
    frame.targetWidth=client.right-client.left;
    frame.targetHeight=client.bottom-client.top;
    frame.sourceWidth=capture.width;
    frame.sourceHeight=capture.height;
    if(frame.sourceWidth==frame.targetWidth&&frame.sourceHeight==frame.targetHeight)return frame;
    frame.mappingKnown=false;
    if(!window||frame.targetWidth<=0||frame.targetHeight<=0||frame.sourceWidth<=0||frame.sourceHeight<=0)return frame;
    POINT origin{client.left,client.top};
    if(!ClientToScreen(window,&origin))return frame;
    const auto useBounds=[&](RECT bounds){
        const int x=origin.x-bounds.left,y=origin.y-bounds.top;
        if(bounds.right-bounds.left!=frame.sourceWidth||bounds.bottom-bounds.top!=frame.sourceHeight||
           x<0||y<0||x+frame.targetWidth>frame.sourceWidth||y+frame.targetHeight>frame.sourceHeight)return false;
        frame.viewX=x;frame.viewY=y;frame.viewWidth=frame.targetWidth;frame.viewHeight=frame.targetHeight;
        frame.mappingKnown=true;
        return true;
    };
    RECT bounds{};
    if(GetWindowRect(window,&bounds)&&useBounds(bounds))return frame;
    if(SUCCEEDED(DwmGetWindowAttribute(window,DWMWA_EXTENDED_FRAME_BOUNDS,&bounds,sizeof(bounds))))useBounds(bounds);
    return frame;
}
using mc::workbench::Page;
using mc::workbench::Section;
namespace ui = mc::workbench;
enum { List=101, Refresh, Attach, Auto, Mode, Visible, Stop, Radius, Opacity, Spacing, Language, DotColor, ReticleColor, ReticleOpacity, ReticleSize, ReticleShape, Hole, Staggered, BoundaryFade, TabRegional, TabEdge, TabStatic, TabTriangle, Gain, Invert, Edges, DebugParameters, Instructions, Appearance, CaptionMinimize, CaptionClose, Theme, RecordLog, PageDots, PageReticle, PauseMotion, NavOverview, NavVisuals, NavGames, NavDiagnostics, NavGeneral, GameChoice, RegisterGame, VisualProfile, ForgetGame, ResetGame, OverviewStyle, RetryCapture, MenuSelect, MenuEffectChoice, MenuNew, MenuSave, MenuDelete, ClearMenuOpen, ClearMenuReturn, RuleName, RuleOpen, RuleReturn, RuleCount, ResetMovement, ResetShortcuts, LiveMeshEdges, MeshDepth, MeshDepthStrength, ReticleFirst=200, MovementFirst=230, ShortcutFirst=250, ClearShortcutFirst=260, ClearMovementFirst=270 };
class App {
public:
    HWND hwnd{},list{},status{},autoBox{},visibleBox{},radius{},opacity{},spacing{};
    std::vector<WindowEntry> windows;
    WindowEntry selected;
    Capture capture;
    Overlay overlay;
    bool automatic=true,dynamic=true,visible=true,smoke=false,failed=false;
    bool captureOnly=false,overlayOnly=false,wasOverlayVisible=false;
    mc::CaptureRecovery recovery;
    std::wstring reconnectPath;
    HWND probedWindow{};
    DWORD probedPid{};
    double nextProbe=0;
    HWND gameChoice{},visualProfile{},gamePathLabel{},gameMessage{},editorScope{},overviewStyle{},stopButton{},retryButton{};
    struct MeshDiagnostics {
        mc::MeshStats stats{};
        size_t visible{};
        double timestamp{},updateMs{},topologyMs{},submitMs{};
    } meshDiagnostics;
    mc::GameRegistry registry;
    mc::MenuController menu;
    mc::MenuEffect lastMenuEffect=mc::MenuEffect::Normal;
    double resumeFlowAfter=0;
    HWND menuSelect{},menuEffectChoice{},menuName{},menuOpen{},menuReturn{},menuCount{};
    HWND menuMessage{},menuStateLabel{},movementHint{},shortcutHint{};
    std::array<HWND,mc::MovementBindings{}.size()> movementButtons{},clearMovementButtons{};
    std::array<HWND,mc::ShortcutBindings{}.size()> shortcutButtons{},shortcutSummary{},clearShortcutButtons{};
    std::vector<mc::MenuChord> pendingOpen,pendingReturn;
    HWND armedBinding{};
    unsigned candidateModifier{};
    mc::MenuChord suppressedShortcut{};
    double suppressShortcutUntil=0;
    void updateBindingLabels();
    void setMenuKeyLabels();
    void armBinding(HWND control);
    bool captureBindingMessage(const MSG& message);
    void applyRecordedBinding(int id,mc::MenuChord chord);
    void toggleOverlay();
    bool installShortcut(size_t index,mc::MenuChord chord);
    void changeShortcuts(const mc::ShortcutBindings& requested);
    void selectBindingProfile();
    void followGameProfile(int index,bool preserveDraft=true);
    void setMovementBinding(size_t action,mc::MenuChord chord);
    bool pendingProfileFollow=false;
    const mc::MovementBindings& runningMovement() const;
    bool usesShortcut(mc::MenuChord chord) const;
    std::wstring menuGamePath;
    int editingRule=-1;
    bool menuMessageError=false,shortcutMessageError=false;
    void setMenuMessage(const std::wstring& value,bool errorMessage=false);
    bool hasUnsavedMenuChanges() const;
    bool confirmRuleNavigation() const;
    void populateMenuEditor(int index);
    void menuCommand(int id);
    void observeInput(HRAWINPUT raw);
    void restoreMenu();
    std::span<const mc::MenuRule> activeRules() const;
    const wchar_t* menuEffectName(mc::MenuEffect result) const;
    mc::Preferences editorBaseline;
    int editorGame=-1,activeGame=-1,lastRunningStyle=-1;
    bool registryWritable=true;
    double started=qpcSeconds(),lastLog=started,lastFlowLogged=0,drawMs=0,drawMax=0;
    int previewTargetWidth=0,previewTargetHeight=0;
    uint64_t draws=0,previousFrames=0,rawEvents=0;
    double rawTimestamp=0;
    double previousCpu=0;
    std::string previousCaptureStatus;
    mc::SessionLog log;
    std::filesystem::path logDirectory;
    bool diagnosticRecordLog=false;
    bool chromaAssist=false;
    HWND logBox{},themeButton{},chromaStatus{},liveMeshEdgesBox{},meshDepthBox{},meshDepthSlider{},meshDepthLabel{};
    mc::ControlTips tips;
    mc::Health health;
    std::string captureDetails;
    std::array<bool,3> hotkeys{};
    double nextHotkeyRetry=0;
    std::wstring healthMessage,statusDetails;
    std::wstring error;
    bool chinese=true,flowDebug=false,showDebug=false;
    double lastDebug=0,fusionMs=0;
    uint64_t debugFrames=0,debugAnalyses=0,debugDraws=0;
    HWND debugButton{};
    mc::Preferences preferences;
    std::filesystem::path settingsPath;
    bool settingsDirty=false,controlsReady=false,sliderPreviewPending=false;double nextSliderPreview=0;
    std::vector<std::pair<HWND,HWND>> sliderLabels;
    double saveAfter=0;
    std::string settingsError;
    HWND appearanceBox{},captionMinimize{},captionClose{};
    float uiScale=1;int uiOffsetX=0,uiOffsetY=0;
    bool liveSizing=false;int layoutWidth=0,layoutHeight=0,layoutDpi=0,fontScalePixels=0;
    Page page=Page::Overview;
    bool motionPaused=false;
    HWND pauseBox{}, headerTitle{}, headerStatus{}, diagnosticsStatus{}, meshDiagnosticsStatus{}, targetWindowLabel{};
    int uiHeight=ui::preferredHeight;
    int settingsPage=0;HWND settingsPane{};std::array<int,2> paneScroll{};int scrollWheel=0;bool scrollDragging=false;int scrollDragY=0,scrollDragStart=0;
    struct ControlRect{HWND window;int x,y,w,h;Section section;};
    std::vector<ControlRect> controlRects;
    HWND boundaryBox{},gain{},gainLabel{},invertBox{},edgesBox{};
    std::array<HWND,mc::VisualStyleCount> tabs{};
    std::array<HWND,mc::ReticleShapeCount> reticleButtons{};
    HFONT smallFont{};
    std::filesystem::path debugDirectory;
    mc::DynamicDriver driver; InputObserver inputs; double lastTick=qpcSeconds();
    mc::ScreenMotionField screenField;
    double fieldMs=0;
    double meshUpdatePeakMs=0,meshTopologyPeakMs=0;
    double meshMotionAfter=0;
    bool meshMotionEligible=false;
    std::vector<std::pair<HWND,std::wstring>> labels;
    const wchar_t* tr(const std::wstring& en) {
        if(!chinese) return en.c_str();
        static const std::vector<std::pair<std::wstring,std::wstring>> words={
            {L"MotionCompensator 0.2 | Capture + experimental motion estimation",L"MotionCompensator 0.2 | 外部采集与实验运动估计"},
            {L"Refresh windows",L"刷新窗口"},{L"Attach selected",L"选择此窗口"},{L"Stop capture",L"停止采集"},
            {L"Automatically connect registered games",L"自动连接已登记游戏"},
            {L"Show dots when selected game is foreground",L"游戏在前台时显示点阵"},
            {L"Dot radius",L"点大小"},{L"Opacity",L"透明度"},{L"Dot spacing",L"点间距"},
            {L"Waiting for a game. Select a window or launch PAYDAY 2.",L"等待游戏。可手动选择窗口，或自动识别前台游戏。"},
            };
        static const std::vector<std::pair<std::wstring,std::wstring>> extra={
            {L"Theme",L"主题"},{L"Log",L"日志"},
            {L"Overview",L"总览"},{L"Visual effects",L"视觉效果"},{L"Game keys",L"游戏按键配置"},
            {L"Diagnostics",L"运行诊断"},{L"General",L"通用设置"},{L"Language",L"语言"},
            {L"Show mesh connections",L"显示网格连线"},{L"Depth effect",L"纵深效果"},{L"Depth strength",L"纵深强度"},
            {L"Global shortcuts",L"全局快捷键"},{L"Pause dynamic motion",L"暂停动态补偿"},
            {L"Choose a game window",L"选择游戏窗口"},
            {L"Game library and per-game profiles",L"游戏库与独立配置"},
            {L"Register and connect",L"登记并连接"},{L"Connect",L"连接"},
            {L"Remove",L"移除游戏"},{L"Visual profile",L"视觉配置"},
            {L"Restore global visuals",L"恢复全局视觉设置"},{L"Global defaults",L"全局默认"},
            {L"Choose registered game",L"选择已登记游戏"},
            {L"Game library",L"游戏库"},{L"Current game",L"当前游戏"},
            {L"Quick controls",L"快捷控制"},{L"Choose game",L"选择游戏"},
            {L"Retry capture",L"重试采集"},
            {L"Windows",L"可连接窗口"},{L"Connect once",L"临时连接"},
            {L"Menu / inventory",L"菜单与背包"},{L"Movement keys",L"移动与动作"},
            {L"Forward",L"前进"},{L"Backward",L"后退"},{L"Move left",L"向左"},{L"Move right",L"向右"},
            {L"Jump",L"跳跃"},{L"Sprint",L"奔跑"},{L"Crouch",L"蹲伏"},{L"Prone",L"趴下"},
            {L"Unbound",L"未绑定"},
            {L"Restore defaults",L"恢复默认按键"},{L"Clear",L"清空"},
            {L"Cycle style",L"切换方案"},{L"Toggle overlay",L"显示或隐藏浮层"},
            {L"Click a field, then press a key or combination.",L"点击按键栏后，直接按下单键或组合键。"},
            {L"Click to record",L"点击录入"},
            {L"Record keys to match your game.",L"录入和游戏内设置一致的按键。"},
            {L"Click a key field to add another option.",L"点击按键栏可追加一个候选键。"},
            {L"New rule",L"新建规则"},{L"Save rule",L"保存规则"},{L"Delete rule",L"删除规则"},
            {L"Menu name",L"菜单名称"},{L"Open keys",L"开启键"},{L"Return keys",L"返回键"},
            {L"Return presses",L"返回次数"},{L"Menu effect",L"菜单期间"},
            {L"Enable diagnostics to view capture and motion data.",L"开启调试显示后查看采集与运动数据。"},
            {L"Dots",L"点阵"},{L"Reticle",L"准星"},{L"Dot style",L"点阵方案"},{L"Appearance settings",L"外观"},{L"Motion settings",L"运动"},
            {L"Visual comfort, in your control.",L"让视野更舒适，让设置更直观。"},
            {L"Game connection",L"游戏连接"},{L"Center reticle",L"中央准星"},
            {L"Regional dots",L"分区点阵"},{L"Screen-edge dots",L"屏边点阵"},{L"Static dots",L"静态点阵"},
            {L"Regional",L"分区点阵"},{L"Screen edge",L"屏边点阵"},{L"Static",L"静态点阵"},{L"Mesh dots",L"网格点阵"},
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
        if(staggerBox&&mc::hasStaggeredOption(v.style))v.staggered=SendMessageW(staggerBox,BM_GETCHECK,0,0)==BST_CHECKED;
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
        if(meshDepthBox)v.meshDepth=SendMessageW(meshDepthBox,BM_GETCHECK,0,0)==BST_CHECKED;
        if(meshDepthSlider)v.meshDepthStrength=float(SendMessageW(meshDepthSlider,TBM_GETPOS,0,0))/100;
        v.reticleShape=preferences.reticleShape;return v;
    }
    static void setText(HWND control,const std::wstring& value){
        int n=GetWindowTextLengthW(control);std::wstring old(n+1,L'\0');GetWindowTextW(control,old.data(),n+1);old.resize(n);
        if(old!=value)SetWindowTextW(control,value.c_str());
    }
    void invalidatePreview();
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
            if(label.second==L"Motion strength") {if(!chinese)text=L"Motion strength";value=std::to_wstring(int(SendMessageW(gain,TBM_GETPOS,0,0)))+L"%";}
            if(label.second==L"Transparency") value=std::to_wstring(int(std::round((1-(transparencyLabel++==0?v.opacity:v.reticleOpacity))*100)))+L"%";
            if(label.first==meshDepthLabel)value=std::to_wstring(int(std::round(v.meshDepthStrength*100)))+L"%";
            if(!value.empty()) setText(label.first,text+L"  "+value);
        }
    }
    const wchar_t* styleName(int style) {
        if(style==mc::TriangleStyle)return chinese?L"网格点阵":L"Mesh dots";
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
        p.appearance=v.appearance;p.boundaryFade=v.boundaryFade;p.edges=v.edges;
        if(mc::hasStaggeredOption(v.style))p.staggered=v.staggered;
        p.gain=float(SendMessageW(gain,TBM_GETPOS,0,0))/100;
        p.inverted=SendMessageW(invertBox,BM_GETCHECK,0,0)==BST_CHECKED;
        preferences.meshDepth=v.meshDepth;preferences.meshDepthStrength=v.meshDepthStrength;
        preferences.reticleShape=v.reticleShape;preferences.reticleSize=v.reticleSize;preferences.reticleOpacity=v.reticleOpacity;preferences.reticleColor=v.reticle;
        preferences.chinese=chinese;preferences.visible=visible;preferences.automatic=automatic;
        mc::sanitize(preferences);
    }
    void markSettings() {collectSettings();settingsDirty=true;saveAfter=qpcSeconds()+.5;invalidatePreview();updateTips();}
    void sliderChanged(HWND control,int code){
        int id=GetDlgCtrlID(control),value=int(SendMessageW(control,TBM_GETPOS,0,0));
        auto& profile=preferences.profiles[preferences.selected];std::wstring text;
        switch(id){
        case Radius:profile.radius=float(value);text=std::wstring(tr(L"Dot radius"))+L"  "+std::to_wstring(value)+L" px";break;
        case Opacity:profile.opacity=1-value/100.f;text=std::wstring(tr(L"Transparency"))+L"  "+std::to_wstring(value)+L"%";break;
        case Spacing:profile.spacing=float(value);text=preferences.selected==2?(chinese?L"排布展开  ":L"Pattern spread  ")+std::to_wstring(int(std::round((value-60)/180.f*100)))+L"%":std::wstring(tr(L"Dot spacing"))+L"  "+std::to_wstring(value)+L" px";break;
        case Hole:if(preferences.selected==1){profile.edgeWidth=float(value);text=(chinese?L"边缘带宽  ":L"Edge width  ")+std::to_wstring(value)+L" px";}else{profile.hole=value/100.f;text=(chinese?L"中央留空  ":L"Clear center  ")+std::to_wstring(value)+L"%";}break;
        case Gain:profile.gain=value/100.f;text=std::wstring(tr(L"Motion strength"))+L"  "+std::to_wstring(value)+L"%";break;
        case MeshDepthStrength:preferences.meshDepthStrength=value/100.f;text=std::wstring(tr(L"Depth strength"))+L"  "+std::to_wstring(value)+L"%";break;
        case ReticleOpacity:preferences.reticleOpacity=1-value/100.f;text=std::wstring(tr(L"Transparency"))+L"  "+std::to_wstring(value)+L"%";break;
        case ReticleSize:preferences.reticleSize=float(value);text=std::wstring(tr(L"Reticle size"))+L"  "+std::to_wstring(value)+L" px";break;
        default:return;
        }
        for(auto item:sliderLabels)if(item.first==control){setText(item.second,text);break;}
        settingsDirty=true;saveAfter=qpcSeconds()+.5;sliderPreviewPending=true;
        if(code==TB_ENDTRACK||code==TB_THUMBPOSITION){sliderPreviewPending=false;invalidatePreview();updateTips();}
    }
    void flushSettings() {
        if(!settingsDirty || smoke)return;
        commitEditor();
        if(!registryWritable){settingsDirty=false;return;}
        if(mc::saveGameRegistry(registry,settingsError)) settingsDirty=false;
        else saveAfter=qpcSeconds()+5;
        updateLibrary();
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
        SendMessageW(staggerBox,BM_SETCHECK,mc::hasStaggeredOption(preferences.selected)&&p.staggered?BST_CHECKED:BST_UNCHECKED,0);
        SendMessageW(boundaryBox,BM_SETCHECK,p.boundaryFade?BST_CHECKED:BST_UNCHECKED,0);
        SendMessageW(gain,TBM_SETPOS,TRUE,int(std::round(p.gain*100)));
        SendMessageW(invertBox,BM_SETCHECK,p.inverted?BST_CHECKED:BST_UNCHECKED,0);
        SendMessageW(meshDepthBox,BM_SETCHECK,preferences.meshDepth?BST_CHECKED:BST_UNCHECKED,0);
        SendMessageW(meshDepthSlider,TBM_SETPOS,TRUE,int(std::round(preferences.meshDepthStrength*100)));
        populateEdges(p.edges);populateAppearance(p.appearance);
        dynamic=preferences.selected!=2;
        updatePageVisibility();
        refreshValueLabels();if(controlsReady)updateTips();for(auto tab:tabs) InvalidateRect(tab,nullptr,FALSE);
    }
    void updatePageVisibility();
    int settingsContentHeight() const;
    int settingY(const ControlRect& item) const;
    int settingsScrollLimit() const;
    void layoutSettings();
    void scrollSettings(int value);
    void revealSetting(HWND control);
    RECT settingsThumb() const;
    static LRESULT CALLBACK settingControlProc(HWND,UINT,WPARAM,LPARAM,UINT_PTR,DWORD_PTR);
    static LRESULT CALLBACK settingsPaneProc(HWND,UINT,WPARAM,LPARAM,UINT_PTR,DWORD_PTR);
    void selectStyle(int style) {
        collectSettings();preferences.selected=std::clamp(style,0,mc::VisualStyleCount-1);loadProfileControls();
        markSettings();lastLog=qpcSeconds()-1;
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
    void layout();
    void paint();

    ~App() { if(smallFont) DeleteObject(smallFont);if(font) DeleteObject(font);if(titleFont) DeleteObject(titleFont);DeleteObject(background);DeleteObject(card); }
    bool shortcutUnavailable(size_t index) const {
        return registry.shortcuts.at(index)!=0&&!hotkeys.at(index);
    }
    bool keysMissing() const {
        for(size_t i=0;i<hotkeys.size();++i)if(shortcutUnavailable(i))return true;
        return false;
    }
    void registerKeys(){
        for(size_t i=0;i<hotkeys.size();++i)
            if(shortcutUnavailable(i))hotkeys[i]=installShortcut(i,registry.shortcuts[i]);
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
        for(HWND combo:{list,gameChoice,visualProfile,edgesBox,appearanceBox,menuSelect,menuEffectChoice})if(combo){COMBOBOXINFO info{};info.cbSize=sizeof(info);if(GetComboBoxInfo(combo,&info)&&info.hwndList)RedrawWindow(info.hwndList,nullptr,nullptr,RDW_INVALIDATE|RDW_FRAME|RDW_UPDATENOW);}
        tips.applyTheme();mc::UsageWindow::applyTheme();
        if(visiblePanel)SendMessageW(hwnd,WM_SETREDRAW,TRUE,0);
        RedrawWindow(hwnd,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN|RDW_FRAME|RDW_UPDATENOW);
    }
    std::wstring hint(int id) const{
        auto choose=[&](const wchar_t* cn,const wchar_t* en){return std::wstring(chinese?cn:en);};
        if(id>=ReticleFirst&&id<ReticleFirst+mc::ReticleShapeCount){
            return std::wstring(mc::reticleName(id-ReticleFirst,chinese))+(chinese?L"：点击切换准星。颜色、透明度和大小由下方控件调整，各点阵方案共用。":L": select this reticle. Color, transparency and size are shared by all dot styles.");
        }
        switch(id){
        case List:return choose(L"列出可手动连接的窗口。列表中的候选窗口不一定是当前自动连接的游戏；实际目标见右侧状态。",L"Choose a window for manual attachment. The active auto-attached target is shown in the status panel.");
        case Refresh:return choose(L"刷新可选择的窗口列表，不改变当前连接。",L"Refresh the window list without changing the active connection.");
        case Attach:return choose(L"连接列表中的窗口，并关闭自动识别；切回该窗口后显示浮层。",L"Attach the selected window and turn off auto detection. Return to that window to show the overlay.");
        case Auto:return choose(L"按已登记的程序路径连接前台游戏。切出游戏时隐藏浮层，切回后恢复。",L"Connect foreground games by their registered executable path. The overlay hides when you switch away.");
        case Stop:return choose(L"停止采集并关闭自动连接。快捷键：Ctrl+Alt+F10。",L"Stop capture and auto attachment. Shortcut: Ctrl+Alt+F10.");
        case Visible:return choose(L"控制游戏前台浮层显示，不改变当前点阵方案。快捷键：Ctrl+Alt+F9。",L"Show or hide the foreground overlay without changing style. Shortcut: Ctrl+Alt+F9.");
        case TabRegional:return choose(L"九宫格分别持续滚动，使用本方案独立设置。Ctrl+Alt+F8依次切换方案。",L"Nine regions scroll continuously with their own saved settings. Ctrl+Alt+F8 cycles styles.");
        case TabEdge:return choose(L"在选定屏边显示渐隐点阵，使用本方案独立设置。Ctrl+Alt+F8依次切换方案。",L"Fading dots on selected edges with independently saved settings. Ctrl+Alt+F8 cycles styles.");
        case TabStatic:return choose(L"固定4×4共16点，左右外列带柔边外圈；仅手动选择进入。",L"A fixed 4x4 reference with halos on the outer columns. Entered only by manual selection.");
        case TabTriangle:return choose(L"用游戏画面的二维运动驱动自适应点阵。可随时切回其他方案。",L"Adaptive dots follow the game's 2D motion field. You can switch back to another style at any time.");
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
        case ReticleShape:return choose(L"点击图标选择准星；准星设置由各方案共用。",L"Click a shape icon to select a reticle. Settings are shared by all dot styles.");
        case ReticleSize:return choose(L"准星大小是中心到端点的长度，完整跨度约为该值的两倍。",L"Size is the center-to-tip length; the full span is approximately twice this value.");
        case DebugParameters:return choose(L"仅控制游戏浮层中的调试可视化，与日志开关独立。",L"Toggle diagnostic visuals in the overlay. This does not enable logging.");
        case RecordLog:return choose(L"每次启动默认关闭。勾选才记录；取消后立即停止并保留已有文件。单文件约10MB、总量100MB、最多20份，超限清理最旧记录。",L"Off at every launch. Check to record; uncheck to stop and keep existing files. About 10 MB per file, 100 MB total, up to 20 files; oldest logs are removed.");
        case RegisterGame:return choose(L"保存所选程序的路径并连接。启用自动连接后，切回游戏即可再次识别。",L"Save the executable path and connect. With auto connect enabled, return to the game to reconnect.");
        case VisualProfile:return choose(L"编辑选中的配置；未单独修改的项目沿用全局默认。",L"Edit the selected profile. Settings you haven't changed inherit global defaults.");
        case OverviewStyle:return choose(L"切换当前游戏的点阵方案。",L"Cycle the dot style for the current game.");
        case RetryCapture:return choose(L"采集失败时可立即重试，也可切回游戏等待自动恢复。",L"Retry a failed capture now, or return to the game to retry automatically.");
        case PageDots:return choose(L"调整点阵方案与各方案独立保存的外观设置。",L"Choose a dot style and adjust its independent appearance settings.");
        case PageReticle:return choose(L"调整所有点阵方案共用的中央准星。",L"Adjust the central reticle shared by all dot styles.");
        case Theme:return choose(L"点击切换：跟随系统 → 浅色 → 深色。\n当前：",L"Click to cycle: system → light → dark.\nCurrent: ")+(chinese?std::array{L"跟随系统",L"浅色",L"深色"}:std::array{L"Follow system",L"Light",L"Dark"})[preferences.theme];
        case LiveMeshEdges:return choose(L"在游戏里显示网格点阵的实际连线。每次启动默认关闭。",L"Show the live triangle connections in the game. Off at launch.");
        case MeshDepth:return choose(L"扩张时中央蓝且小、外围红且大，中间保持中性灰。收缩时中央与外围的效果对调。",L"Expansion makes inner dots smaller and bluer, and outer dots larger and redder. The middle stays neutral. Contraction reverses the effect.");
        case MeshDepthStrength:return choose(L"调整点径和偏色的变化幅度。",L"Adjust how much dot size and tint change.");
        case Language:return choose(L"切换中文和英文界面，不改变视觉参数。",L"Switch between Chinese and English without changing visual parameters.");
        case Instructions:return choose(L"打开使用说明与第三方许可。",L"Open the user guide and third-party notices.");
        case CaptionMinimize:return choose(L"最小化设置窗口，游戏浮层继续按原设置工作。",L"Minimize the panel while the overlay keeps working.");
        case CaptionClose:return choose(L"退出软件并停止采集，自动保存视觉设置。",L"Exit, stop capture and save visual settings.");
        default:return L"";
        }
    }
    void updateTips(){
        if(!controlsReady)return;
        for(const auto& item:labels){
            int id=GetDlgCtrlID(item.first);
            if(id==RuleOpen||id==RuleReturn||id==VisualProfile||item.first==gamePathLabel||item.first==editorScope||item.first==menuMessage)continue;
            const bool shortcutField=id>=ShortcutFirst&&id<ShortcutFirst+int(shortcutButtons.size());
            const bool movementField=id>=MovementFirst&&id<MovementFirst+int(movementButtons.size());
            auto value=shortcutField||movementField?std::wstring(tr(L"Click a field, then press a key or combination.")):hint(id);
            if(id>=ClearMovementFirst&&id<ClearMovementFirst+int(movementButtons.size()))
                value=chinese?L"停止检测这个动作的按键。":L"Stop observing keys for this action.";
            if(value.empty()&&!item.second.empty())value=std::wstring(tr(item.second));
            if(id==Attach&&health.severity==mc::Severity::Error)value+=(chinese?L"\n当前采集异常：":L"\nCapture problem: ")+std::wstring(captureDetails.begin(),captureDetails.end());
            bool conflict=((id>=TabRegional&&id<=TabTriangle)&&shortcutUnavailable(0))||(id==Visible&&shortcutUnavailable(1))||
                (id==Stop&&shortcutUnavailable(2))||(shortcutField&&shortcutUnavailable(size_t(id-ShortcutFirst)));
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
        if(!statusDetails.empty())statusTip+=L"\n\n"+statusDetails;
        if(health.reason==mc::HealthReason::CaptureError)statusTip+=L"\n"+std::wstring(captureDetails.begin(),captureDetails.end());
        if(!settingsError.empty())statusTip+=(chinese?L"\n设置问题：":L"\nSettings problem: ")+std::wstring(settingsError.begin(),settingsError.end());
        if(!inputs.rawReady)statusTip+=chinese?L"\n键鼠注册失败；可靠画面观测仍可使用，请重新启动软件重试。":L"\nRaw input registration failed. Reliable image observations can still be used; restart the app to retry.";
        tips.set(status,statusTip);tips.set(headerStatus,statusTip);

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
    void commitEditor();
    void loadEditor(int index);
    mc::Preferences runningPreferences() const;
    void populateLibrary(int index);
    void updateLibrary();
    void gameCommand(int id);
    void cycleRunningStyle();
    void pollGame(HWND foreground,const CaptureStats& sample,double now);
    void restartCapture(double now,bool manual);
    void refresh() {
        windows.clear(); SendMessageW(list,CB_RESETCONTENT,0,0);
        EnumWindows([](HWND w,LPARAM p)->BOOL {
            if(!candidateWindow(w))return TRUE;
            auto self=reinterpret_cast<App*>(p); auto entry=describe(w);
            if(!entry.title.empty()) {
                std::wstring label=entry.exe+L" ["+std::to_wstring(entry.pid)+L"] "+entry.title;
                self->windows.push_back(entry); SendMessageW(self->list,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label.c_str()));
            } return TRUE;
        },reinterpret_cast<LPARAM>(this));
        if(!windows.empty()) SendMessageW(list,CB_SETCURSEL,0,0);
    }
    void attach(WindowEntry entry);
    void stop(bool forgetTarget=true);
    void create(HINSTANCE instance);
    const wchar_t* connectionLabel(const CaptureStats& sample,bool foreground,double now) const {
        if(!selected.handle){
            if(automatic||!reconnectPath.empty())return chinese?L"等待游戏启动":L"Waiting for a game";
            return chinese?L"未选择游戏":L"No game selected";
        }
        if(sample.status=="Not started")return chinese?L"已发现游戏":L"Game found";
        if(sample.status=="Starting WGC")
            return recovery.retrying()?(chinese?L"正在尝试恢复":L"Recovering capture"):(chinese?L"正在连接":L"Connecting");
        if(!overlayOnly && recovery.failed(sample,now)){
            if(foreground)return chinese?L"采集失败，等待重试":L"Capture failed, waiting to retry";
            return chinese?L"采集失败，切回游戏后重试":L"Capture failed, return to retry";
        }
        if(!foreground)return chinese?L"游戏不在前台":L"Game in background";
        if(!sample.frames&&!overlayOnly)return chinese?L"正在等待画面":L"Waiting for a frame";
        return chinese?L"已连接":L"Connected";
    }
    void appendDiagnosticVectors(std::wostream& out,const mc::MotionSample& flow) {
        constexpr int center=4;
        const auto input=driver.prediction()[center];
        const auto fused=driver.fused()[center];
        const auto observed=driver.observedFlow()[center];
        const auto quality=driver.observedConfidence()[center];
        out<<L"\n\n"<<(chinese?L"中央区域 · px/s":L"Center region · px/s")
           <<std::fixed<<std::setprecision(1)
           <<L"\n"<<(chinese?L"输入预测  ":L"Input prediction  ")<<input.x<<L", "<<input.y
           <<L"\n"<<(chinese?L"光流  ":L"Optical flow  ");
        if(mc::motionFresh(flow,qpcSeconds())&&quality>0)
            out<<observed.x<<L", "<<observed.y<<L"  ·  "<<(chinese?L"可信度 ":L"confidence ")<<quality;
        else out<<L"N/A";
        out<<L"\n"<<(chinese?L"融合  ":L"Fused  ")<<fused.x<<L", "<<fused.y
           <<L"\n"<<(chinese?L"可见点阵节点  ":L"Overlay dot nodes  ")
           <<(IsWindowVisible(overlay.window())?std::to_wstring(overlay.dotCount()):L"N/A");
    }
    void updateMeshDiagnostics(bool selectedMesh,bool live) {
        if(!selectedMesh){
            setText(meshDiagnosticsStatus,chinese?L"网格点阵\n选用网格点阵后可查看节点和耗时。":L"Mesh dots\nSelect Mesh dots to view node counts and timings.");
            return;
        }
        if(!meshDiagnostics.timestamp){
            setText(meshDiagnosticsStatus,chinese?L"网格点阵\n等待网格点阵在游戏中显示。":L"Mesh dots\nWaiting for mesh dots to appear in the game.");
            return;
        }
        const auto& d=meshDiagnostics;
        const auto& m=d.stats;
        std::wostringstream text;
        text<<(live?(chinese?L"网格点阵 · 实时":L"Mesh dots · Live"):(chinese?L"网格点阵 · 最近一次记录":L"Mesh dots · Last sample"))
            <<L"\n"<<(chinese?L"可见点 ":L"Visible dots ")<<d.visible
            <<(chinese?L"  ·  节点 ":L"  ·  Nodes ")<<m.live+m.entering+m.leaving
            <<(chinese?L"  ·  网格面 ":L"  ·  Faces ")<<m.triangles
            <<L"\n"<<(chinese?L"渐入 / 渐出  ":L"Fading in / out  ")<<m.entering<<L" / "<<m.leaving
            <<L"\n"<<(chinese?L"累计补点 / 回收  ":L"Added / retired  ")<<m.inletBirths+m.centerBirths+m.interiorBirths<<L" / "<<m.retired
            <<L"\n"<<(chinese?L"累计拓扑更新  ":L"Topology changes  ")<<m.rewired
            <<L"\n"<<std::fixed<<std::setprecision(2)
            <<(chinese?L"采样窗口峰值（ms）":L"Sample window peaks (ms)")
            <<L"\n"<<(chinese?L"网格更新  ":L"Mesh update  ")<<d.updateMs
            <<(chinese?L"  ·  其中拓扑  ":L"  ·  Topology  ")<<d.topologyMs
            <<L"\n"<<(chinese?L"最近一次浮层提交  ":L"Last overlay submission  ")<<d.submitMs<<L" ms";
        setText(meshDiagnosticsStatus,text.str());
    }
    void tick() {
        if(smoke)setText(status,L"Software capture check "+std::to_wstring(int(qpcSeconds()*60)));
        double tickTime=qpcSeconds();float dt=float(tickTime-lastTick);lastTick=tickTime;
        if(sliderPreviewPending&&tickTime>=nextSliderPreview){sliderPreviewPending=false;nextSliderPreview=tickTime+1./30;invalidatePreview();}
        if(settingsDirty && tickTime>=saveAfter) flushSettings();
        if(keysMissing()&&tickTime>=nextHotkeyRetry){registerKeys();updateBindingLabels();updateTips();}
        if(SendMessageW(logBox,BM_GETCHECK,0,0)==BST_CHECKED&&!log.enabled()){SendMessageW(logBox,BM_SETCHECK,BST_UNCHECKED,0);updateTips();}
        capture.setDiagnosticRecording(log.enabled());
        HWND foreground=GetForegroundWindow();
        pollGame(foreground,capture.snapshot(),tickTime);
        const auto running=runningPreferences();
        const bool inputForeground=selected.handle && foreground==selected.handle && !IsIconic(selected.handle);
        const auto rules=activeRules();
        menu.setForeground(inputForeground,rules);
        const auto* currentMenu=menu.active(rules);
        const auto effect=currentMenu?currentMenu->effect:mc::MenuEffect::Normal;
        if(effect!=lastMenuEffect){
            driver.reset();screenField.reset();overlay.invalidate();lastDebug=0;
            if(effect==mc::MenuEffect::Normal)resumeFlowAfter=tickTime;
            lastMenuEffect=effect;
        }
        auto displayed=running;
        if(effect==mc::MenuEffect::Static)displayed.selected=2;
        const auto visual=mc::visualSettingsFromPreferences(displayed);
        const bool runtimeDynamic=running.selected!=2;
        const bool menuAllowsMotion=effect==mc::MenuEffect::Normal;
        if(lastRunningStyle!=running.selected){
            lastRunningStyle=running.selected;
            driver.reset();screenField.reset();overlay.resetMotion();
            meshDiagnostics={};meshUpdatePeakMs=meshTopologyPeakMs=0;
        }
        capture.setFlowEnabled(runtimeDynamic&&menuAllowsMotion);
        capture.setBackgroundEnabled(effect!=mc::MenuEffect::Hide&&(visual.appearance==1||visual.appearance==3));
        RECT previewTarget{};if(selected.handle&&GetClientRect(selected.handle,&previewTarget)
            &&(previewTarget.right!=previewTargetWidth||previewTarget.bottom!=previewTargetHeight)){
            previewTargetWidth=previewTarget.right;previewTargetHeight=previewTarget.bottom;invalidatePreview();}
        auto fresh=capture.snapshot();overlay.setBackground(fresh.running?fresh.light:mc::BackgroundLight{});
        inputs.setTarget(selected.handle,inputForeground);
        auto input=inputs.poll(tickTime,dt,runningMovement());
        rawEvents=inputs.mouseEvents+inputs.keyEvents;rawTimestamp=inputs.lastEvent;
        double fusionStart=qpcSeconds();
        static const mc::MotionSample noFlow;
        const bool currentSize=fresh.motion.sourceWidth==fresh.width&&fresh.motion.sourceHeight==fresh.height;
        const auto& observed=currentSize&&fresh.motion.timestamp>=resumeFlowAfter?fresh.motion:noFlow;
        const bool motionEnabled=runtimeDynamic&&!motionPaused&&menuAllowsMotion;
        auto velocity=driver.update(observed,input,fusionStart,dt,motionEnabled,inputForeground);
        if(chromaAssist)capture.setChromaHint({driver.prediction(),driver.fieldIntent().timestamp});
        fusionMs=(qpcSeconds()-fusionStart)*1000;
        const double fieldStart=qpcSeconds();
        if(motionEnabled&&inputForeground){
            const auto mapping=captureFieldMapping(selected.handle,previewTarget,fresh);
            screenField.update(observed,driver.fieldIntent(),mapping,fusionStart,true,driver.evidence().stationary,
                               running.selected==mc::TriangleStyle,&driver.screenFixed(),visual.meshDepth&&running.selected==mc::TriangleStyle);
            fieldMs=(qpcSeconds()-fieldStart)*1000;
        }else{
            if(screenField.frame().targetWidth)screenField.reset();
            fieldMs=0;
        }
        if(log && fresh.motion.completedTimestamp>0 && fresh.motion.completedTimestamp!=lastFlowLogged){
            lastFlowLogged=fresh.motion.completedTimestamp;
            const auto& m=fresh.motion;
            log<<"# flow_sample elapsed_s="<<std::fixed<<std::setprecision(6)<<tickTime-started
                <<", source_qpc_s="<<m.timestamp<<", source_age_ms="<<(tickTime-m.timestamp)*1000
                <<", backend="<<(m.analysisWidth<=0?"N/A":m.denseFlow?"DIS_CPU_FAST":"legacy")<<", pair_ms="<<m.pairIntervalMs
                <<", observed_capture_fps="<<fresh.observedCaptureFps<<", target_analysis_fps="<<fresh.targetAnalysisFps
                <<", source_readbacks="<<fresh.sourceReadbacks<<", invalid_pairs="<<fresh.invalidPairs<<", readback_call_ms="<<fresh.readbackCallMs
                <<", analysis_ms="<<fresh.analysisMs<<", dense_pixels="<<m.densePixels
                <<", issue="<<int(m.issue)<<", sampled="<<m.features<<", tracked="<<m.tracked<<", measured_cells="<<m.observedCells
                <<", rejected_texture="<<m.rejectedTexture<<", rejected_photo="<<m.rejectedPhotometric
                <<", rejected_consistency="<<m.rejectedConsistency<<", state="<<int(driver.motionState())
                <<", chroma_active="<<m.chromaActive<<", chroma_ms="<<m.chromaMs<<", grayscale_tracked="<<m.grayscaleTracked
                <<", chroma_attempts="<<m.chromaAttempts<<", chroma_accepted="<<m.chromaAccepted<<", chroma_cells="<<m.chromaCells
                <<", chroma_budget_limited="<<m.chromaBudgetLimited
                <<", chroma_quiet_cells="<<m.chromaQuietCells<<", chroma_corrob_cells="<<m.chromaCorroboratedCells
                <<", source="<<int(driver.source())<<", foreground="<<inputForeground
                <<", drive_policy=direct_local_with_fixed_foreground_correction"
                <<", screen_fixed_cells="<<driver.screenFixed().correctedCells()<<", screen_fixed_regions="<<driver.correctedRegions()
                <<", direct_cells="<<driver.evidence().directCells<<", quiet_regions="<<driver.evidence().quietRegions<<", motion_veto="<<driver.evidence().motionVeto
                <<", inferred_regions="<<driver.inferredRegions()<<", protected_regions="<<driver.protectedRegions()<<", broad_motion="<<driver.evidence().broadMotion
                <<", static_evidence="<<driver.evidence().stationary<<", quiet_cells="<<driver.evidence().quietCells
                <<", connected_moving_cells="<<driver.evidence().movingCells<<", global_q="<<m.globalConfidence<<", moving_q="<<m.movingBackgroundConfidence<<", regions=";
            for(int r=0;r<9;++r){auto raw=m.velocity[r],chosen=driver.observedFlow()[r];
                log<<r<<':'<<raw.x<<':'<<raw.y<<':'<<m.regionConfidence[r]<<':'<<chosen.x<<':'<<chosen.y<<':'<<driver.observedConfidence()[r]<<' ';}
            log<<'\n';
        }

        const auto& profile=running.profiles[running.selected];
        float motionGain=profile.gain*(profile.inverted?-1.f:1.f);
        for(auto& v:velocity) {v.x*=motionGain;v.y*=motionGain;}
        const bool frozen=motionPaused||!menuAllowsMotion;
        bool dynamicActive=runtimeDynamic && inputForeground && !frozen;
        const bool meshEligible=displayed.selected==mc::TriangleStyle&&dynamicActive&&visible&&fresh.running&&
                                fresh.timestamp>0&&fusionStart-fresh.timestamp<.30;
        if(meshEligible&&!meshMotionEligible)meshMotionAfter=fusionStart;
        meshMotionEligible=meshEligible;
        const bool meshReady=meshEligible&&screenField.frame().observationTimestamp>=meshMotionAfter;
        auto shownFused=driver.fused();
        if(running.selected==mc::TriangleStyle&&(showDebug||log||page==Page::Diagnostics)){
            const auto& frame=screenField.frame();
            for(int r=0;r<9;++r){
                const auto sample=meshReady?screenField.sample({(r%3+.5f)*frame.targetWidth/3,(r/3+.5f)*frame.targetHeight/3}):mc::FieldSample{};
                shownFused[r]=sample.velocity;
                velocity[r]=mc::meshVelocity(mc::compensateMeshSample(sample,motionGain),frame.targetHeight);
            }
        }
        if(!captureOnly && effect!=mc::MenuEffect::Hide && selected.handle && visible && !IsIconic(selected.handle) && (foreground==selected.handle || smoke)) {
            RECT rect{}; GetClientRect(selected.handle,&rect); POINT origin{}; ClientToScreen(selected.handle,&origin); OffsetRect(&rect,origin.x,origin.y);
            const auto& settings=visual;
            if(showDebug && tickTime-lastDebug>=.1) {
                mc::DebugSnapshot d;d.input=driver.prediction();d.fused=shownFused;d.compensation=velocity;d.flow=observed;d.selectedFlow=driver.observedFlow();d.selectedConfidence=driver.observedConfidence();d.hasSelectedFlow=true;d.backgroundBlend=driver.backgroundBlend();d.motionState=driver.motionState();d.heldRegions=driver.heldRegions();
                if(settings.style==2&&running.selected==mc::TriangleStyle){d.fused={};d.compensation={};}
                d.modeledCells=screenField.stats().modeled;d.motionBoundaries=screenField.stats().boundaries;
                d.backgroundSupport=screenField.stats().backgroundSupport;d.foregroundSuppressed=screenField.stats().foregroundSuppressed;
                d.independentCells=screenField.stats().independentCells;d.backgroundActive=screenField.stats().backgroundActive;
                d.backgroundHeld=screenField.stats().backgroundHeld;
                d.cameraConfidence=screenField.cameraModel().confidence;
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
            const bool meshFrozen=settings.style==mc::TriangleStyle&&!meshReady;
            if(overlay.draw(rect,settings,{velocity,frozen||meshFrozen?0.f:dt,displayed.selected!=2},
                            meshReady?&screenField:nullptr,motionGain,meshReady&&screenField.stats().moving,dt)) {
                drawMs=(qpcSeconds()-before)*1000.; drawMax=std::max(drawMax,drawMs); ++draws;
            }
            if(settings.style==mc::TriangleStyle){
                meshUpdatePeakMs=std::max(meshUpdatePeakMs,double(overlay.liveMesh().updateMs()));
                if(overlay.liveMesh().updateMs()>0)meshTopologyPeakMs=std::max(meshTopologyPeakMs,overlay.liveMesh().stats().topologyMs);
            }
        } else {overlay.hide();lastDebug=0;}
        bool shown=IsWindowVisible(overlay.window())!=FALSE;
        if(shown!=wasOverlayVisible) { log<<"# overlay_visible="<<shown<<", elapsed_s="<<qpcSeconds()-started<<'\n'; log.flush(); wasOverlayVisible=shown; }
        double now=qpcSeconds();
        const bool meshVisible=shown&&displayed.selected==mc::TriangleStyle;
        if(meshVisible&&(!meshDiagnostics.timestamp||now-lastLog>=1.)){
            meshDiagnostics={overlay.liveMesh().stats(),overlay.dotCount(),now,meshUpdatePeakMs,meshTopologyPeakMs,drawMs};
        }
        if(now-lastLog>=1.) {
            auto s=capture.snapshot();captureDetails=s.status;health=mc::captureHealth(selected.handle!=nullptr,s.running,inputForeground,runtimeDynamic&&menuAllowsMotion,s.status,s.motion,now);
            if(selected.handle&&!overlayOnly&&recovery.failed(s,now)){
                health.reason=mc::HealthReason::CaptureError;
                health.severity=mc::Severity::Error;
            }
            switch(health.reason){
                case mc::HealthReason::NoTarget:healthMessage=chinese?L"等待连接游戏":L"Waiting for a game";break;
                case mc::HealthReason::Starting:healthMessage=chinese?L"正在启动采集":L"Starting capture";break;
                case mc::HealthReason::CaptureError:healthMessage=chinese?L"采集异常，请悬停查看详情":L"Capture problem; hover for details";break;
                case mc::HealthReason::Paused:healthMessage=chinese?L"切回目标窗口后恢复浮层":L"Return to the target to resume the overlay";break;
                case mc::HealthReason::Static:
                    healthMessage=currentMenu?(chinese?L"菜单规则已启用":L"Menu rule active"):
                        (chinese?L"静态方案已连接":L"Static reference connected");break;
                case mc::HealthReason::Waiting:healthMessage=chinese?L"等待有效画面观测":L"Waiting for visual observations";break;
                case mc::HealthReason::Observation:healthMessage=chinese?L"画面观测不足或已过期，动态方案保持":L"Visual observations low or expired; style unchanged";break;
                default:healthMessage=chinese?L"画面观测正常":L"Visual observations available";break;
            }
            if(!settingsError.empty()&&health.severity!=mc::Severity::Error){health.severity=mc::Severity::Warning;healthMessage=chinese?L"设置保存或读取异常，请悬停查看详情":L"Settings problem; hover for details";}
            if(!inputs.rawReady&&health.severity!=mc::Severity::Error){health.severity=mc::Severity::Warning;healthMessage=chinese?L"键鼠观察不可用，请悬停查看详情":L"Input observation unavailable; hover for details";}
            InvalidateRect(status,nullptr,FALSE);InvalidateRect(headerStatus,nullptr,FALSE);
            double fps=double(s.frames>=previousFrames?s.frames-previousFrames:0)/(now-lastLog);
            double age=s.timestamp>0?(now-s.timestamp)*1000:-1;
            FILETIME c{},e{},k{},u{}; GetProcessTimes(GetCurrentProcess(),&c,&e,&k,&u);
            double cpu=(double((uint64_t(k.dwHighDateTime)<<32)|k.dwLowDateTime)+double((uint64_t(u.dwHighDateTime)<<32)|u.dwLowDateTime))/1e7;
            PROCESS_MEMORY_COUNTERS memory{}; GetProcessMemoryInfo(GetCurrentProcess(),&memory,sizeof(memory));
            log<<std::fixed<<std::setprecision(3)<<now-started<<','<<s.frames<<','<<fps<<','<<s.ageMs<<','<<age<<','<<s.width<<','<<s.height<<','<<drawMs<<','<<drawMax<<','<<draws<<','<<rawEvents<<','<<rawTimestamp<<','<<runtimeDynamic<<','<<!runtimeDynamic<<','<<s.running<<','<<100*(cpu-previousCpu)/(now-lastLog)<<','<<memory.WorkingSetSize/(1024.*1024.)<<'\n';
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
            for(int r=0;r<9;++r){auto i=driver.prediction()[r],f=shownFused[r],c=velocity[r];
                log<<r<<":input:"<<i.x<<':'<<i.y<<":fused:"<<f.x<<':'<<f.y<<":dots:"<<std::clamp(c.x,-1200.f,1200.f)<<':'<<std::clamp(c.y,-1200.f,1200.f)<<' ';}
            log<<'\n';
            float offsetMax=0;for(auto v:velocity) offsetMax=std::max(offsetMax,std::hypot(v.x,v.y));
            if(log){
                const auto& frame=screenField.frame();
                const auto& cells=screenField.stats();
                const auto& model=screenField.cameraModel();
                const auto center=screenField.sample({frame.targetWidth*.5f,frame.targetHeight*.5f});
                log<<"# continuous_field_qpc_s="<<frame.timestamp<<", source_flow_qpc_s="<<frame.observationTimestamp<<", build_ms="<<fieldMs
                   <<", source="<<frame.sourceWidth<<'x'<<frame.sourceHeight<<", target="<<frame.targetWidth<<'x'<<frame.targetHeight
                   <<", mapping_known="<<frame.mappingKnown<<", measured="<<cells.measured<<", modeled="<<cells.modeled
                   <<", held="<<cells.held<<", input="<<cells.input<<", stationary="<<cells.stationary
                   <<", background_support="<<cells.backgroundSupport<<", foreground_suppressed="<<cells.foregroundSuppressed
                   <<", background_active="<<cells.backgroundActive<<", background_held="<<cells.backgroundHeld
                   <<", candidate_moving_cells="<<cells.candidateMovingCells<<", candidate_direction_cells="<<cells.candidateDirectionCells
                   <<", candidate_speed_matches="<<cells.candidateSpeedMatches<<", bounded_background_cells="<<cells.boundedBackgroundCells
                   <<", slow_direct_cells="<<cells.slowDirectCells<<", neighbor_filled_cells="<<cells.neighborFilledCells
                   <<", depth_enabled="<<visual.meshDepth<<", depth_demand="<<frame.depthDemand
                   <<", missing_by_band="<<cells.unobservedByBand[0]<<':'<<cells.unobservedByBand[1]<<':'<<cells.unobservedByBand[2]
                   <<", quiet_by_band="<<cells.quietByBand[0]<<':'<<cells.quietByBand[1]<<':'<<cells.quietByBand[2]
                   <<", neighbor_by_band="<<cells.neighborByBand[0]<<':'<<cells.neighborByBand[1]<<':'<<cells.neighborByBand[2]
                   <<", independent_cells="<<cells.independentCells
                   <<", screen_fixed_cells="<<driver.screenFixed().correctedCells()<<", screen_fixed_regions="<<driver.correctedRegions()
                   <<", preserved_motion_cells="<<cells.preservedMotionCells
                   <<", boundaries="<<cells.boundaries<<", camera_confidence="<<model.confidence
                   <<", camera_residual_px_s="<<model.residual<<", center_valid="<<center.valid
                   <<", center_px_s="<<center.velocity.x<<':'<<center.velocity.y<<", center_confidence="<<center.confidence<<'\n';
                if(running.selected==mc::TriangleStyle&&IsWindowVisible(overlay.window())&&displayed.selected==mc::TriangleStyle){
                    const auto& mesh=overlay.liveMesh();const auto& m=mesh.stats();
                    std::array<int,3> visibleByBand{},slowByBand{};
                    const float w=float(previewTarget.right-previewTarget.left),h=float(previewTarget.bottom-previewTarget.top);
                    for(const auto& node:mesh.nodes()){
                        const auto p=node.position;
                        if(node.opacity<=.05f||p.x<0||p.x>=w||p.y<0||p.y>=h)continue;
                        if(mc::dotVisibility(p.x,p.y,w,h,visual.radius,visual.holeDiameter)*visual.opacity*node.opacity<=.01f)continue;
                        const int band=std::min(2,int(p.y*3/h));
                        ++visibleByBand[band];slowByBand[band]+=node.velocity.x*node.velocity.x+node.velocity.y*node.velocity.y<25;
                    }
                    log<<"# live_mesh_field_ready="<<meshReady<<", spacing_px="<<mesh.spacing()<<", update_ms="<<mesh.updateMs()
                       <<", peak_update_ms="<<meshUpdatePeakMs<<", peak_topology_ms="<<meshTopologyPeakMs
                       <<", visible="<<overlay.dotCount()<<", live="<<m.live<<", entering="<<m.entering<<", leaving="<<m.leaving
                       <<", triangles="<<m.triangles<<", inlet_births="<<m.inletBirths<<", center_births="<<m.centerBirths
                       <<", interior_births="<<m.interiorBirths<<", retired="<<m.retired<<", center_exits="<<m.centerExits
                       <<", rewired="<<m.rewired<<", topology_ms="<<m.topologyMs
                       <<", visible_by_band="<<visibleByBand[0]<<':'<<visibleByBand[1]<<':'<<visibleByBand[2]
                       <<", slow_nodes_by_band="<<slowByBand[0]<<':'<<slowByBand[1]<<':'<<slowByBand[2]<<'\n';
                }
            }
            meshUpdatePeakMs=meshTopologyPeakMs=0;
            log<<"# analysis_grid=9x9, analysis_width="<<s.motion.analysisWidth<<", analysis_height="<<s.motion.analysisHeight<<", observed_cells="<<s.motion.observedCells<<", observations="<<s.motion.observations.size()<<", cells=";
            for(int cell=0;cell<81;++cell){const auto& c=s.motion.cells[cell];log<<cell<<':'<<c.samples<<':'<<c.tracked<<':'<<c.inliers<<':'<<c.confidence<<':'<<c.velocity.x<<':'<<c.velocity.y<<':'<<c.smoothed.x<<':'<<c.smoothed.y<<':'<<c.inferred<<' ';}log<<'\n';
            log<<"# cell_evidence=";for(int i=0;i<81;++i){const auto& c=s.motion.cells[i];log<<i<<':'<<c.directConfidence<<':'<<c.directInliers<<':'<<c.rejectedTexture<<':'<<c.rejectedPhoto<<':'<<c.rejectedConsistency<<':'<<c.directVelocity.x<<':'<<c.directVelocity.y<<' ';}log<<'\n';
            log<<"# brightness_qpc_s="<<s.light.timestamp<<", brightness_updates="<<s.brightnessUpdates<<", brightness_cpu_ms="<<s.brightnessMs<<", brightness_age_ms="<<(s.light.timestamp>0?(now-s.light.timestamp)*1000:-1)<<", appearance="<<visual.appearance<<'\n';
            log<<"# pipeline_completed_qpc_s="<<s.motion.completedTimestamp<<", completed_age_ms="<<(s.motion.completedTimestamp>0?(now-s.motion.completedTimestamp)*1000:-1)<<", flow_fresh="<<mc::motionFresh(s.motion,now)<<", analysis_start_age_ms="<<s.analysisStartAgeMs<<", capture_dropped="<<s.droppedCapture<<", analysis_replaced="<<s.droppedAnalysis<<'\n';
            int trustedRegions=0;for(int r=0;r<9;++r)if(mc::regionMotionTrusted(s.motion,r,now))++trustedRegions;
            log<<"# trusted_local_regions="<<trustedRegions<<", stationary_background_rejected="<<driver.stationaryBackgroundRejected()
                <<", motion_state="<<int(driver.motionState())<<", held_regions="<<driver.heldRegions()
                <<", driver_source_flow_qpc_s="<<fresh.motion.timestamp<<", driver_source_age_ms="<<(fresh.motion.timestamp>0?(tickTime-fresh.motion.timestamp)*1000:-1)<<'\n';
            log<<"# analysis_cadence=adjacent_source_pairs_worker30, observed_capture_fps="<<s.observedCaptureFps<<", target_analysis_fps="<<s.targetAnalysisFps<<'\n';
            log<<"# borderless_state="<<s.borderlessState<<'\n';
            log<<"# flow_backend="<<(s.motion.analysisWidth<=0?"N/A":s.motion.denseFlow?"DIS_CPU_FAST":"legacy")<<", pair_ms="<<s.motion.pairIntervalMs<<", dense_pixels="<<s.motion.densePixels<<'\n';
            log<<"# tracking_extra_features="<<s.motion.trackingExtraFeatures<<", pyramid_reused="<<s.motion.pyramidReused<<'\n';
            log<<"# background_blend="<<driver.backgroundBlend()<<", global_confidence="<<s.motion.globalConfidence<<", global_support_cells="<<s.motion.globalSupportCells<<", moving_background_confidence="<<s.motion.movingBackgroundConfidence<<", moving_background_cells="<<s.motion.movingBackgroundSupportCells<<", chosen_flow=";
            for(int r=0;r<9;++r){auto v=driver.observedFlow()[r];log<<r<<':'<<v.x<<':'<<v.y<<':'<<driver.observedConfidence()[r]<<' ';}log<<'\n';
            log<<"# flow_confidence="<<s.motion.confidence<<", analysis_ms="<<s.analysisMs<<", analyses="<<s.analyses<<", history_resets="<<s.flowResets<<", features="<<s.motion.features<<", tracked="<<s.motion.tracked<<", inliers="<<s.motion.inliers<<", issue="<<int(s.motion.issue)<<", mean_delta="<<s.motion.meanDelta<<", dynamic_active="<<effective<<'\n';
            log<<"# input_mouse_events="<<inputs.mouseEvents<<", input_key_events="<<inputs.keyEvents<<", pad_changes="<<inputs.padChanges
               <<", mouse_rate_x="<<input.mouseRate.x<<", mouse_rate_y="<<input.mouseRate.y<<", move_x="<<input.move.x<<", move_y="<<input.move.y
               <<", look_x="<<input.look.x<<", look_y="<<input.look.y<<", pad_connected="<<input.pad<<", drive_source="<<int(driver.source())<<", raw_ready="<<inputs.rawReady
               <<", prone="<<input.prone<<", mouse_buttons="<<input.mouseButtons<<", pad_buttons="<<input.padButtons<<", trigger_l="<<input.triggers.x<<", trigger_r="<<input.triggers.y<<", xinput_available="<<inputs.padApiAvailable()<<'\n';
            log<<"# motion_qpc_s="<<s.motion.timestamp<<", motion_age_ms="<<(s.motion.timestamp>0?(now-s.motion.timestamp)*1000:-1)<<", regions=";
            for(size_t r=0;r<9;++r) log<<r<<':'<<s.motion.velocity[r].x<<':'<<s.motion.velocity[r].y<<':'<<s.motion.regionConfidence[r]<<' ';
            log<<", field_speed_max_px_s="<<offsetMax<<", dot_count="<<overlay.dotCount()<<", phase_max_px="<<overlay.phaseMax()<<", style="<<running.selected<<", enhancement="<<visual.enhancement<<", boundary_fade="<<visual.boundaryFade<<'\n';log.flush();
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
                <<styleName(running.selected)
                <<(runtimeDynamic&&selected.handle?L" · ":L"")<<(runtimeDynamic&&selected.handle?reason:L"")
                <<L"\n"<<std::fixed<<std::setprecision(1)<<fps<<L" fps | "<<(chinese?L"局部有效 ":L"Local valid ")<<trustedRegions<<L"/9"
                <<L"\n";
            if(selected.handle && !s.running) text<<captureStatus;
            else text<<(chinese?L"分析 ":L"Analysis ")<<s.analysisMs<<L" ms | "<<(chinese?L"速度 ":L"Speed ")<<offsetMax<<L" px/s";
            text<<L"\n"<<(chinese?L"输入：":L"Input: ")
                <<(inputs.rawReady?(chinese?L"键鼠已接入":L"KB/mouse ready"):(chinese?L"键鼠注册失败":L"raw registration failed"))
                <<(input.pad?(chinese?L" | 手柄已连接":L" | pad connected"):(inputs.padApiAvailable()?(chinese?L" | 无 XInput 手柄":L" | no XInput pad"):(chinese?L" | XInput 不可用":L" | XInput unavailable")));
            if(!settingsError.empty())text<<L"\n"<<(chinese?L"设置保存/读取失败":L"Settings could not be loaded/saved");
            if(health.severity!=mc::Severity::Normal||health.reason==mc::HealthReason::NoTarget)text<<L"\n"<<healthMessage;
            statusDetails=text.str();
            const auto connection=connectionLabel(s,inputForeground,now);
            const std::wstring target=selected.handle?selected.exe:(chinese?L"未连接":L"No target");
            setText(headerStatus,target+L"  ·  "+connection);
            setText(status,connection);
            std::wstring menuState;
            if(currentMenu)menuState=currentMenu->name+L"  ·  "+
                (chinese?L"还需返回 ":L"Return presses left: ")+std::to_wstring(menu.remaining())+
                L"  ·  "+menuEffectName(effect);
            setText(menuStateLabel,menuState.empty()?tr(L"Windows"):menuState);
            setText(targetWindowLabel,selected.handle&&!selected.title.empty()?selected.title:target);
            setText(overviewStyle,std::wstring(styleName(running.selected))+L"  ↻");
            EnableWindow(pauseBox,runtimeDynamic);
            EnableWindow(stopButton,selected.handle||!reconnectPath.empty()||automatic);
            EnableWindow(retryButton,selected.handle&&!overlayOnly&&recovery.failed(s,now));
            if(page==Page::Diagnostics){
                std::wostringstream analysisMode;
                if(!chromaAssist)analysisMode<<(chinese?L"当前分析：灰度 DIS":L"Analysis: grayscale DIS");
                else if(s.chromaUnavailable)analysisMode<<(chinese?L"色度采集不可用，已回退到灰度 DIS":L"Color capture unavailable. Using grayscale DIS.");
                else if(!s.motion.chromaActive||!mc::motionFresh(s.motion,now))
                    analysisMode<<(chinese?L"色度辅助已开启，等待动态画面":L"Color assist enabled. Waiting for motion analysis.");
                else analysisMode<<std::fixed<<std::setprecision(1)<<(chinese?L"色度辅助：":L"Color assist: ")<<s.motion.chromaMs
                    <<(chinese?L" ms · 补充 ":L" ms · Added ")<<s.motion.chromaAccepted
                    <<(chinese?L" 点 / ":L" matches across ")<<s.motion.chromaCells<<(chinese?L" 格":L" cells");
                setText(chromaStatus,analysisMode.str());
                updateMeshDiagnostics(running.selected==mc::TriangleStyle,meshVisible);
                if(showDebug) {
                    std::wostringstream detail;
                    detail<<connection<<L"\n"<<statusDetails;
                    if(!menuState.empty())detail<<L"\n"<<menuState;
                    appendDiagnosticVectors(detail,s.motion);
                    setText(diagnosticsStatus,detail.str());
                } else {
                    setText(diagnosticsStatus,tr(L"Enable diagnostics to view capture and motion data."));
                }
            }
            updateTips();
            previousFrames=s.frames; lastLog=now;
        }
        if(smoke && now-started>6) {
            auto s=capture.snapshot(); failed=s.frames==0 || draws==0 || !s.running || (dynamic?s.analyses==0:(s.analyses!=0||s.brightnessUpdates==0));
            log<<"# smoke_result="<<(failed?"FAIL":"PASS")<<", frames="<<s.frames<<", draws="<<draws<<", analyses="<<s.analyses<<", brightness_updates="<<s.brightnessUpdates<<", status="<<s.status<<'\n';
            DestroyWindow(hwnd);
        }
    }
    void command(int id) {
        if(id>=NavOverview&&id<=NavGeneral){
            const auto next=static_cast<Page>(id-NavOverview);
            if(page==Page::Games&&next!=page&&!confirmRuleNavigation())return;
            if(page!=next){
                if(armedBinding)armBinding(nullptr);
                if(page==Page::Games){
                    if(pendingProfileFollow)followGameProfile(activeGame,false);
                    else selectBindingProfile();
                }
                page=next;updatePageVisibility();
                if(page==Page::Overview)refresh();
                if(page==Page::Diagnostics)lastLog=qpcSeconds()-1;
            }
            return;
        }
        if(id==RuleOpen||id==RuleReturn||
           (id>=MovementFirst&&id<MovementFirst+int(movementButtons.size()))||
           (id>=ShortcutFirst&&id<ShortcutFirst+int(shortcutButtons.size()))){
            armBinding(GetDlgItem(hwnd,id));return;
        }
        if(id>=MenuSelect&&id<=ClearMenuReturn){menuCommand(id);return;}
        if(id==ResetShortcuts){changeShortcuts(mc::defaultShortcuts());return;}
        if(id>=ClearShortcutFirst&&id<ClearShortcutFirst+int(shortcutButtons.size())){
            auto requested=registry.shortcuts;
            requested[id-ClearShortcutFirst]=0;
            changeShortcuts(requested);
            return;
        }
        if(id>=ClearMovementFirst&&id<ClearMovementFirst+int(movementButtons.size())){
            setMovementBinding(size_t(id-ClearMovementFirst),mc::disabledMovement);
            return;
        }
        if(id==ResetMovement){
            const int index=registry.find(menuGamePath);
            if(index>=0&&registryWritable){
                registry.games[index].movement={};settingsDirty=true;flushSettings();
                if(activeGame==index){driver.reset();inputs.clear();}
                updateBindingLabels();
            }
            return;
        }
        if(id==OverviewStyle){cycleRunningStyle();return;}
        if(id==RetryCapture){restartCapture(qpcSeconds(),true);return;}
        if(id>=GameChoice&&id<=ResetGame){gameCommand(id);return;}
        if(id==PauseMotion){
            motionPaused=SendMessageW(pauseBox,BM_GETCHECK,0,0)==BST_CHECKED;
            driver.reset();screenField.reset();
            if(!motionPaused)resumeFlowAfter=qpcSeconds();
            overlay.invalidate();
            lastLog=qpcSeconds()-1;
            return;
        }
        if(id==PageDots||id==PageReticle){
            settingsPage=id==PageReticle?1:0;
            scrollDragging=false;
            if(GetCapture()==settingsPane)ReleaseCapture();
            updatePageVisibility();return;
        }
        if(id==RecordLog){setLogging(SendMessageW(logBox,BM_GETCHECK,0,0)==BST_CHECKED);return;}
        if(id==MeshDepth){markSettings();updatePageVisibility();return;}
        if(id==LiveMeshEdges){
            const bool enabled=SendMessageW(liveMeshEdgesBox,BM_GETCHECK,0,0)==BST_CHECKED;
            overlay.setMeshEdgesVisible(enabled);
            log<<"# live_mesh_edges="<<enabled<<", elapsed_s="<<qpcSeconds()-started<<'\n';log.flush();
            return;
        }
        if(id==Theme){preferences.theme=(preferences.theme+1)%3;populateTheme();applyTheme();updateTips();markSettings();return;}
        if(id==CaptionMinimize){ShowWindow(hwnd,SW_MINIMIZE);return;}
        if(id==CaptionClose){SendMessageW(hwnd,WM_CLOSE,0,0);return;}
        if(id==Instructions) {mc::UsageWindow::show(hwnd,chinese);return;}
        if(id==DebugParameters) {
            showDebug=!showDebug;lastDebug=0;lastLog=qpcSeconds()-1;overlay.setDebugEnabled(showDebug);

            SetWindowTextW(debugButton,showDebug?(chinese?L"隐藏调试参数":L"Hide debug parameters"):tr(L"Show debug parameters"));
            InvalidateRect(debugButton,nullptr,FALSE);return;
        }
        if(id>=ReticleFirst && id<ReticleFirst+mc::ReticleShapeCount && controlsReady){
            preferences.reticleShape=id-ReticleFirst;populateShapes(preferences.reticleShape);
            overlay.invalidate();markSettings();updateTips();return;
        }
        if(id>=TabRegional && id<=TabTriangle) {selectStyle(id-TabRegional);return;}
        if(id==DotColor) chooseColor(dotColor);
        if(id==ReticleColor) chooseColor(crossColor);
        if(id==Staggered)overlay.invalidate();
        if(id==Language) {chinese=!chinese;populateShapes(preferences.reticleShape);for(const auto& label:labels) if(!label.second.empty()) SetWindowTextW(label.first,tr(label.second));populateTheme();populateLibrary(int(SendMessageW(gameChoice,CB_GETCURSEL,0,0))-1);updateLibrary();selectBindingProfile();populateEdges(preferences.profiles[preferences.selected].edges);populateAppearance(preferences.profiles[preferences.selected].appearance);SetWindowTextW(debugButton,showDebug?(chinese?L"隐藏调试参数":L"Hide debug parameters"):tr(L"Show debug parameters"));refreshValueLabels();updatePageVisibility();InvalidateRect(hwnd,nullptr,FALSE);lastLog=qpcSeconds()-1;}
        if(id==Refresh) refresh();
        if(id==Attach){gameCommand(id);return;}
        if(id==Auto){automatic=SendMessageW(autoBox,BM_GETCHECK,0,0)==BST_CHECKED;nextProbe=0;lastLog=qpcSeconds()-1;}
        if(id==Visible){toggleOverlay();return;}
        if(id==Stop) {automatic=false; SendMessageW(autoBox,BM_SETCHECK,BST_UNCHECKED,0); stop();}
        if(id!=Refresh) markSettings();
    }
};
#include "game_session.inl"
#include "menu_settings_ui.inl"
#include "binding_ui.inl"
#include "workbench_ui.inl"

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
        if(m==WM_GETMINMAXINFO){
            auto limits=reinterpret_cast<MINMAXINFO*>(b);
            auto work=mc::monitorWorkArea(MonitorFromWindow(w,MONITOR_DEFAULTTONEAREST));
            int dpi=int(GetDpiForWindow(w));
            limits->ptMinTrackSize={std::min(work.right-work.left,LONG(MulDiv(980,dpi,96))),
                                   std::min(work.bottom-work.top,LONG(MulDiv(640,dpi,96)))};
            return 0;
        }
        if(m==WM_PAINT) {app->paint();return 0;}
        if(m==WM_ERASEBKGND)return 1;
        if(m==WM_MOUSEWHEEL&&app->page==Page::Visuals){POINT p{GET_X_LPARAM(b),GET_Y_LPARAM(b)};RECT r{};GetWindowRect(app->settingsPane,&r);if(PtInRect(&r,p))return SendMessageW(app->settingsPane,m,a,b);}
        if(m==WM_HSCROLL) {app->sliderChanged(reinterpret_cast<HWND>(b),LOWORD(a));return 0;}
        if(m==WM_CTLCOLOREDIT){auto dc=reinterpret_cast<HDC>(a);SetBkColor(dc,mc::fluent::surface);SetTextColor(dc,mc::fluent::ink);return reinterpret_cast<LRESULT>(app->card);}
        if(m==WM_CTLCOLORLISTBOX){auto dc=reinterpret_cast<HDC>(a);SetBkColor(dc,mc::fluent::surface);SetTextColor(dc,mc::fluent::ink);return reinterpret_cast<LRESULT>(mc::ui::surfaceBrush());}
        if(m==WM_CTLCOLORSTATIC || m==WM_CTLCOLORBTN) {
            HDC dc=reinterpret_cast<HDC>(a);SetTextColor(dc,mc::fluent::ink);
            if(reinterpret_cast<HWND>(b)==app->status||reinterpret_cast<HWND>(b)==app->headerStatus)SetTextColor(dc,mc::fluent::severityColor(app->health.severity));
            SetBkMode(dc,TRANSPARENT);
            RECT r{};GetWindowRect(reinterpret_cast<HWND>(b),&r);MapWindowPoints(nullptr,w,reinterpret_cast<POINT*>(&r),2);
            const int top=int((r.top-app->uiOffsetY)/app->uiScale+.5f);
            const HWND control=reinterpret_cast<HWND>(b);
            bool onCard=top>=88;
            if(app->page==Page::Overview||app->page==Page::Games)
                onCard=top>=152;

            if((control==app->menuMessage&&app->menuMessageError)||
               (control==app->shortcutHint&&app->shortcutMessageError))SetTextColor(dc,mc::ui::palette.warning);
            return reinterpret_cast<LRESULT>(onCard?app->card:app->background);
        }
        if(m==WM_MEASUREITEM){auto item=reinterpret_cast<MEASUREITEMSTRUCT*>(b);if(item->CtlType==ODT_COMBOBOX){item->itemHeight=36;return TRUE;}}
        if(m==WM_DRAWITEM) {
            auto d=reinterpret_cast<DRAWITEMSTRUCT*>(b);
            if(d->CtlID==CaptionMinimize||d->CtlID==CaptionClose){UiPaintBuffer buffer(d->hDC,d->rcItem);auto item=*d;item.hDC=buffer.dc();mc::paintCaptionButton(item,d->CtlID==CaptionClose);buffer.present();return TRUE;}
            if(d->CtlID>=ReticleFirst&&d->CtlID<ReticleFirst+mc::ReticleShapeCount){int shape=int(d->CtlID)-ReticleFirst;mc::fluent::reticleButton(*d,shape==app->preferences.reticleShape,shape,app->chinese);return TRUE;}
            if(d->CtlType==ODT_COMBOBOX){mc::fluent::comboItem(*d);return TRUE;}
            if(d->CtlID>=NavOverview&&d->CtlID<=NavGeneral){
                mc::fluent::sidebarItem(*d,int(d->CtlID-NavOverview)==int(app->page));
                return TRUE;
            }
            if(d->CtlID==PageDots||d->CtlID==PageReticle){
                const int tab=d->CtlID==PageReticle?1:0;
                mc::fluent::navigation(*d,tab==app->settingsPage);return TRUE;
            }
            if(d->CtlID>=TabRegional&&d->CtlID<=TabTriangle){mc::fluent::segment(*d,int(d->CtlID-TabRegional)==app->preferences.selected);return TRUE;}
            bool selected=(d->CtlID==DebugParameters && app->showDebug)||d->CtlID==RegisterGame||d->hwndItem==app->armedBinding||d->CtlID==MenuSave;
            mc::fluent::button(*d,selected);return TRUE;
        }
        if(m==WM_TIMER) { app->tick(); return 0; }
        if(m==WM_COMMAND) {
            int id=LOWORD(a),notification=HIWORD(a);
            bool combo=id==List||id==Edges||id==Appearance||id==GameChoice||id==VisualProfile||id==MenuSelect||id==MenuEffectChoice;
            if(id==0||id==List || (combo?notification!=CBN_SELCHANGE:notification!=BN_CLICKED))return 0;
            app->command(id);return 0;
        }
        if(m==WM_HOTKEY) {
            if(a<1||a>app->registry.shortcuts.size()||!app->hotkeys[a-1])return 0;
            const auto chord=app->registry.shortcuts[a-1];
            if(chord!=shortcutFromMessage(b))return 0;
            if(qpcSeconds()<app->suppressShortcutUntil&&chord==app->suppressedShortcut)return 0;
            if(a==1) app->cycleRunningStyle();
            if(a==2) app->toggleOverlay();
            if(a==3) app->command(Stop);
            return 0;
        }
        if(m==WM_INPUT) {
            app->observeInput(reinterpret_cast<HRAWINPUT>(b));
            return DefWindowProcW(w,m,a,b); // Required foreground Raw Input cleanup.
        }
        if(m==WM_INPUT_DEVICE_CHANGE) {app->inputs.clear();app->menu.setForeground(false,{});return 0;}
        if(m==WM_CLOSE&&app->page==Page::Games&&!app->confirmRuleNavigation())return 0;
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
#ifdef MC_HAS_OPENCV
        app.chromaAssist=wcsstr(command,L"--grayscale")==nullptr;
        app.capture.setChromaEnabled(app.chromaAssist);
#endif
        app.captureOnly=wcsstr(command,L"--capture-only")!=nullptr;
        app.overlayOnly=wcsstr(command,L"--overlay-only")!=nullptr;
        if(app.captureOnly && app.overlayOnly) throw std::runtime_error("Choose one diagnostic isolation mode");
        INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_BAR_CLASSES}; InitCommonControlsEx(&controls);
        WNDCLASSW wc{}; wc.hInstance=instance; wc.lpfnWndProc=proc; wc.lpszClassName=L"MotionCompensatorControl";
        wc.hCursor=LoadCursorW(nullptr,IDC_ARROW); wc.hbrBackground=nullptr; RegisterClassW(&wc);
        POINT cursor{};GetCursorPos(&cursor);RECT desktop=mc::monitorWorkArea(MonitorFromPoint(cursor,MONITOR_DEFAULTTOPRIMARY));
        RECT initial=mc::centeredPanelRect(desktop,GetDpiForSystem(),ui::width,ui::preferredHeight);
        HWND window=CreateWindowExW(0,wc.lpszClassName,L"MotionCompensator",mc::PanelWindowStyle,
            initial.left,initial.top,initial.right-initial.left,initial.bottom-initial.top,nullptr,nullptr,instance,&app);
        if(!window) throw std::runtime_error("Control window creation failed");
        using DwmSet=HRESULT(WINAPI*)(HWND,DWORD,LPCVOID,DWORD);
        if(auto dwm=LoadLibraryExW(L"dwmapi.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32)){auto set=loadFunction<DwmSet>(dwm,"DwmSetWindowAttribute");if(set){DWORD rounded=2;set(window,33,&rounded,sizeof(rounded));}FreeLibrary(dwm);}
        DeleteMenu(GetSystemMenu(window,FALSE),SC_MAXIMIZE,MF_BYCOMMAND);
        // The hidden window now has its actual monitor DPI; correct the initial fit before showing.
        RECT centered=mc::centeredPanelRect(desktop,GetDpiForWindow(window),ui::width,ui::preferredHeight);
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
        MSG msg{};
        while(GetMessageW(&msg,nullptr,0,0)>0) {
            if(app.captureBindingMessage(msg))continue;
            HWND focus=GetFocus();
            if(msg.message==WM_KEYDOWN && msg.wParam==VK_TAB && IsChild(window,focus)
                && !(GetKeyState(VK_CONTROL)&0x8000) && !SendMessageW(focus,CB_GETDROPPEDSTATE,0,0)) {
                if(HWND next=GetNextDlgTabItem(window,focus,(GetKeyState(VK_SHIFT)&0x8000)!=0)) {
                    SendMessageW(window,WM_CHANGEUISTATE,MAKEWPARAM(UIS_CLEAR,UISF_HIDEFOCUS),0);
                    SetFocus(next);
                    continue;
                }
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        } result=static_cast<int>(msg.wParam);
    } catch(const std::exception& e) { MessageBoxA(nullptr,e.what(),"MotionCompensator startup failed",MB_ICONERROR); }
    if(SUCCEEDED(apartment)) CoUninitialize();
    return result;
}
