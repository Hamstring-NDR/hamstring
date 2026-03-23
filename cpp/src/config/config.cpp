#include "hamstring/config/config.hpp"
#include <fstream>
#include <sstream>

namespace hamstring {
namespace config {

// ============================================================================
// LoggingConfig
// ============================================================================

LoggingConfig LoggingConfig::from_yaml(const YAML::Node &node) {
  LoggingConfig config;

  if (node["base"]) {
    config.base_debug = node["base"]["debug"].as<bool>(false);
  }

  if (node["modules"]) {
    for (const auto &module : node["modules"]) {
      std::string module_name = module.first.as<std::string>();
      ModuleLoggingConfig module_config;
      module_config.debug = module.second["debug"].as<bool>(false);
      config.modules[module_name] = module_config;
    }
  }

  return config;
}

// ============================================================================
// KafkaBroker
// ============================================================================

KafkaBroker KafkaBroker::from_yaml(const YAML::Node &node) {
  KafkaBroker broker;
  broker.hostname = node["hostname"].as<std::string>();
  broker.internal_port = node["internal_port"].as<int>();
  broker.external_port = node["external_port"].as<int>();
  broker.node_ip = node["node_ip"].as<std::string>();
  return broker;
}

// ============================================================================
// EnvironmentConfig
// ============================================================================

EnvironmentConfig EnvironmentConfig::from_yaml(const YAML::Node &node) {
  EnvironmentConfig config;

  if (node["kafka_brokers"]) {
    for (const auto &broker_node : node["kafka_brokers"]) {
      config.kafka_brokers.push_back(KafkaBroker::from_yaml(broker_node));
    }
  }

  if (node["kafka_topics_prefix"]) {
    for (const auto &topic : node["kafka_topics_prefix"]["pipeline"]) {
      std::string topic_name = topic.first.as<std::string>();
      std::string topic_value = topic.second.as<std::string>();
      config.kafka_topics_prefix[topic_name] = topic_value;
    }
  }

  if (node["monitoring"] && node["monitoring"]["clickhouse_server"]) {
    config.clickhouse_hostname =
        node["monitoring"]["clickhouse_server"]["hostname"].as<std::string>();
  }

  return config;
}

std::string EnvironmentConfig::get_kafka_bootstrap_servers() const {
  std::stringstream ss;
  for (size_t i = 0; i < kafka_brokers.size(); ++i) {
    if (i > 0)
      ss << ",";
    ss << kafka_brokers[i].node_ip << ":" << kafka_brokers[i].external_port;
  }
  return ss.str();
}

// ============================================================================
// FieldConfig
// ============================================================================

FieldConfig FieldConfig::from_yaml(const YAML::Node &node) {
  FieldConfig config;

  config.name = node[0].as<std::string>();
  std::string type_str = node[1].as<std::string>();

  // Determine field type
  if (type_str == "RegEx") {
    config.type = FieldType::RegEx;
    config.pattern = node[2].as<std::string>();
  } else if (type_str == "Timestamp") {
    config.type = FieldType::Timestamp;
    config.timestamp_format = node[2].as<std::string>();
  } else if (type_str == "IpAddress") {
    config.type = FieldType::IpAddress;
  } else if (type_str == "ListItem") {
    config.type = FieldType::ListItem;
    for (const auto &item : node[2]) {
      config.allowed_list.push_back(item.as<std::string>());
    }
    if (node.size() > 3) {
      for (const auto &item : node[3]) {
        config.relevant_list.push_back(item.as<std::string>());
      }
    }
  }

  return config;
}

// ============================================================================
// BatchHandlerConfig
// ============================================================================

BatchHandlerConfig BatchHandlerConfig::from_yaml(const YAML::Node &node) {
  BatchHandlerConfig config;

  if (node["batch_size"]) {
    config.batch_size = node["batch_size"].as<int>();
  }
  if (node["batch_timeout"]) {
    config.batch_timeout = node["batch_timeout"].as<double>();
  }
  if (node["subnet_id"]) {
    if (node["subnet_id"]["ipv4_prefix_length"]) {
      config.ipv4_prefix_length =
          node["subnet_id"]["ipv4_prefix_length"].as<int>();
    }
    if (node["subnet_id"]["ipv6_prefix_length"]) {
      config.ipv6_prefix_length =
          node["subnet_id"]["ipv6_prefix_length"].as<int>();
    }
  }

  return config;
}

// ============================================================================
// CollectorConfig
// ============================================================================

CollectorConfig
CollectorConfig::from_yaml(const YAML::Node &node,
                           const BatchHandlerConfig &default_config) {
  CollectorConfig config;

  config.name = node["name"].as<std::string>();
  config.protocol_base = node["protocol_base"].as<std::string>();

  if (node["required_log_information"]) {
    for (const auto &field_node : node["required_log_information"]) {
      config.required_log_information.push_back(
          FieldConfig::from_yaml(field_node));
    }
  }

  // Start with default config
  config.batch_handler_config = default_config;

  // Override with collector-specific config if present
  if (node["batch_handler_config_override"]) {
    auto override_node = node["batch_handler_config_override"];
    if (override_node["batch_size"]) {
      config.batch_handler_config.batch_size =
          override_node["batch_size"].as<int>();
    }
    if (override_node["batch_timeout"]) {
      config.batch_handler_config.batch_timeout =
          override_node["batch_timeout"].as<double>();
    }
  }

  return config;
}

// ============================================================================
// PrefilterConfig
// ============================================================================

PrefilterConfig PrefilterConfig::from_yaml(const YAML::Node &node) {
  PrefilterConfig config;

  config.name = node["name"].as<std::string>();
  config.relevance_method = node["relevance_method"].as<std::string>();
  config.collector_name = node["collector_name"].as<std::string>();

  return config;
}

// ============================================================================
// InspectorConfig
// ============================================================================

InspectorConfig InspectorConfig::from_yaml(const YAML::Node &node) {
  InspectorConfig config;

  config.name = node["name"].as<std::string>();
  config.inspector_module_name =
      node["inspector_module_name"].as<std::string>();
  config.inspector_class_name = node["inspector_class_name"].as<std::string>();
  config.prefilter_name = node["prefilter_name"].as<std::string>();
  config.mode = node["mode"].as<std::string>();
  config.anomaly_threshold = node["anomaly_threshold"].as<double>();
  config.score_threshold = node["score_threshold"].as<double>();
  config.time_type = node["time_type"].as<std::string>();
  config.time_range = node["time_range"].as<int>();

  if (node["models"]) {
    config.models = YAML::Clone(node["models"]);
  }
  if (node["ensemble"]) {
    config.ensemble = YAML::Clone(node["ensemble"]);
  }

  return config;
}

// ============================================================================
// DetectorConfig
// ============================================================================

DetectorConfig DetectorConfig::from_yaml(const YAML::Node &node) {
  DetectorConfig config;

  config.name = node["name"].as<std::string>();
  config.detector_module_name = node["detector_module_name"].as<std::string>();
  config.detector_class_name = node["detector_class_name"].as<std::string>();
  config.model = node["model"].as<std::string>();
  config.checksum = node["checksum"].as<std::string>();
  config.base_url = node["base_url"].as<std::string>();
  config.threshold = node["threshold"].as<double>();
  config.inspector_name = node["inspector_name"].as<std::string>();

  return config;
}

// ============================================================================
// MonitoringConfig
// ============================================================================

MonitoringConfig MonitoringConfig::from_yaml(const YAML::Node &node) {
  MonitoringConfig config;

  if (node["clickhouse_connector"]) {
    auto ch_node = node["clickhouse_connector"];
    if (ch_node["batch_size"]) {
      config.clickhouse_batch_size = ch_node["batch_size"].as<int>();
    }
    if (ch_node["batch_timeout"]) {
      config.clickhouse_batch_timeout = ch_node["batch_timeout"].as<double>();
    }
  }

  return config;
}

// ============================================================================
// PipelineConfig
// ============================================================================

PipelineConfig PipelineConfig::from_yaml(const YAML::Node &node) {
  PipelineConfig config;

  // Log storage
  if (node["log_storage"] && node["log_storage"]["logserver"]) {
    config.logserver_input_file =
        node["log_storage"]["logserver"]["input_file"].as<std::string>("");
  }

  // Log collection
  if (node["log_collection"]) {
    auto collection_node = node["log_collection"];

    // Default batch handler config
    if (collection_node["default_batch_handler_config"]) {
      config.default_batch_handler_config = BatchHandlerConfig::from_yaml(
          collection_node["default_batch_handler_config"]);
    }

    // Collectors
    if (collection_node["collectors"]) {
      for (const auto &collector_node : collection_node["collectors"]) {
        config.collectors.push_back(CollectorConfig::from_yaml(
            collector_node, config.default_batch_handler_config));
      }
    }
  }

  // Prefilters
  if (node["log_filtering"]) {
    for (const auto &prefilter_node : node["log_filtering"]) {
      config.prefilters.push_back(PrefilterConfig::from_yaml(prefilter_node));
    }
  }

  // Inspectors
  if (node["data_inspection"]) {
    for (const auto &inspector_node : node["data_inspection"]) {
      config.inspectors.push_back(InspectorConfig::from_yaml(inspector_node));
    }
  }

  // Detectors
  if (node["data_analysis"]) {
    for (const auto &detector_node : node["data_analysis"]) {
      config.detectors.push_back(DetectorConfig::from_yaml(detector_node));
    }
  }

  // Monitoring
  if (node["monitoring"]) {
    config.monitoring = MonitoringConfig::from_yaml(node["monitoring"]);
  }

  return config;
}

// ============================================================================
// Config (Root)
// ============================================================================

std::shared_ptr<Config> Config::load_from_file(const std::string &filepath) {
  YAML::Node root = YAML::LoadFile(filepath);
  return from_yaml(root);
}

std::shared_ptr<Config>
Config::load_from_string(const std::string &yaml_content) {
  YAML::Node root = YAML::Load(yaml_content);
  return from_yaml(root);
}

std::shared_ptr<Config> Config::from_yaml(const YAML::Node &root) {
  auto config = std::make_shared<Config>();

  if (root["logging"]) {
    config->logging = LoggingConfig::from_yaml(root["logging"]);
  }

  if (root["pipeline"]) {
    config->pipeline = PipelineConfig::from_yaml(root["pipeline"]);
  }

  if (root["environment"]) {
    config->environment = EnvironmentConfig::from_yaml(root["environment"]);
  }

  return config;
}

} // namespace config
} // namespace hamstring
