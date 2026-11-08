#pragma once
#include "debug_panel.hpp"
#include <deque>
#include <vector>
namespace mc {
struct DebugRect {float x,y,w,h;Color color;float alpha=1;};
struct DebugStroke {Vec2 a,b;Color color;float width=1;};
struct DebugLabel {float x,y;std::wstring text;Color color;};
struct DebugScene {
    static constexpr float width=460,height=530;
    std::vector<DebugRect> rects;
    std::vector<DebugStroke> strokes;
    std::vector<DebugLabel> labels;
};
inline bool measured(double v){return std::isfinite(v)&&v>=0;}
inline std::wstring debugNumber(double v,int precision=1) {
    if(!measured(v))return L"N/A";
    std::wostringstream s;s<<std::fixed<<std::setprecision(precision)<<v;return s.str();
}
inline bool freshDebugFlow(const DebugSnapshot& d) {
    return motionFresh(d.flow,d.now);
}
class DebugHistory {
    std::deque<DebugSnapshot> samples_;
public:
    void clear(){samples_.clear();}
    void push(const DebugSnapshot& d) {
        if(!std::isfinite(d.now))return;
        if(!samples_.empty() && (d.now<=samples_.back().now || d.now-samples_.back().now>1 || d.style!=samples_.back().style))clear();
        samples_.push_back(d);
        while(samples_.size()>101 || (!samples_.empty() && d.now-samples_.front().now>10))samples_.pop_front();
    }
    const auto& samples()const{return samples_;}
};
inline DebugScene buildDebugScene(const DebugHistory& history,bool chinese,size_t dots) {
    DebugScene s;if(history.samples().empty())return s;
    const auto& d=history.samples().back();
    const Color ink{.88f,.94f,1},muted{.58f,.66f,.76f},cyan{.20f,.78f,1},amber{1,.70f,.22f},purple{.77f,.57f,1},green{.32f,1,.61f};
    auto label=[&](float x,float y,std::wstring text,Color c){s.labels.push_back({x,y,std::move(text),c});};
    auto line=[&](Vec2 a,Vec2 b,Color c,float width=1.f){s.strokes.push_back({a,b,c,width});};
    s.rects.push_back({0,0,s.width,s.height,{.025f,.04f,.07f},.87f});
    std::wstring style=chinese?(d.style==0?L"分区":d.style==1?L"屏边":L"静态"):(d.style==0?L"Regional":d.style==1?L"Edge":L"Static");
    label(12,8,(chinese?L"运动可视化  ·  ":L"Motion view  |  ")+style+(d.staggered?(chinese?L" / 交错":L" / Staggered"):(chinese?L" / 方形":L" / Square")),ink);
    std::array<Color,4> colors{cyan,amber,purple,green};
    auto names=chinese?std::array{L"输入预测",L"驱动光流",L"融合运动",L"点阵运动"}:std::array{L"Input",L"Drive flow",L"Fused",L"Dots"};
    for(int i=0;i<4;++i){s.rects.push_back({12.f+i*111,35,7,7,colors[i],1});label(24.f+i*111,29,names[i],colors[i]);}
    bool ready=d.hasSelectedFlow?std::any_of(d.selectedConfidence.begin(),d.selectedConfidence.end(),[](float q){return std::isfinite(q)&&q>=.65f;}):d.flow.confidence>=.65f;
    const wchar_t* state=d.flow.timestamp<=0?(chinese?L"暂无分析结果":L"No analysis result"):!freshDebugFlow(d)?(chinese?L"结果过期":L"Result expired"):!ready?(chinese?L"可信度/覆盖不足":L"Low confidence/coverage"):(chinese?L"光流可用于补偿":L"Flow ready for fusion");
    std::wstring stateLabel=state;
    if(d.hasSelectedFlow)stateLabel+=(chinese?L"  · 背景权重 ":L" | Background ")+debugNumber(d.backgroundBlend*100,0)+L"%";
    label(12,51,stateLabel,ready && freshDebugFlow(d)?green:amber);
    bool fresh=freshDebugFlow(d);
    auto arrow=[&](Vec2 origin,Vec2 v,Color c,bool valid){
        if(!valid || !std::isfinite(v.x)||!std::isfinite(v.y)) {line({origin.x-2,origin.y},{origin.x+2,origin.y},muted);return;}
        float speed=std::hypot(v.x,v.y);
        if(speed<.5f){line({origin.x-1,origin.y},{origin.x+1,origin.y},c,2);return;}
        float length=std::min(speed/600.f,1.f)*23.f;
        Vec2 unit{v.x/speed,v.y/speed},tip{origin.x+unit.x*length,origin.y+unit.y*length};
        line(origin,tip,c,1.6f);
        float head=std::min(5.f,length*.45f);
        line(tip,{tip.x-unit.x*head-unit.y*head*.65f,tip.y-unit.y*head+unit.x*head*.65f},c,1.6f);
        line(tip,{tip.x-unit.x*head+unit.y*head*.65f,tip.y-unit.y*head-unit.x*head*.65f},c,1.6f);
    };
    for(int r=0;r<9;++r){
        float x=12.f+(r%3)*146,y=74.f+(r/3)*66;
        s.rects.push_back({x,y,142,62,{.18f,.24f,.32f},.48f});
        label(x+5,y+2,L"R"+std::to_wstring(r+1),muted);
        bool confidence=fresh && std::isfinite(d.flow.confidence) && std::isfinite(d.flow.regionConfidence[r]);
        float q=confidence?std::clamp(d.hasSelectedFlow?d.selectedConfidence[r]:std::min(d.flow.confidence,d.flow.regionConfidence[r]),0.f,1.f):0;
        label(x+95,y+2,confidence?debugNumber(q*100,0)+L"%":L"N/A",confidence?ink:muted);
        s.rects.push_back({x+5,y+56,132,2,{.3f,.34f,.4f},1});
        if(confidence)s.rects.push_back({x+5,y+56,132*q,2,q>=.65f?green:amber,1});
        std::array<Vec2,4> vectors{d.input[r],d.hasSelectedFlow?d.selectedFlow[r]:d.flow.velocity[r],d.fused[r],d.compensation[r]};
        for(int i=0;i<4;++i)arrow({x+25.f+i*30,y+35},vectors[i],colors[i],i!=1 || (confidence && q>=.65f));
    }
    label(12,275,chinese?L"色条：光流可信度；灰短线：数据不可用":L"Bar: flow confidence; grey dash: unavailable",muted);
    if(d.style==1)label(12,293,chinese?L"屏边取自 R4 / R6 / R2 / R8":L"Edge sources: R4 / R6 / R2 / R8",muted);
    else label(12,293,(chinese?L"可见点数 ":L"Visible dots ")+std::to_wstring(dots)+(chinese?L"  ·  输入为映射估计":L"  |  Input is an estimate"),muted);
    auto graph=[&](float top,int kind){
        bool fps=kind==0;
        double maximum=fps?60.:kind==1?5.:300.;
        auto values=[&](const DebugSnapshot& v){
            if(kind==0)return std::array{v.captureFps,v.analysisFps,v.renderFps};
            if(kind==1)return std::array{v.analysisMs,v.fusionMs,v.submitMs};
            double sum=0;int count=0;
            if(freshDebugFlow(v))for(int r=0;r<9;++r){auto speed=std::hypot(v.flow.velocity[r].x,v.flow.velocity[r].y);if(v.flow.regionConfidence[r]>0 && std::isfinite(speed)){sum+=speed*speed;++count;}}
            return std::array{count?std::sqrt(sum/count):-1.,-1.,-1.};
        };
        for(const auto& v:history.samples())for(double value:values(v))if(measured(value))maximum=std::max(maximum,value);
        double step=fps?30:kind==1?5:300;maximum=std::ceil(maximum/step)*step;
        constexpr float left=46,right=446,height=29;
        line({left,top+height},{right,top+height},muted);line({left,top},{left,top+height},muted);
        label(12,top-4,debugNumber(maximum,0),muted);label(22,top+15,L"0",muted);
        auto current=values(d);
        label(12,top-21,fps?L"FPS":kind==1?L"ms":L"px/s",muted);
        auto metricNames=fps?(chinese?std::array{L"采集 ",L"分析 ",L"提交 "}:std::array{L"Capture ",L"Flow ",L"Submit "}):(chinese?std::array{L"光流 ",L"融合 ",L"绘制 "}:std::array{L"Flow ",L"Fusion ",L"Draw "});
        if(kind==2)label(66,top-21,(chinese?L"原始光流速度（未必采纳） ":L"Raw flow speed (may be rejected) ")+debugNumber(current[0]),amber);
        else for(int i=0;i<3;++i)label(66.f+i*126,top-21,std::wstring(metricNames[i])+debugNumber(current[i]),colors[i==0?0:i==1?2:3]);
        for(int channel=0;channel<3;++channel){
            bool have=false;Vec2 previous{};double previousTime=0;
            for(const auto& v:history.samples()){
                double value=values(v)[channel];if(!measured(value)){have=false;continue;}
                Vec2 p{left+float((v.now-(d.now-10))/10)*(right-left),top+height-float(value/maximum)*height};
                if(have && v.now-previousTime<=.25)line(previous,p,kind==2?amber:colors[channel==0?0:channel==1?2:3],1.5f);
                else line({p.x-1,p.y},{p.x+1,p.y},kind==2?amber:colors[channel==0?0:channel==1?2:3],1.5f);
                previous=p;previousTime=v.now;have=true;
            }
        }
    };
    graph(333,0);graph(390,1);graph(447,2);
    label(12,485,chinese?L"最近10秒；预测误差 / 实际呈现延迟：N/A":L"Last 10s; prediction error / display latency: N/A",muted);
    label(12,507,(chinese?L"光流帧龄 ":L"Flow source age ")+debugNumber(d.flow.timestamp>0?(d.now-d.flow.timestamp)*1000:-1)+L" ms  | "+(chinese?L"完成后 ":L"Since completion ")+debugNumber(d.flow.completedTimestamp>0?(d.now-d.flow.completedTimestamp)*1000:-1)+L" ms",muted);
    if(d.flow.analysisWidth>0) {
        auto& info=*std::find_if(s.labels.begin(),s.labels.end(),[](const auto& label){return label.y==293;});
        info.text=(chinese?L"分析 ":L"Analysis ")+std::to_wstring(d.flow.analysisWidth)+L"×"+std::to_wstring(d.flow.analysisHeight)+(chinese?L"  小格 ":L" Cells ")+std::to_wstring(d.flow.observedCells)+L"/81"+(chinese?L"  点 ":L" Tracks ")+std::to_wstring(d.flow.tracked)+L"/"+std::to_wstring(d.flow.features)+(chinese?L" 点阵 ":L" Dots ")+std::to_wstring(dots);
    }
    return s;
}
}
