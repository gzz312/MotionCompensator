#include "dots.hpp"
#include <iostream>
#include <limits>
#include <cstdlib>
#include <fstream>
void require(bool ok,const char* why){if(!ok){std::cerr<<"FAIL: "<<why<<'\n';std::exit(1);}}
int main(int argc,char**) {
    if(argc>1){
        std::ofstream out("edge-appearance-frames.json");out<<"[";mc::EdgeDotField field;mc::VisualSettings settings;settings.style=1;settings.radius=14;settings.spacing=100;settings.edgeWidth=160;settings.staggered=true;
        std::array<mc::Vec2,9> velocity;velocity.fill({90,25});
        for(int frame=0;frame<90;++frame){if(frame==45)velocity.fill({-90,-25});auto dots=field.update(velocity,800,500,settings,1.f/30);if(frame)out<<',';out<<"[";bool first=true;for(auto d:dots){if(!first)out<<',';first=false;float radius=settings.radius*d.radiusScale,alpha=d.opacity*.75f*mc::dotVisibility(d.position.x,d.position.y,800,500,radius,.1f);out<<'['<<d.position.x<<','<<d.position.y<<','<<radius<<','<<alpha<<']';}out<<"]";}
        out<<"]";return 0;
    }
    mc::EdgeDotField edge;mc::VisualSettings settings;settings.style=1;settings.radius=9;settings.spacing=140;settings.edgeWidth=140;
    std::array<mc::Vec2,9> field{};field.fill({170,35});float band=132;bool faded=false;
    for(int i=0;i<3600;++i) {
        const auto& dots=edge.update(field,1000,600,settings,1.f/60);
        require(!dots.empty(),"screen-edge dots remain populated through continuous wrapping");
        for(auto d:dots) {
            require(d.position.x>0 && d.position.x<1000 && d.position.y>0 && d.position.y<600,"edge fade rejects centres outside the active strip");
            require(d.position.x<band || d.position.x>1000-band,"side dots never enter the central content");
            require(d.region<2 && std::isfinite(d.opacity) && d.opacity>0 && d.opacity<=1 && d.radiusScale>=.14f && d.radiusScale<=1.201f,"edge appearance stays finite and bounded");
            faded|=d.opacity<.5f;
        }
    }
    require(faded && edge.phaseMax()>0,"rolling edge population fades at zone boundaries");
    settings.edges=2;auto all=edge.update(field,1000,600,settings,.016f);bool top=false,bottom=false;
    for(auto d:all) {top|=d.region==2;bottom|=d.region==3;require(d.position.x<band || d.position.x>1000-band || d.position.y<band || d.position.y>600-band,"four-edge layout preserves centre exclusion");}
    require(top && bottom,"all-edge layout includes top and bottom");
    settings.edges=1;auto horizontal=edge.update(field,1000,600,settings,.016f);
    for(auto d:horizontal) require(d.region>=2,"top/bottom selection omits side strips");
    settings.enhancement=false;auto uniform=edge.update(field,1000,600,settings,0);
    for(auto d:uniform)require(d.radiusScale==1,"disabling size variation restores configured point radius");
    auto original=edge.phaseMax();settings.radius=18;settings.opacity=.2f;edge.update(field,1000,600,settings,0);
    require(edge.phaseMax()==original,"appearance changes never reset edge motion");
    field.fill({std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()});
    for(auto d:edge.update(field,1000,600,settings,.016f))require(std::isfinite(d.position.x)&&std::isfinite(d.position.y),"invalid input does not corrupt edge layout");
    require(edge.update(field,0,0,settings,.016f).empty(),"invalid viewport clears edge dots");

    auto entering=mc::edgeAppearance(8,300,200,600,100,150,0),departing=mc::edgeAppearance(192,300,200,600,100,150,0);
    require(entering.radiusScale>=1.02f && departing.radiusScale>=1.02f && std::abs(entering.radiusScale-departing.radiusScale)<.0001f,"entry and exit size depend on opacity rather than direction");
    auto bodyA=mc::edgeAppearance(40,300,200,600,100,150,0),bodyB=mc::edgeAppearance(130,300,200,600,100,150,0);
    require(bodyA.radiusScale==1.2f && bodyB.radiusScale==1.2f,"most visible travel stays at full reference size");
    require(departing.radiusScale<1.05f && departing.radiusScale>=1.02f,"only the faint tail shrinks, by at most fifteen percent");
    require(entering.opacity>0 && entering.opacity<.04f && departing.opacity>0 && departing.opacity<.04f,"wide fade reaches near-zero opacity well before clipping");
    auto moderate=mc::edgeAppearance(40,300,200,600,100,150,0);
    require(moderate.opacity>.25f && moderate.opacity<.6f,"fade continues over a broad spacing-relative distance rather than a tiny radius band");
    auto smaller=mc::edgeAppearance(40,300,200,600,100,150,1);
    require(std::abs(smaller.radiusScale/moderate.radiusScale-.72f)<.001f,"neighbour size variation is stable and substantial");
    require(mc::edgeAppearance(0,300,200,600,100,150,0).opacity==0 && mc::edgeAppearance(200,300,200,600,100,150,0).opacity==0,"actual boundary is fully transparent");
    float previousAlpha=1,previousSize=2;
    for(float x=110;x<200;x+=.25f){auto a=mc::edgeAppearance(x,300,200,600,100,150,0);require(a.opacity<=previousAlpha+.0001f && a.radiusScale<=previousSize+.0001f,"opacity and radius continuously decrease on exit");previousAlpha=a.opacity;previousSize=a.radiusScale;}
    // Alternating sizes require TWO cells in both axes, including square layout.
    auto pattern=[](float px,float py,bool staggered){std::vector<std::array<float,3>> values;mc::visitLatticeIndexed(0,0,400,600,100,150,50,75,px,py,staggered,[&](float x,float y,int row,int col){if(x>=0&&x<400&&y>=0&&y<600)values.push_back({x,y,(row+col)%2!=0?.72f:1.f});});return values;};
    for(bool staggered:{false,true}){
        auto base=pattern(17,-29,staggered),wrapped=pattern(217,271,staggered);
        require(base==wrapped,"size template and row stagger survive horizontal and vertical phase wraps");
        mc::EdgeDotField rolling;settings={};settings.style=1;settings.staggered=staggered;settings.radius=18;settings.spacing=140;settings.edgeWidth=256;field.fill({170,35});
        std::vector<mc::DotVisual> previous;
        for(int frame=0;frame<1800;++frame){
            auto current=rolling.update(field,1200,900,settings,1.f/60);
            if(frame>30)for(auto old:previous){
                float x=old.position.x+170.f/60,y=old.position.y+35.f/60;
                auto found=std::find_if(current.begin(),current.end(),[&](auto d){return d.region==old.region && std::hypot(d.position.x-x,d.position.y-y)<.03f;});
                if(found!=current.end()){require(std::abs(found->radiusScale-old.radiusScale)*std::max(found->opacity,old.opacity)<.08f && std::abs(found->opacity-old.opacity)<.15f,"visible dot appearance stays smooth across many positive phase wraps");}
                else require(old.opacity<.03f,"departing dots disappear only after becoming nearly transparent");
            }
            previous=current;
        }
    }

    // Reversal must retrace the same point set and the same appearance. It
    // must not move the large-dot lobe to the opposite side of the strip.
    for(int selection=0;selection<3;++selection)for(bool staggered:{false,true}){
        mc::EdgeDotField reversing;settings={};settings.style=1;settings.staggered=staggered;settings.edges=selection;settings.radius=18;settings.spacing=140;settings.edgeWidth=256;
        field.fill({170,350});auto initial=reversing.update(field,1200,900,settings,0);
        std::vector<mc::DotVisual> previous=initial;
        for(int frame=0;frame<800;++frame){
            float sign=(frame/200)%2==0?-1.f:1.f;field.fill({170*sign,350*sign});
            auto current=reversing.update(field,1200,900,settings,1.f/60);
            for(auto old:previous){
                float x=old.position.x+170*sign/60,y=old.position.y+350*sign/60;
                auto found=std::find_if(current.begin(),current.end(),[&](auto d){return d.region==old.region&&std::hypot(d.position.x-x,d.position.y-y)<.03f;});
                if(found!=current.end()){
                    if(old.opacity>=.22f && found->opacity>=.22f)require(std::abs(old.radiusScale-found->radiusScale)<.001f,"visible dot sizes stay attached to the same dots during direction changes");
                    require(std::abs(old.opacity-found->opacity)<.15f,"reversal does not swap the fade envelope");
                }else require(old.opacity<.03f,"reversal cannot teleport a visible dot out of the strip");
            }
            previous=current;
            if(frame%400==399){
                require(std::count_if(initial.begin(),initial.end(),[](auto d){return d.opacity>.001f;})==std::count_if(current.begin(),current.end(),[](auto d){return d.opacity>.001f;}),"equal forward and backward travel restores visible population");
                for(auto old:initial){if(old.opacity<=.001f)continue;auto found=std::find_if(current.begin(),current.end(),[&](auto d){return d.region==old.region&&std::hypot(d.position.x-old.position.x,d.position.y-old.position.y)<.03f;});require(found!=current.end() && std::abs(found->radiusScale-old.radiusScale)<.001f && std::abs(found->opacity-old.opacity)<.001f,"forward/reverse round trip restores original position, size and opacity");}
            }
        }
    }
    std::cout<<"PASS: edge-only layout, continuous rolling, fade, all sides, appearance stability and finite values\n";
}
