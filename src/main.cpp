#include "PCH.h"

#include "OStimBridge.h"
#include "SkinMap.h"
#include "Swaps.h"

using namespace SKSE;
using namespace SKSE::log;
using namespace SKSE::stl;

namespace
{
    void InitializeLogging()
    {
        auto path = log_directory();
        if (!path) {
            report_and_fail("OCreatures: unable to lookup SKSE logs directory."sv);
        }
        *path /= "OCreatures.log";

        std::shared_ptr<spdlog::logger> log;
        if (IsDebuggerPresent()) {
            log = std::make_shared<spdlog::logger>("Global", std::make_shared<spdlog::sinks::msvc_sink_mt>());
        } else {
            log = std::make_shared<spdlog::logger>(
                "Global", std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true));
        }
        log->set_level(spdlog::level::info);
        log->flush_on(spdlog::level::info);

        spdlog::set_default_logger(std::move(log));
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
    }

    void MessageHandler(SKSE::MessagingInterface::Message* a_msg)
    {
        switch (a_msg->type) {
        case SKSE::MessagingInterface::kPostLoad:
            OCreatures::OStimBridge::Init();
            break;
        case SKSE::MessagingInterface::kDataLoaded:
            OCreatures::SkinMap::Load();
            break;
        case SKSE::MessagingInterface::kPostLoadGame:
        case SKSE::MessagingInterface::kNewGame:
            OCreatures::Swaps::RestoreAll();
            break;
        default:
            break;
        }
    }
}

extern "C" DLLEXPORT constinit auto SKSEPlugin_Version = []() {
    SKSE::PluginVersionData v;
    v.PluginVersion(REL::Version("2.0.0.0"sv));
    v.PluginName("OCreatures");
    v.UsesAddressLibrary();
    v.UsesNoStructs();
    return v;
}();

extern "C" DLLEXPORT bool SKSEPlugin_Query(const SKSE::QueryInterface*, SKSE::PluginInfo* a_info)
{
    a_info->infoVersion = SKSE::PluginInfo::kVersion;
    a_info->name = "OCreatures";
    a_info->version = 0x02000000;
    return true;
}

extern "C" DLLEXPORT bool SKSEPlugin_Load(const LoadInterface* a_skse)
{
    InitializeLogging();
    logger::info("OCreatures 2.0.0 loading");

    Init(a_skse, false);

    OCreatures::Swaps::RegisterSerialization();

    if (!SKSE::GetMessagingInterface()->RegisterListener(MessageHandler)) {
        return false;
    }
    return true;
}
