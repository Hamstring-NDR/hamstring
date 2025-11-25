#include "hamstring/prefilter/prefilter.hpp"
#include "hamstring/base/utils.hpp"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace hamstring {
namespace prefilter {

Prefilter::Prefilter(const std::string &name, const std::string &consume_topic,
                     const std::vector<std::string> &produce_topics,
                     const std::string &relevance_function,
                     const std::vector<config::FieldConfig> &validation_config,
                     std::shared_ptr<config::Config> config,
                     const std::string &bootstrap_servers,
                     const std::string &group_id)
    : name_(name), consume_topic_(consume_topic),
      produce_topics_(produce_topics), relevance_function_(relevance_function),
      validation_config_(validation_config), config_(config) {
  logger_ = base::Logger::get_logger("prefilter." + name_);

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

  logger_->info("Prefilter '{}' created", name_);
  logger_->info("  Relevance function: {}", relevance_function_);
  logger_->info("  Consume topic: {}", consume_topic_);
  logger_->info("  Produce topics: {}", produce_topics_.size());
}

Prefilter::~Prefilter() { stop(); }

void Prefilter::start() {
  if (running_) {
    logger_->warn("Prefilter already running");
    return;
  }

  running_ = true;

  logger_->info("Prefilter '{}' started", name_);
  logger_->info("  ⤷ receiving on Kafka topic '{}'", consume_topic_);
  logger_->info("  ⤷ sending to {} topics", produce_topics_.size());
  for (const auto &topic : produce_topics_) {
    logger_->info("      - {}", topic);
  }

  // Start worker thread
  worker_thread_ = std::thread(&Prefilter::consume_loop, this);
}

void Prefilter::stop() {
  if (!running_) {
    return;
  }

  logger_->info("Stopping Prefilter '{}'...", name_);
  running_ = false;

  if (worker_thread_.joinable()) {
    worker_thread_.join();
  }

  // Flush all producers
  for (auto &producer : producers_) {
    producer->flush();
  }

  logger_->info("Prefilter '{}' stopped", name_);
  logger_->info("  Batches consumed: {}", batches_consumed_.load());
  logger_->info("  Batches sent: {}", batches_sent_.load());
  logger_->info("  Loglines received: {}", loglines_received_.load());
  logger_->info("  Loglines filtered: {}", loglines_filtered_.load());
  logger_->info("  Loglines sent: {}", loglines_sent_.load());
}

void Prefilter::consume_loop() {
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

void Prefilter::process_batch(const base::Batch &batch) {
  batches_consumed_++;
  loglines_received_ += batch.loglines.size();

  // Create filtered batch
  base::Batch filtered_batch;
  filtered_batch.batch_id = batch.batch_id;
  filtered_batch.subnet_id = batch.subnet_id;
  filtered_batch.collector_name = batch.collector_name;
  filtered_batch.created_at = batch.created_at;
  filtered_batch.timestamp_in = std::chrono::system_clock::now();

  // Filter log lines based on relevance
  for (const auto &logline_ptr : batch.loglines) {
    if (check_relevance(*logline_ptr)) {
      filtered_batch.loglines.push_back(logline_ptr);
    } else {
      loglines_filtered_++;
    }
  }

  if (!filtered_batch.loglines.empty()) {
    logger_->info("Filtered batch {}: {} → {} loglines", batch.batch_id,
                  batch.loglines.size(), filtered_batch.loglines.size());

    loglines_sent_ += filtered_batch.loglines.size();
    send_batch(filtered_batch);
  } else {
    logger_->debug("Batch {} completely filtered out", batch.batch_id);
  }
}

bool Prefilter::check_relevance(const base::LogLine &logline) {
  // TODO: Implement actual relevance checking based on relevance_function_
  // For now, use a simple heuristic

  if (relevance_function_ == "always_relevant") {
    return true;
  }

  if (relevance_function_ == "check_nxdomain") {
    // Check if this is an NXDOMAIN response (common DGA indicator)
    auto it = logline.fields.find("rcode");
    if (it != logline.fields.end()) {
      return it->second == "NXDOMAIN" || it->second == "3";
    }
  }

  if (relevance_function_ == "check_query_length") {
    // Check if query domain is suspiciously long (possible DGA)
    auto it = logline.fields.find("query");
    if (it != logline.fields.end()) {
      return it->second.length() > 20; // Heuristic threshold
    }
  }

  // Default: pass through
  return true;
}

void Prefilter::send_batch(const base::Batch &batch) {
  std::string batch_json = batch.to_json();

  // Send to all output topics
  for (size_t i = 0; i < producers_.size(); ++i) {
    try {
      producers_[i]->send(batch.batch_id, batch_json);
      logger_->trace("Sent batch {} to topic {}", batch.batch_id,
                     produce_topics_[i]);
    } catch (const std::exception &e) {
      logger_->error("Failed to send batch to {}: {}", produce_topics_[i],
                     e.what());
    }
  }

  batches_sent_++;
}

Prefilter::Stats Prefilter::get_stats() const {
  Stats stats;
  stats.batches_consumed = batches_consumed_.load();
  stats.batches_sent = batches_sent_.load();
  stats.loglines_received = loglines_received_.load();
  stats.loglines_filtered = loglines_filtered_.load();
  stats.loglines_sent = loglines_sent_.load();
  return stats;
}

std::vector<std::shared_ptr<Prefilter>>
create_prefilters(std::shared_ptr<config::Config> config) {
  std::vector<std::shared_ptr<Prefilter>> prefilters;
  auto logger = base::Logger::get_logger("prefilter.factory");

  // Get topic prefixes
  auto &env = config->environment;
  std::string consume_prefix =
      env.kafka_topics_prefix["batch_sender_to_prefilter"];
  std::string produce_prefix =
      env.kafka_topics_prefix["prefilter_to_inspector"];

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

  logger->info("Creating Prefilters for {} prefilters",
               config->pipeline.prefilters.size());
  logger->info("Kafka brokers: {}", bootstrap_servers);

  // Create one Prefilter per configured prefilter
  for (const auto &prefilter_config : config->pipeline.prefilters) {
    std::string consume_topic = consume_prefix + "-" + prefilter_config.name;
    std::string group_id = "prefilter-" + prefilter_config.name;

    // Find all inspectors for this prefilter
    std::vector<std::string> produce_topics;
    for (const auto &inspector : config->pipeline.inspectors) {
      if (inspector.prefilter_name == prefilter_config.name) {
        std::string topic = produce_prefix + "-" + inspector.name;
        produce_topics.push_back(topic);
      }
    }

    // Get validation config from collector
    std::vector<config::FieldConfig> validation_config;
    for (const auto &collector : config->pipeline.collectors) {
      if (collector.name == prefilter_config.collector_name) {
        validation_config = collector.required_log_information;
        break;
      }
    }

    logger->info("Creating Prefilter '{}'", prefilter_config.name);
    logger->info("  Consume from: {}", consume_topic);
    logger->info("  Produce to {} topics", produce_topics.size());
    logger->info("  Relevance function: {}", prefilter_config.relevance_method);

    auto prefilter = std::make_shared<Prefilter>(
        prefilter_config.name, consume_topic, produce_topics,
        prefilter_config.relevance_method, validation_config, config,
        bootstrap_servers, group_id);

    prefilters.push_back(prefilter);
  }

  return prefilters;
}

} // namespace prefilter
} // namespace hamstring
