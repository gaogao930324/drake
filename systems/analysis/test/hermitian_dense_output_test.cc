#include "drake/systems/analysis/hermitian_dense_output.h"

#include <gtest/gtest.h>

#include "drake/common/eigen_types.h"
#include "drake/common/test_utilities/eigen_matrix_compare.h"
#include "drake/common/trajectories/piecewise_polynomial.h"

namespace drake {
namespace systems {
namespace analysis {
namespace {

template <typename T>
class HermitianDenseOutputTest : public ::testing::Test {
 protected:
  const double kInvalidTime{-1.0};
  const double kInitialTime{0.0};
  const double kMidTime{0.5};
  const double kFinalTime{1.0};
  const double kTimeStep{0.1};
  const MatrixX<double> kInitialState{
    (MatrixX<double>(3, 1) << 0., 0., 0.).finished()};
  const MatrixX<double> kMidState{
    (MatrixX<double>(3, 1) << 0.5, 5., 50.).finished()};
  const MatrixX<double> kFinalState{
    (MatrixX<double>(3, 1) << 1., 10., 100.).finished()};
  const MatrixX<double> kFinalStateWithFewerDimensions{
    (MatrixX<double>(2, 1) << 1., 10.).finished()};
  const MatrixX<double> kFinalStateWithMoreDimensions{
    (MatrixX<double>(4, 1) << 1., 10., 100., 1000.).finished()};
  const MatrixX<double> kFinalStateNotAVector{
    (MatrixX<double>(2, 2) << 1., 10., 100., 1000.).finished()};
  const MatrixX<double> kInitialStateDerivative{
    (MatrixX<double>(3, 1) << 0., 1., 0.).finished()};
  const MatrixX<double> kMidStateDerivative{
    (MatrixX<double>(3, 1) << 0.5, 0.5, 0.5).finished()};
  const MatrixX<double> kFinalStateDerivative{
    (MatrixX<double>(3, 1) << 1., 0., 1.).finished()};
  const MatrixX<double> kFinalStateDerivativeWithFewerDimensions{
    (MatrixX<double>(2, 1) << 1., 0.).finished()};
  const MatrixX<double> kFinalStateDerivativeWithMoreDimensions{
    (MatrixX<double>(4, 1) << 1., 0., 1., 0.).finished()};
  const MatrixX<double> kFinalStateDerivativeNotAVector{
    (MatrixX<double>(2, 2) << 0., 1., 0., 1.).finished()};
};


// HermitianDenseOutput types to test.
typedef ::testing::Types<double, AutoDiffXd> OutputTypes;

TYPED_TEST_CASE(HermitianDenseOutputTest, OutputTypes);

// Checks that HermitianDenseOutput consistency is ensured.
TYPED_TEST(HermitianDenseOutputTest, OutputConsistency) {
  // Instantiates dense output.
  HermitianDenseOutput<TypeParam> dense_output;
  // Verifies that the dense output is empty and API behavior
  // is consistent with that fact.
  ASSERT_TRUE(dense_output.is_empty());
  EXPECT_THROW(dense_output.Evaluate(
      this->kInitialTime), std::logic_error);
  EXPECT_THROW(dense_output.get_start_time(), std::logic_error);
  EXPECT_THROW(dense_output.get_end_time(), std::logic_error);
  EXPECT_THROW(dense_output.get_dimensions(), std::logic_error);
  EXPECT_THROW(dense_output.Rollback(), std::logic_error);
  EXPECT_THROW(dense_output.Consolidate(), std::logic_error);

  // Verifies that trying to update the dense output with
  // a zero length step fails.
  typename HermitianDenseOutput<TypeParam>::IntegrationStep first_step(
      this->kInitialTime, this->kInitialState, this->kInitialStateDerivative);
  EXPECT_THROW(dense_output.Update(first_step), std::runtime_error);

  // Verifies that trying to update the dense output with
  // a valid step succeeds.
  first_step.Extend(this->kMidTime, this->kMidState,
                    this->kMidStateDerivative);
  dense_output.Update(first_step);

  // Verifies that an update does not imply a consolidation and thus
  // the dense output remains empty.
  ASSERT_TRUE(dense_output.is_empty());
  EXPECT_THROW(dense_output.Evaluate(
      this->kMidTime), std::logic_error);
  EXPECT_THROW(dense_output.get_start_time(), std::logic_error);
  EXPECT_THROW(dense_output.get_end_time(), std::logic_error);
  EXPECT_THROW(dense_output.get_dimensions(), std::logic_error);

  // Consolidates all previous updates.
  dense_output.Consolidate();

  // Verifies that it is not possible to roll back updates after consolidation.
  EXPECT_THROW(dense_output.Rollback(), std::logic_error);

  // Verifies that the dense output is not empty and that it
  // reflects the data provided on updates.
  ASSERT_FALSE(dense_output.is_empty());
  EXPECT_EQ(dense_output.get_start_time(), first_step.get_start_time());
  EXPECT_EQ(dense_output.get_end_time(), first_step.get_end_time());
  EXPECT_EQ(dense_output.get_dimensions(), first_step.get_dimensions());
  EXPECT_NO_THROW(dense_output.Evaluate(this->kMidTime));

  // Verifies that invalid evaluation arguments generate errors.
  EXPECT_THROW(dense_output.Evaluate(this->kInvalidTime),
               std::runtime_error);

  // Verifies that step updates that would disrupt the output continuity
  // fail.
  typename HermitianDenseOutput<TypeParam>::IntegrationStep second_step(
      (this->kFinalTime + this->kMidTime) / 2.,
      this->kMidState, this->kMidStateDerivative);
  second_step.Extend(this->kFinalTime, this->kFinalState,
                     this->kFinalStateDerivative);
  EXPECT_THROW(dense_output.Update(second_step), std::runtime_error);

  typename HermitianDenseOutput<TypeParam>::IntegrationStep third_step(
      this->kMidTime, this->kMidState * 2., this->kMidStateDerivative);
  third_step.Extend(this->kFinalTime, this->kFinalState,
                     this->kFinalStateDerivative);
  EXPECT_THROW(dense_output.Update(third_step), std::runtime_error);

  typename HermitianDenseOutput<TypeParam>::IntegrationStep fourth_step(
      this->kMidTime, this->kMidState, this->kMidStateDerivative * 2.);
  fourth_step.Extend(this->kFinalTime, this->kFinalState,
                     this->kFinalStateDerivative);
  EXPECT_THROW(dense_output.Update(fourth_step), std::runtime_error);
}

// Checks that HermitianDenseOutput::Step consistency is ensured.
TYPED_TEST(HermitianDenseOutputTest, StepsConsistency) {
  // Verifies that zero length steps are properly constructed.
  typename HermitianDenseOutput<TypeParam>::IntegrationStep step(
      this->kInitialTime, this->kInitialState, this->kInitialStateDerivative);
  ASSERT_EQ(step.get_times().size(), 1);
  EXPECT_EQ(step.get_start_time(), this->kInitialTime);
  EXPECT_EQ(step.get_end_time(), this->kInitialTime);
  EXPECT_EQ(step.get_dimensions(), this->kInitialState.rows());
  ASSERT_EQ(step.get_states().size(), 1);
  EXPECT_TRUE(CompareMatrices(step.get_states().front(), this->kInitialState));
  ASSERT_EQ(step.get_state_derivatives().size(), 1);
  EXPECT_TRUE(CompareMatrices(step.get_state_derivatives().front(),
                              this->kInitialStateDerivative));

  // Verifies that any attempt to break step consistency fails.
  EXPECT_THROW(step.Extend(this->kInvalidTime, this->kFinalState,
                           this->kFinalStateDerivative),
               std::runtime_error);

  EXPECT_THROW(step.Extend(this->kFinalTime,
                           this->kFinalStateWithFewerDimensions,
                           this->kFinalStateDerivative), std::runtime_error);

  EXPECT_THROW(step.Extend(this->kFinalTime,
                           this->kFinalStateWithMoreDimensions,
                           this->kFinalStateDerivative), std::runtime_error);

  EXPECT_THROW(step.Extend(this->kFinalTime,
                           this->kFinalStateNotAVector,
                           this->kFinalStateDerivative), std::runtime_error);

  EXPECT_THROW(step.Extend(this->kFinalTime, this->kFinalState,
                           this->kFinalStateDerivativeWithFewerDimensions),
               std::runtime_error);

  EXPECT_THROW(step.Extend(this->kFinalTime, this->kFinalState,
                           this->kFinalStateDerivativeWithMoreDimensions),
               std::runtime_error);

  EXPECT_THROW(step.Extend(this->kFinalTime, this->kFinalState,
                           this->kFinalStateDerivativeNotAVector),
               std::runtime_error);

  // Extends the step with appropriate values.
  step.Extend(this->kFinalTime, this->kFinalState, this->kFinalStateDerivative);

  // Verifies that the step was properly extended.
  EXPECT_EQ(step.get_times().size(), 2);
  EXPECT_EQ(step.get_start_time(), this->kInitialTime);
  EXPECT_EQ(step.get_end_time(), this->kFinalTime);
  EXPECT_EQ(step.get_dimensions(), this->kInitialState.rows());
  EXPECT_EQ(step.get_states().size(), 2);
  EXPECT_TRUE(CompareMatrices(step.get_states().back(), this->kFinalState));
  EXPECT_EQ(step.get_state_derivatives().size(), 2);
  EXPECT_TRUE(CompareMatrices(step.get_state_derivatives().back(),
                              this->kFinalStateDerivative));
}

// Checks that HermitianDenseOutput properly supports stepwise
// construction.
TYPED_TEST(HermitianDenseOutputTest, CorrectConstruction) {
  // Instantiates dense output.
  HermitianDenseOutput<TypeParam> dense_output;
  // Updates output for the first time.
  typename HermitianDenseOutput<TypeParam>::IntegrationStep first_step(
      this->kInitialTime, this->kInitialState, this->kInitialStateDerivative);
  first_step.Extend(this->kMidTime, this->kMidState, this->kMidStateDerivative);
  dense_output.Update(first_step);
  // Updates output a second time.
  typename HermitianDenseOutput<TypeParam>::IntegrationStep second_step(
      this->kMidTime, this->kMidState, this->kMidStateDerivative);
  second_step.Extend(this->kFinalTime, this->kFinalState,
                     this->kFinalStateDerivative);
  dense_output.Update(second_step);
  // Rolls back the last update.
  dense_output.Rollback();  // `second_step`
  // Consolidates existing updates.
  dense_output.Consolidate();  // only `first_step`

  // Verifies that the dense output only reflects the first step.
  EXPECT_FALSE(dense_output.is_empty());
  EXPECT_EQ(dense_output.get_start_time(), first_step.get_start_time());
  EXPECT_EQ(dense_output.get_end_time(), first_step.get_end_time());
  EXPECT_EQ(dense_output.get_dimensions(), first_step.get_dimensions());
  EXPECT_TRUE(CompareMatrices(dense_output.Evaluate(this->kInitialTime),
                              first_step.get_states().front()));
  EXPECT_TRUE(CompareMatrices(dense_output.Evaluate(this->kMidTime),
                              first_step.get_states().back()));
}

// Checks that HermitianDenseOutput properly implements and evaluates
// an Hermite interpolator.
TYPED_TEST(HermitianDenseOutputTest, CorrectEvaluation) {
  // Creates an Hermite cubic spline with times, states and state
  // derivatives.
  const std::vector<double> spline_times{
    this->kInitialTime, this->kMidTime, this->kFinalTime};
  const std::vector<MatrixX<double>> spline_states{
          this->kInitialState, this->kMidState, this->kFinalState};
  const std::vector<MatrixX<double>> spline_state_derivatives{
          this->kInitialStateDerivative, this->kMidStateDerivative,
          this->kFinalStateDerivative};
  const trajectories::PiecewisePolynomial<double> hermite_spline =
      trajectories::PiecewisePolynomial<double>::Cubic(
          spline_times, spline_states, spline_state_derivatives);
  // Instantiates dense output.
  HermitianDenseOutput<TypeParam> dense_output;
  // Updates output for the first time.
  typename HermitianDenseOutput<TypeParam>::IntegrationStep first_step(
      this->kInitialTime, this->kInitialState, this->kInitialStateDerivative);
  first_step.Extend(this->kMidTime, this->kMidState, this->kMidStateDerivative);
  dense_output.Update(first_step);
  // Updates output a second time.
  typename HermitianDenseOutput<TypeParam>::IntegrationStep second_step(
      this->kMidTime, this->kMidState, this->kMidStateDerivative);
  second_step.Extend(this->kFinalTime, this->kFinalState,
                     this->kFinalStateDerivative);
  dense_output.Update(second_step);
  // Consolidates all previous updates.
  dense_output.Consolidate();
  // Verifies that dense output and Hermite spline match.
  const double kAccuracy{1e-12};
  EXPECT_FALSE(dense_output.is_empty());
  for (TypeParam t = this->kInitialTime;
       t <= this->kFinalTime; t += this->kTimeStep) {
    const MatrixX<double> matrix_value =
        hermite_spline.value(ExtractDoubleOrThrow(t));
    const VectorX<TypeParam> vector_value =
        matrix_value.col(0).template cast<TypeParam>();
    EXPECT_TRUE(CompareMatrices(dense_output.Evaluate(t),
                                vector_value, kAccuracy));
  }
}

}  // namespace
}  // namespace analysis
}  // namespace systems
}  // namespace drake