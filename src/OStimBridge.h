#pragma once

namespace OCreatures::OStimBridge
{
    // Call at kPostLoad. Returns false when OStim's thread interface is unavailable.
    bool Init();
}
