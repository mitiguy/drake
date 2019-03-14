#include <memory>

#include <gflags/gflags.h>

#include "drake/common/text_logging_gflags.h"
#include "drake/geometry/geometry_visualization.h"
#include "drake/geometry/scene_graph.h"
#include "drake/lcm/drake_lcm.h"
#include "drake/multibody/benchmarks/inclined_plane_with_block/inclined_plane_with_block_plant.h"
#include "drake/multibody/plant/contact_results_to_lcm.h"
#include "drake/systems/analysis/simulator.h"
#include "drake/systems/framework/diagram_builder.h"

namespace drake {
namespace multibody {
namespace examples {
namespace inclined_plane_with_block {
namespace {

// To visualize this example, open a terminal, change to the drake directory and
// type something like the following at the operating system command prompt.
// ./bazel-bin/tools/drake_visualizer &
//
// Open a new terminal, change to the drake directory, and compile/run this
// example with its default parameters by typing (all on one command line)
// bazel run
// examples/multibody/inclined_plane_with_block:inclined_plane_with_block
//
// To run this example with different parameters, run the binary executable and
// pass command line arguments to the executable by typing something like
//
// bazel run
// examples/multibody/inclined_plane_with_block:inclined_plane_with_block --
// --target_realtime_rate=0.5 --simulation_time=2.8 --time_step=1.0E-4
// --penetration_allowance=1.0E-4 --muS_block=0.1 --muK_block=0.1
// --slope_degrees=45
// --is_inclined_plane_half_space=true --is_block_with_4Spheres=true
DEFINE_double(target_realtime_rate, 1.0,
              "Desired rate relative to real time.  See documentation for "
              "Simulator::set_target_realtime_rate() for details.");
DEFINE_double(simulation_time, 2.0, "Simulation duration in seconds");
DEFINE_double(time_step, 1.0E-5,
              "If zero, the plant is modeled as a continuous system. "
              "If positive, the period (in seconds) of the discrete updates "
              "for the plant modeled as a discrete system."
              "This parameter must be non-negative.");
DEFINE_double(penetration_allowance, 1.E-5, "Contact penetration allowance.");
DEFINE_double(muS_block, 0.3, "Block static friction coefficient.");
DEFINE_double(muK_block, 0.3, "Block kinetic friction coefficient.");
DEFINE_double(muS_inclined_plane, 0.3, "Inclined-plane static friction coef.");
DEFINE_double(muK_inclined_plane, 0.3, "Inclined-plane kinetic friction coef.");
DEFINE_double(slope_degrees, 15.0, "Inclined plane angle in degrees.");
DEFINE_bool(is_inclined_plane_half_space, true,
            "Is inclined-plane a half-space (true) or box (false).");
DEFINE_bool(is_block_with_4Spheres, true,
            "Is block B's contacting surface 4 spheres (true) or box (false).");

using drake::multibody::MultibodyPlant;

int do_main() {
  const double LBx = 0.4;      // Block B's length in Bx-direction (meters).
  const double LBy = 0.2;      // Block B's length in By-direction (meters).
  const double LBz = 0.04;     // Block B's length in Bz-direction (meters).
  const double LAx = 8 * LBx;  // Inclined-plane A's length in Ax direction.
  const double LAy = 8 * LBy;  // Inclined-plane A's length in Ay direction.
  const double LAz = 0.04;     // Inclined-plane A's length in Az direction.
  const double mass = 0.1;     // Block B's mass (kg).
  const double gravity = 9.8;  // Earth's gravitational acceleration (m/s^2).
  const double slope_radians = FLAGS_slope_degrees / 180 * M_PI;
  const drake::multibody::CoulombFriction<double>
      coefficient_friction_block(FLAGS_muS_block,
                                 FLAGS_muK_block);
  const drake::multibody::CoulombFriction<double>
      coefficient_friction_inclined_plane(FLAGS_muS_inclined_plane,
                                          FLAGS_muK_inclined_plane);

  // Build the multibody plant.
  systems::DiagramBuilder<double> builder;
  auto pair = AddMultibodyPlantSceneGraph(
      &builder, std::make_unique<MultibodyPlant<double>>(FLAGS_time_step));
  MultibodyPlant<double> &plant = pair.plant;
  benchmarks::inclined_plane_with_block_plant::AddInclinedPlaneWithBlockToPlant(
      LAx, LAy, LAz, LBx, LBy, LBz, mass, slope_radians, gravity,
      coefficient_friction_block, coefficient_friction_inclined_plane,
      FLAGS_is_inclined_plane_half_space, FLAGS_is_block_with_4Spheres, &plant);
  plant.Finalize();

  // Set the block to inclined-plane allowable penetration (in meters).
  plant.set_penetration_allowance(FLAGS_penetration_allowance);

  // Set the stiction tolerance for the underlying Stribeck friction model.
  plant.set_stiction_tolerance(1.0E-5);

  // Do a reality check that block is a free-flying rigid body.
  DRAKE_DEMAND(plant.num_velocities() == 6);
  DRAKE_DEMAND(plant.num_positions() == 7);

  // Publish contact results for visualization.
  lcm::DrakeLcm lcm;
  ConnectContactResultsToDrakeVisualizer(&builder, plant, &lcm);

  geometry::ConnectDrakeVisualizer(&builder, pair.scene_graph);
  auto diagram = builder.Build();

  // Create a context for this system:
  std::unique_ptr<systems::Context<double>> diagram_context =
      diagram->CreateDefaultContext();
  diagram->SetDefaultContext(diagram_context.get());
  systems::Context<double> &plant_context =
      diagram->GetMutableSubsystemContext(plant, diagram_context.get());

  // By default, the block's initial configuration has the rotation matrix
  // R_WB = 3x3 identity matrix, the position of Bcm (B's center of mass) from
  // Wo (World origin) as p_WoBcm_W = p_WoBo_W = [0; 0; 0], and zero spatial
  // velocity in World W.
  plant.SetDefaultContext(&plant_context);

  // Set the block's initial value so it is above the inclined plane.
  const drake::multibody::Body<double> &block = plant.GetBodyByName("BlockB");
  const Vector3<double> p_WoBo_W(-1.0, 0, 1.2);
  const math::RigidTransform<double> X_WB(p_WoBo_W);
  plant.SetFreeBodyPoseInWorldFrame(&plant_context, block,
                                    X_WB.GetAsIsometry3());

  systems::Simulator<double> simulator(*diagram, std::move(diagram_context));

  systems::IntegratorBase<double> *integrator =
      simulator.get_mutable_integrator();

  // Set the integration accuracy when the plant is integrated with a variable-
  // step integrator. This value is not used if time_step > 0 (fixed-time step).
  const double integration_accuracy = 1.0E-6;
  integrator->set_target_accuracy(integration_accuracy);

  simulator.set_publish_every_time_step(false);
  simulator.set_target_realtime_rate(FLAGS_target_realtime_rate);
  simulator.Initialize();
  simulator.StepTo(FLAGS_simulation_time);

  return 0;
}

}  // namespace
}  // namespace inclined_plane_with_block
}  // namespace examples
}  // namespace multibody
}  // namespace drake

int main(int argc, char* argv[]) {
  gflags::SetUsageMessage(
      "Simulation of a block on an inclined plane (sticking or sliding) using "
      "Drake's MultibodyPlant with SceneGraph visualization. "
      "Launch drake-visualizer before running this example.");
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  drake::logging::HandleSpdlogGflags();
  return drake::multibody::examples::inclined_plane_with_block::do_main();
}
