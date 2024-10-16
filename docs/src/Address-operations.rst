.. _Address_operations:

=======================================
Address Operations
=======================================

-------------------------------------
Addresses
-------------------------------------

.. doxygenclass:: champsim::address_slice
   :members:

.. doxygenfunction:: champsim::offset
.. doxygenfunction:: champsim::uoffset
.. doxygenfunction:: champsim::splice

^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Convenience typedefs
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Champsim provides five specializations of an address slice in ``inc/champsim.h``

.. doxygentypedef:: champsim::address
.. doxygentypedef:: champsim::page_number
.. doxygentypedef:: champsim::page_offset
.. doxygentypedef:: champsim::block_number
.. doxygentypedef:: champsim::block_offset

--------------------------------------
Extents
--------------------------------------

.. doxygenstruct:: champsim::static_extent
   :members:
.. doxygenstruct:: champsim::dynamic_extent
   :members:
.. doxygenstruct:: champsim::page_number_extent
   :members:
.. doxygenstruct:: champsim::page_offset_extent
   :members:
.. doxygenstruct:: champsim::block_number_extent
   :members:
.. doxygenstruct:: champsim::block_offset_extent
   :members:
