#include "BeastStart.h"

#include "OStimBridge.h"

#include "PluginInterface/Threading/ThreadInterface.h"

namespace OCreatures::BeastStart
{
    namespace
    {
        std::atomic_bool g_ostimHasActor{ false };

        std::uint32_t GamepadKeyCode(std::uint32_t a_mask)
        {
            switch (a_mask) {
            case 0x1: return 266;
            case 0x2: return 267;
            case 0x4: return 268;
            case 0x8: return 269;
            case 0x10: return 270;
            case 0x20: return 271;
            case 0x40: return 272;
            case 0x80: return 273;
            case 0x100: return 274;
            case 0x200: return 275;
            case 0x1000: return 276;
            case 0x2000: return 277;
            case 0x4000: return 278;
            case 0x8000: return 279;
            case 0x9: return 280;
            case 0xA: return 281;
            default: return 282;
            }
        }

        std::uint32_t KeyCode(RE::ButtonEvent* a_event)
        {
            const auto id = a_event->GetIDCode();
            switch (a_event->GetDevice()) {
            case RE::INPUT_DEVICE::kMouse: return 256 + id;
            case RE::INPUT_DEVICE::kGamepad: return GamepadKeyCode(id);
            default: return id;
            }
        }

        std::uint32_t StartKey()
        {
            static auto* global = RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESGlobal>(0xDE7, "OStim.esp");
            return global ? static_cast<std::uint32_t>(global->value) : 200;
        }

        RE::Actor* PickActor(RE::PlayerCharacter* a_player)
        {
            auto* pick = RE::CrosshairPickData::GetSingleton();
#if defined(EXCLUSIVE_SKYRIM_FLAT)
            RE::ObjectRefHandle handles[] = { pick->target, pick->targetActor };
#else
            RE::ObjectRefHandle handles[] = { pick->GetActiveTarget(), pick->targetActor[0] };
#endif
            for (auto& h : handles) {
                auto ref = h.get();
                auto* actor = ref ? ref->As<RE::Actor>() : nullptr;
                if (actor && actor != a_player && !actor->IsDead()) {
                    return actor;
                }
            }

            constexpr float kRange = 300.0f;
            constexpr float kCone = 40.0f;
            RE::Actor* best = nullptr;
            float bestDist = kRange;
            const auto origin = a_player->GetPosition();
            RE::ProcessLists::GetSingleton()->ForEachHighActor([&](RE::Actor* a) {
                if (a && a != a_player && !a->IsDead() && !a->IsDisabled() && a->Is3DLoaded()) {
                    const float dist = origin.GetDistance(a->GetPosition());
                    if (dist < bestDist && a_player->GetHeadingAngle(a->GetPosition(), true) <= kCone) {
                        best = a;
                        bestDist = dist;
                    }
                }
                return RE::BSContainer::ForEachResult::kContinue;
            });
            return best;
        }

        void TryStart()
        {
            auto* player = RE::PlayerCharacter::GetSingleton();
            auto* race = player ? player->GetRace() : nullptr;
            if (!race || race->HasKeywordString("ActorTypeNPC") || g_ostimHasActor) {
                return;
            }
            auto* threads = OStimBridge::Threads();
            if (!threads || threads->getActor(player)) {
                return;
            }
            auto* target = PickActor(player);
            if (!target) {
                logger::info("Beast start: no actor in front of the player");
                return;
            }
            void* actors[2] = { player, target };
            auto* builder = threads->createThreadBuilder(2, actors);
            if (!builder) {
                logger::info("Beast start: {:X} or the player is not eligible", target->GetFormID());
                return;
            }
            logger::info("Beast start: race {:X} with {:X}", race->GetFormID(), target->GetFormID());
            builder->start();
        }

        class CrosshairSink : public RE::BSTEventSink<SKSE::CrosshairRefEvent>
        {
        public:
            RE::BSEventNotifyControl ProcessEvent(const SKSE::CrosshairRefEvent* a_event,
                                                  RE::BSTEventSource<SKSE::CrosshairRefEvent>*) override
            {
                g_ostimHasActor = a_event && a_event->crosshairRef && a_event->crosshairRef->Is(RE::FormType::ActorCharacter);
                return RE::BSEventNotifyControl::kContinue;
            }
        };

        class InputSink : public RE::BSTEventSink<RE::InputEvent*>
        {
        public:
            RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_events, RE::BSTEventSource<RE::InputEvent*>*) override
            {
                auto* ui = RE::UI::GetSingleton();
                if (!a_events || !ui || ui->GameIsPaused() || !ui->IsCursorHiddenWhenTopmost()) {
                    return RE::BSEventNotifyControl::kContinue;
                }
                for (auto* e = *a_events; e; e = e->next) {
                    auto* button = e->AsButtonEvent();
                    if (button && button->IsDown() && KeyCode(button) == StartKey()) {
                        TryStart();
                    }
                }
                return RE::BSEventNotifyControl::kContinue;
            }
        };

        CrosshairSink g_crosshair;
        InputSink     g_input;
    }

    void Register()
    {
        if (!OStimBridge::Threads()) {
            return;
        }
        if (auto* source = SKSE::GetCrosshairRefEventSource()) {
            source->AddEventSink(&g_crosshair);
        }
        if (auto* input = RE::BSInputDeviceManager::GetSingleton()) {
            input->AddEventSink(&g_input);
        }
        logger::info("Beast-form scene start registered");
    }
}
