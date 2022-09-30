#include <limits>

#include <gtest/gtest.h>

#include "drake/common/test_utilities/eigen_matrix_compare.h"
#include "drake/common/test_utilities/expect_no_throw.h"
#include "drake/common/test_utilities/expect_throws_message.h"
#include "drake/common/test_utilities/limit_malloc.h"
#include "drake/multibody/plant/multibody_plant.h"
#include "drake/multibody/plant/test/kuka_iiwa_model_tests.h"
#include "drake/multibody/tree/prismatic_joint.h"
#include "drake/systems/framework/context.h"
#include "drake/systems/primitives/linear_system.h"

using drake::math::RigidTransformd;
using drake::systems::Context;
using drake::test::LimitMalloc;
using Eigen::Vector3d;
using Eigen::VectorXd;

namespace drake {
namespace multibody {

class MultibodyPlantTester {
 public:
  MultibodyPlantTester() = delete;

  static VectorX<double> CalcGeneralizedAccelerations(
      const MultibodyPlant<double>& plant, const Context<double>& context) {
    return plant.EvalForwardDynamics(context).get_vdot();
  }
};

namespace {

const double kEpsilon = std::numeric_limits<double>::epsilon();

// Fixture to perform forward dynamics tests on a model of a KUKA Iiwa arm. The
// base is free.
class KukaIiwaModelForwardDynamicsTests : public test::KukaIiwaModelTests {
 protected:
  // Given the state of the joints in q and v, this method calculates the
  // forward dynamics for the floating KUKA iiwa robot using the articulated
  // body algorithm. The pose and spatial velocity of the base are arbitrary.
  //
  // @param[in] q robot's joint angles (generalized coordinates).
  // @param[in] v robot's joint velocities (generalized velocities).
  // @param[out] vdot generalized accelerations (1st derivative of v).
  void CalcForwardDynamicsViaArticulatedBodyAlgorithm(
      const Eigen::Ref<const VectorX<double>>& q,
      const Eigen::Ref<const VectorX<double>>& v,
      EigenPtr<VectorX<double>> vdot) {
    DRAKE_DEMAND(vdot != nullptr);
    // Update joint positions and velocities.
    VectorX<double> x(q.size() + v.size());
    x << q, v;
    SetState(x);
    *vdot =
        MultibodyPlantTester::CalcGeneralizedAccelerations(*plant_, *context_);
  }

  // This method calculates the forward dynamics for the 7-DOF KUKA iiwa robot
  // by explicitly solving for the inverse of the mass matrix.
  //
  // @param[in] q robot's joint angles (generalized coordinates).
  // @param[in] v robot's joint velocities (generalized velocities).
  // @param[out] vdot generalized accelerations (1st derivative of v).
  void CalcForwardDynamicsViaMassMatrixSolve(
      const Eigen::Ref<const VectorX<double>>& q,
      const Eigen::Ref<const VectorX<double>>& v,
      EigenPtr<VectorX<double>> vdot) {
    DRAKE_DEMAND(vdot != nullptr);
    // Update joint positions and velocities.
    VectorX<double> x(q.size() + v.size());
    x << q, v;
    SetState(x);

    // Compute force element contributions.
    MultibodyForces<double> forces(*plant_);
    plant_->CalcForceElementsContribution(*context_, &forces);

    // Construct M, the mass matrix.
    const int nv = plant_->num_velocities();
    MatrixX<double> M(nv, nv);
    plant_->CalcMassMatrixViaInverseDynamics(*context_, &M);

    // Compute tau = C(q, v)v - tau_app - ∑ J_WBᵀ(q) Fapp_Bo_W via inverse
    // dynamics.
    const VectorX<double> zero_vdot = VectorX<double>::Zero(nv);
    const VectorX<double> tau_id =
        plant_->CalcInverseDynamics(*context_, zero_vdot, forces);

    // Solve for vdot.
    *vdot = M.llt().solve(-tau_id);
  }

  // Verify the solution obtained using the ABA against a reference solution
  // computed by explicitly taking the inverse of the mass matrix.
  void CompareForwardDynamics(const Eigen::Ref<const VectorX<double>>& q,
                              const Eigen::Ref<const VectorX<double>>& v) {
    // Compute forward dynamics using articulated body algorithm.
    VectorX<double> vdot(plant_->num_velocities());
    CalcForwardDynamicsViaArticulatedBodyAlgorithm(q, v, &vdot);

    // Compute forward dynamics using mass matrix.
    VectorX<double> vdot_expected(plant_->num_velocities());
    CalcForwardDynamicsViaMassMatrixSolve(q, v, &vdot_expected);

    // We estimate the difference between vdot and vdot_expected to be in the
    // order of machine epsilon times the condition number "kappa" of the mass
    // matrix.
    const int nv = plant_->num_velocities();
    MatrixX<double> M(nv, nv);
    plant_->CalcMassMatrixViaInverseDynamics(*context_, &M);
    const double kappa = 1.0 / M.llt().rcond();

    // Compare expected results against actual vdot.
    const double kRelativeTolerance = kappa * kEpsilon;
    EXPECT_TRUE(CompareMatrices(vdot, vdot_expected, kRelativeTolerance,
                                MatrixCompareType::relative));
  }
};

// This test is used to verify the correctness of the articulated body algorithm
// for solving forward dynamics. The output from the articulated body algorithm
// is compared against the output from solving using the mass matrix. We verify
// the computation for an arbitrary set of robot states.
TEST_F(KukaIiwaModelForwardDynamicsTests, ForwardDynamicsTest) {
  // Joint angles and velocities.
  VectorX<double> q(kNumJoints), qdot(kNumJoints);
  double q30 = M_PI / 6, q45 = M_PI / 4, q60 = M_PI / 3;

  // Test 1: Static configuration.
  q << 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0;
  qdot << 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0;
  CompareForwardDynamics(q, qdot);

  // Test 2: Another static configuration.
  q << q30, -q45, q60, -q30, q45, -q60, q30;
  qdot << 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0;
  CompareForwardDynamics(q, qdot);

  // Test 3: Non-static configuration.
  q << 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0;
  qdot << 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7;
  CompareForwardDynamics(q, qdot);

  // Test 4: Another non-static configuration.
  q << -q45, q60, -q30, q45, -q60, q30, -q45;
  qdot << 0.7, 0.6, 0.5, 0.4, 0.3, 0.2, 0.1;
  CompareForwardDynamics(q, qdot);

  // Test 5: Another non-static configuration.
  q << q30, q45, q60, -q30, -q45, -q60, 0;
  qdot << 0.3, -0.1, 0.4, -0.1, 0.5, -0.9, 0.2;
  CompareForwardDynamics(q, qdot);
}

// For complex articulated systems such as a humanoid robot, round-off errors
// might accumulate leading to (close to, by machine epsilon) unphysical ABIs in
// the Articulated Body Algorithm. See related issue #12640.
// This test verifies this does not trigger a spurious exception.
GTEST_TEST(MultibodyPlantForwardDynamics, AtlasRobot) {
  MultibodyPlant<double> plant(0.0);
  const std::string model_path =
      FindResourceOrThrow("drake/examples/atlas/urdf/atlas_convex_hull.urdf");
  Parser parser(&plant);
  auto atlas_instance = parser.AddModelFromFile(model_path);
  plant.Finalize();

  // Create a context and store an arbitrary configuration.
  std::unique_ptr<Context<double>> context = plant.CreateDefaultContext();
  for (JointIndex joint_index(0); joint_index < plant.num_joints();
       ++joint_index) {
    const Joint<double>& joint = plant.get_joint(joint_index);
    // This model only has weld and revolute joints. Weld joints have zero DOFs.
    if (joint.num_velocities() != 0) {
      const RevoluteJoint<double>& revolute_joint =
          dynamic_cast<const RevoluteJoint<double>&>(joint);
      // Arbitrary non-zero angle.
      revolute_joint.set_angle(context.get(), 0.5 * joint_index);
    }
  }
  const int num_actuators = plant.num_actuators();
  plant.get_actuation_input_port(atlas_instance)
      .FixValue(context.get(), VectorX<double>::Zero(num_actuators));
  auto derivatives = plant.AllocateTimeDerivatives();
  {
    // CalcTimeDerivatives should not be allocating, but for now we have a few
    // remaining fixes before it's down to zero:
    //  2 temps in MbTS::CalcArticulatedBodyForceCache (F_B_W_, tau_).
    //  1 temp  in MbP::AssembleActuationInput (actuation_input).
    //  2 temps in MbTS::DoCalcTimeDerivatives (xdot, qdot).
    LimitMalloc guard({ .max_num_allocations = 5 });
    EXPECT_NO_THROW(plant.CalcTimeDerivatives(*context, derivatives.get()));
  }

  // Verify that the implicit dynamics match the continuous ones.
  Eigen::VectorXd residual = plant.AllocateImplicitTimeDerivativesResidual();
  plant.CalcImplicitTimeDerivativesResidual(*context, *derivatives, &residual);
  // Note the slightly looser tolerance of 4e-13 which was required for this
  // test.
  EXPECT_TRUE(CompareMatrices(
      residual, Eigen::VectorXd::Zero(plant.num_multibody_states()), 4e-13));
}

// Verifies we can do forward dynamics on a model with a zero-sized state.
GTEST_TEST(WeldedBoxesTest, ForwardDynamicsViaArticulatedBodyAlgorithm) {
  // Problem parameters.
  const double kCubeSize = 1.5;  // Size of the box, in meters.
  const double kBoxMass = 2.0;   // Mass of each box, in Kg.
  // We use discrete_update_period = 0 to set a continuous model that uses the
  // Articulated Body Algorithm (ABA) to evaluate forward dynamics.
  const double discrete_update_period = 0;
  MultibodyPlant<double> plant(discrete_update_period);

  // Set a model with two boxes anchored to the world via weld joints.
  const Vector3d p_BoBcm_B = Vector3d::Zero();
  const UnitInertia<double> G_BBcm =
      UnitInertia<double>::SolidBox(kCubeSize, kCubeSize, kCubeSize);
  const SpatialInertia<double> M_BBo_B =
      SpatialInertia<double>::MakeFromCentralInertia(kBoxMass, p_BoBcm_B,
                                                     G_BBcm);
  // Create two rigid bodies.
  const auto& boxA = plant.AddRigidBody("boxA", M_BBo_B);
  const auto& boxB = plant.AddRigidBody("boxB", M_BBo_B);

  // Desired transformation for the boxes in the world.
  const RigidTransformd X_WA(Vector3d::Zero());
  const RigidTransformd X_WB(Vector3d(kCubeSize, 0, 0));
  const RigidTransformd X_AB = X_WA.inverse() * X_WB;

  // Pin boxA to the world and boxB to boxA with weld joints.
  plant.WeldFrames(plant.world_body().body_frame(), boxA.body_frame(), X_WA);
  plant.WeldFrames(boxA.body_frame(), boxB.body_frame(), X_AB);

  plant.Finalize();
  auto context = plant.CreateDefaultContext();

  // Evaluate forward dynamics.
  const VectorXd vdot =
      MultibodyPlantTester::CalcGeneralizedAccelerations(plant, *context);
  EXPECT_EQ(vdot.size(), 0);
}

std::unique_ptr<systems::LinearSystem<double>> MakeLinearizedCartPole(
    double time_step) {
  const std::string sdf_file = FindResourceOrThrow(
      "drake/examples/multibody/cart_pole/cart_pole.sdf");

  MultibodyPlant<double> plant(time_step);
  Parser(&plant).AddModelFromFile(sdf_file);
  plant.Finalize();

  auto context = plant.CreateDefaultContext();
  plant.get_actuation_input_port().FixValue(context.get(), 0.);
  plant.SetPositionsAndVelocities(context.get(),
                                  Eigen::Vector4d{0, M_PI, 0, 0});

  return systems::Linearize(plant, *context,
                            plant.get_actuation_input_port().get_index(),
                            systems::OutputPortSelection::kNoOutput);
}

// This test revealed a bug (#17037) in MultibodyPlant<AutoDiffXd>.
GTEST_TEST(MultibodyPlantTest, CartPoleLinearization) {
  const double kTimeStep = 0.1;
  auto ct_linearization = MakeLinearizedCartPole(0.0);
  auto dt_linearization = MakeLinearizedCartPole(kTimeStep);

  // v_next = v0 + time_step * (A * x + B * u)
  // q_next = q0 + time_step * v_next
  Eigen::Matrix4d A_expected = Eigen::Matrix4d::Identity();
  A_expected.bottomRows<2>() +=
      kTimeStep * ct_linearization->A().bottomRows<2>();
  A_expected.topRows<2>() += kTimeStep * A_expected.bottomRows<2>();
  Eigen::Vector4d B_expected;
  B_expected.bottomRows<2>() =
      kTimeStep * ct_linearization->B().bottomRows<2>();
  B_expected.topRows<2>() = kTimeStep * B_expected.bottomRows<2>();

  EXPECT_TRUE(CompareMatrices(dt_linearization->A(), A_expected, 1e-16));
  EXPECT_TRUE(CompareMatrices(dt_linearization->B(), B_expected, 1e-16));
}
// TODO(amcastro-tri): Include test with non-zero actuation and external forces.

// Helper function to create a unit inertia for a uniform-density cube B about
// Bo (B's origin point) from a given dimension (length).
// @param[in] length The length of any of the cube's edges.
//   If length = 0, the spatial inertia is that of a particle.
// @retval M_BBo_B Cube B's unit inertia about point Bo (B's origin),
// expressed in terms of unit vectors Bx, By, Bz, each of which are parallel
// to sides (edges) of the cube. Point Bo is the centroid of the face of the
// cube whose outward normal is -Bx. Hence, the position vector from Bo to Bcm
// (B's center of mass) is p_BoBcm_B = Lx/2 Bx.
UnitInertia<double> MakeTestCubeUnitInertia(const double length = 1.0) {
    const UnitInertia<double> G_BBcm_B = UnitInertia<double>::SolidCube(length);
    const Vector3<double> p_BoBcm_B(length / 2, 0, 0);
    const UnitInertia<double> G_BBo_B =
        G_BBcm_B.ShiftFromCenterOfMass(-p_BoBcm_B);
    return G_BBo_B;
}

// Helper function to create a cube-shaped rigid body B and add it to a plant.
// @param[in] plant MultibodyPlant to which body B is added.
// @param[in] body_name name of the body that is being added to the plant.
// @param[in] link_length length, width, and depth of the cube-shaped body.
// @param[in] skip_validity_check setting which is `true` to skip the validity
//  check on the new body B's spatial inertia, which ensures an exception is not
//  thrown when setting body B's spatial inertia (which would otherwise occur if
//  mass or link_length is NaN). Avoiding this early exception allows for a
//  later exception to be thrown in a subsequent function and tested below.
const RigidBody<double>& AddCubicalLink(
    MultibodyPlant<double>* plant,
    const std::string& body_name,
    const double mass,
    const double link_length = 1.0,
    const bool skip_validity_check = false) {
  DRAKE_DEMAND(plant != nullptr);
  const Vector3<double> p_BoBcm_B(link_length / 2, 0, 0);
  const UnitInertia<double> G_BBo_B = MakeTestCubeUnitInertia(link_length);
  const SpatialInertia<double> M_BBo_B(mass, p_BoBcm_B, G_BBo_B,
                                       skip_validity_check);
  return plant->AddRigidBody(body_name, M_BBo_B);
}

// Verify an exception is thrown for a forward dynamic analysis of a single
// zero-mass body that is allowed to translate due to a prismatic joint.
GTEST_TEST(TestSingularHingeMatrix, ThrowErrorForZeroMassTranslatingBody) {
  // Create a plant with discrete_update_period = 0 to set a continuous model
  // that uses the Articulated Body Algorithm (ABA) for forward dynamics.
  const double discrete_update_period = 0;
  MultibodyPlant<double> plant(discrete_update_period);

  double mA = 0;  // Mass of link A.
  const double length = 3;  // Length of uniform-density link (arbitrary > 0).
  const RigidBody<double>& body_A = AddCubicalLink(&plant, "bodyA", mA, length);

  // Add bodyA to world with X-prismatic joint (bodyA has zero mass).
  const RigidBody<double>& world_body = plant.world_body();
  plant.AddJoint<multibody::PrismaticJoint>("WA_prismatic_jointX",
      world_body, std::nullopt, body_A, std::nullopt, Vector3<double>::UnitX());

  // Signal that we are done building the test model.
  plant.Finalize();

  // Create a default context and evaluate forward dynamics.
  auto context = plant.CreateDefaultContext();
  systems::Context<double>* context_ptr = context.get();

  // Verify proper error message is thrown.
  DRAKE_EXPECT_THROWS_MESSAGE(plant.EvalForwardDynamics(*context).get_vdot(),
    "Encountered singular articulated body hinge inertia for body node "
    "index 1. Please ensure that this body has non-zero inertia along "
    "all axes of motion.*");

  // Verify no assertion is thrown if mA = 1E-33.
  body_A.SetMass(context_ptr, mA = 1E-33);
  DRAKE_EXPECT_NO_THROW(plant.EvalForwardDynamics(*context).get_vdot())
}

// Verify an exception is thrown for a forward dynamic analysis of a single
// zero-inertia body that is allowed to rotate due to a revolute joint.
GTEST_TEST(TestSingularHingeMatrix, ThrowErrorForZeroInertiaRotatingBody) {
  // Create a plant with discrete_update_period = 0 to set a continuous model
  // that uses the Articulated Body Algorithm (ABA) for forward dynamics.
  const double discrete_update_period = 0;
  MultibodyPlant<double> plant(discrete_update_period);

  double mA = 0;  // Mass of link A.
  const double length = 3;  // Length of uniform-density link (arbitrary > 0).
  const RigidBody<double>& body_A = AddCubicalLink(&plant, "bodyA", mA, length);

  // Add bodyA to world with Z-revolute joint (bodyA has zero mass/inertia).
  const RigidBody<double>& world_body = plant.world_body();
  plant.AddJoint<multibody::RevoluteJoint>("WA_revolute_jointZ",
      world_body, std::nullopt, body_A, std::nullopt, Vector3<double>::UnitZ());

  // Signal that we are done building the test model.
  plant.Finalize();

  // Create a default context and evaluate forward dynamics.
  auto context = plant.CreateDefaultContext();
  systems::Context<double>* context_ptr = context.get();

  // Verify proper error message is thrown.
  DRAKE_EXPECT_THROWS_MESSAGE(plant.EvalForwardDynamics(*context).get_vdot(),
    "Encountered singular articulated body hinge inertia for body node "
    "index 1. Please ensure that this body has non-zero inertia along "
    "all axes of motion.*");

  // Verify no assertion is thrown if mA = 1E-33.
  body_A.SetMass(context_ptr, mA = 1E-33);
  DRAKE_EXPECT_NO_THROW(plant.EvalForwardDynamics(*context).get_vdot())
}

// Verify an exception may be thrown for a forward dynamic analysis that has
// sequential rigid bodies A and B that translate in the same direction, where
// body A's mass may be disproportionally small (or lage) relative to B's mass.
GTEST_TEST(TestSingularHingeMatrix, DisproportionateMassTranslatingBodiesAB) {
  // Create a plant with discrete_update_period = 0 to set a continuous model
  // that uses the Articulated Body Algorithm (ABA) for forward dynamics.
  const double discrete_update_period = 0;
  MultibodyPlant<double> plant(discrete_update_period);

  double mA = 1E-9, mB = 1E9;  // Mass of links A, B.
  const double length = 3;  // Length of uniform-density link (arbitrary > 0).
  const RigidBody<double>& body_A = AddCubicalLink(&plant, "bodyA", mA, length);
  const RigidBody<double>& body_B = AddCubicalLink(&plant, "bodyB", mB, length);

  // Add bodyA to world with X-prismatic joint.
  const RigidBody<double>& world_body = plant.world_body();
  plant.AddJoint<multibody::PrismaticJoint>("WA_prismatic_jointX",
      world_body, std::nullopt, body_A, std::nullopt, Vector3<double>::UnitX());

  // Add bodyB to bodyA with X-prismatic joint.
  plant.AddJoint<multibody::PrismaticJoint>("AB_prismatic_jointX",
      body_A, std::nullopt, body_B, std::nullopt, Vector3<double>::UnitX());

  // Signal that we are done building the test model.
  plant.Finalize();

  // Create a default context and evaluate forward dynamics.
  auto context = plant.CreateDefaultContext();
  systems::Context<double>* context_ptr = context.get();

  // Verify proper assertion is thrown if mA = 1E-9, mB = 1E9.
  DRAKE_EXPECT_THROWS_MESSAGE(plant.EvalForwardDynamics(*context).get_vdot(),
    "Encountered singular articulated body hinge inertia for body node "
    "index 1. Please ensure that this body has non-zero inertia along "
    "all axes of motion.*");

  // Verify no assertion is thrown if mA = 1E-3, mB = 1E9.
  body_A.SetMass(context_ptr, mA = 1E-3);
  DRAKE_EXPECT_NO_THROW(plant.EvalForwardDynamics(*context).get_vdot())

  // Verify no assertion is thrown if mA = 1E9, mB = 1E-9.
  body_A.SetMass(context_ptr, mA = 1E9);
  body_B.SetMass(context_ptr, mB = 1E-9);
  DRAKE_EXPECT_NO_THROW(plant.EvalForwardDynamics(*context).get_vdot())
}

// Verify an exception may be thrown for a forward dynamic analysis that has
// sequential rigid bodies A and B that rotate in the same direction, where
// body A's inertia may be disproportionally small (or lage) relative to B.
GTEST_TEST(TestSingularHingeMatrix, DisproportionateInertiaRotatingBodiesAB) {
  // Create a plant with discrete_update_period = 0 to set a continuous model
  // that uses the Articulated Body Algorithm (ABA) for forward dynamics.
  const double discrete_update_period = 0;
  MultibodyPlant<double> plant(discrete_update_period);

  double mA = 1, mB = 0;  // Mass of links A, B.
  const double length = 3;  // Length of uniform-density links A, B.
  const RigidBody<double>& body_A = AddCubicalLink(&plant, "bodyA", mA, length);
  const RigidBody<double>& body_B = AddCubicalLink(&plant, "bodyB", mB, length);

  // Add bodyA to world with Z-revolute joint.
  const RigidBody<double>& world_body = plant.world_body();
  const RevoluteJoint<double>& WA_revolute_jointZ =
      plant.AddJoint<multibody::RevoluteJoint>("WA_revolute_jointZ",
      world_body, std::nullopt, body_A, std::nullopt, Vector3<double>::UnitZ());

  // Add bodyB to bodyA with Z-revolute joint.
  const RevoluteJoint<double>& AB_revolute_jointZ =
      plant.AddJoint<multibody::RevoluteJoint>("AB_revolute_jointZ",
      body_A, std::nullopt, body_B, std::nullopt, Vector3<double>::UnitZ());

  // Signal that we are done building the test model.
  plant.Finalize();

  // Create a default context and evaluate forward dynamics.
  auto context = plant.CreateDefaultContext();
  systems::Context<double>* context_ptr = context.get();
  WA_revolute_jointZ.set_angle(context_ptr, M_PI/6.0);
  AB_revolute_jointZ.set_angle(context_ptr, M_PI/4.0);

  // Verify proper assertion is thrown if mA = 1, mB = 0.
  DRAKE_EXPECT_THROWS_MESSAGE(plant.EvalForwardDynamics(*context).get_vdot(),
    "Encountered singular articulated body hinge inertia for body node "
    "index 2. Please ensure that this body has non-zero inertia along "
    "all axes of motion.*");

  // Verify no assertion is thrown if mA = 1, mB = 1E-33.
  body_B.SetMass(context_ptr, mB = 1E-33);
  DRAKE_EXPECT_NO_THROW(plant.EvalForwardDynamics(*context).get_vdot())

  // Verify no assertion is thrown if mA = 1E-11, mB = 1.
  body_A.SetMass(context_ptr, mA = 1);
  body_B.SetMass(context_ptr, mB = 1E-9);
  DRAKE_EXPECT_NO_THROW(plant.EvalForwardDynamics(*context).get_vdot())
}

}  // namespace
}  // namespace multibody
}  // namespace drake
