#pragma once

#include "hamstring/base/clickhouse_sender.hpp"
#include "hamstring/base/data_classes.hpp"
#include "hamstring/base/kafka_handler.hpp"
#include "hamstring/base/logger.hpp"
#include "hamstring/config/config.hpp"
#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace hamstring {
namespace prefilter {

/**
 * @brief Prefilter - Filters batches based on relevance rules
 *
 * The Prefilter consumes batches from the LogCollector stage and applies
 * relevance-based filtering. Only relevant log lines are forwarded to
 * the Inspector stage for anomaly detection.
 *
 * Features:
 * - Rule-based relevance filtering
 * - Batch processing with metrics
 * - ClickHouse monitoring integration
 * - Multi-threaded processing
 */
class Prefilter {
public:
  /**
   * @brief Construct a new Prefilter
   *
   * @param name Prefilter name
   * @param consume_topic Kafka topic to consume batches from
   * @param produce_topics Kafka topics to produce filtered batches to
   * @param relevance_function Name of relevance function to use
   * @param validation_config Field validation rules
   * @param config Global configuration
   * @param bootstrap_servers Kafka broker addresses
   * @param group_id Kafka consumer group ID
   */
  Prefilter(const std::string &name, const std::string &consume_topic,
            const std::vector<std::string> &produce_topics,
            const std::string &relevance_function,
            const std::vector<config::FieldConfig> &validation_config,
            std::shared_ptr<config::Config> config,
            const std::string &bootstrap_servers, const std::string &group_id);

  ~Prefilter();

  /**
   * @brief Start the prefilter
   *
   * Begins consuming batches from Kafka and filtering them.
   */
  void start();

  /**
   * @brief Stop the prefilter gracefully
   */
  void stop();

  /**
   * @brief Check if prefilter is running
   */
  bool is_running() const { return running_; }

  /**
   * @brief Get prefilter statistics
   */
  struct Stats {
    size_t batches_consumed;
    size_t batches_sent;
    size_t loglines_received;
    size_t loglines_filtered;
    size_t loglines_sent;
  };

  Stats get_stats() const;

private:
  /**
   * @brief Main message consumption loop
   */
  void consume_loop();

  /**
   * @brief Process a single batch
   *
   * @param batch Batch to process
   */
  void process_batch(const base::Batch &batch);

  /**
   * @brief Check if a log line is relevant
   *
   * @param logline LogLine to check
   * @return true if relevant, false otherwise
   */
  bool check_relevance(const base::LogLine &logline);

  /**
   * @brief Send filtered batch to Kafka
   *
   * @param batch Filtered batch to send
   */
  void send_batch(const base::Batch &batch);

  // Configuration
  std::string name_;
  std::string consume_topic_;
  std::vector<std::string> produce_topics_;
  std::string relevance_function_;
  std::vector<config::FieldConfig> validation_config_;
  std::shared_ptr<config::Config> config_;

  // Components
  std::unique_ptr<base::KafkaConsumeHandler> consumer_;
  std::vector<std::unique_ptr<base::KafkaProduceHandler>> producers_;
  std::shared_ptr<base::ClickHouseSender> clickhouse_;

  // Threading
  std::atomic<bool> running_{false};
  std::thread worker_thread_;

  // Metrics
  std::atomic<size_t> batches_consumed_{0};
  std::atomic<size_t> batches_sent_{0};
  std::atomic<size_t> loglines_received_{0};
  std::atomic<size_t> loglines_filtered_{0};
  std::atomic<size_t> loglines_sent_{0};

  // Logger
  std::shared_ptr<spdlog::logger> logger_;
};

/**
 * @brief Create Prefilter instances from configuration
 *
 * @param config Application configuration
 * @return Vector of Prefilter instances
 */
std::vector<std::shared_ptr<Prefilter>>
create_prefilters(std::shared_ptr<config::Config> config);

} // namespace prefilter
} // namespace hamstring
