#pragma once

#include "core/IVfiEngine.h"
#include "gpu/GpuPipeline.h"

namespace vfi {

// IVfiEngine backed by the RTX Optical Flow Accelerator and a D3D11 warp/blend
// pipeline. This is the engine that actually ships; BlendEngine remains only as
// a fallback when the OFA is unavailable.
class OfaEngine final : public IVfiEngine {
public:
    HRESULT Configure(const Frame& prototype) override;
    int Multiplier() const override { return kVfiMultiplier; }
    HRESULT Interpolate(const Frame& from,
                        const Frame& to,
                        Frame* outs,
                        int outCount) override;
    void Reset() override;

private:
    GpuPipeline pipeline_;
    bool configured_ = false;
};

} // namespace vfi
