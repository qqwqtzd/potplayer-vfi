#pragma once

#include <streams.h>

#include "core/VfiConfig.h"

// Property page shown by PotPlayer's "Filter properties" dialog. Edits
// vfi::Config and pushes it to the filter through IID_IVfiConfig, then persists
// it to HKCU.
//
// Multiplier / on-off changes take effect on the next pin reconnection (i.e.
// the next file, or after toggling the external filter), because the output
// frame rate is negotiated at connection time.
class CVfiPropertyPage final : public CBasePropertyPage {
public:
    static CUnknown* WINAPI CreateInstance(LPUNKNOWN lpunk, HRESULT* phr);

    CVfiPropertyPage(LPUNKNOWN lpunk, HRESULT* phr);

    HRESULT OnConnect(IUnknown* pUnknown) override;
    HRESULT OnDisconnect() override;
    HRESULT OnActivate() override;
    HRESULT OnDeactivate() override;
    HRESULT OnApplyChanges() override;

    INT_PTR OnReceiveMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) override;

private:
    void PopulateMultipliers();

    vfi::Config config_;
    IVfiConfig* configIface_ = nullptr;
};
