#pragma once
#include "common.hpp"
#include "dynamic.hpp"
#include <xinput.h>
class InputObserver {
    using GetState=DWORD(WINAPI*)(DWORD,XINPUT_STATE*);
    HMODULE dll_{};GetState getState_{};
    HWND target_{};bool active_=false;
    double dx_=0,dy_=0,nextProbe_=0;
    mc::Vec2 smoothed_{};
    uint16_t mouseButtons_=0;
    float wheel_=0;
    HANDLE absoluteDevice_{};LONG absoluteX_=0,absoluteY_=0;bool absoluteKnown_=false;
    std::array<bool,256> keys_{};
    std::array<bool,4> connected_{};
public:
    uint64_t mouseEvents=0,keyEvents=0,padChanges=0;
    double lastEvent=0;
    bool rawReady=false;
    DWORD registrationError=0;
    std::array<DWORD,4> packets_{};
    InputObserver();
    ~InputObserver();
    void initialize(HWND window);
    void clear();
    void setTarget(HWND target,bool active);
    void onRaw(HRAWINPUT handle);
    mc::InputState poll(double now,float dt);
    bool padApiAvailable() const {return getState_!=nullptr;}
};
