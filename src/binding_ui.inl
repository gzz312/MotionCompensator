// Keyboard recording is scoped to the focused settings control.
const mc::MovementBindings& App::runningMovement() const {
    static const mc::MovementBindings defaults{};
    return activeGame>=0&&static_cast<size_t>(activeGame)<registry.games.size()?registry.games[activeGame].movement:defaults;
}

bool App::usesShortcut(mc::MenuChord chord) const {
    return std::find(registry.shortcuts.begin(),registry.shortcuts.end(),chord)!=registry.shortcuts.end();
}

void App::toggleOverlay() {
    const bool restore=menu.active(activeRules())!=nullptr;
    if(restore)restoreMenu();
    const bool next=restore||!visible;
    SendMessageW(visibleBox,BM_SETCHECK,next?BST_CHECKED:BST_UNCHECKED,0);
    if(next!=visible){
        visible=next;
        markSettings();
    }
    lastLog=qpcSeconds()-1;
}

static mc::MenuChord shortcutFromMessage(LPARAM parameter) {
    const auto mods=LOWORD(parameter);
    return mc::MenuChord(HIWORD(parameter)|(mods&MOD_CONTROL?mc::Ctrl:0)|
        (mods&MOD_SHIFT?mc::Shift:0)|(mods&MOD_ALT?mc::Alt:0));
}

bool App::installShortcut(size_t index,mc::MenuChord chord) {
    if(!chord)return false;
    unsigned mods=MOD_NOREPEAT;
    if(chord&mc::Ctrl)mods|=MOD_CONTROL;
    if(chord&mc::Shift)mods|=MOD_SHIFT;
    if(chord&mc::Alt)mods|=MOD_ALT;
    return RegisterHotKey(hwnd,int(index)+1,mods,chord&0xff)!=FALSE;
}

void App::changeShortcuts(const mc::ShortcutBindings& requested) {
    if(!registryWritable)return;
    const auto previous=registry.shortcuts;
    auto feedback=[&](const wchar_t* cn,const wchar_t* en){
        shortcutMessageError=true;
        setText(shortcutHint,chinese?cn:en);
        InvalidateRect(shortcutHint,nullptr,FALSE);
    };
    for(size_t i=0;i<requested.size();++i){
        if(!requested[i]||requested[i]==previous[i])continue;
        if(!mc::validShortcut(requested[i])){
            feedback(L"请录入功能键，或包含 Ctrl、Shift、Alt 的组合键。",L"Use a function key or a combination with Ctrl, Shift or Alt.");
            return;
        }
        if(std::count(requested.begin(),requested.end(),requested[i])>1){
            feedback(L"这个快捷键已分配给另一项全局操作。",L"That shortcut is already assigned to another global action.");
            return;
        }
        for(const auto& game:registry.games){
            auto chord=requested[i];
            auto matches=[&](const auto& keys){return std::find(keys.begin(),keys.end(),chord)!=keys.end();};
            if(matches(game.movement)||std::any_of(game.menus.begin(),game.menus.end(),[&](const auto& rule){
                return matches(rule.openKeys)||matches(rule.returnKeys);
            })){
                feedback(L"这个快捷键已用于某个游戏的移动或菜单规则。",L"That shortcut is already used for movement or a menu rule.");
                return;
            }
        }
    }

    for(size_t i=0;i<requested.size();++i)
        if(requested[i]!=previous[i]&&hotkeys[i]){
            UnregisterHotKey(hwnd,int(i)+1);
            hotkeys[i]=false;
        }
    bool success=true;
    for(size_t i=0;i<requested.size();++i)
        if(requested[i]&&requested[i]!=previous[i]){
            hotkeys[i]=installShortcut(i,requested[i]);
            success=success&&hotkeys[i];
        }
    if(!success){
        for(size_t i=0;i<requested.size();++i)
            if(requested[i]!=previous[i]&&hotkeys[i])UnregisterHotKey(hwnd,int(i)+1);
        for(size_t i=0;i<requested.size();++i)
            if(requested[i]!=previous[i])hotkeys[i]=installShortcut(i,previous[i]);
        updateBindingLabels();
        feedback(L"快捷键被 Windows 或其他程序占用，已保留原设置。",L"Windows or another app uses that shortcut. Previous settings were kept.");
        return;
    }
    registry.shortcuts=requested;
    settingsDirty=true;
    flushSettings();
    updateBindingLabels();
    if(settingsDirty)feedback(L"暂时无法保存，程序将重试。",L"Couldn't save settings yet. Retrying.");
    else{
        shortcutMessageError=false;
        setText(shortcutHint,tr(L"Click a field, then press a key or combination."));
        InvalidateRect(shortcutHint,nullptr,FALSE);
    }
    updateTips();
}

void App::setMenuKeyLabels() {
    auto label=[&](HWND button,const auto& keys){
        if(button==armedBinding)return;
        auto value=mc::formatMenuChords(keys);
        setText(button,value.empty()?std::wstring(tr(L"Click to record")):value);
        tips.set(button,value.empty()?std::wstring(tr(L"Click a field, then press a key or combination.")):
            value+L"\n"+tr(L"Click a key field to add another option."));
    };
    label(menuOpen,pendingOpen);
    label(menuReturn,pendingReturn);
}

void App::updateBindingLabels() {
    if(!menuOpen)return;
    const int index=registry.find(menuGamePath);
    const auto* game=index>=0?&registry.games[index]:nullptr;
    const mc::MovementBindings defaults{};
    for(size_t i=0;i<movementButtons.size();++i){
        const auto button=movementButtons[i];
        EnableWindow(button,game&&registryWritable);
        auto key=mc::movementLabel(game?game->movement:defaults,i);
        EnableWindow(clearMovementButtons[i],game&&registryWritable&&!key.empty());
        if(button!=armedBinding)setText(button,key.empty()?tr(L"Unbound"):key);
    }
    EnableWindow(GetDlgItem(hwnd,ResetMovement),game&&registryWritable&&game->movement!=defaults);
    constexpr std::array names{L"Cycle style",L"Toggle overlay",L"Stop capture"};
    for(size_t i=0;i<shortcutButtons.size();++i){
        const auto chord=registry.shortcuts[i];
        const auto key=chord?mc::chordLabel(chord):std::wstring(tr(L"Unbound"));
        EnableWindow(shortcutButtons[i],registryWritable);
        EnableWindow(clearShortcutButtons[i],registryWritable&&chord!=0);
        if(shortcutButtons[i]!=armedBinding)setText(shortcutButtons[i],key+(shortcutUnavailable(i)?(chinese?L"  ·  待注册":L"  ·  Unavailable"):L""));
        setText(shortcutSummary[i],key+L"   "+tr(names[i]));
    }
    setMenuKeyLabels();
}

void App::selectBindingProfile() {
    const int chosen=int(SendMessageW(gameChoice,CB_GETCURSEL,0,0))-1;
    menuGamePath=chosen>=0&&static_cast<size_t>(chosen)<registry.games.size()?registry.games[chosen].path:L"";
    populateMenuEditor(menuGamePath.empty()||registry.games[chosen].menus.empty()?-1:0);
    setText(movementHint,tr(L"Record keys to match your game."));
}

void App::followGameProfile(int index,bool preserveDraft) {
    const auto current=registry.find(menuGamePath);
    if(current==index){
        pendingProfileFollow=false;
        return;
    }
    if(preserveDraft&&page==Page::Games&&hasUnsavedMenuChanges()){
        pendingProfileFollow=true;
        return;
    }
    pendingProfileFollow=false;
    SendMessageW(gameChoice,CB_SETCURSEL,index+1,0);
    selectBindingProfile();
    updateLibrary();
}

void App::setMovementBinding(size_t action,mc::MenuChord chord) {
    const int index=registry.find(menuGamePath);
    if(index<0||!registryWritable||action>=movementButtons.size())return;
    auto& binding=registry.games[index].movement[action];
    if(binding==chord)return;
    binding=chord;
    settingsDirty=true;flushSettings();
    if(activeGame==index){driver.reset();inputs.clear();}
    updateBindingLabels();
    setText(movementHint,settingsDirty?(chinese?L"暂时无法保存，程序将重试。":L"Couldn't save settings yet. Retrying."):
        tr(L"Record keys to match your game."));
}

void App::armBinding(HWND button) {
    if(button==armedBinding)button=nullptr;
    HWND previous=armedBinding;
    armedBinding=button;
    candidateModifier=0;
    updateBindingLabels();
    if(previous)InvalidateRect(previous,nullptr,FALSE);
    if(!button)return;
    SetFocus(button);
    setText(button,chinese?L"请按键 · 再点取消":L"Press keys · click to cancel");
    InvalidateRect(button,nullptr,FALSE);
}

bool App::captureBindingMessage(const MSG& message) {
    if(!armedBinding)return false;
    if(GetFocus()!=armedBinding||GetForegroundWindow()!=hwnd){armBinding(nullptr);return false;}
    if(message.message==WM_HOTKEY&&message.hwnd==hwnd){
        const size_t index=size_t(message.wParam-1);
        if(index<registry.shortcuts.size()&&hotkeys[index]&&
           registry.shortcuts[index]==shortcutFromMessage(message.lParam))
            applyRecordedBinding(GetDlgCtrlID(armedBinding),registry.shortcuts[index]);
        return true;
    }
    if((message.message!=WM_SYSKEYDOWN&&message.message!=WM_KEYDOWN&&
        message.message!=WM_SYSKEYUP&&message.message!=WM_KEYUP)||message.hwnd!=armedBinding)return false;
    const unsigned key=mc::physicalKey(unsigned(message.wParam),unsigned((message.lParam>>16)&0xff),(message.lParam&(1L<<24))!=0);
    const auto modifier=mc::keyModifier(key);
    const int id=GetDlgCtrlID(armedBinding);
    const bool up=message.message==WM_KEYUP||message.message==WM_SYSKEYUP;
    if(up){
        if(candidateModifier&&key==candidateModifier)applyRecordedBinding(id,mc::MenuChord(key));
        return true;
    }
    if(message.lParam&(1L<<30))return true;
    mc::MenuChord mods=0;
    if(GetKeyState(VK_CONTROL)&0x8000)mods|=mc::Ctrl;
    if(GetKeyState(VK_SHIFT)&0x8000)mods|=mc::Shift;
    if(GetKeyState(VK_MENU)&0x8000)mods|=mc::Alt;
    if(modifier){
        candidateModifier=(id>=MovementFirst&&id<MovementFirst+int(movementButtons.size())&&!(mods&~modifier))?key:0;
        return true;
    }
    candidateModifier=0;
    if(key>0&&key<255)applyRecordedBinding(id,mc::MenuChord(key|mods));
    return true;
}

void App::applyRecordedBinding(int id,mc::MenuChord chord) {
    armBinding(nullptr);
    suppressedShortcut=chord;
    suppressShortcutUntil=qpcSeconds()+.35;
    if(id>=ShortcutFirst&&id<ShortcutFirst+int(shortcutButtons.size())){
        auto candidate=registry.shortcuts;
        candidate[id-ShortcutFirst]=chord;
        changeShortcuts(candidate);
        return;
    }
    const int index=registry.find(menuGamePath);
    if(index<0||!registryWritable)return;
    if(!mc::validChord(chord)||usesShortcut(chord)){
        const bool conflict=usesShortcut(chord);
        const wchar_t* error=conflict?(chinese?L"这个组合键已分配给全局快捷键。":L"That combination is a global shortcut."):
            (chinese?L"这个按键无法录入。":L"That key can't be recorded.");
        if(id==RuleOpen||id==RuleReturn)setMenuMessage(error,true);
        else setText(movementHint,error);
        return;
    }
    if(id==RuleOpen||id==RuleReturn){
        auto& keys=id==RuleOpen?pendingOpen:pendingReturn;
        if(std::find(keys.begin(),keys.end(),chord)==keys.end()){
            auto candidate=keys;candidate.push_back(chord);
            if(mc::formatMenuChords(candidate).size()>mc::maxChordText){
                setMenuMessage(chinese?L"按键列表已满，可先清空再录入。":L"The key list is full. Clear it to record again.",true);
                return;
            }
            keys=std::move(candidate);
        }
        setMenuKeyLabels();
        setMenuMessage(L"");
        return;
    }
    if(id>=MovementFirst&&id<MovementFirst+int(movementButtons.size())){
        setMovementBinding(size_t(id-MovementFirst),chord);
    }
}
