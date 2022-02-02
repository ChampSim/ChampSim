/*
 * Copyright (c) 2001 University of Texas at Austin
 *
 * Daniel A. Jimenez
 * Calvin Lin
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software (the "Software"), to deal in
 * the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE UNIVERSITY OF TEXAS AT
 * AUSTIN BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * This file implements the simulated perceptron branch predictor from:
 *
 * Jimenez, D. A. & Lin, C., Dynamic branch prediction with perceptrons,
 * Proceedings of the Seventh International Symposium on High Performance
 * Computer Architecture (HPCA), Monterrey, NL, Mexico 2001
 *
 * The #define's here specify a perceptron predictor with a history
 * length of 24, 163 perceptrons, and  8-bit weights.  This represents
 * a hardware budget of (24+1)*8*163 = 32600 bits, or about 4K bytes,
 * which is comparable to the hardware budget of the Alpha 21264 hybrid
 * branch predictor.
 */

#include <algorithm>
#include <array>
#include <bitset>
#include <deque>
#include <map>

#include "ooo_cpu.h"

template <typename T, std::size_t HISTLEN, std::size_t BITS>
class perceptron
{
  T bias = 0;
  std::array<T, HISTLEN> weights = {};

public:
  // maximum and minimum weight values
  constexpr static T max_weight = (1 << (BITS - 1)) - 1;
  constexpr static T min_weight = -(max_weight + 1);

  T predict(std::bitset<HISTLEN> history)
  {
    auto output = bias;

    // find the (rest of the) dot product of the history register and the
    // perceptron weights.
    for (std::size_t i = 0; i < std::size(history); i++) {
      if (history[i])
        output += weights[i];
      else
        output -= weights[i];
    }

    return output;
  }

  void update(bool result, std::bitset<HISTLEN> history)
  {
    // if the branch was taken, increment the bias weight, else decrement it,
    // with saturating arithmetic
    if (result)
      bias = std::min(bias + 1, max_weight);
    else
      bias = std::max(bias - 1, min_weight);

    // for each weight and corresponding bit in the history register...
    auto upd_mask = result ? history : ~history; // if the i'th bit in the history positively
                                                 // correlates with this branch outcome,
    for (std::size_t i = 0; i < std::size(upd_mask); i++) {
      // increment the corresponding weight, else decrement it, with saturating
      // arithmetic
      if (upd_mask[i])
        weights[i] = std::min(weights[i] + 1, max_weight);
      else
        weights[i] = std::max(weights[i] - 1, min_weight);
    }
  }
};

constexpr std::size_t PERCEPTRON_HISTORY = 24; // history length for the global history shift register
constexpr std::size_t PERCEPTRON_BITS = 8;     // number of bits per weight
constexpr std::size_t NUM_PERCEPTRONS = 163;

constexpr int THETA = 1.93 * PERCEPTRON_HISTORY + 14; // threshold for training

constexpr std::size_t NUM_UPDATE_ENTRIES = 100; // size of buffer for keeping 'perceptron_state' for update

/* 'perceptron_state' - stores the branch prediction and keeps information
 * such as output and history needed for updating the perceptron predictor
 */
struct perceptron_state {
  uint64_t ip = 0;
  bool prediction = false;                     // prediction: 1 for taken, 0 for not taken
  int output = 0;                              // perceptron output
  std::bitset<PERCEPTRON_HISTORY> history = 0; // value of the history register yielding this prediction
};

std::map<O3_CPU*, std::array<perceptron<int, PERCEPTRON_HISTORY, PERCEPTRON_BITS>,
                             NUM_PERCEPTRONS>> perceptrons;             // table of perceptrons
std::map<O3_CPU*, std::deque<perceptron_state>> perceptron_state_buf;   // state for updating perceptron predictor
std::map<O3_CPU*, std::bitset<PERCEPTRON_HISTORY>> spec_global_history; // speculative global history - updated by predictor
std::map<O3_CPU*, std::bitset<PERCEPTRON_HISTORY>> global_history;      // real global history - updated when the predictor is
                                                                        // updated

void O3_CPU::initialize_branch_predictor() {}

uint8_t O3_CPU::predict_branch(uint64_t ip, uint64_t predicted_target, uint8_t always_taken, uint8_t branch_type)
{
  // hash the address to get an index into the table of perceptrons
  auto index = ip % NUM_PERCEPTRONS;
  auto output = perceptrons[this][index].predict(spec_global_history[this]);

  bool prediction = (output >= 0);

  // record the various values needed to update the predictor
  perceptron_state_buf[this].push_back({ip, prediction, output, spec_global_history[this]});
  if (std::size(perceptron_state_buf[this]) > NUM_UPDATE_ENTRIES)
    perceptron_state_buf[this].pop_front();

  // update the speculative global history register
  spec_global_history[this] <<= 1;
  spec_global_history[this].set(0, prediction);
  return prediction;
}

void O3_CPU::last_branch_result(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type)
{
  auto state = std::find_if(std::begin(perceptron_state_buf[this]), std::end(perceptron_state_buf[this]), [ip](auto x) { return x.ip == ip; });
  if (state == std::end(perceptron_state_buf[this]))
    return; // Skip update because state was lost

  auto [_ip, prediction, output, history] = *state;
  perceptron_state_buf[this].erase(state);

  auto index = ip % NUM_PERCEPTRONS;

  // update the real global history shift register
  global_history[this] <<= 1;
  global_history[this].set(0, taken);

  // if this branch was mispredicted, restore the speculative history to the
  // last known real history
  if (prediction != taken)
    spec_global_history[this] = global_history[this];

  // if the output of the perceptron predictor is outside of the range
  // [-THETA,THETA] *and* the prediction was correct, then we don't need to
  // adjust the weights
  if ((output <= THETA && output >= -THETA) || (prediction != taken))
    perceptrons[this][index].update(taken, history);
}
