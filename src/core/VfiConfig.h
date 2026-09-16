#pragma once

// Runtime configuration for the filter, shown in its property page and stored
// in HKCU. Kept free of DirectShow headers so it can be shared by the filter,
// the property page and the engines.

#include <windows.h>

#include <cstdint>

namespace vfi {

// 1 is "off" and handled by the enable flag; the range mirrors DLSS MFG, which
// generates up to 5 additional frames (6x).
inline constexpr int kMinMultiplier = 2;
inline constexpr int kMaxMultiplier = 6;

struct Config {
    bool interpolationEnabled = true;
    int multiplier = 2;            // 2..6
    bool superResolutionEnabled = false;
    int superResolutionMode = 0;   // reserved
};

// Reads/writes HKCU\Software\PotPlayerVfi.
class ConfigStore {
public:
    static Config Load();
    static void Save(const Config& config);
};

} // namespace vfi

// The property page talks to the filter through this tiny COM interface so it
// never needs to know about CTransformFilter internals.
// {7F2A6E12-9C41-4B2E-8D77-5A1C3E9B0F03}
DEFINE_GUID(IID_IVfiConfig,
    0x7f2a6e12, 0x9c41, 0x4b2e, 0x8d, 0x77, 0x5a, 0x1c, 0x3e, 0x9b, 0x0f, 0x03);

DECLARE_INTERFACE_(IVfiConfig, IUnknown) {
    STDMETHOD(GetVfiConfig)(vfi::Config * config) PURE;
    STDMETHOD(SetVfiConfig)(const vfi::Config* config) PURE;
};
