# Example executable
add_executable(demo
    demo.cpp
)

target_link_libraries(demo
    PRIVATE
        hamstring_base
        hamstring_detector
)
