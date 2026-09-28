// Head tracking through the aim: the hip passes the pose through in both modes; with the sights up
// sights locked keeps rotation whole and eases the lean to nothing, true free look keeps all of it;
// a transition scales only the lean and a reversal carries on from where it was, whether the aim
// button or the true free look key reverses it; and the zoom scales yaw, pitch and the lean but
// never roll.

#include "core/aim_pose.h"

#include <cameraunlock/ads/ads_fade.h>

#include <cmath>
#include <cstdio>
#include <string>

using namespace DL2HT;

namespace {

int g_failures = 0;

void Check(bool ok, const std::string& what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what.c_str());
        ++g_failures;
    }
}

bool Near(float a, float b, float tolerance = 1e-5f) { return std::fabs(a - b) <= tolerance; }

HeadPose Tracked() {
    HeadPose p;
    p.yaw = 20.0f;
    p.pitch = -12.0f;
    p.roll = 9.0f;
    p.lean = cameraunlock::math::Vec3(0.2f, -0.05f, -0.3f);
    return p;
}

bool SameRotation(const HeadPose& a, const HeadPose& b) {
    return a.yaw == b.yaw && a.pitch == b.pitch && a.roll == b.roll;
}

bool SameLean(const HeadPose& a, const cameraunlock::math::Vec3& lean, float scale) {
    return Near(a.lean.x, lean.x * scale) && Near(a.lean.y, lean.y * scale) && Near(a.lean.z, lean.z * scale);
}

constexpr unsigned long long kSettled = cameraunlock::ads::AdsFade::kRaiseMs + 50;

void TestHipPassesThrough() {
    for (const bool freeLook : {false, true}) {
        AimPose pose;
        const HeadPose in = Tracked();
        for (unsigned long long t = 0; t <= kSettled; t += 16) {
            const HeadPose out = pose.Apply(in, false, freeLook, 1.0f, t);
            Check(SameRotation(out, in) && SameLean(out, in.lean, 1.0f),
                  std::string("at the hip the pose passes through untouched, true free look ") + (freeLook ? "on" : "off"));
        }
    }
}

void TestSightsLockedEasesOnlyTheLean() {
    AimPose pose;
    const HeadPose in = Tracked();
    pose.Apply(in, false, false, 1.0f, 0);
    HeadPose out;
    for (unsigned long long t = 16; t <= 16 + kSettled; t += 16) out = pose.Apply(in, true, false, 1.0f, t);
    Check(SameRotation(out, in), "sights locked, sights up: yaw, pitch and roll are the tracker's, unscaled");
    Check(out.lean.x == 0.0f && out.lean.y == 0.0f && out.lean.z == 0.0f,
          "sights locked, sights up: the lean has eased to nothing");
}

void TestTrueFreeLookKeepsEverything() {
    AimPose pose;
    const HeadPose in = Tracked();
    pose.Apply(in, false, true, 1.0f, 0);
    for (unsigned long long t = 16; t <= 16 + kSettled; t += 16) {
        const HeadPose out = pose.Apply(in, true, true, 1.0f, t);
        Check(SameRotation(out, in) && SameLean(out, in.lean, 1.0f),
              "true free look, sights up: the pose passes through untouched");
    }
}

void TestTransitionScalesOnlyTheLean() {
    AimPose pose;
    const HeadPose in = Tracked();
    pose.Apply(in, false, false, 1.0f, 0);
    pose.Apply(in, true, false, 1.0f, 100);
    const HeadPose mid = pose.Apply(in, true, false, 1.0f, 100 + cameraunlock::ads::AdsFade::kLowerMs / 2);
    const float scale = mid.lean.x / in.lean.x;
    Check(scale > 0.0f && scale < 1.0f, "halfway into the aim the lean is part way out");
    Check(SameLean(mid, in.lean, scale), "the lean scales as one on every axis");
    Check(SameRotation(mid, in), "rotation is untouched mid-transition");
}

// Reverses mid-way at `t` by the aim button, or by the true free look key, and holds the lean
// before and after to the step one frame of easing can make.
void TestReversalContinues(bool byToggle) {
    AimPose pose;
    const HeadPose in = Tracked();
    pose.Apply(in, false, false, 1.0f, 0);
    pose.Apply(in, true, false, 1.0f, 100);
    const unsigned long long t = 100 + cameraunlock::ads::AdsFade::kLowerMs / 2;
    const HeadPose before = pose.Apply(in, true, false, 1.0f, t);
    const HeadPose after = byToggle ? pose.Apply(in, true, true, 1.0f, t + 1) : pose.Apply(in, false, false, 1.0f, t + 1);
    Check(std::fabs(after.lean.x - before.lean.x) < 0.01f * std::fabs(in.lean.x),
          std::string("a reversal by the ") + (byToggle ? "true free look key" : "aim button") +
              " carries on from where the lean was");
    HeadPose settled;
    for (unsigned long long k = t + 17; k <= t + 17 + kSettled; k += 16) {
        settled = byToggle ? pose.Apply(in, true, true, 1.0f, k) : pose.Apply(in, false, false, 1.0f, k);
    }
    Check(SameLean(settled, in.lean, 1.0f), "after the reversal the lean returns in full");
}

void TestZoomScalesAllButRoll() {
    Check(ZoomFactor(47.0f, 47.0f) == 1.0f, "the zoom factor is exactly 1 when nothing zooms");
    const float factor = ZoomFactor(23.5f, 47.0f);
    Check(factor > 0.45f && factor < 0.5f, "a view narrowed to half the FOV halves the factor, near enough");
    AimPose pose;
    const HeadPose in = Tracked();
    const HeadPose out = pose.Apply(in, false, false, factor, 0);
    constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;
    Check(Near(std::tan(out.yaw * kDegToRad), std::tan(in.yaw * kDegToRad) * factor) &&
              Near(std::tan(out.pitch * kDegToRad), std::tan(in.pitch * kDegToRad) * factor),
          "yaw and pitch scale with the zoom");
    Check(out.roll == in.roll, "roll does not scale with the zoom");
    Check(SameLean(out, in.lean, factor), "the lean scales with the zoom");
}

void TestStopReturnsToTheHip() {
    AimPose pose;
    const HeadPose in = Tracked();
    pose.Apply(in, false, false, 1.0f, 0);
    for (unsigned long long t = 16; t <= 16 + kSettled; t += 16) pose.Apply(in, true, false, 1.0f, t);
    pose.Stop();
    const HeadPose out = pose.Apply(in, false, false, 1.0f, 1000);
    Check(SameLean(out, in.lean, 1.0f), "after a stop the next frame at the hip has the whole lean");
}

}  // namespace

int main() {
    TestHipPassesThrough();
    TestSightsLockedEasesOnlyTheLean();
    TestTrueFreeLookKeepsEverything();
    TestTransitionScalesOnlyTheLean();
    TestReversalContinues(false);
    TestReversalContinues(true);
    TestZoomScalesAllButRoll();
    TestStopReturnsToTheHip();
    if (g_failures == 0) {
        std::printf("aim pose tests: all passed\n");
        return 0;
    }
    std::printf("aim pose tests: %d failure(s)\n", g_failures);
    return 1;
}
