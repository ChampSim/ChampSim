#ifndef BTB_BASIC_BTB_H
#define BTB_BASIC_BTB_H

#include "return_stack.h"
#include "indirect_predictor.h"
#include "direct_predictor.h"

#include "modules.h"

struct basic_btb : champsim::modules::btb
{
  return_stack ras{};
  indirect_predictor indirect{};
  direct_predictor direct{};

  using btb::btb;
  basic_btb() : btb(nullptr) {}

  void initialize_btb();
  std::pair<uint64_t, uint8_t> btb_prediction(uint64_t ip);
  void update_btb(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type);
};

#endif
