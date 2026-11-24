#include "SkinMap.h"

#include <nlohmann/json.hpp>

namespace OCreatures::SkinMap
{
    namespace
    {
        constexpr auto kDirectory = "Data/SKSE/Plugins/OCreatures/Creatures"sv;

        std::unordered_map<RE::TESObjectARMO*, Entry> g_entries;

        // "Plugin.esp|0xID" or the legacy "__formData|Plugin.esp|0xID".
        RE::TESObjectARMO* ResolveArmor(const std::string& a_text, const std::string& a_file)
        {
            std::string_view text = a_text;
            if (text.starts_with("__formData|")) {
                text.remove_prefix("__formData|"sv.size());
            }
            const auto bar = text.find('|');
            if (bar == std::string_view::npos) {
                logger::warn("{}: malformed form string '{}'", a_file, a_text);
                return nullptr;
            }
            const std::string plugin{ text.substr(0, bar) };
            const std::string idText{ text.substr(bar + 1) };

            RE::FormID id = 0;
            try {
                id = static_cast<RE::FormID>(std::stoul(idText, nullptr, 16));
            } catch (...) {
                logger::warn("{}: bad FormID in '{}'", a_file, a_text);
                return nullptr;
            }

            auto* armor = RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESObjectARMO>(id, plugin);
            if (!armor) {
                logger::warn("{}: '{}' is not a loaded armor", a_file, a_text);
            }
            return armor;
        }

        std::vector<RE::TESObjectARMO*> ReadArmorList(const nlohmann::json& a_value, const std::string& a_file)
        {
            std::vector<RE::TESObjectARMO*> out;
            auto add = [&](const nlohmann::json& v) {
                if (v.is_string()) {
                    if (auto* armor = ResolveArmor(v.get<std::string>(), a_file)) {
                        out.push_back(armor);
                    }
                }
            };
            if (a_value.is_array()) {
                for (const auto& v : a_value) {
                    add(v);
                }
            } else {
                add(a_value);
            }
            return out;
        }

        void LoadFile(const std::filesystem::path& a_path)
        {
            const auto file = a_path.filename().string();
            nlohmann::json json;
            try {
                std::ifstream in(a_path);
                json = nlohmann::json::parse(in, nullptr, true, true);
            } catch (const std::exception& e) {
                logger::error("{}: parse error: {}", file, e.what());
                return;
            }

            const auto creatures = json.find("creatures");
            if (creatures == json.end() || !creatures->is_array()) {
                logger::warn("{}: no 'creatures' array", file);
                return;
            }

            std::size_t loaded = 0;
            for (const auto& c : *creatures) {
                if (!c.is_object()) {
                    continue;
                }
                const auto skinText = c.contains("skin") ? c["skin"] : c.value("skinForm", nlohmann::json{});
                if (!skinText.is_string()) {
                    continue;
                }
                auto* skin = ResolveArmor(skinText.get<std::string>(), file);
                if (!skin) {
                    continue;
                }

                Entry entry;
                entry.name = c.value("name", c.value("skinName", std::string{}));
                entry.type = c.value("type", std::string{});
                if (c.contains("armor")) {
                    entry.armor = ReadArmorList(c["armor"], file);
                } else {
                    for (const auto* key : { "arousedSkinForm", "addonForm" }) {
                        if (c.contains(key)) {
                            auto list = ReadArmorList(c[key], file);
                            entry.armor.insert(entry.armor.end(), list.begin(), list.end());
                        }
                    }
                }
                if (const auto variants = c.find("variants"); variants != c.end() && variants->is_object()) {
                    for (const auto& [name, value] : variants->items()) {
                        entry.variants[name] = ReadArmorList(value, file);
                    }
                }

                if (entry.armor.empty()) {
                    logger::warn("{}: '{}' has no usable armor", file, entry.name);
                    continue;
                }
                g_entries[skin] = std::move(entry);
                ++loaded;
            }
            logger::info("{}: {} creature skins", file, loaded);
        }
    }

    void Load()
    {
        g_entries.clear();

        std::error_code ec;
        if (!std::filesystem::is_directory(kDirectory, ec)) {
            logger::warn("Skin map folder {} not found", kDirectory);
            return;
        }

        std::vector<std::filesystem::path> files;
        for (const auto& item : std::filesystem::directory_iterator(kDirectory, ec)) {
            if (item.is_regular_file() && item.path().extension() == ".json") {
                files.push_back(item.path());
            }
        }
        std::ranges::sort(files);
        for (const auto& path : files) {
            LoadFile(path);
        }
        logger::info("Skin map: {} skins from {} files", g_entries.size(), files.size());
    }

    const Entry* Find(RE::TESObjectARMO* a_skin)
    {
        const auto it = g_entries.find(a_skin);
        return it != g_entries.end() ? &it->second : nullptr;
    }

    std::vector<RE::TESObjectARMO*> ArmorFor(RE::Actor* a_actor)
    {
        if (!a_actor) {
            return {};
        }
        const Entry* entry = Find(a_actor->GetSkin());
        if (!entry) {
            if (auto* race = a_actor->GetRace()) {
                entry = Find(race->skin);
            }
        }
        return entry ? entry->armor : std::vector<RE::TESObjectARMO*>{};
    }
}
