#include "hamstring/base/utils.hpp"
#include <algorithm>
#include <arpa/inet.h>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <openssl/sha.h>
#include <random>
#include <regex>
#include <sstream>

namespace hamstring {
namespace base {
namespace utils {

std::string generate_uuid() {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, 15);
  std::uniform_int_distribution<> dis2(8, 11);

  std::stringstream ss;
  ss << std::hex;

  for (int i = 0; i < 8; i++) {
    ss << dis(gen);
  }
  ss << "-";

  for (int i = 0; i < 4; i++) {
    ss << dis(gen);
  }
  ss << "-4";

  for (int i = 0; i < 3; i++) {
    ss << dis(gen);
  }
  ss << "-";

  ss << dis2(gen);
  for (int i = 0; i < 3; i++) {
    ss << dis(gen);
  }
  ss << "-";

  for (int i = 0; i < 12; i++) {
    ss << dis(gen);
  }

  return ss.str();
}

bool is_valid_ipv4(const std::string &ip) {
  struct sockaddr_in sa;
  return inet_pton(AF_INET, ip.c_str(), &(sa.sin_addr)) == 1;
}

bool is_valid_ipv6(const std::string &ip) {
  struct sockaddr_in6 sa;
  return inet_pton(AF_INET6, ip.c_str(), &(sa.sin6_addr)) == 1;
}

std::string get_subnet_id(const std::string &ip, int prefix_length) {
  if (is_valid_ipv4(ip)) {
    struct in_addr addr;
    inet_pton(AF_INET, ip.c_str(), &addr);

    uint32_t mask = (0xFFFFFFFF << (32 - prefix_length)) & 0xFFFFFFFF;
    uint32_t subnet = ntohl(addr.s_addr) & mask;
    addr.s_addr = htonl(subnet);

    char subnet_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr, subnet_str, INET_ADDRSTRLEN);
    return std::string(subnet_str) + "/" + std::to_string(prefix_length);
  } else if (is_valid_ipv6(ip)) {
    struct in6_addr addr;
    inet_pton(AF_INET6, ip.c_str(), &addr);

    int bytes_to_mask = prefix_length / 8;
    int bits_to_mask = prefix_length % 8;

    for (int i = bytes_to_mask; i < 16; i++) {
      addr.s6_addr[i] = 0;
    }

    if (bits_to_mask > 0 && bytes_to_mask < 16) {
      uint8_t mask = (0xFF << (8 - bits_to_mask)) & 0xFF;
      addr.s6_addr[bytes_to_mask] &= mask;
    }

    char subnet_str[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, &addr, subnet_str, INET6_ADDRSTRLEN);
    return std::string(subnet_str) + "/" + std::to_string(prefix_length);
  }

  return "";
}

std::string format_timestamp(const std::chrono::system_clock::time_point &tp,
                             const std::string &format) {
  auto time_t = std::chrono::system_clock::to_time_t(tp);
  std::tm tm = *std::gmtime(&time_t);

  std::stringstream ss;
  ss << std::put_time(&tm, format.c_str());
  return ss.str();
}

std::chrono::system_clock::time_point
parse_timestamp(const std::string &ts_str, const std::string &format) {
  std::tm tm = {};
  std::istringstream ss(ts_str);
  ss >> std::get_time(&tm, format.c_str());

  auto time_t = std::mktime(&tm);
  return std::chrono::system_clock::from_time_t(time_t);
}

int64_t timestamp_to_ms(const std::chrono::system_clock::time_point &tp) {
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      tp.time_since_epoch());
  return ms.count();
}

std::chrono::system_clock::time_point ms_to_timestamp(int64_t ms) {
  return std::chrono::system_clock::time_point(std::chrono::milliseconds(ms));
}

std::vector<std::string> split(const std::string &str, char delimiter) {
  std::vector<std::string> tokens;
  std::string token;
  std::istringstream token_stream(str);

  while (std::getline(token_stream, token, delimiter)) {
    tokens.push_back(token);
  }

  return tokens;
}

std::string join(const std::vector<std::string> &vec,
                 const std::string &delimiter) {
  if (vec.empty())
    return "";

  std::stringstream ss;
  ss << vec[0];

  for (size_t i = 1; i < vec.size(); i++) {
    ss << delimiter << vec[i];
  }

  return ss.str();
}

std::string trim(const std::string &str) {
  auto start = std::find_if_not(str.begin(), str.end(), [](unsigned char ch) {
    return std::isspace(ch);
  });

  auto end = std::find_if_not(str.rbegin(), str.rend(), [](unsigned char ch) {
               return std::isspace(ch);
             }).base();

  return (start < end) ? std::string(start, end) : std::string();
}

std::string to_lower(const std::string &str) {
  std::string result = str;
  std::transform(result.begin(), result.end(), result.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return result;
}

std::string to_upper(const std::string &str) {
  std::string result = str;
  std::transform(result.begin(), result.end(), result.begin(),
                 [](unsigned char c) { return std::toupper(c); });
  return result;
}

std::string extract_fqdn(const std::string &domain) { return domain; }

std::string extract_second_level_domain(const std::string &domain) {
  auto labels = split(domain, '.');

  if (labels.size() >= 2) {
    return labels[labels.size() - 2] + "." + labels[labels.size() - 1];
  }

  return domain;
}

std::string extract_third_level_domain(const std::string &domain) {
  auto labels = split(domain, '.');

  if (labels.size() >= 3) {
    return labels[0];
  }

  return "";
}

std::optional<std::string> extract_tld(const std::string &domain) {
  auto labels = split(domain, '.');

  if (!labels.empty()) {
    return labels.back();
  }

  return std::nullopt;
}

std::string sha256_file(const std::string &filepath) {
  std::ifstream file(filepath, std::ios::binary);
  if (!file) {
    return "";
  }

  SHA256_CTX sha256;
  SHA256_Init(&sha256);

  const size_t buffer_size = 8192;
  char buffer[buffer_size];

  while (file.read(buffer, buffer_size) || file.gcount() > 0) {
    SHA256_Update(&sha256, buffer, file.gcount());
  }

  unsigned char hash[SHA256_DIGEST_LENGTH];
  SHA256_Final(hash, &sha256);

  std::stringstream ss;
  for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
    ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
  }

  return ss.str();
}

std::string sha256_string(const std::string &data) {
  unsigned char hash[SHA256_DIGEST_LENGTH];
  SHA256_CTX sha256;
  SHA256_Init(&sha256);
  SHA256_Update(&sha256, data.c_str(), data.size());
  SHA256_Final(hash, &sha256);

  std::stringstream ss;
  for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
    ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
  }

  return ss.str();
}

} // namespace utils
} // namespace base
} // namespace hamstring
