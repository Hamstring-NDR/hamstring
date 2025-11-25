# Placeholder for integration tests
add_executable(test_pipeline
    test_pipeline.cpp
)

target_link_libraries(test_pipeline
    PRIVATE
        hamstring_base
        GTest::gtest
        GTest::gtest_main
)

add_test(NAME PipelineTest COMMAND test_pipeline)
