#pragma once

// {7F2A6E10-9C41-4B2E-8D77-5A1C3E9B0F01}
DEFINE_GUID(CLSID_VfiFilter,
    0x7f2a6e10, 0x9c41, 0x4b2e, 0x8d, 0x77, 0x5a, 0x1c, 0x3e, 0x9b, 0x0f, 0x01);

// Friendly name shown in GraphEdit / player filter lists.
#define VFI_FILTER_NAME     L"PotPlayer VFI (frame interpolation)"

// Property page (not implemented yet, reserved).
// {7F2A6E11-9C41-4B2E-8D77-5A1C3E9B0F02}
DEFINE_GUID(CLSID_VfiPropertyPage,
    0x7f2a6e11, 0x9c41, 0x4b2e, 0x8d, 0x77, 0x5a, 0x1c, 0x3e, 0x9b, 0x0f, 0x02);
