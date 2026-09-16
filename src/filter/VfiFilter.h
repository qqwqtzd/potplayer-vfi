#pragma once

#include <streams.h>

#include <memory>
#include <vector>

#include "FilterGuids.h"
#include "core/IVfiEngine.h"
#include "core/VfiConfig.h"

// DirectShow transform filter that inserts motion-compensated in-between frames
// between decoded frames, using the RTX Optical Flow Accelerator.
//
// Why a transform filter and not a renderer hook: DmitriRender, SVP and
// Bluesky all ship as DirectShow filters, because DirectShow guarantees the
// filter sees every decoded frame in order and lets it emit frames with its
// own timestamps. PotPlayer's "Add external filter" loads it directly, and
// enabling/disabling it in that list is the on/off switch.
//
// The 1-in / N-out delivery is the whole trick and lives in Transform(). The
// frame-rate multiplier and the on/off switch come from the property page via
// IVfiConfig.
class CVfiFilter final : public CTransformFilter,
                         public ISpecifyPropertyPages,
                         public IVfiConfig {
public:
    static CUnknown* WINAPI CreateInstance(LPUNKNOWN lpunk, HRESULT* phr);
    DECLARE_IUNKNOWN

    CVfiFilter(LPUNKNOWN lpunk, HRESULT* phr);
    ~CVfiFilter() override;

    // CUnknown
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv) override;

    // ISpecifyPropertyPages
    STDMETHODIMP GetPages(CAUUID* pPages) override;

    // IVfiConfig
    STDMETHODIMP GetVfiConfig(vfi::Config* config) override;
    STDMETHODIMP SetVfiConfig(const vfi::Config* config) override;

    // CTransformFilter
    HRESULT CheckInputType(const CMediaType* mtIn) override;
    HRESULT CheckTransform(const CMediaType* mtIn, const CMediaType* mtOut) override;
    HRESULT GetMediaType(int iPosition, CMediaType* pMediaType) override;
    HRESULT DecideBufferSize(IMemAllocator* pAlloc, ALLOCATOR_PROPERTIES* pProps) override;
    HRESULT SetMediaType(PIN_DIRECTION dir, const CMediaType* pmt) override;
    HRESULT Transform(IMediaSample* pIn, IMediaSample* pOut) override;
    HRESULT NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate) override;

private:
    // Multiplier actually in effect: the configured one when interpolation is
    // on, otherwise 1 (straight pass-through).
    int EffectiveMultiplier() const;

    bool ParseVideoType(const CMediaType* pmt);
    vfi::Frame MakeFrame(uint8_t* data) const;
    vfi::Frame MakeFrame(const std::vector<uint8_t>& buffer) const;

    // Emits (multiplier - 1) in-between frames for the pair (m_prev, cur).
    HRESULT DeliverInBetweens(const vfi::Frame& from, const vfi::Frame& to);

    vfi::Config m_config;
    std::unique_ptr<vfi::IVfiEngine> m_engine;
    int m_multiplier = vfi::kVfiMultiplier;

    vfi::PixelFormat m_format = vfi::PixelFormat::Unknown;
    int m_width = 0;
    int m_height = 0;
    int m_yStride = 0;
    int m_uvStride = 0;
    int m_frameBytes = 0;
    vfi::Time m_outFrameTime = 0;   // AvgTimePerFrame of the output pin

    bool m_havePrev = false;
    std::vector<uint8_t> m_prev;
    vfi::Time m_prevStart = 0;
    vfi::Time m_prevEnd = 0;
};
