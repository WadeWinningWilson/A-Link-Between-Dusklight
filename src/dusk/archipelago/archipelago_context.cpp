#include <dusk/archipelago/archipelago_context.hpp>

#include <thread>

#include "Archipelago.h"
#include "d/d_item.h"
#include "dusk/config.hpp"
#include "dusk/logging.h"
#include "dusk/ui/ui.hpp"

namespace dusk::archi
{

static constexpr int ARCHI_ITEM_OFFSET = 2320000;

ArchipelagoContext& instance() {
    static ArchipelagoContext instance;
    return instance;
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

void ArchipelagoContext::itemRecvImpl(int id) {
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

    AP_Init(GetServerIp().c_str(), "Twilight Princess", GetSlotName().c_str(), GetPassword().c_str());

    AP_NetworkVersion ver{0, 6,7};
    AP_SetClientVersion(&ver);

    AP_SetItemClearCallback([]() {
        DuskLog.info("Item Clear Callback Called!");
    });

    AP_SetItemRecvCallback([](int id, bool notify) {
        DuskLog.info("Item Receive Callback Called! Item: {} Notify: {}", id, notify);
        HandleItemReceived(id, notify);
    });

    AP_SetLocationCheckedCallback([](int loc) {
        DuskLog.info("Location Checked Callback Called! Location: {}", loc);
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
    std::this_thread::sleep_for(std::chrono::seconds(1));

    DuskLog.info("AP Thread started.");

    while (IsConnected()) {
        if (AP_IsMessagePending())
            ParseMessageData();

        instance().m_queueMutex.lock();
        if (randomizer_IsActive() && !instance().m_inactiveItemsQueue.empty()) {
            for (auto item : instance().m_inactiveItemsQueue) {
                instance().itemRecvImpl(item);
            }

            instance().m_inactiveItemsQueue.clear();
        }
        instance().m_queueMutex.unlock();
    }

    DuskLog.info("AP Thread ended.");
}

void ArchipelagoContext::HandleItemReceived(int id, bool notify) {
    // TODO: instead of skipping inventory fill, we should clear the inventory when the clear item callback is called.
    if (!notify) {
        DuskLog.info("Skipping Item.");
        return;
    }

    if (!randomizer_IsActive()) {
        DuskLog.info("Randomizer not active, adding item to queue.");

        instance().m_queueMutex.lock();
        instance().m_inactiveItemsQueue.push_back(id - ARCHI_ITEM_OFFSET);
        instance().m_queueMutex.unlock();
        return;
    }

    instance().itemRecvImpl(id - ARCHI_ITEM_OFFSET);
}
} // dusk::archi