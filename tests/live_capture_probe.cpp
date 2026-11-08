#include "capture.hpp"
#include "dynamic.hpp"
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <vector>
#include <cwctype>
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int){
    wchar_t exe[MAX_PATH]{};GetModuleFileNameW(nullptr,exe,MAX_PATH);auto dir=std::filesystem::path(exe).parent_path();std::ofstream out(dir/L"live-capture-probe.txt");
    HWND game=nullptr;
    EnumWindows([](HWND w,LPARAM data)->BOOL{
        if(!IsWindowVisible(w)||IsIconic(w))return TRUE;DWORD pid=0;GetWindowThreadProcessId(w,&pid);HANDLE p=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);if(!p)return TRUE;
        wchar_t path[32768]{};DWORD n=32768;bool valid=QueryFullProcessImageNameW(p,0,path,&n)!=FALSE;CloseHandle(p);if(!valid)return TRUE;
        auto name=std::filesystem::path(path).filename().wstring();std::transform(name.begin(),name.end(),name.begin(),[](wchar_t c){return std::towlower(c);});
        if(mc::knownGame(name)){*reinterpret_cast<HWND*>(data)=w;return FALSE;}return TRUE;
    },reinterpret_cast<LPARAM>(&game));
    if(!game){out<<"NO_VISIBLE_GAME\n";return 2;}
    Capture capture;capture.start(game,true);mc::DynamicDriver driver;double started=qpcSeconds(),previous=started,lastCompleted=0;uint64_t results=0,trusted=0,fresh=0,driven=0;std::vector<double> ages,costs;CaptureStats stats;
    out<<"Live production capture; no overlay, game input, injection or simulated scene.\n";out.flush();
    while(qpcSeconds()-started<14){
        double now=qpcSeconds();stats=capture.snapshot();mc::InputState input;input.timestamp=now;driver.update(stats.motion,input,now,float(now-previous),true,true);previous=now;
        if(stats.motion.completedTimestamp>lastCompleted){lastCompleted=stats.motion.completedTimestamp;++results;bool q=stats.motion.confidence>=.65f;trusted+=q;bool isFresh=mc::motionFresh(stats.motion,now);fresh+=isFresh;driven+=driver.source()==mc::DriveSource::Flow;
            if(stats.motion.timestamp>0)ages.push_back((now-stats.motion.timestamp)*1000);costs.push_back(stats.analysisMs);
            out<<"result="<<results<<", source_age_ms="<<(stats.motion.timestamp>0?(now-stats.motion.timestamp)*1000:-1)<<", cpu_ms="<<stats.analysisMs<<", confidence="<<stats.motion.confidence<<", tracked="<<stats.motion.tracked<<", cells="<<stats.motion.observedCells<<", fresh="<<isFresh<<", drive="<<int(driver.source())<<'\n';out.flush();
        }
        Sleep(10);
    }
    capture.stop();auto median=[](std::vector<double> v){if(v.empty())return -1.;std::sort(v.begin(),v.end());return v[v.size()/2];};
    out<<"SUMMARY frames="<<stats.frames<<", results="<<results<<", trusted="<<trusted<<", fresh="<<fresh<<", flow_only_driven="<<driven<<", median_source_age_ms="<<median(ages)<<", median_cpu_ms="<<median(costs)<<", replaced="<<stats.droppedAnalysis<<", capture_dropped="<<stats.droppedCapture<<", status="<<stats.status<<'\n';
    return driven>0?0:3;
}
