#pragma once

#include "VfiTypes.h"

namespace vfi {

// Interpolation backend. Two implementations are planned:
//
//   OFA engine   - NVIDIA Optical Flow SDK (D3D11) on the RTX Optical Flow
//                  Accelerator, plus a D3D11 warp/blend compute pass. This is
//                  the DLSS-FG hardware path; it is the shipping engine.
//
//   Blend engine - pure CPU linear blend. No motion estimation. Used to prove
//                  the DirectShow plumbing end to end before the GPU work.
//
class IVfiEngine {
public:
    virtual ~IVfiEngine() = default;

    // Called from SetMediaType() when the negotiated size/format are known.
    virtual HRESULT Configure(const Frame& prototype) = 0;

    // Always >= 2. Kept virtual so a future engine can adapt to GPU headroom.
    virtual int Multiplier() const = 0;

    // Produce (Multiplier() - 1) in-between frames between `from` and `to`.
    // The caller owns `outs` and has already sized each FrameView's planes to
    // the same format/size as `from`. Frame i should be shown at
    //   from.start + (to.start - from.start) * (i + 1) / Multiplier
    virtual HRESULT Interpolate(const Frame& from,
                                const Frame& to,
                                Frame* outs,
                                int outCount) = 0;

    // Called on NewSegment / disconnect / flush.
    virtual void Reset() = 0;
};

} // namespace vfi
