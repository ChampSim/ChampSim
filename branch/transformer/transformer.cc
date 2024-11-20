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