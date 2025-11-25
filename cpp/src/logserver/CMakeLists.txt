# LogServer module
add_library(hamstring_logserver
    logserver.cpp
)

target_link_libraries(hamstring_logserver
    PUBLIC
        hamstring_base
)

# LogServer executable
add_executable(logserver
    main.cpp
)

target_link_libraries(logserver
    PRIVATE
        hamstring_logserver
        hamstring_base
)

install(TARGETS logserver DESTINATION bin)
