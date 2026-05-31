# Core DSP library — plugin-framework independent, testable in isolation
# Reference: docs/01, 02, 07, 22

add_library(valvra_dsp STATIC
    KorenTriode.cpp
    KorenPentode.cpp
    JilesAtherton.cpp
    CathodeBounce.cpp
    PowerSupplySag.cpp
    ComponentVariation.cpp
)

target_include_directories(valvra_dsp
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(valvra_dsp PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(valvra_dsp PRIVATE /W4 /fp:fast /arch:AVX2)
else()
    target_compile_options(valvra_dsp PRIVATE -Wall -Wextra -Wpedantic -ffast-math)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
        target_compile_options(valvra_dsp PRIVATE -mavx2 -mfma)
    endif()
endif()

# Position-independent code for plugin linking
set_target_properties(valvra_dsp PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)
