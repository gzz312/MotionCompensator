#pragma once
#include "common.hpp"
#include "embedded.hpp"
#include <richedit.h>
#include <commdlg.h>
#include <algorithm>
#include "fluent.hpp"
#include "tooltips.hpp"
#include "document_scroll.hpp"
#include "window_frame.hpp"
#include <memory>
namespace mc {
inline const wchar_t* usageText(bool chinese) {
    return chinese?LR"(MotionCompensator 使用说明

版本与使用范围
预览测试版，采用自定义源码公开与商业集成许可。本工具用于个人体验对照，不保证缓解效果。
已知限制：独占全屏可能遮住浮层；复杂光照、手持武器遮挡、切镜和运动模糊会影响观测；键鼠预测不代表角色真实状态；多屏与高DPI尚未完成全面验证。

开始使用
1. 先退出其他版本，再启动本程序，避免浮层重复或快捷键冲突。
2. 启动游戏。勾选“自动识别前台游戏”后，切回已支持的游戏即可连接。
3. 也可点击“刷新窗口”，在列表中选择游戏，再点击“选择此窗口”。手动选择后会关闭自动识别。
4. 切回游戏查看效果；切出游戏时浮层会隐藏。建议先用无边框或窗口模式测试，独占全屏可能遮住外部浮层。

三种点阵方案
分区点阵：九宫格分别持续滚动，适合比较局部画面运动。
屏边点阵：仅在左右、上下或四周边缘显示；宽范围渐显渐隐，邻点大小固定交替；主要靠透明度消失，只在很淡的尾段轻微缩小。
屏边点大小范围为1–64，内置初始预设为30，已有用户设置优先保留；其他两种方案范围保持1–24。
屏边“点大小”作为参考大小，大点约为其1.2倍，小点再乘0.72；只在已经很淡的尾段才轻微缩小，最多缩小15%。
反向转视角只改变滚动方向，不重新分配大小或把点移到另一侧。
静态点阵：固定4×4共16点，左右外列带柔边外圈，内两列只保留小亮点；只通过手动选择进入。
静态方案的“排布展开”调整16点的整体展开范围，不增加或减少点数。
三种方案分别保存点阵大小、透明度、间距、颜色等设置，中央准星共用。

外观和运动设置
顶部主题图标：点击依次切换跟随系统、浅色、深色；主题选择自动保存，只影响设置软件，不改变游戏点阵与准星颜色。
“背景适配”：固定颜色、同色相明暗、同色相双层描边，或明暗与描边组合；每个方案独立保存。
明暗模式保留所选色相，根据点附近的灰度背景柔和调整亮度；约10Hz更新，带滞回和平滑限速。
双层描边为外暗内亮，不改变滚动与消失透明度。预览使用明暗渐变背景，方便比较。
静态明暗适配模式采集背景亮度，但不计算光流；采集尚未建立或停止时采用中性亮度；画面没有变化、不产生新帧时保留最近背景亮度。
“运动强度”和“反转运动方向”已移入各动态方案设置区域，静态方案不显示。
“点阵交错”：相邻行错开半个点距，不改变滚动速度；各方案分别保存。
“边界淡化”：分区交界处轻微淡入淡出，不改变运动范围。
“中央留空”：调整中央不显示点阵的圆形区域，避开游戏原有准星。
屏边方案的同一滑块改为“边缘带宽”。
“透明度”：0% 最明显，100% 完全透明；准星透明度单独调整。
有移动/转视角输入时优先采用覆盖较广的背景运动，输入停止后平滑回到局部光流。
全局背景是同一批灰度观测的运动模型，不是游戏引擎数据；精细采样一直保留。
模型不可靠时继续使用局部观测或输入预测，不强行让所有区域移动。
“运动强度”：调整动态点阵速度；“反转运动方向”改变方向，可用于个人对照。
中央准星可关闭，或选择十字、菱形十字，调整颜色、透明度和大小。
准星大小表示中心到端点的长度，完整跨度约为该数值的两倍。
右侧预览用于查看排布、颜色与准星，不模拟游戏中的动态运动。

调试可视化
点击右下角“显示调试参数”，再切回游戏查看左上角面板；再次点击统一隐藏。
蓝箭头：输入预测；黄箭头：实际用于驱动的光流；紫箭头：融合运动；绿箭头：点阵运动。
箭头越长表示速度越大，显示标尺在 600 px/s 封顶，不限制实际点阵速度。
可信度色条表示画面观测可靠程度；灰短线和 N/A 表示数据不可用。
趋势图显示最近 10 秒的采集、分析、提交频率，CPU 光流、融合、绘制耗时，以及原始光流速度。
“原始光流速度”可包含未被补偿采用的低可信度观测；没有新鲜数据时断线，零速度才显示零。
面板上方会区分暂无结果、结果过期、可信度不足和光流可用于补偿。
这里的 FPS 不是游戏实际帧率；CPU 耗时不是 GPU 完成时间或屏幕呈现延迟。
“日志”位于调试按钮旁，和调试可视化独立。每次启动默认关闭，不记忆开启状态。
勾选后才在程序旁logs目录记录，取消后立即停止并保留已有文件；不会上传。
单文件约10MB，当前日志目录总量100MB、最多20份，超限时清理最旧的session日志。
记录失败时开关变红并取消勾选，悬停可查看原因。

快捷键
Ctrl+Alt+F8：依次切换分区点阵、屏边点阵、静态点阵。
Ctrl+Alt+F9：显示或隐藏浮层。
Ctrl+Alt+F10：停止采集并关闭自动连接；需要时重新选择窗口或勾选自动识别。

常见情况
点阵不显示：确认目标是游戏窗口、已开启“显示浮层”、透明度不是 100%，并切回目标游戏。
快捷键无效：先退出其他版本，或直接使用窗口中的按钮。
纯色墙、强烈运动模糊、切镜头等情况下光流可能无效，不能把采样点数当成有效观测数。
0.12 使用 960×540 灰度画面和 9×9 分析网格，最终点阵显示仍为九宫格；尚未接入 RGB 或 AI。
键鼠输入是动作意图的预测，不等于角色真实速度；跳跃、顶墙和过场的精确状态识别仍待完善。
如果感觉辅助画面干扰，可降低运动强度、提高透明度，或手动选择静态方案作对照。
设置自动保存到 Windows 当前用户配置（HKCU\\Software\\MotionCompensator），程序旁不再生成 settings.ini。
运行只需EXE。只有主动勾选日志时才创建logs目录。依赖、初始配置、说明与第三方许可均内置。
橙色表示快捷键冲突或暂时观测不足，红色表示采集、日志等异常；悬停相关控件查看详情。
每个控件都可悬停查看用途；问题恢复后提示和颜色自动恢复。快捷键占用时窗口按钮仍可使用，程序会自动重试注册。
本工具不修改游戏，不使用 Hook、DLL 注入或游戏进程内存读取；主观缓解效果需自行比较。

设计参考、致谢与许可
屏边点阵设计思路与创意参考 shsr07（拾荒散人）的 MotionStabilizer，感谢作者的分享与交流。
项目：https://github.com/shsr07/MotionStabilizer
作者视频：https://www.bilibili.com/video/BV1saKg6XEvg/
交错与周期连续性思路参考 MotionCues：https://github.com/mcpeixoto/MotionCues
视觉锚点产品参考 Peregrine：https://github.com/Eeymoo/peregrine
经当前源码对比，未发现直接代码复制；检查有范围限制。致谢不代表原作者参与开发或为效果背书。
允许免费使用、学习、修改和分享；允许商业集成，但需注明 MotionCompensator、作者 gzz312、可用来源地址，并保留完整许可。
禁止独立售卖、换皮售卖、付费插件及单独收费解锁本项目功能；简单包装或捆绑不构成商业集成。
本项目采用自定义源码公开许可，不是标准开源许可；第三方组件按各自许可使用。完整规则见“许可与致谢”。
)":LR"(MotionCompensator User Guide

Preview status
Preview under a custom source-available and commercial integration license. Comfort effects vary and are not guaranteed.
Exclusive fullscreen can hide overlays. Occlusion, cuts and blur can reduce observation quality. Input prediction is not the character's actual state. Multi-monitor and high-DPI testing is incomplete.

Getting started
1. Exit other versions before starting this one to avoid duplicate overlays and hotkey conflicts.
2. Enable automatic foreground-game detection, then return to a supported game.
3. Alternatively, refresh the window list, select the game and click Attach selected. Manual attachment disables automatic detection.
4. The overlay appears over the selected foreground game and hides when you switch away. Try borderless or windowed mode first; exclusive fullscreen may cover external overlays.

Styles and settings
Theme icon: click to cycle system (sun/moon), light (sun), dark (moon). Saved independently of game overlay colors.
Background adaptation: fixed color, adaptive same-hue brightness, two-tone outline, or both. Saved per style.
Brightness updates at about 10Hz with hysteresis, smoothing and a rate limit. Alpha fading remains independent.
Static adaptive-brightness mode captures background brightness without running optical flow. Unchanged frames retain the last brightness sample; unavailable capture uses neutral brightness.
Motion strength and reverse direction live in each dynamic style panel.
Screen-edge reference size: 1-64; bundled initial preset: 30. Existing user values are preserved. Other styles retain 1-24.
Regional dots: nine independently scrolling display regions.
Screen-edge dots: left/right, top/bottom or all edges, with broad fades and stable alternating neighbour sizes. Most travel keeps a constant size; only the already faint tail shrinks by up to 15%. Reversing input retraces the same positions and does not swap the size distribution.
Static dots: fixed 4x4 (16 dots), with soft halos only on the two outer columns. Spacing adjusts spread without changing count; entered only by manually selecting it.
Each style saves its own dot settings. The center reticle is shared.
Staggered dots offset alternating rows by half a spacing without changing scrolling speed.
Boundary fade slightly softens regional borders without reducing movement range.
Clear center reserves a circular area around the game crosshair. In edge mode this slider controls strip width.
Transparency: 0% fully visible; 100% hidden. Reticle transparency is separate.
Movement/look input smoothly prioritizes a broad background-motion model. After input stops, local flow gradually regains control. Fine observations remain active throughout; no game-engine data is read. Unreliable background models are not forced.
Motion strength changes scrolling speed. Reverse direction provides a direction comparison.
Choose an off/cross/diamond reticle, color, transparency and size. Size is the center-to-tip distance, about half the full span.
The settings preview shows layout and appearance, not simulated dynamic gameplay.

Visual diagnostics
Click Show debug parameters, then return to the game. Click again to hide everything.
Blue arrows: input prediction. Yellow: flow selected for driving. Purple: fused motion. Green: dot motion.
Arrow length represents speed with a visual cap of 600 px/s; this does not cap actual scrolling.
Confidence bars indicate observation reliability. Grey dashes and N/A mean unavailable data.
Graphs show ten seconds of capture/analysis/submit rates, CPU costs and raw flow speed.
Raw speed may include low-confidence observations; missing or expired observations leave gaps. State labels distinguish no result, expired, low confidence and ready for fusion.
These rates are not the game's FPS. CPU timings are not GPU completion or display latency.
Log is independent of debug visuals and is off at every launch. Check it to record; uncheck it to stop and retain existing files. Nothing is uploaded.
Logs are limited to about 10 MB per file, 100 MB total and 20 files in the current log directory; oldest session logs are removed. A recording error unchecks and colors the control red; hover for details.

Hotkeys
Ctrl+Alt+F8: cycle Regional, Screen-edge and Static styles.
Ctrl+Alt+F9: show/hide overlay.
Ctrl+Alt+F10: stop capture and automatic attachment. Reattach or enable auto detection to resume.

Troubleshooting and limits
If no dots appear, check the selected game, Show overlay, transparency, focus and fullscreen mode.
If hotkeys fail, exit other versions or use panel controls. Shortcut registration is retried automatically.
Amber indicates a shortcut conflict or insufficient observations; red indicates capture or recording errors. Hover over controls for normal explanations or current error details. Recovery restores normal colors.
Uniform walls, blur and camera cuts can invalidate optical flow; sampling positions are not guaranteed valid tracks.
Version 0.12 analyzes 960x540 grayscale frames in 9x9 cells and outputs the original nine display regions. RGB and AI are not connected.
Input prediction represents intent rather than real character velocity. Exact jumping, wall blocking and cutscene states are still being improved.
Settings are saved to the current Windows user profile (HKCU/Software/MotionCompensator). No settings.ini is created beside the EXE. Dependencies, initial settings, guide and third-party notices are embedded; the logs directory is created only when recording is explicitly enabled.
This tool does not modify the game or use hooks, DLL injection or game process memory reading. Subjective comfort must be assessed separately.

Design references and license
Screen-edge visual design ideas were inspired by shsr07 (拾荒散人)'s MotionStabilizer. Thanks for the author's sharing and discussion.
https://github.com/shsr07/MotionStabilizer
Author video: https://www.bilibili.com/video/BV1saKg6XEvg/
Staggered/periodic layout ideas: https://github.com/mcpeixoto/MotionCues
Visual-anchor product reference: https://github.com/Eeymoo/peregrine
The current source comparison found no direct code copying; this is a bounded review. References do not imply author participation or endorsement.
Free use, study, modification and sharing are permitted. Commercial integration into a larger work is permitted with credit to MotionCompensator and gzz312, an available source address, and the full license.
Standalone sale, paid rebrands/plugins and separate paid unlocks of project-based functionality are prohibited. Simple wrappers or bundles do not qualify as integration.
This is a custom source-available license, not a standard open-source license. Third-party components retain their own licenses. See Licenses & credits for the full Chinese governing terms and English summary.
)";
}
// Normalize both embedded LF and Windows CRLF without producing CR-CR-LF.
inline std::wstring usageLines(const std::wstring& text){
    std::wstring out;out.reserve(text.size()+128);
    for(size_t i=0;i<text.size();++i){
        if(text[i]==L'\r'){if(i+1<text.size()&&text[i+1]==L'\n')continue;out+=L"\r\n";}
        else if(text[i]==L'\n')out+=L"\r\n";else out+=text[i];
    }
    return out;
}
class UsageWindow {
    inline static HWND window_{},body_{},guide_{},licenses_{},heading_{},subtitle_{},scroll_{},viewport_{};
    inline static HFONT font_{},headingFont_{};
    inline static float scale_=1;
    inline static bool liveSizing_=false;inline static SIZE layoutSize_{};
    inline static HBRUSH background_=CreateSolidBrush(fluent::canvas);
    inline static std::unique_ptr<ControlTips> tips_;
    inline static bool chinese_=true,rich_=false,notices_=false;
    static void arrange(HWND window){
        if(IsIconic(window)||!body_)return;
        RECT r{};GetClientRect(window,&r);if(r.right<=0||r.bottom<=0||(layoutSize_.cx==r.right&&layoutSize_.cy==r.bottom))return;
        layoutSize_={r.right,r.bottom};auto px=[](float value){return int(value*scale_);};
        SendMessageW(body_,WM_SETREDRAW,FALSE,0);
        UiLayoutBatch batch(window,6);
        batch.place(heading_,px(24),px(22),r.right-px(48),px(40));
        batch.place(subtitle_,px(24),px(66),r.right-px(48),px(28));
        batch.place(guide_,px(24),px(110),px(160),px(38));
        batch.place(licenses_,px(196),px(110),px(230),px(38));
        int width=GetSystemMetricsForDpi(SM_CXVSCROLL,GetDpiForWindow(window));int documentWidth=std::max(1L,r.right-px(48)),height=std::max(1L,r.bottom-px(194));
        batch.place(viewport_,px(24),px(170),std::max(1,documentWidth-width),height);
        if(scroll_)batch.place(scroll_,r.right-px(24)-width,px(170),width,height);
        batch.commit();SetWindowPos(body_,nullptr,0,0,documentWidth,height,SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOREDRAW|SWP_NOCOPYBITS);SendMessageW(body_,WM_SETREDRAW,TRUE,0);
        RedrawWindow(window,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN);
    }
    static void loadText(bool notices){
        notices_=notices;
        auto content=usageLines(notices?embeddedProjectNotices()+L"\r\n\r\n"+embeddedProjectLicense()+L"\r\n\r\n"+embeddedLicenseText():std::wstring(usageText(chinese_)));
        if(!notices){auto header=content.find(L"\r\n\r\n");if(header!=std::wstring::npos)content.erase(0,header+4);}
        if(content.empty())content=chinese_?L"未能加载内容，请重新打开说明窗口。":L"Content unavailable. Please reopen this window.";
        // Set the capacity BEFORE loading; never pass a large document as the
        // creation-time caption of a 1x1 standard EDIT control.
        SendMessageW(body_,WM_SETREDRAW,FALSE,0);
        SendMessageW(body_,rich_?EM_EXLIMITTEXT:EM_SETLIMITTEXT,rich_?0:1024*1024,rich_?1024*1024:0);
        if(!SetWindowTextW(body_,content.c_str()))SetWindowTextW(body_,chinese_?L"内容加载失败，请重新打开窗口。":L"Could not load text. Please reopen the window.");
        if(rich_){
            SendMessageW(body_,EM_SETSEL,0,-1);
            CHARFORMAT2W format{};format.cbSize=sizeof(format);format.dwMask=CFM_COLOR|CFM_SIZE|CFM_BOLD;format.crTextColor=fluent::ink;format.yHeight=280;SendMessageW(body_,EM_SETCHARFORMAT,SCF_ALL,reinterpret_cast<LPARAM>(&format));
            PARAFORMAT2 paragraph{};paragraph.cbSize=sizeof(paragraph);paragraph.dwMask=PFM_SPACEAFTER;paragraph.dySpaceAfter=100;SendMessageW(body_,EM_SETPARAFORMAT,0,reinterpret_cast<LPARAM>(&paragraph));
            if(!notices){
                for(const auto* title:{L"MotionCompensator 使用说明",L"MotionCompensator User Guide",L"开始使用",L"三种点阵方案",L"外观和运动设置",L"调试可视化",L"快捷键",L"常见情况",L"版本与使用范围",L"设计参考、致谢与许可",L"Preview status",L"Design references and license",L"Getting started",L"Styles and settings",L"Visual diagnostics",L"Hotkeys",L"Troubleshooting and limits"}){
                    FINDTEXTEXW find{};find.chrg={0,-1};find.lpstrText=const_cast<wchar_t*>(title);
                    while(SendMessageW(body_,EM_FINDTEXTEXW,FR_DOWN,reinterpret_cast<LPARAM>(&find))>=0){
                        wchar_t before[2]{},after[2]{};TEXTRANGEW range{{std::max(0L,find.chrgText.cpMin-1),find.chrgText.cpMin},before};SendMessageW(body_,EM_GETTEXTRANGE,0,reinterpret_cast<LPARAM>(&range));range={{find.chrgText.cpMax,find.chrgText.cpMax+1},after};SendMessageW(body_,EM_GETTEXTRANGE,0,reinterpret_cast<LPARAM>(&range));
                        if((find.chrgText.cpMin==0||before[0]==L'\r'||before[0]==L'\n')&&(after[0]==0||after[0]==L'\r'||after[0]==L'\n')){
                            SendMessageW(body_,EM_SETSEL,find.chrgText.cpMin,find.chrgText.cpMax);format.dwEffects=CFE_BOLD;format.yHeight=340;format.crTextColor=fluent::accent;SendMessageW(body_,EM_SETCHARFORMAT,SCF_SELECTION,reinterpret_cast<LPARAM>(&format));break;
                        }
                        find.chrg.cpMin=find.chrgText.cpMax;
                    }
                }
            }
        }
        SendMessageW(body_,EM_SETSEL,0,0);SendMessageW(body_,EM_SCROLLCARET,0,0);
        InvalidateRect(guide_,nullptr,FALSE);InvalidateRect(licenses_,nullptr,FALSE);if(scroll_)InvalidateRect(scroll_,nullptr,FALSE);
        SendMessageW(body_,WM_SETREDRAW,TRUE,0);InvalidateRect(body_,nullptr,FALSE);
    }
    static LRESULT CALLBACK proc(HWND w,UINT m,WPARAM a,LPARAM b){
        if(m==WM_SYSCOMMAND && (a&0xfff0)==SC_MAXIMIZE)return 0;
        if(m==WM_NCLBUTTONDBLCLK && a==HTCAPTION)return 0;
        if(m==WM_ENTERSIZEMOVE){liveSizing_=true;return 0;}
        if(m==WM_EXITSIZEMOVE){liveSizing_=false;arrange(w);RedrawWindow(w,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN|RDW_UPDATENOW);return 0;}
        if(m==WM_SIZE){if(a==SIZE_MINIMIZED)return 0;arrange(w);InvalidateRect(w,nullptr,FALSE);return 0;}
        if(m==WM_GETMINMAXINFO){auto limits=reinterpret_cast<MINMAXINFO*>(b);limits->ptMinTrackSize={460,320};return 0;}
        if(m==WM_DRAWITEM){auto item=reinterpret_cast<DRAWITEMSTRUCT*>(b);fluent::button(*item,(item->CtlID==3)==notices_);return TRUE;}
        if(m==WM_ERASEBKGND)return 1;
        if(m==WM_PAINT){PAINTSTRUCT ps{};HDC dc=BeginPaint(w,&ps);RECT rect{};GetClientRect(w,&rect);FillRect(dc,&rect,background_);EndPaint(w,&ps);return 0;}
        if(m==WM_COMMAND && LOWORD(a)==1){if(scroll_)InvalidateRect(scroll_,nullptr,FALSE);return 0;}
        if(m==WM_COMMAND && HIWORD(a)==BN_CLICKED){if(LOWORD(a)==2)loadText(false);if(LOWORD(a)==3)loadText(true);return 0;}
        if(m==WM_CTLCOLOREDIT || m==WM_CTLCOLORSTATIC || m==WM_CTLCOLORBTN){auto dc=reinterpret_cast<HDC>(a);SetTextColor(dc,fluent::ink);SetBkColor(dc,fluent::canvas);return reinterpret_cast<LRESULT>(background_);}
        if(m==WM_CLOSE){DestroyWindow(w);return 0;}
        if(m==WM_NCDESTROY){tips_.reset();layoutSize_={};liveSizing_=false;if(font_)DeleteObject(font_);if(headingFont_)DeleteObject(headingFont_);font_=headingFont_=nullptr;window_=body_=guide_=licenses_=heading_=subtitle_=scroll_=viewport_=nullptr;}
        return DefWindowProcW(w,m,a,b);
    }
public:
    static void applyTheme(){
        if(!window_||!IsWindow(window_))return;
        bool visible=IsWindowVisible(window_);if(visible)SendMessageW(window_,WM_SETREDRAW,FALSE,0);
        DeleteObject(background_);background_=CreateSolidBrush(fluent::canvas);ui::setNativeTheme(window_);
        POINT scroll{};CHARRANGE selection{};if(rich_){SendMessageW(body_,EM_GETSCROLLPOS,0,reinterpret_cast<LPARAM>(&scroll));SendMessageW(body_,EM_EXGETSEL,0,reinterpret_cast<LPARAM>(&selection));SendMessageW(body_,EM_SETBKGNDCOLOR,0,fluent::surface);}
        loadText(notices_);if(rich_){SendMessageW(body_,EM_EXSETSEL,0,reinterpret_cast<LPARAM>(&selection));SendMessageW(body_,EM_SETSCROLLPOS,0,reinterpret_cast<LPARAM>(&scroll));}
        if(tips_)tips_->applyTheme();
        if(visible)SendMessageW(window_,WM_SETREDRAW,TRUE,0);
        RedrawWindow(window_,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN|RDW_FRAME|RDW_UPDATENOW);
    }
    static HWND show(HWND owner,bool chinese){
        chinese_=chinese;
        if(window_ && IsWindow(window_)){
            SetWindowTextW(window_,chinese?L"使用说明":L"User Guide");SetWindowTextW(guide_,chinese?L"使用说明":L"User Guide");SetWindowTextW(licenses_,chinese?L"许可与致谢":L"Licenses & credits");SetWindowTextW(heading_,chinese?L"使用说明":L"User Guide");SetWindowTextW(subtitle_,chinese?L"从连接游戏开始，找到适合自己的视觉提示。":L"Connect your game and make the visual cues your own.");loadText(false);ShowWindow(window_,SW_RESTORE);SetForegroundWindow(window_);return window_;
        }
        auto instance=reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(owner,GWLP_HINSTANCE));
        WNDCLASSW wc{};wc.hInstance=instance;wc.lpfnWndProc=proc;wc.lpszClassName=L"MotionCompensatorUsage";wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hbrBackground=nullptr;RegisterClassW(&wc);
        RECT desktop=monitorWorkArea(MonitorFromWindow(owner,MONITOR_DEFAULTTONEAREST));RECT anchor{};GetWindowRect(owner,&anchor);
        scale_=GetDpiForWindow(owner)/96.f;int width=std::min(LONG(900*scale_),desktop.right-desktop.left-32),height=std::min(LONG(900*scale_),desktop.bottom-desktop.top-32);
        DeleteObject(background_);background_=CreateSolidBrush(fluent::canvas);
        window_=CreateWindowExW(0,wc.lpszClassName,chinese?L"使用说明":L"User Guide",(WS_OVERLAPPEDWINDOW&~(WS_MAXIMIZEBOX|WS_MINIMIZEBOX))|WS_CLIPCHILDREN,std::clamp(anchor.left+40,desktop.left,std::max(desktop.left,desktop.right-width)),std::clamp(anchor.top+40,desktop.top,std::max(desktop.top,desktop.bottom-height)),width,height,owner,nullptr,instance,nullptr);
        if(!window_)return nullptr;
        // Windows ships Rich Edit. Keep the module loaded while its windows exist.
        static HMODULE richModule=LoadLibraryExW(L"Msftedit.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
        rich_=richModule!=nullptr;
        ui::setNativeTheme(window_);
        guide_=CreateWindowW(L"BUTTON",chinese?L"使用说明":L"User Guide",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,16,12,150,34,window_,reinterpret_cast<HMENU>(2),instance,nullptr);
        licenses_=CreateWindowW(L"BUTTON",chinese?L"许可与致谢":L"Licenses & credits",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,178,12,210,34,window_,reinterpret_cast<HMENU>(3),instance,nullptr);
        viewport_=DocumentViewport::create(window_,instance);
        body_=CreateWindowExW(0,rich_?MSFTEDIT_CLASS:L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS|WS_VSCROLL|WS_TABSTOP|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL,16,58,width-48,height-116,viewport_,reinterpret_cast<HMENU>(1),instance,nullptr);
        if(!body_){DestroyWindow(window_);MessageBoxW(owner,chinese?L"说明文本控件未能创建。":L"Could not create the guide text control.",L"MotionCompensator",MB_ICONERROR);return nullptr;}
        if(rich_){scroll_=DocumentScroll::create(window_,body_,instance);auto events=SendMessageW(body_,EM_GETEVENTMASK,0,0);SendMessageW(body_,EM_SETEVENTMASK,0,events|ENM_SCROLL|ENM_UPDATE);}
        heading_=CreateWindowW(L"STATIC",chinese?L"使用说明":L"User Guide",WS_CHILD|WS_VISIBLE,24,22,600,40,window_,nullptr,instance,nullptr);
        subtitle_=CreateWindowW(L"STATIC",chinese?L"从连接游戏开始，找到适合自己的视觉提示。":L"Connect your game and make the visual cues your own.",WS_CHILD|WS_VISIBLE,24,66,800,28,window_,nullptr,instance,nullptr);
        fluent::install(heading_,fluent::Kind::Label);fluent::install(subtitle_,fluent::Kind::Label);
        fluent::install(guide_,fluent::Kind::Button);fluent::install(licenses_,fluent::Kind::Button);SetWindowTheme(body_,L"",L"");
        headingFont_=CreateFontW(-int(28*scale_),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI Variable Text");SendMessageW(heading_,WM_SETFONT,reinterpret_cast<WPARAM>(headingFont_),FALSE);
        font_=CreateFontW(-int(18*scale_),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        for(auto control:{body_,guide_,licenses_,subtitle_})SendMessageW(control,WM_SETFONT,reinterpret_cast<WPARAM>(font_),FALSE);
        if(rich_){SendMessageW(body_,EM_SETBKGNDCOLOR,0,fluent::surface);CHARFORMAT2W format{};format.cbSize=sizeof(format);format.dwMask=CFM_COLOR;format.crTextColor=fluent::ink;SendMessageW(body_,EM_SETCHARFORMAT,SCF_ALL,reinterpret_cast<LPARAM>(&format));}
        SendMessageW(body_,EM_SETMARGINS,EC_LEFTMARGIN|EC_RIGHTMARGIN,MAKELPARAM(int(22*scale_),int(22*scale_)));
        using DwmSet=HRESULT(WINAPI*)(HWND,DWORD,LPCVOID,DWORD);
        if(auto dwm=LoadLibraryExW(L"dwmapi.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32)){auto set=loadFunction<DwmSet>(dwm,"DwmSetWindowAttribute");if(set){DWORD radius=2;set(window_,33,&radius,sizeof(radius));}FreeLibrary(dwm);}
        DeleteObject(background_);background_=CreateSolidBrush(fluent::canvas);ui::setNativeTheme(window_);
        tips_=std::make_unique<ControlTips>();tips_->initialize(window_);tips_->set(guide_,chinese?L"查看软件连接、点阵外观与快捷键说明。":L"Read game connection, appearance and shortcut instructions.");tips_->set(licenses_,chinese?L"查看项目完整使用许可、设计参考、致谢和第三方许可。":L"Read the full project license, design references, credits and dependency licenses.");tips_->set(scroll_,chinese?L"拖动滑条或使用滚轮浏览说明。":L"Drag or scroll to browse the document.");tips_->set(body_,chinese?L"滚轮滚动说明；可选中文字复制。":L"Scroll to read. Select text to copy.");
        arrange(window_);loadText(false);ShowWindow(window_,SW_SHOW);SetFocus(body_);return window_;
    }
};
}
