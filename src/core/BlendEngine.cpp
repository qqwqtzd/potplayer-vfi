#include "BlendEngine.h"

#include <cstring>

namespace vfi {
namespace {

void MixPlane(uint8_t* dst,
              const uint8_t* a,
              const uint8_t* b,
              int width,
              int height,
              int stride,
              int num,
              int den) {
    const int inv = den - num;
    for (int y = 0; y < height; ++y) {
        const uint8_t* ra = a + static_cast<size_t>(y) * stride;
        const uint8_t* rb = b + static_cast<size_t>(y) * stride;
        uint8_t* rd = dst + static_cast<size_t>(y) * stride;
        for (int x = 0; x < width; ++x) {
            rd[x] = static_cast<uint8_t>((ra[x] * inv + rb[x] * num) / den);
        }
    }
}

} // namespace

HRESULT BlendEngine::Configure(const Frame& prototype) {
    format_ = prototype.format;
    planeCount_ = prototype.planeCount;
    return S_OK;
}

HRESULT BlendEngine::Interpolate(const Frame& from,
                                 const Frame& to,
                                 Frame* outs,
                                 int outCount) {
    if (outCount != Multiplier() - 1) {
        return E_INVALIDARG;
    }
    if (from.format != to.format || from.planeCount != to.planeCount) {
        return E_INVALIDARG;
    }

    const int den = Multiplier();
    for (int i = 0; i < outCount; ++i) {
        const int num = i + 1;
        for (int p = 0; p < from.planeCount; ++p) {
            const Plane& pf = from.planes[p];
            const Plane& pt = to.planes[p];
            const Plane& pd = outs[i].planes[p];
            if (!pf.data || !pt.data || !pd.data) {
                return E_POINTER;
            }
            MixPlane(pd.data, pf.data, pt.data,
                     pf.width, pf.height, pf.stride, num, den);
        }
    }
    return S_OK;
}

void BlendEngine::Reset() {
    format_ = PixelFormat::Unknown;
    planeCount_ = 0;
}

} // namespace vfi
