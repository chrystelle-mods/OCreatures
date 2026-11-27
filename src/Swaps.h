#pragma once

namespace OCreatures::Swaps
{
    // Main thread only.
    void Apply(std::int32_t a_threadID, RE::Actor* a_actor);
    void RestoreThread(std::int32_t a_threadID);
    void RestoreAll();

    void RegisterLoadSink();

    void RegisterSerialization();
}
