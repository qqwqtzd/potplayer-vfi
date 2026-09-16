#include "gpu/GpuOpticalFlow.h"

#include <cstring>
#include <utility>

namespace vfi {

GpuOpticalFlow::~GpuOpticalFlow() {
    Shutdown();
}

bool GpuOpticalFlow::Initialize(ID3D11Device* device,
                                ID3D11DeviceContext* context,
                                int width, int height) {
    Shutdown();

    if (!device || !context || width <= 0 || height <= 0) {
        return false;
    }
    device_ = device;
    context_ = context;
    width_ = width;
    height_ = height;

    try {
        nvof_ = NvOFD3D11::Create(device, context,
                                  static_cast<uint32_t>(width),
                                  static_cast<uint32_t>(height),
                                  NV_OF_BUFFER_FORMAT_GRAYSCALE8,
                                  NV_OF_MODE_OPTICALFLOW,
                                  NV_OF_PERF_LEVEL_FAST);
    } catch (...) {
        nvof_.reset();
        return false;
    }
    if (!nvof_) {
        return false;
    }

    // Ask for per-pixel flow; fall back to whatever the hardware minimum is.
    uint32_t grid = NV_OF_OUTPUT_VECTOR_GRID_SIZE_1;
    if (!nvof_->CheckGridSize(grid)) {
        uint32_t minGrid = 0;
        if (!nvof_->GetNextMinGridSize(grid, minGrid)) {
            Shutdown();
            return false;
        }
        grid = minGrid;
    }

    try {
        nvof_->Init(grid);
    } catch (...) {
        Shutdown();
        return false;
    }

    try {
        auto ins = nvof_->CreateBuffers(NV_OF_BUFFER_USAGE_INPUT, 2);
        if (ins.size() < 2) {
            Shutdown();
            return false;
        }
        inputs_ = std::move(ins);

        auto outs = nvof_->CreateBuffers(NV_OF_BUFFER_USAGE_OUTPUT, 1);
        if (outs.empty()) {
            Shutdown();
            return false;
        }
        output_ = std::move(outs[0]);
    } catch (...) {
        Shutdown();
        return false;
    }

    gridSize_ = static_cast<int>(grid);
    return true;
}

void GpuOpticalFlow::Shutdown() {
    output_.reset();
    inputs_.clear();
    nvof_.reset();
    device_ = nullptr;
    context_ = nullptr;
    width_ = 0;
    height_ = 0;
    gridSize_ = 1;
}

bool GpuOpticalFlow::UploadLuma(int slot, const uint8_t* luma, int stride) {
    if (!luma || slot < 0 || slot >= static_cast<int>(inputs_.size())) {
        return false;
    }
    if (stride < width_) {
        return false;
    }

    auto* buffer = static_cast<NvOFBufferD3D11*>(inputs_[slot].get());
    if (!buffer) {
        return false;
    }

    if (stride == width_) {
        buffer->UploadData(luma, nullptr, nullptr);
        return true;
    }

    // The OFA input texture is tightly packed; repack when the decoder's row
    // pitch has padding.
    std::vector<uint8_t> packed(static_cast<size_t>(width_) * height_);
    for (int y = 0; y < height_; ++y) {
        std::memcpy(packed.data() + static_cast<size_t>(y) * width_,
                    luma + static_cast<size_t>(y) * stride,
                    static_cast<size_t>(width_));
    }
    buffer->UploadData(packed.data(), nullptr, nullptr);
    return true;
}

ID3D11Texture2D* GpuOpticalFlow::ExecuteAndGetFlow() {
    if (!IsReady() || inputs_.size() < 2 || !output_) {
        return nullptr;
    }

    try {
        nvof_->Execute(inputs_[0].get(), inputs_[1].get(), output_.get());
    } catch (...) {
        return nullptr;
    }

    auto* buffer = static_cast<NvOFBufferD3D11*>(output_.get());
    return buffer ? buffer->getD3D11TextureHandle() : nullptr;
}

} // namespace vfi
