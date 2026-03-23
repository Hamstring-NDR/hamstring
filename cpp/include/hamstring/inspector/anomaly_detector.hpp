#pragma once

#include "hamstring/base/data_classes.hpp"
#include "hamstring/config/config.hpp"
#include <deque>
#include <memory>
#include <vector>

namespace hamstring {
namespace inspector {

/**
 * @brief Metrics extracted from a batch for anomaly detection
 */
struct BatchMetrics {
  double nxdomain_rate;       // Ratio of NXDOMAIN responses
  double avg_domain_length;   // Average domain name length
  double domain_entropy;      // Shannon entropy of domain names
  double unique_domain_ratio; // Unique domains / total queries
  size_t total_queries;       // Total number of queries
  double query_rate;          // Queries per second

  // Character distribution
  double numeric_char_ratio; // Ratio of numeric characters
  double special_char_ratio; // Ratio of special characters
};

/**
 * @brief Statistical anomaly detector using time-series analysis
 *
 * This class implements lightweight statistical methods for anomaly detection:
 * - Z-score outlier detection
 * - Moving averages and standard deviations
 * - Threshold-based rules
 * - Multi-metric ensemble scoring
 */
class AnomalyDetector {
public:
  /**
   * @brief Construct anomaly detector with configuration
   *
   * @param config Inspector configuration with thresholds
   */
  explicit AnomalyDetector(const config::InspectorConfig &config);

  /**
   * @brief Analyze batch and return suspicion score
   *
   * @param batch Batch to analyze
   * @return Suspicion score [0.0, 1.0] where higher = more suspicious
   */
  double analyze_batch(const base::Batch &batch);

  /**
   * @brief Update internal statistics with new batch
   *
   * @param batch Batch to update statistics with
   */
  void update_state(const base::Batch &batch);

  /**
   * @brief Get current statistics (for debugging/monitoring)
   */
  struct Statistics {
    double mean_nxdomain_rate;
    double stddev_nxdomain_rate;
    double mean_domain_length;
    double stddev_domain_length;
    size_t samples_count;
  };

  Statistics get_statistics() const;

private:
  /**
   * @brief Extract metrics from batch
   */
  BatchMetrics extract_metrics(const base::Batch &batch);

  /**
   * @brief Calculate Z-score for a value given historical data
   */
  double calculate_z_score(double value, double mean, double stddev);

  /**
   * @brief Update rolling statistics
   */
  void update_rolling_stats();

  /**
   * @brief Calculate Shannon entropy of a string
   */
  double calculate_entropy(const std::string &str);

  /**
   * @brief Detect anomalies using multiple methods
   */
  double detect_anomalies(const BatchMetrics &metrics);

  // Configuration
  config::InspectorConfig config_;

  // Rolling window statistics
  struct RollingStats {
    std::deque<double> nxdomain_rates;
    std::deque<double> avg_domain_lengths;
    std::deque<double> query_rates;
    std::deque<double> entropies;

    double mean_nxdomain = 0.0;
    double stddev_nxdomain = 0.0;
    double mean_domain_length = 0.0;
    double stddev_domain_length = 0.0;
    double mean_entropy = 0.0;
    double stddev_entropy = 0.0;
  };

  RollingStats stats_;
  size_t window_size_;
  double z_score_threshold_;
};

} // namespace inspector
} // namespace hamstring
