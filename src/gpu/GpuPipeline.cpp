#include "gpu/GpuPipeline.h"

#include <d3dcompiler.h>

#include <cstring>
#include <string>

namespace vfi {
namespace {

std::wstring ModuleDirectory() {
    HMODULE module = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&ModuleDirectory), &module);
    wchar_t path[MAX_PATH] = {};
    if (GetModuleFileNameW(module, path, MAX_PATH) == 0) {
        return {};
    }
    std::wstring s(path);
    const size_t pos = s.find_last_of(L"\\/");
    return pos == std::wstring::npos ? std::wstring() : s.substr(0, pos);
}

ComPtr<ID3D11ComputeShader> CompileCompute(ID3D11Device* device,
                                           const std::wstring& file,
                                           const char* entry) {
    ComPtr<ID3DBlob> code;
    ComPtr<ID3DBlob> errors;
    if (FAILED(D3DCompileFromFile(file.c_str(), nullptr, nullptr, entry,
                                  "cs_5_0", D3DCOMPILE_ENABLE_STRICTNESS, 0,
                                  &code, &errors))) {
        return nullptr;
    }
    ComPtr<ID3D11ComputeShader> shader;
    if (FAILED(device->CreateComputeShader(code->GetBufferPointer(),
                                           code->GetBufferSize(), nullptr,
                                           &shader))) {
        return nullptr;
    }
    return shader;
}

HRESULT MakeTexture(ID3D11Device* device, DXGI_FORMAT format, UINT width,
                    UINT height, UINT bind, D3D11_USAGE usage, UINT cpuAccess,
                    ID3D11Texture2D** out) {
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.Usage = usage;
    desc.BindFlags = bind;
    desc.CPUAccessFlags = cpuAccess;
    return device->CreateTexture2D(&desc, nullptr, out);
}

void Dispatch(ID3D11DeviceContext* ctx, ID3D11ComputeShader* shader, int width,
              int height) {
    ctx->CSSetShader(shader, nullptr, 0);
    ctx->Dispatch((width + 15) / 16, (height + 15) / 16, 1);
}

struct Params {
    float alpha;
    float invGrid;
    UINT width;
    UINT height;
};

} // namespace

GpuPipeline::~GpuPipeline() {
    Shutdown();
}

bool GpuPipeline::Initialize() {
    if (device_) {
        return true;
    }
    if (!CreateDevice()) {
        return false;
    }
    if (!CreateShaders()) {
        Shutdown();
        return false;
    }
    return true;
}

void GpuPipeline::Shutdown() {
    ReleaseSize();
    csNv12ToRgba_.Reset();
    csWarpBlend_.Reset();
    csRgbaToNv12_.Reset();
    sampler_.Reset();
    params_.Reset();
    context_.Reset();
    device_.Reset();
}

bool GpuPipeline::CreateDevice() {
    UINT flags = 0;
#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_1,
                                        D3D_FEATURE_LEVEL_11_0};
    D3D_FEATURE_LEVEL obtained = D3D_FEATURE_LEVEL_11_0;

    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                                   flags, levels, 2, D3D11_SDK_VERSION, &device,
                                   &obtained, &context);
    if (hr == E_INVALIDARG) {
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                               flags, &levels[1], 1, D3D11_SDK_VERSION, &device,
                               &obtained, &context);
    }
    if (FAILED(hr)) {
        return false;
    }
    device_ = device;
    context_ = context;
    return true;
}

bool GpuPipeline::CreateShaders() {
    const std::wstring dir = ModuleDirectory();
    if (dir.empty()) {
        return false;
    }
    const std::wstring file = dir + L"\\shaders\\vfi.hlsl";

    csNv12ToRgba_ = CompileCompute(device_.Get(), file, "CS_Nv12ToRgba");
    csWarpBlend_ = CompileCompute(device_.Get(), file, "CS_WarpBlend");
    csRgbaToNv12_ = CompileCompute(device_.Get(), file, "CS_RgbaToNv12");
    if (!csNv12ToRgba_ || !csWarpBlend_ || !csRgbaToNv12_) {
        return false;
    }

    D3D11_SAMPLER_DESC sd{};
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    if (FAILED(device_->CreateSamplerState(&sd, &sampler_))) {
        return false;
    }

    D3D11_BUFFER_DESC bd{};
    bd.ByteWidth = sizeof(Params);
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    return SUCCEEDED(device_->CreateBuffer(&bd, nullptr, &params_));
}

bool GpuPipeline::Configure(int width, int height, PixelFormat format) {
    if (!device_ || width <= 0 || height <= 0) {
        return false;
    }
    ReleaseSize();

    if (format != PixelFormat::NV12) {
        return false;   // the GPU path handles NV12 only for now
    }

    width_ = width;
    height_ = height;
    format_ = format;

    if (!flow_.Initialize(device_.Get(), context_.Get(), width, height)) {
        ReleaseSize();
        return false;
    }
    if (!CreateViews()) {
        ReleaseSize();
        return false;
    }
    return true;
}

void GpuPipeline::ReleaseSize() {
    flowSrv_.Reset();
    for (int i = 0; i < 2; ++i) {
        yIn_[i].Reset();
        uvIn_[i].Reset();
        yInSrv_[i].Reset();
        uvInSrv_[i].Reset();
        rgba_[i].Reset();
        rgbaSrv_[i].Reset();
        rgbaUav_[i].Reset();
    }
    mid_.Reset();
    midSrv_.Reset();
    midUav_.Reset();
    yOut_.Reset();
    uvOut_.Reset();
    yOutUav_.Reset();
    uvOutUav_.Reset();
    yStage_.Reset();
    uvStage_.Reset();

    flow_.Shutdown();
    width_ = 0;
    height_ = 0;
    format_ = PixelFormat::Unknown;
}

bool GpuPipeline::CreateViews() {
    ID3D11Device* d = device_.Get();
    const UINT w = static_cast<UINT>(width_);
    const UINT h = static_cast<UINT>(height_);
    const UINT cw = (w + 1) / 2;
    const UINT ch = (h + 1) / 2;

    for (int i = 0; i < 2; ++i) {
        if (FAILED(MakeTexture(d, DXGI_FORMAT_R8_UNORM, w, h,
                               D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT,
                               0, &yIn_[i])) ||
            FAILED(MakeTexture(d, DXGI_FORMAT_R8G8_UNORM, cw, ch,
                               D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT,
                               0, &uvIn_[i])) ||
            FAILED(d->CreateShaderResourceView(yIn_[i].Get(), nullptr,
                                               &yInSrv_[i])) ||
            FAILED(d->CreateShaderResourceView(uvIn_[i].Get(), nullptr,
                                               &uvInSrv_[i])) ||
            FAILED(MakeTexture(d, DXGI_FORMAT_R8G8B8A8_UNORM, w, h,
                               D3D11_BIND_SHADER_RESOURCE |
                                   D3D11_BIND_UNORDERED_ACCESS,
                               D3D11_USAGE_DEFAULT, 0, &rgba_[i])) ||
            FAILED(d->CreateShaderResourceView(rgba_[i].Get(), nullptr,
                                               &rgbaSrv_[i])) ||
            FAILED(d->CreateUnorderedAccessView(rgba_[i].Get(), nullptr,
                                                &rgbaUav_[i]))) {
            return false;
        }
    }

    if (FAILED(MakeTexture(d, DXGI_FORMAT_R8G8B8A8_UNORM, w, h,
                           D3D11_BIND_SHADER_RESOURCE |
                               D3D11_BIND_UNORDERED_ACCESS,
                           D3D11_USAGE_DEFAULT, 0, &mid_)) ||
        FAILED(d->CreateShaderResourceView(mid_.Get(), nullptr, &midSrv_)) ||
        FAILED(d->CreateUnorderedAccessView(mid_.Get(), nullptr, &midUav_))) {
        return false;
    }

    if (FAILED(MakeTexture(d, DXGI_FORMAT_R8_UNORM, w, h,
                           D3D11_BIND_UNORDERED_ACCESS, D3D11_USAGE_DEFAULT, 0,
                           &yOut_)) ||
        FAILED(MakeTexture(d, DXGI_FORMAT_R8G8_UNORM, cw, ch,
                           D3D11_BIND_UNORDERED_ACCESS, D3D11_USAGE_DEFAULT, 0,
                           &uvOut_)) ||
        FAILED(d->CreateUnorderedAccessView(yOut_.Get(), nullptr, &yOutUav_)) ||
        FAILED(d->CreateUnorderedAccessView(uvOut_.Get(), nullptr,
                                            &uvOutUav_))) {
        return false;
    }

    if (FAILED(MakeTexture(d, DXGI_FORMAT_R8_UNORM, w, h, 0,
                           D3D11_USAGE_STAGING, D3D11_CPU_ACCESS_READ,
                           &yStage_)) ||
        FAILED(MakeTexture(d, DXGI_FORMAT_R8G8_UNORM, cw, ch, 0,
                           D3D11_USAGE_STAGING, D3D11_CPU_ACCESS_READ,
                           &uvStage_))) {
        return false;
    }

    return true;
}

bool GpuPipeline::UploadInput(int slot, const Frame& frame) {
    if (slot < 0 || slot > 1 || format_ != PixelFormat::NV12) {
        return false;
    }
    if (frame.format != PixelFormat::NV12 || frame.planeCount < 2) {
        return false;
    }
    const Plane& luma = frame.planes[0];
    const Plane& chroma = frame.planes[1];
    if (!luma.data || !chroma.data) {
        return false;
    }

    ID3D11DeviceContext* ctx = context_.Get();
    ctx->UpdateSubresource(yIn_[slot].Get(), 0, nullptr, luma.data,
                           static_cast<UINT>(luma.stride), 0);
    ctx->UpdateSubresource(uvIn_[slot].Get(), 0, nullptr, chroma.data,
                           static_cast<UINT>(chroma.stride), 0);

    return flow_.UploadLuma(slot, luma.data, luma.stride);
}

bool GpuPipeline::Interpolate(float alpha, const Frame& out) {
    if (!IsConfigured() || format_ != PixelFormat::NV12) {
        return false;
    }
    if (out.format != PixelFormat::NV12 || out.planeCount < 2) {
        return false;
    }

    ID3D11Texture2D* flowTexture = flow_.ExecuteAndGetFlow();
    if (!flowTexture) {
        return false;
    }
    if (!flowSrv_) {
        if (FAILED(device_->CreateShaderResourceView(flowTexture, nullptr,
                                                     &flowSrv_))) {
            return false;
        }
    }

    ID3D11DeviceContext* ctx = context_.Get();
    ID3D11UnorderedAccessView* noUav = nullptr;

    // 1) NV12 -> RGBA for both neighbours.
    for (int i = 0; i < 2; ++i) {
        ID3D11ShaderResourceView* srvs[2] = {yInSrv_[i].Get(), uvInSrv_[i].Get()};
        ID3D11UnorderedAccessView* uavs[1] = {rgbaUav_[i].Get()};
        ctx->CSSetShaderResources(0, 2, srvs);
        ctx->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);
        Dispatch(ctx, csNv12ToRgba_.Get(), width_, height_);
        ctx->CSSetUnorderedAccessViews(0, 1, &noUav, nullptr);
    }

    // 2) Warp both frames along the flow and blend them.
    {
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (FAILED(ctx->Map(params_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0,
                            &mapped))) {
            return false;
        }
        Params params{};
        params.alpha = alpha;
        params.invGrid = 1.0f / static_cast<float>(flow_.GridSize());
        params.width = static_cast<UINT>(width_);
        params.height = static_cast<UINT>(height_);
        std::memcpy(mapped.pData, &params, sizeof(params));
        ctx->Unmap(params_.Get(), 0);

        ID3D11ShaderResourceView* srvs[3] = {flowSrv_.Get(), rgbaSrv_[0].Get(),
                                             rgbaSrv_[1].Get()};
        ID3D11UnorderedAccessView* uavs[1] = {midUav_.Get()};
        ctx->CSSetShaderResources(0, 3, srvs);
        ctx->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);
        ctx->CSSetSamplers(0, 1, sampler_.GetAddressOf());
        ctx->CSSetConstantBuffers(0, 1, params_.GetAddressOf());
        Dispatch(ctx, csWarpBlend_.Get(), width_, height_);
        ctx->CSSetUnorderedAccessViews(0, 1, &noUav, nullptr);
    }

    // 3) RGBA -> NV12 planes.
    {
        ID3D11ShaderResourceView* srvs[1] = {midSrv_.Get()};
        ID3D11UnorderedAccessView* uavs[2] = {yOutUav_.Get(), uvOutUav_.Get()};
        ctx->CSSetShaderResources(0, 1, srvs);
        ctx->CSSetUnorderedAccessViews(0, 2, uavs, nullptr);
        Dispatch(ctx, csRgbaToNv12_.Get(), width_, height_);
        ID3D11UnorderedAccessView* noUavs[2] = {nullptr, nullptr};
        ctx->CSSetUnorderedAccessViews(0, 2, noUavs, nullptr);
    }

    // 4) Read the result back into the caller's NV12 buffers.
    ctx->CopyResource(yStage_.Get(), yOut_.Get());
    ctx->CopyResource(uvStage_.Get(), uvOut_.Get());

    const Plane& outLuma = out.planes[0];
    const Plane& outChroma = out.planes[1];
    if (!outLuma.data || !outChroma.data) {
        return false;
    }

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(ctx->Map(yStage_.Get(), 0, D3D11_MAP_READ, 0, &mapped))) {
        return false;
    }
    for (int y = 0; y < height_; ++y) {
        std::memcpy(outLuma.data + static_cast<size_t>(y) * outLuma.stride,
                    static_cast<uint8_t*>(mapped.pData) +
                        static_cast<size_t>(y) * mapped.RowPitch,
                    static_cast<size_t>(width_));
    }
    ctx->Unmap(yStage_.Get(), 0);

    if (FAILED(ctx->Map(uvStage_.Get(), 0, D3D11_MAP_READ, 0, &mapped))) {
        return false;
    }
    const int chromaHeight = (height_ + 1) / 2;
    const int chromaWidth = ((width_ + 1) / 2) * 2;   // R8G8 texel = 2 bytes
    for (int y = 0; y < chromaHeight; ++y) {
        std::memcpy(outChroma.data + static_cast<size_t>(y) * outChroma.stride,
                    static_cast<uint8_t*>(mapped.pData) +
                        static_cast<size_t>(y) * mapped.RowPitch,
                    static_cast<size_t>(chromaWidth));
    }
    ctx->Unmap(uvStage_.Get(), 0);

    return true;
}

} // namespace vfi
