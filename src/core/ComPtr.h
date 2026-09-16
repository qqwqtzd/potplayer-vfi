#pragma once

// Minimal COM smart pointer, enough for the D3D11 resources used by GpuPipeline.
// Deliberately not WRL so the header does not depend on <wrl/client.h>, which
// proved to be order-sensitive across translation units.

#include <cstddef>
#include <utility>

namespace vfi {

template <class T>
class ComPtr {
public:
    ComPtr() noexcept = default;
    ComPtr(std::nullptr_t) noexcept {}
    explicit ComPtr(T* p) noexcept : p_(p) {}

    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;

    ComPtr(ComPtr&& other) noexcept : p_(other.p_) { other.p_ = nullptr; }
    ComPtr& operator=(ComPtr&& other) noexcept {
        if (this != &other) {
            Reset();
            p_ = other.p_;
            other.p_ = nullptr;
        }
        return *this;
    }

    ~ComPtr() { Reset(); }

    T* Get() const noexcept { return p_; }
    T** GetAddressOf() noexcept { return &p_; }
    T* const* GetAddressOf() const noexcept { return &p_; }
    T** operator&() noexcept { return &p_; }
    T* operator->() const noexcept { return p_; }
    explicit operator bool() const noexcept { return p_ != nullptr; }

    void Reset() noexcept {
        if (p_) {
            p_->Release();
            p_ = nullptr;
        }
    }
    void Release() noexcept { Reset(); }

private:
    T* p_ = nullptr;
};

} // namespace vfi
