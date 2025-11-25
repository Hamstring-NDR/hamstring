#include "hamstring/base/logger.hpp"
#include "hamstring/config/config.hpp"
#include "hamstring/detector/detector_service.hpp"
#include <csignal>
#include <iostream>
#include <thread>
#include <vector>

using namespace hamstring;

std::vector<std::shared_ptr<detector::DetectorService>> services;
volatile sig_atomic_t shutdown_requested = 0;

void signal_handler(int signum) {
  std::cout << std::endl;
  auto logger = base::Logger::get_logger("main");
  logger->info("Received signal {}, shutting down...", signum);
  shutdown_requested = 1;

  for (auto &svc : services) {
    svc->stop();
  }
}

int main(int argc, char *argv[]) {
  // Set up signal handlers
  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);

  auto logger = base::Logger::get_logger("main");

  logger->info("╔════════════════════════════════════════╗");
  logger->info("║   HAMSTRING Detector (C++)             ║");
  logger->info("╚════════════════════════════════════════╝");
  logger->info("");

  // Load configuration
  std::string config_path = (argc > 1) ? argv[1] : "../../config.yaml";
  logger->info("Loading configuration from: {}", config_path);

  try {
    auto config = config::Config::load_from_file(config_path);
    logger->info("Configuration loaded successfully");

    // Create services
    services = detector::create_detector_services(config);

    if (services.empty()) {
      logger->warn("No detectors configured");
      return 0;
    }

    // Start services
    for (auto &svc : services) {
      svc->start();
    }

    logger->info("All DetectorServices started");
    logger->info("Press Ctrl+C to stop");

    // Main loop
    while (!shutdown_requested) {
      std::this_thread::sleep_for(std::chrono::seconds(10));

      if (!shutdown_requested) {
        logger->info("=== Detector Statistics === ");
        for (const auto &svc : services) {
          auto stats = svc->get_stats();
          logger->info("Detector stats:");
          logger->info("  Batches consumed: {}", stats.batches_consumed);
          logger->info("  Domains scanned: {}", stats.domains_scanned);
          logger->info("  Domains detected: {}", stats.domains_detected);
        }
      }
    }

    logger->info("Shutdown complete");

  } catch (const std::exception &e) {
    logger->error("Fatal error: {}", e.what());
    return 1;
  }

  return 0;
}
