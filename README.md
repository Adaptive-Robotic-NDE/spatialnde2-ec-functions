# spatialnde2-ec-functions

Impedance-plane conditioning for eddy-current data, as a compiled spatialnde2
math function.

## What it does

An eddy-current instrument reports a complex impedance, and the raw value is not
what anybody reads. Three operations sit between the coil and the display, and
they are the same three on every instrument ever built:

| | |
|---|---|
| **null** (balance) | subtract the reading taken on good material, so what is left is the *departure* from good material rather than the absolute impedance |
| **rotation** | turn the plane so lift-off — the probe moving off the surface, always present and never the thing being measured — lies along the horizontal axis. Everything that is not lift-off then shows up vertically. **Choosing this angle is the setup procedure** for an eddy-current inspection. |
| **gain** | scale the two axes separately, because after rotation they carry different quantities |

Order matters and is not interchangeable. Rotating before nulling turns the null
point itself into a moving offset, which is the classic way to get a display that
drifts as you adjust the rotation.

## Why it is compiled

Every parameter is a *recording*, not a constant, so an operator can turn the
rotation while watching the display and see it move — the math re-runs over the
whole trace list. That is thousands of traces per adjustment, and doing it in
Python would hold the GIL for the duration of every single adjustment.
spatialnde2 has no Python math-function support for exactly this reason.

Built on the pattern in `spatialnde2/spatialnde2_example_external_cpp_function`.

## The output that matters is spatial

spatialnde2's phase-plane rendering draws the classic Lissajous figure but
carries no position with it, so it cannot show *where* on the part a signal came
from — and on a scan, where is the whole question.

So this emits three scalar channels alongside the conditioned complex value:

```
/…/conditioned   complex, stamped for the phase plane — use it to SET the rotation
/…/horizontal    float32, the lift-off axis after rotation
/…/vertical      float32, the defect axis after rotation
/…/magnitude     float32
```

The scalars are what gets projected onto the surface for the spatial plot. The
phase plane stays useful for setting up; it is not the scan display.

## Building

Needs spatialnde2 installed first — `setup.py` reads the compile flags out of the
installed package.

```bash
python3 setup.py build_ext --inplace
```

**On the ROS container (Ubuntu 22.04, Python 3.10)** prefix that with
`SETUPTOOLS_USE_DISTUTILS=stdlib`. It is not optional there: newer setuptools
removed `distutils.msvccompiler`, which `numpy.distutils` imports
unconditionally, and the build dies in numpy before reaching any of this code.

**Do not set it on Python 3.12 or newer** — 3.12 dropped `distutils` from the
stdlib, so `SETUPTOOLS_USE_DISTUTILS=stdlib` makes `import setuptools` itself
fail with `ModuleNotFoundError: No module named 'distutils'`. On the Arch host
(Python 3.14) the plain command above is the one that works; verified
2026-09-15 against spatialnde2 0.8.2.dev115, and the test below passes on the
result.

Two things make an in-place build enough, with no install step:

* ROS-Scanning's host-side EC tests look for a built copy here when the package
  is not installed in the interpreter running them (see
  `ROS-Scanning/dgpfiles/modules/ec_test_support.py`).
* the extension filename carries the interpreter's ABI tag
  (`ec_condition.cpython-314-…so`), and setuptools' `build/` subdirectories are
  tagged the same way, so a host build and a container build of this directory
  cannot pick up each other's objects — unlike a shared CMake cache.

For a real install — which is what a machine that *runs* eddy-current scans
wants — it is an optional extra of the workspace installer (`--with-extras`
installs every entry in `EXTRAS`; `--only` selects from the main package list
and cannot name an extra):

```bash
./dgpy_install.sh --with-extras
```

On a distribution that marks its Python externally managed (PEP 668 — Arch, and
Ubuntu 23.04+) pip will refuse to write to the system interpreter, and the
script says so. Point it at an interpreter you own, or build in place as above:

```bash
./dgpy_install.sh --with-extras --python /path/to/venv/bin/python
```

## Testing

```bash
PYTHONPATH=. python3 test/test_ec_condition.py
```

No robot, no instrument, no GUI. Checks that the null point conditions to exactly
zero, that rotation moves a known feature onto a known axis in the direction an
instrument's front panel means by it, that the two gains do not leak into each
other, and that **retuning is live** — which is the entire reason the parameters
are recordings.
