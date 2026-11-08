#pragma once
#include <cmath>
namespace mc {
inline float rowShift(int row,float spacing,bool staggered) {
    return staggered && row%2!=0 ? spacing*.5f : 0.f;
}
inline float verticalPeriod(float spacing,bool staggered) {return spacing*(staggered?2.f:1.f);}
// Integer row identity is anchored to the whole surface, including negative
// offscreen rows. A two-row vertical period preserves stagger parity on wrap.
template<class Emit>
void visitLatticeIndexed(float left,float top,float right,float bottom,float sx,float sy,
                  float originX,float originY,float phaseX,float phaseY,bool staggered,Emit emit) {
    int firstRow=int(std::floor((top-originY-phaseY)/sy))-1;
    int lastRow=int(std::ceil((bottom-originY-phaseY)/sy))+1;
    for(int row=firstRow;row<=lastRow;++row) {
        float y=originY+phaseY+row*sy,shift=rowShift(row,sx,staggered);
        int firstCol=int(std::floor((left-originX-phaseX-shift)/sx))-1;
        int lastCol=int(std::ceil((right-originX-phaseX-shift)/sx))+1;
        for(int col=firstCol;col<=lastCol;++col)emit(originX+phaseX+shift+col*sx,y,row,col);
    }
}
template<class Emit>
void visitLattice(float left,float top,float right,float bottom,float sx,float sy,
                  float originX,float originY,float phaseX,float phaseY,bool staggered,Emit emit) {
    visitLatticeIndexed(left,top,right,bottom,sx,sy,originX,originY,phaseX,phaseY,staggered,[&](float x,float y,int,int){emit(x,y);});
}

}
