// Menu session commands and the editor for a registered game's rules.
std::span<const mc::MenuRule> App::activeRules() const {
    if(activeGame<0||static_cast<size_t>(activeGame)>=registry.games.size())return {};
    return registry.games[activeGame].menus;
}

void App::observeInput(HRAWINPUT raw) {
    const auto rules=activeRules();
    auto event=inputs.onRaw(raw,!rules.empty(),runningMovement());
    if(!event)return;
    const auto* previous=menu.active(rules);
    const auto remaining=menu.remaining();
    menu.key(*event,rules);
    if(previous!=menu.active(rules)||remaining!=menu.remaining())lastLog=qpcSeconds()-1;
}

void App::restoreMenu() {
    if(!menu.active(activeRules()))return;
    menu.clearContext();
    lastLog=qpcSeconds()-1;
}

const wchar_t* App::menuEffectName(mc::MenuEffect effect) const {
    switch(effect){
        case mc::MenuEffect::Hide:return chinese?L"隐藏浮层":L"Hide overlay";
        case mc::MenuEffect::Pause:return chinese?L"暂停动态滚动":L"Pause scrolling";
        case mc::MenuEffect::Static:return chinese?L"临时静态点阵":L"Temporary static dots";
        default:return chinese?L"正常显示":L"Normal";
    }
}

static std::wstring editText(HWND control) {
    const int length=GetWindowTextLengthW(control);
    std::wstring result(length+1,L'\0');
    result.resize(GetWindowTextW(control,result.data(),length+1));
    return result;
}

bool App::hasUnsavedMenuChanges() const {
    const int index=registry.find(menuGamePath);
    if(index<0)return false;
    const auto& rules=registry.games[index].menus;
    const mc::MenuRule empty;
    const auto& rule=editingRule>=0&&static_cast<size_t>(editingRule)<rules.size()?rules[editingRule]:empty;
    return editText(menuName)!=rule.name||
        pendingOpen!=rule.openKeys||pendingReturn!=rule.returnKeys||
        editText(menuCount)!=std::to_wstring(rule.returns)||
        SendMessageW(menuEffectChoice,CB_GETCURSEL,0,0)!=int(rule.effect)-1;
}

bool App::confirmRuleNavigation() const {
    if(!hasUnsavedMenuChanges())return true;
    return MessageBoxW(hwnd,chinese?L"放弃尚未保存的菜单规则修改？":L"Discard unsaved menu rule changes?",
        mc::DisplayName,MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2)==IDYES;
}

void App::setMenuMessage(const std::wstring& value,bool errorMessage) {
    menuMessageError=errorMessage;
    setText(menuMessage,value);
    tips.set(menuMessage,value);
    InvalidateRect(menuMessage,nullptr,FALSE);
}

void App::populateMenuEditor(int ruleIndex) {
    const int index=registry.find(menuGamePath);
    const auto* game=index>=0?&registry.games[index]:nullptr;
    editingRule=game&&ruleIndex>=0&&static_cast<size_t>(ruleIndex)<game->menus.size()?ruleIndex:-1;
    SendMessageW(menuSelect,CB_RESETCONTENT,0,0);
    const std::wstring placeholder=chinese?L"新建规则":L"New rule";
    SendMessageW(menuSelect,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(placeholder.c_str()));
    if(game)for(const auto& rule:game->menus)
        SendMessageW(menuSelect,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(rule.name.c_str()));
    SendMessageW(menuSelect,CB_SETCURSEL,editingRule+1,0);
    const mc::MenuRule empty;
    const auto& rule=editingRule>=0?game->menus[editingRule]:empty;
    pendingOpen=rule.openKeys;pendingReturn=rule.returnKeys;
    setText(menuName,rule.name);setText(menuCount,std::to_wstring(rule.returns));
    setMenuKeyLabels();
    mc::fluent::setComboOptions(menuEffectChoice,
        std::array{menuEffectName(mc::MenuEffect::Hide),menuEffectName(mc::MenuEffect::Pause),menuEffectName(mc::MenuEffect::Static)},
        int(rule.effect)-1);
    EnableWindow(GetDlgItem(hwnd,MenuDelete),editingRule>=0);
    EnableWindow(GetDlgItem(hwnd,MenuSave),game&&registryWritable);
    for(HWND control:{menuSelect,menuEffectChoice,menuName,menuCount,menuOpen,menuReturn,GetDlgItem(hwnd,MenuNew),GetDlgItem(hwnd,ClearMenuOpen),GetDlgItem(hwnd,ClearMenuReturn)})
        EnableWindow(control,game&&registryWritable);
    updateBindingLabels();
    setMenuMessage(L"");
}

void App::menuCommand(int id) {
    const int index=registry.find(menuGamePath);
    if(page!=Page::Games||index<0||!registryWritable)return;
    if(id==ClearMenuOpen||id==ClearMenuReturn){
        (id==ClearMenuOpen?pendingOpen:pendingReturn).clear();
        setMenuKeyLabels();
        return;
    }
    if(id==MenuSelect||id==MenuNew){
        const int next=id==MenuNew?-1:int(SendMessageW(menuSelect,CB_GETCURSEL,0,0))-1;
        if(id==MenuSelect&&next==editingRule)return;
        if(!confirmRuleNavigation()){
            SendMessageW(menuSelect,CB_SETCURSEL,editingRule+1,0);
            return;
        }
        populateMenuEditor(next);
        if(id==MenuNew)SetFocus(menuName);
        return;
    }
    auto& rules=registry.games[index].menus;
    if(id==MenuSave){
        mc::MenuRule rule;
        rule.name=editText(menuName);
        if(pendingOpen.empty()||pendingReturn.empty()){
            setMenuMessage(chinese?L"请分别录入至少一个开启键和返回键。":L"Record at least one opening key and one return key.",true);
            return;
        }
        rule.openKeys=pendingOpen;rule.returnKeys=pendingReturn;
        const auto count=editText(menuCount);
        rule.returns=0;
        for(wchar_t digit:count){
            if(digit<L'0'||digit>L'9'){rule.returns=0;break;}
            rule.returns=rule.returns*10+unsigned(digit-L'0');
        }
        rule.effect=static_cast<mc::MenuEffect>(SendMessageW(menuEffectChoice,CB_GETCURSEL,0,0)+1);
        if(!mc::validMenuRule(rule)){
            setMenuMessage(chinese?L"请填写菜单名称，并将返回次数设为 1–99。":L"Enter a menu name and a return count from 1 to 99.",true);
            return;
        }
        auto shortcutUsed=[&](const auto& keys){return std::any_of(keys.begin(),keys.end(),[&](auto key){return usesShortcut(key);});};
        if(shortcutUsed(rule.openKeys)||shortcutUsed(rule.returnKeys)){
            setMenuMessage(chinese?L"规则里有已分配给全局快捷键的组合。":L"A key in this rule is assigned to a global shortcut.",true);
            return;
        }
        if(mc::openingConflict(rule,rules,editingRule)){
            setMenuMessage(chinese?L"开启键已被这个游戏的另一条规则使用。":L"An opening key is already used by another rule for this game.",true);
            return;
        }
        if(editingRule<0){editingRule=int(rules.size());rules.push_back(std::move(rule));}
        else rules[editingRule]=std::move(rule);
    } else if(id==MenuDelete&&editingRule>=0&&static_cast<size_t>(editingRule)<rules.size()){
        if(MessageBoxW(hwnd,chinese?L"删除此菜单规则？":L"Delete this menu rule?",
            mc::DisplayName,MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2)!=IDYES)return;
        rules.erase(rules.begin()+editingRule);
        if(static_cast<size_t>(editingRule)>=rules.size())editingRule=int(rules.size())-1;
    } else return;

    if(index==activeGame)menu.reset();
    settingsDirty=true;
    flushSettings();
    lastLog=qpcSeconds()-1;
    populateMenuEditor(editingRule);
    setMenuMessage(settingsDirty?(chinese?L"写入失败，程序将重试。":L"Couldn't save settings. Retrying."):
        (chinese?L"菜单规则已保存。":L"Menu rules saved."),settingsDirty);
    if(pendingProfileFollow)followGameProfile(activeGame);
}
