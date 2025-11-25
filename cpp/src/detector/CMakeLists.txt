# Detector module
add_library(hamstring_detector
    feature_extractor.cpp
    detector.cpp
    detector_service.cpp
)

target_link_libraries(hamstring_detector
    PUBLIC
        hamstring_base
        onnxruntime
)

add_executable(test_detector test_detector.cpp)
target_link_libraries(test_detector PRIVATE hamstring_detector)

add_executable(detector main.cpp)
target_link_libraries(detector
    PRIVATE
        hamstring_detector
        hamstring_base
)
install(TARGETS detector DESTINATION bin)

# TODO: Add detector executable when ready
# add_executable(detector
#     main.cpp
# )
#
# target_link_libraries(detector
#     PRIVATE
#         hamstring_detector
#         hamstring_base
# )
#
# install(TARGETS detector DESTINATION bin)
