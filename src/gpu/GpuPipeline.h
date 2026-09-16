#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include "core/VfiTypes.h"
#include "gpu/GpuOpticalFlow.h"

namespace vfi {

// Owns the D3D11 device and every GPU resource the interpolation needs, and
// runs the three compute passes end to end:
//
//   NV12 -> RGBA  (x2)  ->  warp + blend at alpha  ->  RGBA -> NV12
//
// The flow texture fed to the warp pass comes straight from the OFA; it is
// never read back to the CPU.
class GpuPipeline {
public:
    GpuPipeline() = default;
    ~GpuPipeline();

    GpuPipeline(const GpuPipeline&) = delete;
    GpuPipeline& operator=(const GpuPipeline&) = delete;

    bool Initialize();
    void Shutdown();

    bool Configure(int width, int height, PixelFormat format);
    void ReleaseSize();
    bool IsConfigured() const { return width_ > 0 && height_ > 0; }

    GpuOpticalFlow& Flow() { return flow_; }
    ID3D11Device* Device() const { return device_.Get(); }
    ID3D11DeviceContext* Context() const { return context_.Get(); }

    // Uploads the luma + chroma of `frame` into slot 0 or 1 (NV12 only).
    bool UploadInput(int slot, const Frame& frame);

    // Warps/blends slots 0 and 1 at `alpha` and writes NV12 into `out.planes`.
    bool Interpolate(float alpha, const Frame& out);

private:
    using ComPtr = Microsoft::WRL::ComPtr;

    bool CreateDevice();
    bool CreateShaders();
    bool CreateViews();

    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;

    ComPtr<ID3D11ComputeShader> csNv12ToRgba_;
    ComPtr<ID3D11ComputeShader> csWarpBlend_;
    ComPtr<ID3D11ComputeShader> csRgbaToNv12_;
    ComPtr<ID3D11SamplerState> sampler_;
    ComPtr<ID3D11Buffer> params_;

    ComPtr<ID3D11Texture2D> yIn_[2];
    ComPtr<ID3D11Texture2D> uvIn_[2];
    ComPtr<ID3D11ShaderResourceView> yInSrv_[2];
    ComPtr<ID3D11ShaderResourceView> uvInSrv_[2];

    ComPtr<ID3D11Texture2D> rgba_[2];
    ComPtr<ID3D11ShaderResourceView> rgbaSrv_[2];
    ComPtr<ID3D11UnorderedAccessView> rgbaUav_[2];

    ComPtr<ID3D11Texture2D> mid_;
    ComPtr<ID3D11ShaderResourceView> midSrv_;
    ComPtr<ID3D11UnorderedAccessView> midUav_;

    ComPtr<ID3D11Texture2D> yOut_;
    ComPtr<ID3D11Texture2D> uvOut_;
    ComPtr<ID3D11UnorderedAccessView> yOutUav_;
    ComPtr<ID3D11UnorderedAccessView> uvOutUav_;
    ComPtr<ID3D11Texture2D> yStage_;
    ComPtr<ID3D11Texture2D> uvStage_;

    ComPtr<ID3D11ShaderResourceView> flowSrv_;

    GpuOpticalFlow flow_;
    int width_ = 0;
    int height_ = 0;
    PixelFormat format_ = PixelFormat::Unknown;
};

} // namespace vfi
