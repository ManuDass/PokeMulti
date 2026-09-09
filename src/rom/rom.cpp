#include "rom/rom.hpp"
#include "rom/zip.hpp"
#include <Windows.h>
#include <bcrypt.h>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <stdexcept>
namespace fr {
namespace {
struct Algorithm {
    BCRYPT_ALG_HANDLE handle{};
    ~Algorithm() { if (handle) BCryptCloseAlgorithmProvider(handle, 0); }
};
struct Hash {
    BCRYPT_HASH_HANDLE handle{};
    ~Hash() { if (handle) BCryptDestroyHash(handle); }
};
void check(NTSTATUS status) { if (status < 0) throw std::runtime_error("Windows cryptographic hashing failed."); }
std::string headerText(std::span<const uint8_t> bytes, size_t offset, size_t count) {
    std::string value;
    for (size_t i = offset; i < offset + count && bytes[i]; ++i)
        value += bytes[i] >= 32 && bytes[i] <= 126 ? static_cast<char>(bytes[i]) : '?';
    return value;
}
}
std::string digest(std::span<const uint8_t> bytes, bool sha256) {
    Algorithm algorithm;
    check(BCryptOpenAlgorithmProvider(&algorithm.handle, sha256 ? BCRYPT_SHA256_ALGORITHM : BCRYPT_SHA1_ALGORITHM, nullptr, 0));
    DWORD objectSize{}, resultSize{}, returned{};
    check(BCryptGetProperty(algorithm.handle, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectSize), sizeof(objectSize), &returned, 0));
    check(BCryptGetProperty(algorithm.handle, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&resultSize), sizeof(resultSize), &returned, 0));
    std::vector<uint8_t> object(objectSize), result(resultSize);
    Hash hash;
    check(BCryptCreateHash(algorithm.handle, &hash.handle, object.data(), objectSize, nullptr, 0, 0));
    for (size_t offset = 0; offset < bytes.size();) {
        const auto count = static_cast<ULONG>((std::min)(bytes.size() - offset, size_t{65536}));
        check(BCryptHashData(hash.handle, const_cast<PUCHAR>(bytes.data() + offset), count, 0));
        offset += count;
    }
    check(BCryptFinishHash(hash.handle, result.data(), resultSize, 0));
    std::ostringstream out;
    for (auto byte : result) out << std::hex << std::setfill('0') << std::setw(2) << static_cast<unsigned>(byte);
    return out.str();
}
std::vector<uint8_t> readRom(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) throw std::runtime_error("Cannot open the ROM. Select an accessible, locally stored .gba file.");
    const auto length = file.tellg();
    auto extension=path.extension().wstring();
    std::transform(extension.begin(),extension.end(),extension.begin(),[](wchar_t c){return c>=L'A' && c<=L'Z' ? wchar_t(c-L'A'+L'a') : c;});
    const bool zip=extension==L".zip";
    if (length < 0 || length > (zip ? 64 : 32) * 1024 * 1024) throw std::runtime_error("ROM/ZIP exceeds its input size limit.");
    file.seekg(0);
    std::vector<uint8_t> bytes(static_cast<size_t>(length));
    if (!bytes.empty() && !file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())))
        throw std::runtime_error("Could not read the entire ROM.");
    if (file.peek() != std::char_traits<char>::eof()) throw std::runtime_error("ROM changed during reading. Try again.");
    return zip ? readZipRom(bytes) : bytes;
}
FireRedRevision identifyRevision(std::string_view sha1, uint8_t headerVersion) {
    // Public metadata only: https://github.com/pret/pokefirered (firered*.sha1).
    if (headerVersion == 0 && sha1 == "41cb23d8dccc8ebd7c649cd8fbb58eeace6e2fdc") return FireRedRevision::FireRed_US_10;
    if (headerVersion == 1 && sha1 == "dd5945db9b930750cb39d00c84da8571feebf417") return FireRedRevision::FireRed_US_11;
    return FireRedRevision::Unsupported;
}
RomReport inspectRom(std::span<const uint8_t> bytes) {
    if (bytes.size() > 32 * 1024 * 1024) throw std::runtime_error("ROM exceeds the 32 MiB input limit.");
    RomReport report;
    report.size = bytes.size();
    report.sha256 = digest(bytes);
    report.sha1 = digest(bytes, false);
    if (bytes.size() < 0xC0) { report.error = "This file is too short to contain a GBA ROM header."; return report; }
    report.title = headerText(bytes, 0xA0, 12);
    report.gameCode = headerText(bytes, 0xAC, 4);
    report.headerVersion = bytes[0xBC];
    if (bytes.size() != FireRedRomSize) { report.error = "Supported FireRed ROMs must be exactly 16 MiB. Select an uncompressed .gba file."; return report; }
    if (report.gameCode != "BPRE" || report.title != "POKEMON FIRE" || bytes[0xB2] != 0x96 || bytes[0xB0] != '0' || bytes[0xB1] != '1') {
        report.error = "This is not a supported English FireRed GBA ROM header."; return report;
    }
    uint8_t checksum = 0;
    for (size_t i = 0xA0; i <= 0xBC; ++i) checksum = static_cast<uint8_t>(checksum - bytes[i]);
    checksum = static_cast<uint8_t>(checksum - 0x19);
    if (checksum != bytes[0xBD]) { report.error = "GBA header checksum is invalid. The ROM may be damaged."; return report; }
    report.revision = identifyRevision(report.sha1, report.headerVersion);
    if (!report.supported()) report.error = "This FireRed ROM revision is not currently supported. Modified or damaged ROMs are not accepted.";
    return report;
}
std::string revisionName(FireRedRevision revision) {
    switch (revision) {
    case FireRedRevision::FireRed_US_10: return "FireRed US v1.0";
    case FireRedRevision::FireRed_US_11: return "FireRed US v1.1";
    default: return "Unsupported";
    }
}
std::string describe(const RomReport& report) {
    std::ostringstream out;
    out << (report.supported() ? "ROM identity verified: " + revisionName(report.revision) : report.error)
        << "\nSHA-256: " << report.sha256 << "\nSHA-1: " << report.sha1
        << "\nSize: " << report.size << " bytes\nHeader: " << report.title << " / " << report.gameCode
        << " / revision " << static_cast<unsigned>(report.headerVersion)
        << "\nSupported identities: FireRed US v1.0, FireRed US v1.1."
        << "\nIdentity support does not mean playable runtime support.";
    return out.str();
}
}
