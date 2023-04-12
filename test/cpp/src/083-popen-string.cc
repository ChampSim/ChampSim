#include <catch.hpp>

#include "tracereader.h"

TEST_CASE("Gzip-compressed files are detected in popen strings")
{
  std::string fname{"test.gz"};
  auto test_string = champsim::get_fptr_cmd(fname);
  REQUIRE_THAT(test_string, Catch::Matchers::ContainsSubstring("gzip"));
}

TEST_CASE("Xz-compressed files are detected in popen strings")
{
  std::string fname{"test.xz"};
  auto test_string = champsim::get_fptr_cmd(fname);
  REQUIRE_THAT(test_string, Catch::Matchers::ContainsSubstring("xz"));
}

TEST_CASE("Non-compressed files are detected in popen strings")
{
  std::string fname{"test.champsimtrace"};
  auto test_string = champsim::get_fptr_cmd(fname);
  REQUIRE_THAT(test_string, !Catch::Matchers::ContainsSubstring("xz") && !Catch::Matchers::ContainsSubstring("gzip"));
}

TEST_CASE("Remote files can be retrieved")
{
  std::string fname{"http://example.com/files/test.champsimtrace"};
  auto test_string = champsim::get_fptr_cmd(fname);
  REQUIRE_THAT(test_string, Catch::Matchers::ContainsSubstring("wget"));
}

TEST_CASE("Remote gzip-compressed files can be retrieved")
{
  std::string fname{"http://example.com/files/test.gz"};
  auto test_string = champsim::get_fptr_cmd(fname);
  REQUIRE_THAT(test_string, Catch::Matchers::ContainsSubstring("wget") && Catch::Matchers::ContainsSubstring("gzip"));
}

TEST_CASE("Remote xz-compressed files can be retrieved")
{
  std::string fname{"http://example.com/files/test.xz"};
  auto test_string = champsim::get_fptr_cmd(fname);
  REQUIRE_THAT(test_string, Catch::Matchers::ContainsSubstring("wget") && Catch::Matchers::ContainsSubstring("xz"));
}
