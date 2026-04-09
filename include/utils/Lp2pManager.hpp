#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <switch.h>

namespace pksm
{
    namespace utils
    {

        class Lp2pManager
        {
        public:
            struct NetworkInfo
            {
                std::string ssid;
                std::string passphrase;
                uint16_t port = 0;
            };

            static Result Initialize(Lp2pServiceType serviceType = Lp2pServiceType_App);
            static void Finalize();

            static Result StartServer(NetworkInfo &out, uint16_t port = 7777);
            static void StopServer();

            static bool IsInitialized();
            static bool IsServerRunning();

            static int GetSocketFd();
            static uint16_t GetPort();
            static uint32_t GetBroadcastAddr();
            static const Lp2pIpConfig &GetIpConfig();
            static const Lp2pGroupInfo &GetGroupInfo();

            static Result SendBroadcast(const void *data, size_t size);

        private:
            static Result EnsureSockets();
            static void GenerateRandomHex(char *out, size_t size);
            static void CloseSocket();
            static Result CreateGroup(const char *ssid, const char *passphrase);
            static Result SetupBroadcastSocket(uint16_t port);
        };

    }

} // namespace pksm