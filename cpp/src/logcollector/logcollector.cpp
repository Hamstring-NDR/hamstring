#include "hamstring/logcollector/logcollector.hpp"
#include "hamstring/base/utils.hpp"
#include <algorithm>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace hamstring {
namespace logcollector {

// ============================================================================
// Buffered Batch Implementation
// ============================================================================

BufferedBatch::BufferedBatch(const std::string &collector_name,
                             size_t batch_size, int batch_timeout_ms)
    : collector_name_(collector_name), batch_size_(batch_size),
      batch_timeout_(batch_timeout_ms) {
  logger_ = base::Logger::get_logger("logcollector.batch");
  logger_->info(
      "BufferedBatch created for collector '{}' (size={}, timeout={}ms)",
      collector_name_, batch_size_, batch_timeout_ms);
}

BufferedBatch::~BufferedBatch() {
  logger_->info(
      "BufferedBatch destroyed. Total processed: {} loglines, {} batches",
      total_loglines_processed_.load(), total_batches_sent_.load());
}

bool BufferedBatch::add_logline(const std::string &subnet_id,
                                const base::LogLine &logline) {
  std::lock_guard<std::mutex> lock(batches_mutex_);

  auto &batch_data = batches_[subnet_id];

  // Initialize new batch if needed
  if (batch_data.loglines.empty()) {
    batch_data.batch_id = base::utils::generate_uuid();
    batch_data.subnet_id = subnet_id;
    batch_data.created_at = std::chrono::system_clock::now();
    logger_->debug("Created new batch {} for subnet {}", batch_data.batch_id,
                   subnet_id);
  }

  batch_data.loglines.push_back(logline);
  batch_data.last_updated = std::chrono::system_clock::now();
  total_loglines_processed_++;

  logger_->trace("Added logline to batch {} (size: {})", batch_data.batch_id,
                 batch_data.loglines.size());

  // Check if batch is ready
  return batch_data.loglines.size() >= batch_size_;
}

base::Batch BufferedBatch::get_batch(const std::string &subnet_id) {
  std::lock_guard<std::mutex> lock(batches_mutex_);

  auto it = batches_.find(subnet_id);
  if (it == batches_.end() || it->second.loglines.empty()) {
    throw std::runtime_error("No batch available for subnet: " + subnet_id);
  }

  auto &batch_data = it->second;

  // Create Batch object
  base::Batch batch;
  batch.batch_id = batch_data.batch_id;
  batch.subnet_id = batch_data.subnet_id;

  // Convert LogLine vector to shared_ptr vector
  for (auto &logline : batch_data.loglines) {
    batch.loglines.push_back(std::make_shared<base::LogLine>(logline));
  }

  batch.created_at = batch_data.created_at;
  batch.timestamp_in = std::chrono::system_clock::now();

  // Remove the batch
  batches_.erase(it);
  total_batches_sent_++;

  logger_->info("Completed batch {} with {} loglines", batch.batch_id,
                batch.loglines.size());

  return batch;
}

std::vector<base::Batch> BufferedBatch::get_ready_batches() {
  std::vector<base::Batch> ready_batches;
  std::vector<std::string> ready_subnets;

  {
    std::lock_guard<std::mutex> lock(batches_mutex_);

    auto now = std::chrono::system_clock::now();

    for (const auto &[subnet_id, batch_data] : batches_) {
      bool size_trigger = batch_data.loglines.size() >= batch_size_;
      auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
          now - batch_data.created_at);
      bool timeout_trigger = age >= batch_timeout_;

      if (size_trigger || timeout_trigger) {
        ready_subnets.push_back(subnet_id);

        if (size_trigger) {
          logger_->debug("Batch {} ready (size trigger): {} loglines",
                         batch_data.batch_id, batch_data.loglines.size());
        } else {
          logger_->debug("Batch {} ready (timeout trigger): age={}ms",
                         batch_data.batch_id, age.count());
        }
      }
    }
  }

  // Get batches outside the lock to avoid holding it too long
  for (const auto &subnet_id : ready_subnets) {
    try {
      ready_batches.push_back(get_batch(subnet_id));
    } catch (const std::exception &e) {
      logger_->warn("Failed to get batch for subnet {}: {}", subnet_id,
                    e.what());
    }
  }

  return ready_batches;
}

std::vector<base::Batch> BufferedBatch::flush_all() {
  std::vector<base::Batch> all_batches;
  std::vector<std::string> all_subnets;

  {
    std::lock_guard<std::mutex> lock(batches_mutex_);
    for (const auto &[subnet_id, _] : batches_) {
      all_subnets.push_back(subnet_id);
    }
  }

  logger_->info("Flushing {} batches", all_subnets.size());

  for (const auto &subnet_id : all_subnets) {
    try {
      all_batches.push_back(get_batch(subnet_id));
    } catch (const std::exception &e) {
      logger_->warn("Failed to flush batch for subnet {}: {}", subnet_id,
                    e.what());
    }
  }

  return all_batches;
}

BufferedBatch::Stats BufferedBatch::get_stats() const {
  std::lock_guard<std::mutex> lock(batches_mutex_);

  Stats stats;
  stats.total_batches = batches_.size();
  stats.total_loglines = 0;
  stats.largest_batch = 0;
  stats.oldest_batch_age = std::chrono::milliseconds(0);

  auto now = std::chrono::system_clock::now();

  for (const auto &[_, batch_data] : batches_) {
    stats.total_loglines += batch_data.loglines.size();
    stats.largest_batch =
        std::max(stats.largest_batch, batch_data.loglines.size());

    auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - batch_data.created_at);
    stats.oldest_batch_age = std::max(stats.oldest_batch_age, age);
  }

  return stats;
}

// ============================================================================
// LogCollector Implementation
// ============================================================================

LogCollector::LogCollector(
    const std::string &name, const std::string &protocol,
    const std::string &consume_topic,
    const std::vector<std::string> &produce_topics,
    const std::vector<config::FieldConfig> &validation_config,
    std::shared_ptr<config::Config> config,
    const std::string &bootstrap_servers, const std::string &group_id)
    : name_(name), protocol_(protocol), consume_topic_(consume_topic),
      produce_topics_(produce_topics), validation_config_(validation_config),
      config_(config) {
  logger_ = base::Logger::get_logger("logcollector." + name_);

  // Get batch configuration
  // TODO: Extract from config based on collector name
  batch_size_ = 100;        // Default
  batch_timeout_ms_ = 5000; // 5 seconds default
  ipv4_prefix_length_ = 24;
  ipv6_prefix_length_ = 64;

  // Create batch handler
  batch_handler_ =
      std::make_unique<BufferedBatch>(name_, batch_size_, batch_timeout_ms_);

  // Create Kafka consumer
  consumer_ = std::make_unique<base::KafkaConsumeHandler>(
      bootstrap_servers, group_id, std::vector<std::string>{consume_topic_});

  // Create Kafka producer (using first produce topic for now)
  // TODO: Support multiple produce topics
  if (!produce_topics_.empty()) {
    producer_ = std::make_unique<base::KafkaProduceHandler>(bootstrap_servers,
                                                            produce_topics_[0]);
  }

  // Create ClickHouse sender for monitoring
  clickhouse_ = std::make_shared<base::ClickHouseSender>(
      config_->environment.clickhouse_hostname,
      9000,        // Default port
      "hamstring", // Database name
      "default",   // Username
      "");         // Password

  logger_->info("LogCollector '{}' created", name_);
  logger_->info("  Protocol: {}", protocol_);
  logger_->info("  Consume topic: {}", consume_topic_);
  logger_->info("  Produce topics: {}", produce_topics_.size());
  logger_->info("  Batch size: {}, timeout: {}ms", batch_size_,
                batch_timeout_ms_);
}

LogCollector::~LogCollector() { stop(); }

void LogCollector::start() {
  if (running_) {
    logger_->warn("LogCollector already running");
    return;
  }

  running_ = true;

  logger_->info("Starting LogCollector '{}'", name_);

  // Start consumer thread
  consumer_thread_ = std::thread(&LogCollector::consume_loop, this);

  // Start batch timeout handler thread
  batch_timer_thread_ = std::thread(&LogCollector::batch_timeout_handler, this);

  logger_->info("LogCollector '{}' started", name_);
}

void LogCollector::stop() {
  if (!running_) {
    return;
  }

  logger_->info("Stopping LogCollector '{}'...", name_);
  running_ = false;

  // Join threads
  if (consumer_thread_.joinable()) {
    consumer_thread_.join();
  }

  if (batch_timer_thread_.joinable()) {
    batch_timer_thread_.join();
  }

  // Flush remaining batches
  auto remaining_batches = batch_handler_->flush_all();
  if (!remaining_batches.empty()) {
    logger_->info("Flushing {} remaining batches", remaining_batches.size());
    send_batches(remaining_batches);
  }

  logger_->info("LogCollector '{}' stopped", name_);
  logger_->info("  Total consumed: {}", messages_consumed_.load());
  logger_->info("  Total validated: {}", messages_validated_.load());
  logger_->info("  Total failed: {}", messages_failed_.load());
  logger_->info("  Total batches sent: {}", batches_sent_.load());
}

void LogCollector::consume_loop() {
  logger_->info("Consumer loop started");

  while (running_) {
    try {
      // Poll for messages with 1 second timeout
      consumer_->poll(
          [this](const std::string &topic, const std::string &key,
                 const std::string &value, int64_t timestamp) {
            if (!running_)
              return;

            logger_->trace("Consumed message from topic '{}'", topic);
            process_message(value);

            // Commit the offset
            consumer_->commit();
          },
          1000); // 1 second timeout

    } catch (const std::exception &e) {
      logger_->error("Error in consumer loop: {}", e.what());
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }

  logger_->info("Consumer loop stopped");
}

void LogCollector::process_message(const std::string &message) {
  messages_consumed_++;

  try {
    // Validate the log line
    auto logline = validate_logline(message);
    messages_validated_++;

    // Calculate subnet ID
    std::string subnet_id = get_subnet_id(logline.fields["src_ip"]);

    // Add to batch
    bool batch_ready = batch_handler_->add_logline(subnet_id, logline);

    if (batch_ready) {
      try {
        auto batch = batch_handler_->get_batch(subnet_id);
        send_batches({batch});
      } catch (const std::exception &e) {
        logger_->error("Failed to get batch: {}", e.what());
      }
    }

  } catch (const std::exception &e) {
    messages_failed_++;
    log_failed_logline(message, e.what());
    logger_->debug("Validation failed: {}", e.what());
  }
}

base::LogLine LogCollector::validate_logline(const std::string &message) {
  // Parse JSON
  json j = json::parse(message);

  base::LogLine logline;
  logline.logline_id = base::utils::generate_uuid();

  // Validate required fields
  std::vector<std::string> required_fields = {"ts", "src_ip"};

  for (const auto &field_name : required_fields) {
    if (!j.contains(field_name)) {
      throw std::runtime_error("Missing required field: " + field_name);
    }
  }

  // Extract all fields
  for (auto &[key, value] : j.items()) {
    if (value.is_string()) {
      logline.fields[key] = value.get<std::string>();
    } else {
      logline.fields[key] = value.dump();
    }
  }

  // Parse timestamp
  logline.timestamp =
      std::chrono::system_clock::now(); // TODO: Parse from "ts" field

  // Validate fields against config
  // TODO: Implement field validation using validation_config_

  return logline;
}

std::string LogCollector::get_subnet_id(const std::string &ip_address) {
  // Determine if IPv4 or IPv6
  if (ip_address.find(':') != std::string::npos) {
    // IPv6
    return base::utils::get_subnet_id(ip_address, ipv6_prefix_length_);
  } else {
    // IPv4
    return base::utils::get_subnet_id(ip_address, ipv4_prefix_length_);
  }
}

void LogCollector::batch_timeout_handler() {
  logger_->info("Batch timeout handler started (interval: {}ms)",
                batch_timeout_ms_ / 2);

  // Check every half the timeout period
  auto check_interval = std::chrono::milliseconds(batch_timeout_ms_ / 2);

  while (running_) {
    std::this_thread::sleep_for(check_interval);

    // Get ready batches
    auto ready_batches = batch_handler_->get_ready_batches();

    if (!ready_batches.empty()) {
      logger_->debug("Timeout handler sending {} batches",
                     ready_batches.size());
      send_batches(ready_batches);
    }
  }

  logger_->info("Batch timeout handler stopped");
}

void LogCollector::send_batches(const std::vector<base::Batch> &batches) {
  for (const auto &batch : batches) {
    std::string batch_json = batch.to_json();

    // Send to all produce topics using producer
    if (producer_) {
      try {
        producer_->send(batch.batch_id, batch_json);
        logger_->info("Sent batch {} ({} loglines)", batch.batch_id,
                      batch.loglines.size());
      } catch (const std::exception &e) {
        logger_->error("Failed to send batch {}: {}", batch.batch_id, e.what());
      }
    }

    batches_sent_++;
  }
}

void LogCollector::log_failed_logline(const std::string &message,
                                      const std::string &reason) {
  // TODO: Log to ClickHouse when implemented
  logger_->debug("Failed logline: {} (reason: {})", message, reason);
}

LogCollector::Stats LogCollector::get_stats() const {
  Stats stats;
  stats.messages_consumed = messages_consumed_.load();
  stats.messages_validated = messages_validated_.load();
  stats.messages_failed = messages_failed_.load();
  stats.batches_sent = batches_sent_.load();
  stats.avg_validation_time_ms = 0.0; // TODO: Track timing
  stats.avg_batch_time_ms = 0.0;      // TODO: Track timing
  return stats;
}

// ============================================================================
// Factory Function
// ============================================================================

std::vector<std::shared_ptr<LogCollector>>
create_logcollectors(std::shared_ptr<config::Config> config) {
  std::vector<std::shared_ptr<LogCollector>> collectors;
  auto logger = base::Logger::get_logger("logcollector.factory");

  // Get topic prefixes
  auto &env = config->environment;
  std::string consume_prefix =
      env.kafka_topics_prefix["logserver_to_collector"];
  std::string produce_prefix =
      env.kafka_topics_prefix["batch_sender_to_prefilter"];

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

  logger->info("Creating LogCollectors for {} collectors",
               config->pipeline.collectors.size());
  logger->info("Kafka brokers: {}", bootstrap_servers);

  // Create one LogCollector per configured collector
  for (const auto &collector_config : config->pipeline.collectors) {
    std::string consume_topic = consume_prefix + "-" + collector_config.name;
    std::string group_id = "logcollector-" + collector_config.name;

    // Find all prefilters for this collector
    std::vector<std::string> produce_topics;
    for (const auto &prefilter : config->pipeline.prefilters) {
      if (prefilter.collector_name == collector_config.name) {
        std::string topic = produce_prefix + "-" + prefilter.name;
        produce_topics.push_back(topic);
      }
    }

    logger->info("Creating LogCollector '{}' for protocol '{}'",
                 collector_config.name, collector_config.protocol_base);
    logger->info("  Consume from: {}", consume_topic);
    logger->info("  Produce to {} topics", produce_topics.size());

    auto collector = std::make_shared<LogCollector>(
        collector_config.name, collector_config.protocol_base, consume_topic,
        produce_topics, collector_config.required_log_information, config,
        bootstrap_servers, group_id);

    collectors.push_back(collector);
  }

  return collectors;
}

} // namespace logcollector
} // namespace hamstring
