# LogCollector module
add_library(hamstring_logcollector
    logcollector.cpp
)

target_link_libraries(hamstring_logcollector
    PUBLIC
        hamstring_base
)

# LogCollector executable
add_executable(logcollector
    main.cpp
)

target_link_libraries(logcollector
    PRIVATE
        hamstring_logcollector
        hamstring_base
)

install(TARGETS logcollector DESTINATION bin)
