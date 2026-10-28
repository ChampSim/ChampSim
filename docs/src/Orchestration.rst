.. _Orchestration:

====================================
Simulation Orchestration
====================================

ChampSim's simulation loop is itself modular. The orchestrator knows nothing about
instructions, cores, or traces: it ticks *operables*, asks *phase controllers* when the
run is done, and reports progress through *listeners*. Work flows from *packet producers*
into *packet consumers* as opaque **packets** — instructions for a core, but equally
packets for a network consumer or records for a memory-stream consumer. With the right
modules defined, an explicit configuration can assume any shape (an OoO CPU simulator,
a memory-only system, a network simulator) without modifying ChampSim source.

The legacy configuration format hides all of this: it always builds the classic
out-of-order CPU simulator with the default orchestration modules.

------------------------------------------
The Simulation Loop
------------------------------------------

Each cycle, the orchestrator:

1. Ticks a global picosecond clock by the smallest clock period among the operables (see
   :ref:`Orch_Discovery`) and lets every operable catch up to it (``operable::operate_on``).
   Operables in slower clock domains naturally run on fewer ticks.
2. Sums the *progress* returned by every ``operate()`` call and passes it to every phase
   controller's ``advance()``.
3. Acts on each controller's returned status: **CONTINUE** keeps ticking; **COMPLETE** means
   a phase ended, so it collects that phase's statistics and calls ``advance()`` again to
   begin the next; **DONE** retires that controller; **ABORT** stops the run early. The run
   ends once every controller is DONE.

A phase controller owns *both* edges of its phases: ``advance()`` begins the next phase
(setting each governed module's warmup flag before that phase's first simulated cycle) and,
on completion, ends it. The orchestrator never begins or ends a phase itself — it only ticks
operables, calls ``advance()``, and collects a completed phase's statistics.

Phases are described by ``champsim::phase_info``: a name, an ``is_warmup`` flag, an
``roi`` flag, and a length denominated in **each consumer's own progress unit**
(instructions for a core). ``roi`` decides whether the controller keeps what that phase
measured; it defaults to ``!is_warmup``, and setting it explicitly allows unmeasured
non-warmup phases (e.g. a fast-forward region between warmup and the measured region).
A phase appears in the results exactly when its ``roi`` is set.

------------------------------------------
Packets: Producers and Consumers
------------------------------------------

A **packet** is one discrete unit of work. Producers produce packets; consumers execute them.
The orchestration layer never sees the packet type — a consumer and its producers agree on
it by construction.

Packet producers
^^^^^^^^^^^^^^^^^^^^

``champsim::modules::packet_producer`` is the base contract every producer satisfies: the
packet-agnostic lifecycle plus the producer identity stamped on its packets:

.. code-block:: cpp

    struct packet_producer {
      virtual bool eof() const = 0;              // no more packets, ever
      virtual std::string describe() const;      // e.g. the trace path, for reports
      uint32_t producer_id() const;                // which producer produced the packet
      const std::string& producer_group() const;   // the "producer_group" sharing label, if any
    };

The typed pull protocol lives on a template subclass:

.. code-block:: cpp

    template <typename Packet>
    struct typed_packet_producer : packet_producer {
      virtual const Packet* peek() = 0;   // next packet without consuming; nullptr = none now
      virtual void consume() = 0;        // discard the peeked packet
      std::optional<Packet> next();       // peek + consume in one step
    };

``peek()`` is always safe to call — a null return is the emptiness signal, so exhausted
and empty producers need no special-casing. Paced consumers can ``peek()`` a packet, wait
until it is due, and only then ``consume()`` it.

``champsim::modules::instruction_producer`` extends ``typed_packet_producer<ooo_model_instr>``
with the execution-driven feedback hooks (``retire_instruction``, ``squash_instruction``,
``branch_mispredict``). It is the registered interface a core attaches; the shipped
``INSTRUCTION_PRODUCER`` model reads a trace file:

* ``trace_file`` (string) — path to the trace
* ``producer_group`` (optional string) — sharing label: producers with the same label share
  one producer id; unlabeled producers each get their own
* ``cloudsuite``, ``repeat`` (optional booleans)

Producers are declared as ``children`` of their consumer and are bound to it after
construction.

Packet consumers
^^^^^^^^^^^^^^^^^^^^

Any module that executes packets inherits the ``champsim::modules::packet_consumer``
mixin. It is the orchestration layer's entire view of "the thing doing work":

.. code-block:: cpp

    struct packet_consumer {
      int consumer_id() const;                     // hardware-context id, framework-assigned
      virtual uint64_t sim_progress() const;       // cumulative packets completed
      virtual bool producers_eof() const;             // all attached producers exhausted
      virtual consumer_health check_health(uint64_t elapsed);  // periodic self-check
      virtual void reset_health();                 // re-baseline at phase start
      // report formatting: producer_finish_message, phase_complete_message
    };

**Health is the consumer's own judgment.** The consumer knows its expected progress rate
(a core knows what a plausible IPC floor is; a packet injector knows its schedule), so the
phase controller delegates the liveness decision to ``check_health``: it is called every
``health_period`` cycles, and a ``stalled`` verdict aborts the run. ``core_module``
implements the instruction-rate policy.

Consumer ids are framework-assigned by enumerating consumers in declaration order, dense
from 0; they are never set in a configuration. The consumer count — derived automatically
by counting the consumers in the config — sizes per-consumer tables such as ``ship`` and
``drrip`` replacement state.

------------------------------------------
Phase Controllers
------------------------------------------

A phase controller decides when a phase is complete and when a run is unhealthy. It is
an ordinary module (interface ``phase_controller``, parent: the environment):

.. code-block:: cpp

    struct phase_controller {
      enum class status { CONTINUE, COMPLETE, ABORT, DONE };
      // The sole driver; owns both phase edges. Returns CONTINUE (keep ticking), COMPLETE
      // (a phase ended — the orchestrator takes the stats it collected, then calls advance()
      // again to begin the next), DONE (no phases remain — stop clean), or ABORT (deadlock).
      virtual status advance(long progress) = 0;
      virtual const phase_info& phase() const = 0;                     // the phase now running
      virtual std::vector<unsigned> newly_completed_consumers() const = 0;
      const std::vector<module_lifecycle*>& governed_modules() const;  // the set it governs
      std::optional<phase_stats> take_phase_stats();                   // what a measured phase reported
    };

The generic shipped model is ``PHASE_CONTROLLER``. It is packet-agnostic and owns its own
phase plan plus these generic mechanics over its governed set:

* **Completion** — a consumer completes when its ``sim_progress()`` delta reaches the
  phase length (in its own packet unit), or when one of its producers signals end-of-stream.
* **Deadlock** — ``deadlock_cycles`` consecutive zero-progress cycles abort the run. Work
  that is scheduled but retires nothing (an in-flight DRAM refresh) reports itself through
  ``operate()``'s progress, so a plain zero-progress count is sufficient.
* **Health** — every ``health_period`` cycles each governed consumer's ``check_health``
  runs; a ``stalled`` verdict aborts.

Parameters::

    {
        "name": "pc", "module": "phase_controller", "model": "PHASE_CONTROLLER",
        "deadlock_cycles": 500,
        "health_period": 10000000,
        "eof_policy": "complete_all",
        "warmup_length": "$warmup_instructions",
        "simulation_length": "$simulation_instructions"
    }

By default a trace producer replays (``repeat: true``): it reopens the trace at the end and
never signals end-of-stream, so it feeds instructions until the consumer reaches the phase
length. ``eof_policy`` matters only for a *finite* producer that does signal end-of-stream (a
non-repeating trace, or a bounded generator): ``"complete_all"`` (default) ends the phase
for every consumer at that first signal; ``"complete_consumer"`` retires only the consumer
whose producer ended and lets the others run on to their own phase lengths (heterogeneous
mixes). Any value other than the exact string ``"complete_consumer"`` is treated as
``"complete_all"``.

Instead of ``warmup_length``/``simulation_length``, a controller may declare an
arbitrary phase list::

    "phases": [
        {"name": "Warmup",      "is_warmup": true,  "length": 10000000},
        {"name": "FastForward", "is_warmup": false, "roi": false, "length": 50000000},
        {"name": "Measured",    "is_warmup": false, "length": 100000000}
    ]

**Multiple controllers.** A configuration may declare any number of phase controllers.
Each governs a set of ``module_lifecycle`` modules, named by reference in
``"governs": [@L1D, @L2C]`` (default: all of them), so different completion and health
policies can apply to different parts of a heterogeneous system. The governed sets must be
disjoint and together cover every ``module_lifecycle`` module — so each module hears phase
edges from exactly one controller and its statistics are attributed unambiguously — which
the environment validates at startup. Each controller owns its own phase plan and drives it
independently: the run aborts if *any* controller aborts and ends once *every* controller is
DONE.

When a configuration declares no controller, the orchestrator creates a default
``PHASE_CONTROLLER`` and the ``-w``/``-i`` CLI options define the phases — this is the
legacy environment's path.

------------------------------------------
Hooks and Listeners
------------------------------------------

A **listener** watches a run and reports on it without being part of it. Performance
characterisation lives here: anything that wants to observe what the simulator is doing --
count events, trace behaviour, print progress -- is a listener, and none of it requires
touching the models being measured.

Listeners observe **hooks**: named points the simulator reports from. The hooks it emits are

.. list-table::
   :header-rows: 1
   :widths: 20 80

   * - Hook
     - Reported when
   * - ``progress``
     - A consumer advanced. Carries the consumer, and its cumulative progress and cycles, in
       that consumer's own unit.
   * - ``phase_begin``
     - A phase controller began a phase on the modules it governs. Carries the ``phase_info``.

Using listeners
^^^^^^^^^^^^^^^^^^^^

A listener is an ordinary module: you get one because a configuration asks for one, and a run
that asks for nothing observes nothing. Declare it alongside the caches and cores::

    {"name": "heartbeat", "module": "listener", "model": "HEARTBEAT", "frequency": 1000000}

Give a parameter a ``$var`` and it becomes a command-line option, so a listener's settings can
change per run without editing the configuration::

    {"name": "heartbeat", "module": "listener", "model": "HEARTBEAT", "frequency": "$hb_freq"}

then ``--hb_freq 1000000``. There is no separate switch for choosing listeners; declaring them
is how you choose them.

The legacy configuration format has no place to declare one, so it builds a ``HEARTBEAT`` of
its own — the classic ChampSim behaviour, where every run prints progress. Its interval comes
from the root-level ``"heartbeat_frequency"``, and ``--hide-heartbeat`` (or ``"hide_heartbeat":
true``) omits it.

The shipped ``HEARTBEAT`` prints the periodic
``Heartbeat CPU N instructions: ... cumulative IPC: ...`` line.

Writing a listener
^^^^^^^^^^^^^^^^^^^^

Subscribe to the hooks you care about and keep the handles. Parameters come from the
configuration, so a study's knobs can change without recompiling:

.. code-block:: cpp

    class miss_counter : public champsim::modules::listener
    {
      uint64_t threshold_;
      champsim::subscription sub_;

    public:
      explicit miss_counter(champsim::modules::ModuleBuilder builder)
        : threshold_(builder.get_parameter<uint64_t>("threshold", true, 1000)),
          sub_(champsim::hooks::progress.subscribe(
              [this](const champsim::modules::packet_consumer& consumer, const uint64_t& progress, const uint64_t& cycles) {
                // ... observe ...
              }))
      {}
    };
    static champsim::modules::listener::register_module<miss_counter> reg("MISS_COUNTER");

Put the implementation in ``src/listeners/``; it is compiled in from there. Keep the
``champsim::subscription`` a member -- dropping it unsubscribes, so a listener that discards
its handles is never called.

To report something new, declare a hook next to whatever reports it and emit it. No central
file lists them, so a study can add its own without modifying the framework:

.. code-block:: cpp

    // in your own header
    namespace champsim::hooks {
    inline champsim::hook<void(const champsim::address&, bool)> my_event{"my_event"};
    }

    // wherever the event happens
    champsim::hooks::my_event.emit(addr, hit);

A hook nobody is listening to costs a load and a branch, so one may sit in a hot path and you do
not need to guard it -- ``emit`` is already the fast path. If building the payload is itself
expensive, ``listening()`` lets you skip that too::

    if (champsim::hooks::my_event.listening()) {
      champsim::hooks::my_event.emit(addr, expensive_to_compute());
    }

A listener that needs phase edges, or that wants to publish its own statistics into a phase's
results, may also inherit ``champsim::module_lifecycle`` -- it is then a phase participant and
must be governed by a phase controller like any other.

------------------------------------------
Idle Cycle Skipping
------------------------------------------

Operables may skip cycles in which they have no work. The hook:

.. code-block:: cpp

    class operable {
      // Called before each local cycle is simulated, with current_time already
      // advanced to the cycle under consideration.
      //   return 0 -> simulate this cycle (operate() runs)
      //   return n -> skip n local cycles without simulation
      virtual long poll_cycle() { return 0; }
    };

A skipped cycle advances ``current_time`` and contributes no progress — exactly like an
idle ``operate()`` — so observable behavior is unchanged. Implementations must uphold
two rules:

* **Parent-ticked hooks continue.** Any submodule hook that is contractually per-cycle
  must still fire on skipped cycles. The cache's ``poll_cycle`` calls
  ``prefetcher_cycle_operate`` (and keeps its upper-level round-robin aligned) while
  skipping, so prefetcher-internal clocks observe an unbroken cycle stream.
* **Push-fed operables skip at most one cycle at a time.** A module that can receive work
  by external push (channel enqueue, returned-queue push) cannot know that nothing will
  arrive; sleeping further would add latency. Returns greater than 1 are reserved for
  self-contained modules with no external inputs.

All shipped operables implement the hook (cache, core, memory controller, page-table
walker). Skipping is enabled by default; the root config key ``"cycle_skip": false``
disables it globally, which is the switch used to A/B-verify that skipping does not change
results.

Liveness is judged purely by progress: the deadlock detector counts consecutive cycles in
which every ``operate()`` returned zero. **Timer-scheduled** work that retires nothing but
is genuinely advancing — a DRAM refresh in flight, a busy bank — must therefore report a
non-zero progress from its ``operate()`` so those cycles do not count as a stall. Work that
merely waits on another module's response reports zero, which is exactly the state the
deadlock detector exists to catch.

.. _Orch_Discovery:

------------------------------------------
Discovery: Views and Enrollment
------------------------------------------

The environment exposes every constructed module through ``view(interface_name)`` /
``typed_view<T>(interface_name)``. Two aggregate keys cut across interfaces:

* ``"operable"`` — every instance that inherits ``champsim::operable``, whether the
  *interface* or only the *model* inherits it. Anything in this view is ticked
  automatically by the orchestrator.
* ``"packet_consumer"`` — every instance that inherits ``packet_consumer``.

Submodule-created instances participate: when a parent constructs its children (a core
its producers, a cache its prefetchers), each instance self-enrolls with the environment,
appended after the top-level modules. An operable packet producer nested under a consumer
is therefore found and ticked with no additional wiring. ``get_num(name)`` always agrees
with ``view(name).size()``.

Modules opt into further orchestration roles by inheriting mixins:

* ``champsim::module_lifecycle`` — a module that resets each phase and reports what that
  phase measured. Implement two hooks:

  .. code-block:: cpp

      virtual void begin_phase(bool warmup) {}               // start counting again
      virtual void end_phase(champsim::stat_report& out) {}  // write what this phase measured

  Write plaintext lines and JSON into the same report; whatever you write appears in that
  phase's results, keyed by your module's interface, model and name. Write nothing and your
  module simply has no statistics. ``operable`` already inherits this, so a cache or a core
  need only override the hooks; a module that is not operable may inherit it directly. Each
  one must be governed by exactly one phase controller.

------------------------------------------
Root Configuration Key Reference
------------------------------------------

``environment``
    Environment model: ``"LEGACY_ENVIRONMENT"`` (default; classic config format) or
    ``"ENVIRONMENT"`` (explicit format).
``cycle_skip``
    Enable idle cycle skipping (default ``true``).
``heartbeat_frequency``
    Interval, in each consumer's own progress unit, for the heartbeat the legacy format builds.
``hide_heartbeat``
    Omit the heartbeat the legacy format would otherwise build (what ``--hide-heartbeat`` sets).
``block_size``, ``page_size``
    System-wide geometry, published to all modules via the builder globals.

Any other non-reserved top-level scalar is likewise published as a global; see
:doc:`Explicit-configuration-format` for globals and lexical scoping.
