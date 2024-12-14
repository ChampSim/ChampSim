#include "transformer.hh"

#include <algorithm>
#include <array>
#include <bitset>
#include <cmath>
#include <deque>
#include <map>

#include "msl/fwcounter.h"
#include "ooo_cpu.h"
#include "FixedVector.hh"

constexpr std::size_t WEIGHT_BITS = 8;     // We can quantize down to 4 later
constexpr std::size_t HISTORY_LENGTH = 24; // We can adjust. Defaulting to current perceptron's length for closer 1-to-1 comparison

namespace
{
template <std::size_t HISTLEN, std::size_t BITS> // We set the history length and size of weights
class Transformer : public TransformerBase
{
public:
  Transformer(const std::string& config_file) : TransformerBase(config_file) {}

  FixedVector<FixedVector<float>> positionalEncoding(uint64_t input)
  {
    /*
      Positionally encode the input and add it to the sequence_history
    */
    FixedVector<float> input_vec(64);
    FixedVector<FixedVector<float>> output(this->sequence_len, FixedVector<float>(this->d_model));
    return output;
  };
};


} // namespace


void O3_CPU::initialize_branch_predictor(){
}

uint8_t O3_CPU::predict_branch(uint64_t ip){
  return 1;
}

void O3_CPU::last_branch_result(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type){
}


// @deprecated
// class deprecated
// {
// private:
//   // @deprecated
//   uint32_t cyclic_positional_encoding(uint32_t input_vector)
//   {
//     /*
//         Creates the appended positional encoding

//         We are using Contextual positional encoding (CoPE).

//         This is a technique which better positions groups of inputs (eg: words, sentences, paragaphs, etc)
//         For our case, this will help to position groups of instructions (eg: conditional, functions, classes, etc)

//         CoPE also helps us deal with finite hardware, allowing a way to cyclicly position each incoming instruction.

//         https://arxiv.org/abs/2405.18719  (See section 4)
//     */

//     const uint32_t golden_ratio = 2643325761U; // Derived from Knuth's equation ϕ=(1+√5)/2 -> 2^32 * (ϕ - 1) = ratio

//     // Instead of hashing using modulo, we will

//     return (input_vector * golden_ratio) % 0xFFFFFFFF;
//   }

//   // @deprecated
//   uint64_t get_positional_encoding(uint32_t* input_vector)
//   {
//     // The positional encoding is appended to the 32-bit instruction pointer

//     // Create the cyclic encoding
//     uint32_t(*input_vector * 2643325761U) % 0xFFFFFFFF; // Hash-like spreading
//   }

// public:
//   // Predict
//   auto predict(std::bitset<HISTLEN> history) {} // Predicts branch taken or not.

//   void update(bool result, std::bitset<HISTLEN> history) {} // Updates the weights based off branch prediction result.
// };