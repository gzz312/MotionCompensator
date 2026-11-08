#pragma once
#include <windows.h>
#include <wrl/client.h>
#include <stdexcept>
#include <sstream>
#include <bit>
template<class T> T loadFunction(HMODULE module, const char* name) {
    return std::bit_cast<T>(GetProcAddress(module,name));
}
using Microsoft::WRL::ComPtr;
inline void check(HRESULT hr, const char* operation) {
    if (FAILED(hr)) { std::ostringstream s; s << operation << " HRESULT=0x" << std::hex << static_cast<unsigned long>(hr); throw std::runtime_error(s.str()); }
}
inline double qpcSeconds() { LARGE_INTEGER t{}, f{}; QueryPerformanceCounter(&t); QueryPerformanceFrequency(&f); return double(t.QuadPart)/double(f.QuadPart); }
