# distutils: language = c++

# Impedance-plane conditioning for eddy-current data: null, rotation, gain.
# The implementation is in ec_condition_cpp.hpp; this exposes it to Python.

from cython.operator cimport dereference as deref
from libcpp.memory cimport shared_ptr
from libc.stdint cimport uintptr_t

import spatialnde2 as snde

from ec_condition_cpp cimport ec_condition_function as ec_condition_function_cpp

# The C++ side defines AND registers the function (see the note in the header
# about static initialization order); this hands the same shared_ptr to Python
# as a SWIG-wrapped object so a .dpi can instantiate it.
ec_condition_function = snde.math_function.from_raw_shared_ptr(<uintptr_t>&ec_condition_function_cpp)
