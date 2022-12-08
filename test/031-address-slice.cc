#include "catch.hpp"

#include "address.h"
#include "util/detect.h"

TEST_CASE("A statically-sized address slice is constructible by certian means") {
  STATIC_REQUIRE(std::is_constructible_v<champsim::address_slice<20,16>, champsim::address>);
  STATIC_REQUIRE(std::is_copy_constructible_v<champsim::address_slice<20,16>>);
  STATIC_REQUIRE(std::is_move_constructible_v<champsim::address_slice<20,16>>);
  STATIC_REQUIRE(std::is_copy_assignable_v<champsim::address_slice<20,16>>);
  STATIC_REQUIRE(std::is_move_assignable_v<champsim::address_slice<20,16>>);
  STATIC_REQUIRE(std::is_destructible_v<champsim::address_slice<20,16>>);
  STATIC_REQUIRE(std::is_swappable_v<champsim::address_slice<20,16>>);
  STATIC_REQUIRE(std::is_assignable_v<champsim::address_slice<20,16>, champsim::address_slice<20,16>>);
}

namespace {
  template <typename SliceA, typename SliceB>
    using cmp_slice = decltype( std::declval<SliceA>() == std::declval<SliceB>() );
}

TEST_CASE("Statically sized address slices can be compared if their begins and ends are the same") {
  STATIC_REQUIRE(champsim::is_detected_v<::cmp_slice, champsim::address_slice<16,8>, champsim::address_slice<16,8>>);

  champsim::address addr_a{0xdeadbeef};
  champsim::address addr_b{0xcafefeed};

  REQUIRE(champsim::address_slice<16,8>{addr_a} == champsim::address_slice<16,8>{addr_a});
  REQUIRE_FALSE(champsim::address_slice<16,8>{addr_a} != champsim::address_slice<16,8>{addr_a});
  REQUIRE(champsim::address_slice<16,8>{addr_a} != champsim::address_slice<16,8>{addr_b});
  REQUIRE_FALSE(champsim::address_slice<16,8>{addr_a} == champsim::address_slice<16,8>{addr_b});
}

TEST_CASE("Statically sized address slices can compare equal even if the underlying addresses differ outside the slice") {
  champsim::address addr_a{0xcafefeef};
  champsim::address addr_b{0xcafefeed};

  REQUIRE(champsim::address_slice<16,8>{addr_a} == champsim::address_slice<16,8>{addr_b});
  REQUIRE_FALSE(champsim::address_slice<16,8>{addr_a} != champsim::address_slice<16,8>{addr_b});
  REQUIRE(champsim::address_slice<20,12>{addr_a} == champsim::address_slice<20,12>{addr_b});
  REQUIRE_FALSE(champsim::address_slice<20,12>{addr_a} != champsim::address_slice<20,12>{addr_b});
  REQUIRE(champsim::address_slice<24,16>{addr_a} == champsim::address_slice<24,16>{addr_b});
  REQUIRE_FALSE(champsim::address_slice<24,16>{addr_a} != champsim::address_slice<24,16>{addr_b});
  REQUIRE(champsim::address_slice<28,20>{addr_a} == champsim::address_slice<28,20>{addr_b});
  REQUIRE_FALSE(champsim::address_slice<28,20>{addr_a} != champsim::address_slice<28,20>{addr_b});
  REQUIRE(champsim::address_slice<2,0>{addr_a} != champsim::address_slice<2,0>{addr_b});
  REQUIRE_FALSE(champsim::address_slice<2,0>{addr_a} == champsim::address_slice<2,0>{addr_b});
}

TEST_CASE("Statically-sized address slices cannot be compared if their begins and ends are not the same") {
  STATIC_REQUIRE_FALSE(champsim::is_detected_v<::cmp_slice, champsim::address_slice<20,16>, champsim::address_slice<10,6>>);
  STATIC_REQUIRE_FALSE(champsim::is_detected_v<::cmp_slice, champsim::address_slice<20,16>, champsim::address_slice<20,6>>);
  STATIC_REQUIRE_FALSE(champsim::is_detected_v<::cmp_slice, champsim::address_slice<20,16>, champsim::address_slice<30,16>>);
}

TEST_CASE("Statically-sized address sliced can be re-sliced") {
  champsim::address addr{0xabcdef89};

  REQUIRE(champsim::address_slice<20,8>{addr}.slice<8,0>() == champsim::address_slice<16,8>{addr});
}
