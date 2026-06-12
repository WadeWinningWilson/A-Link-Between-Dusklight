#include <dusk/archipelago/archipelago_context.hpp>

#include <thread>

#include "Archipelago.h"
#include "d/d_item.h"
#include "dusk/config.hpp"
#include "dusk/logging.h"
#include "dusk/randomizer/game/tools.h"
#include "dusk/ui/ui.hpp"

namespace dusk::archi
{

static constexpr int ARCHI_ITEM_OFFSET = 2320000;

struct SettingsNameConvert {
    std::string apName;
    std::string dusklightName;
    bool invert = false;
};

static auto sArchiSettingToDusklight = std::to_array<SettingsNameConvert>({
    {"", ""},
    {"golden_bugs_shuffled", "Golden Bugs"},
    {"sky_characters_shuffled", "Sky Characters"},
    {"npc_items_shuffled", "Gifts From NPCs"},
    {"shop_items_shuffled", "Shop Items"},
    {"hidden_skills_shuffled", "Hidden Skills"},
    // {"poe_shuffled", ""}, // poe shuffle is Overworld, Dungeon, All, or Vanilla, so special logic is needed to convert
    // {"heart_piece_shuffled", ""},
    // {"overworld_shuffled", ""},
    // {"dungeons_shuffled", ""},
    {"dungeon_rewards_progression", "Dungeon Rewards Can Be Anywhere"},
    {"small_keys_on_bosses", "No Small Keys on Bosses", true},
    {"skip_prologue", "Skip Prologue"},
    {"faron_twilight_cleared", "Faron Twilight Cleared"},
    {"eldin_twilight_cleared", "Eldin Twilight Cleared"},
    {"lanayru_twilight_cleared", "Lanayru Twilight Cleared"},
    {"skip_mdh", "Skip Midna's Desparate Hour"},
    {"open_map", "Unlock Map Regions"},
    {"increase_wallet", "Increase Wallet Capacity"},
    {"transform_anywhere", "Logic Transform Anywhere"},
    {"bonks_do_damage", "Bonks Do Damage"},
    {"skip_lakebed_entrance", "Lakebed Does Not Require Water Bombs"},
    {"skip_arbiters_grounds_entrance", "Arbiters Does Not Require Bulblin Camp"},
    {"skip_snowpeak_entrance", "Snowpeak Does Not Require Reekfish Scent"},
    {"skip_city_in_the_sky_entrance", "City Does Not Require Filled Skybook"},
});

ArchipelagoContext& instance() {
    static ArchipelagoContext instance;
    return instance;
}

const SettingsNameConvert& GetAPSettingNameConvert(const std::string& apSettingName) {
    for (const auto& entry : sArchiSettingToDusklight) {
        if (entry.apName == apSettingName)
            return entry;
    }
    return sArchiSettingToDusklight[0];
}

const char* getMessageTypeName(AP_MessageType type) {
    switch (type) {
    case AP_MessageType::Plaintext:
        return "Plaintext";
    case AP_MessageType::ItemSend:
        return "ItemSend";
    case AP_MessageType::ItemRecv:
        return "ItemRecv";
    case AP_MessageType::Hint:
        return "Hint";
    case AP_MessageType::Countdown:
        return "Countdown";
    default:
        return nullptr;
    }
}

void ParseMessageData() {
    auto msg = AP_GetLatestMessage();

    switch (msg->type) {
    case AP_MessageType::ItemRecv: {
        auto recvMsg = (AP_ItemRecvMessage*)msg;

        ui::push_toast({
            .title = "Item Received",
            .content = fmt::format("Got {} From {}", recvMsg->item, recvMsg->sendPlayer),
            .duration = std::chrono::seconds(3),
        });
        // fallthrough for debug logging text contents
    }
    case AP_MessageType::Plaintext:
    case AP_MessageType::ItemSend:
    case AP_MessageType::Hint:
    case AP_MessageType::Countdown:
        DuskLog.info("[{}] {}", getMessageTypeName(msg->type), msg->text);
        break;
    default:
        DuskLog.warn("Unknown message type! Type: {}", fmt::underlying(msg->type));
        break;
    }

    AP_ClearLatestMessage();
}

void ArchipelagoContext::LoadTempItemInfo() {
    auto itemDataTree = LOAD_EMBED_YAML(RANDO_DATA_PATH "items.yaml");
    for (const auto& itemNode : itemDataTree) {
        if (!itemNode["APItemId"]) {
            DuskLog.warn("Item {} missing APItemId field!", itemNode["Name"].as<std::string>());
            continue;
        }
        auto apItemId = itemNode["APItemId"].as<int>();

        if (apItemId == -1)
            continue;

        auto id = itemNode["Id"].as<int>();
        auto importance = randomizer::logic::item::ImportanceFromStr(itemNode["Importance"].as<std::string>());
        auto itemName = itemNode["Name"].as<std::string>();

        m_apItemToGameItem[apItemId] = {
            id,
            importance,
            itemName
        };
    }
}

void ArchipelagoContext::LoadTempLocationInfo() {
    auto locDataTree = LOAD_EMBED_YAML(RANDO_DATA_PATH "locations.yaml");
    for (const auto& locNode : locDataTree) {
        const auto& metadata = locNode["Metadata"];
        auto locationName =  locNode["Name"].as<std::string>();

        if (!metadata.IsMap()) {
            DuskLog.warn("Location {} missing correct Metadata field!", locationName);
            continue;
        }

        if (!metadata["APLocationId"]) {
            DuskLog.warn("Location {} missing APLocationId field!", locationName);
            continue;
        }

        auto apLocationId = metadata["APLocationId"].as<int>();

        if (apLocationId == -1)
            continue;

        m_apLocToGameLoc.push_back({
            apLocationId,
            locationName
        });
    }
}

void ArchipelagoContext::itemRecvImpl(int id, bool notify) {
    if (!m_apItemToGameItem.contains(id)) {
        DuskLog.warn("Got an invalid Item Id: {}", id);
        return;
    }

    auto& item = m_apItemToGameItem[id];

    DuskLog.info("[AP] Adding Item: {}", item.itemName);

    if (item.importance == randomizer::logic::item::Importance::MAJOR) {
        g_randomizerState.addItemToEventQueue(item.itemId);
    }else {
        execItemGet(item.itemId);
    }
}

int ArchipelagoContext::getItemIdFromApId(int apId) {
    if (!m_apItemToGameItem.contains(apId)) {
        DuskLog.warn("Got an invalid Item Id: {}", apId);
        return -1;
    }

    auto& item = m_apItemToGameItem[apId];

    return item.itemId;
}

std::string ArchipelagoContext::getLocationNameFromApId(int apId) const {
    for (const auto& entry : m_apLocToGameLoc) {
        if (entry.apId == apId)
            return entry.locName;
    }
    return "";
}

ArchipelagoContext::ArchipelagoContext() {

}

void ArchipelagoContext::SetServerIp(const std::string_view& ip) {
    getSettings().archipelago.serverIP.setValue(std::string(ip));
}

void ArchipelagoContext::SetSlotName(const std::string_view& name) {
    getSettings().archipelago.slotName.setValue(std::string(name));
}

void ArchipelagoContext::SetPassword(const std::string_view& pass) {
    getSettings().archipelago.serverPass.setValue(std::string(pass));
}

const std::string& ArchipelagoContext::GetServerIp() {
    return getSettings().archipelago.serverIP.getValue();
}

const std::string& ArchipelagoContext::GetSlotName() {
    return getSettings().archipelago.slotName.getValue();
}

const std::string& ArchipelagoContext::GetPassword() {
    return getSettings().archipelago.serverPass.getValue();
}

void ArchipelagoContext::ConnectToServer() {
    config::Save();

    instance().LoadTempItemInfo();

    instance().LoadTempLocationInfo();

    AP_Init(GetServerIp().c_str(), "Twilight Princess", GetSlotName().c_str(), GetPassword().c_str());

    AP_NetworkVersion ver{0, 6,7};
    AP_SetClientVersion(&ver);

    AP_SetItemClearCallback([]() {
        DuskLog.info("Item Clear Callback Called!");
        // HandleResetInventory();
    });

    AP_SetItemRecvCallback([](int id, bool notify) {
        DuskLog.info("Item Receive Callback Called! Item: {} Notify: {}", id, notify);
        HandleItemReceived(id, notify);
    });

    AP_SetLocationCheckedCallback([](int loc) {
        DuskLog.info("Location Checked Callback Called! Location: {}", loc);
    });

    AP_SetLocationInfoCallback([](std::vector<AP_NetworkItem> items) {
        DuskLog.info("Got {} Location Scouts from Server.", items.size());
        HandleReceiveLocationScout(items);
    });

    AP_Start();

    if (AP_GetConnectionStatus() == AP_ConnectionStatus::ConnectionRefused) {
        DuskLog.warn("Failed to Connect to Archipelago Server.");
        return;
    }

    std::thread messageThread = std::thread(MainThreadFunc);
    messageThread.detach();
}

void ArchipelagoContext::DisconnectFromServer() {
    if (!IsConnected()) {
        DuskLog.warn("Attempted to disconnect from an already disconnected state!");
        return;
    }

    AP_Shutdown();
}

bool ArchipelagoContext::IsConnected() {
    auto status = AP_GetConnectionStatus();
    return status == AP_ConnectionStatus::Connected || status == AP_ConnectionStatus::Authenticated;
}

void ArchipelagoContext::MainThreadFunc() {
    // wait a bit before checking connection state, as websocket is probably not connected yet
    // (i really am not liking APCpp, why cant I check if the websocket is in the process of connecting???)
    std::this_thread::sleep_for(std::chrono::seconds(2));

    DuskLog.info("AP Thread started.");

    if (IsConnected())
        RequestAllLocationScout();

    while (IsConnected()) {
        if (AP_IsMessagePending())
            ParseMessageData();

        instance().m_queueMutex.lock();
        if (randomizer_IsActive() && !instance().m_inactiveItemsQueue.empty()) {
            for (auto item : instance().m_inactiveItemsQueue) {
                instance().itemRecvImpl(item, false);
            }

            instance().m_inactiveItemsQueue.clear();
        }
        instance().m_queueMutex.unlock();
    }

    DuskLog.info("AP Thread ended.");
}

void ArchipelagoContext::HandleItemReceived(int id, bool notify) {
    int relativeId = id - ARCHI_ITEM_OFFSET;

    //  && ((relativeId >= 0 && relativeId <= 6) || relativeId == 7)
    if (!notify) {
        // skip rupee refills so players cant abuse disconnect/reconnect
        return;
    }

    if (!randomizer_IsActive()) {
        DuskLog.info("Randomizer not active, adding item to queue.");

        instance().m_queueMutex.lock();
        instance().m_inactiveItemsQueue.push_back(relativeId);
        instance().m_queueMutex.unlock();
        return;
    }

    instance().itemRecvImpl(relativeId, notify);
}

void ArchipelagoContext::HandleResetInventory() {
    // NOTE: this does not clear ALL things from save, so if a player managed to do something while disconnected from the archi, it might mess with things

    auto& playerInfo = g_dComIfG_gameInfo.info.getPlayer();

    // reset items
    playerInfo.getItem().init();

    // reset collect (poes, shards, swords)
    playerInfo.getCollect().init();

    playerInfo.getPlayerStatusA().setMaxLife(15);
    playerInfo.getPlayerStatusA().setWalletSize(WALLET);
    // dont reset rupees, and instead reject rupee updates while refilling inv

}

void ArchipelagoContext::HandleReceiveLocationScout(const std::vector<AP_NetworkItem>& items) {
    for (const auto& item : items) {
        int parsedItemId = dItemNo_Randomizer_ARCHIPELAGO_ITEM_e;
        if (item.player == AP_GetPlayerID()) {
            parsedItemId = instance().getItemIdFromApId(item.item - ARCHI_ITEM_OFFSET);
        }
        int locationId = item.location - ARCHI_ITEM_OFFSET;

        auto locName = instance().getLocationNameFromApId(locationId);

        if (locName.empty()) {
            DuskLog.info("No location with ID {} found.", locationId);
            continue;
        }

        instance().m_locationItemInfo[locName] = {
            parsedItemId,
            item.location,
            false
        };
    }
}

void ArchipelagoContext::UpdateCheckedLocations() {
    // TODO: we need a randomizer world to be able to check all valid locations and compare collection
    // state against a previously cached value, and if collected we can send a location update packet.

    return;

    // non-usable example code

    // replace this with however we'll be getting our world
    std::unique_ptr<randomizer::logic::world::World> world = std::make_unique<randomizer::logic::world::World>(1, nullptr);

    for (auto location : world->GetAllLocations()) {
        auto locName = location->GetName();

        if (!instance().m_locationItemInfo.contains(locName)) {
            DuskLog.warn("No item found for ({}).", locName);
            continue;
        }

        auto& cachedLocData = instance().m_locationItemInfo[locName];

        bool isCollected = isLocationObtained(location);

        if (isCollected && !cachedLocData.collected) {
            cachedLocData.collected = true;
            AP_SendItem(cachedLocData.apLocationId);
        }
    }
}

void ArchipelagoContext::RequestAllLocationScout(bool isHint) {
    std::set<int64_t> locations;
    // TEMP: apworld has 475 locations with ids in sequential order, so add them all individually to location set
    // (eventually we will iterate through locations.yaml for a better data-driven solution)
    for (int i = 0; i < 475; i++) {
        locations.insert(ARCHI_ITEM_OFFSET + i);
    }

    AP_SendLocationScouts(locations, isHint);
}

void ArchipelagoContext::SetAPConfigYamlPath(const std::string_view& path) {
    instance().m_apConfigPath = path;
}

bool ArchipelagoContext::GenerateConfigFromAP(randomizer::seedgen::config::Config& outConfig) {
    if (instance().m_apConfigPath.empty()) {
        DuskLog.warn("AP Config Path Empty!");
        return false;
    }

    if (!std::filesystem::exists(instance().m_apConfigPath)) {
        DuskLog.warn("AP Config Path does not exist!");
        return false;
    }

    YAML::Node apConfigYaml;
    try {
        apConfigYaml = YAML::LoadFile(instance().m_apConfigPath);
    }catch (YAML::BadFile& e) {
        DuskLog.warn("Failed to load AP Config YAML file!");
        return false;
    }

    outConfig.SetSeed("Archipelago");

    randomizer::seedgen::settings::Settings settings;

    auto defaultSettings = randomizer::seedgen::settings::GetAllSettingsInfo();

    // add default settings to config first
    for (const auto& [name, info] : *defaultSettings) {
        if (info->GetType() == randomizer::seedgen::settings::Type::STANDARD)
            settings.InsertSetting(name, randomizer::seedgen::settings::Setting(info.get(), info->GetDefaultOption()));
    }

    // update settings using ap config
    for (const auto& apSettingEntry : apConfigYaml["Twilight Princess"]) {
        auto apSettingName = apSettingEntry.first.as<std::string>();

        // ignore AP-only settings
        if (apSettingName == "progression_balancing" ||
            apSettingName == "accessibility" ||
            apSettingName == "local_items" ||
            apSettingName == "non_local_items" ||
            apSettingName == "start_inventory" ||
            apSettingName == "start_hints" ||
            apSettingName == "start_location_hints" ||
            apSettingName == "exclude_locations" ||
            apSettingName == "priority_locations" ||
            apSettingName == "start_inventory_from_pool")
            continue;

        const auto& settingConvert = GetAPSettingNameConvert(apSettingName);

        if (!settingConvert.apName.empty()) {
            bool apSettingValue = apSettingEntry.second.as<bool>();

            if (settingConvert.invert)
                apSettingValue = !apSettingValue;

            auto& setting = settings.GetMap().at(settingConvert.dusklightName);

            if (apSettingValue) {
                setting.SetCurrentOption("On");
            }else {
                setting.SetCurrentOption("Off");
            }

            continue;
        }
        // remaining settings will have string values

        auto apSettingValue = apSettingEntry.second.as<std::string>();

        // TODO: the rest of the translations

        if (apSettingName == "castle_requirements") {

        }

    }

    outConfig.GetSettingsList().push_back(settings);

    return true;
}

int ArchipelagoContext::GetItemAtLocation(const std::string& locName) {
    if (!instance().m_locationItemInfo.contains(locName)) {
        DuskLog.warn("No item found for ({}).", locName);
        return 0;
    }
    return instance().m_locationItemInfo[locName].itemId;
}
} // dusk::archi