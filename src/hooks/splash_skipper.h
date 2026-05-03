#pragma once

namespace DL2HT {

// Posts ESC/Space to the game window during the publisher-splash and
// intro-cutscene sequence so the launch-to-main-menu cycle is faster
// during development. Stops the moment the game reports being on the
// main menu, or after a hard timeout.
//
// Opt-in via Config::skipSplash; the production default keeps it off
// so end users aren't surprised by ghost key presses.
void StartSplashSkipper();
void StopSplashSkipper();

} // namespace DL2HT
