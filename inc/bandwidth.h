/*
 *    Copyright 2023 The ChampSim Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef BANDWIDTH_H
#define BANDWIDTH_H

namespace champsim
{
/**
 * This class encapuslates the operation of consuming a fixed number of operations, a very common operation in ChampSim.
 * Once initialized, the maximum bandwidth cannot be changed. Instead, consuming the bandwidth reduces the amount available
 * until it is depleted.
 */
class bandwidth
{
  using underlying_type = long int;
  enum class max_t : underlying_type {};

public:
  /**
   * The type of the maximum. This type is integer-like, with the exception that it is immutable.
   *
   * This type is exported so that other types can keep maximums as members.
   */
  using maximum_type = max_t;

private:
  underlying_type value_;
  maximum_type maximum_;

public:
  /**
   * Consume some of the bandwidth.
   *
   * \param delta The amount of bandwidth to consume
   *
   * \throws std::range_error if more than the maximum amount of bandwidth will have been consumed.
   */
  void consume(underlying_type delta);

  /**
   * Consume one unit of bandwidth.
   *
   * \throws std::range_error if more than the maximum amount of bandwidth will have been consumed.
   */
  void consume();

  /**
   * Report if the bandwidth has one or more unit remaining.
   */
  bool has_remaining() const;

  /**
   * Report the amount of bandwidth that has been consumed
   */
  underlying_type amount_consumed() const;

  /**
   * Report the amount of bandwidth that remains
   */
  underlying_type amount_remaining() const;

  /**
   * Reset the bandwidth, so that it can be used again.
   */
  void reset();

  /**
   * Initialize a bandwidth with the specified maximum.
   */
  explicit bandwidth(maximum_type maximum);
};
} // namespace champsim

#endif
