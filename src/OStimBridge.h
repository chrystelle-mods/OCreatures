#pragma once

namespace OStim
{
    class ThreadInterface;
}

namespace OCreatures::OStimBridge
{
    // Call at kPostLoad. Returns false when OStim's thread interface is unavailable.
    bool Init();
    OStim::ThreadInterface* Threads();
}
