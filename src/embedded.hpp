#pragma once
#include "common.hpp"
#include <string>
namespace mc {
inline std::string embeddedText(int identifier){
    HMODULE module=GetModuleHandleW(nullptr);auto resource=FindResourceW(module,MAKEINTRESOURCEW(identifier),RT_RCDATA);
    if(!resource)return {};
    auto handle=LoadResource(module,resource);auto bytes=SizeofResource(module,resource);auto data=static_cast<const char*>(LockResource(handle));
    return data?std::string(data,bytes):std::string{};
}
inline std::wstring embeddedWideText(int identifier){
    auto text=embeddedText(identifier);int length=MultiByteToWideChar(CP_UTF8,0,text.data(),int(text.size()),nullptr,0);std::wstring result(length,L'\0');
    if(length)MultiByteToWideChar(CP_UTF8,0,text.data(),int(text.size()),result.data(),length);
    return result;
}
inline std::wstring embeddedLicenseText(){return embeddedWideText(102);}
inline std::wstring embeddedProjectNotices(){return embeddedWideText(103);}
inline std::wstring embeddedProjectLicense(){return embeddedWideText(104);}

}
