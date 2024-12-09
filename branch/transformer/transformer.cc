<<<<<<< HEAD
/*
This is a very basic transformer model.

We need to emphasize speed, size and complexity, in that order.

Because of this we will be using smaller weights (int4)

 */

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"

#include "transformer.hh"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#include "msl/fwcounter.h"
#include "ooo_cpu.h"
#include <torch/torch.h>

#define NTABLES 16
#define MAXHIST 232
#define MINHIST 3
#define SPEED 18 // Don't actually know what this does

 // We will definetly reduce the layer dimensionality size
#define SUB_LAYER_DIM 512 // Sub-layer dimensionality. See 3.1 Encoder of "Attention is all you need"

#define PERCEPTRON_HISTORY 24 // implimentation copying the perceptron
namespace
{
  // Map the transformer predictor to each active core using 'this' pointer
  std::map<O3_CPU*, TransformerBranchPredictor> transformer_predictors;

  // Global history for each core
  std::map<O3_CPU*, std::bitset<PERCEPTRON_HISTORY>> spec_global_history;
  std::map<O3_CPU*, std::bitset<PERCEPTRON_HISTORY>> global_history;

} // namespace

void O3_CPU::initialize_branch_predictor()
{
  // Model parameters
  int64_t src_vocab = 2; // 0 and 1, taken / not taken
  int64_t tgt_vocab = 1; // One bit output (taken / not taken)
  int64_t d_model = 64;  // Model dimension
  int64_t N = 2;         // Number of encoder layers
  int64_t h = 4;         // Number of attention heads
  int64_t d_ff = 64;     // Feed-forward network size
  double dropout = 0.1;  // Dropout rate

  // Initialize the predictor for this CPU using emplace
  auto result = transformer_predictors.emplace(this, TransformerBranchPredictor(src_vocab, tgt_vocab, d_model, N, h, d_ff, dropout));

  if (!result.second) {
    // If the insertion failed (predictor already exists), handle accordingly
    std::cerr << "Predictor already initialized for this CPU" << std::endl;
    std::exit(EXIT_FAILURE);
  }

  // Load trained weights
  try {
    torch::load(result.first->second, "/data/model_weights.pt");
  }
  catch (const c10::Error& e) {
    std::cerr << "ERROR: Failed to load model weights! " << e.what() << std::endl;
    std::exit(EXIT_FAILURE);
  }
}

uint8_t O3_CPU::predict_branch(uint64_t ip)
{
  // Find the predictor for this CPU
  auto it = transformer_predictors.find(this);
  if (it == transformer_predictors.end()) {
    std::cerr << "Error: Predictor is not initialized for this CPU" << std::endl;
    std::exit(EXIT_FAILURE);
  }

  // Embed input as a vector
  std::vector<int64_t> history_tokens(PERCEPTRON_HISTORY);
  for (size_t i = 0; i < PERCEPTRON_HISTORY; ++i) {
    history_tokens[i] = spec_global_history[this][i] ? 1 : 0;
  }
  torch::Tensor src = torch::tensor(history_tokens, torch::kInt64).unsqueeze(0);

  // Masking
  torch::Tensor src_mask = torch::ones({ PERCEPTRON_HISTORY, PERCEPTRON_HISTORY }, torch::kFloat32);

  // Get prediction
  torch::Tensor output = it->second->forward(src, src_mask);
  float prediction_score = output.item<float>();
  bool prediction = prediction_score >= 0.5f;

  // Update speculative global history
  spec_global_history[this] <<= 1;
  spec_global_history[this].set(0, prediction);

  return static_cast<uint8_t>(prediction);
}

void O3_CPU::last_branch_result(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type)
{
  // Update real global history
  global_history[this] <<= 1;
  global_history[this].set(0, taken);

  // Correct speculative history if misprediction occurred
  if (spec_global_history[this][0] != taken) {
    spec_global_history[this] = global_history[this];
  }
}
=======
#include <algorithm>
#include <array>
#include <bitset>
#include <cmath>
#include <deque>
#include <map>

#include "msl/fwcounter.h"
#include "ooo_cpu.h"

constexpr std::size_t WEIGHT_BITS = 8;     // We can quantize down to 4 later
constexpr std::size_t HISTORY_LENGTH = 24; // We can adjust. Defaulting to current perceptron's length for closer 1-to-1 comparison

namespace
{
template <std::size_t HISTLEN, std::size_t BITS> // We set the history length and size of weights
class transformer
{
private:
 // @deprecated
  uint32_t cyclic_positional_encoding(uint32_t input_vector)
  {
    /*
        Creates the appended positional encoding


        We are using Contextual positional encoding (CoPE).

        This is a technique which better positions groups of inputs (eg: words, sentences, paragaphs, etc)
        For our case, this will help to position groups of instructions (eg: conditional, functions, classes, etc)

        CoPE also helps us deal with finite hardware, allowing a way to cyclicly position each incoming instruction.

        https://arxiv.org/abs/2405.18719  (See section 4)
    */

    const uint32_t golden_ratio = 2643325761U; // Derived from Knuth's equation ϕ=(1+√5)/2 -> 2^32 * (ϕ - 1) = ratio

    // Instead of hashing using modulo, we will

    return (input_vector * golden_ratio) % 0xFFFFFFFF;
  }

// @deprecated
  uint64_t get_positional_encoding(uint32_t* input_vector)
  {
    // The positional encoding is appended to the 32-bit instruction pointer

    // Create the cyclic encoding
    uint32_t(*input_vector * 2643325761U) % 0xFFFFFFFF; // Hash-like spreading
  }

public:
  // Predict
  auto predict(std::bitset<HISTLEN> history){} // Predicts branch taken or not.

  void update(bool result, std::bitset<HISTLEN> history){} // Updates the weights based off branch prediction result.


};

} // namespace
>>>>>>> nick/scratch_transformer
