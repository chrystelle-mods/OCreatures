#pragma once

namespace OCreatures::SkinMap
{
    struct Entry
    {
        std::string                                               name;
        std::string                                               type;
        std::vector<RE::TESObjectARMO*>                           armor;
        std::map<std::string, std::vector<RE::TESObjectARMO*>>    variants;
    };

    void Load();

    const Entry* Find(RE::TESObjectARMO* a_skin);

    // The add-on armor for an actor in its current form, or empty if its skin is not mapped.
    std::vector<RE::TESObjectARMO*> ArmorFor(RE::Actor* a_actor);
}
