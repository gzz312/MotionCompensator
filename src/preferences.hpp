#pragma once
#include "core.hpp"
#include <filesystem>
#include <istream>
#include <ostream>
namespace mc {
struct DotProfile {
    float radius=3,opacity=1,spacing=122,hole=.10f,edgeWidth=140,gain=1;
    Color color{};
    bool enhancement=false,boundaryFade=true,inverted=false,staggered=false;
    int edges=0,appearance=1;
    bool operator==(const DotProfile&) const = default;
};
struct Preferences {
    std::array<DotProfile,3> profiles{};
    int selected=0,reticleShape=1,theme=0; // system, light, dark; logging is deliberately not persisted
    float reticleOpacity=1,reticleSize=10;
    Color reticleColor{.2f,1,.45f};
    bool chinese=true,visible=true,automatic=true;
    Preferences() {
        profiles[1].enhancement=true;
        profiles[1].radius=24;profiles[1].opacity=.75f;profiles[1].spacing=150;profiles[1].edgeWidth=300;profiles[1].staggered=true;profiles[1].color={0,1,0};profiles[1].boundaryFade=false;
        profiles[2].enhancement=false;profiles[2].boundaryFade=false;
    }
    bool operator==(const Preferences&) const = default;
};
inline constexpr const wchar_t* UserSettingsKey=L"Software\\MotionCompensator";
bool loadUserPreferences(Preferences& result,bool& found,std::string& error,const wchar_t* key=UserSettingsKey);
bool saveUserPreferences(const Preferences& value,std::string& error,const wchar_t* key=UserSettingsKey);
void sanitize(Preferences& value);
VisualSettings visualSettingsFromPreferences(const Preferences& value);
bool readPreferences(std::istream& input,Preferences& result);
void writePreferences(std::ostream& output,const Preferences& value);
bool loadPreferences(const std::filesystem::path& path,Preferences& result,std::string& error);
bool savePreferences(const std::filesystem::path& path,const Preferences& value,std::string& error);

}
