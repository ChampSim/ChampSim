/*
This is a very basic transformer model.

We need to emphasize speed, size and complexity, in that order.

Because of this we will be using smaller weights (int4)

 */

#include <functional>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#include "msl/fwcounter.h"
#include "ooo_cpu.h"

#define NTABLES 16
#define MAXHIST 232
#define MINHIST 3
#define SPEED 18 // Don't actually know what this does

// We will definetly reduce the layer dimensionality size
#define SUB_LAYER_DIM 512 // Sub-layer dimensionality. See 3.1 Encoder of "Attention is all you need"

class EncoderDecoder
{

public:
  Encoder encoder;
  Decoder decoder;
  Embedding src_embed;
  Embedding tgt_embed;
  Generator generator;

  EncoderDecoder(Encoder encoder, Decoder decoder, Embedding src_embed, Embedding tgt_embed, Generator generator)
  {
    encoder = encoder;
    decoder = decoder;
    src_embed = src_embed;
    tgt_embed = tgt_embed;
    generator = generator;
  }
};

class Generator
{
public:
};

class Encoder
{
public:
};

namespace
{

template <std::size_t HISTLEN, std::size_t BITS>
class perceptron
{
  // We will use this class for the MLP layers

  champsim::msl::sfwcounter<BITS> bias{0}; // Initalize signed fixed-width counter to 0
  std::array<champsim::msl::sfwcounter<BITS>, HISTLEN> weights = {};

public:
  auto predict(std::bitset<HISTLEN> history)
  {
    auto output = bias.value();

    for (std::size_t i = 0; i < std::size(history); i++) {
      if (history[i])
        output += weights[i].value();
      else
        output -= weights[i].value();
    }
    return output;
  }

  void update(bool result, std::bitset<HISTLEN> history) { bias += result ? 1 : -1; }
};

} // namespace
