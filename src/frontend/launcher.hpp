#pragma once
#include "frontend/profile.hpp"
#include "frontend/world_store.hpp"
#include <array>
#include "platform/log.hpp"
#include "rom/rom.hpp"
#include "frontend/cartridge.hpp"
#include <Windows.h>
#include <d2d1.h>
#include <dwrite_3.h>
#include <wrl/client.h>
#include <future>
#include <map>
namespace fr {
class Launcher {
public:
    Launcher(HINSTANCE instance, std::filesystem::path dataRoot, bool smoke, std::filesystem::path initialRom = {});
    ~Launcher();
    int run(int showCommand);
private:
    enum class Page { Welcome, Profile, Menu, Settings, NewWorld, HostWorld, JoinWorld, EditName, Updates, UpdateRom };
    struct Button { HWND window{}; D2D1_RECT_F rect{}; std::wstring label; bool primary = false; };
    struct Validation { std::filesystem::path path; RomReport report; std::string error; };
    HINSTANCE instance_;
    HWND window_{}, nameEdit_{};
    HFONT font_{};
    HICON icon_{};
    Image logo_,art_;
    std::future<Image> artPending_;
    bool artBusy_=false,dragging_=false;
    std::atomic_bool artCancelled_{false};
    float yaw_=-.40f,pitch_=-.18f,dragX_=0,dragY_=0;
    HBRUSH editBrush_{};
    std::filesystem::path dataRoot_, initialRom_;
    Log log_;
    Page page_ = Page::Welcome;
    std::optional<Profile> profile_;
    std::optional<Validation> validated_;
    std::future<Validation> pending_;
    bool busy_ = false, smoke_ = false, fullscreen_ = false, overlay_ = false;
    std::string expectedHash_;
    std::wstring status_ = L"Select your ROM to get started.";
    std::map<int,Button> buttons_;
    WINDOWPLACEMENT placement_{sizeof(WINDOWPLACEMENT)};
    float scale_ = 1, offsetX_ = 0, offsetY_ = 0;
    Microsoft::WRL::ComPtr<ID2D1Factory> factory_;
    Microsoft::WRL::ComPtr<IDWriteFactory> textFactory_;
    Microsoft::WRL::ComPtr<IDWriteFontCollection1> fontCollection_;
    std::vector<std::filesystem::path> privateFonts_;
    Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> target_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush_;
    static LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT handle(UINT message, WPARAM wParam, LPARAM lParam);
    void initialize();
    void layout();
    void paint();
    HRESULT drawScene(ID2D1RenderTarget* surface);
    void paintToDc(HDC dc);
    void saveCapture();
    ID2D1RenderTarget* surface_ = nullptr;
    void button(int id, std::wstring label, D2D1_RECT_F rect, bool primary = false, bool enabled = true);
    void drawButton(const DRAWITEMSTRUCT& item);
    void command(int id);
    uint32_t accent() const;
    uint32_t pressedAccent() const;
    void selectRom();
    void selectArtwork();
    void bitmap(const Image& image,D2D1_RECT_F rect,bool nearest=true);
    void playGame(int mode=0);
    std::vector<WorldInfo> worlds_;size_t selectedWorld_=0,worldPage_=0;
    std::array<HWND,4> worldEdits_{};uint8_t worldRewards_=3;
    void refreshWorlds();void layoutWorlds();void paintWorlds();bool worldCommand(int id);
    void worldEdit(unsigned index,const std::wstring& value,D2D1_RECT_F rect,bool number=false);
    std::wstring worldEditText(unsigned index);
    HANDLE gameProcess_ = nullptr;
    int gameMode_=0;
    bool gameFailed_=false;
    HANDLE installerMutex_ = nullptr;
    HANDLE updateProcess_ = nullptr;
    std::filesystem::path updateWork_;
    std::wstring updateStatus_=L"Check for a new release when you are ready.",latestVersion_;
    bool updateAvailable_=false;
    void startUpdate(const std::wstring& mode);void launchUpdateWorker(const std::wstring& mode);void pollUpdate();
    void validateAsync(const std::filesystem::path& path, std::string expected = {});
    void pollValidation();
    void fullscreen();
    void error(const std::string& message);
    void panel(D2D1_RECT_F rect, uint32_t color, float radius = 12, uint32_t border = 0);
    void text(std::wstring_view value, D2D1_RECT_F rect, float size, uint32_t color, bool bold = false);
};
}
