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
MotionCompensator 是一款旨在缓解游戏中 3D 眩晕与晕动不适的 Windows 视觉辅助工具。它通过可调节的点阵和中央准星提供视觉参照。当前为预览版，体验因人和游戏而异，不保证缓解效果；出现不适时请停止使用。

开始使用
1. 退出其他正在运行的版本，然后启动 MotionCompensator.exe。
2. 启动游戏，优先使用无边框或窗口模式。独占全屏可能遮住浮层。
3. 勾选“自动识别前台游戏”，切回已识别的游戏即可自动连接。也可点击“刷新窗口”，从列表选择游戏，再点击“选择此窗口”。手动连接会关闭自动识别。
4. 选择点阵方案并调整外观，切回游戏查看效果。浮层仅在目标游戏位于前台时显示。
5. 顶部“中文 / English”按钮切换界面语言；主题按钮依次切换跟随系统、浅色和深色。界面主题不影响游戏中的点阵与准星颜色。

三种点阵方案
分区点阵：不同区域的点阵根据画面运动和输入预测持续滚动，可调整中央留空与区域边界淡化。
屏边点阵：在左右、上下或四周边缘显示较大的圆点，以滚动、渐隐和相邻点的大小差异呈现运动。
静态点阵：固定的4×4共16个参考点，左右外列带柔边外圈，内两列为小亮点。“排布展开”调整整体展开范围，点数保持不变。
三种方案分别保存点阵外观设置。动态方案不会自动切换为静态；可通过方案按钮或快捷键自行切换。

外观和运动设置
“点大小”“透明度”“点间距”与颜色按钮用于调整点阵外观。透明度0%为完全可见，100%为完全透明。屏边点大小范围为1–64，其他方案为1–24。
“中央留空”调整画面中央圆形留空范围；屏边方案中对应控件为“边缘带宽”，用于调整点阵所在的屏边区域。
“点阵交错”使相邻行错开排列；“边界淡化”柔化分区点阵交界处的出现和消失。
动态方案的“运动强度”调整滚动速度，“反转运动方向”切换滚动方向。这些设置可分别保存。
“背景适配”可选择固定颜色、同色相明暗、同色相双层描边或两者组合。明暗适配根据点附近的背景柔和调整亮度，双层描边采用外暗内亮的同色相轮廓。
右侧预览显示排布、颜色和准星，并提供明暗背景对照。动态滚动效果请在游戏中查看。

中央准星
点击横排图标选择关闭、十字、菱十字、圆点、圆环、留空十字、T形或菱形。蓝色按钮表示当前选择，悬停可查看名称与提示。
准星颜色、透明度和大小由下方控件调整，三种点阵方案共用并自动保存。
十字保留细小中心间隙；留空十字与T形留出更大的中心空间，T形不绘制上臂。菱十字由十字与菱形组成，菱形仅显示外轮廓。
大小表示线形或轮廓准星从中心到端点或外缘的距离，完整跨度约为该值的两倍。圆点半径约为该值的18%，最小1像素。
准星固定在屏幕中心，不跟随后坐力，也不表示实际弹着位置。

调试可视化
点击“显示调试参数”后，切回游戏查看面板；再次点击可隐藏。
蓝色箭头表示输入预测，黄色表示用于驱动的光流，紫色表示融合运动，绿色表示点阵运动。箭头长度反映速度，显示长度设有上限，不限制实际滚动速度。
面板显示运动中、疑似停止、已停止或观测不足。“延续”表示短暂使用最近可靠运动，未将其标成新观测；这些状态是画面判断，不是游戏引擎的角色或车辆状态。
每个区域的D表示九个分析格中通过直接观测检查的格数。当前版本不以邻区或背景模型向量填补缺测；原始观测与短时延续在日志中分别记录。可靠性评分不代表识别准确率。
可信度条表示画面观测的可靠程度；N/A和曲线断线表示相应数据不可用。趋势图显示最近10秒的处理频率、耗时与原始光流速度。
面板中的FPS是采集、分析或渲染提交频率，不是游戏帧率；CPU耗时也不等于屏幕最终呈现延迟。
“日志”与调试显示独立，每次启动默认关闭。需要反馈问题时勾选，复现后取消。日志保存在程序旁的logs目录，不会自动上传；分享前请检查进程信息等本地内容。
日志约10MB一份，目录上限100MB、最多20份，达到上限后清理最旧的会话日志。记录失败时开关变红并取消勾选，悬停可查看原因。

快捷键
Ctrl+Alt+F8：切换分区点阵、屏边点阵和静态点阵。
Ctrl+Alt+F9：显示或隐藏浮层。
Ctrl+Alt+F10：停止采集并关闭自动连接；再次使用时选择游戏窗口或重新勾选自动识别。

常见情况
点阵不显示：检查连接目标、“显示浮层”、透明度和游戏窗口模式，并确认目标游戏在前台。
快捷键无效：退出其他版本或占用快捷键的程序，也可使用窗口中的按钮。软件会自动重试注册快捷键。
橙色提示表示快捷键冲突或观测不足，红色提示表示采集、日志等异常。悬停相关控件可查看详情。

动态点阵如何响应
动态方案根据局部灰度光流和短时输入预测驱动点阵。不同区域保留独立方向与速度，不按车速直接驱动点阵。
采集侧持续更新最新画面，分析侧以每秒30次为目标，每次使用最新相邻的两张已回读画面。例如实际回读为60帧/秒时，两帧通常相隔约16.7毫秒。采集丢帧或回读繁忙会使间隔变长，不代表读取了游戏引擎帧率。
分析繁忙时替换过时任务，不积压或重复画面。位移按真实时间间隔换算速度；不足8毫秒或超过150毫秒的帧对跳过。检测频率与点阵绘制频率分别运行，实际分析频率可能低于30次/秒。
连续、分布充分的直接观测显示背景静止时，点阵会平滑停止，按住前进键也不会强制持续运动。缺少纹理、匹配失败或遮挡表示观测不足，不作为静止证据。
局部观测不足时，仅短暂延续最近可靠运动，超过时限后收敛。当前版本不进行额外范围匹配、上级方向借用或背景模型填补。移动按键只提供短暂起步预测，转视角输入仍参与短时预测。没有可靠画面证据时不强迫持续滚动，也不会自动切换静态方案。
鼠标滚轮不参与运动输入采集。滚轮在游戏中引起的镜头或场景变化仍可能被画面检测识别。

采集与兼容性
采集时会向Windows申请隐藏本会话的捕获边框；若出现系统授权提示，请按需选择。不支持或未获准时保留边框，仍可正常采集，不修改系统隐私设置。
纯色墙、运动模糊、切镜或武器/HUD遮挡可能影响运动估计。输入预测表示操作意图，不能准确判断跳跃、顶墙或过场等角色状态；当前没有自动识别CG并隐藏浮层的功能。
多显示器、高DPI和完整Tab导航尚未全面适配。使用前请确认目标游戏允许外部浮层。
设置自动保存到当前Windows用户配置。运行只需EXE，无需安装OpenCV、编译器或额外程序DLL。
问题反馈：https://github.com/gzz312/MotionCompensator/issues

设计参考、致谢与许可
屏边点阵的视觉设计参考了shsr07（拾荒散人）的MotionStabilizer，感谢作者分享设计思路与创意。
项目：https://github.com/shsr07/MotionStabilizer
作者视频：https://www.bilibili.com/video/BV1saKg6XEvg/
交错排布与周期连续性参考MotionCues：https://github.com/mcpeixoto/MotionCues
视觉锚点设计参考Peregrine：https://github.com/Eeymoo/peregrine
上述项目提供了设计参考；源码比对未发现直接代码复制，具体比对范围见项目仓库的设计参考记录。
本项目采用自定义源码公开与商业集成许可：允许免费使用、学习、修改和分享，允许符合许可条件的署名商业集成；禁止独立售卖、换皮售卖和单独收费解锁本项目功能。
完整条款见“许可与致谢”，以许可中文正文为准。第三方组件保留各自许可。
)":LR"(MotionCompensator User Guide

Preview status
MotionCompensator is a Windows visual aid designed to help reduce motion discomfort during gaming, using customizable dots and a central reticle as visual references. This is a preview. Experiences vary by person and game, and relief is not guaranteed. Stop if discomfort develops.

Getting started
1. Close other running versions, then launch MotionCompensator.exe.
2. Start a game, preferably in borderless or windowed mode. Exclusive fullscreen may hide the overlay.
3. Enable automatic foreground-game detection and return to a recognized game. Alternatively, refresh the window list, select the game and click Attach selected. Manual attachment disables automatic detection.
4. Choose a dot style and adjust its appearance. Return to the game to view the overlay, which is visible only when the target game is in the foreground.
5. Use 中文 / English to switch language. The theme button cycles system, light and dark themes without changing overlay colors.

Styles and settings
Regional dots: independently scrolling regions driven by observed motion and input prediction, with a clear center and optional boundary fading.
Screen-edge dots: larger dots along the left/right, top/bottom or all edges, using scrolling, fading and alternating sizes to present motion.
Static dots: a fixed 4x4 reference with sixteen dots, soft halos on the outer columns and small dots in the inner columns. Pattern spread changes the layout's extent without changing the count.
Each style saves its own dot appearance. Dynamic styles never switch to Static automatically; select a style through its button or the shortcut.
Dot size, transparency, spacing and color control appearance. Transparency is fully visible at 0% and hidden at 100%. Screen-edge size ranges from 1 to 64; other styles range from 1 to 24.
Clear center adjusts the central circular gap. In Screen-edge mode, Edge width controls the strip occupied by the dots.
Staggered dots offset alternating rows. Boundary fade softens the appearance and disappearance of dots at regional borders.
Each dynamic style has its own motion strength and direction-reversal settings.
Background adaptation offers fixed color, same-hue brightness, a two-tone outline, or both. Brightness follows the nearby background smoothly; outlines use a darker outer and lighter inner edge.
The preview shows layout, color and reticle appearance against a light/dark background. View dynamic scrolling in the game.

Center reticle
Click an icon to select Off, Cross, Diamond cross, Dot, Ring, Open cross, T-shape or Diamond. Blue marks the current selection; hover for its name and help.
The color, transparency and size controls are shared across dot styles and saved automatically.
Cross has a small center gap. Open cross and T-shape have wider gaps; T-shape omits the top arm. Diamond cross combines a cross and diamond, while Diamond shows only the outline.
Size is the center-to-tip or outer-edge distance for line/outline shapes, approximately half the full span. Dot radius is about 18% of that value, with a one-pixel minimum.
The reticle is fixed at the screen center. It does not follow recoil or indicate actual bullet impact.

Visual diagnostics
Click Show debug parameters and return to the game. Click again to hide the panel.
Blue arrows show input prediction, yellow shows flow used for driving, purple shows fused motion and green shows dot motion. Arrow length represents speed, with a display limit that does not cap actual scrolling.
The panel distinguishes Moving, Checking stop, Stopped and Observation missing. Held regions temporarily use recent reliable motion, not new measurements. These are visual estimates, not game-engine character or vehicle states.
D counts directly observed analysis cells in each region. Missing regions are not filled with neighbour or background-model vectors. Logs distinguish raw observations from bounded continuation. Reliability scores are not accuracy probabilities.
Confidence bars indicate observation reliability. N/A and gaps in graphs indicate unavailable data. Graphs show ten seconds of processing rates, CPU times and raw flow speed.
Panel FPS values are capture, analysis or render-submission rates, not the game's frame rate. CPU timings are not final display latency.
Log is independent of the panel and is off at each launch. Enable it to reproduce a problem, then disable it. Files are saved beside the EXE in logs/ and are not uploaded automatically. Review process information and other local details before sharing.
Logs are limited to approximately 10 MB per file, 100 MB total and twenty files. Oldest session logs are removed at the limit. A recording failure unchecks and colors the control red; hover for the reason.

Hotkeys
Ctrl+Alt+F8: cycle Regional, Screen-edge and Static dots.
Ctrl+Alt+F9: show or hide the overlay.
Ctrl+Alt+F10: stop capture and automatic attachment. Select a game window or re-enable auto detection to resume.

Troubleshooting and limits
No dots: check the target window, Show overlay, transparency, window mode and whether the target game is in the foreground.
Shortcut unavailable: close other versions or the app using the shortcut, or use the panel buttons. Registration is retried automatically.
Amber indicates a shortcut conflict or insufficient observations. Red indicates a capture or logging error. Hover over the relevant control for details.

How dynamic dots respond
Dynamic styles use local grayscale optical flow and brief input prediction. Regions retain independent directions and speeds; dots do not directly represent vehicle speed.
Capture continuously updates source images while analysis targets 30 updates per second, using the latest adjacent readback images. At 60 delivered readbacks per second the pair interval is usually about 16.7ms. Capture drops and readback pressure can increase that interval; this is not game-engine FPS.
Busy analysis replaces outdated pending work without queuing or repeating images. Velocity uses actual source timestamps. Pairs below 8ms or above 150ms are skipped. Rendering runs separately and actual analysis throughput may be below 30Hz.
Repeated, sufficiently distributed direct observations of a stationary background make the dots settle, even if Forward remains held. Low texture, failed matching or occlusion mean insufficient observations and do not prove stillness.
Missing observations may briefly continue recent reliable motion, then settle when that expires. Additional context matching, parent borrowing and background-model filling are disabled. Movement input only supplies a short initial prediction; camera-look prediction remains. Missing visual evidence does not force indefinite scrolling or automatically select Static.
Mouse-wheel input is ignored for motion prediction. Visual changes caused by an in-game wheel action can still be detected.

Capture and compatibility
Capture requests Windows permission to hide this session's capture border. Respond to a system prompt if one appears. If unsupported or denied, capture continues with its border. System privacy settings are not changed.
Uniform walls, blur, camera cuts and weapon/HUD occlusion can affect motion estimation. Input prediction represents intent and cannot precisely determine jumping, wall blocking or cutscene states. Automatic cutscene detection and overlay hiding are not available.
Multi-monitor/high-DPI support and full Tab navigation are incomplete. Check that the target game permits external overlays.
Settings are saved to the current Windows user profile. Only the EXE is needed to run; no separate OpenCV, compiler or application DLL installation is required.
Report problems at https://github.com/gzz312/MotionCompensator/issues

Design references and license
The screen-edge visual design draws on shsr07 (拾荒散人)'s MotionStabilizer. Thanks to the author for sharing the ideas.
Project: https://github.com/shsr07/MotionStabilizer
Author's video: https://www.bilibili.com/video/BV1saKg6XEvg/
Staggered-layout and periodic-continuity reference: https://github.com/mcpeixoto/MotionCues
Visual-anchor reference: https://github.com/Eeymoo/peregrine
These projects informed the visual design. Source comparison found no direct code copying; the repository's design-reference record documents the comparison scope.
The custom source-available and commercial integration license permits free use, study, modification and sharing, plus commercial integration with attribution subject to its conditions. Standalone sale, paid rebrands and separate paid unlocks of project-based functionality are prohibited.
See Licenses & credits for the complete terms; the Chinese license text governs. Third-party components retain their own licenses.
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
                for(const auto* title:{L"MotionCompensator 使用说明",L"MotionCompensator User Guide",L"开始使用",L"三种点阵方案",L"外观和运动设置",L"中央准星",L"Center reticle",L"调试可视化",L"快捷键",L"常见情况",L"版本与使用范围",L"设计参考、致谢与许可",L"Preview status",L"Design references and license",L"Getting started",L"Styles and settings",L"Visual diagnostics",L"Hotkeys",L"Troubleshooting and limits"}){
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
