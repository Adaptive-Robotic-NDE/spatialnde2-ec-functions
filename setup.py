import sys
import os
import os.path
import numpy as np
from setuptools import setup,Extension
from Cython.Build import cythonize
import spatialnde2 as snde

def fixup_cmake_libs(libs):
    ret_libs = []
    ret_explicit = []
    for lib in libs:
        if lib.startswith("-l"):
            ret_libs.append(lib[2:])
            pass
        elif "/" in lib or "\\" in lib:
            ret_explicit.append(lib)
            pass
        else:
            ret_libs.append(lib)
            pass
        pass
    return (ret_explicit,ret_libs)

spatialnde2_compile_configuration = open(os.path.join(os.path.dirname(snde.__file__),"compile_configuration.txt")).read().strip().split(" ")
spatialnde2_compile_definitions = open(os.path.join(os.path.dirname(snde.__file__),"compile_definitions.txt")).read().strip().split(" ")
spatialnde2_compile_include_dirs = open(os.path.join(os.path.dirname(snde.__file__),"compile_include_dirs.txt")).read().strip().split(" ")
spatialnde2_link_configuration = open(os.path.join(os.path.dirname(snde.__file__),"link_configuration.txt")).read().strip().split(" ")
spatialnde2_compile_library_dirs = open(os.path.join(os.path.dirname(snde.__file__),"compile_library_dirs.txt")).read().strip().split(" ")
spatialnde2_compile_libraries_cmake = open(os.path.join(os.path.dirname(snde.__file__),"compile_libraries.txt")).read().strip().split(" ")

(spatialnde2_compile_explicit_libraries,spatialnde2_compile_libraries) = fixup_cmake_libs(spatialnde2_compile_libraries_cmake)

extra_compile_args = ["-O0"] +  spatialnde2_compile_configuration + spatialnde2_compile_definitions
extra_link_args= spatialnde2_link_configuration + spatialnde2_compile_explicit_libraries

debug=False
if debug:
    if sys.platform.startswith("win"):
        extra_compile_args += ["/Ox","/Zi"]
        extra_link_args += ["/debug:full"]
        pass
    else:
        extra_compile_args += ["-O0","-g"]
        extra_link_args += ["-g"]
        pass
    pass

ext_modules=cythonize(
    Extension("spatialnde2_ec_functions.ec_condition",
              sources=["spatialnde2_ec_functions/ec_condition.pyx" ],
              include_dirs=[os.path.dirname(snde.__file__)] + spatialnde2_compile_include_dirs,
              
              library_dirs=[os.path.dirname(snde.__file__)] + spatialnde2_compile_library_dirs,
              extra_compile_args = extra_compile_args,
              extra_link_args = extra_link_args,
              libraries=["spatialnde2"]  + spatialnde2_compile_libraries,
              undef_macros = ["NDEBUG"]
              ),include_path=["spatialnde2_ec_functions"])

setup(name="spatialnde2_ec_functions",
            description="Eddy-current impedance-plane conditioning for spatialnde2",
            author="Tyler Lesthaeghe",
            url="https://github.com/Adaptive-Robotic-NDE",
            ext_modules=ext_modules,
            packages=["spatialnde2_ec_functions"])
