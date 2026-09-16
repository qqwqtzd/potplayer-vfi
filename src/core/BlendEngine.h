#pragma once

#include "IVfiEngine.h"

namespace vfi {

// CPU-only placeholder engine. Produces cross-dissolves between the two
// neighbouring frames. This has visible ghosting on motion (it is NOT motion
// compensated) and is only here to validate the DirectShow pipeline, the
// frame-rate up-conversion and the timestamp rewriting. It will be replaced by
// the OFA engine.
class BlendEngine final : public IVfiEngine {
public:
    BlendEngine() = default;

    HRESULT Configure(const Frame& prototype) override;
    int Multiplier() const override { return kVfiMultiplier; }
    HRESULT Interpolate(const Frame& from,
                        const Frame& to,
                        Frame* outs,
                        int outCount) override;
    void Reset() override;

private:
    PixelFormat format_ = PixelFormat::Unknown;
    int planeCount_ = 0;
};

} // namespace vfi
