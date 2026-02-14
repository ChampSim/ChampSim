#ifndef EVENT_LISTENERS_H
#define EVENT_LISTENERS_H

#include <iostream>
#include <string>
#include <tuple>
#include <vector>

#include "events.h"
#include "listeners/heartbeat.h"

inline auto listeners = std::make_tuple(Heartbeat(&std::cout));

template <typename>
struct listener_names_helper {
};
template <typename... Listeners>
struct listener_names_helper<std::tuple<Listeners...>> {
  constexpr static auto names = std::array<const char*, sizeof...(Listeners)>{{Listeners::cli_key...}};
};
constexpr inline auto listener_names = listener_names_helper<decltype(listeners)>::names;

inline std::bitset<std::tuple_size_v<decltype(listeners)>> listener_activation_map;

inline void init_event_listeners(const std::vector<std::string>& requested_listeners)
{
  listener_activation_map.reset();
  listener_activation_map[0] = true; // heartbeat is always enabled
  for (std::string name : requested_listeners) {
    bool found = false;
    for (size_t i = 0; i < listener_names.size(); i++) {
      if (listener_names[i] == name) {
        listener_activation_map[i] = true;
        found = true;
        break;
      }
    }
    if (!found) {
      std::cout << "WARNING: Listener \"" << name << "\" not found\n";
    }
  }
}

template <Event e, std::size_t Idx, typename... Args>
void handle_listener_event(Args&&... args)
{
  if (listener_activation_map[Idx]) {
    std::get<Idx>(listeners).template handle_event<e>(std::forward<Args>(args)...);
  }
}

template <Event e, typename... Args, std::size_t... Is>
void handle_listener_event(std::index_sequence<Is...>, Args&&... args)
{
  (handle_listener_event<e, Is>(std::forward<Args>(args)...), ...);
}

template <Event e, typename... Args>
void handle_event(Args&&... args)
{
  handle_listener_event<e>(std::make_index_sequence<std::tuple_size_v<decltype(listeners)>>{}, std::forward<Args>(args)...);
}

#endif