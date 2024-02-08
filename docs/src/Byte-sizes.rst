.. Byte Sizes:

===========================================
Byte Sizes
===========================================

ChampSim provides a strong type to represent a unit of byte size.
Objects of this type are convertible to other byte sizes, but are more difficult to unintentionally use in arithmetic operations.

.. doxygenclass:: champsim::data::size
   :members:

-----------------------------------------
Convenience specializations
-----------------------------------------

ChampSim provides a number of ratio specializations for use in sizes:

.. doxygentypedef:: champsim::kibi
.. doxygentypedef:: champsim::mebi
.. doxygentypedef:: champsim::gibi
.. doxygentypedef:: champsim::tebi
.. doxygentypedef:: champsim::pebi
.. doxygentypedef:: champsim::exbi

These types are also used in the convenience specializations:

.. doxygentypedef:: champsim::data::bytes
.. doxygentypedef:: champsim::data::kibibytes
.. doxygentypedef:: champsim::data::mebibytes
.. doxygentypedef:: champsim::data::gibibytes
.. doxygentypedef:: champsim::data::tebibytes
