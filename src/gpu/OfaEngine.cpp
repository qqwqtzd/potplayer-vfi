#include "gpu/OfaEngine.h"

namespace vfi {

HRESULT OfaEngine::Configure(const Frame& prototype) {
    if (prototype.format != PixelFormat::NV12) {
        return E_NOTIMPL;   // the OFA path feeds on NV12 luma
    }
    if (!pipeline_.Initialize()) {
        return E_FAIL;
    }
    if (!pipeline_.Configure(prototype.width, prototype.height,
                             prototype.format)) {
        return E_FAIL;
    }
    configured_ = true;
    return S_OK;
}

HRESULT OfaEngine::Interpolate(const Frame& from,
                               const Frame& to,
                               Frame* outs,
                               int outCount) {
    if (!configured_) {
        return E_UNEXPECTED;
    }
    if (outCount != Multiplier() - 1 || !outs) {
        return E_INVALIDARG;
    }
    if (!pipeline_.UploadInput(0, from) || !pipeline_.UploadInput(1, to)) {
        return E_FAIL;
    }

    const int n = Multiplier();
    for (int i = 0; i < outCount; ++i) {
        const float alpha = static_cast<float>(i + 1) / static_cast<float>(n);
        if (!pipeline_.Interpolate(alpha, outs[i])) {
            return E_FAIL;
        }
    }
    return S_OK;
}

void OfaEngine::Reset() {
    configured_ = false;
    pipeline_.ReleaseSize();
}

} // namespace vfi
