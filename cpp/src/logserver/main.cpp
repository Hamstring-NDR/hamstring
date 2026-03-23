#include "hamstring/base/logger.hpp"
#include "hamstring/config/config.hpp"
#include "hamstring/logserver/logserver.hpp"
#include <csignal>
#include <iostream>
#include <memory>
#include <vector>

using namespace hamstring;

// Global vector of servers for signal handling
std::vector<std::shared_ptr<logserver::LogServer>> g_servers;

void signal_handler(int signum) {
  auto logger = base::Logger::get_logger("main");
  logger->info("Received signal {}, shutting down...", signum);

  for (auto &server : g_servers) {
    server->stop();
  }
}

int main(int argc, char **argv) {
  // Parse arguments
  std::string config_path = (argc > 1) ? argv[1] : "../../config.yaml";

  // Initialize logging
  base::Logger::initialize(true); // debug mode
  auto logger = base::Logger::get_logger("main");

  logger->info("HAMSTRING LogServer");
  logger->info("==================");

  try {
    // Load configuration
    logger->info("Loading configuration from: {}", config_path);
    auto config = config::Config::load_from_file(config_path);

    logger->info("Configuration loaded successfully");
    logger->info("Kafka brokers: {}", config->environment.kafka_brokers.size());
    logger->info("Collectors: {}", config->pipeline.collectors.size());

    // Create LogServer instances
    g_servers = logserver::create_logservers(config);

    logger->info("Created {} LogServer instances", g_servers.size());

    // Set up signal handlers for graceful shutdown
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // Start all servers
    for (auto &server : g_servers) {
      server->start();
    }

    logger->info("All LogServers started");
    logger->info("Press Ctrl+C to stop");

    // Wait for servers to finish (they run in background threads)
    while (true) {
      bool any_running = false;
      for (const auto &server : g_servers) {
        if (server->is_running()) {
          any_running = true;
          break;
        }
      }

      if (!any_running) {
        break;
      }

      std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    logger->info("All LogServers stopped");

  } catch (const std::exception &e) {
    logger->error("Fatal error: {}", e.what());
    return 1;
  }

  return 0;
}
