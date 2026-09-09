#include "rom/rom.hpp"
#include "runtime/probe.hpp"
#include <Windows.h>
#include <fcntl.h>
#include <io.h>
#include <array>
#include <iostream>
#include <stdexcept>
int wmain(int argc, wchar_t** argv) {
    try {
        if (argc == 2 && std::wstring_view(argv[1]) == L"--help") {
            std::cout << "FireRed Recomp 0.1.0\n"
                "  firered_tool --validate <local.gba>\n"
                "  firered_tool --probe <local.gba>\n"
                "  firered_tool --validate-stdin   (bounded binary ROM stream)\n"
                "  firered_tool --probe-stdin\n"
                "Exit codes: 0 validated, 2 invalid ROM, 3 I/O or usage error,\n"
                "4 probe stopped (expected: runtime is incomplete).\n";
            return 0;
        }
        if (argc < 2) throw std::runtime_error("Use --help for usage.");
        const std::wstring_view mode(argv[1]);
        const bool stdinMode = mode == L"--validate-stdin" || mode == L"--probe-stdin";
        const bool probeMode = mode == L"--probe" || mode == L"--probe-stdin";
        if ((!stdinMode && (argc != 3 || (mode != L"--validate" && mode != L"--probe"))) || (stdinMode && argc != 2))
            throw std::runtime_error("Invalid arguments. Use --help.");
        std::vector<uint8_t> bytes;
        if (stdinMode) {
            if (_setmode(_fileno(stdin), _O_BINARY) == -1) throw std::runtime_error("Cannot switch stdin to binary mode.");
            std::array<char, 65536> chunk{};
            while (std::cin) {
                std::cin.read(chunk.data(), chunk.size());
                const auto count = static_cast<size_t>(std::cin.gcount());
                if (bytes.size() + count > 32 * 1024 * 1024) throw std::runtime_error("ROM stream exceeds 32 MiB.");
                bytes.insert(bytes.end(), chunk.data(), chunk.data() + count);
            }
            if (!std::cin.eof()) throw std::runtime_error("ROM stream read failed.");
        } else bytes = fr::readRom(argv[2]);
        const auto report = fr::inspectRom(bytes);
        std::cout << "[ROM] " << fr::describe(report) << '\n';
        if (!report.supported()) return 2;
        if (probeMode) {
            const auto result = fr::runtime::probe(bytes);
            for (const auto& line : result.trace) std::cout << "[RECOMP] " << line << '\n';
            std::cout << "[RUNTIME] Executed " << result.executed << " instructions. " << result.stopReason << '\n';
            return 4;
        }
        return 0;
    } catch (const std::exception& e) { std::cerr << "[ERROR] " << e.what() << '\n'; return 3; }
}
