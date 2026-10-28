.. _Core_model:

=====================================
Core Model
=====================================

The core model in ChampSim is split into an **interface** and a **default implementation**:

* ``champsim::modules::core_module`` is the abstract interface that defines the contract
  all core implementations must fulfill. Branch predictors and BTBs interact with their
  parent core through this interface.

* ``O3_CPU`` is the default implementation of ``core_module`` (registered as
  ``"DEFAULT_CORE"``). It provides an aggressive out-of-order execution model with
  configurable pipeline widths, buffer sizes, and latencies.

--------------------------------------
Configuration Parameters
--------------------------------------

Buffer and Queue Sizes
^^^^^^^^^^^^^^^^^^^^^^^^

``ifetch_buffer_size``
    Instruction fetch buffer capacity.

``dispatch_buffer_size``
    Dispatch buffer capacity.

``decode_buffer_size``
    Decode buffer capacity.

``rob_size``
    Reorder buffer size.

``lq_size``
    Load queue capacity.

``sq_size``
    Store queue capacity.

``register_file_size``
    Register file size.

``dib_hit_buffer_size``
    Decoded instruction buffer (DIB) hit buffer size.

Pipeline Widths
^^^^^^^^^^^^^^^^^^

``fetch_width``, ``decode_width``, ``dispatch_width``, ``schedule_width``, ``execute_width``, ``retire_width``
    Maximum number of instructions processed per cycle in each pipeline stage.

``lq_width`` / ``sq_width``
    Load and store queue widths per cycle.

``l1i_bandwidth`` / ``l1d_bandwidth``
    L1 instruction and data cache bandwidth.

``dib_inorder_width``
    DIB in-order processing width.

Pipeline Latencies
^^^^^^^^^^^^^^^^^^^^^

``decode_latency``, ``dispatch_latency``, ``schedule_latency``, ``execute_latency``
    Latency (in cycles) for each pipeline stage.

``mispredict_penalty``
    Branch misprediction penalty in cycles.

``dib_hit_latency``
    Latency for a DIB hit.

Decoded Instruction Buffer (DIB)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

``dib_set``, ``dib_way``, ``dib_window``
    Set/way/window geometry of the decoded instruction buffer.

--------------------------------------
Submodules
--------------------------------------

Each ``O3_CPU`` instance has three submodules attached through its ``"children"`` array:

* A **branch predictor** (``"module": "branch_predictor"``), e.g. ``hashed_perceptron``,
  ``bimodal``, ``gshare``, ``perceptron``.
* A **BTB** (``"module": "btb"``), e.g. ``basic_btb``.
* An **instruction producer** (interface ``"instruction_producer"``), e.g. ``INSTRUCTION_PRODUCER``,
  which supplies the core's instruction stream. A core requires exactly one.

--------------------------------------
Pipeline Stages
--------------------------------------

The ``O3_CPU`` models a full out-of-order pipeline. Each cycle, ``operate()`` drives
the following stages:

1. **Fetch** (``fetch_instruction()``): Fetch instructions from the L1I cache into
   the ``IFETCH_BUFFER``.
2. **DIB Check** (``check_dib()``): Check the decoded instruction buffer for hits.
3. **Decode** (``decode_instruction()``): Move instructions from the fetch buffer
   through ``promote_to_decode()`` into the ``DECODE_BUFFER``.
4. **Dispatch** (``dispatch_instruction()``): Dispatch decoded instructions into
   the ``ROB``.
5. **Schedule** (``schedule_instruction()``): Select ready instructions for execution.
6. **Execute** (``execute_instruction()``): Execute instructions, including memory
   operations via ``operate_lsq()``.
7. **Complete** (``complete_inflight_instruction()``): Complete in-flight instructions
   and handle memory returns (``handle_memory_return()``).
8. **Retire** (``retire_rob()``): Retire completed instructions from the ``ROB``.

--------------------------------------
Internal Buffers
--------------------------------------

``IFETCH_BUFFER``
    Holds fetched instructions waiting to be decoded.

``DECODE_BUFFER``
    Holds instructions being decoded.

``DISPATCH_BUFFER``
    Holds decoded instructions waiting for dispatch into the ROB.

``ROB``
    Reorder buffer — tracks all in-flight instructions.

``DIB_HIT_BUFFER``
    Buffer for instructions that hit in the decoded instruction buffer.

``LQ``
    Load queue — tracks outstanding loads.

``SQ``
    Store queue — tracks outstanding stores.

--------------------------------------
Key Public Methods
--------------------------------------

Lifecycle hooks
^^^^^^^^^^^^^^^^^^

``initialize()``, ``begin_phase()``, ``end_phase()``
    Called by the simulation framework at appropriate lifecycle points.

``operate()``
    Main per-cycle function that drives all pipeline stages.

Statistics
^^^^^^^^^^^^

``sim_instr()`` / ``sim_cycle()`` / ``roi_instr()`` / ``roi_cycle()``
    Return instruction and cycle counts for simulation and region-of-interest phases.

``get_sim_stats()`` / ``get_roi_stats()``
    Return full performance counter structures.

--------------------------------------
Module Interface Hooks
--------------------------------------

The ``O3_CPU`` class delegates to its submodules through these internal hooks:

**Branch predictor hooks:**
    ``impl_initialize_branch_predictor()``, ``impl_predict_branch()``,
    ``impl_last_branch_result()``

**BTB hooks:**
    ``impl_initialize_btb()``, ``impl_update_btb()``, ``impl_btb_prediction()``

