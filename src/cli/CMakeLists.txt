# Standalone command-line audio processor — used for validation and as a
# reference implementation for the plugin.  Reads raw float32 from stdin,
# writes processed float32 to stdout.

add_executable(valvra_process
    valvra_process.cpp
)

target_link_libraries(valvra_process
    PRIVATE
        valvra_dsp
)

target_include_directories(valvra_process PRIVATE
    ${CMAKE_SOURCE_DIR}/vendor
)

target_compile_features(valvra_process PUBLIC cxx_std_20)

# dr_wav is a C library; silence warnings about its style in our strict build.
if(MSVC)
    target_compile_options(valvra_process PRIVATE /W4)
else()
    target_compile_options(valvra_process PRIVATE
        -Wall -Wextra -Wpedantic
        -Wno-unused-function -Wno-unused-variable
    )
endif()
