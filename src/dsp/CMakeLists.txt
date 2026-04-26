# Core DSP library — plugin-framework independent, testable in isolation
# Reference: docs/01, 02, 07, 22

add_library(valvra_dsp STATIC
    KorenTriode.cpp
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

# Position-independent code for plugin linking
set_target_properties(valvra_dsp PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)
