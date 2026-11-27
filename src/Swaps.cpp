#include "Swaps.h"

#include "SkinMap.h"

namespace OCreatures::Swaps
{
    namespace
    {
        constexpr std::uint32_t kUniqueID = 'OCRE';
        constexpr std::uint32_t kRecordType = 'SWAP';
        constexpr std::uint32_t kVersion = 2;
        constexpr std::int32_t  kPending = -1;
        // Longest vanilla cell reset (cleared cells). Past this the creature has respawned clean.
        constexpr float kMaxPendingDays = 30.0f;

        struct Record
        {
            RE::FormID   actor;
            RE::FormID   armor;
            std::int32_t thread;
            float        day;
        };

        std::mutex          g_lock;
        std::vector<Record> g_records;

        float Today()
        {
            auto* calendar = RE::Calendar::GetSingleton();
            return calendar ? calendar->GetDaysPassed() : 0.0f;
        }

        enum class Result
        {
            kRemoved,
            kGone,
            kNotInMemory
        };

        Result Remove(const Record& a_record)
        {
            auto* armor = RE::TESForm::LookupByID<RE::TESObjectARMO>(a_record.armor);
            if (!armor) {
                return Result::kGone;
            }
            auto* actor = RE::TESForm::LookupByID<RE::Actor>(a_record.actor);
            if (!actor) {
                return Result::kNotInMemory;
            }
            if (actor->IsDeleted()) {
                return Result::kGone;
            }
            RE::ActorEquipManager::GetSingleton()->UnequipObject(actor, armor, nullptr, 1, nullptr, false, true, false, true);
            actor->RemoveItem(armor, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
            logger::info("Restored {:08X}: removed {:08X}", a_record.actor, a_record.armor);
            return Result::kRemoved;
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

            const float         today = Today();
            std::vector<Record> pending;
            for (auto r : matched) {
                if (Remove(r) != Result::kNotInMemory) {
                    continue;
                }
                if (r.thread != kPending) {
                    r.thread = kPending;
                    r.day = today;
                }
                if (today - r.day > kMaxPendingDays) {
                    logger::info("Dropped {:08X}/{:08X}: not loaded for {:.0f} days", r.actor, r.armor, today - r.day);
                    continue;
                }
                logger::info("Pending {:08X}: not in memory, will remove {:08X} when it loads", r.actor, r.armor);
                pending.push_back(r);
            }
            if (!pending.empty()) {
                std::scoped_lock lock(g_lock);
                g_records.insert(g_records.end(), pending.begin(), pending.end());
            }
        }

        class LoadSink : public RE::BSTEventSink<RE::TESObjectLoadedEvent>
        {
        public:
            RE::BSEventNotifyControl ProcessEvent(const RE::TESObjectLoadedEvent* a_event,
                                                  RE::BSTEventSource<RE::TESObjectLoadedEvent>*) override
            {
                if (!a_event || !a_event->loaded) {
                    return RE::BSEventNotifyControl::kContinue;
                }
                const auto id = a_event->formID;
                bool       hit = false;
                {
                    std::scoped_lock lock(g_lock);
                    hit = std::ranges::any_of(g_records, [id](const Record& r) { return r.actor == id && r.thread == kPending; });
                }
                if (hit) {
                    SKSE::GetTaskInterface()->AddTask([id]() {
                        RemoveWhere([id](const Record& r) { return r.actor == id && r.thread == kPending; });
                    });
                }
                return RE::BSEventNotifyControl::kContinue;
            }
        };

        LoadSink g_loadSink;

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
                a_intfc->WriteRecordData(r.day);
            }
        }

        void OnLoad(SKSE::SerializationInterface* a_intfc)
        {
            std::scoped_lock lock(g_lock);
            g_records.clear();

            const float   today = Today();
            std::uint32_t type, version, length;
            while (a_intfc->GetNextRecordInfo(type, version, length)) {
                if (type != kRecordType || version < 1 || version > kVersion) {
                    continue;
                }
                std::uint32_t count = 0;
                a_intfc->ReadRecordData(count);
                for (std::uint32_t i = 0; i < count; ++i) {
                    RE::FormID actor = 0, armor = 0;
                    float      day = today;
                    a_intfc->ReadRecordData(actor);
                    a_intfc->ReadRecordData(armor);
                    if (version >= 2) {
                        a_intfc->ReadRecordData(day);
                    }
                    if (a_intfc->ResolveFormID(actor, actor) && a_intfc->ResolveFormID(armor, armor)) {
                        g_records.push_back({ actor, armor, kPending, day });
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

        const auto id = a_actor->GetFormID();
        RemoveWhere([id](const Record& r) { return r.actor == id && r.thread == kPending; });

        auto*       equip = RE::ActorEquipManager::GetSingleton();
        const float today = Today();
        for (auto* armor : armors) {
            a_actor->AddObjectToContainer(armor, nullptr, 1, nullptr);
            equip->EquipObject(a_actor, armor, nullptr, 1, nullptr, false, true, false, true);
            {
                std::scoped_lock lock(g_lock);
                g_records.push_back({ id, armor->GetFormID(), a_threadID, today });
            }
            logger::info("Thread {}: {:08X} '{}' equipped {:08X}", a_threadID, id, a_actor->GetName(), armor->GetFormID());
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

    void RegisterLoadSink()
    {
        if (auto* holder = RE::ScriptEventSourceHolder::GetSingleton()) {
            holder->AddEventSink<RE::TESObjectLoadedEvent>(&g_loadSink);
        }
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
