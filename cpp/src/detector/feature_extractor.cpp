#include "hamstring/detector/feature_extractor.hpp"
#include <algorithm>
#include <cctype>
#include <set>
#include <sstream>

namespace hamstring {
namespace detector {

std::vector<float> DomainFeatures::to_vector() const {
  std::vector<float> vec;
  vec.reserve(50); // Approximate size

  // Label statistics
  vec.push_back(static_cast<float>(label_length));
  vec.push_back(static_cast<float>(label_max));
  vec.push_back(static_cast<float>(label_average));

  // Character frequencies (a-z)
  for (char c = 'a'; c <= 'z'; ++c) {
    auto it = char_freq.find(c);
    vec.push_back(it != char_freq.end() ? static_cast<float>(it->second)
                                        : 0.0f);
  }

  // FQDN counts
  vec.push_back(static_cast<float>(fqdn_full_count));
  vec.push_back(static_cast<float>(fqdn_alpha_count));
  vec.push_back(static_cast<float>(fqdn_numeric_count));
  vec.push_back(static_cast<float>(fqdn_special_count));

  // Second level domain counts
  vec.push_back(static_cast<float>(secondleveldomain_full_count));
  vec.push_back(static_cast<float>(secondleveldomain_alpha_count));
  vec.push_back(static_cast<float>(secondleveldomain_numeric_count));
  vec.push_back(static_cast<float>(secondleveldomain_special_count));

  // Third level domain counts
  vec.push_back(static_cast<float>(thirdleveldomain_full_count));
  vec.push_back(static_cast<float>(thirdleveldomain_alpha_count));
  vec.push_back(static_cast<float>(thirdleveldomain_numeric_count));
  vec.push_back(static_cast<float>(thirdleveldomain_special_count));

  // Entropy
  vec.push_back(static_cast<float>(fqdn_entropy));
  vec.push_back(static_cast<float>(secondleveldomain_entropy));
  vec.push_back(static_cast<float>(thirdleveldomain_entropy));

  return vec;
}

std::vector<std::string> DomainFeatures::get_feature_names() {
  std::vector<std::string> names;

  names.push_back("label_length");
  names.push_back("label_max");
  names.push_back("label_average");

  for (char c = 'a'; c <= 'z'; ++c) {
    names.push_back(std::string("freq_") + c);
  }

  names.push_back("fqdn_full_count");
  names.push_back("fqdn_alpha_count");
  names.push_back("fqdn_numeric_count");
  names.push_back("fqdn_special_count");

  names.push_back("secondleveldomain_full_count");
  names.push_back("secondleveldomain_alpha_count");
  names.push_back("secondleveldomain_numeric_count");
  names.push_back("secondleveldomain_special_count");

  names.push_back("thirdleveldomain_full_count");
  names.push_back("thirdleveldomain_alpha_count");
  names.push_back("thirdleveldomain_numeric_count");
  names.push_back("thirdleveldomain_special_count");

  names.push_back("fqdn_entropy");
  names.push_back("secondleveldomain_entropy");
  names.push_back("thirdleveldomain_entropy");

  return names;
}

DomainFeatures FeatureExtractor::extract(const std::string &domain) const {
  DomainFeatures features;

  // Label statistics
  features.label_length = count_labels(domain);
  features.label_max = get_max_label_length(domain);
  features.label_average = get_average_label_length(domain);

  // Character frequency
  features.char_freq = calculate_char_frequency(domain);

  // Extract domain levels
  std::string fqdn = extract_fqdn(domain);
  std::string sld = extract_second_level_domain(domain);
  std::string tld_domain = extract_third_level_domain(domain);

  // FQDN counts
  if (!fqdn.empty()) {
    features.fqdn_full_count = 1.0;
    features.fqdn_alpha_count = calculate_alpha_ratio(fqdn);
    features.fqdn_numeric_count = calculate_numeric_ratio(fqdn);
    features.fqdn_special_count = calculate_special_ratio(fqdn);
    features.fqdn_entropy = calculate_entropy(fqdn);
  }

  // Second level domain counts
  if (!sld.empty()) {
    features.secondleveldomain_full_count = 1.0;
    features.secondleveldomain_alpha_count = calculate_alpha_ratio(sld);
    features.secondleveldomain_numeric_count = calculate_numeric_ratio(sld);
    features.secondleveldomain_special_count = calculate_special_ratio(sld);
    features.secondleveldomain_entropy = calculate_entropy(sld);
  }

  // Third level domain counts
  if (!tld_domain.empty()) {
    features.thirdleveldomain_full_count = 1.0;
    features.thirdleveldomain_alpha_count = calculate_alpha_ratio(tld_domain);
    features.thirdleveldomain_numeric_count =
        calculate_numeric_ratio(tld_domain);
    features.thirdleveldomain_special_count =
        calculate_special_ratio(tld_domain);
    features.thirdleveldomain_entropy = calculate_entropy(tld_domain);
  }

  return features;
}

int FeatureExtractor::count_labels(const std::string &domain) const {
  if (domain.empty())
    return 0;

  int count = 1;
  for (char c : domain) {
    if (c == '.')
      count++;
  }
  return count;
}

int FeatureExtractor::get_max_label_length(const std::string &domain) const {
  std::istringstream iss(domain);
  std::string label;
  int max_len = 0;

  while (std::getline(iss, label, '.')) {
    max_len = std::max(max_len, static_cast<int>(label.length()));
  }

  return max_len;
}

double
FeatureExtractor::get_average_label_length(const std::string &domain) const {
  // Remove dots and calculate average
  std::string without_dots;
  for (char c : domain) {
    if (c != '.')
      without_dots += c;
  }

  return static_cast<double>(without_dots.length());
}

std::map<char, double>
FeatureExtractor::calculate_char_frequency(const std::string &domain) const {
  std::map<char, double> freq;
  std::string lower_domain;

  // Convert to lowercase
  for (char c : domain) {
    lower_domain += std::tolower(c);
  }

  // Count occurrences
  for (char c = 'a'; c <= 'z'; ++c) {
    int count = std::count(lower_domain.begin(), lower_domain.end(), c);
    freq[c] =
        domain.empty() ? 0.0 : static_cast<double>(count) / domain.length();
  }

  return freq;
}

double FeatureExtractor::calculate_alpha_ratio(const std::string &text) const {
  if (text.empty())
    return 0.0;

  int alpha_count = 0;
  for (char c : text) {
    if (std::isalpha(c))
      alpha_count++;
  }

  return static_cast<double>(alpha_count) / text.length();
}

double
FeatureExtractor::calculate_numeric_ratio(const std::string &text) const {
  if (text.empty())
    return 0.0;

  int numeric_count = 0;
  for (char c : text) {
    if (std::isdigit(c))
      numeric_count++;
  }

  return static_cast<double>(numeric_count) / text.length();
}

double
FeatureExtractor::calculate_special_ratio(const std::string &text) const {
  if (text.empty())
    return 0.0;

  int special_count = 0;
  for (char c : text) {
    if (!std::isalnum(c) && !std::isspace(c))
      special_count++;
  }

  return static_cast<double>(special_count) / text.length();
}

double FeatureExtractor::calculate_entropy(const std::string &text) const {
  if (text.empty())
    return 0.0;

  // Calculate character probabilities
  std::map<char, int> char_counts;
  for (char c : text) {
    char_counts[c]++;
  }

  // Calculate entropy using Shannon formula
  double entropy = 0.0;
  double log2_base = std::log(2.0);

  for (const auto &[ch, count] : char_counts) {
    double prob = static_cast<double>(count) / text.length();
    entropy += -prob * (std::log(prob) / log2_base);
  }

  return entropy;
}

std::string FeatureExtractor::extract_fqdn(const std::string &domain) const {
  return domain;
}

std::string
FeatureExtractor::extract_second_level_domain(const std::string &domain) const {
  // Extract second level domain (e.g., "example.com" from "www.example.com")
  std::istringstream iss(domain);
  std::vector<std::string> labels;
  std::string label;

  while (std::getline(iss, label, '.')) {
    labels.push_back(label);
  }

  if (labels.size() >= 2) {
    return labels[labels.size() - 2] + "." + labels[labels.size() - 1];
  }

  return domain;
}

std::string
FeatureExtractor::extract_third_level_domain(const std::string &domain) const {
  // Extract third level domain (e.g., "www" from "www.example.com")
  std::istringstream iss(domain);
  std::vector<std::string> labels;
  std::string label;

  while (std::getline(iss, label, '.')) {
    labels.push_back(label);
  }

  if (labels.size() >= 3) {
    return labels[0];
  }

  return "";
}

} // namespace detector
} // namespace hamstring
