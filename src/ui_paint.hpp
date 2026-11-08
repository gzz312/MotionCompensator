#pragma once
#include "common.hpp"
#include <vector>
class UiPaintBuffer {
    HDC target_,dc_;HBITMAP bitmap_{};HGDIOBJ old_{};int state_=0;RECT bounds_;bool presented_=false;
public:
    UiPaintBuffer(HDC target,RECT bounds):target_(target),dc_(CreateCompatibleDC(target)),bounds_(bounds){
        if(dc_)bitmap_=CreateCompatibleBitmap(target,std::max(1L,bounds.right),std::max(1L,bounds.bottom));
        if(bitmap_)old_=SelectObject(dc_,bitmap_);else {if(dc_)DeleteDC(dc_);dc_=target_;}
        state_=SaveDC(dc_);
    }
    HDC dc() const{return dc_;}
    void present(){if(presented_)return;RestoreDC(dc_,state_);if(bitmap_)BitBlt(target_,0,0,bounds_.right,bounds_.bottom,dc_,0,0,SRCCOPY);presented_=true;}
    ~UiPaintBuffer(){if(!presented_)RestoreDC(dc_,state_);if(bitmap_){SelectObject(dc_,old_);DeleteObject(bitmap_);DeleteDC(dc_);}}
};

// Move the children as one layout transaction, without native copy/erase paths.
// The caller invalidates the completed layout once after all fonts are assigned.
class UiLayoutBatch {
    struct Placement{HWND window;int x,y,w,h;};std::vector<Placement> placements_;
public:
    UiLayoutBatch(HWND,int count){placements_.reserve(count);}
    ~UiLayoutBatch(){commit();}
    void place(HWND child,int x,int y,int width,int height){if(child)placements_.push_back({child,x,y,width,height});}
    void commit(){
        if(placements_.empty())return;
        UINT flags=SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOREDRAW|SWP_NOCOPYBITS;
        HDWP batch=BeginDeferWindowPos(int(placements_.size()));
        for(auto p:placements_){if(!batch)break;batch=DeferWindowPos(batch,p.window,nullptr,p.x,p.y,p.w,p.h,flags);}
        bool success=batch&&EndDeferWindowPos(batch);
        if(!success)for(auto p:placements_)SetWindowPos(p.window,nullptr,p.x,p.y,p.w,p.h,flags);
        placements_.clear();
    }
};

