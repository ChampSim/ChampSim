.. _Configuration_API:

=====================================
Configuration API
=====================================

.. automodule:: config

------------------------
Parsing API
------------------------

.. autofunction:: config.parse.parse_config


------------------------
File Generation API
------------------------

The file generation API contains two interfaces: a high-level interface with :py:class:`config.filewrite.FileWriter`, and a low-level interface with :py:class:`config.filewrite.Fragment`.
Users should prefer the high-level interface where possible.
The low-level interface may provide greater flexibility when needed, for example a more parallel application.

.. autoclass:: config.filewrite.FileWriter
   :members:
   :special-members: __enter__, __exit__

.. autoclass:: config.filewrite.Fragment
   :members:

--------------------------
Utility Functions
--------------------------

^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
System operations
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

ChampSim's configuration makes frequent use of sequences of dictionaries. The following functions operate on a system, a dictionary whose values are dictionaries.

.. autofunction:: config.util.iter_system
.. autofunction:: config.util.combine_named
.. autofunction:: config.util.upper_levels_for
.. autofunction:: config.util.propogate_down

^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Itertools extentions
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The following functions are extentions of the itertools package.

.. autofunction:: config.util.collect
.. autofunction:: config.util.batch
.. autofunction:: config.util.sliding
.. autofunction:: config.util.cut
.. autofunction:: config.util.do_for_first
.. autofunction:: config.util.append_except_last
.. autofunction:: config.util.multiline
.. autofunction:: config.util.yield_from_star

^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Dictionary Operations
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

ChampSim frequently operates on dictionaries, so these functions are provided as convenience functions.

.. autofunction:: config.util.chain
.. autofunction:: config.util.subdict
.. autofunction:: config.util.extend_each
.. autofunction:: config.util.explode
.. autofunction:: config.parse.duplicate_to_length
.. autofunction:: config.parse.extract_element

