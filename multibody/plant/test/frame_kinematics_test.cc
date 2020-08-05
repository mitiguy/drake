#include <limits>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "drake/common/autodiff.h"
#include "drake/common/test_utilities/eigen_matrix_compare.h"
#include "drake/math/autodiff_gradient.h"
#include "drake/multibody/plant/test/kuka_iiwa_model_tests.h"
#include "drake/multibody/test_utilities/add_fixed_objects_to_plant.h"
#include "drake/multibody/tree/body.h"
#include "drake/multibody/tree/frame.h"

namespace drake {

namespace multibody {

using math::RigidTransformd;
using test::KukaIiwaModelTests;

namespace {

TEST_F(KukaIiwaModelTests, FramesKinematics) {
  // Numerical tolerance used to verify numerical results.
  const double kTolerance = 10 * std::numeric_limits<double>::epsilon();
  SetArbitraryConfiguration();

  const RigidTransform<double>& X_WE =
      end_effector_link_->EvalPoseInWorld(*context_);
  const RigidTransform<double> X_WH = frame_H_->CalcPoseInWorld(*context_);
  const RigidTransform<double> X_WH_expected = X_WE * X_EH_;
  EXPECT_TRUE(CompareMatrices(
      X_WH.GetAsMatrix34(), X_WH_expected.GetAsMatrix34(),
      kTolerance, MatrixCompareType::relative));

  // Alternatively, we can get the pose X_WE using the plant's output port for
  // poses.
  const auto& X_WB_all =
      plant_->get_body_poses_output_port()
          .Eval<std::vector<RigidTransform<double>>>(*context_);
  ASSERT_EQ(X_WB_all.size(), plant_->num_bodies());
  const RigidTransform<double>& X_WE_from_port =
      X_WB_all[end_effector_link_->index()];
  EXPECT_TRUE(CompareMatrices(
      X_WE.GetAsMatrix34(), X_WE_from_port.GetAsMatrix34(),
      kTolerance, MatrixCompareType::relative));

  // Verify the invariance X_WB_all[0] = RigidTransform<double>::Identity().
  EXPECT_TRUE(
      CompareMatrices(X_WB_all[0].GetAsMatrix34(),
                      RigidTransform<double>::Identity().GetAsMatrix34(),
                      kTolerance, MatrixCompareType::relative));

  const RotationMatrix<double> R_WH =
      frame_H_->CalcRotationMatrixInWorld(*context_);
  const RotationMatrix<double> R_WH_expected = X_WH_expected.rotation();
  EXPECT_TRUE(CompareMatrices(R_WH.matrix(), R_WH_expected.matrix(), kTolerance,
                              MatrixCompareType::relative));

  const Body<double>& link3 = plant_->GetBodyByName("iiwa_link_3");
  const RigidTransform<double> X_HL3 =
      link3.body_frame().CalcPose(*context_, *frame_H_);
  const RigidTransform<double> X_WL3 =
      link3.body_frame().CalcPoseInWorld(*context_);
  const RigidTransform<double> X_HL3_expected = X_WH.inverse() * X_WL3;
  EXPECT_TRUE(CompareMatrices(
      X_HL3.GetAsMatrix34(), X_HL3_expected.GetAsMatrix34(),
      kTolerance, MatrixCompareType::relative));

  const RotationMatrix<double> R_HL3 =
      link3.body_frame().CalcRotationMatrix(*context_, *frame_H_);
  const RotationMatrix<double> R_WL3 =
      link3.body_frame().CalcRotationMatrixInWorld(*context_);
  const RotationMatrix<double> R_HL3_expected = R_WH.inverse() * R_WL3;
  EXPECT_TRUE(CompareMatrices(R_HL3.matrix(), R_HL3_expected.matrix(),
                              kTolerance, MatrixCompareType::relative));

  const SpatialVelocity<double> V_WE =
      end_effector_link_->EvalSpatialVelocityInWorld(*context_);
  const SpatialVelocity<double> V_WH =
      frame_H_->CalcSpatialVelocityInWorld(*context_);
  const Vector3<double> p_EH =
      frame_H_->GetFixedPoseInBodyFrame().translation();
  const RotationMatrix<double>& R_WE = X_WE.rotation();
  const Vector3<double> p_EH_W = R_WE * p_EH;
  const SpatialVelocity<double> V_WH_expected = V_WE.Shift(p_EH_W);
  EXPECT_TRUE(CompareMatrices(
      V_WH.get_coeffs(), V_WH_expected.get_coeffs(),
      kTolerance, MatrixCompareType::relative));

  // Alternatively, we can get the spatial velocity V_WE using the plant's
  // output port for spatial velocities.
  const auto& V_WB_all =
      plant_->get_body_spatial_velocities_output_port()
          .Eval<std::vector<SpatialVelocity<double>>>(*context_);
  ASSERT_EQ(V_WB_all.size(), plant_->num_bodies());
  const SpatialVelocity<double>& V_WE_from_port =
      V_WB_all[end_effector_link_->index()];
  EXPECT_EQ(V_WE.get_coeffs(), V_WE_from_port.get_coeffs());

  // Verify a short-cut return from Frame::CalcSpatialAccelerationInWorld()
  // when dealing with a body_frame (as opposed to a generic frame).
  // Compare results with the A_WE_W from an associated plant method.
  const Frame<double>& frame_E = end_effector_link_->body_frame();
  const SpatialAcceleration<double> A_WE_W =
      frame_E.CalcSpatialAccelerationInWorld(*context_);
  const SpatialAcceleration<double> A_WE_W_alternate1 =
     plant_->EvalBodySpatialAccelerationInWorld(*context_, *end_effector_link_);
  EXPECT_EQ(A_WE_W.get_coeffs(), A_WE_W_alternate1.get_coeffs());

  // Also verify A_WE_W against Body::EvalSpatialAccelerationInWorld().
  const SpatialAcceleration<double> A_WE_W_alternate2 =
    end_effector_link_->EvalSpatialAccelerationInWorld(*context_);
  EXPECT_EQ(A_WE_W.get_coeffs(), A_WE_W_alternate2.get_coeffs());

  // Also verify A_WE_W from the plant's output port for spatial acceleration.
  const auto& A_WB_all =
    plant_->get_body_spatial_accelerations_output_port()
        .Eval<std::vector<SpatialAcceleration<double>>>(*context_);
  ASSERT_EQ(A_WB_all.size(), plant_->num_bodies());
  const SpatialAcceleration<double>& A_WE_W_from_port =
      A_WB_all[end_effector_link_->index()];
  EXPECT_EQ(A_WE_W.get_coeffs(), A_WE_W_from_port.get_coeffs());

  // Verify A_WH_W, frame_H's spatial acceleration in world W, expressed in W.
  const SpatialAcceleration<double> A_WH_W =
    frame_H_->CalcSpatialAccelerationInWorld(*context_);
  const Vector3<double> w_WE_W = V_WH.rotational();
  const SpatialAcceleration<double> A_WH_W_expected =
      A_WE_W.Shift(p_EH_W, w_WE_W);
  EXPECT_TRUE(CompareMatrices(A_WH_W.get_coeffs(), A_WH_W_expected.get_coeffs(),
                              kTolerance, MatrixCompareType::relative));

  // Reverify A_WH_W by differentiating the spatial velocity V_WH_W.
  // Spatial acceleration is a function of the generalized accelerations vdot.
  // Use forward dynamics to calculate values for vdot (for the given q, v).
  const auto& derivs = plant_->EvalTimeDerivatives(*context_);
  const auto& vdot_auto = derivs.get_generalized_velocity();
  EXPECT_EQ(vdot_auto.size(), plant_->num_velocities());
  const VectorX<double> vdot(vdot_auto.CopyToVector());

  // Enable q_autodiff and v_autodiff to differentiate with respect to time.
  // Note: Pass MatrixXd() so the return gradient uses AutoDiffXd (for which we
  // do have explicit instantiations) instead of AutoDiffScalar<Matrix1d>.
  const VectorX<double> q = plant_->GetPositions(*context_);
  const VectorX<double> v = plant_->GetVelocities(*context_);
  VectorX<double> qdot(plant_->num_positions());
  plant_->MapVelocityToQDot(*context_, v, &qdot);
  auto q_autodiff =
      math::initializeAutoDiffGivenGradientMatrix(q, Eigen::MatrixXd(qdot));
  auto v_autodiff =
      math::initializeAutoDiffGivenGradientMatrix(v, Eigen::MatrixXd(vdot));

  // Set the context for AutoDiffXd computations.
  VectorX<AutoDiffXd> x_autodiff(plant_->num_multibody_states());
  x_autodiff << q_autodiff, v_autodiff;
  plant_autodiff_->GetMutablePositionsAndVelocities(context_autodiff_.get()) =
      x_autodiff;

  // Using AutoDiff, compute V_WHo_W (point Ho's spatial velocity in the world
  // frame W, expressed in W), and its time derivative which is A_WHo_W
  // (point Ho's spatial acceleration in W, expressed in W).
  const Frame<AutoDiffXd>& frame_H_autodiff =
      plant_autodiff_->get_frame(frame_H_->index());
  const SpatialVelocity<AutoDiffXd> V_WHo_W_autodiff =
      frame_H_autodiff.CalcSpatialVelocityInWorld(*context_autodiff_);

  // Form the expected spatial acceleration via AutoDiffXd results.
  // Reminder: Eigen returns a weirdly sized matrix if all derivatives = 0.
  auto Dt_V_WHo_W =
      math::autoDiffToGradientMatrix(V_WHo_W_autodiff.get_coeffs());
  const bool is_empty_matrix = Dt_V_WHo_W.size() == 0;
  const Vector6<AutoDiffXd> A_WHo_W_expected = is_empty_matrix ?
                                                Vector6<AutoDiffXd>::Zero() :
                                                Vector6<AutoDiffXd>(Dt_V_WHo_W);
  const Vector6<double> A_WHo_W_expected_double(
      math::autoDiffToValueMatrix(A_WHo_W_expected));

  // Verify computed spatial acceleration numerical values.
  EXPECT_TRUE(CompareMatrices(A_WH_W.get_coeffs(), A_WHo_W_expected_double,
                               kTolerance, MatrixCompareType::relative));

  // Spatial velocity of link 3 measured in the H frame and expressed in the
  // end-effector frame E.
  const SpatialVelocity<double> V_HL3_E =
      link3.body_frame().CalcSpatialVelocity(
          *context_, *frame_H_, end_effector_link_->body_frame());
  // Compute V_HL3_E_expected.
  const SpatialVelocity<double> V_WH_E = R_WE.transpose() * V_WH;
  const math::RotationMatrix<double> R_EH =
      frame_H_->GetFixedPoseInBodyFrame().rotation();
  const Vector3<double> p_HL3_E = R_EH * X_HL3.translation();
  const SpatialVelocity<double> V_WL3_E =
      R_WE.transpose() * link3.EvalSpatialVelocityInWorld(*context_);
  // V_WL3_E = V_WH_E.Shift(p_HL3_E) + V_HL3_E
  const SpatialVelocity<double> V_HL3_E_expected =
      V_WL3_E - V_WH_E.Shift(p_HL3_E);
  EXPECT_TRUE(CompareMatrices(
      V_HL3_E.get_coeffs(), V_HL3_E_expected.get_coeffs(),
      kTolerance, MatrixCompareType::relative));

  // Test for a simple identity case of CalcRelativeTransform().
  const RigidTransformd X_HH =
      plant_->CalcRelativeTransform(*context_, *frame_H_, *frame_H_);
  EXPECT_TRUE(CompareMatrices(X_HH.rotation().matrix(),
      Matrix3<double>::Identity(), kTolerance, MatrixCompareType::relative));
  EXPECT_TRUE(CompareMatrices(X_HH.translation(), Vector3<double>::Zero(),
                              kTolerance, MatrixCompareType::relative));

  // Test for a simple identity case of CalcRelativeRotationMatrix().
  const RotationMatrixd R_HH =
      plant_->CalcRelativeRotationMatrix(*context_, *frame_H_, *frame_H_);
  EXPECT_TRUE(CompareMatrices(R_HH.matrix(), Matrix3<double>::Identity(),
                              kTolerance, MatrixCompareType::relative));
}

GTEST_TEST(MultibodyPlantTest, FixedWorldKinematics) {
  // Numerical tolerance used to verify numerical results.
  const double kTolerance = 10 * std::numeric_limits<double>::epsilon();

  MultibodyPlant<double> plant(0.0);
  test::AddFixedObjectsToPlant(&plant);
  plant.Finalize();
  std::unique_ptr<Context<double>> context = plant.CreateDefaultContext();

  // The point of this test is that we can compute poses and spatial velocities
  // even for a model with zero dofs.
  ASSERT_EQ(plant.num_positions(), 0);
  ASSERT_EQ(plant.num_velocities(), 0);
  // However the world is non-empty.
  ASSERT_NE(plant.num_bodies(), 0);

  const Body<double>& mug = plant.GetBodyByName("main_body");

  // The objects frame O is affixed to a robot table defined by
  // test::AddFixedObjectsToPlant().
  const Frame<double>& objects_frame = plant.GetFrameByName("objects_frame");

  // This will trigger the computation of position kinematics.
  const RigidTransformd& X_WM = mug.EvalPoseInWorld(*context);

  // From test::AddFixedObjectsToPlant() we know the fixed pose of the mug frame
  // M in the objects frame O.
  const RigidTransformd X_OM = Translation3d(0.0, 0.0, 0.05);
  // Therefore we expect the pose of the mug to be:
  const RigidTransformd& X_WM_expected =
      objects_frame.CalcPoseInWorld(*context) * X_OM;

  // We verify the results.
  EXPECT_TRUE(CompareMatrices(X_WM.GetAsMatrix34(),
                              X_WM_expected.GetAsMatrix34(), kTolerance));

  // Now we evaluate some velocity kinematics.
  const SpatialVelocity<double>& V_WM =
      mug.EvalSpatialVelocityInWorld(*context);
  // Since all bodies are anchored, they all have zero spatial velocity.
  EXPECT_EQ(V_WM.get_coeffs(), Vector6<double>::Zero());
}

}  // namespace
}  // namespace multibody
}  // namespace drake
