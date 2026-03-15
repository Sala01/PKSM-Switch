#include "utils/Lp2pManager.hpp"

#include <arpa/inet.h>
#include <stdio.h>
#include <string>
#include <errno.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "utils/Logger.hpp"

namespace pksm
{
    namespace utils
    {

namespace
{

bool g_initialized = false;
bool g_server_running = false;

int g_sockfd = -1;
uint16_t g_port = 0;

Lp2pIpConfig g_ip_config{};
Lp2pGroupInfo g_group_info{};

sockaddr_in g_broadcast_addr{};

std::string ToHex(Result rc)
{
    char buf[11] = {0};
    snprintf(buf, sizeof(buf), "0x%08X", static_cast<u32>(rc));
    return std::string(buf);
}

}

Result Lp2pManager::Initialize(Lp2pServiceType serviceType)
{
    if (g_initialized) {
        return 0;
    }

    Result rc = lp2pInitialize(serviceType);
    if (R_FAILED(rc)) {
        LOG_ERROR("lp2pInitialize failed: " + ToHex(rc));
        return rc;
    }

    g_initialized = true;
    return 0;
}

void Lp2pManager::Finalize()
{
    StopServer();

    if (!g_initialized) {
        return;
    }

    lp2pExit();
    g_initialized = false;
}

bool Lp2pManager::IsInitialized()
{
    return g_initialized;
}

bool Lp2pManager::IsServerRunning()
{
    return g_server_running;
}

int Lp2pManager::GetSocketFd()
{
    return g_sockfd;
}

uint16_t Lp2pManager::GetPort()
{
    return g_port;
}

uint32_t Lp2pManager::GetBroadcastAddr()
{
    return ntohl(g_broadcast_addr.sin_addr.s_addr);
}

const Lp2pIpConfig &Lp2pManager::GetIpConfig()
{
    return g_ip_config;
}

const Lp2pGroupInfo &Lp2pManager::GetGroupInfo()
{
    return g_group_info;
}

Result Lp2pManager::EnsureSockets()
{
    const auto alreadyInit = R_VALUE(MAKERESULT(Module_Libnx, LibnxError_AlreadyInitialized));

    Result rc = socketInitializeDefault();
    if (R_FAILED(rc) && R_VALUE(rc) != alreadyInit) {
        LOG_ERROR("socketInitializeDefault failed: " + ToHex(rc));
        return rc;
    }

    return 0;
}

void Lp2pManager::GenerateRandomHex(char *out, size_t size)
{
    if (out == nullptr || size == 0) {
        return;
    }

    if ((size & 1) != 0) {
        size--;
    }

    for (size_t i = 0; i < size; i += 2) {
        u8 tmp = 0;
        randomGet(&tmp, sizeof(tmp));
        snprintf(&out[i], 3, "%02X", tmp);
    }
}

Result Lp2pManager::CreateGroup(const char *ssid, const char *passphrase)
{
    Lp2pGroupInfo in_group_info{};
    lp2pCreateGroupInfo(&in_group_info);

    s8 flags = 0;
    lp2pGroupInfoSetFlags(&in_group_info, &flags, 1);

    lp2pGroupInfoSetServiceName(&in_group_info, ssid);
    lp2pGroupInfoSetPassphrase(&in_group_info, passphrase);

    Result rc = lp2pCreateGroup(&in_group_info);
    if (R_FAILED(rc)) {
        return rc;
    }

    g_group_info = {};
    rc = lp2pGetGroupInfo(&g_group_info);
    if (R_FAILED(rc)) {
        lp2pDestroyGroup();
        return rc;
    }

    g_ip_config = {};
    rc = lp2pGetIpConfig(&g_ip_config);
    if (R_FAILED(rc)) {
        lp2pDestroyGroup();
        return rc;
    }

    return 0;
}

void Lp2pManager::CloseSocket()
{
    if (g_sockfd >= 0) {
        ::close(g_sockfd);
        g_sockfd = -1;
    }
}

Result Lp2pManager::SetupBroadcastSocket(uint16_t port)
{
    CloseSocket();

    g_port = port;

    g_sockfd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (g_sockfd < 0) {
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    }

    int optval = 1;
    if (setsockopt(g_sockfd, SOL_SOCKET, SO_BROADCAST, &optval, sizeof(optval)) != 0) {
        CloseSocket();
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    }

    if (setsockopt(g_sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) != 0) {
        CloseSocket();
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    }

    sockaddr_in bind_addr{};
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    bind_addr.sin_port = htons(port);

    if (bind(g_sockfd, reinterpret_cast<sockaddr *>(&bind_addr), sizeof(bind_addr)) != 0) {
        CloseSocket();
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    }

    memset(&g_broadcast_addr, 0, sizeof(g_broadcast_addr));
    g_broadcast_addr.sin_family = AF_INET;
    g_broadcast_addr.sin_port = htons(port);

    const u32 ip = ntohl(reinterpret_cast<const sockaddr_in *>(g_ip_config.ip_addr)->sin_addr.s_addr);
    const u32 mask = ntohl(reinterpret_cast<const sockaddr_in *>(g_ip_config.subnet_mask)->sin_addr.s_addr);
    const u32 broadcast = htonl(ip | ~mask);

    g_broadcast_addr.sin_addr.s_addr = broadcast;

    return 0;
}

Result Lp2pManager::StartServer(NetworkInfo &out, uint16_t port)
{
    if (!g_initialized) {
        Result rc = Initialize(Lp2pServiceType_App);
        if (R_FAILED(rc)) {
            return rc;
        }
    }

    Result rc = EnsureSockets();
    if (R_FAILED(rc)) {
        return rc;
    }

    StopServer();

    char ssid[0x20] = {0};
    strncpy(ssid, "pksm-", sizeof(ssid) - 1);

    char passphrase[0x40] = {0};

    GenerateRandomHex(&ssid[strlen(ssid)], 8);
    GenerateRandomHex(passphrase, 16);

    rc = CreateGroup(ssid, passphrase);
    if (R_FAILED(rc)) {
        return rc;
    }

    rc = SetupBroadcastSocket(port);
    if (R_FAILED(rc)) {
        u32 tmp32 = 0;
        lp2pLeave(&tmp32);
        lp2pDestroyGroup();
        return rc;
    }

    out.ssid = g_group_info.service_name;
    out.passphrase = passphrase;
    out.port = port;

    g_server_running = true;
    return 0;
}

void Lp2pManager::StopServer()
{
    if (!g_server_running) {
        return;
    }

    CloseSocket();
    g_port = 0;

    u32 tmp32 = 0;
    lp2pLeave(&tmp32);
    lp2pDestroyGroup();

    g_ip_config = {};
    g_group_info = {};
    g_server_running = false;
}

Result Lp2pManager::SendBroadcast(const void *data, size_t size)
{
    if (!g_server_running || g_sockfd < 0 || data == nullptr || size == 0) {
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    }

    const ssize_t ret = sendto(
        g_sockfd,
        data,
        size,
        0,
        reinterpret_cast<sockaddr *>(&g_broadcast_addr),
        sizeof(g_broadcast_addr)
    );

    if (ret < 0) {
        const int err = errno;
        LOG_ERROR("lp2p sendto failed: " + std::to_string(err));
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    }

    return 0;
}

    }
    
} // namespace pksm