# Placeholder for base tests
add_executable(test_utils
    test_utils.cpp
)

target_link_libraries(test_utils
    PRIVATE
        hamstring_base
        GTest::gtest
        GTest::gtest_main
)

add_test(NAME UtilsTest COMMAND test_utils)
