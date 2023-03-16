#ifndef REPEATABLE_H
#define REPEATABLE_H

#include <memory>

#include "instruction.h"

namespace champsim
{
class repeatable
{
  struct repeatable_concept {
    virtual ~repeatable_concept() = default;
    virtual ooo_model_instr operator()() = 0;
    virtual bool eof() const = 0;
    virtual std::string trace_string() const = 0;
    virtual void restart() = 0;
  };

  template <typename T>
  struct repeatable_model final : public repeatable_concept {
    T intern_;
    repeatable_model(T&& val) : intern_(std::move(val)) {}

    ooo_model_instr operator()() override { return intern_(); }
    bool eof() const override { return intern_.eof(); }
    std::string trace_string() const override { return intern_.trace_string; } // forward to member variable
    void restart() { intern_.restart(); }
  };

  std::unique_ptr<repeatable_concept> pimpl_;

public:
  template <typename T>
  repeatable(T&& val) : pimpl_(std::make_unique<repeatable_model<T>>(std::move(val)))
  {
  }

  ooo_model_instr operator()();
};
} // namespace champsim

#endif
