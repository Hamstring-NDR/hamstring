# Base sources
set(BASE_SOURCES
    base/logger.cpp
    base/utils.cpp
    base/data_classes.cpp
    base/clickhouse_sender.cpp
    base/kafka_handler.cpp
)

# Config sources  
set(CONFIG_SOURCES
    config/config.cpp
)

# Base library with core infrastructure
add_library(hamstring_base
    ${BASE_SOURCES}
    ${CONFIG_SOURCES}
)

target_link_libraries(hamstring_base
    PUBLIC
        spdlog::spdlog
        fmt::fmt
        RdKafka::rdkafka
        RdKafka::rdkafka++
        nlohmann_json::nlohmann_json
        yaml-cpp
        OpenSSL::SSL
        OpenSSL::Crypto
        ${CLICKHOUSE_CPP_LIB}
        ${ZSTD_LIB}
        ${CITYHASH_LIB}
)

target_include_directories(hamstring_base
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${CLICKHOUSE_INCLUDE_DIR}
)

# Modules
add_subdirectory(logserver)
add_subdirectory(logcollector)
add_subdirectory(prefilter)
add_subdirectory(inspector)
# add_subdirectory(detector)  # Requires ONNX Runtime
# add_subdirectory(monitoring)
