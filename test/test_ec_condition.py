"""Verify eddy-current impedance-plane conditioning: null, rotation, gain.

Runs without a robot, an instrument or a GUI. What it checks:

  * the null point conditions to exactly zero, which is the whole purpose of
    nulling and the one result that is unambiguous
  * rotation moves a known feature onto a known axis, in the direction an
    instrument's front panel means by it
  * the two gains scale their own axes and not each other's
  * RETUNING IS LIVE. The parameters are recordings rather than constants
    precisely so an operator can turn the rotation while watching the display;
    if that did not re-run the math on the existing traces, the design would be
    pointless and the whole thing may as well be a constant.
"""
import numpy as np
import spatialnde2 as snde

from spatialnde2_ec_functions import ec_condition_function


def _scalar_chan(trans, path, values):
    ch = trans.define_channel(path, "main")
    ref = snde.create_ndarray_ref(trans, ch, snde.SNDE_RTN_FLOAT64)
    ref.allocate_storage([len(values)], False)
    for i, v in enumerate(values):
        ref.data[i] = float(v)
    ref.rec.mark_data_and_metadata_ready()
    return ch


def _outputs(g):
    return (g.get_ndarray_ref("/ec/conditioned").data,
            g.get_ndarray_ref("/ec/horizontal").data,
            g.get_ndarray_ref("/ec/vertical").data,
            g.get_ndarray_ref("/ec/magnitude").data)


def build(recdb, raw, null, rotation, gain):
    trans = recdb.start_transaction()
    tch = trans.define_channel("/ec/traces", "main")
    tref = snde.create_ndarray_ref(trans, tch, snde.SNDE_RTN_COMPLEXFLOAT64)
    tref.allocate_storage([len(raw)], False)
    tref.data[:] = raw
    tref.rec.mark_data_and_metadata_ready()

    chans = {
        "null": _scalar_chan(trans, "/ec/null", null),
        "rotation": _scalar_chan(trans, "/ec/rotation", rotation),
        "gain": _scalar_chan(trans, "/ec/gain", gain),
    }
    fcn = ec_condition_function.instantiate(
        [snde.math_parameter_recording("/ec/traces"),
         snde.math_parameter_recording("/ec/null"),
         snde.math_parameter_recording("/ec/rotation"),
         snde.math_parameter_recording("/ec/gain")],
        [snde.shared_string("/ec/conditioned"), snde.shared_string("/ec/horizontal"),
         snde.shared_string("/ec/vertical"), snde.shared_string("/ec/magnitude")],
        "/", False, False, False,
        snde.math_definition("ec condition"), [], None)
    trans.add_math_function(fcn, False)
    g = trans.end_transaction().globalrev_wait()
    g.wait_complete()
    return chans, g


def retune(recdb, chan, values):
    """Write a new parameter value and wait for the math to catch up."""
    trans = recdb.start_transaction()
    ref = snde.create_ndarray_ref(trans, chan, snde.SNDE_RTN_FLOAT64)
    ref.allocate_storage([len(values)], False)
    for i, v in enumerate(values):
        ref.data[i] = float(v)
    ref.rec.mark_data_and_metadata_ready()
    g = trans.end_transaction().globalrev_wait()
    g.wait_complete()
    return g


def make_recdb():
    recdb = snde.recdatabase()
    snde.setup_cpu(recdb, [], 4)
    snde.setup_storage_manager(recdb)
    snde.setup_math_functions(recdb, [])
    recdb.startup()
    return recdb


RAW = np.array([2 + 3j, 3 + 3j, 2 + 4j, 3 + 4j], dtype=np.complex128)
NULL = (2.0, 3.0)


def test_null_and_rotation():
    recdb = make_recdb()
    _, g = build(recdb, RAW, NULL, (90.0,), (1.0, 1.0))
    cond, h, v, m = _outputs(g)

    # An instrument quoting "lift-off at 90 degrees" means rotate the data by
    # -90 to bring it onto the horizontal axis: multiply by exp(-i*90deg).
    expected = (RAW - (NULL[0] + 1j * NULL[1])) * np.exp(-1j * np.pi / 2)

    assert cond[0] == 0, "the null point must condition to exactly zero"
    assert np.allclose(cond, expected, atol=1e-5)
    assert np.allclose(h, expected.real, atol=1e-5)
    assert np.allclose(v, expected.imag, atol=1e-5)
    assert np.allclose(m, np.abs(expected), atol=1e-5)


def test_gain_is_per_axis():
    recdb = make_recdb()
    _, g = build(recdb, RAW, NULL, (0.0,), (2.0, 5.0))
    _, h, v, _ = _outputs(g)
    nulled = RAW - (NULL[0] + 1j * NULL[1])
    assert np.allclose(h, nulled.real * 2.0, atol=1e-5)
    assert np.allclose(v, nulled.imag * 5.0, atol=1e-5), \
        "vertical gain must not leak into horizontal or vice versa"


def test_rotation_retunes_live():
    """The point of parameters-as-recordings."""
    recdb = make_recdb()
    chans, g = build(recdb, RAW, NULL, (0.0,), (1.0, 1.0))
    before = _outputs(g)[0].copy()

    g2 = retune(recdb, chans["rotation"], (90.0,))
    after = _outputs(g2)[0]

    nulled = RAW - (NULL[0] + 1j * NULL[1])
    assert np.allclose(before, nulled, atol=1e-5)
    assert np.allclose(after, nulled * np.exp(-1j * np.pi / 2), atol=1e-5), \
        "changing the rotation channel must re-run the math on existing traces"


if __name__ == "__main__":
    test_null_and_rotation(); print("  null + rotation      OK")
    test_gain_is_per_axis(); print("  per-axis gain        OK")
    test_rotation_retunes_live(); print("  live retune          OK")
    print("all EC conditioning tests passed")
