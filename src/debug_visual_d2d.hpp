#pragma once
#include "debug_visual.hpp"
#include <d2d1.h>
#include <dwrite.h>
inline void paintDebugScene(ID2D1RenderTarget* target,ID2D1SolidColorBrush* brush,IDWriteTextFormat* font,const mc::DebugScene& scene) {
    for(const auto& r:scene.rects){brush->SetColor(D2D1::ColorF(r.color.r,r.color.g,r.color.b,r.alpha));target->FillRectangle(D2D1::RectF(r.x,r.y,r.x+r.w,r.y+r.h),brush);}
    for(const auto& l:scene.strokes){brush->SetColor(D2D1::ColorF(l.color.r,l.color.g,l.color.b,1));target->DrawLine(D2D1::Point2F(l.a.x,l.a.y),D2D1::Point2F(l.b.x,l.b.y),brush,l.width);}
    for(const auto& t:scene.labels){brush->SetColor(D2D1::ColorF(t.color.r,t.color.g,t.color.b,1));target->DrawText(t.text.c_str(),UINT32(t.text.size()),font,D2D1::RectF(t.x,t.y,scene.width-8,t.y+18),brush,D2D1_DRAW_TEXT_OPTIONS_CLIP);}
}
