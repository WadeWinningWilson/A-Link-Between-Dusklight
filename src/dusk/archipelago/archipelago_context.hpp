#pragma once

#include <mutex>
#include <string>

namespace dusk::archi
{
    class ArchipelagoContext {
    private:
        struct TEMP_GameItemInfo {
            int itemId;
            randomizer::logic::item::Importance importance;
            std::string itemName;
        };

        std::vector<int> m_inactiveItemsQueue;
        std::mutex m_queueMutex;

        // TEMP
        std::map<int, TEMP_GameItemInfo> m_apItemToGameItem;

        void LoadTempItemInfo();

        void itemRecvImpl(int id);
    public:
        ArchipelagoContext();

        // Config Getters/Setters

        static void SetServerIp(const std::string_view& ip);
        static void SetSlotName(const std::string_view& name);
        static void SetPassword(const std::string_view& pass);

        static const std::string& GetServerIp();
        static const std::string& GetSlotName();
        static const std::string& GetPassword();

        // Connection Handlers

        static void ConnectToServer();

        static void DisconnectFromServer();

        static bool IsConnected();

        // State Handlers

        static void MainThreadFunc();

        static void HandleItemReceived(int id, bool notify);

    };
} // dusk::archi