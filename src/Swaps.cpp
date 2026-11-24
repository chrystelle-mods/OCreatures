#include "Swaps.h"

#include "SkinMap.h"

namespace OCreatures::Swaps
{
    namespace
    {
        constexpr std::uint32_t kUniqueID = 'OCRE';
        constexpr std::uint32_t kRecordType = 'SWAP';
        constexpr std::uint32_t kVersion = 1;

        struct Record
        {
            RE::FormID   actor;
            RE::FormID   armor;
            std::int32_t thread;
        };

        std::mutex          g_lock;
        std::vector<Record> g_records;

        void Remove(const Record& a_record)
        {
            auto* actor = RE::TESForm::LookupByID<RE::Actor>(a_record.actor);
            auto* armor = RE::TESForm::LookupByID<RE::TESObjectARMO>(a_record.armor);
            if (!actor || !armor) {
                return;
            }
            RE::ActorEquipManager::GetSingleton()->UnequipObject(actor, armor, nullptr, 1, nullptr, false, true, false, true);
            actor->RemoveItem(armor, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
            logger::info("Restored {:08X}: removed {:08X}", a_record.actor, a_record.armor);
        }

        template <class Pred>
        void RemoveWhere(Pred a_pred)
        {
            std::vector<Record> matched;
            {
                std::scoped_lock lock(g_lock);
                for (auto it = g_records.begin(); it != g_records.end();) {
                    if (a_pred(*it)) {
                        matched.push_back(*it);
                        it = g_records.erase(it);
                    } else {
                        ++it;
                    }
                }
            }
            for (const auto& r : matched) {
                Remove(r);
            }
        }

        void OnSave(SKSE::SerializationInterface* a_intfc)
        {
            std::scoped_lock lock(g_lock);
            if (!a_intfc->OpenRecord(kRecordType, kVersion)) {
                logger::error("Co-save: could not open record");
                return;
            }
            const auto count = static_cast<std::uint32_t>(g_records.size());
            a_intfc->WriteRecordData(count);
            for (const auto& r : g_records) {
                a_intfc->WriteRecordData(r.actor);
                a_intfc->WriteRecordData(r.armor);
            }
        }

        void OnLoad(SKSE::SerializationInterface* a_intfc)
        {
            std::scoped_lock lock(g_lock);
            g_records.clear();

            std::uint32_t type, version, length;
            while (a_intfc->GetNextRecordInfo(type, version, length)) {
                if (type != kRecordType || version != kVersion) {
                    continue;
                }
                std::uint32_t count = 0;
                a_intfc->ReadRecordData(count);
                for (std::uint32_t i = 0; i < count; ++i) {
                    RE::FormID actor = 0, armor = 0;
                    a_intfc->ReadRecordData(actor);
                    a_intfc->ReadRecordData(armor);
                    if (a_intfc->ResolveFormID(actor, actor) && a_intfc->ResolveFormID(armor, armor)) {
                        g_records.push_back({ actor, armor, -1 });
                    }
                }
            }
            if (!g_records.empty()) {
                logger::info("Co-save: {} swaps left over from the saved game", g_records.size());
            }
        }

        void OnRevert(SKSE::SerializationInterface*)
        {
            std::scoped_lock lock(g_lock);
            g_records.clear();
        }
    }

    void Apply(std::int32_t a_threadID, RE::Actor* a_actor)
    {
        if (!a_actor) {
            return;
        }
        const auto armors = SkinMap::ArmorFor(a_actor);
        if (armors.empty()) {
            return;
        }

        auto* equip = RE::ActorEquipManager::GetSingleton();
        for (auto* armor : armors) {
            a_actor->AddObjectToContainer(armor, nullptr, 1, nullptr);
            equip->EquipObject(a_actor, armor, nullptr, 1, nullptr, false, true, false, true);
            {
                std::scoped_lock lock(g_lock);
                g_records.push_back({ a_actor->GetFormID(), armor->GetFormID(), a_threadID });
            }
            logger::info("Thread {}: {:08X} '{}' equipped {:08X}", a_threadID, a_actor->GetFormID(), a_actor->GetName(), armor->GetFormID());
        }
        if (a_actor->Is3DLoaded()) {
            a_actor->NotifyAnimationGraph("SOSFastErect"sv);
        }
    }

    void RestoreThread(std::int32_t a_threadID)
    {
        RemoveWhere([a_threadID](const Record& r) { return r.thread == a_threadID; });
    }

    void RestoreAll()
    {
        RemoveWhere([](const Record&) { return true; });
    }

    void RegisterSerialization()
    {
        auto* serialization = SKSE::GetSerializationInterface();
        serialization->SetUniqueID(kUniqueID);
        serialization->SetSaveCallback(OnSave);
        serialization->SetLoadCallback(OnLoad);
        serialization->SetRevertCallback(OnRevert);
    }
}
