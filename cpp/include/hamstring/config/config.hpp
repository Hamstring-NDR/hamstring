#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <yaml-cpp/yaml.h>

namespace hamstring {
namespace config {

// Logging configuration
struct ModuleLoggingConfig {
    bool debug = false;
};

struct LoggingConfig {
    bool base_debug = false;
    std::map<std::string, ModuleLoggingConfig> modules;
    
    static LoggingConfig from_yaml(const YAML::Node& node);
};

// Kafka broker configuration
struct KafkaBroker {
    std::string hostname;
    int internal_port;
    int external_port;
    std::string node_ip;
    
    static KafkaBroker from_yaml(const YAML::Node& node);
};

// Environment configuration
struct EnvironmentConfig {
    std::vector<KafkaBroker> kafka_brokers;
    std::map<std::string, std::string> kafka_topics_prefix;
    std::string clickhouse_hostname;
    
    static EnvironmentConfig from_yaml(const YAML::Node& node);
    std::string get_kafka_bootstrap_servers() const;
};

// Field validation types
enum class FieldType {
    RegEx,
    Timestamp,
    IpAddress,
    ListItem
};

struct FieldConfig {
    std::string name;
    FieldType type;
    std::string pattern;  // For RegEx
    std::string timestamp_format;  // For Timestamp
    std::vector<std::string> allowed_list;  // For ListItem
    std::vector<std::string> relevant_list;  // For ListItem
    
    static FieldConfig from_yaml(const YAML::Node& node);
};

// Batch handler configuration
struct BatchHandlerConfig {
    int batch_size = 2000;
    double batch_timeout = 30.0;
    int ipv4_prefix_length = 24;
    int ipv6_prefix_length = 64;
    
    static BatchHandlerConfig from_yaml(const YAML::Node& node);
};

// Collector configuration
struct CollectorConfig {
    std::string name;
    std::string protocol_base;
    std::vector<FieldConfig> required_log_information;
    BatchHandlerConfig batch_handler_config;
    
    static CollectorConfig from_yaml(const YAML::Node& node, const BatchHandlerConfig& default_config);
};

// Prefilter configuration
struct PrefilterConfig {
    std::string name;
    std::string relevance_method;
    std::string collector_name;
    
    static PrefilterConfig from_yaml(const YAML::Node& node);
};

// Inspector configuration
struct InspectorConfig {
    std::string name;
    std::string inspector_module_name;
    std::string inspector_class_name;
    std::string prefilter_name;
    std::string mode;  // univariate, multivariate, ensemble
    YAML::Node models;
    YAML::Node ensemble;
    double anomaly_threshold;
    double score_threshold;
    std::string time_type;
    int time_range;
    
    static InspectorConfig from_yaml(const YAML::Node& node);
};

// Detector configuration
struct DetectorConfig {
    std::string name;
    std::string detector_module_name;
    std::string detector_class_name;
    std::string model;
    std::string checksum;
    std::string base_url;
    double threshold;
    std::string inspector_name;
    
    static DetectorConfig from_yaml(const YAML::Node& node);
};

// Monitoring configuration
struct MonitoringConfig {
    int clickhouse_batch_size = 50;
    double clickhouse_batch_timeout = 2.0;
    
    static MonitoringConfig from_yaml(const YAML::Node& node);
};

// Pipeline configuration
struct PipelineConfig {
    std::string logserver_input_file;
    BatchHandlerConfig default_batch_handler_config;
    std::vector<CollectorConfig> collectors;
    std::vector<PrefilterConfig> prefilters;
    std::vector<InspectorConfig> inspectors;
    std::vector<DetectorConfig> detectors;
    MonitoringConfig monitoring;
    
    static PipelineConfig from_yaml(const YAML::Node& node);
};

// Root configuration
class Config {
public:
    LoggingConfig logging;
    PipelineConfig pipeline;
    EnvironmentConfig environment;
    
    // Load configuration from YAML file
    static std::shared_ptr<Config> load_from_file(const std::string& filepath);
    
    // Load configuration from YAML string
    static std::shared_ptr<Config> load_from_string(const std::string& yaml_content);
    
private:
    static std::shared_ptr<Config> from_yaml(const YAML::Node& root);
};

} // namespace config
} // namespace hamstring
