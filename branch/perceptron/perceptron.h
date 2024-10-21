#ifndef BRANCH_PERCEPTRON_H
#define BRANCH_PERCEPTRON_H

#include <array>
#include <bitset>
#include <deque>

#include "modules.h"
#include "msl/fwcounter.h"

struct perceptron : champsim::modules::branch_predictor {
  template <std::size_t HISTLEN, std::size_t BITS>
  class internal_perceptron
  {
    using counter_type = champsim::msl::sfwcounter<BITS>;

    counter_type bias{0};
    std::array<counter_type, HISTLEN> weights = {};

  public:
    typename counter_type::value_type predict(std::bitset<HISTLEN> history);

    void update(bool result, std::bitset<HISTLEN> history);
  };

  static constexpr std::size_t PERCEPTRON_HISTORY = 24; // history length for the global history shift register
  static constexpr std::size_t PERCEPTRON_BITS = 8;     // number of bits per weight
  static constexpr std::size_t NUM_PERCEPTRONS = 163;

  static constexpr std::size_t NUM_UPDATE_ENTRIES = 100; // size of buffer for keeping 'perceptron_state' for update

  /* 'perceptron_state' - stores the branch prediction and keeps information
   * such as output and history needed for updating the perceptron predictor
   */
  struct perceptron_state {
    champsim::address ip{};
    bool prediction = false;                     // prediction: 1 for taken, 0 for not taken
    long long int output = 0;                    // perceptron output
    std::bitset<PERCEPTRON_HISTORY> history = 0; // value of the history register yielding this prediction
  };

  std::array<internal_perceptron<PERCEPTRON_HISTORY, PERCEPTRON_BITS>, NUM_PERCEPTRONS> perceptrons; // table of perceptrons
  std::deque<perceptron_state> perceptron_state_buf;                                                 // state for updating perceptron predictor
  std::bitset<PERCEPTRON_HISTORY> spec_global_history;                                               // speculative global history - updated by predictor
  std::bitset<PERCEPTRON_HISTORY> global_history;                                                    // real global history - updated when the predictor is
                                                                                                     // updated

  using branch_predictor::branch_predictor;

  bool predict_branch(champsim::address ip);
  void last_branch_result(champsim::address ip, champsim::address branch_target, bool taken, uint8_t branch_type);
};

template <std::size_t HISTLEN, std::size_t BITS>
auto perceptron::internal_perceptron<HISTLEN, BITS>::predict(std::bitset<HISTLEN> history) -> typename counter_type::value_type
{
  auto output = bias.value();

  // find the (rest of the) dot product of the history register and the perceptron weights.
  for (std::size_t i = 0; i < std::size(history); i++) {
    if (history[i])
      output += weights[i].value();
    else
      output -= weights[i].value();
  }

  return output;
}

template <std::size_t HISTLEN, std::size_t BITS>
void perceptron::internal_perceptron<HISTLEN, BITS>::update(bool result, std::bitset<HISTLEN> history)
{
  // if the branch was taken, increment the bias weight, else decrement it, with saturating arithmetic
  bias += result ? 1 : -1;

  // for each weight and corresponding bit in the history register...
  auto upd_mask = result ? history : ~history; // if the i'th bit in the history positively
                                               // correlates with this branch outcome,
  for (std::size_t i = 0; i < std::size(upd_mask); i++) {
    // increment the corresponding weight, else decrement it, with saturating arithmetic
    weights[i] += upd_mask[i] ? 1 : -1;
  }
}

#endif
