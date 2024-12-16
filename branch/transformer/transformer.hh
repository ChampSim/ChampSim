#include <array>
#include <bitset>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "FixedVector.hh"
#include <nlohmann/json.hpp> // Nlohmann-json dep

using json = nlohmann::json;

class TransformerBase
{
protected:
  int d_in;    // Input Dimensionality (64 bit IP)
  int d_pos;   // Positional encoding size
  int d_model; // Embeding dimension
  int d_ff;    // Feed-Forward layer size
  int d_q;     // Query Dimension size
  int d_k;     // Key Dimension size
  int d_v;     // Value dimension size

  int num_ma_heads;  // Number of Multi-headed attention heads
  int num_mma_heads; // Number of Masked Multi-headed Attention Heads

  int sequence_len;   // Number of previous instructions passed in as input
  float dropout_rate; // Dropout rate

  FixedVector<uint64_t> ip_history;
  FixedVector<FixedVector<float>> sequence_history; // The previous sequence to
  FixedVector<FixedVector<float>> queries;
  FixedVector<FixedVector<float>> keys;
  FixedVector<FixedVector<float>> values;

public:
  // Construct the transformer from a given input configuration file
  TransformerBase(const std::string& config_file)
  {
    json config = loadConfig(config_file);

    d_in = config["d_in"];
    d_model = config["d_model"];
    d_ff = config["d_ff"];
    d_q = config["d_q"];
    d_k = config["d_k"];
    d_v = config["d_v"];
    d_pos = config["d_pos"];
    num_ma_heads = config["num_ma_heads"];
    num_mma_heads = config["num_mma_heads"];
    dropout_rate = config["dropout_rate"];
    sequence_len = config["sequence_len"]; // 24

    if (d_model % num_mma_heads || d_model % num_ma_heads) {
      throw std::runtime_error("Model size not compatible with number of heads!");
    }
    // Setup Sequence history matrix.
    FixedVector<FixedVector<float>> matrix(sequence_len, FixedVector<float>(d_model, 0)); // Create 2d (d_model x seq_len) matrix of 0's
    sequence_history = matrix;

    // Setup query, key, value matricies
  }

  virtual ~TransformerBase() = default;

  json loadConfig(const std::string& config_file)
  {
    std::ifstream file(config_file);
    if (!file.is_open()) {
      throw std::runtime_error("Could not open config file.");
    }

    return json::parse(file);
  }

  // Virtual function implementations

  // Returns vector of [d_in + d_pos, sequence_len] of floating point "binary-vectors" (Only binary values stored in each float)
  // [d_model * sequence_len]
  // The following needs to be updated for dynamic bitset sizing. (Should be this->sequence_len)
  virtual void hashed_pos_encoding(uint64_t& input, std::bitset<24> global_history) = 0;
  virtual void fixed_pos_encoding(uint64_t& ip) = 0;
  // virtual void learnable_posEncoding(uint64_t ip) = 0;

  // [seuqnece_len * d_model]  (d_model is == to 96-bit positional ecoding)
  virtual FixedVector<FixedVector<float>> MMALayer(const FixedVector<FixedVector<float>>& input) = 0;

  // [sequence_len, d_model]
  virtual FixedVector<FixedVector<float>> MALayer(
      // [num_heads, sequence_len, d_(q,k,v)]
      const FixedVector<FixedVector<FixedVector<float>>>& query, const FixedVector<FixedVector<FixedVector<float>>>& key,
      const FixedVector<FixedVector<FixedVector<float>>>& value) = 0;

  // Input: [sequence_len, d_model]
  // Output: [sequence_len, d_model]
  virtual FixedVector<FixedVector<float>> feedForwardLayer(FixedVector<FixedVector<float>>& input) = 0;
  virtual FixedVector<FixedVector<float>> layerNormalization(FixedVector<FixedVector<float>>& input) = 0;

  virtual bool predict(uint64_t input) = 0; // Final output, branch taken, or not
};