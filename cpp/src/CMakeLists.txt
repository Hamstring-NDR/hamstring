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
        yaml-cpp
        spdlog::spdlog
        fmt::fmt
        nlohmann_json::nlohmann_json
        OpenSSL::SSL
        OpenSSL::Crypto
        RdKafka::rdkafka++
)

target_include_directories(hamstring_base
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
)

# Modules
add_subdirectory(logserver)
add_subdirectory(logcollector)
add_subdirectory(prefilter)
add_subdirectory(inspector)
add_subdirectory(detector)
# add_subdirectory(monitoring)
