#ifndef FOLDED_SHIFT_REGISTER_H
#define FOLDED_SHIFT_REGISTER_H

#include <vector>

#include "modules.h"
#include "msl/bits.h"
#include "msl/fwcounter.h"
#include "util/bit_enum.h"

/**
 * @brief A shift register that folds words before it returns its value.
 *
 * This class maintains a history of bits that have been pushed into it.
 * When the user asks for its value, it folds the history in WORD_LEN chunks,
 * returning the XOR of the words.
 */
template <champsim::data::bits WORD_LEN>
class folded_shift_register
{
  /*
   * This class uses two terms: "value" and "word". A value contains one or more packed words.
   */
  using value_type = unsigned long long;
  static_assert(champsim::data::bits{std::numeric_limits<value_type>::digits} >= WORD_LEN);
  constexpr static auto NUM_WORDS_PER_VALUE = champsim::data::bits{std::numeric_limits<value_type>::digits}
                                              / WORD_LEN; // this many 12-bit words will be kept per int in the table in the global history
  constexpr static auto VALUE_LEN = WORD_LEN * NUM_WORDS_PER_VALUE;

  value_type last_value_mask;    // The last word may not be the full width of the value
  std::vector<value_type> words; // The history is represented as a series of values
public:
  folded_shift_register();
  explicit folded_shift_register(champsim::data::bits length);

  std::size_t value() const;

  /**
   *  Insert this value into the shift register
   **/
  void push_back(bool ins);
};

template <champsim::data::bits WORD_LEN>
folded_shift_register<WORD_LEN>::folded_shift_register() : folded_shift_register(champsim::data::bits{})
{
}

template <champsim::data::bits WORD_LEN>
folded_shift_register<WORD_LEN>::folded_shift_register(champsim::data::bits length)
    : last_value_mask(champsim::msl::bitmask(length % VALUE_LEN)), words((length / VALUE_LEN) + ((length % VALUE_LEN != champsim::data::bits{}) ? 1 : 0))
{
}

template <champsim::data::bits WORD_LEN>
std::size_t folded_shift_register<WORD_LEN>::value() const
{
  // XOR all of the entries together
  auto joined_words = std::accumulate(std::begin(words), std::end(words), value_type{}, std::bit_xor<>{});

  // Unpack the words from the accumulated values
  std::size_t result = joined_words;
  if constexpr (NUM_WORDS_PER_VALUE > 1) {
    for (std::size_t i = 0; i <= NUM_WORDS_PER_VALUE; ++i) {
      joined_words >>= champsim::to_underlying(WORD_LEN);
      result ^= joined_words;
    }
  }
  return result & champsim::msl::bitmask(champsim::data::bits{WORD_LEN});
}

template <champsim::data::bits WORD_LEN>
void folded_shift_register<WORD_LEN>::push_back(bool ins)
{
  /**
   * Find the MSB of the value. This MSB will be passed along to the next word in the array.
   */
  auto extract_msb = [](auto x) {
    auto msb_loc = champsim::to_underlying(VALUE_LEN) - 1;
    return (x & (value_type{1} << msb_loc)) >> msb_loc;
  };

  auto shift_and_apply_lsb = [](auto x, auto lsb) {
    return ((x << 1) | lsb) & champsim::msl::bitmask(VALUE_LEN);
  };

  // The MSBs to be passed to the next value.
  // The vector is initialized with the new bit, so we get our shift for free.
  std::vector<value_type> msbs = {ins ? value_type{0x1} : value_type{0x0}};
  std::transform(std::cbegin(words), std::cend(words), std::back_inserter(msbs), extract_msb);
  std::transform(std::cbegin(words), std::cend(words), std::begin(msbs), std::begin(words), shift_and_apply_lsb);

  // Don't apply the mask if the last value is full-width
  if (last_value_mask != value_type{}) {
    words.back() &= last_value_mask;
  }
}

#endif
