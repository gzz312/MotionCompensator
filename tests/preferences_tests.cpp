#include "preferences.hpp"
#include "reticle.hpp"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <sstream>
#include <iostream>
#include <fstream>
#include <chrono>
#include <limits>
#include <cstdlib>
void require(bool ok,const char* why){if(!ok){std::cerr<<"FAIL: "<<why<<'\n';std::exit(1);}}
int main() {
    mc::Preferences input;input.selected=1;input.theme=2;input.chinese=false;input.reticleSize=12;
    input.profiles[0].staggered=true;input.profiles[2].staggered=true;input.profiles[0].spacing=133;input.profiles[0].boundaryFade=false;
    input.profiles[1].radius=64;input.profiles[1].edgeWidth=240;input.profiles[1].edges=2;input.profiles[1].inverted=true;input.profiles[1].gain=1.75;
    input.profiles[0].appearance=0;input.profiles[1].appearance=2;input.profiles[2].appearance=3;
    input.profiles[2].color={.9f,.2f,.3f};input.profiles[2].opacity=.45;
    std::stringstream file;mc::writePreferences(file,input);auto text=file.str();mc::Preferences output;
    require(mc::readPreferences(file,output) && input==output,"all three profiles, active style and shared reticle round trip independently");
    for(int shape=0;shape<mc::ReticleShapeCount;++shape){
        input.reticleShape=shape;std::stringstream saved;mc::writePreferences(saved,input);mc::Preferences reloaded;
        require(mc::readPreferences(saved,reloaded)&&reloaded==input,"all reticle IDs survive settings round trip without changing other preferences");
    }
    std::stringstream legacy("MotionCompensatorSettings 1\ncommon 0 1 1 1 1 1 16 .2 1 .45\nprofile 0 3 1 122 .1 140 .81 1 1 1 1 1 0 0\nprofile 1 18 .75 140 .1 256 1 0 1 0 0 0 0 0\nprofile 2 5 1 240 .1 140 1 1 1 1 1 0 0 0\n");
    mc::Preferences migrated;require(mc::readPreferences(legacy,migrated),"version 1 upgrades without losing user settings");
    require(migrated.reticleSize==16 && migrated.profiles[0].gain==.81f && migrated.profiles[2].radius==5 && !migrated.profiles[0].enhancement && migrated.profiles[1].enhancement && !migrated.profiles[2].enhancement,"old enhancement choices are replaced by fixed per-style behavior");
    require(!migrated.profiles[0].staggered && !migrated.profiles[1].staggered && !migrated.profiles[2].staggered,"migration preserves square baseline");
    require(output.profiles[1].radius==64 && mc::dotRadiusMax(1)==64 && mc::dotRadiusMax(0)==24 && mc::dotRadiusMax(2)==24,"edge-only expanded size range survives saving");
    require(output.theme==2,"theme choice persists independently of visuals");
    std::stringstream legacy3("MotionCompensatorSettings 3\ncommon 0 1 1 1 1 1 16 .2 1 .45\nprofile 0 3 1 122 .1 140 1 1 1 1 0 1 0 0 1 3\nprofile 1 30 .75 150 .1 300 1 0 1 0 1 0 0 0 1 1\nprofile 2 5 1 240 .1 140 1 1 1 1 0 0 0 0 0 1\n");
    mc::Preferences upgraded;require(mc::readPreferences(legacy3,upgraded)&&upgraded.theme==0&&upgraded.profiles[0].appearance==3&&upgraded.profiles[1].radius==30,"V3 migrates without changing game visuals; theme follows system");
    auto saved=output;std::stringstream truncated(text.substr(0,text.size()/2));
    require(!mc::readPreferences(truncated,output) && output==saved,"truncated settings cannot partially overwrite a profile");
    auto invalid=text;invalid.replace(invalid.find("profile 1"),9,"profile 0");std::stringstream duplicate(invalid);
    require(!mc::readPreferences(duplicate,output) && output==saved,"duplicate profile rejected transactionally");
    std::stringstream future("MotionCompensatorSettings 999\n");require(!mc::readPreferences(future,output),"unsupported version rejected");
    input.profiles[0].radius=999;input.profiles[1].gain=std::numeric_limits<float>::quiet_NaN();input.profiles[2].opacity=-1;input.selected=99;
    mc::sanitize(input);require(input.profiles[0].radius==24 && input.profiles[1].gain==1 && input.profiles[2].opacity==0 && input.selected==2,"invalid numeric settings are bounded");
    auto path=std::filesystem::current_path()/("preferences-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".ini");
    std::string error;require(mc::savePreferences(path,saved,error),"initial atomic file save");
    mc::Preferences loaded;require(mc::loadPreferences(path,loaded,error) && loaded==saved,"disk reload keeps all independent values");
    saved.selected=2;require(mc::savePreferences(path,saved,error),"replace existing file atomically");
    require(mc::loadPreferences(path,loaded,error) && loaded==saved,"second disk reload keeps last selected style");
    {std::ofstream bad(path);bad<<"damaged";}
    auto before=loaded;require(!mc::loadPreferences(path,loaded,error) && loaded==before && !error.empty(),"damaged disk file reports an error without partial state");
    std::filesystem::remove(path);
    require(!mc::savePreferences(path/"missing"/"settings.ini",saved,error) && !error.empty(),"write failure is reported");
    std::wstring registryKey=L"Software\\MotionCompensatorTest-"+std::to_wstring(GetCurrentProcessId());
    bool found=false;mc::Preferences registryLoaded;
    require(mc::loadUserPreferences(registryLoaded,found,error,registryKey.c_str()) && !found,"missing user settings preserve embedded defaults");
    bool registrySaved=mc::saveUserPreferences(saved,error,registryKey.c_str());
    if(!registrySaved && error=="Cannot save user settings: 5")std::cout<<"SKIP: registry writes denied on build host; verify persistence in desktop runtime\n";
    else {
        require(registrySaved,"single-exe settings save to isolated user key");
        require(mc::loadUserPreferences(registryLoaded,found,error,registryKey.c_str()) && found && registryLoaded==saved,"user settings survive reload without a sidecar file");
        require(RegDeleteKeyW(HKEY_CURRENT_USER,registryKey.c_str())==ERROR_SUCCESS,"temporary settings test key cleaned up");
    }
    std::cout<<"PASS: independent profiles, shared state, corruption, clamping, atomic save/replace and restart-style reload\n";
}
