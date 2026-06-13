function(enable_lto target)
    option(IGECKO_ENABLE_LTO "Enable Link Time Optimization" ON)

    if(IGECKO_ENABLE_LTO)
        include(CheckIPOSupported)
        check_ipo_supported(RESULT ipo_supported)

        if(ipo_supported)
            set_property(TARGET ${target} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
        endif()
    endif()
endfunction()
