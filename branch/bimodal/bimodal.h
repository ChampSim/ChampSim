#ifndef BRANCH_BIMODAL_H
#define BRANCH_BIMODAL_H

#include <array>

#include "address.h"
#include "modules.h"
#include "msl/fwcounter.h"

class bimodal : public champsim::modules::branch_predictor
{
  [[nodiscard]] static constexpr auto hash(champsim::address ip) { return ip.to<unsigned long>() % PRIME; }

  static constexpr std::size_t TABLE_SIZE = 16384;
  static constexpr std::size_t PRIME = 16381;
  static constexpr std::size_t BITS = 2;

  std::array<champsim::msl::fwcounter<BITS>, TABLE_SIZE> bimodal_table;

public:

  void initialize_branch_predictor() {};
  bool predict_branch(champsim::address ip, champsim::address predicted_target, bool always_taken, uint8_t branch_type);
  void last_branch_result(champsim::address ip, champsim::address branch_target, bool taken, uint8_t branch_type);
};


#endif
