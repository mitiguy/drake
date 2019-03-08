#pragma once

#include <memory>

#include "drake/geometry/scene_graph.h"
#include "drake/multibody/plant/multibody_plant.h"

namespace drake {
namespace multibody {
namespace benchmarks {
namespace block_on_inclined_plane_plant {

/// This method creates mass, geometry, and surface friction properties for a
/// uniform-density block B whose bottom surface makes contact with the
/// top-surface of an inclined-plane A.
/// Right-handed sets of orthonormal vectors Wx, Wy, Wz; Ax, Ay, Az; Bx, By, Bz;
/// are fixed in the World W, inclined-plane A, and block B, respectively.
/// Wz is directed vertically upward (opposite Earth's gravity).
/// The inclined-plane is oriented by initially setting Ax=Wx, Ay=Wy, Az=Wz, and
/// then subjecting A to a right-handed rotation in W about Ay=Wy, so that Ax
/// is directed downhill.
///
/// @param[in] LAx Length of inclined-plane A in the Ax-direction (meters).
/// @param[in] LAy Length of inclined-plane A in the Ay-direction (meters).
/// @param[in] LAz Length of inclined-plane A in the Az-direction (meters).
/// @param[in] LBx Length of block B in the Bx-direction (meters).
/// @param[in] LBy Length of block B in the By-direction (meters).
/// @param[in] LBz Length of block B in the Bz-direction (meters).
/// @param[in] mB Block B's mass (kilograms).
/// @param[in] slope The slope of inclined-plane A (radians).
/// @param[in] gravity The magnitude of Earth's local gravitational acceleration
///   in m/s².  Earth's gravity is in the World's -z direction.
/// @param[in] coefficient_of_friction_inclined_plane Coulomb's coefficient of
///   friction for inclined-plane A (sliding friction, static friction).
/// @param[in] coefficient_of_friction_block Coulomb's coefficient of friction
///   data for block B's surfaces (sliding friction, static friction).
/// @param[in] is_inclined_plane_half_space This is `true` if inclined-plane A's
///   contact with block B is modeled using a half-space whereas `false` means
///   inclined-plane A is modeled using a box whose dimensions scale with B.
/// @param[in] is_block_with_4Spheres This flag is `true` if block B's contact
///   with inclined-plane A is modeled using 4 identical massless spheres
///   welded to the block B's four "bottom" corners, whereas this flag is
///   `false`if block B's contact is modeled with a block (box).
/// @param[out] plant Plant that contains inclined-plane A and block B.
/// @throws std::exception if plant is nullptr or there is invalid data.
/// @pre plant must be registered with a scene graph.
void AddBlockAndInclinedPlaneToPlant(
    double LAx, double LAy, double LAz,
    double LBx, double LBy, double LBz, double mB, double slope, double gravity,
    const CoulombFriction<double>& coefficient_of_friction_blockB,
    const CoulombFriction<double>& coefficient_of_friction_inclined_plane,
    bool is_inclined_plane_half_space, bool is_block_with_4Spheres,
    MultibodyPlant<double>* plant);

}  // namespace block_on_inclined_plane_plant
}  // namespace benchmarks
}  // namespace multibody
}  // namespace drake
