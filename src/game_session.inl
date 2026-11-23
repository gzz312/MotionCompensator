// Win32 session and profile coordination. Included after the application declaration.
void App::commitEditor() {
    if(!controlsReady)return;
    collectSettings();
    registry.update(editorGame,preferences,editorBaseline);
    editorBaseline=preferences;
}

void App::loadEditor(int index) {
    editorGame=index;
    SendMessageW(visualProfile,CB_SETCURSEL,index+1,0);
    preferences=registry.effective(index,registry.defaults);
    crossColor=preferences.reticleColor;
    SendMessageW(reticleOpacity,TBM_SETPOS,TRUE,int(std::round((1-preferences.reticleOpacity)*100)));
    SendMessageW(reticleSize,TBM_SETPOS,TRUE,int(preferences.reticleSize));
    populateShapes(preferences.reticleShape);
    loadProfileControls();
    collectSettings();
    editorBaseline=preferences;
    updateLibrary();
    invalidatePreview();
}

mc::Preferences App::runningPreferences() const {
    if(activeGame==editorGame)return preferences;
    return registry.effective(activeGame,editorGame<0?preferences:registry.defaults);
}

void App::populateLibrary(int index) {
    for(HWND combo:{gameChoice,visualProfile}){
        SendMessageW(combo,CB_RESETCONTENT,0,0);
        auto first=tr(combo==visualProfile?L"Global defaults":L"Choose registered game");
        SendMessageW(combo,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(first));
        for(const auto& game:registry.games){
            const auto name=std::filesystem::path(game.path).filename().wstring();
            SendMessageW(combo,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(name.c_str()));
        }
        const int selectedIndex=combo==visualProfile?editorGame:index;
        SendMessageW(combo,CB_SETCURSEL,selectedIndex+1,0);
    }
}

void App::updateLibrary() {
    const int index=int(SendMessageW(gameChoice,CB_GETCURSEL,0,0))-1;
    const bool chosen=index>=0&&static_cast<size_t>(index)<registry.games.size();
    const bool visualChosen=editorGame>=0&&size_t(editorGame)<registry.games.size();
    EnableWindow(GetDlgItem(hwnd,ForgetGame),chosen);
    EnableWindow(GetDlgItem(hwnd,ResetGame),visualChosen&&registryWritable&&registry.games[editorGame].overrides!=0);
    tips.set(visualProfile,visualChosen?registry.games[editorGame].path:std::wstring(tr(L"Global defaults")));
    const auto path=chosen?registry.games[index].path:
        std::wstring(chinese?L"选择游戏以编辑按键。":L"Select a game to edit its keys.");
    setText(gamePathLabel,path);
    tips.set(gamePathLabel,path);
    setText(editorScope,tr(L"Visual profile"));
}

void App::cycleRunningStyle() {
    if(activeGame!=editorGame){
        commitEditor();
        loadEditor(activeGame);
    }
    selectStyle((preferences.selected+1)%mc::VisualStyleCount);
}

void App::attach(WindowEntry entry) {
    const int next=registry.find(entry.path);
    if(!mc::sameExecutable(entry.path,selected.path)){
        motionPaused=false;
        SendMessageW(pauseBox,BM_SETCHECK,BST_UNCHECKED,0);
    }
    if(next!=editorGame){
        commitEditor();
        loadEditor(next);
    }
    activeGame=next;
    followGameProfile(next);
    updateLibrary();
    menu.reset();resumeFlowAfter=0;
    lastDebug=0;lastLog=qpcSeconds()-1;previousFrames=0;
    driver.reset();screenField.reset();overlay.resetMotion();overlay.hide();
    meshMotionEligible=false;
    meshDiagnostics={};meshUpdatePeakMs=meshTopologyPeakMs=0;
    inputs.setTarget(nullptr,false);inputs.clear();
    selected=std::move(entry);
    recovery.begin(qpcSeconds());
    const auto visual=mc::visualSettingsFromPreferences(runningPreferences());
    capture.setFlowEnabled(visual.style!=2);
    capture.setBackgroundEnabled(visual.appearance==1||visual.appearance==3);
    if(!overlayOnly)capture.start(selected.handle,smoke,debugDirectory);
    log<<"# attach_pid="<<selected.pid<<", hwnd="<<reinterpret_cast<uintptr_t>(selected.handle)<<'\n';log.flush();
    invalidatePreview();
}

void App::stop(bool forgetTarget) {
    if(forgetTarget)reconnectPath.clear();
    overlay.hide();capture.stop();selected={};
    activeGame=-1;menu.reset();resumeFlowAfter=0;lastLog=qpcSeconds()-1;lastRunningStyle=-1;
    motionPaused=false;
    SendMessageW(pauseBox,BM_SETCHECK,BST_UNCHECKED,0);
    driver.reset();screenField.reset();overlay.resetMotion();inputs.setTarget(nullptr,false);
    meshMotionEligible=false;
    meshDiagnostics={};meshUpdatePeakMs=meshTopologyPeakMs=0;
    nextProbe=0;
    invalidatePreview();
}

void App::restartCapture(double now,bool manual) {
    if(!selected.handle||overlayOnly)return;
    const auto stats=capture.snapshot();
    if(!recovery.failed(stats,now))return;
    if(manual)recovery.begin(now);
    recovery.restarting(now);
    driver.reset();screenField.reset();lastDebug=0;lastLog=now-1;previousFrames=0;
    meshMotionEligible=false;
    meshDiagnostics={};meshUpdatePeakMs=meshTopologyPeakMs=0;
    capture.start(selected.handle,smoke,debugDirectory);
    log<<"# capture_retry elapsed_s="<<now-started<<'\n';log.flush();
}

void App::pollGame(HWND foreground,const CaptureStats& stats,double now) {
    DWORD targetPid{};
    if(selected.handle)GetWindowThreadProcessId(selected.handle,&targetPid);
    if(selected.handle&&targetPid!=selected.pid){
        stop(false);
        return;
    }
    if(selected.handle==foreground&&foreground&&!overlayOnly&&!IsIconic(foreground)&&recovery.ready(stats,now)){
        restartCapture(now,false);
        return;
    }
    if(smoke||(!automatic&&reconnectPath.empty())||!foreground||foreground==hwnd||
       foreground==selected.handle)return;

    DWORD foregroundPid{};
    GetWindowThreadProcessId(foreground,&foregroundPid);
    if(foreground==probedWindow&&foregroundPid==probedPid&&now<nextProbe)return;
    probedWindow=foreground;probedPid=foregroundPid;nextProbe=now+.75;
    if(!candidateWindow(foreground)||GetWindow(foreground,GW_OWNER))return;
    auto entry=describe(foreground);
    if(entry.pid!=foregroundPid||entry.title.empty())return;
    if(mc::sameExecutable(entry.path,reconnectPath)||(automatic&&registry.find(entry.path)>=0)){
        if(!mc::sameExecutable(entry.path,reconnectPath))reconnectPath.clear();
        attach(std::move(entry));
        setText(gameMessage,L"");
    }
}

void App::gameCommand(int id) {
    const auto selector=id==VisualProfile||id==ResetGame?visualProfile:gameChoice;
    int index=int(SendMessageW(selector,CB_GETCURSEL,0,0))-1;
    bool chosen=index>=0&&static_cast<size_t>(index)<registry.games.size();
    if(id==GameChoice){
        if(!confirmRuleNavigation()){
            SendMessageW(gameChoice,CB_SETCURSEL,registry.find(menuGamePath)+1,0);
            return;
        }
        pendingProfileFollow=false;
        selectBindingProfile();
        updateLibrary();
        return;
    }
    if(id==VisualProfile){
        if((chosen?index:-1)!=editorGame){commitEditor();loadEditor(chosen?index:-1);}
        return;
    }
    if(id==Attach||id==RegisterGame){
        const auto row=SendMessageW(list,CB_GETCURSEL,0,0);
        if(row<0||static_cast<size_t>(row)>=windows.size()){
            setText(gameMessage,chinese?L"请先从列表选择一个窗口。":L"Select a window from the list.");
            return;
        }
        const auto& cached=windows[row];
        auto entry=candidateWindow(cached.handle)?describe(cached.handle):WindowEntry{};
        if(!entry.pid||entry.pid!=cached.pid||entry.title.empty()){
            setText(gameMessage,chinese?L"窗口已关闭或更换，请刷新列表。":L"The window changed. Refresh the list and try again.");
            return;
        }
        if(id==RegisterGame){
            if(!registryWritable||!mc::validGamePath(entry.path)||GetWindow(entry.handle,GW_OWNER)){
                setText(gameMessage,chinese?L"无法登记此程序。你仍可临时连接窗口。":L"Couldn't register this app. You can still connect once.");
                return;
            }
            index=registry.find(entry.path);
            if(index<0){
                if(registry.games.size()>=mc::GameRegistry::MaxGames){
                    setText(gameMessage,chinese?L"游戏库已满，请先移除一个游戏。":L"The library is full. Remove a game to make room.");
                    return;
                }
                commitEditor();
                index=int(registry.games.size());
                registry.games.push_back({entry.path,registry.defaults,0,{},{}});
                settingsDirty=true;saveAfter=qpcSeconds()+.5;
            }
            populateLibrary(index);selectBindingProfile();updateLibrary();
            reconnectPath=entry.path;
        } else {
            automatic=false;
            SendMessageW(autoBox,BM_SETCHECK,BST_UNCHECKED,0);
            markSettings();
            reconnectPath.clear();
        }
        attach(std::move(entry));
        setText(gameMessage,L"");
        return;
    }
    if(!chosen)return;
    const bool remove=id==ForgetGame;
    const auto action=remove
        ?(chinese?L"移除此游戏及其独立设置？":L"Remove this game and its settings?")
        :(chinese?L"清除该游戏的独立设置，恢复全局继承？":L"Clear this game's overrides and use global defaults?");
    if(MessageBoxW(hwnd,action,mc::DisplayName,MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2)!=IDYES)return;
    commitEditor();
    if(remove){
        if(mc::sameExecutable(registry.games[index].path,reconnectPath))reconnectPath.clear();
        if(index==activeGame)menu.reset();
        registry.games.erase(registry.games.begin()+index);
        activeGame=registry.find(selected.path);
        int next=editorGame==index?-1:editorGame-int(editorGame>index);
        populateLibrary(-1);
        selectBindingProfile();
        loadEditor(next);
    }else{
        registry.games[index].overrides=0;
        if(editorGame==index)loadEditor(index);
    }
    settingsDirty=true;saveAfter=qpcSeconds()+.5;nextProbe=0;lastLog=qpcSeconds()-1;
    setText(gameMessage,L"");
    updateLibrary();
}
