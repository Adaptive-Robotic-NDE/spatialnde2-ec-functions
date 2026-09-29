from libcpp.memory cimport shared_ptr

from spatialnde2.recmath cimport math_function


cdef extern from "ec_condition_cpp.hpp" namespace "snde_ec" nogil:
    cdef shared_ptr[math_function] define_ec_condition()
    cdef shared_ptr[math_function] ec_condition_function
    pass
