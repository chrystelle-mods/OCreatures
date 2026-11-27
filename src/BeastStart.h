#pragma once

namespace OCreatures::BeastStart
{
    // In a non-ActorTypeNPC race (werewolf, werebear, vampire lord), OStim's scene-start key finds no target (SKSE's crosshair ref stays empty without an
    // activation prompt). When that happens, start the same thread OStim would with the actor the player faces.
    void Register();
}
