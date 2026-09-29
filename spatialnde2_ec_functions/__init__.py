# Importing spatialnde2 FIRST is required, not tidiness.
#
# The extension links against libspatialnde2.so, which lives inside the
# spatialnde2 package directory and is not on any default library search path.
# Nothing sets an RPATH -- neither this package nor the upstream example it is
# modelled on -- so the dynamic loader can only find it if it is already mapped
# into the process, which importing spatialnde2 is what does.
#
# It cannot be fixed in ec_condition.pyx: that file's own `import spatialnde2`
# is Python-level code that runs AFTER the .so is dlopen'd, which is already too
# late. Without the line below you get, at import time:
#
#     ImportError: libspatialnde2.so: cannot open shared object file
#
# which names the library rather than the cause and sends people looking at
# LD_LIBRARY_PATH.
import spatialnde2 as _snde  # noqa: F401

from .ec_condition import ec_condition_function
