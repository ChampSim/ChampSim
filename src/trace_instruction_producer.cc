/*
 * Default trace-driven instruction producer module.
 * Wraps a champsim::tracereader to provide instruction packets from a trace file.
 * Parameters (from ModuleBuilder):
 *   - trace_file (std::string): path to the trace file
 *   - producer_group (string, optional): sharing label. Producers with the same
 *     label share one framework-assigned producer id; unlabeled producers each get
 *     their own. Numeric ids are never configured (see origin.h).
 *   - cloudsuite (bool, optional): use cloudsuite trace format (default: false)
 *   - repeat (bool, optional): loop the trace on EOF, replaying it so the producer
 *     never signals end-of-stream and the phase runs to its length (default: true)
 */

#include <optional>
#include <string>

#include "instruction_producer.h"
#include "modules.h"
#include "origin.h"
#include "tracereader.h"

namespace
{

struct trace_instruction_producer : public champsim::modules::instruction_producer {
  std::string trace_path_;
  bool cloudsuite_;
  bool repeat_;

  // Constructed lazily on first peek(): stamping needs the owning consumer's
  // id, and the consumer is bound only after construction.
  std::optional<champsim::tracereader> reader_;
  std::optional<ooo_model_instr> buffer_;

  explicit trace_instruction_producer(champsim::modules::ModuleBuilder builder)
      : trace_path_(builder.get_parameter<std::string>("trace_file")), cloudsuite_(builder.get_parameter<bool>("cloudsuite", true, false)),
        repeat_(builder.get_parameter<bool>("repeat", true, true))
  {
    producer_group_ = builder.get_parameter<std::string>("producer_group", true, std::string{});
  }

  champsim::tracereader& reader()
  {
    if (!reader_.has_value()) {
      auto consumer_id = static_cast<champsim::origin::id_type>(consumer_ != nullptr ? consumer_->consumer_id() : -1);
      reader_.emplace(get_tracereader(trace_path_, champsim::origin{consumer_id, producer_id()}, cloudsuite_, repeat_));
    }
    return *reader_;
  }

  const ooo_model_instr* peek() override
  {
    if (!buffer_.has_value() && !reader().eof()) {
      buffer_ = reader()();
    }
    return buffer_.has_value() ? &*buffer_ : nullptr;
  }

  void consume() override { buffer_.reset(); }

  [[nodiscard]] bool eof() const override { return !buffer_.has_value() && reader_.has_value() && reader_->eof(); }

  std::string describe() const override { return trace_path_; }
};

static champsim::modules::instruction_producer::register_module<trace_instruction_producer> trace_ws_reg("INSTRUCTION_PRODUCER");

} // anonymous namespace
