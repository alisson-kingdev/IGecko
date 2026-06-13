option(IGECKO_ENABLE_CLANG_TIDY "Enable clang-tidy" OFF)

if(IGECKO_ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY clang-tidy)

    if(CLANG_TIDY)
        set(CMAKE_CXX_CLANG_TIDY
            ${CLANG_TIDY};
            --use-color
        )
    else()
        message(WARNING "clang-tidy requested but not found")
    endif()
endif()
