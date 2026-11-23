#include "input.hpp"
#include <cstring>
InputObserver::InputObserver() {
    dll_=LoadLibraryExW(L"xinput1_4.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!dll_) dll_=LoadLibraryExW(L"xinput9_1_0.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(dll_) {
        auto symbol=GetProcAddress(dll_,"XInputGetState");
        static_assert(sizeof(symbol)==sizeof(getState_));
        std::memcpy(&getState_,&symbol,sizeof(symbol));
    }
}
InputObserver::~InputObserver() {if(dll_) FreeLibrary(dll_);}
void InputObserver::initialize(HWND window) {
    RAWINPUTDEVICE devices[]={{1,2,RIDEV_INPUTSINK|RIDEV_DEVNOTIFY,window},{1,6,RIDEV_INPUTSINK|RIDEV_DEVNOTIFY,window}};
    // Observation only: no NOLEGACY, capture, hooks, input injection or rumble.
    rawReady=RegisterRawInputDevices(devices,2,sizeof(devices[0]))!=FALSE;
    if(!rawReady) registrationError=GetLastError();
}
void InputObserver::clear() {dx_=dy_=0;smoothed_={};absoluteKnown_=false;mouseButtons_=0;}
void InputObserver::setTarget(HWND target,bool active) {
    if(target!=target_ || active!=active_) clear();
    target_=target;active_=active;
}
std::optional<mc::RawKey> InputObserver::onRaw(HRAWINPUT handle,bool observeMenuKeys,const mc::MovementBindings& bindings) {
    if(!active_ || !target_ || GetForegroundWindow()!=target_) return std::nullopt;
    RAWINPUT raw{};UINT size=sizeof(raw);
    UINT read=GetRawInputData(handle,RID_INPUT,&raw,&size,sizeof(RAWINPUTHEADER));
    if(read==UINT(-1) || read<sizeof(RAWINPUTHEADER)) return std::nullopt;
    if(raw.header.dwType==RIM_TYPEMOUSE) {
        const auto& mouse=raw.data.mouse;
        if(mouse.lLastX==0&&mouse.lLastY==0&&(mouse.usButtonFlags&0x03ff)==0)return std::nullopt;
        ++mouseEvents;lastEvent=qpcSeconds();
        for(int button=0;button<5;++button) {
            if(mouse.usButtonFlags&(1<<(button*2))) mouseButtons_|=uint16_t(1<<button);
            if(mouse.usButtonFlags&(2<<(button*2))) mouseButtons_&=uint16_t(~(1<<button));
        }
        if(mouse.usFlags&MOUSE_MOVE_ABSOLUTE) {
            if(absoluteKnown_ && absoluteDevice_==raw.header.hDevice) {
                dx_+=(mouse.lLastX-absoluteX_)*GetSystemMetrics(SM_CXVIRTUALSCREEN)/65535.;
                dy_+=(mouse.lLastY-absoluteY_)*GetSystemMetrics(SM_CYVIRTUALSCREEN)/65535.;
            }
            absoluteX_=mouse.lLastX;absoluteY_=mouse.lLastY;absoluteDevice_=raw.header.hDevice;absoluteKnown_=true;
        } else {dx_+=mouse.lLastX;dy_+=mouse.lLastY;absoluteKnown_=false;}
    } else if(raw.header.dwType==RIM_TYPEKEYBOARD) {
        const auto& keyboard=raw.data.keyboard;
        const auto key=mc::physicalKey(keyboard.VKey,keyboard.MakeCode,(keyboard.Flags&RI_KEY_E0)!=0);
        if(key>0&&key<255){
            if(mc::movementUsesKey(bindings,key)){++keyEvents;lastEvent=qpcSeconds();}
            if(observeMenuKeys)return mc::RawKey{uint16_t(key),(keyboard.Flags&RI_KEY_BREAK)==0};
        }
    }
    return std::nullopt;
}
mc::InputState InputObserver::poll(double now,float dt,const mc::MovementBindings& bindings) {
    mc::InputState out;out.timestamp=now;
    if(!active_ || !target_ || GetForegroundWindow()!=target_) {clear();return out;}
    float seconds=std::clamp(std::isfinite(dt)?dt:0.f,.004f,.1f);
    float alpha=1-std::exp(-seconds/.025f);
    smoothed_.x+=alpha*(float(dx_)/seconds-smoothed_.x);smoothed_.y+=alpha*(float(dy_)/seconds-smoothed_.y);dx_=dy_=0;
    if(std::abs(smoothed_.x)<.5f) smoothed_.x=0;
    if(std::abs(smoothed_.y)<.5f) smoothed_.y=0;
    out.mouseRate=smoothed_;out.mouse=std::hypot(smoothed_.x,smoothed_.y)>1;
    out.mouseButtons=mouseButtons_;
    // Resync held movement keys after focus changes or missed break messages.
    using mc::MoveAction;
    auto down=[&](MoveAction action){return mc::movementPressed(bindings,size_t(action));};
    out.sprint=down(MoveAction::Sprint);out.crouch=down(MoveAction::Crouch);out.jump=down(MoveAction::Jump);
    out.prone=down(MoveAction::Prone);
    out.move={float(down(MoveAction::Right))-float(down(MoveAction::Left)),
              float(down(MoveAction::Forward))-float(down(MoveAction::Back))};
    out.keyboard=out.move.x!=0 || out.move.y!=0;
    float length=std::hypot(out.move.x,out.move.y);if(length>1) {out.move.x/=length;out.move.y/=length;}
    float best=-1;bool probe=now>=nextProbe_;
    if(getState_) for(DWORD slot=0;slot<4;++slot) if(connected_[slot] || probe) {
        XINPUT_STATE state{};
        if(getState_(slot,&state)!=ERROR_SUCCESS) {connected_[slot]=false;continue;}
        connected_[slot]=true;
        if(state.dwPacketNumber!=packets_[slot]) {++padChanges;packets_[slot]=state.dwPacketNumber;lastEvent=now;}
        const auto& g=state.Gamepad;
        mc::Vec2 move{mc::stickAxis(g.sThumbLX,XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE),mc::stickAxis(g.sThumbLY,XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE)};
        mc::Vec2 look{mc::stickAxis(g.sThumbRX,XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE),-mc::stickAxis(g.sThumbRY,XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE)};
        float score=std::hypot(move.x,move.y)+std::hypot(look.x,look.y);
        out.pad=true;
        if(score>best) {
            best=score;out.look=look;if(!out.keyboard) out.move=move;
            out.padButtons=g.wButtons;out.triggers={g.bLeftTrigger/255.f,g.bRightTrigger/255.f};
        }
    }
    if(probe) nextProbe_=now+1;
    return out;
}
