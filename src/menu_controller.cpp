#include "menu_controller.hpp"
#include "common.hpp"
#include <algorithm>
#include <cwctype>

namespace mc {
namespace {
constexpr std::pair<std::wstring_view,unsigned> namedKeys[]={
    {L"Esc",VK_ESCAPE},{L"Tab",VK_TAB},{L"Space",VK_SPACE},{L"Enter",VK_RETURN},
    {L"Backspace",VK_BACK},{L"Delete",VK_DELETE},{L"Insert",VK_INSERT},
    {L"Home",VK_HOME},{L"End",VK_END},{L"PageUp",VK_PRIOR},{L"PageDown",VK_NEXT},
    {L"Left",VK_LEFT},{L"Right",VK_RIGHT},{L"Up",VK_UP},{L"Down",VK_DOWN},
    {L"LShift",VK_LSHIFT},{L"RShift",VK_RSHIFT},{L"LCtrl",VK_LCONTROL},{L"RCtrl",VK_RCONTROL},
    {L"LAlt",VK_LMENU},{L"RAlt",VK_RMENU},{L"CapsLock",VK_CAPITAL},
    {L"Minus",VK_OEM_MINUS},{L"Equals",VK_OEM_PLUS},{L"LeftBracket",VK_OEM_4},
    {L"RightBracket",VK_OEM_6},{L"Backslash",VK_OEM_5},{L"Semicolon",VK_OEM_1},
    {L"Quote",VK_OEM_7},{L"Comma",VK_OEM_COMMA},{L"Period",VK_OEM_PERIOD},
    {L"Slash",VK_OEM_2},{L"Backtick",VK_OEM_3}
};
constexpr std::array modifierKeys{VK_LCONTROL,VK_RCONTROL,VK_LSHIFT,VK_RSHIFT,VK_LMENU,VK_RMENU};
constexpr uint16_t modifierMask=Ctrl|Shift|Alt;
constexpr std::array<std::array<unsigned,2>,size_t(MoveAction::Count)> legacyMovement{{
    {'W',VK_UP},{'S',VK_DOWN},{'A',VK_LEFT},{'D',VK_RIGHT},{VK_SPACE,0},{VK_SHIFT,0},{VK_CONTROL,0},{0,0}
}};
bool down(unsigned key) {return key&&(GetAsyncKeyState(int(key))&0x8000)!=0;}

std::wstring_view trim(std::wstring_view text) {
    while(!text.empty()&&std::iswspace(text.front()))text.remove_prefix(1);
    while(!text.empty()&&std::iswspace(text.back()))text.remove_suffix(1);
    return text;
}

bool equalName(std::wstring_view first,std::wstring_view second) {
    return first.size()==second.size()&&std::equal(first.begin(),first.end(),second.begin(),[](wchar_t a,wchar_t b){
        return std::towlower(a)==std::towlower(b);
    });
}

bool digit(wchar_t value) {return value>=L'0'&&value<=L'9';}
unsigned keyCode(std::wstring_view text) {
    if(text.size()==1){
        auto key=std::towupper(text.front());
        if((key>=L'A'&&key<=L'Z')||digit(wchar_t(key)))return key;
    }
    if(text.size()>=2&&text.size()<=3&&(text.front()==L'f'||text.front()==L'F')){
        unsigned number=0;
        for(wchar_t value:text.substr(1)){
            if(!digit(value))return 0;
            number=number*10+unsigned(value-L'0');
        }
        if(number>=1&&number<=24)return VK_F1+number-1;
    }
    for(auto [name,key]:namedKeys)if(equalName(text,name))return key;
    if(equalName(text,L"Escape"))return VK_ESCAPE;
    if(text.size()==4&&equalName(text.substr(0,3),L"Num")&&digit(text[3]))return VK_NUMPAD0+text[3]-L'0';
    return 0;
}

std::wstring keyName(unsigned code) {
    if((code>='A'&&code<='Z')||(code>='0'&&code<='9'))return std::wstring(1,wchar_t(code));
    if(code>=VK_F1&&code<=VK_F24)return L"F"+std::to_wstring(code-VK_F1+1);
    if(code>=VK_NUMPAD0&&code<=VK_NUMPAD9)return L"Num"+std::to_wstring(code-VK_NUMPAD0);
    if(code==VK_SHIFT)return L"Shift";
    if(code==VK_CONTROL)return L"Ctrl";
    for(auto [name,key]:namedKeys)if(code==key)return std::wstring(name);
    return {};
}

}

bool validChord(MenuChord chord) {
    if((chord&0xff00)&~modifierMask)return false;
    const unsigned key=chord&0xff;
    if(keyName(key).empty())return false;
    if(key==VK_SHIFT||key==VK_CONTROL||(keyModifier(key)&&(chord&modifierMask)))return false;
    if((chord&Alt)&&(key==VK_TAB||key==VK_F4))return false;
    return true;
}

bool validShortcut(MenuChord chord) {
    const unsigned key=chord&0xff;
    return validChord(chord)&&!keyModifier(key)&&((chord&modifierMask)||(key>=VK_F1&&key<=VK_F24));
}

ShortcutBindings defaultShortcuts() {return {Ctrl|Alt|VK_F8,Ctrl|Alt|VK_F9,Ctrl|Alt|VK_F10};}

MenuChord keyModifier(unsigned key) {
    if(key==VK_CONTROL||key==VK_LCONTROL||key==VK_RCONTROL)return Ctrl;
    if(key==VK_SHIFT||key==VK_LSHIFT||key==VK_RSHIFT)return Shift;
    if(key==VK_MENU||key==VK_LMENU||key==VK_RMENU)return Alt;
    return 0;
}

unsigned physicalKey(unsigned key,unsigned scan,bool extended) {
    if(key==VK_SHIFT){
        const auto mapped=MapVirtualKeyW(scan,MAPVK_VSC_TO_VK_EX);
        return mapped==VK_RSHIFT?VK_RSHIFT:VK_LSHIFT;
    }
    if(key==VK_CONTROL)return extended?VK_RCONTROL:VK_LCONTROL;
    if(key==VK_MENU)return extended?VK_RMENU:VK_LMENU;
    return key;
}

std::wstring chordLabel(MenuChord chord) {
    return formatMenuChords(std::span(&chord,1));
}

std::wstring movementLabel(const MovementBindings& bindings,size_t action) {
    if(bindings[action]==disabledMovement)return {};
    if(bindings[action])return chordLabel(bindings[action]);
    const auto& keys=legacyMovement[action];
    auto label=keyName(keys[0]);
    if(keys[1])label+=L" / "+keyName(keys[1]);
    return label;
}

bool movementPressed(const MovementBindings& bindings,size_t action) {
    if(bindings[action]==disabledMovement)return false;
    if(const auto chord=bindings[action]){
        if(!down(chord&0xff))return false;
        return (!(chord&Ctrl)||down(VK_CONTROL))&&(!(chord&Shift)||down(VK_SHIFT))&&(!(chord&Alt)||down(VK_MENU));
    }
    const auto& keys=legacyMovement[action];
    return down(keys[0])||down(keys[1]);
}

bool movementUsesKey(const MovementBindings& bindings,unsigned key) {
    for(size_t i=0;i<bindings.size();++i){
        if(bindings[i]==disabledMovement)continue;
        if(bindings[i]){
            if((bindings[i]&0xff)==key)return true;
        } else {
            const auto& keys=legacyMovement[i];
            if(key&& (key==keys[0]||key==keys[1]||(keyModifier(key)&&keyModifier(key)==keyModifier(keys[0]))))return true;
        }
    }
    return false;
}

namespace {
MenuChord parseChord(std::wstring_view text) {
    uint16_t modifiers=0;
    for(size_t plus=text.find(L'+');plus!=text.npos;plus=text.find(L'+')){
        auto part=trim(text.substr(0,plus));
        unsigned bit=equalName(part,L"Ctrl")?Ctrl:equalName(part,L"Shift")?Shift:equalName(part,L"Alt")?Alt:0;
        if(!bit||(modifiers&bit))return 0;
        modifiers|=uint16_t(bit);
        text.remove_prefix(plus+1);
    }
    const auto chord=MenuChord(modifiers|keyCode(trim(text)));
    return validChord(chord)?chord:0;
}

bool contains(std::span<const MenuChord> keys,MenuChord chord) {
    return std::find(keys.begin(),keys.end(),chord)!=keys.end();
}
}

std::optional<std::vector<MenuChord>> parseMenuChords(std::wstring_view text) {
    if(text.size()>maxChordText)return std::nullopt;
    std::vector<MenuChord> keys;
    for(;;){
        const auto comma=text.find_first_of(L",，");
        const auto chord=parseChord(text.substr(0,comma));
        if(!chord||contains(keys,chord))return std::nullopt;
        keys.push_back(chord);
        if(comma==text.npos)break;
        text.remove_prefix(comma+1);
    }
    return keys;
}

std::wstring formatMenuChords(std::span<const MenuChord> keys) {
    std::wstring text;
    for(auto chord:keys){
        if(!text.empty())text+=L", ";
        if(chord&Ctrl)text+=L"Ctrl+";
        if(chord&Shift)text+=L"Shift+";
        if(chord&Alt)text+=L"Alt+";
        text+=keyName(chord&0xff);
    }
    return text;
}

bool validMenuRule(const MenuRule& rule) {
    if(trim(rule.name).empty()||rule.name.size()>maxMenuName||rule.name.find(L'\0')!=rule.name.npos||
       rule.returns<1||rule.returns>maxMenuReturns||
       rule.effect<MenuEffect::Hide||rule.effect>MenuEffect::Static)return false;
    if(!WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,rule.name.data(),int(rule.name.size()),nullptr,0,nullptr,nullptr))
        return false;
    for(const auto* keys:{&rule.openKeys,&rule.returnKeys}){
        if(keys->empty()||!std::all_of(keys->begin(),keys->end(),[](auto key){return validChord(key)&&!keyModifier(key&0xff);})||
           formatMenuChords(*keys).size()>maxChordText)return false;
        for(size_t i=0;i<keys->size();++i)
            if(contains(std::span(*keys).first(i),(*keys)[i]))return false;
    }
    return true;
}

bool openingConflict(const MenuRule& rule,std::span<const MenuRule> rules,int except) {
    for(size_t i=0;i<rules.size();++i)
        if(int(i)!=except)
            for(auto chord:rule.openKeys)
                if(contains(rules[i].openKeys,chord))return true;
    return false;
}

void MenuController::setForeground(bool foreground,std::span<const MenuRule> rules) {
    foreground=foreground&&!rules.empty();
    if(focused_==foreground)return;
    focused_=foreground;
    pressed_.fill(false);
    if(!focused_)return;

    // A key held across a focus change should not reopen or close a menu.
    watched_.fill(false);
    for(auto key:modifierKeys)watched_[key]=true;
    for(const auto& rule:rules)
        for(const auto* keys:{&rule.openKeys,&rule.returnKeys})
            for(auto chord:*keys)watched_[chord&0xff]=true;
    for(unsigned key=0;key<watched_.size();++key)
        if(watched_[key])pressed_[key]=(GetAsyncKeyState(int(key))&0x8000)!=0;
}

const MenuRule* MenuController::active(std::span<const MenuRule> rules) const {
    return active_&&*active_<rules.size()?&rules[*active_]:nullptr;
}

void MenuController::key(RawKey event,std::span<const MenuRule> rules) {
    if(!focused_||event.code>=pressed_.size()||!watched_[event.code])return;
    const bool alreadyDown=pressed_[event.code];
    pressed_[event.code]=event.down;
    if(!event.down||alreadyDown)return;
    MenuChord chord=event.code;
    if(!keyModifier(event.code)){
        if(pressed_[VK_LCONTROL]||pressed_[VK_RCONTROL])chord|=Ctrl;
        if(pressed_[VK_LSHIFT]||pressed_[VK_RSHIFT])chord|=Shift;
        if(pressed_[VK_LMENU]||pressed_[VK_RMENU])chord|=Alt;
    }
    if(const auto* rule=active(rules);rule&&contains(rule->returnKeys,chord)){
        if(--remaining_==0)clearContext();
        return;
    }
    for(size_t i=0;i<rules.size();++i){
        if(!contains(rules[i].openKeys,chord))continue;
        if(active_!=i){active_=i;remaining_=rules[i].returns;}
        return;
    }
}

void MenuController::clearContext() {active_.reset();remaining_=0;}

void MenuController::reset() {
    clearContext();
    focused_=false;
    pressed_.fill(false);
}

}
