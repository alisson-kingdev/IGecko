function(enable_sanitizers target)

    option(IGECKO_ENABLE_SANITIZERS "Enable sanitizers" OFF)

    if(IGECKO_ENABLE_SANITIZERS AND NOT MSVC)

        target_compile_options(${target} PRIVATE
            -fsanitize=address
            -fsanitize=undefined
            -fno-omit-frame-pointer
        )

        target_link_options(${target} PRIVATE
            -fsanitize=address
            -fsanitize=undefined
        )

    endif()

endfunction()
