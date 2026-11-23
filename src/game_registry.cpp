#include "common.hpp"
#include "game_registry.hpp"
#include <iomanip>
#include <sstream>
#include <locale>

namespace mc {
namespace {
constexpr auto key = L"Software\\MotionCompensator";
constexpr std::array libraryValues{L"GameLibraryV7",L"GameLibraryV6",L"GameLibraryV5",L"GameLibraryV4",L"GameLibraryV3",L"GameLibraryV2",L"GameLibraryV1"};
constexpr DWORD maxBytes = 16 * 1024 * 1024;

// Keep the existing override bit order. Append new visual profiles at the end.
// Global controls and transient state are never visited.
template<class A, class B, class Fn>
void visualFields(A& a, B& b, Fn apply) {
    static_assert(5+VisualStyleCount*12+2<64);
    uint64_t bit = 1;
    auto field = [&](auto& lhs, auto& rhs) {
        apply(bit, lhs, rhs);
        bit <<= 1;
    };
    field(a.selected,b.selected);
    field(a.reticleShape,b.reticleShape);
    field(a.reticleSize,b.reticleSize);
    field(a.reticleOpacity,b.reticleOpacity);
    field(a.reticleColor,b.reticleColor);
    for(size_t i=0;i<a.profiles.size();++i) {
        auto& x=a.profiles[i];
        auto& y=b.profiles[i];
        field(x.radius,y.radius); field(x.opacity,y.opacity); field(x.spacing,y.spacing);
        field(x.hole,y.hole); field(x.edgeWidth,y.edgeWidth); field(x.gain,y.gain);
        field(x.color,y.color); field(x.boundaryFade,y.boundaryFade);
        field(x.inverted,y.inverted); field(x.staggered,y.staggered);
        field(x.edges,y.edges); field(x.appearance,y.appearance);
    }
    field(a.meshDepth,b.meshDepth);
    field(a.meshDepthStrength,b.meshDepthStrength);
}

void writeProfile(std::ostream& out, const Preferences& preferences) {
    std::ostringstream payload;
    payload.imbue(std::locale::classic());
    writePreferences(payload,preferences);
    out << std::quoted(payload.str()) << '\n';
}

bool readProfile(std::istream& in, Preferences& preferences) {
    std::string payload;
    if(!(in>>std::quoted(payload)) || payload.size()>16384)return false;
    std::istringstream profile(payload);
    profile.imbue(std::locale::classic());
    return readPreferences(profile,preferences);
}

std::string toUtf8(const std::wstring& value) {
    if(value.empty())return {};
    const int size=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value.data(),int(value.size()),nullptr,0,nullptr,nullptr);
    if(!size)return {};
    std::string out(size,'\0');
    WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value.data(),int(value.size()),out.data(),size,nullptr,nullptr);
    return out;
}

std::wstring fromUtf8(const std::string& value) {
    if(value.empty())return {};
    const int size=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),int(value.size()),nullptr,0);
    if(!size)return {};
    std::wstring out(size,L'\0');
    MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),int(value.size()),out.data(),size);
    return out;
}

bool readMenus(std::istream& in,std::vector<MenuRule>& rules) {
    size_t count{};
    if(!(in>>count)||count>maxBytes/16)return false;
    for(size_t i=0;i<count;++i){
        std::string name,opens,returns;
        unsigned effect{};
        MenuRule rule;
        if(!(in>>std::quoted(name)>>std::quoted(opens)>>std::quoted(returns)>>rule.returns>>effect)||
           name.size()>4*maxMenuName||opens.size()>maxChordText||returns.size()>maxChordText)return false;
        rule.name=fromUtf8(name);
        auto openKeys=parseMenuChords(fromUtf8(opens));
        auto returnKeys=parseMenuChords(fromUtf8(returns));
        if(!openKeys||!returnKeys)return false;
        rule.openKeys=std::move(*openKeys);rule.returnKeys=std::move(*returnKeys);
        rule.effect=static_cast<MenuEffect>(effect);
        if(!validMenuRule(rule)||openingConflict(rule,rules))return false;
        rules.push_back(std::move(rule));
    }
    return true;
}

void writeMenus(std::ostream& out,const std::vector<MenuRule>& rules) {
    out<<rules.size()<<'\n';
    for(const auto& rule:rules)
        out<<std::quoted(toUtf8(rule.name))<<' '<<std::quoted(toUtf8(formatMenuChords(rule.openKeys)))<<' '
           <<std::quoted(toUtf8(formatMenuChords(rule.returnKeys)))<<' '
           <<rule.returns<<' '<<static_cast<unsigned>(rule.effect)<<'\n';
}

bool decode(const std::string& data, GameRegistry& output) {
    std::istringstream in(data);
    in.imbue(std::locale::classic());
    std::string signature;
    unsigned version{};
    size_t count{};
    GameRegistry next;
    if(!(in>>signature>>version>>count) || signature!="MotionCompensatorGameLibrary"
       || (version<1||version>7) || count>GameRegistry::MaxGames || !readProfile(in,next.defaults))
        return false;
    if(version>=3){
        for(auto& chord:next.shortcuts)
            if(!(in>>chord)||(chord?!validShortcut(chord):version<4))return false;
        for(size_t i=0;i<next.shortcuts.size();++i)
            for(size_t j=0;j<i;++j)
                if(next.shortcuts[i]&&next.shortcuts[i]==next.shortcuts[j])return false;
    }
    uint64_t allowed{};
    visualFields(next.defaults,next.defaults,[&](uint64_t bit,auto&,auto&){
        const int fieldCount=5+(version>=6?VisualStyleCount:3)*12+(version>=7?2:0);
        if(bit<(uint64_t{1}<<fieldCount))allowed|=bit;
    });
    for(size_t i=0;i<count;++i) {
        std::u8string path;
        std::string utf8;
        GameProfile game;
        if(!(in>>std::quoted(utf8)>>game.overrides) || utf8.size()>4*32767
           || (game.overrides&~allowed) || !readProfile(in,game.values))return false;
        path.assign(utf8.begin(),utf8.end());
        game.path=std::filesystem::path(path).lexically_normal().wstring();
        if(!validGamePath(game.path)||next.find(game.path)>=0)return false;
        if(version>=2&&!readMenus(in,game.menus))return false;
        if(version>=3){
            // Version 3 stored seven actions. New actions remain unbound when importing it.
            const auto count=version==3?size_t(MoveAction::Prone):game.movement.size();
            for(auto& chord:std::span(game.movement).first(count)){
                if(!(in>>chord))return false;
                if(chord&&!(version>=5&&chord==disabledMovement)&&!validChord(chord))return false;
            }
        }
        next.games.push_back(std::move(game));
    }
    in>>std::ws;
    if(!in.eof())return false;
    output=std::move(next);
    return true;
}
}

bool sameExecutable(const std::wstring& first,const std::wstring& second) {
    return !first.empty() && !second.empty() &&
        CompareStringOrdinal(first.c_str(),-1,second.c_str(),-1,TRUE)==CSTR_EQUAL;
}

bool validGamePath(const std::wstring& path) {
    if(path.empty()||path.size()>32767||path.find(L'\0')!=std::wstring::npos)return false;
    const std::filesystem::path file(path);
    return file.is_absolute() && sameExecutable(file.extension().wstring(),L".exe");
}

int GameRegistry::find(const std::wstring& path) const {
    if(path.empty())return -1;
    for(size_t i=0;i<games.size();++i)
        if(sameExecutable(games[i].path,path))return static_cast<int>(i);
    return -1;
}

Preferences GameRegistry::effective(int index,const Preferences& base) const {
    Preferences result=base;
    if(index>=0&&static_cast<size_t>(index)<games.size()) {
        const auto& game=games[index];
        visualFields(result,game.values,[&](uint64_t bit,auto& to,const auto& from){
            if(game.overrides&bit)to=from;
        });
    }
    return result;
}

void GameRegistry::update(int index,const Preferences& edited,const Preferences& before) {
    defaults.chinese=edited.chinese;
    defaults.theme=edited.theme;
    defaults.visible=edited.visible;
    defaults.automatic=edited.automatic;
    if(index<0||static_cast<size_t>(index)>=games.size()) {
        uint64_t changed{};
        visualFields(edited,before,[&](uint64_t bit,const auto& after,const auto& prior){
            if(after!=prior)changed|=bit;
        });
        visualFields(edited,defaults,[&](uint64_t bit,const auto& from,auto& to){
            if(changed&bit)to=from;
        });
        return;
    }
    auto& game=games[index];
    uint64_t changed{};
    visualFields(edited,before,[&](uint64_t bit,const auto& after,const auto& prior){
        if(after!=prior)changed|=bit;
    });
    game.overrides|=changed;
    visualFields(edited,game.values,[&](uint64_t bit,const auto& from,auto& to){
        if(changed&bit)to=from;
    });
}

bool loadGameRegistry(GameRegistry& registry,bool& found,std::string& error) {
    found=false;
    error.clear();
    DWORD size{};
    const DWORD flags=RRF_RT_REG_BINARY;
    const wchar_t* name=nullptr;
    LONG status=ERROR_FILE_NOT_FOUND;
    for(auto value:libraryValues){
        name=value;
        status=RegGetValueW(HKEY_CURRENT_USER,key,name,flags,nullptr,nullptr,&size);
        if(status!=ERROR_FILE_NOT_FOUND)break;
    }
    if(status==ERROR_FILE_NOT_FOUND||status==ERROR_PATH_NOT_FOUND)return true;
    found=true;
    if(status==ERROR_SUCCESS&&size&&size<=maxBytes){
        std::string data(size,'\0');
        status=RegGetValueW(HKEY_CURRENT_USER,key,name,flags,nullptr,data.data(),&size);
        if(status==ERROR_SUCCESS){
            data.resize(size);
            try { if(decode(data,registry))return true; }
            catch(const std::filesystem::filesystem_error&) {}
        }
    }
    error="Game library could not be read; the existing data was preserved";
    return false;
}

bool saveGameRegistry(const GameRegistry& registry,std::string& error) {
    error.clear();
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out<<"MotionCompensatorGameLibrary 7 "<<registry.games.size()<<'\n';
    writeProfile(out,registry.defaults);
    for(auto chord:registry.shortcuts)out<<chord<<' ';
    out<<'\n';
    for(const auto& game:registry.games){
        const auto path=std::filesystem::path(game.path).u8string();
        out<<std::quoted(std::string(path.begin(),path.end()))<<' '<<game.overrides<<'\n';
        writeProfile(out,game.values);
        writeMenus(out,game.menus);
        for(auto chord:game.movement)out<<chord<<' ';
        out<<'\n';
    }
    const auto data=out.str();
    if(data.size()>maxBytes){error="Game library exceeded its storage limit";return false;}
    HKEY settings{};
    auto status=RegCreateKeyExW(HKEY_CURRENT_USER,key,0,nullptr,0,KEY_SET_VALUE,nullptr,&settings,nullptr);
    if(status==ERROR_SUCCESS){
        status=RegSetValueExW(settings,libraryValues.front(),0,REG_BINARY,
            reinterpret_cast<const BYTE*>(data.data()),static_cast<DWORD>(data.size()));
        RegCloseKey(settings);
    }
    if(status==ERROR_SUCCESS)return true;
    error="Cannot save game library: "+std::to_string(status);
    return false;
}
}
