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
namespace inspector {

/**
 * @brief Inspector - Performs anomaly detection on batches
 *
 * The Inspector consumes filtered batches from the Prefilter stage and
 * performs anomaly detection. Suspicious batches are grouped by source IP
 * and forwarded to the Detector stage for further analysis.
 *
 * Features:
 * - Anomaly detection using statistical models
 * - IP-based batch grouping
 * - Threshold-based filtering
 * - ClickHouse monitoring integration
 */
class Inspector {
public:
  /**
   * @brief Construct a new Inspector
   *
   * @param name Inspector name
   * @param consume_topic Kafka topic to consume batches from
   * @param produce_topics Kafka topics to produce suspicious batches to
   * @param mode Anomaly detection mode (univariate, multivariate, ensemble)
   * @param anomaly_threshold Threshold for anomaly ratio
   * @param score_threshold Threshold for anomaly scores
   * @param config Global configuration
   * @param bootstrap_servers Kafka broker addresses
   * @param group_id Kafka consumer group ID
   */
  Inspector(const std::string &name, const std::string &consume_topic,
            const std::vector<std::string> &produce_topics,
            const std::string &mode, double anomaly_threshold,
            double score_threshold, std::shared_ptr<config::Config> config,
            const std::string &bootstrap_servers, const std::string &group_id);

  ~Inspector();

  /**
   * @brief Start the inspector
   *
   * Begins consuming batches from Kafka and performing anomaly detection.
   */
  void start();

  /**
   * @brief Stop the inspector gracefully
   */
  void stop();

  /**
   * @brief Check if inspector is running
   */
  bool is_running() const { return running_; }

  /**
   * @brief Get inspector statistics
   */
  struct Stats {
    size_t batches_consumed;
    size_t batches_suspicious;
    size_t batches_filtered;
    size_t suspicious_batches_sent;
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
   * @param batch Batch to inspect
   */
  void process_batch(const base::Batch &batch);

  /**
   * @brief Check if batch is suspicious
   *
   * @param batch Batch to check
   * @return true if suspicious, false otherwise
   */
  bool is_suspicious(const base::Batch &batch);

  /**
   * @brief Send suspicious batches to Kafka
   *
   * @param batches_by_ip Map of IP address to batches
   * @param original_batch_id Original batch ID
   */
  void send_suspicious_batches(
      const std::map<std::string, std::vector<std::shared_ptr<base::LogLine>>>
          &batches_by_ip,
      const std::string &original_batch_id);

  // Configuration
  std::string name_;
  std::string consume_topic_;
  std::vector<std::string> produce_topics_;
  std::string mode_;
  double anomaly_threshold_;
  double score_threshold_;
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
  std::atomic<size_t> batches_suspicious_{0};
  std::atomic<size_t> batches_filtered_{0};
  std::atomic<size_t> suspicious_batches_sent_{0};

  // Logger
  std::shared_ptr<spdlog::logger> logger_;
};

/**
 * @brief Create Inspector instances from configuration
 *
 * @param config Application configuration
 * @return Vector of Inspector instances
 */
std::vector<std::shared_ptr<Inspector>>
create_inspectors(std::shared_ptr<config::Config> config);

} // namespace inspector
} // namespace hamstring
