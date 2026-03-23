#include "hamstring/detector/feature_extractor.hpp"
#include <cmath>
#include <gtest/gtest.h>

using namespace hamstring::detector;

class FeatureExtractorTest : public ::testing::Test {
protected:
  FeatureExtractor extractor;
};

TEST_F(FeatureExtractorTest, BasicDomainExtraction) {
  std::string domain = "example.com";
  auto features = extractor.extract(domain);

  // Check label count
  EXPECT_EQ(features.label_length, 2);

  // Check that features are populated
  EXPECT_GT(features.fqdn_entropy, 0.0);
  EXPECT_GT(features.secondleveldomain_entropy, 0.0);
}

TEST_F(FeatureExtractorTest, SubdomainExtraction) {
  std::string domain = "www.example.com";
  auto features = extractor.extract(domain);

  // Check label count
  EXPECT_EQ(features.label_length, 3);

  // Check third level domain is extracted
  EXPECT_GT(features.thirdleveldomain_entropy, 0.0);
}

TEST_F(FeatureExtractorTest, CharacterFrequency) {
  std::string domain = "aaa.com";
  auto features = extractor.extract(domain);

  // 'a' appears 3 times out of 7 characters
  EXPECT_NEAR(features.char_freq['a'], 3.0 / 7.0, 0.01);

  // 'c' appears 1 time out of 7 characters
  EXPECT_NEAR(features.char_freq['c'], 1.0 / 7.0, 0.01);
}

TEST_F(FeatureExtractorTest, AlphaNumericRatios) {
  std::string domain = "test123.com";
  auto features = extractor.extract(domain);

  // Should have both alpha and numeric characters
  EXPECT_GT(features.fqdn_alpha_count, 0.0);
  EXPECT_GT(features.fqdn_numeric_count, 0.0);

  // Alpha + numeric should be close to 1.0 (only dot is special)
  EXPECT_NEAR(features.fqdn_alpha_count + features.fqdn_numeric_count,
              1.0 - features.fqdn_special_count, 0.01);
}

TEST_F(FeatureExtractorTest, EntropyCalculation) {
  // Domain with all same characters should have low entropy
  std::string low_entropy = "aaaa.com";
  auto features_low = extractor.extract(low_entropy);

  // Domain with varied characters should have higher entropy
  std::string high_entropy = "abcdefgh.com";
  auto features_high = extractor.extract(high_entropy);

  EXPECT_LT(features_low.fqdn_entropy, features_high.fqdn_entropy);
}

TEST_F(FeatureExtractorTest, DGALikeDomain) {
  // Typical DGA domain: random characters, high entropy
  std::string dga_domain = "xjk3n2m9pq.com";
  auto features = extractor.extract(dga_domain);

  // DGA domains typically have:
  // - High entropy
  // - Mix of alpha and numeric
  EXPECT_GT(features.fqdn_entropy, 2.0);
  EXPECT_GT(features.fqdn_alpha_count, 0.5);
}

TEST_F(FeatureExtractorTest, FeatureVectorSize) {
  std::string domain = "test.example.com";
  auto features = extractor.extract(domain);
  auto vec = features.to_vector();

  // Should have 44 features:
  // 3 (label stats) + 26 (char freq) + 12 (domain level counts) + 3 (entropy)
  EXPECT_EQ(vec.size(), 44);
}

TEST_F(FeatureExtractorTest, FeatureNames) {
  auto names = DomainFeatures::get_feature_names();

  // Should match vector size
  EXPECT_EQ(names.size(), 44);

  // Check some expected names
  EXPECT_EQ(names[0], "label_length");
  EXPECT_EQ(names[1], "label_max");
  EXPECT_EQ(names[2], "label_average");
  EXPECT_EQ(names[3], "freq_a");
}

TEST_F(FeatureExtractorTest, EmptyDomain) {
  std::string domain = "";
  auto features = extractor.extract(domain);

  // Empty domain should have zero features
  EXPECT_EQ(features.label_length, 0);
  EXPECT_EQ(features.fqdn_entropy, 0.0);
}

TEST_F(FeatureExtractorTest, SingleLabel) {
  std::string domain = "localhost";
  auto features = extractor.extract(domain);

  // Single label domain
  EXPECT_EQ(features.label_length, 1);

  // Should have FQDN features but not second/third level
  EXPECT_GT(features.fqdn_entropy, 0.0);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
