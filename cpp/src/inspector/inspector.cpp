#include "hamstring/inspector/inspector.hpp"
#include "hamstring/base/utils.hpp"
#include <map>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace hamstring {
namespace inspector {

Inspector::Inspector(const std::string &name, const std::string &consume_topic,
                     const std::vector<std::string> &produce_topics,
                     const std::string &mode, double anomaly_threshold,
                     double score_threshold,
                     std::shared_ptr<config::Config> config,
                     const std::string &bootstrap_servers,
                     const std::string &group_id)
    : name_(name), consume_topic_(consume_topic),
      produce_topics_(produce_topics), mode_(mode),
      anomaly_threshold_(anomaly_threshold), score_threshold_(score_threshold),
      config_(config) {
  logger_ = base::Logger::get_logger("inspector." + name_);

  // Create Kafka consumer
  consumer_ = std::make_unique<base::KafkaConsumeHandler>(
      bootstrap_servers, group_id, std::vector<std::string>{consume_topic_});

  // Create Kafka producers for each output topic
  for (const auto &topic : produce_topics_) {
    auto producer =
        std::make_unique<base::KafkaProduceHandler>(bootstrap_servers, topic);
    producers_.push_back(std::move(producer));
  }

  // Create ClickHouse sender for monitoring
  clickhouse_ = std::make_shared<base::ClickHouseSender>(
      config_->environment.clickhouse_hostname, 9000, "hamstring", "default",
      "");

  // Create anomaly detector with inspector config
  config::InspectorConfig inspector_config;
  inspector_config.anomaly_threshold = anomaly_threshold_;
  inspector_config.score_threshold = score_threshold_;
  inspector_config.time_range = 100; // Default window size
  anomaly_detector_ = std::make_unique<AnomalyDetector>(inspector_config);

  logger_->info("Inspector '{}' created", name_);
  logger_->info("  Mode: {}", mode_);
  logger_->info("  Anomaly threshold: {}", anomaly_threshold_);
  logger_->info("  Score threshold: {}", score_threshold_);
  logger_->info("  Consume topic: {}", consume_topic_);
  logger_->info("  Produce topics: {}", produce_topics_.size());
}

Inspector::~Inspector() { stop(); }

void Inspector::start() {
  if (running_) {
    logger_->warn("Inspector already running");
    return;
  }

  running_ = true;

  logger_->info("Inspector '{}' started", name_);
  logger_->info("  ⤷ receiving on Kafka topic '{}'", consume_topic_);
  logger_->info("  ⤷ sending to {} topics", produce_topics_.size());
  for (const auto &topic : produce_topics_) {
    logger_->info("      - {}", topic);
  }

  // Start worker thread
  worker_thread_ = std::thread(&Inspector::consume_loop, this);
}

void Inspector::stop() {
  if (!running_) {
    return;
  }

  logger_->info("Stopping Inspector '{}'...", name_);
  running_ = false;

  if (worker_thread_.joinable()) {
    worker_thread_.join();
  }

  // Flush all producers
  for (auto &producer : producers_) {
    producer->flush();
  }

  logger_->info("Inspector '{}' stopped", name_);
  logger_->info("  Batches consumed: {}", batches_consumed_.load());
  logger_->info("  Batches suspicious: {}", batches_suspicious_.load());
  logger_->info("  Batches filtered: {}", batches_filtered_.load());
  logger_->info("  Suspicious batches sent: {}",
                suspicious_batches_sent_.load());
}

void Inspector::consume_loop() {
  logger_->info("Consumer loop started");

  while (running_) {
    try {
      // Poll for messages with 1 second timeout
      consumer_->poll(
          [this](const std::string &topic, const std::string &key,
                 const std::string &value, int64_t timestamp) {
            if (!running_)
              return;

            logger_->trace("Consumed batch from topic '{}'", topic);

            try {
              // Parse batch JSON
              auto batch_ptr = base::Batch::from_json(value);

              logger_->debug("Received batch {} with {} loglines",
                             batch_ptr->batch_id, batch_ptr->loglines.size());

              // Process the batch
              process_batch(*batch_ptr);

              // Commit the offset
              consumer_->commit();

            } catch (const std::exception &e) {
              logger_->error("Failed to process batch: {}", e.what());
            }
          },
          1000); // 1 second timeout

    } catch (const std::exception &e) {
      logger_->error("Error in consumer loop: {}", e.what());
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }

  logger_->info("Consumer loop stopped");
}

void Inspector::process_batch(const base::Batch &batch) {
  batches_consumed_++;

  // Check if batch is suspicious
  if (is_suspicious(batch)) {
    batches_suspicious_++;

    // Group log lines by source IP
    std::map<std::string, std::vector<std::shared_ptr<base::LogLine>>>
        batches_by_ip;

    for (const auto &logline_ptr : batch.loglines) {
      auto it = logline_ptr->fields.find("src_ip");
      if (it != logline_ptr->fields.end()) {
        std::string src_ip = it->second;
        batches_by_ip[src_ip].push_back(logline_ptr);
      } else {
        // If no src_ip, use "unknown"
        batches_by_ip["unknown"].push_back(logline_ptr);
      }
    }

    logger_->info("Batch {} is suspicious - grouped into {} IP buckets",
                  batch.batch_id, batches_by_ip.size());

    // Send suspicious batches
    send_suspicious_batches(batches_by_ip, batch.batch_id);

  } else {
    batches_filtered_++;
    logger_->debug("Batch {} filtered out (not suspicious)", batch.batch_id);
  }
}

bool Inspector::is_suspicious(const base::Batch &batch) {
  if (batch.loglines.empty()) {
    return false;
  }

  // Use statistical anomaly detection
  double suspicion_score = anomaly_detector_->analyze_batch(batch);

  // Update detector state for future analysis
  anomaly_detector_->update_state(batch);

  // Log detailed statistics periodically
  if (batches_consumed_ % 100 == 0) {
    auto stats = anomaly_detector_->get_statistics();
    logger_->debug(
        "Anomaly detector stats: NXDOMAIN mean={:.3f} stddev={:.3f}, "
        "domain_length mean={:.1f} stddev={:.1f}, samples={}",
        stats.mean_nxdomain_rate, stats.stddev_nxdomain_rate,
        stats.mean_domain_length, stats.stddev_domain_length,
        stats.samples_count);
  }

  bool is_anomalous = suspicion_score > anomaly_threshold_;

  if (is_anomalous) {
    logger_->info("Batch {} is SUSPICIOUS (score: {:.3f})", batch.batch_id,
                  suspicion_score);
  } else {
    logger_->debug("Batch {} is normal (score: {:.3f})", batch.batch_id,
                   suspicion_score);
  }

  return is_anomalous;
}

void Inspector::send_suspicious_batches(
    const std::map<std::string, std::vector<std::shared_ptr<base::LogLine>>>
        &batches_by_ip,
    const std::string &original_batch_id) {
  for (const auto &[src_ip, loglines] : batches_by_ip) {
    // Create a new batch for this IP
    base::Batch suspicious_batch;
    suspicious_batch.batch_id = base::utils::generate_uuid();
    suspicious_batch.subnet_id = src_ip;
    suspicious_batch.loglines = loglines;
    suspicious_batch.created_at = std::chrono::system_clock::now();
    suspicious_batch.timestamp_in = std::chrono::system_clock::now();

    std::string batch_json = suspicious_batch.to_json();

    logger_->info("Sending suspicious batch {} for IP {} ({} loglines)",
                  suspicious_batch.batch_id, src_ip, loglines.size());

    // Send to all output topics
    for (size_t i = 0; i < producers_.size(); ++i) {
      try {
        producers_[i]->send(suspicious_batch.batch_id, batch_json);
        logger_->trace("Sent suspicious batch {} to topic {}",
                       suspicious_batch.batch_id, produce_topics_[i]);
      } catch (const std::exception &e) {
        logger_->error("Failed to send suspicious batch to {}: {}",
                       produce_topics_[i], e.what());
      }
    }

    suspicious_batches_sent_++;
  }
}

Inspector::Stats Inspector::get_stats() const {
  Stats stats;
  stats.batches_consumed = batches_consumed_.load();
  stats.batches_suspicious = batches_suspicious_.load();
  stats.batches_filtered = batches_filtered_.load();
  stats.suspicious_batches_sent = suspicious_batches_sent_.load();
  return stats;
}

std::vector<std::shared_ptr<Inspector>>
create_inspectors(std::shared_ptr<config::Config> config) {
  std::vector<std::shared_ptr<Inspector>> inspectors;
  auto logger = base::Logger::get_logger("inspector.factory");

  // Get topic prefixes
  auto &env = config->environment;
  std::string consume_prefix =
      env.kafka_topics_prefix["prefilter_to_inspector"];
  std::string produce_prefix = env.kafka_topics_prefix["inspector_to_detector"];

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

  logger->info("Creating Inspectors for {} inspectors",
               config->pipeline.inspectors.size());
  logger->info("Kafka brokers: {}", bootstrap_servers);

  // Create one Inspector per configured inspector
  for (const auto &inspector_config : config->pipeline.inspectors) {
    std::string consume_topic = consume_prefix + "-" + inspector_config.name;
    std::string group_id = "inspector-" + inspector_config.name;

    // Find all detectors for this inspector
    std::vector<std::string> produce_topics;
    for (const auto &detector : config->pipeline.detectors) {
      if (detector.inspector_name == inspector_config.name) {
        std::string topic = produce_prefix + "-" + detector.name;
        produce_topics.push_back(topic);
      }
    }

    logger->info("Creating Inspector '{}'", inspector_config.name);
    logger->info("  Consume from: {}", consume_topic);
    logger->info("  Produce to {} topics", produce_topics.size());
    logger->info("  Mode: {}", inspector_config.mode);
    logger->info("  Anomaly threshold: {}", inspector_config.anomaly_threshold);
    logger->info("  Score threshold: {}", inspector_config.score_threshold);

    auto inspector = std::make_shared<Inspector>(
        inspector_config.name, consume_topic, produce_topics,
        inspector_config.mode, inspector_config.anomaly_threshold,
        inspector_config.score_threshold, config, bootstrap_servers, group_id);

    inspectors.push_back(inspector);
  }

  return inspectors;
}

} // namespace inspector
} // namespace hamstring
