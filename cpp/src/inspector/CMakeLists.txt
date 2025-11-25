# Inspector library
add_library(hamstring_inspector
    inspector.cpp
    anomaly_detector.cpp
)

target_link_libraries(hamstring_inspector
    PUBLIC
        hamstring_base
)

# Inspector executable
add_executable(inspector
    main.cpp
)

target_link_libraries(inspector
    PRIVATE
        hamstring_inspector
        hamstring_base
)
