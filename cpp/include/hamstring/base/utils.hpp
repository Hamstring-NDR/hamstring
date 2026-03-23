#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace hamstring {
namespace base {
namespace utils {

// UUID generation
std::string generate_uuid();

// IP address utilities
bool is_valid_ipv4(const std::string &ip);
bool is_valid_ipv6(const std::string &ip);
std::string get_subnet_id(const std::string &ip, int prefix_length);

// Time utilities
std::string format_timestamp(const std::chrono::system_clock::time_point &tp,
                             const std::string &format = "%Y-%m-%dT%H:%M:%S");
std::chrono::system_clock::time_point
parse_timestamp(const std::string &ts_str,
                const std::string &format = "%Y-%m-%dT%H:%M:%S");
int64_t timestamp_to_ms(const std::chrono::system_clock::time_point &tp);
std::chrono::system_clock::time_point ms_to_timestamp(int64_t ms);

// String utilities
std::vector<std::string> split(const std::string &str, char delimiter);
std::string join(const std::vector<std::string> &vec,
                 const std::string &delimiter);
std::string trim(const std::string &str);
std::string to_lower(const std::string &str);
std::string to_upper(const std::string &str);

// Domain name utilities
std::string extract_fqdn(const std::string &domain);
std::string extract_second_level_domain(const std::string &domain);
std::string extract_third_level_domain(const std::string &domain);
std::optional<std::string> extract_tld(const std::string &domain);

// Hash utilities
std::string sha256_file(const std::string &filepath);
std::string sha256_string(const std::string &data);

} // namespace utils
} // namespace base
} // namespace hamstring
