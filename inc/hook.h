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

#ifndef HOOK_H
#define HOOK_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>
#include <fmt/core.h>

/**
 * Named observation points.
 *
 * A hook is declared once, beside whatever emits it, as a registration object -- the same shape the
 * module system uses for interfaces and models:
 *
 *     namespace champsim::hooks {
 *     inline champsim::hook<void(const modules::packet_consumer&, uint64_t, uint64_t)> progress{"progress"};
 *     }
 *
 * Anywhere in the simulator can then emit it by that name, and any listener can subscribe:
 *
 *     champsim::hooks::progress.emit(consumer, retired, cycles);
 *     auto sub = champsim::hooks::progress.subscribe([](auto& c, uint64_t p, uint64_t cy) { ... });
 *
 * Both ends are ordinary typed calls, so an argument that does not match the declaration is a
 * compile error at that line rather than a silently ignored event. Declaring a new hook touches no
 * framework file: a researcher may declare one in their own header, emit it from their own module,
 * and write a listener for it.
 */
namespace champsim
{

class subscription;

// Type-erased base: the registry lists hooks through it, and a subscription cancels through it.
class hook_base
{
public:
  hook_base(const hook_base&) = delete;
  hook_base& operator=(const hook_base&) = delete;
  hook_base(hook_base&&) = delete;
  hook_base& operator=(hook_base&&) = delete;
  virtual ~hook_base() = default;

  [[nodiscard]] const std::string& name() const { return name_; }
  [[nodiscard]] virtual std::size_t subscriber_count() const = 0;
  // Drop every subscription. Only for test teardown: module instances outlive a test case, so
  // their subscriptions would otherwise accumulate across the binary's lifetime.
  virtual void clear_subscribers() = 0;

protected:
  explicit hook_base(std::string name);

private:
  virtual void unsubscribe(uint64_t id) = 0;
  friend class subscription;

  std::string name_;
};

/**
 * The declared hooks, by name. A hook registers itself at static initialisation, so a duplicate
 * name is reported at startup the way a duplicate interface or model name is.
 */
class hook_registry
{
public:
  static std::map<std::string, hook_base*>& hooks()
  {
    static std::map<std::string, hook_base*> registry;
    return registry;
  }

  static void register_hook(const std::string& name, hook_base* hook)
  {
    if (auto [it, fresh] = hooks().try_emplace(name, hook); !fresh) {
      fmt::print("[HOOK] ERROR: duplicate hook name: {}\n", name);
      exit(-1);
    }
  }

  // Test teardown only: see hook_base::clear_subscribers.
  static void clear_all_subscribers()
  {
    for (auto& [name, hook] : hooks()) {
      hook->clear_subscribers();
    }
  }
};

inline hook_base::hook_base(std::string name) : name_(std::move(name)) { hook_registry::register_hook(name_, this); }

/**
 * A live subscription. Cancels itself when destroyed, so a listener that holds one as a member
 * stops being called the moment it goes away -- there is no dangling callback to clean up by hand.
 */
class subscription
{
public:
  subscription() = default;
  subscription(hook_base* owner, uint64_t id) : owner_(owner), id_(id) {}

  subscription(const subscription&) = delete;
  subscription& operator=(const subscription&) = delete;
  subscription(subscription&& other) noexcept : owner_(std::exchange(other.owner_, nullptr)), id_(other.id_) {}
  subscription& operator=(subscription&& other) noexcept
  {
    if (this != &other) {
      cancel();
      owner_ = std::exchange(other.owner_, nullptr);
      id_ = other.id_;
    }
    return *this;
  }
  ~subscription() { cancel(); }

  // Cancel early; harmless on an empty or already-cancelled handle.
  void cancel()
  {
    if (owner_ != nullptr) {
      owner_->unsubscribe(id_);
      owner_ = nullptr;
    }
  }

  [[nodiscard]] bool active() const { return owner_ != nullptr; }

private:
  hook_base* owner_ = nullptr;
  uint64_t id_ = 0;
};

template <typename Signature>
class hook;

/**
 * A hook with the given signature. Payloads are taken by const reference: a listener observes, it
 * does not mutate the caller's state.
 */
template <typename... Args>
class hook<void(Args...)> final : public hook_base
{
  using callback_type = std::function<void(const Args&...)>;

  struct entry {
    uint64_t id;
    callback_type callback;
  };

public:
  explicit hook(std::string name) : hook_base(std::move(name)) {}

  // Notify every subscriber.
  //
  // An unobserved hook costs one load of a bool and a branch that predicts perfectly, so a hook may
  // sit in a hot path and its emitters need not guard it. That flag is why: reaching through the
  // subscriber list to ask whether it is empty is far more expensive than it looks when it happens
  // every cycle, so the answer is kept beside the hook and maintained as subscriptions come and go.
  void emit(const Args&... args) const
  {
    if (!listening_) {
      return;
    }
    dispatch(args...);
  }

  // True when anything is listening. emit() already checks this; a caller needs it only to skip
  // building a payload that is expensive in its own right.
  [[nodiscard]] bool listening() const { return listening_; }

  // Attach a callback. The returned handle owns the subscription -- drop it and the callback stops.
  [[nodiscard]] subscription subscribe(callback_type callback)
  {
    const auto id = next_id_++;
    subscribers_.push_back(entry{id, std::move(callback)});
    listening_ = true;
    return subscription{this, id};
  }

  [[nodiscard]] std::size_t subscriber_count() const override { return subscribers_.size(); }
  void clear_subscribers() override
  {
    subscribers_.clear();
    listening_ = false;
  }

private:
  // The whole fan-out, kept out of emit() so that emitting to nobody stays small enough to inline
  // everywhere it appears.
  void dispatch(const Args&... args) const
  {
    for (const auto& sub : subscribers_) {
      sub.callback(args...);
    }
  }

  void unsubscribe(uint64_t id) override
  {
    subscribers_.erase(std::remove_if(std::begin(subscribers_), std::end(subscribers_), [id](const auto& sub) { return sub.id == id; }),
                       std::end(subscribers_));
    listening_ = !subscribers_.empty();
  }

  // Declared first, and a plain bool: this is what a hot emit site reads.
  bool listening_ = false;
  std::vector<entry> subscribers_;
  uint64_t next_id_ = 0;
};

} // namespace champsim

#endif // HOOK_H
