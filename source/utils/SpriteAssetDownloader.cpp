#include "utils/SpriteAssetDownloader.hpp"

#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <optional>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <switch.h>

#include "utils/Logger.hpp"

namespace pksm::utils {

namespace {

constexpr const char* SD_DATA_JSON = "sdmc:/switch/PKSM/assets/data.json";
constexpr const char* SD_SPRITES_DIR = "sdmc:/switch/PKSM/assets/sprites";
constexpr const char* SD_KNOWN_MISSING_SPRITES = "sdmc:/switch/PKSM/assets/known_missing_sprites.txt";
constexpr const char* ROMFS_DATA_JSON = "romfs:/gfx/data/data.json";
constexpr const char* POKESPRITE_BASE = "https://raw.githubusercontent.com/msikma/pokesprite/master";

struct UrlParts {
    bool https = true;
    std::string host;
    std::string path;
    std::string port;
};

struct HttpResponse {
    int statusCode = 0;
    std::unordered_map<std::string, std::string> headers;
    std::vector<u8> body;
};

std::string ToLower(std::string v) {
    std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return v;
}

std::string Trim(const std::string& v) {
    const auto first = v.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    const auto last = v.find_last_not_of(" \t\r\n");
    return v.substr(first, (last - first) + 1);
}

bool StartsWith(const std::string& value, const std::string& prefix) {
    return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
}

bool EndsWith(const std::string& value, const std::string& suffix) {
    return value.size() >= suffix.size() && value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool ParseUrl(const std::string& url, UrlParts& out, std::string& err) {
    std::string working = url;
    if (StartsWith(working, "https://")) {
        out.https = true;
        working = working.substr(8);
    } else if (StartsWith(working, "http://")) {
        out.https = false;
        working = working.substr(7);
    } else {
        err = "Unsupported URL scheme";
        return false;
    }

    const auto slashPos = working.find('/');
    std::string hostPort = (slashPos == std::string::npos) ? working : working.substr(0, slashPos);
    out.path = (slashPos == std::string::npos) ? "/" : working.substr(slashPos);
    if (out.path.empty()) {
        out.path = "/";
    }

    const auto colonPos = hostPort.rfind(':');
    if (colonPos != std::string::npos && colonPos != 0 && colonPos + 1 < hostPort.size()) {
        out.host = hostPort.substr(0, colonPos);
        out.port = hostPort.substr(colonPos + 1);
    } else {
        out.host = hostPort;
        out.port = out.https ? "443" : "80";
    }

    if (out.host.empty()) {
        err = "URL host is empty";
        return false;
    }
    return true;
}

bool EnsureParentDir(const std::string& path) {
    try {
        std::filesystem::path p(path);
        const auto parent = p.parent_path();
        if (!parent.empty()) {
            std::filesystem::create_directories(parent);
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool WriteBinaryFile(const std::string& path, const std::vector<u8>& bytes, std::string& err) {
    if (!EnsureParentDir(path)) {
        err = "Failed to create output directory: " + path;
        return false;
    }

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out.good()) {
        err = "Failed to open output file: " + path;
        return false;
    }

    if (!bytes.empty()) {
        out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
    if (!out.good()) {
        err = "Failed writing output file: " + path;
        return false;
    }
    return true;
}

bool ReadBinaryFile(const std::string& path, std::vector<u8>& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in.good()) {
        return false;
    }
    in.seekg(0, std::ios::end);
    const auto size = static_cast<std::size_t>(in.tellg());
    in.seekg(0, std::ios::beg);
    out.resize(size);
    if (size != 0) {
        in.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(size));
    }
    return in.good();
}

bool DecodeChunkedBody(const std::vector<u8>& in, std::vector<u8>& out) {
    std::size_t pos = 0;
    while (pos < in.size()) {
        std::size_t lineEnd = std::string::npos;
        for (std::size_t i = pos; i + 1 < in.size(); ++i) {
            if (in[i] == '\r' && in[i + 1] == '\n') {
                lineEnd = i;
                break;
            }
        }
        if (lineEnd == std::string::npos) {
            return false;
        }

        std::string sizeLine(reinterpret_cast<const char*>(in.data() + pos), lineEnd - pos);
        const auto semi = sizeLine.find(';');
        if (semi != std::string::npos) {
            sizeLine = sizeLine.substr(0, semi);
        }
        sizeLine = Trim(sizeLine);
        if (sizeLine.empty()) {
            return false;
        }

        std::size_t chunkSize = 0;
        try {
            chunkSize = static_cast<std::size_t>(std::stoull(sizeLine, nullptr, 16));
        } catch (...) {
            return false;
        }

        pos = lineEnd + 2;
        if (chunkSize == 0) {
            return true;
        }

        if (pos + chunkSize + 2 > in.size()) {
            return false;
        }

        out.insert(out.end(), in.begin() + static_cast<std::ptrdiff_t>(pos), in.begin() + static_cast<std::ptrdiff_t>(pos + chunkSize));
        pos += chunkSize;

        if (in[pos] != '\r' || in[pos + 1] != '\n') {
            return false;
        }
        pos += 2;
    }
    return false;
}

bool ParseHttpResponse(const std::vector<u8>& raw, HttpResponse& out, std::string& err) {
    const std::string marker = "\r\n\r\n";
    const auto it = std::search(raw.begin(), raw.end(), marker.begin(), marker.end());
    if (it == raw.end()) {
        err = "HTTP response header not found";
        return false;
    }

    const auto headerLen = static_cast<std::size_t>(std::distance(raw.begin(), it));
    const auto bodyOffset = headerLen + marker.size();

    std::string headerStr(reinterpret_cast<const char*>(raw.data()), headerLen);
    std::istringstream stream(headerStr);

    std::string statusLine;
    if (!std::getline(stream, statusLine)) {
        err = "HTTP status line missing";
        return false;
    }
    statusLine = Trim(statusLine);

    int statusCode = 0;
    {
        std::istringstream statusStream(statusLine);
        std::string version;
        statusStream >> version >> statusCode;
        if (statusCode <= 0) {
            err = "Invalid HTTP status line: " + statusLine;
            return false;
        }
    }
    out.statusCode = statusCode;

    std::string line;
    while (std::getline(stream, line)) {
        line = Trim(line);
        if (line.empty()) {
            continue;
        }
        const auto colon = line.find(':');
        if (colon == std::string::npos) {
            continue;
        }
        const auto key = ToLower(Trim(line.substr(0, colon)));
        const auto value = Trim(line.substr(colon + 1));
        out.headers[key] = value;
    }

    std::vector<u8> body;
    body.insert(body.end(), raw.begin() + static_cast<std::ptrdiff_t>(bodyOffset), raw.end());

    const auto teIt = out.headers.find("transfer-encoding");
    if (teIt != out.headers.end() && ToLower(teIt->second).find("chunked") != std::string::npos) {
        std::vector<u8> decoded;
        if (!DecodeChunkedBody(body, decoded)) {
            err = "Failed to decode chunked HTTP body";
            return false;
        }
        out.body = std::move(decoded);
    } else {
        out.body = std::move(body);
    }

    return true;
}

class NetworkSession {
public:
    bool Start(std::string& err) {
        const auto alreadyInit = R_VALUE(MAKERESULT(Module_Libnx, LibnxError_AlreadyInitialized));

        Result rc = socketInitializeDefault();
        if (R_FAILED(rc) && R_VALUE(rc) != alreadyInit) {
            err = "socketInitializeDefault failed: 0x" + ToHex(rc);
            return false;
        }

        rc = sslInitialize(3);
        if (R_FAILED(rc) && R_VALUE(rc) != alreadyInit) {
            err = "sslInitialize failed: 0x" + ToHex(rc);
            return false;
        }
        return true;
    }

    ~NetworkSession() {
        // Keep SSL and BSD sockets alive for app lifetime to avoid dropping nxlink stdio.
    }

private:
    static std::string ToHex(Result rc) {
        std::ostringstream ss;
        ss << std::hex << static_cast<u32>(rc);
        return ss.str();
    }

};

int ConnectTcpSocket(const UrlParts& url, std::string& err) {
    auto configureTimeouts = [](int sockfd) {
        timeval tv{};
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) != 0) {
            return false;
        }
        if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) != 0) {
            return false;
        }
        return true;
    };

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;

    addrinfo* result = nullptr;
    const int gai = getaddrinfo(url.host.c_str(), url.port.c_str(), &hints, &result);
    if (gai != 0 || result == nullptr) {
        err = "getaddrinfo failed for " + url.host;
        return -1;
    }

    int sockfd = -1;
    for (addrinfo* rp = result; rp != nullptr; rp = rp->ai_next) {
        sockfd = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sockfd < 0) {
            continue;
        }

        if (::connect(sockfd, rp->ai_addr, static_cast<socklen_t>(rp->ai_addrlen)) == 0) {
            if (!configureTimeouts(sockfd)) {
                ::close(sockfd);
                sockfd = -1;
                continue;
            }
            break;
        }

        ::close(sockfd);
        sockfd = -1;
    }

    freeaddrinfo(result);

    if (sockfd < 0) {
        err = "connect failed for " + url.host;
    }
    return sockfd;
}

bool SendAllPlain(int sockfd, const std::string& req, std::string& err) {
    std::size_t sent = 0;
    while (sent < req.size()) {
        const ssize_t n = ::send(sockfd, req.data() + static_cast<std::ptrdiff_t>(sent), req.size() - sent, 0);
        if (n <= 0) {
            err = "send failed";
            return false;
        }
        sent += static_cast<std::size_t>(n);
    }
    return true;
}

bool RecvAllPlain(int sockfd, std::vector<u8>& out, std::string& err) {
    std::array<u8, 0x4000> buf{};
    while (true) {
        const ssize_t n = ::recv(sockfd, buf.data(), buf.size(), 0);
        if (n == 0) {
            break;
        }
        if (n < 0) {
            err = "recv failed";
            return false;
        }
        out.insert(out.end(), buf.begin(), buf.begin() + n);
    }
    return true;
}

bool SendAllTls(SslConnection& conn, const std::string& req, std::string& err) {
    std::size_t sent = 0;
    while (sent < req.size()) {
        u32 chunk = 0;
        const auto remaining = req.size() - sent;
        const u32 writeLen = static_cast<u32>(std::min<std::size_t>(remaining, 0x4000));
        const Result rc = sslConnectionWrite(&conn, req.data() + sent, writeLen, &chunk);
        if (R_FAILED(rc) || chunk == 0) {
            std::ostringstream ss;
            ss << "ssl write failed: 0x" << std::hex << static_cast<u32>(rc);
            err = ss.str();
            return false;
        }
        sent += chunk;
    }
    return true;
}

bool RecvAllTls(SslConnection& conn, std::vector<u8>& out, std::string& err) {
    std::array<u8, 0x4000> buf{};
    while (true) {
        u32 read = 0;
        const Result rc = sslConnectionRead(&conn, buf.data(), static_cast<u32>(buf.size()), &read);
        if (R_FAILED(rc)) {
            std::ostringstream ss;
            ss << "ssl read failed: 0x" << std::hex << static_cast<u32>(rc);
            err = ss.str();
            return false;
        }
        if (read == 0) {
            break;
        }
        out.insert(out.end(), buf.begin(), buf.begin() + read);
    }
    return true;
}

bool HttpGet(const std::string& url, HttpResponse& out, std::string& err, int redirectDepth = 0) {
    if (redirectDepth > 5) {
        err = "Too many HTTP redirects";
        return false;
    }

    UrlParts parts;
    if (!ParseUrl(url, parts, err)) {
        return false;
    }

    const int sockfd = ConnectTcpSocket(parts, err);
    if (sockfd < 0) {
        return false;
    }

    SslContext sslContext{};
    SslConnection sslConn{};
    bool tlsReady = false;

    auto closeSocket = [&]() {
        if (sockfd >= 0) {
            ::close(sockfd);
        }
    };

    auto closeTls = [&]() {
        if (tlsReady) {
            sslConnectionClose(&sslConn);
            sslContextClose(&sslContext);
            tlsReady = false;
        }
    };

    if (parts.https) {
        Result rc = sslCreateContext(&sslContext, SslVersion_Auto);
        if (R_FAILED(rc)) {
            std::ostringstream ss;
            ss << "sslCreateContext failed: 0x" << std::hex << static_cast<u32>(rc);
            err = ss.str();
            closeSocket();
            return false;
        }

        rc = sslContextCreateConnection(&sslContext, &sslConn);
        if (R_FAILED(rc)) {
            std::ostringstream ss;
            ss << "sslContextCreateConnection failed: 0x" << std::hex << static_cast<u32>(rc);
            err = ss.str();
            sslContextClose(&sslContext);
            closeSocket();
            return false;
        }
        tlsReady = true;

        rc = sslConnectionSetHostName(&sslConn, parts.host.c_str(), static_cast<u32>(parts.host.size() + 1));
        if (R_FAILED(rc)) {
            std::ostringstream ss;
            ss << "sslConnectionSetHostName failed: 0x" << std::hex << static_cast<u32>(rc);
            err = ss.str();
            closeTls();
            closeSocket();
            return false;
        }

        rc = sslConnectionSetOption(&sslConn, SslOptionType_DoNotCloseSocket, true);
        if (R_FAILED(rc)) {
            std::ostringstream ss;
            ss << "sslConnectionSetOption(DoNotCloseSocket) failed: 0x" << std::hex << static_cast<u32>(rc);
            err = ss.str();
            closeTls();
            closeSocket();
            return false;
        }

        const int outSock = socketSslConnectionSetSocketDescriptor(&sslConn, sockfd);
        if (outSock < 0 && errno != ENOENT) {
            err = "socketSslConnectionSetSocketDescriptor failed";
            closeTls();
            closeSocket();
            return false;
        }

        rc = sslConnectionDoHandshake(&sslConn, nullptr, nullptr, nullptr, 0);
        if (R_FAILED(rc)) {
            std::ostringstream ss;
            ss << "TLS handshake failed: 0x" << std::hex << static_cast<u32>(rc);
            err = ss.str();
            closeTls();
            closeSocket();
            return false;
        }
    }

    const std::string request =
        "GET " + parts.path + " HTTP/1.1\r\n"
        "Host: " + parts.host + "\r\n"
        "User-Agent: PKSM-Switch\r\n"
        "Accept: */*\r\n"
        "Connection: close\r\n\r\n";

    std::vector<u8> raw;
    bool ioOk = false;
    if (parts.https) {
        ioOk = SendAllTls(sslConn, request, err) && RecvAllTls(sslConn, raw, err);
    } else {
        ioOk = SendAllPlain(sockfd, request, err) && RecvAllPlain(sockfd, raw, err);
    }

    closeTls();
    closeSocket();

    if (!ioOk) {
        return false;
    }

    HttpResponse response;
    if (!ParseHttpResponse(raw, response, err)) {
        return false;
    }

    if (response.statusCode == 301 || response.statusCode == 302 || response.statusCode == 307 || response.statusCode == 308) {
        const auto it = response.headers.find("location");
        if (it == response.headers.end() || it->second.empty()) {
            err = "HTTP redirect without location";
            return false;
        }

        std::string redirectUrl = it->second;
        if (StartsWith(redirectUrl, "/")) {
            redirectUrl = (parts.https ? "https://" : "http://") + parts.host + redirectUrl;
        }
        return HttpGet(redirectUrl, out, err, redirectDepth + 1);
    }

    out = std::move(response);
    return true;
}

std::string NormalizeCdnBase(const std::string& raw) {
    if (raw.empty()) {
        return "https://cdn.sigkill.tech";
    }
    std::string out = raw;
    while (!out.empty() && out.back() == '/') {
        out.pop_back();
    }
    return out;
}

std::optional<std::string> BuildPokespriteFallbackUrl(const std::string& filename) {
    if (!EndsWith(filename, ".png")) {
        return std::nullopt;
    }

    bool shiny = false;
    std::string baseName = filename;

    if (EndsWith(baseName, "_shiny.png")) {
        shiny = true;
        baseName = baseName.substr(0, baseName.size() - std::strlen("_shiny.png")) + ".png";
    }

    if (baseName.empty()) {
        return std::nullopt;
    }

    const std::string folder = shiny ? "shiny" : "regular";
    return std::string(POKESPRITE_BASE) + "/pokemon-gen8/" + folder + "/" + baseName;
}

bool FileExistsAndNotEmpty(const std::filesystem::path& p) {
    try {
        return std::filesystem::exists(p) && std::filesystem::is_regular_file(p) && std::filesystem::file_size(p) > 0;
    } catch (...) {
        return false;
    }
}

std::unordered_set<std::string> LoadKnownMissingSprites() {
    std::unordered_set<std::string> knownMissing;

    std::ifstream in(SD_KNOWN_MISSING_SPRITES);
    if (!in.good()) {
        return knownMissing;
    }

    std::string line;
    while (std::getline(in, line)) {
        line = Trim(line);
        if (!line.empty()) {
            knownMissing.insert(line);
        }
    }

    return knownMissing;
}

bool SaveKnownMissingSprites(const std::unordered_set<std::string>& knownMissing, std::string& err) {
    if (!EnsureParentDir(SD_KNOWN_MISSING_SPRITES)) {
        err = "Failed to create known-missing cache directory";
        return false;
    }

    std::vector<std::string> ordered(knownMissing.begin(), knownMissing.end());
    std::sort(ordered.begin(), ordered.end());

    std::ofstream out(SD_KNOWN_MISSING_SPRITES, std::ios::trunc);
    if (!out.good()) {
        err = "Failed to open known-missing cache file";
        return false;
    }

    for (const auto& entry : ordered) {
        out << entry << '\n';
    }

    if (!out.good()) {
        err = "Failed writing known-missing cache file";
        return false;
    }

    return true;
}

bool BuildMissingSpriteListFromData(
    const std::vector<u8>& dataJsonBytes,
    std::vector<std::string>& outMissing,
    std::size_t& outPresent,
    std::string& err
) {
    std::string jsonText(reinterpret_cast<const char*>(dataJsonBytes.data()), dataJsonBytes.size());
    nlohmann::json parsed = nlohmann::json::parse(jsonText, nullptr, false);
    if (parsed.is_discarded() || !parsed.is_object() || !parsed.contains("pokemon") || !parsed["pokemon"].is_array()) {
        err = "Invalid pokemon data.json format";
        return false;
    }

    const auto spritesRoot = std::filesystem::path(SD_SPRITES_DIR);
    try {
        std::filesystem::create_directories(spritesRoot);
    } catch (...) {
        err = "Failed to create sprites directory";
        return false;
    }

    outMissing.clear();
    outPresent = 0;

    for (const auto& entry : parsed["pokemon"]) {
        if (!entry.is_object() || !entry.contains("file_path") || !entry["file_path"].is_string()) {
            continue;
        }

        const std::string fullPath = entry["file_path"].get<std::string>();
        const auto slash = fullPath.find_last_of('/');
        const std::string filename = (slash == std::string::npos) ? fullPath : fullPath.substr(slash + 1);
        if (filename.empty()) {
            continue;
        }

        const auto localPath = spritesRoot / filename;
        if (FileExistsAndNotEmpty(localPath)) {
            ++outPresent;
        } else {
            outMissing.push_back(filename);
        }
    }

    return true;
}

}  // namespace

std::size_t SpriteAssetDownloader::CountMissingSprites(std::string* error) {
    std::vector<u8> dataJsonBytes;
    if (!ReadBinaryFile(SD_DATA_JSON, dataJsonBytes) || dataJsonBytes.empty()) {
        if (!ReadBinaryFile(ROMFS_DATA_JSON, dataJsonBytes) || dataJsonBytes.empty()) {
            if (error != nullptr) {
                *error = "No local data.json found on SD or romfs";
            }
            return 0;
        }
    }

    std::vector<std::string> missing;
    std::size_t present = 0;
    std::string parseErr;
    if (!BuildMissingSpriteListFromData(dataJsonBytes, missing, present, parseErr)) {
        if (error != nullptr) {
            *error = parseErr;
        }
        return 0;
    }

    const auto knownMissing = LoadKnownMissingSprites();
    if (knownMissing.empty()) {
        return missing.size();
    }

    std::size_t pending = 0;
    for (const auto& filename : missing) {
        if (!knownMissing.contains(filename)) {
            ++pending;
        }
    }

    return pending;
}

SpriteAssetDownloader::SyncResult SpriteAssetDownloader::SyncFromCdn(
    const std::string& cdnBase,
    std::size_t maxDownloadsPerRun,
    std::uint32_t maxMilliseconds,
    const std::function<void(const ProgressInfo&)>& onProgress
) {
    SyncResult result;
    result.attemptedNetwork = true;

    std::string networkErr;
    NetworkSession session;
    if (!session.Start(networkErr)) {
        result.error = networkErr;
        return result;
    }

    const std::string normalizedBase = NormalizeCdnBase(cdnBase);
    std::vector<u8> dataJsonBytes;

    static std::mutex dataJsonCacheMutex;
    static std::vector<u8> cachedDataJsonBytes;

    {
        std::lock_guard<std::mutex> lock(dataJsonCacheMutex);
        if (!cachedDataJsonBytes.empty()) {
            dataJsonBytes = cachedDataJsonBytes;
        }
    }

    if (dataJsonBytes.empty()) {
        const std::string dataUrl = normalizedBase + "/assets/data.json";
        HttpResponse dataResp;
        std::string dataErr;

        if (HttpGet(dataUrl, dataResp, dataErr) && dataResp.statusCode == 200) {
            dataJsonBytes = std::move(dataResp.body);
            if (dataJsonBytes.empty()) {
                result.error = "Downloaded data.json is empty";
                return result;
            }

            std::string writeErr;
            if (!WriteBinaryFile(SD_DATA_JSON, dataJsonBytes, writeErr)) {
                result.error = writeErr;
                return result;
            }
            result.downloadedDataJson = true;
        } else {
            if (!ReadBinaryFile(SD_DATA_JSON, dataJsonBytes) || dataJsonBytes.empty()) {
                if (!ReadBinaryFile(ROMFS_DATA_JSON, dataJsonBytes) || dataJsonBytes.empty()) {
                    if (dataErr.empty()) {
                        std::ostringstream ss;
                        ss << "Failed to download data.json (status " << dataResp.statusCode << ") and no local fallback";
                        result.error = ss.str();
                    } else {
                        result.error = dataErr;
                    }
                    return result;
                }

                LOG_WARNING("SpriteAssetDownloader: using romfs data.json fallback");
                std::string writeErr;
                if (!WriteBinaryFile(SD_DATA_JSON, dataJsonBytes, writeErr)) {
                    LOG_WARNING("SpriteAssetDownloader: failed to persist romfs data.json to SD: " + writeErr);
                }
            } else {
                LOG_WARNING("SpriteAssetDownloader: using local SD data.json fallback");
            }

            if (dataJsonBytes.empty()) {
                if (dataErr.empty()) {
                    std::ostringstream ss;
                    ss << "Failed to download data.json (status " << dataResp.statusCode << ") and no local fallback";
                    result.error = ss.str();
                } else {
                    result.error = dataErr;
                }
                return result;
            }
        }

        std::lock_guard<std::mutex> lock(dataJsonCacheMutex);
        cachedDataJsonBytes = dataJsonBytes;
    }

    std::vector<std::string> missingSprites;
    std::size_t locallyPresent = 0;
    std::string parseErr;
    if (!BuildMissingSpriteListFromData(dataJsonBytes, missingSprites, locallyPresent, parseErr)) {
        result.error = parseErr;
        return result;
    }

    auto knownMissing = LoadKnownMissingSprites();
    std::vector<std::string> retryableMissingSprites;
    retryableMissingSprites.reserve(missingSprites.size());

    std::size_t knownMissingSkipped = 0;
    for (const auto& filename : missingSprites) {
        if (!knownMissing.empty() && knownMissing.contains(filename)) {
            ++knownMissingSkipped;
            continue;
        }
        retryableMissingSprites.push_back(filename);
    }

    result.skippedSprites = locallyPresent + knownMissingSkipped;

    const auto spritesRoot = std::filesystem::path(SD_SPRITES_DIR);

    result.totalMissingSprites = retryableMissingSprites.size();
    result.remainingSprites = result.totalMissingSprites;

    auto emitProgress = [&](std::size_t processed) {
        if (!onProgress) {
            return;
        }
        ProgressInfo info;
        info.totalMissing = result.totalMissingSprites;
        info.processed = processed;
        info.downloaded = result.downloadedSprites;
        info.failed = result.failedSprites;
        info.remaining = (info.totalMissing > info.processed) ? (info.totalMissing - info.processed) : 0;
        onProgress(info);
    };
    emitProgress(0);

    if (retryableMissingSprites.empty()) {
        LOG_INFO("SpriteAssetDownloader: sync finished (downloaded=0, skipped=" + std::to_string(result.skippedSprites) + ", failed=0, remaining=0)");
        return result;
    }

    const auto syncStart = std::chrono::steady_clock::now();
    std::size_t attemptedDownloads = 0;
    std::size_t processedSprites = 0;
    std::size_t newlyKnownMissing = 0;

    for (const auto& filename : retryableMissingSprites) {
        if (maxDownloadsPerRun > 0 && attemptedDownloads >= maxDownloadsPerRun) {
            result.budgetReached = true;
            break;
        }

        if (maxMilliseconds > 0) {
            const auto elapsedMs = static_cast<std::uint32_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - syncStart).count()
            );
            if (elapsedMs >= maxMilliseconds) {
                result.budgetReached = true;
                break;
            }
        }

        ++attemptedDownloads;

        const auto localPath = spritesRoot / filename;

        const std::string spriteUrl = normalizedBase + "/assets/sprites/" + filename;
        HttpResponse spriteResp;
        std::string spriteErr;
        int primaryStatus = 0;
        int fallbackStatus = 0;
        bool fallbackAttempted = false;

        bool downloaded = false;
        if (HttpGet(spriteUrl, spriteResp, spriteErr) && spriteResp.statusCode == 200 && !spriteResp.body.empty()) {
            downloaded = true;
            primaryStatus = spriteResp.statusCode;
        } else {
            primaryStatus = spriteResp.statusCode;
            const auto fallbackUrl = BuildPokespriteFallbackUrl(filename);
            if (fallbackUrl.has_value()) {
                fallbackAttempted = true;
                HttpResponse fallbackResp;
                std::string fallbackErr;
                const bool fallbackOk = HttpGet(*fallbackUrl, fallbackResp, fallbackErr);
                fallbackStatus = fallbackResp.statusCode;
                if (fallbackOk && fallbackResp.statusCode == 200 && !fallbackResp.body.empty()) {
                    spriteResp = std::move(fallbackResp);
                    downloaded = true;
                } else if (spriteErr.empty()) {
                    if (fallbackErr.empty()) {
                        std::ostringstream ss;
                        ss << "fallback status " << fallbackResp.statusCode;
                        spriteErr = ss.str();
                    } else {
                        spriteErr = fallbackErr;
                    }
                }
            }
        }

        if (!downloaded) {
            const bool primaryClientError = (primaryStatus >= 400) && (primaryStatus < 500);
            const bool permanentlyMissing =
                (primaryStatus == 404 && !fallbackAttempted)
                || (fallbackAttempted && fallbackStatus == 404 && primaryClientError);
            if (permanentlyMissing && knownMissing.insert(filename).second) {
                ++newlyKnownMissing;
            }

            ++result.failedSprites;
            ++processedSprites;
            emitProgress(processedSprites);
            if (result.error.empty()) {
                if (spriteErr.empty()) {
                    std::ostringstream ss;
                    ss << "Failed downloading sprite " << filename << " (status " << spriteResp.statusCode << ")";
                    result.error = ss.str();
                } else {
                    result.error = "Failed downloading sprite " + filename + ": " + spriteErr;
                }
            }
            continue;
        }

        std::string writeErr;
        if (!WriteBinaryFile(localPath.string(), spriteResp.body, writeErr)) {
            ++result.failedSprites;
            ++processedSprites;
            emitProgress(processedSprites);
            if (result.error.empty()) {
                result.error = writeErr;
            }
            continue;
        }

        ++result.downloadedSprites;
        ++processedSprites;
        emitProgress(processedSprites);
    }

    if (newlyKnownMissing > 0) {
        std::string cacheErr;
        if (!SaveKnownMissingSprites(knownMissing, cacheErr)) {
            LOG_WARNING("SpriteAssetDownloader: failed to persist known-missing cache: " + cacheErr);
        } else {
            LOG_INFO("SpriteAssetDownloader: cached " + std::to_string(newlyKnownMissing) + " known-missing sprites (404)");
        }
    }

    result.remainingSprites = (result.totalMissingSprites > result.downloadedSprites) ?
        (result.totalMissingSprites - result.downloadedSprites) :
        0;

    LOG_INFO("SpriteAssetDownloader: sync finished (downloaded=" + std::to_string(result.downloadedSprites) +
             ", skipped=" + std::to_string(result.skippedSprites) +
             ", failed=" + std::to_string(result.failedSprites) +
             ", remaining=" + std::to_string(result.remainingSprites) +
             ", budgetReached=" + std::string(result.budgetReached ? "true" : "false") + ")");

    return result;
}

}  // namespace pksm::utils