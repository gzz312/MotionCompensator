// Win32 workbench implementation. Included after the application declaration.
void App::invalidatePreview() {
    if(page!=Page::Visuals)return;
    RECT area{uiOffsetX+int(184*uiScale),uiOffsetY+int(140*uiScale),
              uiOffsetX+int(728*uiScale),uiOffsetY+int((uiHeight-16)*uiScale)};
    InvalidateRect(hwnd,&area,FALSE);
}

void App::updatePageVisibility(){
    for(const auto& item:controlRects){
        bool show=ui::visible(item.section,page,settingsPage==1,dynamic);
        if(item.window==boundaryBox)show=show&&preferences.selected==0;
        if(item.window==staggerBox)show=show&&mc::hasStaggeredOption(preferences.selected);
        if(item.window==liveMeshEdgesBox||item.window==meshDepthBox)show=show&&preferences.selected==mc::TriangleStyle;
        if(item.window==meshDepthSlider||item.window==meshDepthLabel)
            show=show&&preferences.selected==mc::TriangleStyle&&preferences.meshDepth;
        if(item.window==edgesBox)show=show&&preferences.selected==1;

        if(bool(GetWindowLongPtrW(item.window,GWL_STYLE)&WS_VISIBLE)!=show)SetWindowPos(item.window,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOREDRAW|(show?SWP_SHOWWINDOW:SWP_HIDEWINDOW));
    }
    const bool visual=page==Page::Visuals;
    if(bool(GetWindowLongPtrW(settingsPane,GWL_STYLE)&WS_VISIBLE)!=visual)
        SetWindowPos(settingsPane,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOREDRAW|(visual?SWP_SHOWWINDOW:SWP_HIDEWINDOW));
    EnableWindow(pauseBox,runningPreferences().selected!=2);
    if(visual)layoutSettings();
    if(controlsReady)RedrawWindow(hwnd,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN);
}
int App::settingY(const ControlRect& item) const {
    constexpr int optionRowStart=508,optionRowHeight=44;
    return preferences.selected==2&&item.section==Section::Dots&&item.y>=optionRowStart?item.y-optionRowHeight:item.y;
}
int App::settingsContentHeight() const {return settingsPage==1?348:dynamic?728:476;}
int App::settingsScrollLimit() const {RECT r{};if(!settingsPane||uiScale<=0||settingsPage>=int(paneScroll.size()))return 0;GetClientRect(settingsPane,&r);return std::max(0,settingsContentHeight()-int(r.bottom/uiScale));}
void App::layoutSettings(){
    if(!settingsPane||settingsPage>=int(paneScroll.size()))return;
    auto& offset=paneScroll[settingsPage];offset=std::clamp(offset,0,settingsScrollLimit());
    UiLayoutBatch batch(settingsPane,int(controlRects.size()));
    for(const auto& c:controlRects)if(ui::scrollable(c.section))batch.place(c.window,int((c.x-600)*uiScale),int((settingY(c)-312-offset)*uiScale),int(c.w*uiScale),int(c.h*uiScale));
    batch.commit();RedrawWindow(settingsPane,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN);
}
void App::scrollSettings(int value){if(settingsPage>=int(paneScroll.size()))return;int next=std::clamp(value,0,settingsScrollLimit());if(next!=paneScroll[settingsPage]){paneScroll[settingsPage]=next;layoutSettings();}}
void App::revealSetting(HWND control){
    if(settingsPage>=int(paneScroll.size()))return;
    RECT r{};GetClientRect(settingsPane,&r);int page=int(r.bottom/std::max(.1f,uiScale));
    for(const auto& c:controlRects)if(c.window==control&&ui::scrollable(c.section)){int y=settingY(c)-312,offset=paneScroll[settingsPage];if(y<offset)scrollSettings(y-8);else if(y+std::min(c.h,36)>offset+page)scrollSettings(y+std::min(c.h,36)-page+8);break;}
}
RECT App::settingsThumb() const {
    RECT r{};GetClientRect(settingsPane,&r);int limit=settingsScrollLimit();if(!limit)return {};
    int page=std::max(1L,r.bottom),track=std::max(1,page-16),height=std::max(24,int(float(track)*page/(page+limit*uiScale)));
    int y=8+int(float(track-height)*paneScroll[settingsPage]/limit);return {r.right-8,y,r.right-4,y+height};
}
LRESULT CALLBACK App::settingControlProc(HWND w,UINT m,WPARAM a,LPARAM b,UINT_PTR id,DWORD_PTR ref){
    auto app=reinterpret_cast<App*>(ref);
    if(m==WM_MOUSEWHEEL){if(GetDlgCtrlID(w)==Appearance||GetDlgCtrlID(w)==Edges){if(SendMessageW(w,CB_GETDROPPEDSTATE,0,0))return DefSubclassProc(w,m,a,b);}return SendMessageW(app->settingsPane,m,a,b);}
    if(m==WM_SETFOCUS)app->revealSetting(w);
    if(m==WM_NCDESTROY)RemoveWindowSubclass(w,settingControlProc,id);
    return DefSubclassProc(w,m,a,b);
}
LRESULT CALLBACK App::settingsPaneProc(HWND w,UINT m,WPARAM a,LPARAM b,UINT_PTR id,DWORD_PTR ref){
    auto app=reinterpret_cast<App*>(ref);
    if(m==WM_ERASEBKGND)return 1;
    if(m==WM_PAINT){PAINTSTRUCT ps{};HDC dc=BeginPaint(w,&ps);RECT r{};GetClientRect(w,&r);UiPaintBuffer frame(dc,r);FillRect(frame.dc(),&r,mc::ui::surfaceBrush());
        if(app->settingsPage==0){Gdiplus::Graphics g(frame.dc());g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);float scale=app->uiScale;mc::fluent::rounded(g,4*scale,(44-app->paneScroll[0])*scale,464*scale,48*scale,6*scale,mc::ui::palette.hover,mc::ui::palette.line);}
        auto thumb=app->settingsThumb();if(thumb.bottom>thumb.top){Gdiplus::Graphics g(frame.dc());g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);mc::fluent::rounded(g,float(thumb.left),float(thumb.top),float(thumb.right-thumb.left),float(thumb.bottom-thumb.top),2,mc::ui::palette.muted);}
        frame.present();EndPaint(w,&ps);return 0;}
    if(m==WM_CTLCOLORSTATIC||m==WM_CTLCOLORBTN){HDC dc=reinterpret_cast<HDC>(a);SetTextColor(dc,mc::fluent::ink);SetBkMode(dc,TRANSPARENT);return reinterpret_cast<LRESULT>(mc::ui::surfaceBrush());}
    if(m==WM_MOUSEWHEEL&&app->settingsPage<int(app->paneScroll.size())){app->scrollWheel+=GET_WHEEL_DELTA_WPARAM(a);int steps=app->scrollWheel/WHEEL_DELTA;app->scrollWheel%=WHEEL_DELTA;if(steps)app->scrollSettings(app->paneScroll[app->settingsPage]-steps*48);return 0;}
    if(m==WM_LBUTTONDOWN&&app->settingsPage<int(app->paneScroll.size())){RECT r{};GetClientRect(w,&r);int x=GET_X_LPARAM(b),y=GET_Y_LPARAM(b);if(app->settingsScrollLimit()>0&&x>=r.right-16){auto thumb=app->settingsThumb();if(y>=thumb.top&&y<thumb.bottom){app->scrollDragging=true;app->scrollDragY=y;app->scrollDragStart=app->paneScroll[app->settingsPage];SetCapture(w);}else app->scrollSettings(app->paneScroll[app->settingsPage]+(y<thumb.top?-1:1)*int(r.bottom/std::max(.1f,app->uiScale)));return 0;}}
    if(m==WM_MOUSEMOVE&&app->scrollDragging&&app->settingsPage<int(app->paneScroll.size())){RECT r{};GetClientRect(w,&r);auto thumb=app->settingsThumb();int travel=std::max(1L,r.bottom-16-(thumb.bottom-thumb.top));app->scrollSettings(app->scrollDragStart+(GET_Y_LPARAM(b)-app->scrollDragY)*app->settingsScrollLimit()/travel);return 0;}
    if(m==WM_LBUTTONUP||m==WM_CANCELMODE||m==WM_CAPTURECHANGED){app->scrollDragging=false;if(GetCapture()==w)ReleaseCapture();}
    if(m==WM_COMMAND||m==WM_HSCROLL||m==WM_MEASUREITEM||m==WM_DRAWITEM||m==WM_CTLCOLORLISTBOX)return SendMessageW(app->hwnd,m,a,b);
    if(m==WM_NCDESTROY)RemoveWindowSubclass(w,settingsPaneProc,id);
    return DefSubclassProc(w,m,a,b);
}

void App::layout(){
    if(!controlsReady||IsIconic(hwnd))return;
    RECT client{};GetClientRect(hwnd,&client);int dpi=int(GetDpiForWindow(hwnd));
    if(client.right<=0||client.bottom<=0||(layoutWidth==client.right&&layoutHeight==client.bottom&&layoutDpi==dpi))return;
    layoutWidth=client.right;layoutHeight=client.bottom;layoutDpi=dpi;
    int caption=mc::captionHeight(hwnd);
    const auto frame=ui::fit(client.right,client.bottom,caption,dpi);
    uiScale=frame.scale;
    uiOffsetX=frame.x;
    uiOffsetY=frame.y;
    uiHeight=frame.height;
    UiLayoutBatch batch(hwnd,int(controlRects.size())+2);
    int captionWidth=mc::captionButtonWidth(hwnd);
    if(captionMinimize)batch.place(captionMinimize,client.right-2*captionWidth,0,captionWidth,caption);
    if(captionClose)batch.place(captionClose,client.right-captionWidth,0,captionWidth,caption);
    for(const auto& c:controlRects) {
        if(ui::scrollable(c.section))continue;
        const bool visual=c.section==Section::VisualHeader;
        const int x=c.x+(visual?ui::visualLeft:0), y=c.y-(visual?ui::visualRise:0);
        batch.place(c.window,uiOffsetX+int(x*uiScale),uiOffsetY+int(y*uiScale),
                    int(c.w*uiScale),int(c.h*uiScale));
    }
    batch.place(settingsPane,uiOffsetX+int((600+ui::visualLeft)*uiScale),
                uiOffsetY+int(ui::paneTop*uiScale),int(488*uiScale),
                int((uiHeight-ui::paneTop-32)*uiScale));
    int scalePixels=std::max(1,int(14*uiScale))*10000+std::max(1,int(12*uiScale))*100+std::max(1,int(28*uiScale));
    if(fontScalePixels!=scalePixels){
        fontScalePixels=scalePixels;HFONT oldFont=font,oldSmall=smallFont,oldTitle=titleFont;
        auto make=[&](int size,int weight){return CreateFontW(-std::max(1,int(size*uiScale)),0,0,0,weight,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI Variable Text");};
        font=make(14,FW_NORMAL);smallFont=make(12,FW_NORMAL);titleFont=make(28,FW_SEMIBOLD);
        for(auto c:controlRects)SendMessageW(c.window,WM_SETFONT,reinterpret_cast<WPARAM>((c.window==headerTitle||(c.section!=Section::Shared&&c.x==208&&(c.y==104||c.y==112)))?titleFont:(c.window==headerStatus||c.window==editorScope||c.window==gamePathLabel)?smallFont:font),FALSE);
        DeleteObject(oldFont);DeleteObject(oldSmall);DeleteObject(oldTitle);
    }
    batch.commit();layoutSettings();
    for(HWND combo:{list,gameChoice,visualProfile,edgesBox,appearanceBox,menuSelect,menuEffectChoice})mc::fluent::comboHeight(combo,int(32*uiScale));
    RedrawWindow(hwnd,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN);
}
void App::paint() {
    PAINTSTRUCT paint{};HDC screen=BeginPaint(hwnd,&paint);RECT client{};GetClientRect(hwnd,&client);UiPaintBuffer buffer(screen,client);HDC dc=buffer.dc();FillRect(dc,&client,background);
    HFONT captionFont=CreateFontW(-MulDiv(13,int(GetDpiForWindow(hwnd)),96),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    RECT captionRect{MulDiv(12,int(GetDpiForWindow(hwnd)),96),0,client.right-2*mc::captionButtonWidth(hwnd),mc::captionHeight(hwnd)};mc::fluent::text(dc,captionRect,mc::DisplayName,captionFont,mc::fluent::ink);DeleteObject(captionFont);
    SetMapMode(dc,MM_ANISOTROPIC);
    SetWindowExtEx(dc,ui::width,uiHeight,nullptr);
    SetViewportExtEx(dc,int(ui::width*uiScale),int(uiHeight*uiScale),nullptr);
    SetViewportOrgEx(dc,uiOffsetX,uiOffsetY,nullptr);
    HPEN border=CreatePen(PS_SOLID,1,mc::ui::palette.line);auto savedPen=SelectObject(dc,border);SelectObject(dc,card);
    auto panel=[&](int left,int top,int right,int bottom){RoundRect(dc,left,top,right,bottom,16,16);};
    panel(16,88,168,uiHeight-16);
    if(page==Page::Visuals) {
        panel(184,140,728,uiHeight-16);
        panel(752,140,1256,uiHeight-16);
    } else if(page==Page::Overview){
        panel(184,152,820,454);
        panel(840,152,1256,454);
        panel(184,474,1256,556);
    } else if(page==Page::Games){
        panel(184,152,1256,258);
        panel(184,278,680,647);
        panel(700,278,1256,647);
        panel(880,369,1232,407);
        panel(828,511,896,549);
    } else {
        panel(184,88,1256,uiHeight-16);
    }
    SelectObject(dc,savedPen);DeleteObject(border);
    if(page!=Page::Visuals) {
        buffer.present();
        EndPaint(hwnd,&paint);
        return;
    }
    SetViewportOrgEx(dc,uiOffsetX+int(ui::visualLeft*uiScale),
                    uiOffsetY-int(ui::visualRise*uiScale),nullptr);
    // A settings preview, not a simulated game. Uses the same mask as the overlay.
    RECT preview{40,292,552,612};HBRUSH dark=CreateSolidBrush(RGB(20,29,44));FillRect(dc,&preview,dark);DeleteObject(dark);
    auto v=visualSettings();
    MONITORINFO monitor{};monitor.cbSize=sizeof(monitor);
    GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&monitor);
    int sourceW=std::max(1L,monitor.rcMonitor.right-monitor.rcMonitor.left),sourceH=std::max(1L,monitor.rcMonitor.bottom-monitor.rcMonitor.top);RECT targetSize{};
    if(selected.handle && GetClientRect(selected.handle,&targetSize) && targetSize.right>0 && targetSize.bottom>0){sourceW=targetSize.right;sourceH=targetSize.bottom;}
    auto scene=mc::buildPreview(v,512,320,sourceW,sourceH);
    static const auto ramp=[](){std::array<uint32_t,512> pixels{};for(int i=0;i<512;++i){uint32_t shade=24+196*i/511;pixels[i]=shade|(shade<<8)|(shade<<16);}return pixels;}();
    BITMAPINFO rampInfo{};rampInfo.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);rampInfo.bmiHeader.biWidth=512;rampInfo.bmiHeader.biHeight=-1;rampInfo.bmiHeader.biPlanes=1;rampInfo.bmiHeader.biBitCount=32;rampInfo.bmiHeader.biCompression=BI_RGB;
    int previewLeft=int(std::ceil(scene.contentLeft)),previewTop=int(std::ceil(scene.contentTop)),previewRight=int(std::floor(scene.contentRight)),previewBottom=int(std::floor(scene.contentBottom));
    StretchDIBits(dc,40+previewLeft,292+previewTop,previewRight-previewLeft,previewBottom-previewTop,0,0,512,1,ramp.data(),&rampInfo,DIB_RGB_COLORS,SRCCOPY);
    paintPreview(dc,{40,292},scene);
    SetBkMode(dc,TRANSPARENT);SetTextColor(dc,mc::fluent::muted);HFONT paintFont=CreateFontW(-14,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");auto savedFont=SelectObject(dc,paintFont);
    auto text=[&](int x,int y,std::wstring value){TextOutW(dc,x,y,value.c_str(),int(value.size()));};
    text(40,632,styleName(preferences.selected));
    text(40,656,chinese?L"调整设置即时预览，切回游戏应用。":L"Live preview. Return to your game to apply.");
    if(!dynamic)text(328,632,chinese?L"4 × 4 · 外列柔边":L"4 x 4 / outer halos");
    SelectObject(dc,savedFont);DeleteObject(paintFont);buffer.present();EndPaint(hwnd,&paint);
}


void App::create(HINSTANCE instance) {
    wchar_t path[MAX_PATH]{};GetModuleFileNameW(nullptr,path,MAX_PATH);settingsPath=std::filesystem::path(path).parent_path()/L"settings.ini";
    auto seed=mc::embeddedText(101);std::istringstream initial(seed);mc::readPreferences(initial,preferences);
    bool found=false;
    registry.defaults=preferences;
    registryWritable=mc::loadGameRegistry(registry,found,settingsError);
    if(found){
        preferences=registry.defaults;
    }else{
        bool legacyFound=false;
        registryWritable=mc::loadUserPreferences(preferences,legacyFound,settingsError);
        if(registryWritable&&!legacyFound){
            std::error_code error;
            if(std::filesystem::exists(settingsPath,error))
                registryWritable=mc::loadPreferences(settingsPath,preferences,settingsError);
        }
        registry.defaults=preferences;
        settingsDirty=registryWritable;saveAfter=qpcSeconds()+.5;
    }
    if(smoke){
        for(auto& profile:preferences.profiles)profile.appearance=1;
        registry.defaults=preferences;
    }
    mc::ui::setTheme(preferences.theme);applyTheme(true);
    chinese=preferences.chinese;visible=preferences.visible;automatic=preferences.automatic;dynamic=preferences.selected!=2;
    crossColor=preferences.reticleColor;
    smallFont=CreateFontW(-14,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    font=CreateFontW(-14,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    settingsPane=CreateWindowExW(WS_EX_CONTROLPARENT,L"STATIC",L"",WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN,0,0,1,1,hwnd,nullptr,instance,nullptr);
    if(!settingsPane || !SetWindowSubclass(settingsPane,settingsPaneProc,77,reinterpret_cast<DWORD_PTR>(this)))
        throw std::runtime_error("Creating the settings scroll pane failed");
    Section section=Section::Shared;
    auto child=[&](const wchar_t* cls,const wchar_t* text,DWORD style,int x,int y,int w,int h,int id)->HWND {
        if(wcscmp(cls,L"BUTTON")==0||wcscmp(cls,L"COMBOBOX")==0||wcscmp(cls,L"EDIT")==0)style|=WS_TABSTOP;
        if(wcscmp(cls,L"COMBOBOX")==0)style|=CBS_OWNERDRAWFIXED|CBS_HASSTRINGS;
        if(wcscmp(cls,L"STATIC")==0)style|=SS_NOTIFY;
        bool scrollable=ui::scrollable(section);
        HWND c=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,x,y,w,h,scrollable?settingsPane:hwnd,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),instance,nullptr);
        if(!c)throw std::runtime_error("Creating a workbench control failed");
        if(wcscmp(cls,L"STATIC")==0)mc::fluent::install(c,mc::fluent::Kind::Label);
        else if(wcscmp(cls,L"COMBOBOX")==0)mc::fluent::install(c,mc::fluent::Kind::Combo);
        else if(wcscmp(cls,TRACKBAR_CLASSW)==0)mc::fluent::install(c,mc::fluent::Kind::Slider);
        else if(wcscmp(cls,L"BUTTON")==0)mc::fluent::install(c,(style&BS_TYPEMASK)==BS_AUTOCHECKBOX?mc::fluent::Kind::Check:mc::fluent::Kind::Button);
        if(scrollable && !SetWindowSubclass(c,settingControlProc,78,reinterpret_cast<DWORD_PTR>(this)))
            throw std::runtime_error("Installing the scroll handler failed");
        controlRects.push_back({c,x,y,w,h,section});
        labels.push_back({c,text});SetWindowTextW(c,tr(labels.back().second));
        SendMessageW(c,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE); return c;
    };
    titleFont=CreateFontW(-28,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI Variable Text");
    headerTitle=child(L"STATIC",L"MotionCompensator",0,24,5,550,42,0);
    SendMessageW(headerTitle,WM_SETFONT,reinterpret_cast<WPARAM>(titleFont),TRUE);
    headerStatus=child(L"STATIC",L"",0,24,49,1016,24,0);
    child(L"BUTTON",L"User Guide",BS_OWNERDRAW,1100,20,156,36,Instructions);
    const std::array<const wchar_t*,5> pages={L"Overview",L"Visual effects",L"Game keys",L"Diagnostics",L"General"};
    for(int i=0;i<int(pages.size());++i)
        child(L"BUTTON",pages[i],BS_OWNERDRAW,24,104+i*48,136,40,NavOverview+i);

    section=Section::Overview;
    child(L"STATIC",L"Overview",0,208,104,500,36,0);
    child(L"STATIC",L"Current game",0,208,169,580,24,0);
    targetWindowLabel=child(L"STATIC",L"",0,208,199,588,29,0);
    status=child(L"STATIC",L"",0,208,233,588,24,0);
    menuStateLabel=child(L"STATIC",L"Windows",0,208,258,588,22,0);
    list=child(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,208,289,588,248,List);
    child(L"BUTTON",L"Register and connect",BS_OWNERDRAW,208,341,196,36,RegisterGame);
    child(L"BUTTON",L"Connect once",BS_OWNERDRAW,416,341,160,36,Attach);
    child(L"BUTTON",L"Refresh windows",BS_OWNERDRAW,588,341,176,36,Refresh);
    autoBox=child(L"BUTTON",L"Automatically connect registered games",BS_AUTOCHECKBOX,208,394,588,30,Auto);
    SendMessageW(autoBox,BM_SETCHECK,automatic?BST_CHECKED:BST_UNCHECKED,0);
    gameMessage=child(L"STATIC",L"",0,208,426,588,20,0);

    child(L"STATIC",L"Quick controls",0,864,169,368,24,0);
    overviewStyle=child(L"BUTTON",L"",BS_OWNERDRAW,864,209,368,38,OverviewStyle);
    visibleBox=child(L"BUTTON",L"Show overlay",BS_AUTOCHECKBOX,864,264,368,32,Visible);
    pauseBox=child(L"BUTTON",L"Pause dynamic motion",BS_AUTOCHECKBOX,864,308,368,32,PauseMotion);
    retryButton=child(L"BUTTON",L"Retry capture",BS_OWNERDRAW,864,370,176,36,RetryCapture);
    stopButton=child(L"BUTTON",L"Stop capture",BS_OWNERDRAW,1052,370,180,36,Stop);
    EnableWindow(retryButton,FALSE);
    SendMessageW(visibleBox,BM_SETCHECK,visible?BST_CHECKED:BST_UNCHECKED,0);
    child(L"STATIC",L"Global shortcuts",0,208,484,1008,24,0);
    for(size_t i=0;i<shortcutSummary.size();++i)
        shortcutSummary[i]=child(L"STATIC",L"",0,208+int(i)*344,512,332,28,0);

    section=Section::Games;
    child(L"STATIC",L"Game keys",0,208,104,580,36,0);
    child(L"STATIC",L"Game library",0,208,164,500,22,0);
    gameChoice=child(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,208,191,876,248,GameChoice);
    gamePathLabel=child(L"STATIC",L"",0,208,227,876,22,0);
    child(L"BUTTON",L"Remove",BS_OWNERDRAW,1108,191,124,36,ForgetGame);

    child(L"STATIC",L"Movement keys",0,208,293,440,24,0);
    constexpr std::array movementNames{L"Forward",L"Backward",L"Move left",L"Move right",L"Jump",L"Sprint",L"Crouch",L"Prone"};
    for(size_t i=0;i<movementButtons.size();++i){
        const int y=328+int(i)*32;
        child(L"STATIC",movementNames[i],0,208,y,136,30,0);
        movementButtons[i]=child(L"BUTTON",L"",BS_OWNERDRAW,354,y,204,30,MovementFirst+int(i));
        clearMovementButtons[i]=child(L"BUTTON",L"Clear",BS_OWNERDRAW,570,y,84,30,ClearMovementFirst+int(i));
    }
    child(L"BUTTON",L"Restore defaults",BS_OWNERDRAW,208,590,174,32,ResetMovement);
    movementHint=child(L"STATIC",L"Record keys to match your game.",0,394,584,260,50,0);

    child(L"STATIC",L"Menu / inventory",0,724,293,500,24,0);
    menuSelect=child(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,724,324,358,248,MenuSelect);
    child(L"BUTTON",L"New rule",BS_OWNERDRAW,1094,324,138,36,MenuNew);
    child(L"STATIC",L"Menu name",0,724,369,150,38,0);
    menuName=child(L"EDIT",L"",ES_AUTOHSCROLL,890,377,332,24,RuleName);
    SendMessageW(menuName,EM_LIMITTEXT,int(mc::maxMenuName),0);
    child(L"STATIC",L"Open keys",0,724,417,142,36,0);
    menuOpen=child(L"BUTTON",L"",BS_OWNERDRAW,880,417,270,36,RuleOpen);
    child(L"BUTTON",L"Clear",BS_OWNERDRAW,1160,417,72,36,ClearMenuOpen);
    child(L"STATIC",L"Return keys",0,724,463,142,36,0);
    menuReturn=child(L"BUTTON",L"",BS_OWNERDRAW,880,463,270,36,RuleReturn);
    child(L"BUTTON",L"Clear",BS_OWNERDRAW,1160,463,72,36,ClearMenuReturn);
    child(L"STATIC",L"Return presses",0,724,511,102,38,0);
    menuCount=child(L"EDIT",L"",ES_AUTOHSCROLL|ES_NUMBER,838,519,48,24,RuleCount);
    SendMessageW(menuCount,EM_LIMITTEXT,2,0);
    child(L"STATIC",L"Menu effect",0,908,511,142,38,0);
    menuEffectChoice=child(L"COMBOBOX",L"",CBS_DROPDOWNLIST,1054,511,178,170,MenuEffectChoice);
    child(L"BUTTON",L"Save rule",BS_OWNERDRAW,724,568,166,36,MenuSave);
    child(L"BUTTON",L"Delete rule",BS_OWNERDRAW,902,568,166,36,MenuDelete);
    menuMessage=child(L"STATIC",L"",0,724,611,508,24,0);

    section=Section::Diagnostics;
    child(L"STATIC",L"Diagnostics",0,208,112,500,36,0);
    debugButton=child(L"BUTTON",L"Show debug parameters",BS_OWNERDRAW,208,172,248,36,DebugParameters);
    logBox=child(L"BUTTON",L"Log",BS_AUTOCHECKBOX,480,172,112,36,RecordLog);
    chromaStatus=child(L"STATIC",L"",0,208,228,1008,28,0);
    diagnosticsStatus=child(L"STATIC",L"Enable diagnostics to view capture and motion data.",0,208,270,536,330,0);
    meshDiagnosticsStatus=child(L"STATIC",L"",0,774,270,442,330,0);

    section=Section::General;
    child(L"STATIC",L"General",0,208,112,500,36,0);
    child(L"STATIC",L"Language",0,208,172,184,36,0);
    child(L"BUTTON",L"中文 / English",BS_OWNERDRAW,400,172,184,36,Language);
    child(L"STATIC",L"Theme",0,208,232,184,36,0);
    themeButton=child(L"BUTTON",L"",BS_OWNERDRAW,400,232,184,36,Theme);populateTheme();
    child(L"STATIC",L"Global shortcuts",0,208,320,500,28,0);
    constexpr std::array shortcutNames{L"Cycle style",L"Toggle overlay",L"Stop capture"};
    for(size_t i=0;i<shortcutButtons.size();++i){
        const int y=364+int(i)*54;
        child(L"STATIC",shortcutNames[i],0,208,y,184,36,0);
        shortcutButtons[i]=child(L"BUTTON",L"",BS_OWNERDRAW,400,y,246,36,ShortcutFirst+int(i));
        clearShortcutButtons[i]=child(L"BUTTON",L"Clear",BS_OWNERDRAW,658,y,98,36,ClearShortcutFirst+int(i));
    }
    child(L"BUTTON",L"Restore defaults",BS_OWNERDRAW,208,532,184,36,ResetShortcuts);
    shortcutHint=child(L"STATIC",L"Click a field, then press a key or combination.",0,400,532,810,36,0);

    section=Section::VisualHeader;
    editorScope=child(L"STATIC",L"Visual profile",0,40,682,500,26,0);
    visualProfile=child(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,40,715,292,248,VisualProfile);
    child(L"BUTTON",L"Restore global visuals",BS_OWNERDRAW,344,715,208,32,ResetGame);
    child(L"STATIC",L"Appearance preview",0,40,264,320,24,0);
    child(L"BUTTON",L"Dots",BS_OWNERDRAW,608,264,128,36,PageDots);
    child(L"BUTTON",L"Reticle",BS_OWNERDRAW,744,264,128,36,PageReticle);
    section=Section::Dots;
    child(L"STATIC",L"Dot style",0,608,324,456,24,0);
    tabs[0]=child(L"BUTTON",L"Regional",BS_OWNERDRAW,608,360,108,40,TabRegional);
    tabs[1]=child(L"BUTTON",L"Screen edge",BS_OWNERDRAW,723,360,108,40,TabEdge);
    tabs[2]=child(L"BUTTON",L"Static",BS_OWNERDRAW,838,360,95,40,TabStatic);
    tabs[mc::TriangleStyle]=child(L"BUTTON",L"Mesh dots",BS_OWNERDRAW,940,360,124,40,TabTriangle);
    child(L"STATIC",L"Appearance settings",0,608,420,456,28,0);
    staggerBox=child(L"BUTTON",L"Staggered dots",BS_AUTOCHECKBOX,608,460,160,24,Staggered);
    liveMeshEdgesBox=child(L"BUTTON",L"Show mesh connections",BS_AUTOCHECKBOX,608,460,228,24,LiveMeshEdges);
    meshDepthBox=child(L"BUTTON",L"Depth effect",BS_AUTOCHECKBOX,846,460,218,24,MeshDepth);
    edgesBox=child(L"COMBOBOX",L"",CBS_DROPDOWNLIST,792,456,272,180,Edges);
    auto slider=[&](const wchar_t* label,int y,int id,int low,int high,int initial) {
        HWND valueLabel=child(L"STATIC",label,0,608,y,192,32,0);
        HWND control=child(TRACKBAR_CLASSW,L"",TBS_NOTICKS|WS_TABSTOP,808,y,256,32,id);
        SendMessageW(control,TBM_SETRANGE,TRUE,MAKELPARAM(low,high));SendMessageW(control,TBM_SETPOS,TRUE,initial);sliderLabels.push_back({control,valueLabel});return control;
    };
    radius=slider(L"Dot radius",508,Radius,1,24,3);
    opacity=slider(L"Transparency",564,Opacity,0,100,0);
    spacing=slider(L"Dot spacing",620,Spacing,60,240,122);
    child(L"BUTTON",L"Choose dot color",BS_OWNERDRAW,608,676,456,36,DotColor);
    hole=slider(L"Clear center (%)",728,Hole,5,25,10);
    child(L"STATIC",L"Background",0,608,780,192,32,0);
    appearanceBox=child(L"COMBOBOX",L"",CBS_DROPDOWNLIST,808,780,256,180,Appearance);
    section=Section::Motion;
    child(L"STATIC",L"Motion settings",0,608,848,456,28,0);
    gainLabel=child(L"STATIC",L"Motion strength",0,608,892,192,32,0);
    gain=child(TRACKBAR_CLASSW,L"",TBS_NOTICKS|WS_TABSTOP,808,892,256,32,Gain);SendMessageW(gain,TBM_SETRANGE,TRUE,MAKELPARAM(25,200));sliderLabels.push_back({gain,gainLabel});
    invertBox=child(L"BUTTON",L"Reverse direction",BS_AUTOCHECKBOX,608,944,456,24,Invert);
    boundaryBox=child(L"BUTTON",L"Boundary fade",BS_AUTOCHECKBOX,608,988,456,24,BoundaryFade);
    meshDepthSlider=slider(L"Depth strength",988,MeshDepthStrength,0,100,70);
    meshDepthLabel=sliderLabels.back().second;
    section=Section::Reticle;
    child(L"STATIC",L"Center reticle",0,608,328,400,28,0);
    for(int i=0;i<mc::ReticleShapeCount;++i)
        reticleButtons[i]=child(L"BUTTON",L"",BS_OWNERDRAW,608+i*58,380,52,52,ReticleFirst+i);
    populateShapes(preferences.reticleShape);
    reticleOpacity=slider(L"Transparency",488,ReticleOpacity,0,100,0);
    reticleSize=slider(L"Reticle size",544,ReticleSize,mc::ReticleSizeMin,mc::ReticleSizeMax,10);
    child(L"BUTTON",L"Choose reticle color",BS_OWNERDRAW,608,600,456,36,ReticleColor);
    registerKeys();
    inputs.initialize(hwnd);
    if(!inputs.rawReady) error+=L" Raw Input unavailable.";
    SendMessageW(reticleOpacity,TBM_SETPOS,TRUE,int(std::round((1-preferences.reticleOpacity)*100)));
    SendMessageW(reticleSize,TBM_SETPOS,TRUE,int(preferences.reticleSize));
    captionMinimize=CreateWindowW(L"BUTTON",L"最小化 / Minimize",WS_CHILD|WS_VISIBLE|BS_OWNERDRAW,0,0,1,1,hwnd,reinterpret_cast<HMENU>(CaptionMinimize),instance,nullptr);
    captionClose=CreateWindowW(L"BUTTON",L"关闭 / Close",WS_CHILD|WS_VISIBLE|BS_OWNERDRAW,0,0,1,1,hwnd,reinterpret_cast<HMENU>(CaptionClose),instance,nullptr);
    mc::fluent::install(captionMinimize,mc::fluent::Kind::Button);mc::fluent::install(captionClose,mc::fluent::Kind::Button);
    controlsReady=true;
    populateLibrary(-1);
    selectBindingProfile();
    loadEditor(-1);
    const auto startingStatus=connectionLabel(CaptureStats{},false,qpcSeconds());
    setText(headerStatus,std::wstring(chinese?L"未连接":L"No target")+L"  ·  "+startingStatus);
    setText(targetWindowLabel,chinese?L"未连接":L"No target");
    setText(status,startingStatus);
    setText(overviewStyle,std::wstring(styleName(preferences.selected))+L"  ↻");
    layout();
    updatePageVisibility();
    overlay.initialize(instance);overlay.invalidate();populateShapes(preferences.reticleShape);refresh();refreshValueLabels();
    wchar_t exe[MAX_PATH]{}; GetModuleFileNameW(nullptr,exe,MAX_PATH);
    logDirectory=std::filesystem::path(exe).parent_path()/L"logs";
    if(flowDebug)debugDirectory=logDirectory/(L"flow-debug-"+std::to_wstring(GetCurrentProcessId()));
    log.setHeader("elapsed_s,capture_frames,capture_fps,frame_age_at_receive_ms,latest_frame_age_ms,width,height,last_render_submit_ms,render_max_ms,render_count,raw_events,raw_timestamp_qpc_s,requested_dynamic,manual_static,capture_running,cpu_one_core_pct,working_set_mb\n");
    tips.initialize(hwnd);updateLibrary();updateTips();updateBindingLabels();
    if(diagnosticRecordLog)setLogging(true);
    FILETIME c{},e{},k{},u{}; GetProcessTimes(GetCurrentProcess(),&c,&e,&k,&u);
    previousCpu=(double((uint64_t(k.dwHighDateTime)<<32)|k.dwLowDateTime)+double((uint64_t(u.dwHighDateTime)<<32)|u.dwLowDateTime))/1e7;
    InvalidateRect(hwnd,nullptr,FALSE);
    SetTimer(hwnd,1,16,nullptr);
}
