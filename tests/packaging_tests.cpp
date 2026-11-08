#define NOMINMAX
#include <windows.h>
#include "preferences.hpp"
#include <sstream>
#include <iostream>
#include <cstdlib>
void require(bool ok,const char* why){if(!ok){std::cerr<<"FAIL: "<<why<<'\n';std::exit(1);}}
int main(){
    HMODULE module=LoadLibraryExW(L"MotionCompensator.exe",nullptr,LOAD_LIBRARY_AS_DATAFILE|LOAD_LIBRARY_AS_IMAGE_RESOURCE);require(module!=nullptr,"release executable loads as resources without executing");
    auto text=[&](int id){auto resource=FindResourceW(module,MAKEINTRESOURCEW(id),MAKEINTRESOURCEW(10));require(resource!=nullptr,"required resource exists");auto handle=LoadResource(module,resource);auto data=static_cast<const char*>(LockResource(handle));return std::string(data,SizeofResource(module,resource));};
    require(FindResourceW(module,MAKEINTRESOURCEW(1),MAKEINTRESOURCEW(24))!=nullptr,"modern common-controls manifest is embedded");
    auto preset=text(101);std::istringstream input(preset);mc::Preferences prefs;require(mc::readPreferences(input,prefs),"embedded initial settings parse using production settings reader");
    require(prefs.profiles[1].radius==30 && prefs.profiles[1].spacing==150 && prefs.profiles[1].edgeWidth==300,"embedded settings preserve user's current visual values");
    auto licenses=text(102);require(licenses.size()>10000 && licenses.find("Apache License")!=std::string::npos,"dependency license text is embedded in the executable");
    auto notices=text(103);require(notices.find("BV1saKg6XEvg")!=std::string::npos&&notices.find("MotionStabilizer")!=std::string::npos&&notices.find("LicenseRef-MotionCompensator-Integration-1.0")!=std::string::npos,"preview status and agreed design credits are embedded");
    auto projectLicense=text(104);require(projectLicense.find("LicenseRef-MotionCompensator-Integration-1.0")!=std::string::npos && projectLicense.find("Standalone sale")!=std::string::npos && projectLicense.find("Commercial integration")!=std::string::npos,"complete project license and commercial integration restrictions are embedded");
    require(licenses.find("GCC RUNTIME LIBRARY EXCEPTION")!=std::string::npos&&licenses.find("winpthreads")!=std::string::npos,"static runtime notices are embedded");
    FreeLibrary(module);std::cout<<"PASS: single-exe preset, latest user visual values and bundled third-party notices\n";
}
