#include "OStimBridge.h"

#include "Swaps.h"

#include "PluginInterface/InterfaceExchangeMessage.h"
#include "PluginInterface/Threading/ThreadInterface.h"

namespace OCreatures::OStimBridge
{
    namespace
    {
        OStim::ThreadInterface* g_threads = nullptr;

        std::vector<RE::FormID> ThreadActors(OStim::Thread* a_thread)
        {
            std::vector<RE::FormID> ids;
            const std::uint32_t     n = a_thread->getActorCount();
            for (std::uint32_t i = 0; i < n; ++i) {
                auto* ta = a_thread->getActor(i);
                if (auto* actor = ta ? static_cast<RE::Actor*>(ta->getGameActor()) : nullptr) {
                    ids.push_back(actor->GetFormID());
                }
            }
            return ids;
        }

        class ThreadStartListener : public OStim::ThreadEventListener
        {
        public:
            void listen(OStim::Thread* a_thread) override
            {
                if (!a_thread) {
                    return;
                }
                const auto threadID = a_thread->getThreadID();
                auto       ids = ThreadActors(a_thread);
                SKSE::GetTaskInterface()->AddTask([threadID, ids]() {
                    for (const auto id : ids) {
                        Swaps::Apply(threadID, RE::TESForm::LookupByID<RE::Actor>(id));
                    }
                });
            }
        };

        class ThreadStopListener : public OStim::ThreadEventListener
        {
        public:
            void listen(OStim::Thread* a_thread) override
            {
                if (!a_thread) {
                    return;
                }
                const auto threadID = a_thread->getThreadID();
                SKSE::GetTaskInterface()->AddTask([threadID]() { Swaps::RestoreThread(threadID); });
            }
        };

        ThreadStartListener g_start;
        ThreadStopListener  g_stop;
    }

    bool Init()
    {
        OStim::InterfaceExchangeMessage msg;
        SKSE::GetMessagingInterface()->Dispatch(OStim::InterfaceExchangeMessage::MESSAGE_TYPE, &msg,
                                                sizeof(OStim::InterfaceExchangeMessage), "OStim");
        if (!msg.interfaceMap) {
            logger::error("OStim not found; OCreatures is inactive");
            return false;
        }
        g_threads = static_cast<OStim::ThreadInterface*>(msg.interfaceMap->queryInterface(OStim::ThreadInterface::NAME));
        if (!g_threads) {
            logger::error("OStim Threads interface unavailable; OCreatures is inactive");
            return false;
        }
        g_threads->registerThreadStartListener(&g_start);
        g_threads->registerThreadStopListener(&g_stop);
        logger::info("Registered OStim thread start/stop listeners");
        return true;
    }
}
