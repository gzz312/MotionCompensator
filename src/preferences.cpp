#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "preferences.hpp"
#include <fstream>
#include <iomanip>
#include <sstream>
namespace mc {
void sanitize(Preferences& v) {
    auto bound=[](float x,float lo,float hi,float fallback){return std::clamp(std::isfinite(x)?x:fallback,lo,hi);};
    auto color=[&](Color& c){c.r=bound(c.r,0,1,1);c.g=bound(c.g,0,1,1);c.b=bound(c.b,0,1,1);};
    v.theme=std::clamp(v.theme,0,2);v.selected=std::clamp(v.selected,0,2);v.reticleShape=std::clamp(v.reticleShape,0,2);
    v.reticleOpacity=bound(v.reticleOpacity,0,1,1);v.reticleSize=bound(v.reticleSize,ReticleSizeMin,ReticleSizeMax,10);color(v.reticleColor);
    for(size_t i=0;i<v.profiles.size();++i) {
        auto& p=v.profiles[i];p.enhancement=i==1;
        p.radius=bound(p.radius,1,float(dotRadiusMax(int(i))),i==1?24.f:3.f);p.opacity=bound(p.opacity,0,1,1);p.spacing=bound(p.spacing,60,240,122);
        p.hole=bound(p.hole,.05f,.25f,.1f);p.edgeWidth=bound(p.edgeWidth,40,320,140);p.gain=bound(p.gain,.25f,2,1);
        p.appearance=std::clamp(p.appearance,0,3);p.edges=std::clamp(p.edges,0,2);color(p.color);
    }
}
VisualSettings visualSettingsFromPreferences(const Preferences& input) {
    auto value=input;sanitize(value);const auto& p=value.profiles[value.selected];VisualSettings v;
    v.style=value.selected;v.radius=p.radius;v.opacity=p.opacity;v.spacing=p.spacing;v.holeDiameter=p.hole;v.edgeWidth=p.edgeWidth;v.edges=p.edges;
    v.appearance=p.appearance;v.dots=p.color;v.enhancement=p.enhancement;v.boundaryFade=p.boundaryFade;v.staggered=p.staggered;
    v.reticleShape=value.reticleShape;v.reticleSize=value.reticleSize;v.reticleOpacity=value.reticleOpacity;v.reticle=value.reticleColor;
    return v;
}
void writePreferences(std::ostream& out,const Preferences& input) {
    auto v=input;sanitize(v);out<<std::setprecision(9)<<"MotionCompensatorSettings 4\n";
    out<<"common "<<v.selected<<' '<<v.chinese<<' '<<v.visible<<' '<<v.automatic<<' '<<v.reticleShape<<' '<<v.reticleOpacity<<' '<<v.reticleSize<<' '<<v.reticleColor.r<<' '<<v.reticleColor.g<<' '<<v.reticleColor.b<<' '<<v.theme<<'\n';
    for(size_t i=0;i<3;++i) {auto p=v.profiles[i];out<<"profile "<<i<<' '<<p.radius<<' '<<p.opacity<<' '<<p.spacing<<' '<<p.hole<<' '<<p.edgeWidth<<' '<<p.gain<<' '<<p.color.r<<' '<<p.color.g<<' '<<p.color.b<<' '<<p.enhancement<<' '<<p.boundaryFade<<' '<<p.inverted<<' '<<p.edges<<' '<<p.staggered<<' '<<p.appearance<<'\n';}
}
bool readPreferences(std::istream& in,Preferences& result) {
    Preferences next;std::string key;int version=0;
    if(!(in>>key>>version) || key!="MotionCompensatorSettings" || (version!=1 && version!=2 && version!=3 && version!=4)) return false;
    if(version==1)for(auto& p:next.profiles)p.staggered=false;
    if(!(in>>key) || key!="common" || !(in>>next.selected>>next.chinese>>next.visible>>next.automatic>>next.reticleShape>>next.reticleOpacity>>next.reticleSize>>next.reticleColor.r>>next.reticleColor.g>>next.reticleColor.b)) return false;
    if(version>=4 && !(in>>next.theme))return false;
    std::array<bool,3> seen{};
    for(int n=0;n<3;++n) {
        int i=-1;if(!(in>>key>>i) || key!="profile" || i<0 || i>2 || seen[i]) return false;
        seen[i]=true;auto& p=next.profiles[i];
        if(!(in>>p.radius>>p.opacity>>p.spacing>>p.hole>>p.edgeWidth>>p.gain>>p.color.r>>p.color.g>>p.color.b>>p.enhancement>>p.boundaryFade>>p.inverted>>p.edges)) return false;
        if(version>=2 && !(in>>p.staggered)) return false;
        if(version>=3 && !(in>>p.appearance)) return false;
    }
    in>>std::ws;if(!in.eof()) return false;
    sanitize(next);result=next;return true;
}
bool loadPreferences(const std::filesystem::path& path,Preferences& result,std::string& error) {
    error.clear();std::error_code ec;
    if(!std::filesystem::exists(path,ec)) {if(ec) error=ec.message();return !ec;}
    auto bytes=std::filesystem::file_size(path,ec);if(ec || bytes>16384) {error="Invalid settings file size";return false;}
    std::ifstream input(path);if(!input || !readPreferences(input,result)) {error="Settings format is invalid";return false;}return true;
}
bool savePreferences(const std::filesystem::path& path,const Preferences& value,std::string& error) {
    error.clear();auto temporary=path;temporary+=L"."+std::to_wstring(GetCurrentProcessId())+L".tmp";
    {std::ofstream out(temporary,std::ios::trunc);if(!out) {error="Cannot open settings temporary file";return false;}writePreferences(out,value);out.flush();if(!out) {error="Cannot write settings";return false;}}
    if(!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) {
        error="Cannot replace settings file: "+std::to_string(GetLastError());std::error_code ec;std::filesystem::remove(temporary,ec);return false;
    }
    return true;
}
bool loadUserPreferences(Preferences& result,bool& found,std::string& error,const wchar_t* key){
    found=false;error.clear();HKEY handle{};auto status=RegOpenKeyExW(HKEY_CURRENT_USER,key,0,KEY_QUERY_VALUE,&handle);
    if(status==ERROR_FILE_NOT_FOUND)return true;
    if(status!=ERROR_SUCCESS){error="Cannot open user settings: "+std::to_string(status);return false;}
    DWORD type=0,bytes=0;const wchar_t* name=L"PreferencesV4";
    status=RegQueryValueExW(handle,name,nullptr,&type,nullptr,&bytes);
    if(status==ERROR_FILE_NOT_FOUND){name=L"PreferencesV3";status=RegQueryValueExW(handle,name,nullptr,&type,nullptr,&bytes);}
    if(status==ERROR_FILE_NOT_FOUND){name=L"Preferences";status=RegQueryValueExW(handle,name,nullptr,&type,nullptr,&bytes);}
    if(status==ERROR_FILE_NOT_FOUND){RegCloseKey(handle);return true;}
    if(status!=ERROR_SUCCESS || type!=REG_BINARY || bytes==0 || bytes>16384){RegCloseKey(handle);error="Invalid user settings value";return false;}
    std::string data(bytes,'\0');status=RegQueryValueExW(handle,name,nullptr,&type,reinterpret_cast<BYTE*>(data.data()),&bytes);RegCloseKey(handle);
    if(status!=ERROR_SUCCESS){error="Cannot read user settings";return false;}
    std::istringstream input(data);if(!readPreferences(input,result)){error="Invalid user settings format";return false;}
    found=true;return true;
}
bool saveUserPreferences(const Preferences& value,std::string& error,const wchar_t* key){
    error.clear();std::ostringstream output;writePreferences(output,value);auto data=output.str();HKEY handle{};
    auto status=RegCreateKeyExW(HKEY_CURRENT_USER,key,0,nullptr,0,KEY_SET_VALUE,nullptr,&handle,nullptr);
    if(status==ERROR_SUCCESS){status=RegSetValueExW(handle,L"PreferencesV4",0,REG_BINARY,reinterpret_cast<const BYTE*>(data.data()),DWORD(data.size()));RegCloseKey(handle);}
    if(status!=ERROR_SUCCESS){error="Cannot save user settings: "+std::to_string(status);return false;}
    return true;
}

}
