#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <thread>
LRESULT CALLBACK windowProc(HWND w,UINT m,WPARAM a,LPARAM b){if(m==WM_APP){DestroyWindow(w);return 0;}if(m==WM_CLOSE)return 0;if(m==WM_DESTROY){PostQuitMessage(0);return 0;}return DefWindowProcW(w,m,a,b);}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int){
    wchar_t path[32768]{};GetModuleFileNameW(nullptr,path,32768);auto dir=std::filesystem::path(path).parent_path();
    WNDCLASSW wc{};wc.hInstance=instance;wc.lpfnWndProc=windowProc;wc.lpszClassName=L"MCSoftwareChecks";wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);RegisterClassW(&wc);
    HWND window=CreateWindowW(wc.lpszClassName,L"MotionCompensator software checks",WS_OVERLAPPEDWINDOW,100,100,600,180,nullptr,nullptr,instance,nullptr);ShowWindow(window,SW_SHOW);
    CreateWindowW(L"STATIC",L"Checking the application's own window. No game input. This window closes when finished.",WS_CHILD|WS_VISIBLE,20,20,540,80,window,nullptr,instance,nullptr);
    std::jthread worker([&]{
        std::ofstream out(dir/L"desktop-smoke-results.txt");
        for(auto mode:{L"--diagnostic-static",L"--diagnostic-regional",L"--diagnostic-edge"}){
            std::wstring command=L"\""+(dir/L"MotionCompensator.exe").wstring()+L"\" --smoke-test --diagnostic-record-log "+mode;
            STARTUPINFOW startup{sizeof(startup)};PROCESS_INFORMATION info{};
            if(!CreateProcessW(nullptr,command.data(),nullptr,nullptr,FALSE,0,nullptr,dir.c_str(),&startup,&info)){out<<"launch_error="<<GetLastError()<<"\n";break;}
            DWORD wait=WaitForSingleObject(info.hProcess,20000),result=999;GetExitCodeProcess(info.hProcess,&result);
            out<<std::string(mode,mode+wcslen(mode))<<" exit="<<result<<" wait="<<wait<<"\n";out.flush();CloseHandle(info.hThread);CloseHandle(info.hProcess);
            if(wait!=WAIT_OBJECT_0)break;
        }
        PostMessageW(window,WM_APP,0,0);
    });
    MSG msg{};while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}return 0;
}
