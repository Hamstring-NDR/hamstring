# Detector tests
add_executable(test_feature_extractor
    test_feature_extractor.cpp
)

target_link_libraries(test_feature_extractor
    PRIVATE
        hamstring_detector
        hamstring_base
        GTest::gtest
        GTest::gtest_main
)

add_test(NAME FeatureExtractorTest COMMAND test_feature_extractor)
