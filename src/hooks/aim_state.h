#pragma once

namespace DL2HT {

// What the first-person camera is doing this frame, read from the game's own state.
struct FppCameraSample {
    // The camera being moved is the one the level renders the view from. Only its update reads
    // the sights.
    bool view = false;
    // It is also the player's first-person camera (CameraFPPDI). Everything below is meaningless
    // when false.
    bool fpp = false;
    // Aiming down the sights of a firearm, polled from the player's firearm module every frame.
    bool aiming = false;
    // Whether both FOV terms were readable. When false, the zoom factor is not known.
    bool fovKnown = false;
    // Vertical FOV the camera is projecting with this frame, degrees.
    float liveFovDeg = 0.0f;
    // Vertical FOV the game renders at when nothing zooms: the FPP camera's own FOV before the
    // zoom divisor and the animation override, degrees. Equal to liveFovDeg at the hip.
    float baseFovDeg = 0.0f;
};

// Resolves the accessors from the loaded game code, logging which parts it found. Whatever it
// could not find, SampleFppCamera reports as unknown: the sights down and the FOV unreadable.
void InitializeAimState();

// `innerCamera` is the engine camera object MoveCameraFromForwardUpPos was called on, `level` the
// active ILevel. Call on the thread that runs the camera update.
FppCameraSample SampleFppCamera(void* innerCamera, void* level);

} // namespace DL2HT
