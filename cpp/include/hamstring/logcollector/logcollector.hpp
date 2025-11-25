#pragma once

#include "hamstring/base/clickhouse_sender.hpp"
#include "hamstring/base/data_classes.hpp"
#include "hamstring/base/kafka_handler.hpp"
#include "hamstring/base/logger.hpp"
#include "hamstring/config/config.hpp"
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace hamstring {
namespace logcollector {

/**
 * @brief Buffered batch for efficient log line aggregation
 *
 * Thread-safe batch container that groups log lines by subnet ID.
 * Automatically triggers sends when size or timeout limits are reached.
 */
class BufferedBatch {
public:
  /**
   * @brief Construct a new Buffered Batch
   *
   * @param collector_name Name of the collector
   * @param batch_size Max messages per batch
   * @param batch_timeout_ms Timeout in milliseconds
   */
  BufferedBatch(const std::string &collector_name, size_t batch_size,
                int batch_timeout_ms);

  ~BufferedBatch();

  /**
   * @brief Add a log line to the batch
   *
   * Thread-safe method to add a message to the appropriate batch.
   *
   * @param subnet_id Subnet identifier for batching
   * @param logline LogLine to add
   * @return true if batch is ready to send
   */
  bool add_logline(const std::string &subnet_id, const base::LogLine &logline);

  /**
   * @brief Get completed batch for a subnet
   *
   * @param subnet_id Subnet identifier
   * @return Batch object ready to send
   */
  base::Batch get_batch(const std::string &subnet_id);

  /**
   * @brief Get all ready batches
   *
   * @return Vector of batches that are ready to send
   */
  std::vector<base::Batch> get_ready_batches();

  /**
   * @brief Force send all batches (timeout or shutdown)
   *
   * @return Vector of all batches
   */
  std::vector<base::Batch> flush_all();

  /**
   * @brief Get statistics about current batches
   */
  struct Stats {
    size_t total_batches;
    size_t total_loglines;
    size_t largest_batch;
    std::chrono::milliseconds oldest_batch_age;
  };

  Stats get_stats() const;

private:
  struct BatchData {
    std::string batch_id;
    std::string subnet_id;
    std::vector<base::LogLine> loglines;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point last_updated;
  };

  mutable std::mutex batches_mutex_;
  std::unordered_map<std::string, BatchData> batches_;

  std::string collector_name_;
  size_t batch_size_;
  std::chrono::milliseconds batch_timeout_;

  // Metrics
  std::atomic<size_t> total_loglines_processed_{0};
  std::atomic<size_t> total_batches_sent_{0};

  std::shared_ptr<spdlog::logger> logger_;
};

/**
 * @brief LogCollector - Validates and batches log lines
 *
 * Main component for log collection stage. Features:
 * - Multi-threaded log line processing
 * - Field validation with configurable rules
 * - Subnet-based batching
 * - Automatic batch dispatch
 * - ClickHouse monitoring integration
 * - Horizontal scalability support
 */
class LogCollector {
public:
  /**
   * @brief Construct a new Log Collector
   *
   * @param name Collector name
   * @param protocol Protocol type (dns, http, etc.)
   * @param consume_topic Kafka topic to consume from
   * @param produce_topics Kafka topics to produce to
   * @param validation_config Field validation rules
   * @param config Global configuration
   * @param bootstrap_servers Kafka broker addresses
   * @param group_id Kafka consumer group ID
   */
  LogCollector(const std::string &name, const std::string &protocol,
               const std::string &consume_topic,
               const std::vector<std::string> &produce_topics,
               const std::vector<config::FieldConfig> &validation_config,
               std::shared_ptr<config::Config> config,
               const std::string &bootstrap_servers,
               const std::string &group_id);

  ~LogCollector();

  /**
   * @brief Start the collector
   *
   * Begins consuming from Kafka and processing log lines.
   */
  void start();

  /**
   * @brief Stop the collector gracefully
   *
   * Finishes processing in-flight messages and flushes batches.
   */
  void stop();

  /**
   * @brief Check if collector is running
   */
  bool is_running() const { return running_; }

  /**
   * @brief Get collector statistics
   */
  struct Stats {
    size_t messages_consumed;
    size_t messages_validated;
    size_t messages_failed;
    size_t batches_sent;
    double avg_validation_time_ms;
    double avg_batch_time_ms;
  };

  Stats get_stats() const;

private:
  /**
   * @brief Main message consumption loop
   */
  void consume_loop();

  /**
   * @brief Process a single message
   *
   * @param message Raw message string from Kafka
   */
  void process_message(const std::string &message);

  /**
   * @brief Validate and parse a log line
   *
   * @param message Raw JSON message
   * @return Validated LogLine object
   * @throws std::runtime_error if validation fails
   */
  base::LogLine validate_logline(const std::string &message);

  /**
   * @brief Calculate subnet ID from IP address
   *
   * @param ip_address IP address string
   * @return Subnet ID string
   */
  std::string get_subnet_id(const std::string &ip_address);

  /**
   * @brief Batch timeout handler
   *
   * Periodically checks for batches that need to be sent due to timeout.
   */
  void batch_timeout_handler();

  /**
   * @brief Send batches to Kafka
   *
   * @param batches Batches to send
   */
  void send_batches(const std::vector<base::Batch> &batches);

  /**
   * @brief Log failed validation to ClickHouse
   *
   * @param message Original message
   * @param reason Failure reason
   */
  void log_failed_logline(const std::string &message,
                          const std::string &reason);

  // Configuration
  std::string name_;
  std::string protocol_;
  std::string consume_topic_;
  std::vector<std::string> produce_topics_;
  std::vector<config::FieldConfig> validation_config_;
  std::shared_ptr<config::Config> config_;

  // Batch configuration
  size_t batch_size_;
  int batch_timeout_ms_;
  int ipv4_prefix_length_;
  int ipv6_prefix_length_;

  // Components
  std::unique_ptr<BufferedBatch> batch_handler_;
  std::unique_ptr<base::KafkaConsumeHandler> consumer_;
  std::unique_ptr<base::KafkaProduceHandler> producer_;
  std::shared_ptr<base::ClickHouseSender> clickhouse_;

  // Threading
  std::atomic<bool> running_{false};
  std::thread consumer_thread_;
  std::thread batch_timer_thread_;

  // Metrics
  std::atomic<size_t> messages_consumed_{0};
  std::atomic<size_t> messages_validated_{0};
  std::atomic<size_t> messages_failed_{0};
  std::atomic<size_t> batches_sent_{0};

  // Logger
  std::shared_ptr<spdlog::logger> logger_;
};

/**
 * @brief Create LogCollector instances from configuration
 *
 * Factory function that creates collectors for each configured collector.
 * Supports horizontal scaling by creating multiple instances.
 *
 * @param config Application configuration
 * @return Vector of LogCollector instances
 */
std::vector<std::shared_ptr<LogCollector>>
create_logcollectors(std::shared_ptr<config::Config> config);

} // namespace logcollector
} // namespace hamstring
