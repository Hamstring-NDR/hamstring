#pragma once

#include <cmath>
#include <map>
#include <string>
#include <vector>

namespace hamstring {
namespace detector {

// Feature vector for a domain name
struct DomainFeatures {
  // Label statistics
  int label_length = 0;
  int label_max = 0;
  double label_average = 0.0;

  // Character frequency (a-z)
  std::map<char, double> char_freq;

  // Domain level counts
  double fqdn_full_count = 0.0;
  double fqdn_alpha_count = 0.0;
  double fqdn_numeric_count = 0.0;
  double fqdn_special_count = 0.0;

  double secondleveldomain_full_count = 0.0;
  double secondleveldomain_alpha_count = 0.0;
  double secondleveldomain_numeric_count = 0.0;
  double secondleveldomain_special_count = 0.0;

  double thirdleveldomain_full_count = 0.0;
  double thirdleveldomain_alpha_count = 0.0;
  double thirdleveldomain_numeric_count = 0.0;
  double thirdleveldomain_special_count = 0.0;

  // Entropy
  double fqdn_entropy = 0.0;
  double secondleveldomain_entropy = 0.0;
  double thirdleveldomain_entropy = 0.0;

  // Convert to vector for ML model input
  std::vector<float> to_vector() const;

  // Get feature names (for debugging/logging)
  static std::vector<std::string> get_feature_names();
};

// Feature extractor matching Python implementation
class FeatureExtractor {
public:
  FeatureExtractor() = default;

  // Extract features from a domain name
  DomainFeatures extract(const std::string &domain) const;

private:
  // Helper methods matching Python implementation
  int count_labels(const std::string &domain) const;
  int get_max_label_length(const std::string &domain) const;
  double get_average_label_length(const std::string &domain) const;

  std::map<char, double>
  calculate_char_frequency(const std::string &domain) const;

  double calculate_alpha_ratio(const std::string &text) const;
  double calculate_numeric_ratio(const std::string &text) const;
  double calculate_special_ratio(const std::string &text) const;

  double calculate_entropy(const std::string &text) const;

  std::string extract_fqdn(const std::string &domain) const;
  std::string extract_second_level_domain(const std::string &domain) const;
  std::string extract_third_level_domain(const std::string &domain) const;
};

} // namespace detector
} // namespace hamstring
