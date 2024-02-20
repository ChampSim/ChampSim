.. _Module_support_library:

=========================================
Module Support Library
=========================================

-----------------------------------------
Fixed-width saturating counter
-----------------------------------------

.. doxygenclass:: champsim::msl::base_fwcounter
   :members:

.. doxygentypedef:: champsim::msl::fwcounter
.. doxygentypedef:: champsim::msl::sfwcounter

------------------------------------------
Functions for bit operations
------------------------------------------

.. doxygenfunction:: champsim::msl::lg2
.. doxygenfunction:: champsim::msl::next_pow2
.. doxygenfunction:: champsim::msl::is_power_of_2
.. doxygenfunction:: champsim::msl::ipow
.. doxygenfunction:: champsim::msl::bitmask(champsim::data::bits, champsim::data::bits)
.. doxygenfunction:: champsim::msl::bitmask(champsim::data::bits)
.. doxygenfunction:: champsim::msl::bitmask(std::size_t, std::size_t)
.. doxygenfunction:: champsim::msl::splice_bits(T, T, champsim::data::bits, champsim::data::bits)
.. doxygenfunction:: champsim::msl::splice_bits(T, T, champsim::data::bits)
.. doxygenfunction:: champsim::msl::splice_bits(T, T, std::size_t, std::size_t)
.. doxygenfunction:: champsim::msl::splice_bits(T, T, std::size_t)
