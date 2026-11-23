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

关于 MotionCompensator
MotionCompensator 是一款 Windows 游戏视觉辅助工具，通过可调节的点阵和中央准星提供视觉参照，帮助缓解晕 3D 和游戏中的晕动不适。建议从较淡、较少的点开始，按自己的感受调整。

开始使用
1. 启动 MotionCompensator.exe，然后打开游戏。建议在游戏里选择无边框或窗口模式，方便显示浮层。
2. 在“总览”点击“刷新窗口”，选中游戏，再点“登记并连接”。开启“自动连接已登记游戏”，下次打开游戏并切到前台时，软件会重新连接。
3. 只想连接这一次，可选择“临时连接”。此操作会关闭自动连接。
4. 打开“视觉效果”，在外观预览下方选择要编辑的配置，再调整点阵或准星。切回目标游戏即可查看浮层。
5. 在“通用设置”使用“中文 / English”切换界面语言。主题按钮可在跟随系统、浅色和深色之间切换。游戏内的颜色在“视觉效果”中设置。

选择点阵
在“视觉效果 → 点阵”选择方案。可以在同一个游戏场景试着直行、转向和停下，比较哪种看起来最舒服。每种方案分别保存外观设置。

静态点阵适合想要固定视觉参照的玩家。屏幕上有 4×4 共 16 个圆点，外侧两列带柔边。“排布展开”用于调节圆点覆盖的范围。
屏边点阵适合想留出画面中央的玩家。圆点沿左右、上下或四周边缘滚动，大小交错并渐隐。“边缘带宽”控制点阵向画面内延伸的距离。
分区点阵适合想让画面各处分别响应运动的玩家。九个区域各自根据当地画面和输入滚动。觉得交界太明显时，可以开启“边界淡化”。
网格点阵适合想观察跨区域连续运动的玩家。圆点按所在位置移动，网格根据局部间距补充或回收节点。可开启“纵深效果”，用颜色和点径区分画面扩张与收缩。扩张时靠中心的点小而偏蓝，外围大而偏红，中间是常规点径的灰色圆点。收缩时中央与外围的颜色和大小对调。新点沿用所在位置的外观并淡入，停下后点径和颜色逐渐恢复。

调整外观与运动
“点大小”“透明度”“点间距”和颜色按钮用于调整外观。透明度 0% 为完全可见，100% 为完全透明。“中央留空”用于为准星或画面主体留出空间。“点阵交错”可让分区和屏边点阵的相邻行错开。
在“背景适配”中，选“固定颜色”使用选定的点阵色，选“同色相明暗”根据背景调整亮度，选“同色相双层描边”给圆点加上外暗内亮的边缘。想同时获得亮度变化和轮廓对比时，选“明暗 + 双层描边”。
动态方案可以调整“运动强度”，也可用“反转运动方向”切换圆点的滚动方向。网格点阵的“纵深强度”调节颜色和点径的变化幅度。“显示网格连线”用于在游戏里观察节点之间的连接。
设置区可用滚轮或滚动条浏览。左侧外观预览提供明暗背景对照，切回游戏可查看滚动和纵深效果。

中央准星
准星为屏幕中心提供固定参照。点击横排图标可选择十字、菱十字、圆点、圆环、留空十字、T 形、菱形或关闭。鼠标悬停可查看名称，蓝色表示当前选择。
留空十字与 T 形适合想给画面中心多留一点空间时使用。圆环和菱形提供轮廓参照。下方可以调整颜色、透明度和大小，设置由各点阵方案共用。
线形和轮廓准星的尺寸按中心到外缘的距离设置，完整跨度约为该值的两倍。圆点半径约为尺寸值的 18%，最小为 1 像素。

游戏配置
在外观预览下方选择“全局默认”，调整各游戏共用的起始设置。也可以选择某款已登记的游戏，为它单独保存改动。“恢复全局视觉设置”可让所选游戏重新沿用全局默认值。
连接已登记的游戏时，程序会选择对应配置。正在编辑的菜单规则可先保存或放弃，再切换游戏按键页。游戏安装位置变更后请重新登记，同名游戏可查看完整路径来区分。

游戏按键与菜单
在“游戏按键配置”选中已登记的游戏，在“移动与动作”中录入游戏内使用的方向、跳跃、奔跑、蹲伏和趴下键。这些绑定用于短时运动预测。“趴下”默认留空，点击“清空”可解除单项绑定，“恢复默认按键”可还原整组动作键。
在“菜单与背包”中设置打开菜单后的浮层行为，可选隐藏浮层、暂停动态滚动或临时显示静态点阵。输入名称和返回次数，点击开启键或返回键栏并按下要录入的按键。再次录入可添加候选键。修改后点击“保存规则”。
同一游戏的每条规则请使用不同的开启键。按住返回键计为一次。打开另一个已配置的菜单时，按新规则重新计数。
规则在目标游戏位于前台时生效。若通过鼠标关闭菜单后点阵还未恢复，按“显示或隐藏浮层”快捷键或在总览点击“显示浮层”，即可结束当前规则。

快捷键
默认 Ctrl+Alt+F8 切换点阵方案，Ctrl+Alt+F9 显示或隐藏浮层，Ctrl+Alt+F10 停止采集并关闭自动连接。菜单规则生效时，Ctrl+Alt+F9 会结束规则并显示浮层。
在“通用设置”点击按键栏后按下新组合即可修改。用“清空”解除单项绑定，或用“恢复默认按键”还原。总览显示当前快捷键，也可直接操作点阵开关、暂停补偿和停止采集。

运行诊断与日志
“运行诊断”显示游戏连接、采集和运动识别情况。选择网格点阵时，可以查看可见点数、网格面数、补点与回收计数，以及网格更新、拓扑和浮层提交耗时。切回软件可查看最近一次记录。
要查看游戏内的方向箭头和可信度，点击“显示调试参数”并切回游戏。蓝色表示输入预测，黄色表示用于驱动的光流，紫色表示融合运动，绿色表示点阵运动。箭头越长，估计的速度越高。D 显示每个区域中有直接观测的分析格数。
可信度条反映画面观测的可靠程度。“延续”表示当前在短时间内沿用最近的可靠运动。N/A 或曲线断线提示对应数据暂缺。FPS 按面板标注分别对应采集、分析和渲染提交频率。CPU 耗时用于查看处理步骤的开销。
需要反馈问题时，在“运行诊断”勾选“日志”，复现后取消勾选。文件保存在程序旁的 logs 文件夹。分享前可先检查内容。单份约 10 MB，目录最多保留 20 份或 100 MB，达到上限时清理最旧的日志。

常见情况
• 浮层未出现时，检查是否选中了正确的游戏窗口，“显示浮层”是否开启，以及透明度是否合适。切回游戏并优先使用无边框或窗口模式。
• 采集异常时，在总览点击“重试采集”，或切回游戏等待自动重试。Windows 如果询问是否允许隐藏捕获边框，可以按需授权。保留边框时同样可以采集。
• 点阵偶尔停顿时，可以在有明显纹理或景物的场景对比。动态点阵通过画面变化估计运动，结合键鼠和手柄输入提供短时预测。纯色墙、模糊画面、切镜和前景遮挡会让画面观测变少。色度辅助用于在灰度信息不足时补充观测。
• 快捷键发生冲突时，可换一个组合，或使用总览中的对应按钮。橙色提示表示快捷键冲突或观测不足，红色表示采集或日志等异常。将鼠标悬停在相关控件上可查看详情。

设置信息保存在当前 Windows 用户配置中。运行时直接打开 EXE 即可。
问题反馈：https://github.com/gzz312/MotionCompensator/issues

设计参考、致谢与许可
感谢 shsr07（拾荒散人）分享 MotionStabilizer 的屏边点阵设计。MotionCompensator 借鉴了这项设计思路，并独立实现了代码。
项目：https://github.com/shsr07/MotionStabilizer
作者视频：https://www.bilibili.com/video/BV1saKg6XEvg/
交错排布与周期连续性参考 MotionCues：https://github.com/mcpeixoto/MotionCues
视觉锚点设计参考 Peregrine：https://github.com/Eeymoo/peregrine
本项目的自定义源码公开与商业集成许可允许免费使用、学习、修改和分享，也允许符合许可条件的署名商业集成。禁止独立售卖、换皮售卖和单独收费解锁本项目功能。
在“许可与致谢”中可查看完整条款和第三方许可。项目许可以中文正文为准。
)":LR"(MotionCompensator User Guide

About MotionCompensator
MotionCompensator is a Windows visual aid designed to ease motion discomfort in games. Customizable dots and a center reticle provide visual references. Start with a subtle pattern and adjust it to your preference.

Getting started
1. Launch MotionCompensator.exe, then open your game. Select borderless or windowed mode for the overlay.
2. In Overview, click Refresh windows, select the game, then choose Register and connect. Turn on automatic connection to reconnect when the registered game is next in the foreground.
3. Choose Connect once for a single session. This turns off automatic connection.
4. In Visual effects, choose a profile below Appearance preview and adjust the dots or reticle. Return to the game to see the overlay.
5. In General, use 中文 / English to change the interface language. The theme button cycles system, light and dark. Set game overlay colors in Visual effects.

Choosing a dot style
Choose a style under Visual effects → Dots. Try moving straight, turning and stopping in the same scene and pick whichever feels most comfortable. Each style saves its own appearance settings.

Static dots provide sixteen fixed reference points in a 4×4 pattern, with soft halos on the outer columns. Pattern spread adjusts how much of the screen the dots cover.
Screen-edge dots leave the middle of the screen open. Dots of alternating sizes scroll and fade along the left and right, top and bottom, or all four edges. Edge width controls how far the pattern extends into the screen.
Regional dots split the screen into nine areas. Each area responds to local motion and input. Use Boundary fade to soften the transitions between areas.
Mesh dots suit players who want continuous movement across the screen. Dots move according to their position, and the mesh adds or removes nodes to manage spacing. Turn on Depth effect to use color and size as cues for expansion and contraction. During expansion, inner dots are smaller and bluer, outer dots are larger and redder, and the middle stays gray at the usual size. Contraction swaps the inner and outer effects. New dots fade in with the appearance of their current position. Dots return to their usual size and color when you stop.

Appearance and motion
Use Dot size, Transparency, Dot spacing and the color picker to adjust the pattern. At 0% transparency dots are fully visible, and at 100% they are fully transparent. Clear center leaves room for the reticle or subject. Staggered dots offsets alternating rows in Regional and Screen-edge modes.
In Background, choose Fixed color for a single dot color, Adaptive brightness for brightness that follows the background, or Two-tone outline for a darker outer edge and a lighter inner edge. Choose Brightness + outline to combine the two effects.
Use Motion strength to adjust scrolling speed and Reverse direction to change the direction of dot movement. For Mesh dots, Depth strength adjusts the size and color changes. Show mesh connections lets you inspect links between nodes in the game.
Scroll the settings area to see all controls. The preview on the left shows the pattern against light and dark backgrounds. Return to your game to see scrolling and depth effects.

Center reticle
The reticle provides a fixed reference at the center of the screen. Choose Cross, Diamond cross, Dot, Ring, Open cross, T-shape, Diamond or Off. Hover for the name. Blue marks the selected shape.
Open cross and T-shape leave more room at the center. Ring and Diamond provide an outline. Use the controls below to set color, transparency and size. All dot styles share these settings.
For line and outline shapes, Size measures the distance from the center to the outer edge. The full span is about twice that value. The Dot shape uses a radius of about 18% of the size value, with a one-pixel minimum.

Game profiles
Select Global defaults below Appearance preview to set the starting appearance for all games. Select a registered game to customize its settings. Restore global visuals makes the selected game use the global defaults again.
Connecting a registered game selects its profile. Save or discard any open menu-rule edits before switching the Game keys page. Register the game again if its installation path changes. Use the full path to tell apart games with the same name.

Game keys and menus
In Game keys, select a registered game and record its movement, jump, sprint, crouch and prone keys under Movement keys. MotionCompensator uses these bindings for short-term motion prediction. Prone starts blank. Use Clear to unbind an action and Restore defaults to reset the group.
Use Menu / inventory to hide the overlay, pause scrolling or show static dots while a menu is open. Enter a name and return count, then click the opening or return key field and press the key. Record again to add another key. Click Save rule when finished.
Use a different opening key for each rule in a game. Holding a return key counts as one press. Opening another configured menu starts its own return count.
Rules apply while the target game is in the foreground. If you close a menu with the mouse and the dots have yet to return, use the Toggle overlay shortcut or Show overlay in Overview to end the active rule.

Hotkeys
By default, Ctrl+Alt+F8 cycles dot styles, Ctrl+Alt+F9 toggles the overlay, and Ctrl+Alt+F10 stops capture and turns off automatic connection. While a menu rule is active, Ctrl+Alt+F9 ends the rule and shows the overlay.
In General, click a shortcut field and press the new key combination. Use Clear to remove one shortcut or Restore defaults to reset the group. Overview shows your current shortcuts and provides on-screen capture and overlay controls.

Diagnostics and logs
Diagnostics shows the game connection, capture and motion analysis. With Mesh dots selected, you can see visible dots, faces, how many nodes were added or retired, and timings for mesh updates, topology and overlay submission. The latest sample stays available when you return from the game.
Click Show debug parameters and return to the game to see direction arrows and confidence. Blue shows input prediction, yellow shows the optical flow used for motion, purple shows fused motion, and green shows dot motion. Longer arrows indicate higher estimated speeds. D counts analysis cells with direct observations in each region.
Confidence bars show the reliability of observed motion. Held regions briefly use recent reliable motion. N/A or a gap in a graph means the reading is unavailable. The FPS labels identify capture, analysis and render-submission rates. CPU timings show the cost of processing steps.
To report a problem, enable Log in Diagnostics, reproduce the issue, then turn logging off. Files are saved in the logs folder beside the EXE. Review the contents before sharing. Files are about 10 MB each. The oldest logs are removed at the limit of 20 files or 100 MB.

Troubleshooting
• If the overlay is missing, check the selected game, Show overlay and the transparency setting. Return to the game and try borderless or windowed mode.
• For a capture problem, click Retry capture in Overview or return to the game to let the app retry. If Windows asks whether to hide the capture border, choose your preference. Capture is available with the border visible.
• If dots pause unexpectedly, compare the effect in a scene with clear textures or scenery. Dynamic dots estimate motion from the picture and use keyboard, mouse and controller input for short-term prediction. Plain surfaces, blur, scene cuts and foreground objects can reduce usable observations. Color assist helps where grayscale detail is sparse.
• If a shortcut is taken, assign a different combination or use the matching button in Overview. Amber indicates a shortcut conflict or sparse observations. Red indicates a problem such as a capture or logging error. Hover over the control for details.

Settings are saved to your Windows user profile. Open the EXE to run the app.
Report problems at https://github.com/gzz312/MotionCompensator/issues

Design references and license
Thanks to shsr07 (拾荒散人) for sharing the screen-edge dot design in MotionStabilizer. MotionCompensator drew on the design and implemented its own code.
Project: https://github.com/shsr07/MotionStabilizer
Author's video: https://www.bilibili.com/video/BV1saKg6XEvg/
Staggered layout and continuous scrolling: https://github.com/mcpeixoto/MotionCues
Visual anchors: https://github.com/Eeymoo/peregrine
The custom source-available and commercial integration license permits free use, study, modification and sharing, plus commercial integration with attribution under its terms. The license prohibits standalone sale, paid rebrands and separate paid unlocks of project features.
See Licenses & credits for the full terms and third-party licenses. The Chinese project license text governs.
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
                for(const auto* title:{L"MotionCompensator 使用说明",L"关于 MotionCompensator",L"开始使用",L"选择点阵",L"调整外观与运动",L"中央准星",L"游戏配置",L"游戏按键与菜单",L"快捷键",L"运行诊断与日志",L"常见情况",L"设计参考、致谢与许可",L"MotionCompensator User Guide",L"About MotionCompensator",L"Getting started",L"Choosing a dot style",L"Appearance and motion",L"Center reticle",L"Game profiles",L"Game keys and menus",L"Hotkeys",L"Diagnostics and logs",L"Troubleshooting",L"Design references and license"}){
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
