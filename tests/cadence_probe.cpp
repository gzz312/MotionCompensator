#include "capture.hpp"
#include <psapi.h>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <vector>
#include <iomanip>
#include <cwctype>
#ifndef MC_CADENCE_LABEL
#define MC_CADENCE_LABEL "new"
#endif
double processCpu(){FILETIME a{},b{},k{},u{};GetProcessTimes(GetCurrentProcess(),&a,&b,&k,&u);ULARGE_INTEGER kernel{},user{};kernel.LowPart=k.dwLowDateTime;kernel.HighPart=k.dwHighDateTime;user.LowPart=u.dwLowDateTime;user.HighPart=u.dwHighDateTime;return (kernel.QuadPart+user.QuadPart)/1e7;}
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int){
    wchar_t exe[32768]{};GetModuleFileNameW(nullptr,exe,32768);auto dir=std::filesystem::path(exe).parent_path();
    std::ofstream out(dir/(std::string("cadence-probe-")+MC_CADENCE_LABEL+".csv"));
    HWND game=nullptr;EnumWindows([](HWND w,LPARAM data)->BOOL{
        if(!IsWindowVisible(w)||IsIconic(w))return TRUE;DWORD pid=0;GetWindowThreadProcessId(w,&pid);
        HANDLE p=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);if(!p)return TRUE;
        wchar_t path[32768]{};DWORD n=32768;bool valid=QueryFullProcessImageNameW(p,0,path,&n);CloseHandle(p);if(!valid)return TRUE;
        auto name=std::filesystem::path(path).filename().wstring();std::transform(name.begin(),name.end(),name.begin(),[](wchar_t c){return std::towlower(c);});
        if(mc::knownGame(name)){*reinterpret_cast<HWND*>(data)=w;return FALSE;}return TRUE;
    },reinterpret_cast<LPARAM>(&game));
    if(!game){out<<"NO_VISIBLE_GAME\n";return 2;}
    out<<"# capture-only performance, no overlay or game input; not a driving-accuracy test\n";
    out<<"elapsed_s,frames,analyses,pair_ms,analysis_ms,source_age_ms,observed_capture_fps,target_analysis_fps,replaced,tracked,cells,source_readbacks,readback_call_ms,invalid_pairs\n";
    Capture capture;capture.start(game,true);double start=qpcSeconds(),cpu=processCpu(),lastCompleted=0;CaptureStats s;
    uint64_t samples=0;double peakMemory=0;
    while(qpcSeconds()-start<12){double now=qpcSeconds();s=capture.snapshot();
        PROCESS_MEMORY_COUNTERS memory{};memory.cb=sizeof(memory);GetProcessMemoryInfo(GetCurrentProcess(),&memory,sizeof(memory));peakMemory=std::max(peakMemory,memory.WorkingSetSize/(1024.*1024.));
        if(s.motion.completedTimestamp>lastCompleted){lastCompleted=s.motion.completedTimestamp;++samples;
            out<<std::fixed<<std::setprecision(4)<<now-start<<','<<s.frames<<','<<s.analyses<<','<<s.motion.pairIntervalMs<<','<<s.analysisMs<<','<<(now-s.motion.timestamp)*1000<<','<<s.observedCaptureFps<<','<<s.targetAnalysisFps<<','<<s.droppedAnalysis<<','<<s.motion.tracked<<','<<s.motion.observedCells<<','<<s.sourceReadbacks<<','<<s.readbackCallMs<<','<<s.invalidPairs<<'\n';}
        Sleep(4);
    }
    double elapsed=qpcSeconds()-start,cpuPct=(processCpu()-cpu)/elapsed*100;capture.stop();
    out<<"# elapsed_s="<<elapsed<<", samples="<<samples<<", frames="<<s.frames<<", analyses="<<s.analyses<<", capture_fps="<<s.frames/elapsed<<", analysis_fps="<<s.analyses/elapsed<<", cpu_one_core_pct="<<cpuPct<<", peak_working_set_mb="<<peakMemory<<", replaced="<<s.droppedAnalysis<<", status="<<s.status<<'\n';out.flush();
    return samples>=10?0:3;
}
