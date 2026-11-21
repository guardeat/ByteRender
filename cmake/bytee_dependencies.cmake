# Third-party dependencies, pinned by version and SHA-256.
# Header-only use: SOURCE_SUBDIR points at a non-existent directory so the projects' own CMake files never run;
# we define minimal INTERFACE targets ourselves (with SYSTEM includes, so their headers never trigger our warnings).

include(FetchContent)

function(bytee_fetch_doctest)
    if(TARGET bytee_doctest)
        return()
    endif()
    FetchContent_Declare(doctest
        URL https://github.com/doctest/doctest/archive/refs/tags/v2.4.12.tar.gz
        URL_HASH SHA256=73381c7aa4dee704bd935609668cf41880ea7f19fa0504a200e13b74999c2d70
        SOURCE_SUBDIR do-not-build
    )
    FetchContent_MakeAvailable(doctest)
    add_library(bytee_doctest INTERFACE)
    target_include_directories(bytee_doctest SYSTEM INTERFACE "${doctest_SOURCE_DIR}")
endfunction()
