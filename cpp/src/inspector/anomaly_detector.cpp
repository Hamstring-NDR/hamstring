#include "hamstring/inspector/anomaly_detector.hpp"
#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace hamstring {
namespace inspector {

AnomalyDetector::AnomalyDetector(const config::InspectorConfig &config)
    : config_(config), window_size_(100), // Default rolling window size
      z_score_threshold_(3.0)             // 3-sigma rule (99.7% of normal data)
{
  // Override defaults from config if available
  if (config_.time_range > 0) {
    window_size_ = static_cast<size_t>(config_.time_range);
  }
}

BatchMetrics AnomalyDetector::extract_metrics(const base::Batch &batch) {
  BatchMetrics metrics{};

  if (batch.loglines.empty()) {
    return metrics;
  }

  metrics.total_queries = batch.loglines.size();

  size_t nxdomain_count = 0;
  double total_length = 0.0;
  std::unordered_set<std::string> unique_domains;
  size_t numeric_chars = 0;
  size_t special_chars = 0;
  size_t total_chars = 0;
  std::string all_domains;

  for (const auto &logline : batch.loglines) {
    // Extract query/domain
    auto query_it = logline->fields.find("query");
    if (query_it != logline->fields.end()) {
      std::string domain = query_it->second;

      // Remove trailing dot
      if (!domain.empty() && domain.back() == '.') {
        domain.pop_back();
      }

      total_length += domain.length();
      unique_domains.insert(domain);
      all_domains += domain;

      // Count character types
      for (char c : domain) {
        total_chars++;
        if (std::isdigit(c)) {
          numeric_chars++;
        } else if (!std::isalnum(c) && c != '.' && c != '-') {
          special_chars++;
        }
      }
    }

    // Check for NXDOMAIN
    auto rcode_it = logline->fields.find("rcode");
    if (rcode_it != logline->fields.end()) {
      if (rcode_it->second == "NXDOMAIN" || rcode_it->second == "3") {
        nxdomain_count++;
      }
    }
  }

  // Calculate metrics
  metrics.nxdomain_rate =
      static_cast<double>(nxdomain_count) / metrics.total_queries;
  metrics.avg_domain_length = total_length / metrics.total_queries;
  metrics.unique_domain_ratio =
      static_cast<double>(unique_domains.size()) / metrics.total_queries;

  if (total_chars > 0) {
    metrics.numeric_char_ratio =
        static_cast<double>(numeric_chars) / total_chars;
    metrics.special_char_ratio =
        static_cast<double>(special_chars) / total_chars;
  }

  // Calculate entropy of all domains combined
  metrics.domain_entropy = calculate_entropy(all_domains);

  // Calculate query rate (queries per second)
  // Assuming batch represents ~1 second of traffic for simplicity
  metrics.query_rate = metrics.total_queries;

  return metrics;
}

double AnomalyDetector::calculate_entropy(const std::string &str) {
  if (str.empty()) {
    return 0.0;
  }

  // Count character frequencies
  std::unordered_map<char, size_t> freq;
  for (char c : str) {
    freq[c]++;
  }

  // Calculate Shannon entropy
  double entropy = 0.0;
  double len = static_cast<double>(str.length());

  for (const auto &[ch, count] : freq) {
    double prob = static_cast<double>(count) / len;
    entropy -= prob * std::log2(prob);
  }

  return entropy;
}

double AnomalyDetector::calculate_z_score(double value, double mean,
                                          double stddev) {
  if (stddev == 0.0) {
    return 0.0;
  }
  return (value - mean) / stddev;
}

void AnomalyDetector::update_rolling_stats() {
  // Calculate mean and stddev for each metric

  // NXDOMAIN rate
  if (!stats_.nxdomain_rates.empty()) {
    double sum = 0.0;
    for (double val : stats_.nxdomain_rates) {
      sum += val;
    }
    stats_.mean_nxdomain = sum / stats_.nxdomain_rates.size();

    double variance = 0.0;
    for (double val : stats_.nxdomain_rates) {
      variance += (val - stats_.mean_nxdomain) * (val - stats_.mean_nxdomain);
    }
    stats_.stddev_nxdomain = std::sqrt(variance / stats_.nxdomain_rates.size());
  }

  // Domain length
  if (!stats_.avg_domain_lengths.empty()) {
    double sum = 0.0;
    for (double val : stats_.avg_domain_lengths) {
      sum += val;
    }
    stats_.mean_domain_length = sum / stats_.avg_domain_lengths.size();

    double variance = 0.0;
    for (double val : stats_.avg_domain_lengths) {
      variance +=
          (val - stats_.mean_domain_length) * (val - stats_.mean_domain_length);
    }
    stats_.stddev_domain_length =
        std::sqrt(variance / stats_.avg_domain_lengths.size());
  }

  // Entropy
  if (!stats_.entropies.empty()) {
    double sum = 0.0;
    for (double val : stats_.entropies) {
      sum += val;
    }
    stats_.mean_entropy = sum / stats_.entropies.size();

    double variance = 0.0;
    for (double val : stats_.entropies) {
      variance += (val - stats_.mean_entropy) * (val - stats_.mean_entropy);
    }
    stats_.stddev_entropy = std::sqrt(variance / stats_.entropies.size());
  }
}

double AnomalyDetector::detect_anomalies(const BatchMetrics &metrics) {
  double anomaly_score = 0.0;
  int score_count = 0;

  // Need minimum samples for statistical detection
  if (stats_.nxdomain_rates.size() < 10) {
    // Use simple threshold-based detection for cold start
    if (metrics.nxdomain_rate > config_.anomaly_threshold) {
      anomaly_score += 0.5;
      score_count++;
    }

    if (metrics.avg_domain_length > 30.0) {
      anomaly_score += 0.3;
      score_count++;
    }

    if (metrics.domain_entropy > 4.5) {
      anomaly_score += 0.2;
      score_count++;
    }

    return score_count > 0 ? anomaly_score : 0.0;
  }

  // Z-score based detection

  // 1. NXDOMAIN rate anomaly
  double z_nxdomain = calculate_z_score(
      metrics.nxdomain_rate, stats_.mean_nxdomain, stats_.stddev_nxdomain);

  if (std::abs(z_nxdomain) > z_score_threshold_) {
    // Normalize to [0, 1]
    anomaly_score +=
        std::min(1.0, std::abs(z_nxdomain) / (z_score_threshold_ * 2));
    score_count++;
  }

  // 2. Domain length anomaly
  double z_length =
      calculate_z_score(metrics.avg_domain_length, stats_.mean_domain_length,
                        stats_.stddev_domain_length);

  if (std::abs(z_length) > z_score_threshold_) {
    anomaly_score +=
        std::min(1.0, std::abs(z_length) / (z_score_threshold_ * 2));
    score_count++;
  }

  // 3. Entropy anomaly
  double z_entropy = calculate_z_score(
      metrics.domain_entropy, stats_.mean_entropy, stats_.stddev_entropy);

  if (std::abs(z_entropy) > z_score_threshold_) {
    anomaly_score +=
        std::min(1.0, std::abs(z_entropy) / (z_score_threshold_ * 2));
    score_count++;
  }

  // 4. Threshold-based rules (additional signals)
  if (metrics.nxdomain_rate > 0.7) {
    anomaly_score += 0.5;
    score_count++;
  }

  if (metrics.numeric_char_ratio > 0.3) {
    anomaly_score += 0.2;
    score_count++;
  }

  // Average the scores
  return score_count > 0 ? (anomaly_score / score_count) : 0.0;
}

double AnomalyDetector::analyze_batch(const base::Batch &batch) {
  // Extract metrics
  BatchMetrics metrics = extract_metrics(batch);

  // Detect anomalies
  double score = detect_anomalies(metrics);

  return score;
}

void AnomalyDetector::update_state(const base::Batch &batch) {
  // Extract metrics
  BatchMetrics metrics = extract_metrics(batch);

  // Update rolling windows
  stats_.nxdomain_rates.push_back(metrics.nxdomain_rate);
  stats_.avg_domain_lengths.push_back(metrics.avg_domain_length);
  stats_.query_rates.push_back(metrics.query_rate);
  stats_.entropies.push_back(metrics.domain_entropy);

  // Maintain window size
  if (stats_.nxdomain_rates.size() > window_size_) {
    stats_.nxdomain_rates.pop_front();
  }
  if (stats_.avg_domain_lengths.size() > window_size_) {
    stats_.avg_domain_lengths.pop_front();
  }
  if (stats_.query_rates.size() > window_size_) {
    stats_.query_rates.pop_front();
  }
  if (stats_.entropies.size() > window_size_) {
    stats_.entropies.pop_front();
  }

  // Recalculate statistics
  update_rolling_stats();
}

AnomalyDetector::Statistics AnomalyDetector::get_statistics() const {
  Statistics stats;
  stats.mean_nxdomain_rate = stats_.mean_nxdomain;
  stats.stddev_nxdomain_rate = stats_.stddev_nxdomain;
  stats.mean_domain_length = stats_.mean_domain_length;
  stats.stddev_domain_length = stats_.stddev_domain_length;
  stats.samples_count = stats_.nxdomain_rates.size();
  return stats;
}

} // namespace inspector
} // namespace hamstring
