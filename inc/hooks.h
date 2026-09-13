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

#ifndef HOOKS_H
#define HOOKS_H

#include <cstdint>

#include "hook.h"
#include "packet_consumer.h"
#include "phase_info.h"

// The hooks the simulator itself emits. A hook belongs beside its emitter, so a module or study
// that declares its own puts it in its own header -- nothing here needs editing to add one.
namespace champsim::hooks
{

// A consumer advanced. The counts are cumulative and in the consumer's own unit; the consumer
// formats any human-readable line itself (packet_consumer::progress_message), so an observer of
// this hook never needs to know what an instruction is.
inline champsim::hook<void(const modules::packet_consumer&, uint64_t /*total_progress*/, uint64_t /*total_cycles*/)> progress{"progress"};

// A phase controller began a phase on the modules it governs.
inline champsim::hook<void(const champsim::phase_info&)> phase_begin{"phase_begin"};

} // namespace champsim::hooks

#endif // HOOKS_H
