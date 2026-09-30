#include "core/aim_projection.h"
#include "core/rotation_math.h"

#include <cstdio>
#include <initializer_list>
#include <limits>

int main() {
    int failures = 0;
    const auto check = [&](bool result, const char* name) {
        if (!result) { std::printf("FAIL: %s\n", name); ++failures; }
    };
    const auto near = [](float a, float b) { return std::fabs(a - b) < 0.0001f; };
    const float forward[3] = {1, 0, 0}, up[3] = {0, 1, 0};
    float x = 0.0f, y = 0.0f;
    for (float distance : {0.5f, 3.0f, 30.0f}) {
        for (float lean : {-0.2f, 0.2f}) {
            check(DL2HT::ProjectAim({-distance, 0, -lean}, forward, up, x, y) &&
                  near(x, lean / distance) && near(y, 0), "lateral parallax at near and far ranges");
            check(DL2HT::ProjectAim({-distance, -lean, 0}, forward, up, x, y) &&
                  near(x, 0) && near(y, -lean / distance), "vertical parallax at near and far ranges");
        }
    }
    float trackedForward[3] = {1, 0, 0}, trackedUp[3] = {0, 1, 0};
    DL2HT::ApplyHeadTrackingRotation(trackedForward, trackedUp, 0.3f, -0.2f, 0.1f, DL2HT::YawMode::WorldLocked);
    const cameraunlock::math::Vec3 f(trackedForward[0], trackedForward[1], trackedForward[2]);
    const cameraunlock::math::Vec3 u(trackedUp[0], trackedUp[1], trackedUp[2]);
    const auto r = cameraunlock::math::Vec3::Cross(u, f);
    check(DL2HT::ProjectAim(f * -5.0f + r * 0.5f + u, trackedForward, trackedUp, x, y) &&
          near(x, 0.1f) && near(y, 0.2f), "combined pose uses written basis");
    check(!DL2HT::ProjectAim({1, 0, 0}, forward, up, x, y), "behind camera hidden");
    check(!DL2HT::ProjectAim({-0.00001f, 1, 0}, forward, up, x, y), "near camera plane hidden");
    check(!DL2HT::ProjectAim({}, forward, up, x, y), "eye at impact hidden");
    check(DL2HT::AimScreenPosition(0.1f, 0.2f, 90, 1600, 900, x, y) &&
          near(x, 845) && near(y, 360), "vertical FOV pixel scale");
    check(!DL2HT::AimScreenPosition(2, 0, 90, 1600, 900, x, y), "offscreen aim hidden without clamping");
    check(!DL2HT::AimScreenPosition(0, 0, 0, 1600, 900, x, y), "unreadable FOV hidden");
    check(!DL2HT::AimScreenPosition(std::numeric_limits<float>::quiet_NaN(), 0, 90, 1600, 900, x, y),
          "nonfinite projection hidden");
    return failures ? 1 : 0;
}
