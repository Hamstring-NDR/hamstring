#include "hamstring/base/logger.hpp"
#include "hamstring/base/utils.hpp"
#include "hamstring/config/config.hpp"
#include "hamstring/detector/feature_extractor.hpp"
#include <iostream>

using namespace hamstring;

int main(int argc, char **argv) {
  // Initialize logger
  base::Logger::initialize(true); // debug mode
  auto logger = base::Logger::get_logger("example");

  logger->info("HAMSTRING C++ Example");
  logger->info("=====================");

  // Load configuration
  std::string config_path = (argc > 1) ? argv[1] : "../../config.yaml";
  logger->info("Loading configuration from: {}", config_path);

  try {
    auto config = config::Config::load_from_file(config_path);

    logger->info("Configuration loaded successfully");
    logger->info("Number of collectors: {}",
                 config->pipeline.collectors.size());
    logger->info("Number of detectors: {}", config->pipeline.detectors.size());
    logger->info("Kafka brokers: {}", config->environment.kafka_brokers.size());

    // Show Kafka bootstrap servers
    std::string bootstrap = config->environment.get_kafka_bootstrap_servers();
    logger->info("Kafka bootstrap servers: {}", bootstrap);

  } catch (const std::exception &e) {
    logger->error("Failed to load configuration: {}", e.what());
    logger->warn("Continuing with feature extraction demo...");
  }

  // Demonstrate feature extraction
  logger->info("");
  logger->info("Feature Extraction Demo");
  logger->info("=======================");

  detector::FeatureExtractor extractor;

  // Test domains
  std::vector<std::string> test_domains = {"google.com", "www.example.com",
                                           "xjk3n2m9pq.com", // DGA-like
                                           "mail.google.com"};

  for (const auto &domain : test_domains) {
    logger->info("");
    logger->info("Domain: {}", domain);

    auto features = extractor.extract(domain);

    logger->info("  Label length: {}", features.label_length);
    logger->info("  Label max: {}", features.label_max);
    logger->info("  FQDN entropy: {:.4f}", features.fqdn_entropy);
    logger->info("  SLD entropy: {:.4f}", features.secondleveldomain_entropy);
    logger->info("  Alpha ratio: {:.4f}", features.fqdn_alpha_count);
    logger->info("  Numeric ratio: {:.4f}", features.fqdn_numeric_count);

    auto vec = features.to_vector();
    logger->info("  Feature vector size: {}", vec.size());

    // Check if DGA-like (high entropy)
    if (features.fqdn_entropy > 3.0) {
      logger->warn("  ⚠ High entropy - possible DGA domain!");
    }
  }

  // Demonstrate utilities
  logger->info("");
  logger->info("Utilities Demo");
  logger->info("==============");

  std::string uuid = base::utils::generate_uuid();
  logger->info("Generated UUID: {}", uuid);

  std::string ip = "192.168.1.100";
  std::string subnet = base::utils::get_subnet_id(ip, 24);
  logger->info("IP: {} -> Subnet: {}", ip, subnet);

  std::string domain = "www.example.com";
  std::string sld = base::utils::extract_second_level_domain(domain);
  std::string tld_part = base::utils::extract_third_level_domain(domain);
  logger->info("Domain: {}", domain);
  logger->info("  Second level: {}", sld);
  logger->info("  Third level: {}", tld_part);

  logger->info("");
  logger->info("Example completed successfully!");

  return 0;
}
