#ifndef BTB_BASIC_BTB_H
#define BTB_BASIC_BTB_H

#include "direct_predictor.h"
#include "indirect_predictor.h"
#include "modules.h"
#include "return_stack.h"

struct basic_btb : champsim::modules::btb {
  return_stack ras{};
  indirect_predictor indirect{};
  direct_predictor direct{};

  using btb::btb;
  basic_btb() : btb(nullptr) {}

  void initialize_btb();
  std::pair<uint64_t, bool> btb_prediction(uint64_t ip);
  void update_btb(uint64_t ip, uint64_t branch_target, bool taken, uint8_t branch_type);
};

#endif
