#include <windows.h>

#include <initguid.h>   // must precede FilterGuids.h so DEFINE_GUID allocates

#include <olectl.h>
#include <streams.h>

#include "FilterGuids.h"
#include "filter/VfiFilter.h"
#include "filter/VfiPropertyPage.h"

namespace {

const AMOVIESETUP_MEDIATYPE kPinTypes[] = {
    {&MEDIATYPE_Video, &MEDIASUBTYPE_NV12},
    {&MEDIATYPE_Video, &MEDIASUBTYPE_YV12},
    {&MEDIATYPE_Video, &MEDIASUBTYPE_RGB32},
    {&MEDIATYPE_Video, &MEDIASUBTYPE_RGB24},
};

const AMOVIESETUP_PIN kPins[] = {
    {
        const_cast<LPWSTR>(L"Input"),   // strName
        FALSE,                          // bRendered
        FALSE,                          // bOutput
        FALSE,                          // bZero
        FALSE,                          // bMany
        &CLSID_NULL,                    // clsConnectsToFilter
        nullptr,                        // strConnectsToPin
        1,                              // nTypes
        kPinTypes,                      // lpTypes
    },
    {
        const_cast<LPWSTR>(L"Output"),
        FALSE,
        TRUE,
        FALSE,
        FALSE,
        &CLSID_NULL,
        nullptr,
        1,
        kPinTypes,
    },
};

const AMOVIESETUP_FILTER kFilter = {
    &CLSID_VfiFilter,      // clsID
    VFI_FILTER_NAME,       // strName
    MERIT_DO_NOT_USE,      // dwMerit
    2,                     // nPins
    kPins,                 // lpPin
};

} // namespace

CFactoryTemplate g_Templates[] = {
    {
        VFI_FILTER_NAME,
        &CLSID_VfiFilter,
        CVfiFilter::CreateInstance,
        nullptr,
        &kFilter,
    },
    {
        const_cast<LPWSTR>(L"VFI Properties"),
        &CLSID_VfiPropertyPage,
        CVfiPropertyPage::CreateInstance,
        nullptr,
        nullptr,
    },
};
int g_cTemplates = sizeof(g_Templates) / sizeof(g_Templates[0]);

STDAPI DllRegisterServer() {
    return AMovieDllRegisterServer2(TRUE);
}

STDAPI DllUnregisterServer() {
    return AMovieDllRegisterServer2(FALSE);
}

extern "C" BOOL WINAPI DllEntryPoint(HINSTANCE, ULONG, LPVOID);

BOOL WINAPI DllMain(HANDLE hDll, DWORD dwReason, LPVOID lpReserved) {
    return DllEntryPoint(reinterpret_cast<HINSTANCE>(hDll), dwReason, lpReserved);
}
