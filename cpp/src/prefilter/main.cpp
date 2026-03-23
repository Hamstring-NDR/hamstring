#include "hamstring/base/logger.hpp"
#include "hamstring/config/config.hpp"
#include "hamstring/prefilter/prefilter.hpp"
#include <csignal>
#include <iostream>
#include <thread>

using namespace hamstring;

std::vector<std::shared_ptr<prefilter::Prefilter>> prefilters;
volatile sig_atomic_t shutdown_requested = 0;

void signal_handler(int signum) {
  std::cout << std::endl;
  auto logger = base::Logger::get_logger("main");
  logger->info("Received signal {}, shutting down...", signum);
  shutdown_requested = 1;

  // Stop all prefilters
  for (auto &pf : prefilters) {
    pf->stop();
  }
}

int main(int argc, char *argv[]) {
  // Set up signal handlers
  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);

  auto logger = base::Logger::get_logger("main");

  logger->info("╔════════════════════════════════════════╗");
  logger->info("║   HAMSTRING Prefilter (C++)            ║");
  logger->info("╚════════════════════════════════════════╝");
  logger->info("");

  // Load configuration
  std::string config_path = (argc > 1) ? argv[1] : "../config.yaml";
  logger->info("Loading configuration from: {}", config_path);

  std::shared_ptr<config::Config> config;
  try {
    config = config::Config::load_from_file(config_path);
    logger->info("Configuration loaded successfully");
    logger->info("  Prefilters: {}", config->pipeline.prefilters.size());
    logger->info("  Inspectors: {}", config->pipeline.inspectors.size());
    logger->info("  Kafka brokers: {}",
                 config->environment.kafka_brokers.size());
    logger->info("");
  } catch (const std::exception &e) {
    logger->error("Failed to load configuration: {}", e.what());
    return 1;
  }

  // Create prefilters
  try {
    prefilters = prefilter::create_prefilters(config);
    logger->info("Created {} Prefilter instances", prefilters.size());
    logger->info("");
  } catch (const std::exception &e) {
    logger->error("Failed to create prefilters: {}", e.what());
    return 1;
  }

  // Start all prefilters
  for (auto &pf : prefilters) {
    pf->start();
  }

  logger->info("All Prefilters started");
  logger->info("Press Ctrl+C to stop");
  logger->info("");

  // Report statistics periodically
  while (!shutdown_requested) {
    std::this_thread::sleep_for(std::chrono::seconds(10));

    if (!shutdown_requested) {
      logger->info("=== Prefilter Statistics ===");
      for (const auto &pf : prefilters) {
        auto stats = pf->get_stats();
        logger->info("Prefilter stats:");
        logger->info("  Batches consumed: {}", stats.batches_consumed);
        logger->info("  Batches sent: {}", stats.batches_sent);
        logger->info("  Loglines received: {}", stats.loglines_received);
        logger->info("  Loglines filtered: {}", stats.loglines_filtered);
        logger->info("  Loglines sent: {}", stats.loglines_sent);
      }
    }
  }

  logger->info("Shutdown complete");
  return 0;
}
