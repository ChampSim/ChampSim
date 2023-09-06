.. _Modules:

=============================
The ChampSim Module System
=============================

ChampSim uses four kinds of modules:

* Branch Direction Predictors
* Branch Target Predictors
* Memory Prefetchers
* Cache Replacement Policies

Each of these is implemented as a set of hook functions. Each hook must be implemented, or compilation will fail.

----------------------------
Branch Predictors
----------------------------

A branch predictor module must implement three functions.

::

  void O3_CPU::initialize_branch_predictor()

This function is called when the core is initialized. You can use it to initialize elements of dynamic structures, such as `std::vector` or `std::map`.

::

  uint8_t O3_CPU::predict_branch(uint64_t ip, uint64_t predicted_target, uint8_t always_taken, uint8_t branch_type)

This function is called when a prediction is needed. The parameters passed are:

* ip: The instruction pointer of the branch
* predicted_target: The predicted target of the branch. This is passed directly from the branch target predictor module and may be incorrect.
* always_taken: A boolean value. This parameter will be nonzero if the branch target predictor determines that the branch is always taken.
* branch_type: One of the following

  * `BRANCH_DIRECT_JUMP`: Direct non-call, unconditional branches, whose target is encoded in the instruction
  * `BRANCH_INDIRECT`: Indirect non-call, unconditional branches, whose target is stored in a register
  * `BRANCH_CONDITIONAL`: A direct conditional branch
  * `BRANCH_DIRECT_CALL`: A call to a procedure whose target is encoded in the instruction
  * `BRANCH_INDIRECT_CALL`: A call to a procedure whose target is stored in a register
  * `BRANCH_RETURN`: A return to a calling procedure
  * `BRANCH_OTHER`: If the branch type cannot be determined

::

  void O3_CPU::last_branch_result(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type)


This function is called when a branch is resolved. The parameters are the same as in the previous hook, except that the last three are guaranteed to be correct.

-----------------------------------
Branch Target Buffers
-----------------------------------

A BTB module must implement three functions.

::

  void O3_CPU::initialize_btb()

This function is called when the core is initialized. You can use it to initialize elements of dynamic structures, such as `std::vector` or `std::map`.

::

  std::pair<uint64_t, bool> O3_CPU::btb_prediction(uint64_t ip)

This function is called when a prediction is needed. The parameters passed are:

* ip: The instruction pointer of the branch

The function should return a pair containing the predicted address and a boolean that describes if the branch is known to be always taken. If the prediction fails, the function should return a default-initialized address, e.g. `uint64_t{}`.

::

  void O3_CPU::update_btb(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type)


This function is called when a branch is resolved. The parameters are:

* ip: The instruction pointer of the branch
* branch_target: The correct target of the branch.
* taken: A boolean value. This parameter will be nonzero if the branch was taken.
* branch_type: One of the following

  * `BRANCH_DIRECT_JUMP`: Direct non-call, unconditional branches, whose target is encoded in the instruction
  * `BRANCH_INDIRECT`: Indirect non-call, unconditional branches, whose target is stored in a register
  * `BRANCH_CONDITIONAL`: A direct conditional branch
  * `BRANCH_DIRECT_CALL`: A call to a procedure whose target is encoded in the instruction
  * `BRANCH_INDIRECT_CALL`: A call to a procedure whose target is stored in a register
  * `BRANCH_RETURN`: A return to a calling procedure
  * `BRANCH_OTHER`: If the branch type cannot be determined

