#pragma once
#include "preferences.hpp"
#include "menu_controller.hpp"
#include <string>
#include <vector>

namespace mc {

struct GameProfile {
    std::wstring path;
    Preferences values;
    uint64_t overrides{};
    std::vector<MenuRule> menus;
    MovementBindings movement{};
};

struct GameRegistry {
    static constexpr size_t MaxGames = 128;
    Preferences defaults;
    ShortcutBindings shortcuts=defaultShortcuts();
    std::vector<GameProfile> games;

    int find(const std::wstring& path) const;
    Preferences effective(int index, const Preferences& base) const;
    void update(int index, const Preferences& edited, const Preferences& before);
};

bool sameExecutable(const std::wstring& first, const std::wstring& second);
bool validGamePath(const std::wstring& path);
bool loadGameRegistry(GameRegistry& registry, bool& found, std::string& error);
bool saveGameRegistry(const GameRegistry& registry, std::string& error);

}
