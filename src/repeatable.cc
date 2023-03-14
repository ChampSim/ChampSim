#include "repeatable.h"

#include <iostream>

ooo_model_instr champsim::repeatable::operator()()
{
  // Reopen trace if we've reached the end of the file
  if (pimpl_->eof()) {
    auto name = pimpl_->trace_string();
    std::cout << "*** Reached end of trace: " << name << std::endl;
    pimpl_->restart();
  }

  return (*pimpl_)();
}

