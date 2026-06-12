#pragma once

#include <mutex>
#include <string>

#include "Archipelago.h"

namespace dusk::archi
{
    class ArchipelagoContext {
    private:
        struct TEMP_GameItemInfo {
            int itemId;
            randomizer::logic::item::Importance importance;
            std::string itemName;
        };

        struct TEMP_GameLocationInfo {
            int apId;
            std::string locName;
        };

        struct GameLocationInfo {
            int itemId;
            int64_t apLocationId;
            bool collected;
        };

        std::vector<int> m_inactiveItemsQueue;
        std::mutex m_queueMutex;

        std::unordered_map<std::string, GameLocationInfo> m_locationItemInfo;

        // TEMP
        std::map<int, TEMP_GameItemInfo> m_apItemToGameItem;
        std::vector<TEMP_GameLocationInfo> m_apLocToGameLoc;
        std::string m_apConfigPath;

        void LoadTempItemInfo();

        void LoadTempLocationInfo();

        void itemRecvImpl(int id, bool notify);

        int getItemIdFromApId(int apId);

        std::string getLocationNameFromApId(int apId) const;
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

        static void HandleResetInventory();

        static void HandleReceiveLocationScout(const std::vector<AP_NetworkItem>& items);

        static void UpdateCheckedLocations();

        // State Requesters

        static void RequestAllLocationScout(bool isHint = false);

        // AP -> Internal Rando Converters

        static void SetAPConfigYamlPath(const std::string_view& path);

        static bool GenerateConfigFromAP(randomizer::seedgen::config::Config& outConfig);

        static int GetItemAtLocation(const std::string& locName);

    };
} // dusk::archi