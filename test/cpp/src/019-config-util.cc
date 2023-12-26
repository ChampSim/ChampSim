#include <catch.hpp>

#include "config.h"

TEST_CASE("int_or_prefixed_size() passes through integers") {
  CHECK(champsim::config::int_or_prefixed_size(1l) == 1l);
  CHECK(champsim::config::int_or_prefixed_size(10l) == 10l);
  CHECK(champsim::config::int_or_prefixed_size(100l) == 100l);
}

TEST_CASE("int_or_prefixed_size() parses strings") {
  CHECK(champsim::config::int_or_prefixed_size("1") == 1l);
  CHECK(champsim::config::int_or_prefixed_size("10") == 10l);
  CHECK(champsim::config::int_or_prefixed_size("100") == 100l);

  CHECK(champsim::config::int_or_prefixed_size("1B") == 1l);
  CHECK(champsim::config::int_or_prefixed_size("10B") == 10l);
  CHECK(champsim::config::int_or_prefixed_size("100B") == 100l);

  CHECK(champsim::config::int_or_prefixed_size("1k") == 1l*1024);
  CHECK(champsim::config::int_or_prefixed_size("10k") == 10l*1024);
  CHECK(champsim::config::int_or_prefixed_size("100k") == 100l*1024);

  CHECK(champsim::config::int_or_prefixed_size("1kB") == 1l*1024);
  CHECK(champsim::config::int_or_prefixed_size("10kB") == 10l*1024);
  CHECK(champsim::config::int_or_prefixed_size("100kB") == 100l*1024);

  CHECK(champsim::config::int_or_prefixed_size("1kiB") == 1l*1024);
  CHECK(champsim::config::int_or_prefixed_size("10kiB") == 10l*1024);
  CHECK(champsim::config::int_or_prefixed_size("100kiB") == 100l*1024);

  CHECK(champsim::config::int_or_prefixed_size("1M") == 1l*1024*1024);
  CHECK(champsim::config::int_or_prefixed_size("10M") == 10l*1024*1024);
  CHECK(champsim::config::int_or_prefixed_size("100M") == 100l*1024*1024);

  CHECK(champsim::config::int_or_prefixed_size("1MB") == 1l*1024*1024);
  CHECK(champsim::config::int_or_prefixed_size("10MB") == 10l*1024*1024);
  CHECK(champsim::config::int_or_prefixed_size("100MB") == 100l*1024*1024);

  CHECK(champsim::config::int_or_prefixed_size("1MiB") == 1l*1024*1024);
  CHECK(champsim::config::int_or_prefixed_size("10MiB") == 10l*1024*1024);
  CHECK(champsim::config::int_or_prefixed_size("100MiB") == 100l*1024*1024);

  CHECK(champsim::config::int_or_prefixed_size("1G") == 1l*1024*1024*1024);
  CHECK(champsim::config::int_or_prefixed_size("10G") == 10l*1024*1024*1024);
  CHECK(champsim::config::int_or_prefixed_size("100G") == 100l*1024*1024*1024);

  CHECK(champsim::config::int_or_prefixed_size("1GB") == 1l*1024*1024*1024);
  CHECK(champsim::config::int_or_prefixed_size("10GB") == 10l*1024*1024*1024);
  CHECK(champsim::config::int_or_prefixed_size("100GB") == 100l*1024*1024*1024);

  CHECK(champsim::config::int_or_prefixed_size("1GiB") == 1l*1024*1024*1024);
  CHECK(champsim::config::int_or_prefixed_size("10GiB") == 10l*1024*1024*1024);
  CHECK(champsim::config::int_or_prefixed_size("100GiB") == 100l*1024*1024*1024);

  CHECK(champsim::config::int_or_prefixed_size("1T") == 1l*1024*1024*1024*1024);
  CHECK(champsim::config::int_or_prefixed_size("10T") == 10l*1024*1024*1024*1024);
  CHECK(champsim::config::int_or_prefixed_size("100T") == 100l*1024*1024*1024*1024);

  CHECK(champsim::config::int_or_prefixed_size("1TB") == 1l*1024*1024*1024*1024);
  CHECK(champsim::config::int_or_prefixed_size("10TB") == 10l*1024*1024*1024*1024);
  CHECK(champsim::config::int_or_prefixed_size("100TB") == 100l*1024*1024*1024*1024);

  CHECK(champsim::config::int_or_prefixed_size("1TiB") == 1l*1024*1024*1024*1024);
  CHECK(champsim::config::int_or_prefixed_size("10TiB") == 10l*1024*1024*1024*1024);
  CHECK(champsim::config::int_or_prefixed_size("100TiB") == 100l*1024*1024*1024*1024);
}
