#include <catch.hpp>
#include <vector>
#include "msl/stat_methods.h"

TEST_CASE("A categorizer exhausts the full category space evenly") {
  std::size_t sample_rate = GENERATE(4, 8, 16, 32,64);
  auto full_space_factor = 4;
  champsim::msl::categorizer<long> cat(sample_rate);
  std::vector<std::size_t> seen(static_cast<std::size_t>(sample_rate), 0);

  for(std::size_t i = 0; i < sample_rate * full_space_factor; i++) {
    auto sample_cat = cat.get_sample_category(i);
    seen.at(sample_cat)++;
    REQUIRE(sample_cat < sample_rate);
  }
  REQUIRE(seen == std::vector<std::size_t>(static_cast<std::size_t>(sample_rate), full_space_factor));
}

TEST_CASE("Recommended sample rate for set dueling produces a reasonable sample rate") {
  std::size_t num_sets = GENERATE(1024,256,64,8);

  auto sample_rate = champsim::msl::get_sample_rate(num_sets);
  
  REQUIRE(sample_rate < num_sets); //more than 1 sample per set
  REQUIRE(sample_rate > 1); //not all sets are sampled
}

TEST_CASE("Number of sets in recommended sample rate for set dueling produces a reasonable number of sets") {
  std::size_t num_sets = GENERATE(1024,256,64,8);

  std::size_t samples = champsim::msl::get_num_samples(num_sets);
  
  REQUIRE(samples < num_sets); //not all sets are sampled
  REQUIRE(samples > 1); //at least 2 sets are sampled
}

TEST_CASE("Sampled sets and sample rate are consistent") {
  std::size_t num_sets = GENERATE(1024,256,64,8);

  std::size_t samples = champsim::msl::get_num_samples(num_sets);
  std::size_t sample_rate = champsim::msl::get_sample_rate(num_sets);

  REQUIRE(num_sets / sample_rate == samples); //sample rate and number of sampled sets are consistent
}


TEST_CASE("Dueling counter predicts the counter value for non-sampled sets") {
    auto dsc = champsim::msl::dscounter<long, 4>(32); //sample rate of 32, so 1 in 32 sets are sampled
    champsim::msl::categorizer<long> cat_sampler(32); //should give same category as dscounter
    bool pos_or_neg = GENERATE(true, false); //whether to reinforce good or bad for categories

    //find cat 0
    long cat0_candidate = 0;
    while(cat_sampler.get_sample_category(cat0_candidate) != 0) {
        cat0_candidate++;
        if(cat0_candidate > 10000) {
            FAIL("Could not find category 0 candidate");
        }
    }
    //find cat 1
    long cat1_candidate = 0;
    while(cat_sampler.get_sample_category(cat1_candidate) != 1) {
        cat1_candidate++;
        if(cat1_candidate > 10000) {
            FAIL("Could not find category 1 candidate");
        }
    }
    
    //find cat 2
    long cat2_candidate = 0;
    while(cat_sampler.get_sample_category(cat2_candidate) != 2) {
        cat2_candidate++;
        if(cat2_candidate > 10000) {
            FAIL("Could not find category 2 candidate");
        }
    }
    
    //reinforce cat 0
    for(int i = 0; i < 8; i++) {
        if(pos_or_neg) {
            dsc.update_good(cat0_candidate);
        } else {
            dsc.update_bad(cat0_candidate);
        }
    }
    REQUIRE(dsc.decide(cat2_candidate) == pos_or_neg); //should be more likely to predict true for cat 2 candidate

    //reinforce cat 1
    for(int i = 0; i < 8; i++) {
        if(pos_or_neg) {
            dsc.update_good(cat1_candidate);
        } else {
            dsc.update_bad(cat1_candidate);
        }
    }
    REQUIRE(dsc.decide(cat2_candidate) == !pos_or_neg); //should be more likely to predict false for cat 2 candidate
}

TEST_CASE("Dueling counter predicts static values for sampled sets") {
    auto dsc = champsim::msl::dscounter<long, 4>(32); //sample rate of 32, so 1 in 32 sets are sampled
    champsim::msl::categorizer<long> cat_sampler(32); //should give same category as dscounter
    bool pos_or_neg = GENERATE(true, false); //whether to reinforce good or bad for categories
    //find cat 0
    long cat0_candidate = 0;
    while(cat_sampler.get_sample_category(cat0_candidate) != 0) {
        cat0_candidate++;
        if(cat0_candidate > 10000) {
            FAIL("Could not find category 0 candidate");
        }
    }
    //find cat 1
    long cat1_candidate = 0;
    while(cat_sampler.get_sample_category(cat1_candidate) != 1) {
        cat1_candidate++;
        if(cat1_candidate > 10000) {
            FAIL("Could not find category 1 candidate");
        }
    }
  
    //reinforce cat 0
    for(int i = 0; i < 8; i++) {
        if(pos_or_neg) {
            dsc.update_good(cat0_candidate);
        } else {
            dsc.update_bad(cat0_candidate);
        }
    }
    REQUIRE(dsc.decide(cat0_candidate) == true); //should always predict true for cat 0 candidate
    REQUIRE(dsc.decide(cat1_candidate) == false); //should always predict false for cat 1 candidate

    //reinforce cat 1
    for(int i = 0; i < 8; i++) {
        if(pos_or_neg) {
            dsc.update_good(cat1_candidate);
        } else {
            dsc.update_bad(cat1_candidate);
        }
    }
    REQUIRE(dsc.decide(cat1_candidate) == false); //should always predict false for cat 1 candidate
    REQUIRE(dsc.decide(cat0_candidate) == true); //should always predict true for cat 0 candidate
}


