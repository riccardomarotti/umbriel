#include "check.h"
#include "output/frame_schedule.h"

using umbriel::OutputFrameFollowup;
using umbriel::outputFrameFollowup;
using umbriel::TearingCommitRecovery;

UMBRIEL_TEST(nestedBackendCanScheduleFrames) {
  CHECK_EQ(outputFrameFollowup(false, nullptr, false, true), OutputFrameFollowup::Schedule);
}

UMBRIEL_TEST(activeNativeSessionCanScheduleFrames) {
  wlr_session session{};
  session.active = true;
  CHECK_EQ(outputFrameFollowup(false, &session, false, true), OutputFrameFollowup::Schedule);
}

UMBRIEL_TEST(inactiveNativeSessionCannotScheduleFrames) {
  wlr_session session{};
  session.active = false;
  CHECK_EQ(outputFrameFollowup(false, &session, false, true), OutputFrameFollowup::None);
}

UMBRIEL_TEST(stoppingCompositorCannotScheduleFrames) {
  wlr_session session{};
  session.active = true;
  CHECK_EQ(outputFrameFollowup(true, &session, false, true), OutputFrameFollowup::None);
}

UMBRIEL_TEST(failedCommitUsesDelayedRetry) {
  wlr_session session{};
  session.active = true;
  CHECK_EQ(outputFrameFollowup(false, &session, true, false), OutputFrameFollowup::RetryDelayed);
}

UMBRIEL_TEST(failedAnimatedFrameDoesNotScheduleImmediately) {
  wlr_session session{};
  session.active = true;
  CHECK_EQ(outputFrameFollowup(false, &session, true, true), OutputFrameFollowup::RetryDelayed);
}

UMBRIEL_TEST(idleSuccessfulFrameNeedsNoFollowup) {
  wlr_session session{};
  session.active = true;
  CHECK_EQ(outputFrameFollowup(false, &session, false, false), OutputFrameFollowup::None);
}

UMBRIEL_TEST(failedTearingCommitForcesARegularRecoveryCommit) {
  TearingCommitRecovery recovery;

  CHECK(recovery.requestTearing(true));
  recovery.recordCommit(true, false);

  CHECK(recovery.regularCommitPending());
  CHECK(!recovery.requestTearing(true));
}

UMBRIEL_TEST(failedRegularRecoveryStaysRegularUntilItSucceeds) {
  TearingCommitRecovery recovery;
  recovery.recordCommit(true, false);
  recovery.recordCommit(false, false);

  CHECK(recovery.regularCommitPending());
  CHECK(!recovery.requestTearing(true));

  recovery.recordCommit(false, true);
  CHECK(!recovery.regularCommitPending());
  CHECK(recovery.requestTearing(true));
}

UMBRIEL_TEST(rejectedTearingTestKeepsFailedRegularFallbackOnTheRegularPath) {
  TearingCommitRecovery recovery;
  recovery.forceRegularCommit();
  recovery.recordCommit(false, false);

  CHECK(recovery.regularCommitPending());
  CHECK(!recovery.requestTearing(true));

  recovery.recordCommit(false, true);
  CHECK(recovery.requestTearing(true));
}

UMBRIEL_TEST(ineligibleFramesNeverRequestTearing) {
  TearingCommitRecovery recovery;
  CHECK(!recovery.requestTearing(false));
  recovery.recordCommit(false, true);
  CHECK(!recovery.regularCommitPending());
}

UMBRIEL_TEST(effectFrameDelayFollowsRefreshOrCapsAtMaxFps) {
  CHECK_EQ(umbriel::effectFrameDelayMs(0, 1000, 990), uint64_t{0});
  CHECK_EQ(umbriel::effectFrameDelayMs(60, 1000, 1000), uint64_t{17});
  CHECK_EQ(umbriel::effectFrameDelayMs(60, 1010, 1000), uint64_t{7});
  CHECK_EQ(umbriel::effectFrameDelayMs(60, 1100, 1000), uint64_t{1});
  CHECK_EQ(umbriel::effectFrameDelayMs(1, 1000, 1000), uint64_t{1000});
}

UMBRIEL_TEST(effectFrameDelayWaitsAFullIntervalWhenTheLastFrameIsAhead) {
  CHECK_EQ(umbriel::effectFrameDelayMs(60, 990, 1000), uint64_t{17});
}

UMBRIEL_TEST(effectFrameDelayNeverDropsBelowOneMillisecond) {
  CHECK_EQ(umbriel::effectFrameDelayMs(2000, 1000, 1000), uint64_t{1});
}

UMBRIEL_TEST(effectPresentationDelayUsesDistinctPresentedSamples) {
  umbriel::EffectPresentationDelay estimate;
  CHECK(!estimate.estimateNs().has_value());
  estimate.observe(1, 1'000'000, 14'000'000);
  CHECK_EQ(estimate.estimateNs(), std::optional<uint64_t>(13'000'000));
  estimate.observe(1, 1'000'000, 22'000'000); // duplicate snapshot must not bias timing
  CHECK_EQ(estimate.estimateNs(), std::optional<uint64_t>(13'000'000));
  estimate.observe(2, 2'000'000, 23'000'000);
  CHECK_EQ(estimate.estimateNs(), std::optional<uint64_t>(14'000'000));
  estimate.observe(3, 10, 9); // invalid clock ordering
  estimate.observe(4, 10, 60'000'010); // stalled presentation
  CHECK_EQ(estimate.estimateNs(), std::optional<uint64_t>(14'000'000));
  estimate.observe(5, 2'000'000, 10'000'000);
  CHECK_EQ(estimate.estimateNs(), std::optional<uint64_t>(13'250'000));
}

int main() { return RUN_TESTS(); }
