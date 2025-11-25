#include "hamstring/base/logger.hpp"
#include "hamstring/config/config.hpp"
#include "hamstring/inspector/inspector.hpp"
#include <csignal>
#include <iostream>
#include <thread>

using namespace hamstring;

std::vector<std::shared_ptr<inspector::Inspector>> inspectors;
volatile sig_atomic_t shutdown_requested = 0;

void signal_handler(int signum) {
  std::cout << std::endl;
  auto logger = base::Logger::get_logger("main");
  logger->info("Received signal {}, shutting down...", signum);
  shutdown_requested = 1;

  // Stop all inspectors
  for (auto &insp : inspectors) {
    insp->stop();
  }
}

int main(int argc, char *argv[]) {
  // Set up signal handlers
  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);

  auto logger = base::Logger::get_logger("main");

  logger->info("╔════════════════════════════════════════╗");
  logger->info("║   HAMSTRING Inspector (C++)            ║");
  logger->info("╚════════════════════════════════════════╝");
  logger->info("");

  // Load configuration
  std::string config_path = (argc > 1) ? argv[1] : "../config.yaml";
  logger->info("Loading configuration from: {}", config_path);

  std::shared_ptr<config::Config> config;
  try {
    config = config::Config::load_from_file(config_path);
    logger->info("Configuration loaded successfully");
    logger->info("  Inspectors: {}", config->pipeline.inspectors.size());
    logger->info("  Detectors: {}", config->pipeline.detectors.size());
    logger->info("  Kafka brokers: {}",
                 config->environment.kafka_brokers.size());
    logger->info("");
  } catch (const std::exception &e) {
    logger->error("Failed to load configuration: {}", e.what());
    return 1;
  }

  // Create inspectors
  try {
    inspectors = inspector::create_inspectors(config);
    logger->info("Created {} Inspector instances", inspectors.size());
    logger->info("");
  } catch (const std::exception &e) {
    logger->error("Failed to create inspectors: {}", e.what());
    return 1;
  }

  // Start all inspectors
  for (auto &insp : inspectors) {
    insp->start();
  }

  logger->info("All Inspectors started");
  logger->info("Press Ctrl+C to stop");
  logger->info("");

  // Report statistics periodically
  while (!shutdown_requested) {
    std::this_thread::sleep_for(std::chrono::seconds(10));

    if (!shutdown_requested) {
      logger->info("=== Inspector Statistics ===");
      for (const auto &insp : inspectors) {
        auto stats = insp->get_stats();
        logger->info("Inspector stats:");
        logger->info("  Batches consumed: {}", stats.batches_consumed);
        logger->info("  Batches suspicious: {}", stats.batches_suspicious);
        logger->info("  Batches filtered: {}", stats.batches_filtered);
        logger->info("  Suspicious batches sent: {}",
                     stats.suspicious_batches_sent);
      }
    }
  }

  logger->info("Shutdown complete");
  return 0;
}
