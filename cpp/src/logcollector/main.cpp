#include "hamstring/base/logger.hpp"
#include "hamstring/config/config.hpp"
#include "hamstring/logcollector/logcollector.hpp"
#include <csignal>
#include <iostream>
#include <memory>
#include <vector>

using namespace hamstring;

// Global vector of collectors for signal handling
std::vector<std::shared_ptr<logcollector::LogCollector>> g_collectors;

void signal_handler(int signum) {
  auto logger = base::Logger::get_logger("main");
  logger->info("Received signal {}, shutting down...", signum);

  for (auto &collector : g_collectors) {
    collector->stop();
  }
}

void print_stats() {
  auto logger = base::Logger::get_logger("main");

  logger->info("=== LogCollector Statistics ===");
  for (const auto &collector : g_collectors) {
    auto stats = collector->get_stats();
    logger->info("Collector stats:");
    logger->info("  Messages consumed: {}", stats.messages_consumed);
    logger->info("  Messages validated: {}", stats.messages_validated);
    logger->info("  Messages failed: {}", stats.messages_failed);
    logger->info("  Batches sent: {}", stats.batches_sent);
  }
}

int main(int argc, char **argv) {
  // Parse arguments
  std::string config_path = (argc > 1) ? argv[1] : "../../config.yaml";

  // Initialize logging
  base::Logger::initialize(true); // debug mode
  auto logger = base::Logger::get_logger("main");

  logger->info("╔════════════════════════════════════════╗");
  logger->info("║   HAMSTRING LogCollector (C++)         ║");
  logger->info("╚════════════════════════════════════════╝");
  logger->info("");

  try {
    // Load configuration
    logger->info("Loading configuration from: {}", config_path);
    auto config = config::Config::load_from_file(config_path);

    logger->info("Configuration loaded successfully");
    logger->info("  Collectors: {}", config->pipeline.collectors.size());
    logger->info("  Prefilters: {}", config->pipeline.prefilters.size());
    logger->info("  Kafka brokers: {}",
                 config->environment.kafka_brokers.size());
    logger->info("");

    // Create LogCollector instances
    g_collectors = logcollector::create_logcollectors(config);

    logger->info("Created {} LogCollector instances", g_collectors.size());
    logger->info("");

    // Set up signal handlers for graceful shutdown
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // Start all collectors
    for (auto &collector : g_collectors) {
      collector->start();
    }

    logger->info("All LogCollectors started");
    logger->info("Press Ctrl+C to stop");
    logger->info("");

    // Wait for collectors to finish (they run in background threads)
    while (true) {
      bool any_running = false;
      for (const auto &collector : g_collectors) {
        if (collector->is_running()) {
          any_running = true;
          break;
        }
      }

      if (!any_running) {
        break;
      }

      // Print stats every 10 seconds
      std::this_thread::sleep_for(std::chrono::seconds(10));
      print_stats();
    }

    logger->info("");
    logger->info("All LogCollectors stopped");
    print_stats();

  } catch (const std::exception &e) {
    logger->error("Fatal error: {}", e.what());
    return 1;
  }

  return 0;
}
