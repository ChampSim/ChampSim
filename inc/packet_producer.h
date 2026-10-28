/*
 *    Copyright 2026 The ChampSim Contributors
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

#ifndef PACKET_PRODUCER_H
#define PACKET_PRODUCER_H

#include <cstdint>
#include <optional>
#include <string>

#include "packet_consumer.h"

namespace champsim::modules
{
// Base contract for a producer of work packets. It carries the producer identity
// stamped on the packets it produces and the packet lifecycle; concrete packet
// types extend it via typed_packet_producer (see instruction_producer). Bound to
// the consumer it feeds after construction; producers stamp packets with
// origin{consumer, producer}, where the producer id defaults to the consumer's id
// (see origin.h).
struct packet_producer {
  virtual ~packet_producer() = default;

  // True when the producer will never provide another packet.
  [[nodiscard]] virtual bool eof() const = 0;

  // Human-readable identity for reports (e.g. the trace path). Empty to suppress.
  virtual std::string describe() const { return {}; }

  // Producer id: identifies which producer produced this packet. Assigned by the
  // framework at startup: every producer gets its own id unless producers share a
  // "producer_group" label in the configuration, in which case they share one.
  // Each context reads it through the packet's origin (e.g. asid() in the
  // instruction/address-space context). Never written as a number in a configuration.
  uint32_t producer_id() const
  {
    if (producer_id_.has_value()) {
      return *producer_id_;
    }
    return default_producer();
  }
  void set_producer_id(uint32_t id)
  {
    if (!producer_id_pinned_) {
      producer_id_ = id;
    }
  }
  // Producers that mirror another holder's producer id (rather than owning one of
  // their own) may pin; pinned producers are skipped by the startup enumeration
  // and do not occupy an id slot.
  void pin_producer_id(uint32_t id)
  {
    producer_id_ = id;
    producer_id_pinned_ = true;
  }
  bool producer_id_pinned() const { return producer_id_pinned_; }
  // The configuration's "producer_group" sharing label; empty when unlabeled.
  const std::string& producer_group() const { return producer_group_; }
  // The instance name this producer was configured under (set by the module
  // factory). Use champsim::identities() for id <-> name lookups.
  const std::string& producer_name() const { return identity_name_; }
  void set_identity_name(std::string name) { identity_name_ = std::move(name); }

protected:
  // The consumer this producer feeds; bound by the framework after construction.
  packet_consumer* consumer_ = nullptr;
  // Set by concrete producers that accept the optional "producer_group" label.
  std::string producer_group_{};
  // Fallback identity for standalone instances (unit tests): the owning
  // consumer's id, the historical default.
  uint32_t default_producer() const { return static_cast<uint32_t>(consumer_ != nullptr ? consumer_->consumer_id() : 0); }

private:
  std::optional<uint32_t> producer_id_{};
  bool producer_id_pinned_ = false;
  std::string identity_name_{};
};

/**
 * Typed pull protocol for packet producers.
 *
 * peek() materializes the next packet without consuming it (so paced
 * consumers can wait until it is due), returning nullptr when no packet is
 * available — the safe emptiness signal, valid to call at any time.
 * consume() discards the peeked packet; next() is the one-shot form.
 *
 * \tparam Packet The discrete unit of work this producer provides.
 */
template <typename Packet>
struct typed_packet_producer : packet_producer {
  // The next packet, or nullptr if none is available now. The pointer is
  // valid until consume() or the next peek().
  virtual const Packet* peek() = 0;

  // Discard the current peeked packet. Only valid after a non-null peek().
  virtual void consume() = 0;

  // Retrieve and consume the next packet, if one is available.
  std::optional<Packet> next()
  {
    if (const Packet* packet = peek(); packet != nullptr) {
      auto retval = std::optional<Packet>{*packet};
      consume();
      return retval;
    }
    return std::nullopt;
  }
};
} // namespace champsim::modules

#endif
