#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace mc {

enum class MenuEffect : unsigned { Normal, Hide, Pause, Static };
enum MenuModifier : uint16_t { Ctrl=0x100, Shift=0x200, Alt=0x400 };
using MenuChord = uint16_t;
enum class MoveAction : size_t { Forward, Back, Left, Right, Jump, Sprint, Crouch, Prone, Count };
inline constexpr MenuChord disabledMovement=0xffff;
using MovementBindings = std::array<MenuChord,size_t(MoveAction::Count)>;
using ShortcutBindings = std::array<MenuChord,3>;
ShortcutBindings defaultShortcuts();
bool validChord(MenuChord chord);
bool validShortcut(MenuChord chord);
MenuChord keyModifier(unsigned key);
unsigned physicalKey(unsigned key,unsigned scan,bool extended);
std::wstring chordLabel(MenuChord chord);
std::wstring movementLabel(const MovementBindings& bindings,size_t action);
bool movementPressed(const MovementBindings& bindings,size_t action);
bool movementUsesKey(const MovementBindings& bindings,unsigned key);

struct MenuRule {
    std::wstring name;
    std::vector<MenuChord> openKeys;
    std::vector<MenuChord> returnKeys;
    unsigned returns=1;
    MenuEffect effect=MenuEffect::Hide;
    bool operator==(const MenuRule&) const = default;
};

struct RawKey {
    uint16_t code;
    bool down;
};

constexpr size_t maxMenuName=80;
constexpr size_t maxChordText=512;
constexpr unsigned maxMenuReturns=99;
std::optional<std::vector<MenuChord>> parseMenuChords(std::wstring_view text);
std::wstring formatMenuChords(std::span<const MenuChord> keys);
bool validMenuRule(const MenuRule& rule);
bool openingConflict(const MenuRule& rule,std::span<const MenuRule> rules,int except=-1);

class MenuController {
    std::array<bool,256> watched_{};
    std::array<bool,256> pressed_{};
    bool focused_=false;
    std::optional<size_t> active_;
    unsigned remaining_=0;
public:
    void setForeground(bool foreground,std::span<const MenuRule> rules);
    void key(RawKey event,std::span<const MenuRule> rules);
    void clearContext();
    void reset();
    const MenuRule* active(std::span<const MenuRule> rules) const;
    unsigned remaining() const {return remaining_;}
};

}
