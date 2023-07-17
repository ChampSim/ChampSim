#ifndef BTB_BASIC_BTB_H
#define BTB_BASIC_BTB_H

#include "return_stack.h"
#include "indirect_predictor.h"
#include "direct_predictor.h"

#include "address.h"
#include "modules.h"

struct basic_btb : champsim::modules::btb
{
  return_stack ras{};
  indirect_predictor indirect{};
  direct_predictor direct{};

  using btb::btb;
  basic_btb() : btb(nullptr) {}

  void initialize_btb();
  std::pair<champsim::address, bool> btb_prediction(champsim::address ip);
  void update_btb(champsim::address ip, champsim::address branch_target, bool taken, uint8_t branch_type);
};

#endif
