#include "core/VfiConfig.h"

namespace vfi {
namespace {

constexpr wchar_t kKeyPath[] = L"Software\\PotPlayerVfi";

bool ReadDword(HKEY key, const wchar_t* name, DWORD* out) {
    DWORD type = 0;
    DWORD size = sizeof(DWORD);
    return RegQueryValueExW(key, name, nullptr, &type,
                            reinterpret_cast<BYTE*>(out),
                            &size) == ERROR_SUCCESS &&
           type == REG_DWORD;
}

} // namespace

Config ConfigStore::Load() {
    Config config;

    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kKeyPath, 0, KEY_READ, &key) !=
        ERROR_SUCCESS) {
        return config;
    }

    DWORD value = 0;
    if (ReadDword(key, L"Interpolation", &value)) {
        config.interpolationEnabled = value != 0;
    }
    if (ReadDword(key, L"Multiplier", &value) &&
        value >= static_cast<DWORD>(kMinMultiplier) &&
        value <= static_cast<DWORD>(kMaxMultiplier)) {
        config.multiplier = static_cast<int>(value);
    }
    if (ReadDword(key, L"SuperResolution", &value)) {
        config.superResolutionEnabled = value != 0;
    }
    if (ReadDword(key, L"SuperResolutionMode", &value)) {
        config.superResolutionMode = static_cast<int>(value);
    }

    RegCloseKey(key);
    return config;
}

void ConfigStore::Save(const Config& config) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kKeyPath, 0, nullptr, 0, KEY_WRITE,
                        nullptr, &key, nullptr) != ERROR_SUCCESS) {
        return;
    }

    DWORD value = config.interpolationEnabled ? 1 : 0;
    RegSetValueExW(key, L"Interpolation", 0, REG_DWORD,
                   reinterpret_cast<const BYTE*>(&value), sizeof(value));

    value = static_cast<DWORD>(config.multiplier);
    RegSetValueExW(key, L"Multiplier", 0, REG_DWORD,
                   reinterpret_cast<const BYTE*>(&value), sizeof(value));

    value = config.superResolutionEnabled ? 1 : 0;
    RegSetValueExW(key, L"SuperResolution", 0, REG_DWORD,
                   reinterpret_cast<const BYTE*>(&value), sizeof(value));

    value = static_cast<DWORD>(config.superResolutionMode);
    RegSetValueExW(key, L"SuperResolutionMode", 0, REG_DWORD,
                   reinterpret_cast<const BYTE*>(&value), sizeof(value));

    RegCloseKey(key);
}

} // namespace vfi
