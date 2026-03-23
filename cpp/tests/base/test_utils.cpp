#include "hamstring/base/utils.hpp"
#include <gtest/gtest.h>

using namespace hamstring::base::utils;

TEST(UtilsTest, UUIDGeneration) {
  auto uuid1 = generate_uuid();
  auto uuid2 = generate_uuid();

  // UUIDs should be different
  EXPECT_NE(uuid1, uuid2);

  // UUID should have correct format (36 characters with dashes)
  EXPECT_EQ(uuid1.length(), 36);
}

TEST(UtilsTest, IPv4Validation) {
  EXPECT_TRUE(is_valid_ipv4("192.168.1.1"));
  EXPECT_TRUE(is_valid_ipv4("10.0.0.1"));
  EXPECT_FALSE(is_valid_ipv4("256.1.1.1"));
  EXPECT_FALSE(is_valid_ipv4("not.an.ip.address"));
  EXPECT_FALSE(is_valid_ipv4("::1"));
}

TEST(UtilsTest, IPv6Validation) {
  EXPECT_TRUE(is_valid_ipv6("::1"));
  EXPECT_TRUE(is_valid_ipv6("2001:db8::1"));
  EXPECT_FALSE(is_valid_ipv6("192.168.1.1"));
  EXPECT_FALSE(is_valid_ipv6("not:valid:ipv6"));
}

TEST(UtilsTest, StringSplit) {
  auto parts = split("a,b,c", ',');
  ASSERT_EQ(parts.size(), 3);
  EXPECT_EQ(parts[0], "a");
  EXPECT_EQ(parts[1], "b");
  EXPECT_EQ(parts[2], "c");
}

TEST(UtilsTest, StringJoin) {
  std::vector<std::string> parts = {"a", "b", "c"};
  auto joined = join(parts, ",");
  EXPECT_EQ(joined, "a,b,c");
}

TEST(UtilsTest, StringTrim) {
  EXPECT_EQ(trim("  hello  "), "hello");
  EXPECT_EQ(trim("hello"), "hello");
  EXPECT_EQ(trim("   "), "");
}

TEST(UtilsTest, DomainExtraction) {
  EXPECT_EQ(extract_fqdn("www.example.com"), "www.example.com");
  EXPECT_EQ(extract_second_level_domain("www.example.com"), "example.com");
  EXPECT_EQ(extract_third_level_domain("www.example.com"), "www");
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
