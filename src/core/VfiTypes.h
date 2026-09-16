#pragma once

// Platform-neutral frame/format types shared by the filter and the
// interpolation engines. Deliberately free of DirectShow/BaseClasses headers
// so the GPU/OFA code can be unit-reasoned about in isolation.

#include <windows.h>
#include <cstdint>

namespace vfi {

using Time = long long;   // 100 ns units, same as REFERENCE_TIME

enum class PixelFormat : uint32_t {
    Unknown = 0,
    NV12,       // 2 planes: Y, interleaved UV (half res)
    YV12,       // 3 planes: Y, V, U (all chroma half res)
    RGB32,      // 1 plane BGRA / BGRX, top-down
    RGB24,      // 1 plane BGR, top-down
};

inline int PlaneCount(PixelFormat f) {
    switch (f) {
        case PixelFormat::NV12:  return 2;
        case PixelFormat::YV12:  return 3;
        case PixelFormat::RGB32:
        case PixelFormat::RGB24: return 1;
        default:                 return 0;
    }
}

// A strided view over one plane. `data` may point into a D3D staging buffer,
// a DirectShow IMediaSample, or an engine-owned allocation.
struct Plane {
    uint8_t* data = nullptr;
    int stride = 0;   // bytes per row, can be negative for bottom-up RGB
    int width = 0;    // pixels
    int height = 0;   // rows
};

struct Frame {
    Plane planes[3];
    int planeCount = 0;
    PixelFormat format = PixelFormat::Unknown;
    int width = 0;
    int height = 0;
    Time start = 0;
    Time end = 0;
};

// Output frames produced per input frame. 2 == classic frame doubling, which
// is what DmitriRender/SVP default to and what looks best on 24/30 fps video.
inline constexpr int kVfiMultiplier = 2;

} // namespace vfi
