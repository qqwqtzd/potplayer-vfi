#pragma once

// RAII wrapper around the NVIDIA Optical Flow SDK D3D11 backend.
//
// The SDK's own wrapper sources (NvOF.cpp / NvOFD3D11.cpp) resolve the runtime
// with LoadLibrary("nvofapi64.dll"), so no import library is linked. That DLL
// must exist on the target machine (the NVIDIA display driver normally places
// it in C:\Windows\System32).
//
// Only the luma plane is needed: the video decoder already gives us NV12, whose
// Y plane is exactly the GRAYSCALE8 image the Optical Flow Accelerator wants.

#include <d3d11.h>

#include <cstdint>
#include <vector>

#include "NvOF.h"
#include "NvOFD3D11.h"

namespace vfi {

class GpuOpticalFlow {
public:
    GpuOpticalFlow() = default;
    ~GpuOpticalFlow();

    GpuOpticalFlow(const GpuOpticalFlow&) = delete;
    GpuOpticalFlow& operator=(const GpuOpticalFlow&) = delete;

    // Returns false (rather than throwing) when the OFA is unavailable, e.g. a
    // non-RTX GPU, an old driver, or a missing nvofapi64.dll.
    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context,
                    int width, int height);
    void Shutdown();

    bool IsReady() const { return nvof_ != nullptr; }
    int Width() const { return width_; }
    int Height() const { return height_; }

    // Vector grid the hardware actually accepted (1, 2 or 4). When > 1 the flow
    // texture is width/gridSize x height/gridSize.
    int GridSize() const { return gridSize_; }

    // Uploads one full-resolution 8-bit luma plane into input slot 0 or 1.
    // stride is the source row pitch in bytes and may exceed width.
    bool UploadLuma(int slot, const uint8_t* luma, int stride);

    // Runs OFA on the two uploaded slots and returns the flow texture
    // (DXGI_FORMAT_R16G16_SINT). The texture is owned by this object.
    ID3D11Texture2D* ExecuteAndGetFlow();

private:
    NvOFObj nvof_;
    std::vector<NvOFBufferObj> inputs_;
    NvOFBufferObj output_;

    ID3D11Device* device_ = nullptr;
    ID3D11DeviceContext* context_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    int gridSize_ = 1;
};

} // namespace vfi
