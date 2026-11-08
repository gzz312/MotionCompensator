#include "dots.hpp"
#include "preview.hpp"
#include "debug_panel.hpp"
#include "dynamic.hpp"
#include <iostream>
#include <cstdlib>
void require(bool ok,const char* why){if(!ok){std::cerr<<"FAIL: "<<why<<'\n';std::exit(1);}}
int main(){
    require(mc::rowShift(-1,100,true)==50 && mc::rowShift(-2,100,true)==0,"negative row parity remains globally anchored");
    require(mc::verticalPeriod(100,true)==200 && mc::verticalPeriod(100,false)==100,"staggered grid repeats after two rows");
    for(float direction: {-1.f,1.f}) {
        mc::RegionalDotField square,stagger;std::array<mc::Vec2,9> v;v.fill({170*direction,237*direction});
        auto previous=stagger.update(v,1200,900,100,0,true,false,false,3,true);
        for(int frame=0;frame<1800;++frame){
            square.update(v,1200,900,100,.016f,true,false,false,3,false);
            auto next=stagger.update(v,1200,900,100,.016f,true,false,false,3,true);
            for(int r=0;r<9;++r){
                require(std::abs(std::remainder(square.phases()[r].x-stagger.phases()[r].x,100))<.02f && std::abs(std::remainder(square.phases()[r].y-stagger.phases()[r].y,100))<.02f,"layout cannot change sustained speed or direction");
            }
            for(auto dot:previous){
                float x=dot.position.x+170*direction*.016f,y=dot.position.y+237*direction*.016f;
                float left=(dot.region%3)*400.f,top=(dot.region/3)*300.f;
                if(x<left+10 || x>left+390 || y<top+10 || y>top+290)continue;
                float nearest=1e9f;for(auto other:next)if(other.region==dot.region)nearest=std::min(nearest,std::hypot(x-other.position.x,y-other.position.y));
                require(nearest<.03f,"interior dots remain continuous across positive and negative wrap, including row parity");
            }
            previous=next;
        }
        auto phase=stagger.phases();stagger.update(v,1200,900,100,0,true,false,false,3,false);
        require(stagger.phases()[0].x==phase[0].x && std::abs(std::remainder(stagger.phases()[0].y-phase[0].y,100))<.001,"toggle retains equivalent accumulated phase");
    }
    mc::VisualSettings settings;auto plain=mc::buildPreview(settings);settings.staggered=true;auto stagger=mc::buildPreview(settings);
    bool different=plain.dots.size()!=stagger.dots.size();for(size_t i=0;i<std::min(plain.dots.size(),stagger.dots.size());++i)different|=plain.dots[i].position.x!=stagger.dots[i].position.x;
    require(different,"preview reflects selected layout");
    mc::EdgeDotField edge;settings.style=1;std::array<mc::Vec2,9> v;v.fill({0,600});
    for(int frame=0;frame<900;++frame){auto dots=edge.update(v,1200,900,settings,.016f);require(!dots.empty(),"staggered edge dots remain populated");for(auto dot:dots)require(dot.opacity>0 && std::isfinite(dot.position.x),"staggered edge geometry remains finite");}
    mc::RegionalDotField still;auto staticDots=still.update(v,1200,900,100,.016f,false,false,false,3,true);require(!staticDots.empty() && still.phaseMax()==0,"staggered static remains static");
    mc::DebugSnapshot d;auto text=mc::formatDebug(d);require(text.find(L"N/A")!=std::wstring::npos && text.find(L"Square")!=std::wstring::npos,"missing debug measurements stay N/A");
    d.staggered=true;d.now=10;d.flow.timestamp=9.98;d.flow.confidence=.8f;d.flow.velocity[0]={123,45};d.compensation[0]={-30,20};text=mc::formatDebug(d);
    require(text.find(L"Staggered")!=std::wstring::npos && text.find(L"123.0")!=std::wstring::npos && text.find(L"-30.0")!=std::wstring::npos,"debug reports real regional observations and final compensation");
    mc::DynamicDriver driver;mc::InputState input;input.timestamp=10;input.mouseRate={100,0};mc::MotionSample flow;
    driver.update(flow,input,10,.016,true,true);require(driver.prediction()[0].x==-140 && driver.fused()[0].x==-140,"debug stages expose actual input and fused values");
    driver.update(flow,input,10.016,.016,false,true);require(driver.prediction()[0].x==0 && driver.fused()[0].x==0,"suspended stages clear stale vectors");
    std::cout<<"PASS: two-row wrap continuity, signed travel, unchanged velocity, preview, edge/static layouts and honest debug data\n";
}
