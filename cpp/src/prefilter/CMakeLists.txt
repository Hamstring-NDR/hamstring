# Prefilter library
add_library(hamstring_prefilter
    prefilter.cpp
)

target_link_libraries(hamstring_prefilter
    PUBLIC
        hamstring_base
)

# Prefilter executable
add_executable(prefilter
    main.cpp
)

target_link_libraries(prefilter
    PRIVATE
        hamstring_prefilter
        hamstring_base
)
