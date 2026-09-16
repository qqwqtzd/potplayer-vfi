#include "filter/VfiFilter.h"

#include <algorithm>
#include <cstring>

#include "core/BlendEngine.h"
#include "gpu/OfaEngine.h"

namespace {
// Delivered in-between samples are held by the renderer while we keep pulling
// from the allocator. Too few buffers and GetDeliveryBuffer() blocks on the
// first burst; 8 gives comfortable headroom for a 2x multiplier.
constexpr int kOutputBufferCount = 8;
} // namespace

CVfiFilter::CVfiFilter(LPUNKNOWN lpunk, HRESULT* phr)
    : CTransformFilter(NAME("PotPlayer VFI"), lpunk, CLSID_VfiFilter),
      m_engine(std::make_unique<vfi::OfaEngine>()) {
    m_config = vfi::ConfigStore::Load();
    m_multiplier = EffectiveMultiplier();
    if (phr && FAILED(*phr)) {
        return;
    }
}

CVfiFilter::~CVfiFilter() = default;

CUnknown* WINAPI CVfiFilter::CreateInstance(LPUNKNOWN lpunk, HRESULT* phr) {
    auto* filter = new CVfiFilter(lpunk, phr);
    if (filter && phr && FAILED(*phr)) {
        delete filter;
        return nullptr;
    }
    return filter;
}

int CVfiFilter::EffectiveMultiplier() const {
    if (!m_config.interpolationEnabled) {
        return 1;
    }
    int m = m_config.multiplier;
    if (m < vfi::kMinMultiplier) {
        m = vfi::kMinMultiplier;
    }
    if (m > vfi::kMaxMultiplier) {
        m = vfi::kMaxMultiplier;
    }
    return m;
}

STDMETHODIMP CVfiFilter::NonDelegatingQueryInterface(REFIID riid, void** ppv) {
    if (!ppv) {
        return E_POINTER;
    }
    if (riid == IID_ISpecifyPropertyPages) {
        return GetInterface(static_cast<ISpecifyPropertyPages*>(this), ppv);
    }
    if (riid == IID_IVfiConfig) {
        return GetInterface(static_cast<IVfiConfig*>(this), ppv);
    }
    return CTransformFilter::NonDelegatingQueryInterface(riid, ppv);
}

STDMETHODIMP CVfiFilter::GetPages(CAUUID* pPages) {
    if (!pPages) {
        return E_POINTER;
    }
    pPages->cElems = 1;
    pPages->pElems =
        static_cast<GUID*>(CoTaskMemAlloc(sizeof(GUID) * pPages->cElems));
    if (!pPages->pElems) {
        pPages->cElems = 0;
        return E_OUTOFMEMORY;
    }
    pPages->pElems[0] = CLSID_VfiPropertyPage;
    return S_OK;
}

STDMETHODIMP CVfiFilter::GetVfiConfig(vfi::Config* config) {
    if (!config) {
        return E_POINTER;
    }
    *config = m_config;
    return S_OK;
}

STDMETHODIMP CVfiFilter::SetVfiConfig(const vfi::Config* config) {
    if (!config) {
        return E_POINTER;
    }
    m_config = *config;
    m_multiplier = EffectiveMultiplier();
    return S_OK;
}

HRESULT CVfiFilter::CheckInputType(const CMediaType* mtIn) {
    if (!mtIn) {
        return E_POINTER;
    }
    if (mtIn->majortype != MEDIATYPE_Video) {
        return VFW_E_TYPE_NOT_ACCEPTED;
    }
    if (mtIn->formattype != FORMAT_VideoInfo) {
        return VFW_E_TYPE_NOT_ACCEPTED;
    }
    const GUID& subtype = mtIn->subtype;
    if (subtype != MEDIASUBTYPE_NV12 && subtype != MEDIASUBTYPE_YV12 &&
        subtype != MEDIASUBTYPE_RGB32 && subtype != MEDIASUBTYPE_RGB24) {
        return VFW_E_TYPE_NOT_ACCEPTED;
    }
    return S_OK;
}

HRESULT CVfiFilter::CheckTransform(const CMediaType* mtIn, const CMediaType* mtOut) {
    if (!mtIn || !mtOut) {
        return E_POINTER;
    }
    if (FAILED(CheckInputType(mtIn))) {
        return VFW_E_TYPE_NOT_ACCEPTED;
    }
    // Pixel layout is untouched; only AvgTimePerFrame changes.
    if (mtIn->subtype != mtOut->subtype || mtIn->formattype != mtOut->formattype) {
        return VFW_E_TYPE_NOT_ACCEPTED;
    }
    return S_OK;
}

HRESULT CVfiFilter::GetMediaType(int iPosition, CMediaType* pMediaType) {
    if (!pMediaType) {
        return E_POINTER;
    }
    if (iPosition < 0) {
        return E_INVALIDARG;
    }
    if (iPosition > 0) {
        return VFW_S_NO_MORE_ITEMS;
    }
    const CMediaType& in = m_pInput->CurrentMediaType();
    if (in.majortype == MEDIATYPE_None) {
        return VFW_S_NO_MORE_ITEMS;
    }

    *pMediaType = in;

    // Advertise the up-converted frame rate so the renderer/sync logic knows
    // it will receive `mult` times as many samples.
    const int mult = std::max(1, EffectiveMultiplier());
    if (pMediaType->formattype == FORMAT_VideoInfo) {
        auto* pvih = reinterpret_cast<VIDEOINFOHEADER*>(pMediaType->pbFormat);
        if (pvih->AvgTimePerFrame > 0) {
            pvih->AvgTimePerFrame =
                std::max<REFERENCE_TIME>(1, pvih->AvgTimePerFrame / mult);
        }
    } else if (pMediaType->formattype == FORMAT_VideoInfo2) {
        auto* pvih2 = reinterpret_cast<VIDEOINFOHEADER2*>(pMediaType->pbFormat);
        if (pvih2->AvgTimePerFrame > 0) {
            pvih2->AvgTimePerFrame =
                std::max<REFERENCE_TIME>(1, pvih2->AvgTimePerFrame / mult);
        }
    }
    return S_OK;
}

HRESULT CVfiFilter::DecideBufferSize(IMemAllocator* pAlloc, ALLOCATOR_PROPERTIES* pProps) {
    if (!pAlloc || !pProps) {
        return E_POINTER;
    }
    if (m_frameBytes <= 0) {
        return E_FAIL;
    }

    ALLOCATOR_PROPERTIES actual{};
    pProps->cbBuffer = std::max<long>(pProps->cbBuffer, m_frameBytes);
    pProps->cBuffers = std::max<long>(pProps->cBuffers, kOutputBufferCount);
    if (pProps->cbAlign < 1) {
        pProps->cbAlign = 1;
    }

    const HRESULT hr = pAlloc->SetProperties(pProps, &actual);
    if (FAILED(hr)) {
        return hr;
    }
    if (actual.cbBuffer < m_frameBytes) {
        return E_FAIL;
    }
    return S_OK;
}

HRESULT CVfiFilter::SetMediaType(PIN_DIRECTION dir, const CMediaType* pmt) {
    const HRESULT hr = CTransformFilter::SetMediaType(dir, pmt);
    if (FAILED(hr)) {
        return hr;
    }

    if (dir == PINDIR_INPUT && pmt) {
        if (!ParseVideoType(pmt)) {
            return VFW_E_TYPE_NOT_ACCEPTED;
        }
        m_multiplier = EffectiveMultiplier();
        vfi::Frame proto;
        proto.format = m_format;
        proto.planeCount = vfi::PlaneCount(m_format);
        proto.width = m_width;
        proto.height = m_height;
        if (m_engine) {
            if (FAILED(m_engine->Configure(proto))) {
                // OFA unavailable (non-RTX GPU, old driver, missing
                // nvofapi64.dll) or a format the GPU path does not handle:
                // degrade to the CPU blend engine so playback still works.
                m_engine = std::make_unique<vfi::BlendEngine>();
                m_engine->Configure(proto);
            }
        }
        m_havePrev = false;
        m_prev.clear();
    } else if (dir == PINDIR_OUTPUT && pmt) {
        if (pmt->formattype == FORMAT_VideoInfo) {
            const auto* pvih = reinterpret_cast<const VIDEOINFOHEADER*>(pmt->pbFormat);
            m_outFrameTime = pvih->AvgTimePerFrame;
        }
    }
    return S_OK;
}

HRESULT CVfiFilter::NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate) {
    m_havePrev = false;
    m_prev.clear();
    if (m_engine) {
        m_engine->Reset();
    }
    return CTransformFilter::NewSegment(tStart, tStop, dRate);
}

HRESULT CVfiFilter::Transform(IMediaSample* pIn, IMediaSample* pOut) {
    if (!pIn || !pOut) {
        return E_POINTER;
    }
    if (m_frameBytes <= 0) {
        return E_FAIL;
    }

    BYTE* inData = nullptr;
    HRESULT hr = pIn->GetPointer(&inData);
    if (FAILED(hr) || !inData) {
        return FAILED(hr) ? hr : E_POINTER;
    }
    if (pIn->GetActualDataLength() < m_frameBytes) {
        return S_FALSE;   // malformed/truncated sample: pass nothing downstream
    }

    vfi::Time tStart = 0;
    vfi::Time tEnd = 0;
    if (FAILED(pIn->GetTime(&tStart, &tEnd))) {
        tStart = m_prevEnd;
        tEnd = tStart + (m_outFrameTime > 0 ? m_outFrameTime * m_multiplier
                                            : static_cast<vfi::Time>(333333));
    }

    BYTE* outData = nullptr;
    hr = pOut->GetPointer(&outData);
    if (FAILED(hr) || !outData) {
        return FAILED(hr) ? hr : E_POINTER;
    }

    vfi::Frame cur = MakeFrame(inData);
    cur.start = tStart;
    cur.end = tEnd;

    if (!m_havePrev) {
        std::memcpy(outData, inData, m_frameBytes);
        pOut->SetTime(&tStart, &tEnd);
        pOut->SetSyncPoint(TRUE);
        pOut->SetPreroll(FALSE);
        pOut->SetMediaTime(nullptr, nullptr);

        m_prev.assign(inData, inData + m_frameBytes);
        m_prevStart = tStart;
        m_prevEnd = tEnd;
        m_havePrev = true;
        return S_OK;
    }

    vfi::Frame from = MakeFrame(m_prev);
    from.start = m_prevStart;
    from.end = m_prevEnd;

    hr = DeliverInBetweens(from, cur);
    if (FAILED(hr)) {
        return hr;
    }

    // The real current frame is emitted last, through the sample that
    // BaseClasses allocated for us; it gets delivered after we return S_OK.
    std::memcpy(outData, inData, m_frameBytes);
    pOut->SetTime(&tStart, &tEnd);
    pOut->SetSyncPoint(TRUE);
    pOut->SetPreroll(FALSE);
    pOut->SetMediaTime(nullptr, nullptr);

    m_prev.assign(inData, inData + m_frameBytes);
    m_prevStart = tStart;
    m_prevEnd = tEnd;
    return S_OK;
}

HRESULT CVfiFilter::DeliverInBetweens(const vfi::Frame& from, const vfi::Frame& to) {
    if (!m_engine) {
        return E_UNEXPECTED;
    }

    const int n = m_multiplier;
    const int count = n - 1;
    if (count <= 0) {
        return S_OK;
    }

    std::vector<std::vector<uint8_t>> scratch(count, std::vector<uint8_t>(m_frameBytes));
    std::vector<vfi::Frame> outs(count);
    for (int i = 0; i < count; ++i) {
        outs[i] = MakeFrame(scratch[i]);
    }

    HRESULT hr = m_engine->Interpolate(from, to, outs.data(), count);
    if (FAILED(hr)) {
        return hr;
    }

    const vfi::Time span = to.start - from.start;

    for (int i = 0; i < count; ++i) {
        IMediaSample* pSample = nullptr;
        hr = m_pOutput->GetDeliveryBuffer(&pSample, nullptr, nullptr, 0);
        if (FAILED(hr)) {
            return hr;
        }

        BYTE* dst = nullptr;
        hr = pSample->GetPointer(&dst);
        if (FAILED(hr) || !dst) {
            pSample->Release();
            return FAILED(hr) ? hr : E_POINTER;
        }
        std::memcpy(dst, scratch[i].data(), m_frameBytes);

        REFERENCE_TIME ts = from.start + span * (i + 1) / n;
        REFERENCE_TIME te = from.start + span * (i + 2) / n;
        pSample->SetTime(&ts, &te);
        pSample->SetSyncPoint(TRUE);
        pSample->SetPreroll(FALSE);
        pSample->SetMediaTime(nullptr, nullptr);

        hr = m_pOutput->Deliver(pSample);
        pSample->Release();
        if (FAILED(hr)) {
            return hr;
        }
    }
    return S_OK;
}

bool CVfiFilter::ParseVideoType(const CMediaType* pmt) {
    if (!pmt || pmt->majortype != MEDIATYPE_Video ||
        pmt->formattype != FORMAT_VideoInfo) {
        return false;
    }
    const auto* pvih = reinterpret_cast<const VIDEOINFOHEADER*>(pmt->pbFormat);
    if (!pvih) {
        return false;
    }
    const BITMAPINFOHEADER& bih = pvih->bmiHeader;
    m_width = bih.biWidth;
    m_height = bih.biHeight < 0 ? -bih.biHeight : bih.biHeight;
    if (m_width <= 0 || m_height <= 0) {
        return false;
    }

    const GUID& subtype = pmt->subtype;
    if (subtype == MEDIASUBTYPE_NV12) {
        m_format = vfi::PixelFormat::NV12;
        m_yStride = m_width;
        m_uvStride = m_width;
        m_frameBytes = m_width * m_height * 3 / 2;
    } else if (subtype == MEDIASUBTYPE_YV12) {
        m_format = vfi::PixelFormat::YV12;
        m_yStride = m_width;
        m_uvStride = m_width / 2;
        m_frameBytes = m_width * m_height * 3 / 2;
    } else if (subtype == MEDIASUBTYPE_RGB32) {
        m_format = vfi::PixelFormat::RGB32;
        m_yStride = m_width * 4;
        m_uvStride = 0;
        m_frameBytes = m_yStride * m_height;
    } else if (subtype == MEDIASUBTYPE_RGB24) {
        m_format = vfi::PixelFormat::RGB24;
        m_yStride = m_width * 3;
        m_uvStride = 0;
        m_frameBytes = m_yStride * m_height;
    } else {
        return false;
    }

    // Decoders occasionally allocate more than the nominal frame (row padding,
    // extra tail). Trust their sample size when it is larger.
    if (pmt->lSampleSize > static_cast<ULONG>(m_frameBytes)) {
        m_frameBytes = static_cast<int>(pmt->lSampleSize);
    }
    return true;
}

vfi::Frame CVfiFilter::MakeFrame(uint8_t* data) const {
    vfi::Frame f;
    f.format = m_format;
    f.width = m_width;
    f.height = m_height;

    switch (m_format) {
        case vfi::PixelFormat::NV12:
            f.planes[0] = { data, m_yStride, m_width, m_height };
            f.planes[1] = {
                data + static_cast<size_t>(m_yStride) * m_height,
                m_uvStride, m_width / 2, m_height / 2};
            f.planeCount = 2;
            break;
        case vfi::PixelFormat::YV12:
            f.planes[0] = { data, m_yStride, m_width, m_height };
            f.planes[1] = {
                data + static_cast<size_t>(m_yStride) * m_height,
                m_yStride / 2, m_width / 2, m_height / 2};
            f.planes[2] = {
                data + static_cast<size_t>(m_yStride) * m_height +
                    static_cast<size_t>(m_yStride / 2) * (m_height / 2),
                m_yStride / 2, m_width / 2, m_height / 2};
            f.planeCount = 3;
            break;
        case vfi::PixelFormat::RGB32:
            f.planes[0] = { data, m_width * 4, m_width, m_height };
            f.planeCount = 1;
            break;
        case vfi::PixelFormat::RGB24:
            f.planes[0] = { data, m_width * 3, m_width, m_height };
            f.planeCount = 1;
            break;
        default:
            f.planeCount = 0;
            break;
    }
    return f;
}

vfi::Frame CVfiFilter::MakeFrame(const std::vector<uint8_t>& buffer) const {
    return MakeFrame(const_cast<uint8_t*>(buffer.data()));
}
