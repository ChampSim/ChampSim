#include <catch.hpp>

#include "../replacement/srrip/srrip.h"

TEST_CASE("SRRIP matches the performance in the published work") {
  /*
   * Aamer Jaleel, Kevin B. Theobald, Simon C. Steely, and Joel Emer. 2010. High performance cache replacement using re-reference interval prediction (RRIP). In Proceedings of the 37th annual international symposium on Computer architecture (ISCA '10). Association for Computing Machinery, New York, NY, USA, 60–71. https://doi.org/10.1145/1815961.1815971
   */

  srrip_set_helper uut{4};

  uut.update(0,false); // a1
  uut.update(1,false); // a2
  uut.update(1,true);  // a2
  uut.update(0,true);  // a1
  uut.update(2,false); // b1
  uut.update(3,false); // b2

  auto victim_b3 = uut.victim();
  REQUIRE(victim_b3 == 2);
  uut.update(victim_b3,false); // b3
}
