#include "frontend/launcher.hpp"
#include "platform/text.hpp"
#include <shellapi.h>
#include <shlobj.h>
#include <stdexcept>
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    const auto com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(com)) { MessageBoxW(nullptr, L"Cannot initialize Windows COM.", L"Pok\u00e9Multi", MB_ICONERROR); return 1; }
    int result = 1;
    try {
        int argc{};
        auto args = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (!args) throw std::runtime_error("Cannot read startup arguments.");
        std::filesystem::path dataRoot, initialRom;
        bool smoke = false;
        try {
            for (int i = 1; i < argc; ++i) {
                const std::wstring_view arg(args[i]);
                if (arg == L"--data-dir" && i + 1 < argc) dataRoot = std::filesystem::absolute(args[++i]);
                else if (arg == L"--rom" && i + 1 < argc) initialRom = std::filesystem::absolute(args[++i]);
                else if (arg == L"--smoke-test") smoke = true;
                else throw std::runtime_error("Usage: pokemulti [--data-dir <folder>] [--rom <file.gba>] [--smoke-test]");
            }
        } catch (...) { LocalFree(args); throw; }
        LocalFree(args);
        if (dataRoot.empty()) {
            PWSTR localAppData{};
            if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &localAppData)))
                throw std::runtime_error("Cannot locate your local application data folder.");
            dataRoot = std::filesystem::path(localAppData) / L"FireRedRecomp";
            CoTaskMemFree(localAppData);
        }
        fr::Launcher app(instance, dataRoot, smoke, initialRom);
        result = app.run(showCommand);
    } catch (const std::exception& e) {
        MessageBoxW(nullptr, fr::widen(e.what()).c_str(), L"Pok\u00e9Multi", MB_OK | MB_ICONERROR);
    }
    CoUninitialize();
    return result;
}
