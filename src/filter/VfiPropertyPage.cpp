#include "filter/VfiPropertyPage.h"

#include "filter/resource.h"

CVfiPropertyPage::CVfiPropertyPage(LPUNKNOWN lpunk, HRESULT* phr)
    : CBasePropertyPage(NAME("VFI Properties"), lpunk, IDD_VFI_PROPERTIES,
                        L"VFI Settings") {
    if (phr && FAILED(*phr)) {
        return;
    }
    config_ = vfi::ConfigStore::Load();
}

CUnknown* WINAPI CVfiPropertyPage::CreateInstance(LPUNKNOWN lpunk,
                                                  HRESULT* phr) {
    auto* page = new CVfiPropertyPage(lpunk, phr);
    if (page && phr && FAILED(*phr)) {
        delete page;
        return nullptr;
    }
    return page;
}

HRESULT CVfiPropertyPage::OnConnect(IUnknown* pUnknown) {
    if (!pUnknown) {
        return E_POINTER;
    }
    // Prefer the live filter config; fall back to what we loaded from HKCU.
    const HRESULT hr =
        pUnknown->QueryInterface(IID_IVfiConfig,
                                 reinterpret_cast<void**>(&configIface_));
    if (SUCCEEDED(hr) && configIface_) {
        configIface_->GetVfiConfig(&config_);
    } else {
        configIface_ = nullptr;
    }
    return S_OK;
}

HRESULT CVfiPropertyPage::OnDisconnect() {
    if (configIface_) {
        configIface_->Release();
        configIface_ = nullptr;
    }
    return S_OK;
}

HRESULT CVfiPropertyPage::OnActivate() {
    HWND hwnd = m_hwnd;

    CheckDlgButton(hwnd, IDC_CHK_INTERPOLATION,
                   config_.interpolationEnabled ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hwnd, IDC_CHK_SUPERRES,
                   config_.superResolutionEnabled ? BST_CHECKED
                                                  : BST_UNCHECKED);

    PopulateMultipliers();

    SetDlgItemTextW(hwnd, IDC_STATIC_NOTE,
                    L"Interpolation uses the RTX Optical Flow Accelerator; "
                    L"changes apply on the next file / filter reload.\n"
                    L"Super resolution becomes available once the RTX Video "
                    L"SDK is integrated.");
    return S_OK;
}

void CVfiPropertyPage::PopulateMultipliers() {
    HWND combo = GetDlgItem(m_hwnd, IDC_CMB_MULTIPLIER);
    if (!combo) {
        return;
    }
    SendMessageW(combo, CB_RESETCONTENT, 0, 0);

    int select = 0;
    for (int i = vfi::kMinMultiplier; i <= vfi::kMaxMultiplier; ++i) {
        wchar_t label[16];
        wsprintfW(label, L"%dx", i);
        const int index =
            static_cast<int>(SendMessageW(combo, CB_ADDSTRING, 0,
                                          reinterpret_cast<LPARAM>(label)));
        SendMessageW(combo, CB_SETITEMDATA, index, i);
        if (i == config_.multiplier) {
            select = index;
        }
    }
    SendMessageW(combo, CB_SETCURSEL, select, 0);
}

HRESULT CVfiPropertyPage::OnDeactivate() {
    return S_OK;
}

HRESULT CVfiPropertyPage::OnApplyChanges() {
    config_.interpolationEnabled =
        IsDlgButtonChecked(m_hwnd, IDC_CHK_INTERPOLATION) == BST_CHECKED;
    config_.superResolutionEnabled =
        IsDlgButtonChecked(m_hwnd, IDC_CHK_SUPERRES) == BST_CHECKED;

    HWND combo = GetDlgItem(m_hwnd, IDC_CMB_MULTIPLIER);
    if (combo) {
        const int sel =
            static_cast<int>(SendMessageW(combo, CB_GETCURSEL, 0, 0));
        if (sel != CB_ERR) {
            const int multiplier = static_cast<int>(
                SendMessageW(combo, CB_GETITEMDATA, sel, 0));
            if (multiplier >= vfi::kMinMultiplier &&
                multiplier <= vfi::kMaxMultiplier) {
                config_.multiplier = multiplier;
            }
        }
    }

    if (configIface_) {
        configIface_->SetVfiConfig(&config_);
    }
    vfi::ConfigStore::Save(config_);
    return S_OK;
}

BOOL CVfiPropertyPage::OnReceiveMessage(HWND hwnd, UINT msg, WPARAM wParam,
                                        LPARAM lParam) {
    switch (msg) {
        case WM_COMMAND:
            if (HIWORD(wParam) == BN_CLICKED ||
                HIWORD(wParam) == CBN_SELCHANGE) {
                m_bDirty = TRUE;
                if (m_pPageSite) {
                    m_pPageSite->OnStatusChange(PROPPAGESTATUS_DIRTY);
                }
                return TRUE;
            }
            break;
        default:
            break;
    }
    return FALSE;
}
