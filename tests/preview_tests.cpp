#include "preview_gdi.hpp"
#include "preferences.hpp"
#include <iostream>
#include <sstream>
#include <cstdlib>
void require(bool ok,const char* why){if(!ok){std::cerr<<"FAIL: "<<why<<'\n';std::exit(1);}}
int main() {
    mc::Preferences prefs;
    require(prefs.profiles[1].radius==24 && prefs.profiles[1].opacity==.75f && prefs.profiles[1].spacing==150 && prefs.profiles[1].edgeWidth==300 && prefs.profiles[1].color==mc::Color{0,1,0},"screen-edge factory defaults match user screenshot");
    require(prefs.profiles[1].staggered && prefs.profiles[1].edges==0,"edge defaults retain staggered rows and left/right selection");
    HDC dc=CreateCompatibleDC(nullptr);BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=324;info.bmiHeader.biHeight=-272;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    void* pixels=nullptr;HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);require(dc&&bitmap&&pixels,"offscreen GDI surface created");auto old=SelectObject(dc,bitmap);
    COLORREF background=RGB(20,29,44);HBRUSH brush=CreateSolidBrush(background);RECT bounds{0,0,324,272};
    for(int style=0;style<3;++style) {
        prefs.selected=style;auto settings=mc::visualSettingsFromPreferences(prefs);auto scene=mc::buildPreview(settings);
        require(!scene.dots.empty() && scene.reticle.size()==4,"first scene includes dots and cross without a user toggle");
        for(auto dot:scene.dots) require(dot.radius>=.75f && dot.opacity>0,"small previews retain visible dot radius");
        FillRect(dc,&bounds,brush);paintPreview(dc,{0,0},scene);GdiFlush();
        int count=0;for(int y=0;y<272;++y)for(int x=0;x<324;++x) if(std::hypot(float(x-162),float(y-136))>30 && GetPixel(dc,x,y)!=background)++count;
        std::cout<<"preview style="<<style<<", dots="<<scene.dots.size()<<", pixels="<<count<<"\n";
        require(count>=(style==2?64:81),"actual GDI pixels contain the selected dot pattern");
        require(GetPixel(dc,164,136)!=background,"startup cross arm is painted before any selection event");
    }

    // Match the actual game-space dot geometry and preserve one common scale.
    prefs.selected=1;prefs.profiles[1].radius=64;
    auto actualSettings=mc::visualSettingsFromPreferences(prefs);mc::EdgeDotField actualField;std::array<mc::Vec2,9> zero{};
    auto actual=actualField.update(zero,2560,1600,actualSettings,0);auto scaled=mc::buildPreview(actualSettings,324,272,2560,1600);
    float scale=324.f/2560,offsetY=(272-1600*scale)*.5f;bool matched=false;
    for(auto a:actual)for(auto b:scaled.dots)if(std::hypot(b.position.x-a.position.x*scale,b.position.y-(offsetY+a.position.y*scale))<.001f){
        require(std::abs(b.radius-std::max(.75f,64*a.radiusScale*scale))<.001f,"large edge dots scale with spacing and band width, not an independent exaggeration");matched=true;}
    require(matched,"preview derives from the production edge field");
    // A larger sentinel surface proves no pixel escapes the preview rectangle,
    // including the static staggered points whose centres sit on a boundary.
    HDC outer=CreateCompatibleDC(nullptr);BITMAPINFO outerInfo=info;outerInfo.bmiHeader.biWidth=364;outerInfo.bmiHeader.biHeight=-312;void* outerPixels=nullptr;
    HBITMAP outerBitmap=CreateDIBSection(outer,&outerInfo,DIB_RGB_COLORS,&outerPixels,nullptr,0);auto outerOld=SelectObject(outer,outerBitmap);
    HBRUSH sentinel=CreateSolidBrush(RGB(211,23,137));RECT outerBounds{0,0,364,312};
    for(int style=0;style<3;++style){auto v=actualSettings;v.style=style;v.radius=style==1?64:24;v.spacing=60;v.staggered=true;FillRect(outer,&outerBounds,sentinel);auto scene=mc::buildPreview(v);paintPreview(outer,{20,20},scene);GdiFlush();
        for(int y=0;y<312;++y)for(int x=0;x<364;++x)if(x<20+int(std::ceil(scene.contentLeft))||x>=20+int(std::floor(scene.contentRight))||y<20+int(std::ceil(scene.contentTop))||y>=20+int(std::floor(scene.contentBottom)))require(GetPixel(outer,x,y)==RGB(211,23,137),"all three styles are strictly clipped to their preview box");
    }
    SelectObject(outer,outerOld);DeleteObject(outerBitmap);DeleteObject(sentinel);DeleteDC(outer);
    prefs.reticleSize=64;std::stringstream file;mc::writePreferences(file,prefs);mc::Preferences loaded;
    require(mc::readPreferences(file,loaded) && loaded.reticleSize==64,"expanded crosshair size survives saving and reload");
    auto large=mc::buildPreview(mc::visualSettingsFromPreferences(loaded),324,272,324,272);
    require(large.reticle[0].from.x==98,"large crosshair preview uses expanded arm length");
    prefs.reticleShape=0;auto off=mc::buildPreview(mc::visualSettingsFromPreferences(prefs));require(off.reticle.empty(),"saved off setting remains off");
    prefs.reticleShape=2;require(mc::buildPreview(mc::visualSettingsFromPreferences(prefs)).reticle.size()==8,"diamond cross remains available");
    prefs.selected=0;prefs.profiles[0].opacity=0;auto hidden=mc::buildPreview(mc::visualSettingsFromPreferences(prefs));require(hidden.dots.empty(),"intentional full transparency remains respected");
    prefs.reticleSize=999;mc::sanitize(prefs);require(prefs.reticleSize==mc::ReticleSizeMax,"UI and settings use one common reticle maximum");
    SelectObject(dc,old);DeleteObject(bitmap);DeleteObject(brush);DeleteDC(dc);
    std::cout<<"PASS: three preview modes, offscreen GDI pixels, startup cross, saved off/diamond and 4-64 size persistence\n";
}
