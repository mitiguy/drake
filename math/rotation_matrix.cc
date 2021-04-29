#include "drake/math/rotation_matrix.h"

#include <string>

#include <fmt/format.h>

#include "drake/common/unused.h"

namespace drake {
namespace math {

template <typename T>
void RotationMatrix<T>::ThrowIfNotValid(const Matrix3<T>& R) {
  if constexpr (scalar_predicate<T>::is_bool) {
    if (!R.allFinite()) {
      throw std::logic_error(
          "Error: Rotation matrix contains an element that is infinity or"
          " NaN.");
    }
    // If the matrix is not-orthogonal, try to give a detailed message.
    // This is particularly important if matrix is very-near orthogonal.
    if (!IsOrthonormal(R, get_internal_tolerance_for_orthonormality())) {
      const T measure_of_orthonormality = GetMeasureOfOrthonormality(R);
      const double measure = ExtractDoubleOrThrow(measure_of_orthonormality);
      std::string message = fmt::format(
          "Error: Rotation matrix is not orthonormal.\n"
          "  Measure of orthonormality error: {}  (near-zero is good).\n"
          "  To calculate the proper orthonormal rotation matrix closest to"
          " the alleged rotation matrix, use the SVD (expensive) static method"
          " RotationMatrix<T>::ProjectToRotationMatrix(), or for a less"
          " expensive (but not necessarily closest) rotation matrix, use"
          " RotationMatrix<T>(RotationMatrix<T>::ToQuaternion<T>(your_matrix))."
          " Alternatively, if using quaternions, ensure the quaternion is"
          " normalized.", measure);
      throw std::logic_error(message);
    }
    if (R.determinant() < 0) {
      throw std::logic_error(
          "Error: Rotation matrix determinant is negative."
          " It is possible a basis is left-handed.");
    }
  } else {
    unused(R);
  }
}

double ProjectMatToRotMatWithAxis(const Eigen::Matrix3d& M,
                                  const Eigen::Vector3d& axis,
                                  const double angle_lb,
                                  const double angle_ub) {
  if (angle_ub < angle_lb) {
    throw std::runtime_error(
        "The angle upper bound should be no smaller than the angle lower "
        "bound.");
  }
  const double axis_norm = axis.norm();
  if (axis_norm == 0) {
    throw std::runtime_error("The axis argument cannot be the zero vector.");
  }
  const Eigen::Vector3d a = axis / axis_norm;
  Eigen::Matrix3d A;
  // clang-format off
  A <<    0,  -a(2),   a(1),
       a(2),      0,  -a(0),
      -a(1),   a(0),      0;
  // clang-format on
  const double alpha =
      atan2(-(M.transpose() * A * A).trace(), (A.transpose() * M).trace());
  double theta{};
  // The bounds on θ + α is [angle_lb + α, angle_ub + α].
  if (std::isinf(angle_lb) && std::isinf(angle_ub)) {
    theta = M_PI_2 - alpha;
  } else if (std::isinf(angle_ub)) {
    // First if the angle upper bound is inf, start from the angle_lb, and
    // find the angle θ, such that θ + α = 0.5π + 2kπ
    const int k = ceil((angle_lb + alpha - M_PI_2) / (2 * M_PI));
    theta = (2 * k + 0.5) * M_PI - alpha;
  } else if (std::isinf(angle_lb)) {
    // If the angle lower bound is inf, start from the angle_ub, and find the
    // angle θ, such that θ + α = 0.5π + 2kπ
    const int k = floor((angle_ub + alpha - M_PI_2) / (2 * M_PI));
    theta = (2 * k + 0.5) * M_PI - alpha;
  } else {
    // Now neither angle_lb nor angle_ub is inf. Check if there exists an
    // integer k, such that 0.5π + 2kπ ∈ [angle_lb + α, angle_ub + α]
    const int k = floor((angle_ub + alpha - M_PI_2) / (2 * M_PI));
    const double max_sin_angle = M_PI_2 + 2 * k * M_PI;
    if (max_sin_angle >= angle_lb + alpha) {
      // 0.5π + 2kπ ∈ [angle_lb + α, angle_ub + α]
      theta = max_sin_angle - alpha;
    } else {
      // Now the maximal is at the boundary, either θ = angle_lb or angle_ub
      if (sin(angle_lb + alpha) >= sin(angle_ub + alpha)) {
        theta = angle_lb;
      } else {
        theta = angle_ub;
      }
    }
  }
  return theta;
}

template <typename T>
void RotationMatrix<T>::ThrowUnlessVectorMagnitudeIsBigEnough(
    const Vector3<T>& v, const char* function_name, double min_magnitude) {
  if constexpr (scalar_predicate<T>::is_bool) {
    ThrowIfVectorContainsNonFinite(v, function_name);
    const T v_norm_as_T = v.norm();
    const double v_norm = ExtractDoubleOrThrow(v_norm_as_T);
    if (v_norm < min_magnitude) {
      const double vx = ExtractDoubleOrThrow(v(0));
      const double vy = ExtractDoubleOrThrow(v(1));
      const double vz = ExtractDoubleOrThrow(v(2));
      const std::string message = fmt::format(
          "RotationMatrix::{}(). The vector {} {} {} with magnitude {},"
          " is smaller than the required minimum value {}. "
          " If you are confident that this vector v's direction is"
          " meaningful, pass v.normalized() in place of v.",
          function_name, vx, vy, vz, v_norm, min_magnitude);
      throw std::logic_error(message);
    }
  } else {
    unused(v, function_name, min_magnitude);
  }
}

template <typename T>
void RotationMatrix<T>::ThrowIfVectorContainsNonFinite(
    const Vector3<T>& v, const char* function_name) {
  if constexpr (scalar_predicate<T>::is_bool) {
    if (!v.allFinite()) {
      const double vx = ExtractDoubleOrThrow(v(0));
      const double vy = ExtractDoubleOrThrow(v(1));
      const double vz = ExtractDoubleOrThrow(v(2));
      const std::string message = fmt::format(
          "RotationMatrix::{}() was passed an invalid vector argument.  There"
          " is a NaN or infinity in the vector {} {} {}.",
          function_name, vx, vy, vz);
      throw std::runtime_error(message);
    }
  } else {
    unused(v, function_name);
  }
}

template <typename T>
void RotationMatrix<T>::ThrowIfInvalidUnitVector(const Vector3<T>& u,
    double tolerance, const char* function_name) {
  // Throw a nicely worded exception if u is not a unit vector because
  // u contains a NAN element or u is a zero vector.
  ThrowIfVectorContainsNonFinite(u, function_name);

  // Skip symbolic expressions.
  // TODO(Mitiguy) This is a generally-useful method.  Consider moving it
  //  into public view in an appropriate file and also deal with symbolic
  //  expressions that can be easily evaluated to a number, e.g., consider:
  //  ThrowIfInvalidUnitVector(Vector3<symbolic::Expression> u_sym(3, 2, 1));
  if constexpr (scalar_predicate<T>::is_bool) {
    // Give a detailed message if |u| is not within tolerance of 1.
    const T u_norm_as_T = u.norm();
    const double u_norm = ExtractDoubleOrThrow(u_norm_as_T);
    const double abs_deviation = std::abs(1.0 - u_norm);
    if (abs_deviation > tolerance) {
      const double ux = ExtractDoubleOrThrow(u(0));
      const double uy = ExtractDoubleOrThrow(u(1));
      const double uz = ExtractDoubleOrThrow(u(2));
      const std::string message = fmt::format(
          "RotationMatrix::{}(). Vector is not a unit vector."
          " The magnitude of vector {} {} {} deviates from 1."
          " The vector's actual magnitude is {}."
          " Its deviation from 1 is {}."
          " The allowable tolerance (deviation) is {}."
          " To normalize a vector u, consider using u.normalized().",
          function_name, ux, uy, uz, u_norm, abs_deviation, tolerance);
      throw std::logic_error(message);
    }
  } else {
    drake::unused(tolerance);
  }
}

}  // namespace math
}  // namespace drake

DRAKE_DEFINE_CLASS_TEMPLATE_INSTANTIATIONS_ON_DEFAULT_SCALARS(
    class ::drake::math::RotationMatrix)
