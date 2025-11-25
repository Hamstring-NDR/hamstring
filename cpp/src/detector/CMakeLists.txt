# Detector module
add_library(hamstring_detector
    feature_extractor.cpp
)

target_link_libraries(hamstring_detector
    PUBLIC
        hamstring_base
)

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
