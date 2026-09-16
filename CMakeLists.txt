cmake_minimum_required(VERSION 3.24)
project(potplayer_vfi VERSION 0.1.0 LANGUAGES CXX RC)

# DirectShow is a Windows-only technology. This project is authored on any OS
# but can only be configured/built on Windows with the MSVC toolchain, because
# it depends on the Windows SDK (strmif.h, d3d11.h) and DirectShow BaseClasses.
if(NOT WIN32)
    message(FATAL_ERROR
        "potplayer-vfi is a Windows DirectShow filter. Configure and build it on "
        "Windows with MSVC (Visual Studio 2022 + Windows 10/11 SDK).")
endif()

if(MSVC)
    add_compile_options(/W4 /MP /permissive-)
    add_compile_definitions(
        _WINDOWS
        UNICODE
        _UNICODE
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -----------------------------------------------------------------------------
# DirectShow BaseClasses
#
# The Windows 10/11 SDK no longer ships the BaseClasses sources. Drop them into
# third_party/baseclasses (the classic "strmbase" sources), e.g. from
# microsoft/Windows-classic-samples -> Samples/Multimedia/DirectShow/BaseClasses
# or from the legacy DirectX SDK.
# -----------------------------------------------------------------------------
file(GLOB BASECLASSES_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/baseclasses/*.cpp")
if(NOT BASECLASSES_SOURCES)
    message(FATAL_ERROR
        "third_party/baseclasses/*.cpp not found. Put the DirectShow BaseClasses "
        "sources there before building.")
endif()

add_library(strmbase STATIC ${BASECLASSES_SOURCES})
target_include_directories(strmbase PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/baseclasses")
target_compile_definitions(strmbase PRIVATE _CRT_SECURE_NO_WARNINGS)

# -----------------------------------------------------------------------------
# NVIDIA Optical Flow SDK wrapper
#
# The SDK source tree ships its own C++ wrappers. They do NOT link an import
# library: NvOFAPI::LoadNvOFAPI() does LoadLibrary("nvofapi64.dll") at runtime,
# so that DLL must be present on the target machine (the NVIDIA display driver
# normally installs it into C:\Windows\System32).
# -----------------------------------------------------------------------------
set(NVOF_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/nvof/Optical_Flow_SDK_5.0.7")
add_library(nvof_wrapper STATIC
    "${NVOF_ROOT}/Common/NvOFBase/NvOF.cpp"
    "${NVOF_ROOT}/Common/NvOFBase/NvOFD3D11.cpp"
    "${NVOF_ROOT}/Common/NvOFBase/NvOFD3DCommon.cpp"
)
target_include_directories(nvof_wrapper PUBLIC
    "${NVOF_ROOT}/NvOFInterface"
    "${NVOF_ROOT}/Common/NvOFBase"
)
target_link_libraries(nvof_wrapper PUBLIC d3d11 dxgi)

# -----------------------------------------------------------------------------
# The filter
# -----------------------------------------------------------------------------
add_library(potplayer_vfi SHARED
    src/filter/dllmain.cpp
    src/filter/VfiFilter.cpp
    src/core/BlendEngine.cpp
    src/core/VfiConfig.cpp
    src/gpu/GpuOpticalFlow.cpp
    src/gpu/GpuPipeline.cpp
    src/gpu/OfaEngine.cpp
    src/filter/VfiPropertyPage.cpp
    src/filter/vfi.rc
)

target_include_directories(potplayer_vfi PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src")

target_link_libraries(potplayer_vfi PRIVATE
    nvof_wrapper
    advapi32        # config registry access
    strmbase
    strmiids        # CLSID/IID definitions for DirectShow
    quartz          # DirectShow runtime
    ole32
    oleaut32
    uuid
    d3d11
    dxgi
    d3dcompiler
    dxguid
)

set_target_properties(potplayer_vfi PROPERTIES
    OUTPUT_NAME "VfiFilter"
    PREFIX ""
    SUFFIX ".ax"    # DirectShow filters are traditionally .ax, .dll works too
)

# Copy the HLSL sources next to the built binary so the runtime D3DCompile
# loader can find them during development.
add_custom_command(TARGET potplayer_vfi POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/src/shaders"
        "$<TARGET_FILE_DIR:potplayer_vfi>/shaders")
