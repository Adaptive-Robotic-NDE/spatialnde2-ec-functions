#ifndef SNDE_EC_CONDITION_HPP
#define SNDE_EC_CONDITION_HPP

// Impedance-plane conditioning for eddy-current data.
//
// An eddy-current instrument reports a complex impedance, and the raw value is
// not what anybody reads. Three operations sit between the coil and the display,
// and they are the same three on every instrument ever built:
//
//   NULL (balance)  subtract the reading taken on good material, so the display
//                   sits at the origin and what is left is the DEPARTURE from
//                   good material rather than the absolute impedance.
//   ROTATION        turn the plane so that lift-off -- the probe moving off the
//                   surface, which is always present and never the thing being
//                   measured -- lies along the horizontal axis. Everything that
//                   is not lift-off then shows up vertically. Choosing this
//                   angle IS the setup procedure for an eddy-current inspection.
//   GAIN            scale the two axes separately, because after rotation they
//                   carry different quantities and want different sensitivity.
//
// Only after those three does a number mean anything, which is why this is one
// function rather than something a caller is trusted to remember to do.
//
// WHY THIS IS COMPILED. Every parameter is a recording, so an operator can turn
// the rotation while watching the display and see it move -- the math re-runs on
// the whole trace list. That is thousands of traces per adjustment, and doing it
// in Python would hold the GIL for the duration of every single adjustment.
//
// THE OUTPUT THAT MATTERS IS SPATIAL. spatialnde2's phase-plane rendering draws
// the classic Lissajous figure but carries no position with it, so it cannot
// show WHERE on the part a signal came from -- and on a scan, where is the whole
// question. So this emits scalar channels (horizontal, vertical, magnitude)
// alongside the conditioned complex value, and those scalars are what gets
// projected onto the surface. The phase plane stays useful for setting the
// rotation; it is not the scan display.

#include <cmath>

#include "snde/recmath.hpp"
#include "snde/snde_error.hpp"
#include "snde/recmath_cppfunction.hpp"

namespace snde_ec {

  // Pull .real/.imag out of whichever complex struct T is. spatialnde2's
  // complex types are plain structs, not std::complex, so there is no
  // std::real() to lean on.
  // make_cppfuncexec_complextypes dispatches over complexfloat32,
  // complexfloat64 and compleximagedata; the third is a typedef of the first,
  // so these two specializations cover all of it.
  template <typename T> struct ec_component_type;
  template <> struct ec_component_type<snde_complexfloat32> { typedef snde_float32 type; };
  template <> struct ec_component_type<snde_complexfloat64> { typedef snde_float64 type; };

  template <typename T>
  class ec_condition: public snde::recmath_cppfuncexec<
    std::shared_ptr<snde::ndtyped_recording_ref<T>>,              // traces
    std::shared_ptr<snde::ndtyped_recording_ref<snde_float64>>,   // null: [re, im]
    std::shared_ptr<snde::ndtyped_recording_ref<snde_float64>>,   // rotation: [degrees]
    std::shared_ptr<snde::ndtyped_recording_ref<snde_float64>>>   // gain: [h, v]
  {
  public:
    typedef snde::recmath_cppfuncexec<
      std::shared_ptr<snde::ndtyped_recording_ref<T>>,
      std::shared_ptr<snde::ndtyped_recording_ref<snde_float64>>,
      std::shared_ptr<snde::ndtyped_recording_ref<snde_float64>>,
      std::shared_ptr<snde::ndtyped_recording_ref<snde_float64>>> base_type;

    typedef typename base_type::metadata_function_override_type metadata_function_override_type;
    typedef typename base_type::lock_alloc_function_override_type lock_alloc_function_override_type;
    typedef typename base_type::exec_function_override_type exec_function_override_type;

    ec_condition(std::shared_ptr<snde::recording_set_state> rss,
                 std::shared_ptr<snde::instantiated_math_function> inst) :
      base_type(rss,inst)
    {
    }

    std::shared_ptr<metadata_function_override_type>
    define_recs(std::shared_ptr<snde::ndtyped_recording_ref<T>> traces,
                std::shared_ptr<snde::ndtyped_recording_ref<snde_float64>> null_ref,
                std::shared_ptr<snde::ndtyped_recording_ref<snde_float64>> rotation_ref,
                std::shared_ptr<snde::ndtyped_recording_ref<snde_float64>> gain_ref)
    {
      snde::snde_debug(SNDE_DC_APP,"ec_condition define_recs()");

      // Four outputs. The complex one feeds the phase plane; the three scalars
      // are what a spatial plot can actually use.
      std::shared_ptr<snde::ndtyped_recording_ref<T>> conditioned =
        snde::create_typed_ndarray_ref_math<T>(this->get_result_channel_path(0),this->rss);
      std::shared_ptr<snde::ndtyped_recording_ref<snde_float32>> horizontal =
        snde::create_typed_ndarray_ref_math<snde_float32>(this->get_result_channel_path(1),this->rss);
      std::shared_ptr<snde::ndtyped_recording_ref<snde_float32>> vertical =
        snde::create_typed_ndarray_ref_math<snde_float32>(this->get_result_channel_path(2),this->rss);
      std::shared_ptr<snde::ndtyped_recording_ref<snde_float32>> magnitude =
        snde::create_typed_ndarray_ref_math<snde_float32>(this->get_result_channel_path(3),this->rss);

      return std::make_shared<metadata_function_override_type>([ this,traces,null_ref,rotation_ref,gain_ref,conditioned,horizontal,vertical,magnitude ]() {

        // The conditioned complex channel is the one worth handing to the phase
        // plane, so it is stamped for it here rather than by the caller.
        std::unordered_map<std::string,snde::metadatum> cmd;
        cmd.emplace("snde_render_goal",snde::metadatum("snde_render_goal","SNDE_SRG_PHASEPLANE"));
        cmd.emplace("snde_phase_plane_historical_fade",snde::metadatum_bool("snde_phase_plane_historical_fade",true));
        cmd.emplace("nde_array-ampl_coord",snde::metadatum("nde_array-ampl_coord","Impedance"));
        cmd.emplace("nde_array-ampl_units",snde::metadatum("nde_array-ampl_units","Volts"));
        conditioned->rec->metadata=std::make_shared<snde::immutable_metadata>(cmd);
        conditioned->rec->mark_metadata_done();

        // The scalars are per-trace values to be laid out in SPACE by whatever
        // projects them, so they carry no render goal of their own.
        for (auto &&scalar_and_name : std::vector<std::pair<std::shared_ptr<snde::ndtyped_recording_ref<snde_float32>>,std::string>>{
            {horizontal,"Horizontal"},{vertical,"Vertical"},{magnitude,"Magnitude"}}) {
          std::unordered_map<std::string,snde::metadatum> smd;
          smd.emplace("nde_array-ampl_coord",snde::metadatum("nde_array-ampl_coord",scalar_and_name.second));
          smd.emplace("nde_array-ampl_units",snde::metadatum("nde_array-ampl_units","Volts"));
          scalar_and_name.first->rec->metadata=std::make_shared<snde::immutable_metadata>(smd);
          scalar_and_name.first->rec->mark_metadata_done();
        }

        return std::make_shared<lock_alloc_function_override_type>([ this,traces,null_ref,rotation_ref,gain_ref,conditioned,horizontal,vertical,magnitude ]() {

          conditioned->allocate_storage(traces->layout.dimlen);
          horizontal->allocate_storage(traces->layout.dimlen);
          vertical->allocate_storage(traces->layout.dimlen);
          magnitude->allocate_storage(traces->layout.dimlen);

          snde::rwlock_token_set locktokens = this->lockmgr->lock_recording_refs({
              { traces, false },
              { null_ref, false },
              { rotation_ref, false },
              { gain_ref, false },
              { conditioned, true },
              { horizontal, true },
              { vertical, true },
              { magnitude, true },
            });

          return std::make_shared<exec_function_override_type>([ this,locktokens,traces,null_ref,rotation_ref,gain_ref,conditioned,horizontal,vertical,magnitude ]() {

            typedef typename ec_component_type<T>::type CT;

            // A missing or short parameter array means "not set", and the
            // identity value is the right answer -- an unconditioned display is
            // wrong but readable, whereas refusing to compute leaves the
            // operator with nothing to tune against.
            snde_float64 null_re = (null_ref->layout.flattened_length() > 0) ? null_ref->element(0) : 0.0;
            snde_float64 null_im = (null_ref->layout.flattened_length() > 1) ? null_ref->element(1) : 0.0;
            snde_float64 rot_deg = (rotation_ref->layout.flattened_length() > 0) ? rotation_ref->element(0) : 0.0;
            snde_float64 gain_h  = (gain_ref->layout.flattened_length() > 0) ? gain_ref->element(0) : 1.0;
            snde_float64 gain_v  = (gain_ref->layout.flattened_length() > 1) ? gain_ref->element(1) : 1.0;

            // Rotating the DATA by -theta is what puts a feature that currently
            // lies at +theta onto the horizontal axis, which is how the angle is
            // quoted on an instrument: "lift-off at 40 degrees" means rotate by
            // -40 to flatten it.
            const snde_float64 theta = -rot_deg * M_PI / 180.0;
            const snde_float64 ct = std::cos(theta);
            const snde_float64 st = std::sin(theta);

            snde_float64 scale,offset;
            std::tie(scale,offset)=traces->get_ampl_scale_offset();

            snde_index len = traces->layout.flattened_length();
            for (snde_index pos=0; pos < len; pos++) {
              const T &raw = traces->element(pos);

              // scale/offset first: they describe the stored integer-to-volts
              // relationship and everything below is in volts.
              snde_float64 re = (snde_float64)raw.real * scale + offset;
              snde_float64 im = (snde_float64)raw.imag * scale + offset;

              // null, then rotate, then gain. The order is not interchangeable:
              // rotating before nulling turns the null point itself into a
              // moving offset, which is the classic way to get a display that
              // drifts as the rotation is adjusted.
              re -= null_re;
              im -= null_im;

              snde_float64 h = re*ct - im*st;
              snde_float64 v = re*st + im*ct;

              h *= gain_h;
              v *= gain_v;

              T out;
              out.real = (CT)h;
              out.imag = (CT)v;
              conditioned->element(pos) = out;
              horizontal->element(pos) = (snde_float32)h;
              vertical->element(pos) = (snde_float32)v;
              magnitude->element(pos) = (snde_float32)std::sqrt(h*h + v*v);
            }

            snde::unlock_rwlock_token_set(locktokens);

            conditioned->rec->mark_data_ready();
            horizontal->rec->mark_data_ready();
            vertical->rec->mark_data_ready();
            magnitude->rec->mark_data_ready();
          });
        });
      });
    }
  };

  std::shared_ptr<snde::math_function> define_ec_condition()
  {
    return std::make_shared<snde::cpp_math_function>(
      "spatialnde2_ec_functions.ec_condition_function",4,
      [] (std::shared_ptr<snde::recording_set_state> rss,
          std::shared_ptr<snde::instantiated_math_function> inst) {
        return snde::make_cppfuncexec_complextypes<ec_condition>(rss,inst);
      });
  }

  // Must be defined in this header, not linked in, or C++ static
  // initialization order decides whether the pointer is valid when Cython
  // takes its address.
  static std::shared_ptr<snde::math_function> ec_condition_function=define_ec_condition();

  static int registered_ec_condition_function = register_math_function(ec_condition_function);

};

#endif // SNDE_EC_CONDITION_HPP
