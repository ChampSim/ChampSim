#include <catch.hpp>

#include "environment.h"
#include "cache.h"
#include "channel.h"
#include "defaults.hpp"
#include <vector>

TEST_CASE("Module Build Order")
{
  //given a sequence of cache builders, when we create the environment, the order should be preserved
  GIVEN("A sequence of cache builders") {

    auto caches{
        champsim::configured::build<CACHE>(
        champsim::cache_builder{ champsim::defaults::default_llc }
            .name("cache0"),
        champsim::cache_builder{ champsim::defaults::default_dtlb }
            .name("cache1"),
        champsim::cache_builder{ champsim::defaults::default_itlb }
            .name("cache2"),
        champsim::cache_builder{ champsim::defaults::default_l1d }
            .name("cache3")
        )
    };

    std::vector<std::string> expected_names{"cache0", "cache1", "cache2", "cache3"};

    //require the names match the order of the builders
    THEN("The caches are built and returned in the same order as the builders") {

        //grab the names of the built caches
        std::vector<std::string> cache_names{};
        for (const auto& cache : caches)
            cache_names.push_back(cache.NAME);

        REQUIRE_THAT(cache_names, Catch::Matchers::Equals(expected_names));
    }
  };
}
