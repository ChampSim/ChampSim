#ifndef CHAMPSIM_PACKET_H
#define CHAMPSIM_PACKET_H

#include <cstdint>
#include <limits>
#include <vector>

#include "access_type.h"
#include "address.h"
#include "origin.h"

namespace champsim
{
struct request {
  bool is_translated = true;
  bool response_requested = true;

  access_type type{access_type::LOAD};

  uint32_t pf_metadata = 0;
  // Provenance: which consumer injected this request, which stream
  // (address space) it belongs to. See origin.h.
  champsim::origin origin{};

  champsim::address address{};
  champsim::address v_address{};
  champsim::address data{};
  uint64_t instr_id = 0;
  champsim::address ip{};

  std::vector<uint64_t> instr_depend_on_me{};
};

struct response {
  champsim::address address{};
  champsim::address v_address{};
  champsim::address data{};
  uint32_t pf_metadata = 0;
  std::vector<uint64_t> instr_depend_on_me{};

  response(champsim::address addr, champsim::address v_addr, champsim::address data_, uint32_t pf_meta, std::vector<uint64_t> deps)
      : address(addr), v_address(v_addr), data(data_), pf_metadata(pf_meta), instr_depend_on_me(deps)
  {
  }
  explicit response(request req) : response(req.address, req.v_address, req.data, req.pf_metadata, req.instr_depend_on_me) {}
};
}; // namespace champsim

#endif // CHAMPSIM_PACKET_H