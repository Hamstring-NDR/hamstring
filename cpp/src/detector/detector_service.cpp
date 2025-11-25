#include "hamstring/detector/detector_service.hpp"
#include "hamstring/base/utils.hpp"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace hamstring {
namespace detector {

DetectorService::DetectorService(const std::string &name,
                                 const std::string &consume_topic,
                                 const std::string &model_path,
                                 double threshold,
                                 std::shared_ptr<config::Config> config,
                                 const std::string &bootstrap_servers,
                                 const std::string &group_id)
    : name_(name), consume_topic_(consume_topic), model_path_(model_path),
      threshold_(threshold), config_(config) {
  logger_ = base::Logger::get_logger("detector." + name_);

  // Initialize detector
  try {
    detector_.load_model(model_path_);
  } catch (const std::exception &e) {
    logger_->error("Failed to load model from {}: {}", model_path_, e.what());
    // We might want to throw here or continue with a broken detector
    throw;
  }

  // Create Kafka consumer
  consumer_ = std::make_unique<base::KafkaConsumeHandler>(
      bootstrap_servers, group_id, std::vector<std::string>{consume_topic_});

  // Create ClickHouse sender
  clickhouse_ = std::make_shared<base::ClickHouseSender>(
      config_->environment.clickhouse_hostname, 9000, "hamstring", "default",
      "");

  logger_->info("DetectorService '{}' created", name_);
  logger_->info("  Model: {}", model_path_);
  logger_->info("  Threshold: {}", threshold_);
  logger_->info("  Consume topic: {}", consume_topic_);
}

DetectorService::~DetectorService() { stop(); }

void DetectorService::start() {
  if (running_) {
    logger_->warn("DetectorService already running");
    return;
  }

  running_ = true;

  logger_->info("DetectorService '{}' started", name_);
  logger_->info("  ⤷ receiving on Kafka topic '{}'", consume_topic_);

  // Start worker thread
  worker_thread_ = std::thread(&DetectorService::consume_loop, this);
}

void DetectorService::stop() {
  if (!running_) {
    return;
  }

  logger_->info("Stopping DetectorService '{}'...", name_);
  running_ = false;

  if (worker_thread_.joinable()) {
    worker_thread_.join();
  }

  logger_->info("DetectorService '{}' stopped", name_);
  logger_->info("  Batches consumed: {}", batches_consumed_.load());
  logger_->info("  Domains scanned: {}", domains_scanned_.load());
  logger_->info("  Domains detected: {}", domains_detected_.load());
}

void DetectorService::consume_loop() {
  logger_->info("Consumer loop started");

  while (running_) {
    try {
      consumer_->poll(
          [this](const std::string &topic, const std::string &key,
                 const std::string &value, int64_t timestamp) {
            if (!running_)
              return;

            try {
              auto batch_ptr = base::Batch::from_json(value);
              process_batch(*batch_ptr);
              consumer_->commit();
            } catch (const std::exception &e) {
              logger_->error("Failed to process batch: {}", e.what());
            }
          },
          1000);

    } catch (const std::exception &e) {
      logger_->error("Error in consumer loop: {}", e.what());
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }

  logger_->info("Consumer loop stopped");
}

void DetectorService::process_batch(const base::Batch &batch) {
  batches_consumed_++;

  for (const auto &logline : batch.loglines) {
    auto it = logline->fields.find(
        "query"); // Assuming 'query' field holds the domain
    if (it != logline->fields.end()) {
      std::string domain = it->second;

      // Remove trailing dot if present
      if (!domain.empty() && domain.back() == '.') {
        domain.pop_back();
      }

      domains_scanned_++;

      try {
        float score = detector_.predict(domain);

        if (score > threshold_) {
          domains_detected_++;
          logger_->info("DGA DETECTED: {} (score: {:.4f})", domain, score);

          // TODO: Send to output topic or ClickHouse
          // For now just logging is fine as per requirements "Integrate ... in
          // C++" We could add a "detected_dga" table in ClickHouse
        }
      } catch (const std::exception &e) {
        logger_->warn("Prediction failed for {}: {}", domain, e.what());
      }
    }
  }
}

DetectorService::Stats DetectorService::get_stats() const {
  Stats stats;
  stats.batches_consumed = batches_consumed_.load();
  stats.domains_scanned = domains_scanned_.load();
  stats.domains_detected = domains_detected_.load();
  return stats;
}

std::vector<std::shared_ptr<DetectorService>>
create_detector_services(std::shared_ptr<config::Config> config) {
  std::vector<std::shared_ptr<DetectorService>> services;
  auto logger = base::Logger::get_logger("detector.factory");

  auto &env = config->environment;
  std::string consume_prefix = env.kafka_topics_prefix["inspector_to_detector"];

  // Build bootstrap servers string
  std::vector<std::string> broker_addresses;
  for (const auto &broker : env.kafka_brokers) {
    broker_addresses.push_back(broker.hostname + ":" +
                               std::to_string(broker.internal_port));
  }
  std::string bootstrap_servers;
  for (size_t i = 0; i < broker_addresses.size(); ++i) {
    if (i > 0)
      bootstrap_servers += ",";
    bootstrap_servers += broker_addresses[i];
  }

  logger->info("Creating DetectorServices for {} detectors",
               config->pipeline.detectors.size());

  for (const auto &detector_config : config->pipeline.detectors) {
    // Topic convention: inspector_to_detector-{detector_name}
    // But wait, Inspector produces to `inspector_to_detector-{detector_name}`.
    // So we consume from that.
    std::string consume_topic = consume_prefix + "-" + detector_config.name;
    std::string group_id = "detector-" + detector_config.name;

    logger->info("Creating DetectorService '{}'", detector_config.name);
    logger->info("  Consume from: {}", consume_topic);
    logger->info("  Model: {}", detector_config.model);

    auto service = std::make_shared<DetectorService>(
        detector_config.name, consume_topic, detector_config.model,
        detector_config.threshold, config, bootstrap_servers, group_id);

    services.push_back(service);
  }

  return services;
}

} // namespace detector
} // namespace hamstring
