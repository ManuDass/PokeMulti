#include "frontend/launcher.hpp"
#include <shellapi.h>
#include <windowsx.h>
#include "platform/text.hpp"
#include "platform/version.hpp"
#include "runtime/probe.hpp"
#include <shobjidl.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <fstream>
#include <sstream>
#include "online/connection.hpp"
#include "online/limits.hpp"
#include <stdexcept>
namespace fr {
namespace {
constexpr uint32_t Ink = 0x253445, Red = 0xC74C3B, Cream = 0xFFFDF5, Muted = 0x667382, Line = 0xDFDBCE;
constexpr int Browse = 101, Create = 102, Continue = 103, Host = 104, Join = 105, Settings = 106, Quit = 107, Back = 108, Fullscreen = 109, Probe = 110, Details = 111, Artwork = 112;
constexpr int EditName=114, SaveName=115, Updates=116, CheckUpdate=117, InstallUpdate=118;
D2D1_COLOR_F color(uint32_t rgb) { return D2D1::ColorF(rgb); }
COLORREF gdiColor(uint32_t rgb) { return RGB((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255); }
void check(HRESULT hr, const char* message) { if (FAILED(hr)) throw std::runtime_error(message); }
}
Launcher::Launcher(HINSTANCE instance, std::filesystem::path dataRoot, bool smoke, std::filesystem::path initialRom)
    : instance_(instance), dataRoot_(std::move(dataRoot)), initialRom_(std::move(initialRom)), log_(dataRoot_ / "logs" / "launcher.log"), smoke_(smoke) {}
Launcher::~Launcher() {
    if (pending_.valid()) pending_.wait();
    artCancelled_=true;
    if (artPending_.valid()) artPending_.wait();
    if (icon_) DestroyIcon(icon_);
    if (gameProcess_) CloseHandle(gameProcess_);
    if (installerMutex_) CloseHandle(installerMutex_);
    if (updateProcess_) CloseHandle(updateProcess_);
    if (font_) DeleteObject(font_);
    for(const auto& file:privateFonts_)RemoveFontResourceExW(file.c_str(),FR_PRIVATE,nullptr);
    if (editBrush_) DeleteObject(editBrush_);
}
int Launcher::run(int showCommand) {
    WNDCLASSEXW wc{sizeof(WNDCLASSEXW)};
    wc.lpfnWndProc = windowProc; wc.hInstance = instance_;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW); wc.lpszClassName = L"FireRedRecompLauncher";
    if (!RegisterClassExW(&wc)) throw std::runtime_error("Could not register launcher window.");
    const auto dpi = GetDpiForSystem();
    RECT size{0, 0, MulDiv(1040, static_cast<int>(dpi), 96), MulDiv(700, static_cast<int>(dpi), 96)};
    AdjustWindowRectExForDpi(&size, WS_OVERLAPPEDWINDOW, FALSE, 0, dpi);
    window_ = CreateWindowExW(0, wc.lpszClassName, L"Pok\u00e9Multi", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, size.right-size.left, size.bottom-size.top, nullptr, nullptr, instance_, this);
    if (!window_) throw std::runtime_error("Could not create launcher window.");
    initialize();
    ShowWindow(window_, showCommand);
    UpdateWindow(window_);
    MSG message{};
    int readResult{};
    while ((readResult = static_cast<int>(GetMessageW(&message, nullptr, 0, 0))) > 0) {
        if (message.message == WM_KEYDOWN && message.wParam == VK_F11) { fullscreen(); continue; }
        if (message.message == WM_KEYDOWN && message.wParam == VK_F3) { overlay_ = !overlay_; InvalidateRect(window_, nullptr, FALSE); continue; }
        if (message.message == WM_KEYDOWN && message.wParam == VK_RETURN) {
            const auto focus = GetFocus();
            if (focus == nameEdit_) command(page_==Page::NewWorld?206:page_==Page::EditName?SaveName:Create);
            else if (focus != window_ && IsWindowEnabled(focus)) SendMessageW(focus, BM_CLICK, 0, 0);
            continue;
        }
        if (!IsDialogMessageW(window_, &message)) { TranslateMessage(&message); DispatchMessageW(&message); }
    }
    return readResult < 0 ? 1 : static_cast<int>(message.wParam);
}
LRESULT CALLBACK Launcher::windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto app = reinterpret_cast<Launcher*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        app = static_cast<Launcher*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        app->window_ = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }
    if (app) {
        try { return app->handle(message, wParam, lParam); }
        catch (const std::exception& e) {
            // Never let C++ exceptions unwind through the Windows message dispatcher.
            MessageBoxW(window, widen(e.what()).c_str(), L"Pok\u00e9Multi", MB_ICONERROR);
            DestroyWindow(window);
            PostQuitMessage(1);
            return 0;
        }
    }
    return DefWindowProcW(window, message, wParam, lParam);
}
void Launcher::initialize() {
    installerMutex_=CreateMutexW(nullptr,FALSE,L"Local\\PokeMulti.AppRunning");
    check(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, factory_.GetAddressOf()), "Direct2D initialization failed.");
    check(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(textFactory_.GetAddressOf())), "DirectWrite initialization failed.");
    Microsoft::WRL::ComPtr<IDWriteFactory3> fontsFactory;check(textFactory_.As(&fontsFactory),"DirectWrite custom fonts are unavailable");
    Microsoft::WRL::ComPtr<IDWriteFontSetBuilder> builder;check(fontsFactory->CreateFontSetBuilder(&builder),"Cannot create the UI font collection");
    for(const auto* name:{L"PixelOperator.ttf",L"PixelOperator-Bold.ttf"}){
        const auto file=executableFolder()/"Fonts"/name;
        if(!AddFontResourceExW(file.c_str(),FR_PRIVATE,nullptr))throw std::runtime_error("Cannot load supplied UI font");privateFonts_.push_back(file);
        Microsoft::WRL::ComPtr<IDWriteFontFile> fontFile;check(fontsFactory->CreateFontFileReference(file.c_str(),nullptr,&fontFile),"Cannot read supplied font");
        Microsoft::WRL::ComPtr<IDWriteFontFaceReference> face;check(fontsFactory->CreateFontFaceReference(fontFile.Get(),0,DWRITE_FONT_SIMULATIONS_NONE,&face),"Cannot create font face");
        check(builder->AddFontFaceReference(face.Get()),"Cannot add font face");
    }
    Microsoft::WRL::ComPtr<IDWriteFontSet> set;check(builder->CreateFontSet(&set),"Cannot finish font set");
    check(fontsFactory->CreateFontCollectionFromFontSet(set.Get(),&fontCollection_),"Cannot finish UI fonts");
    logo_=readImage(executableFolder()/"Program_Icon.png");
    // Window/taskbar icon uses the same source pixels at an exact 2x scale.
    BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=32;bi.bmiHeader.biHeight=-32;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;
    void* pixels=nullptr;const auto dib=CreateDIBSection(nullptr,&bi,DIB_RGB_COLORS,&pixels,nullptr,0);
    const auto bgra=premultipliedBgra(logo_);
    if(dib){for(int y=0;y<32;++y)for(int x=0;x<32;++x)static_cast<uint32_t*>(pixels)[y*32+x]=bgra[(y*logo_.height/32)*logo_.width+x*logo_.width/32];
        const std::array<uint8_t,128> emptyMask{};const auto mask=CreateBitmap(32,32,1,1,emptyMask.data());ICONINFO info{};info.fIcon=TRUE;info.hbmColor=dib;info.hbmMask=mask;icon_=CreateIconIndirect(&info);DeleteObject(mask);DeleteObject(dib);
        SendMessageW(window_,WM_SETICON,ICON_BIG,reinterpret_cast<LPARAM>(icon_));SendMessageW(window_,WM_SETICON,ICON_SMALL,reinterpret_cast<LPARAM>(icon_));}
    editBrush_ = CreateSolidBrush(gdiColor(Cream));
    nameEdit_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"Trainer", WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL,
        0,0,0,0,window_,reinterpret_cast<HMENU>(201),instance_,nullptr);
    if (!nameEdit_ || !editBrush_) throw std::runtime_error("Could not create profile controls.");
    SendMessageW(nameEdit_, EM_SETLIMITTEXT, 24, 0);
    SetTimer(window_, 1, 50, nullptr);
    if (smoke_) SetTimer(window_, 2, 1800, nullptr);
    log_.write("RUNTIME", std::string("Launcher ")+BuildVersion+". Native runtime and online rooms enabled.");
    const auto lastUpdate=dataRoot_/"update-last.txt";
    if(std::filesystem::exists(lastUpdate)&&std::filesystem::file_size(lastUpdate)<4096){std::ifstream file(lastUpdate);std::string message;std::getline(file,message);updateStatus_=widen(message);}
    try {
        profile_ = loadProfile(dataRoot_ / "profile.cfg");
        if (!initialRom_.empty()) validateAsync(initialRom_);
        else if (profile_) validateAsync(profile_->romPath, profile_->romSha256);
    } catch (const std::exception& e) {
        profile_.reset();
        status_ = L"Your profile could not be loaded. Select your ROM to set up again.";
        log_.write("SAVE", e.what());
        if (!smoke_) MessageBoxW(window_, widen(e.what()).c_str(), L"Profile needs attention", MB_ICONWARNING);
    }
    layout();
}
LRESULT Launcher::handle(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_ERASEBKGND: return 1;
    case WM_PRINTCLIENT: if (factory_) paintToDc(reinterpret_cast<HDC>(wParam)); return 0;
    case WM_APP + 40: saveCapture(); return 0;
    case WM_PAINT: if (factory_) paint(); else { PAINTSTRUCT ps{}; BeginPaint(window_, &ps); EndPaint(window_, &ps); } return 0;
    case WM_SIZE:
        if (target_) { const auto hr = target_->Resize(D2D1::SizeU(LOWORD(lParam),HIWORD(lParam))); if (FAILED(hr)) { brush_.Reset(); target_.Reset(); } }
        if (factory_) layout();
        InvalidateRect(window_, nullptr, FALSE); return 0;
    case WM_GETMINMAXINFO: {
        auto min = reinterpret_cast<MINMAXINFO*>(lParam);
        const auto dpi = GetDpiForWindow(window_);
        min->ptMinTrackSize = {MulDiv(900, static_cast<int>(dpi), 96), MulDiv(660, static_cast<int>(dpi), 96)};
        return 0;
    }
    case WM_DPICHANGED: {
        const auto rect = reinterpret_cast<RECT*>(lParam);
        SetWindowPos(window_, nullptr, rect->left, rect->top, rect->right-rect->left, rect->bottom-rect->top, SWP_NOZORDER);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        const float x=(GET_X_LPARAM(lParam)-offsetX_)/scale_,y=(GET_Y_LPARAM(lParam)-offsetY_)/scale_;
        if(x>=40&&x<=410&&y>=203&&y<510){dragging_=true;dragX_=x;dragY_=y;SetCapture(window_);}return 0;
    }
    case WM_MOUSEMOVE:
        if(dragging_){const float x=(GET_X_LPARAM(lParam)-offsetX_)/scale_,y=(GET_Y_LPARAM(lParam)-offsetY_)/scale_;
            yaw_+=(x-dragX_)*.012f;pitch_=std::clamp(pitch_+(y-dragY_)*.01f,-1.2f,1.2f);dragX_=x;dragY_=y;InvalidateRect(window_,nullptr,FALSE);}return 0;
    case WM_LBUTTONUP: if(dragging_){dragging_=false;ReleaseCapture();}return 0;
    case WM_CAPTURECHANGED: dragging_=false;return 0;
    case WM_COMMAND: if (HIWORD(wParam) == BN_CLICKED) command(LOWORD(wParam)); return 0;
    case WM_DRAWITEM: drawButton(*reinterpret_cast<DRAWITEMSTRUCT*>(lParam)); return TRUE;
    case WM_CTLCOLOREDIT: {
        auto dc = reinterpret_cast<HDC>(wParam);
        SetTextColor(dc,gdiColor(Ink)); SetBkColor(dc,gdiColor(Cream));
        return reinterpret_cast<LRESULT>(editBrush_);
    }
    case WM_TIMER:
        pollUpdate();
        if(!IsWindow(window_))return 0;
        if (gameProcess_ && WaitForSingleObject(gameProcess_,0)==WAIT_OBJECT_0) {
            DWORD code=0; GetExitCodeProcess(gameProcess_,&code); CloseHandle(gameProcess_); gameProcess_=nullptr;
            ShowWindow(window_,SW_SHOW);
            SetForegroundWindow(window_);
            status_=code==0 ? L"World closed. Your latest checkpoint is kept by the world owner." : L"Game stopped with an error. See runtime.log in your profile folder.";
            const auto reason=dataRoot_/"world-return.txt";if(std::filesystem::exists(reason)){auto bytes=readWorldFile(reason,1024);status_=widen(std::string(bytes.begin(),bytes.end()));std::filesystem::remove(reason);}
            page_=Page::Menu;
            layout();
        }
        if (wParam == 1) {
            pollValidation();
            if(artBusy_&&artPending_.valid()&&artPending_.wait_for(std::chrono::seconds(0))==std::future_status::ready){
                try{art_=artPending_.get();}catch(const std::exception& e){log_.write("ART",e.what());}
                artBusy_=false;layout();
            }
        }
        else if (wParam == 2) { log_.write("RENDER", "Smoke test completed."); DestroyWindow(window_); }
        return 0;
    case WM_DESTROY: KillTimer(window_,1); KillTimer(window_,2); PostQuitMessage(0); return 0;
    default: return DefWindowProcW(window_,message,wParam,lParam);
    }
}
void Launcher::button(int id, std::wstring label, D2D1_RECT_F rect, bool primary, bool enabled) {
    auto& b = buttons_[id];
    b.label = std::move(label); b.rect = rect; b.primary = primary;
    if (!b.window) {
        b.window = CreateWindowExW(0,L"BUTTON",b.label.c_str(), WS_CHILD | WS_TABSTOP | BS_OWNERDRAW,
            0,0,0,0,window_,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),instance_,nullptr);
        if (!b.window) throw std::runtime_error("Could not create a menu button.");
    }
    SetWindowTextW(b.window,b.label.c_str());
    SendMessageW(b.window,WM_SETFONT,reinterpret_cast<WPARAM>(font_),TRUE);
    MoveWindow(b.window, static_cast<int>(offsetX_+rect.left*scale_),static_cast<int>(offsetY_+rect.top*scale_),
        static_cast<int>((rect.right-rect.left)*scale_),static_cast<int>((rect.bottom-rect.top)*scale_),TRUE);
    EnableWindow(b.window,enabled); ShowWindow(b.window,SW_SHOW);
}
#include "launcher_worlds.inc"
#include "launcher_updates.inc"
void Launcher::layout() {
    RECT rect{}; GetClientRect(window_,&rect);
    scale_ = (std::min)(static_cast<float>(rect.right)/1040.f, static_cast<float>(rect.bottom)/700.f);
    scale_ = (std::max)(scale_,0.25f);
    offsetX_ = (static_cast<float>(rect.right)-1040.f*scale_)/2;
    offsetY_ = (static_cast<float>(rect.bottom)-700.f*scale_)/2;
    if (font_) DeleteObject(font_);
    font_ = CreateFontW(-static_cast<int>(20*scale_),0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,NONANTIALIASED_QUALITY,DEFAULT_PITCH,L"Pixel Operator");
    for (auto& [id,b] : buttons_) { (void)id; ShowWindow(b.window,SW_HIDE); }
    ShowWindow(nameEdit_,SW_HIDE);for(auto edit:worldEdits_)if(edit)ShowWindow(edit,SW_HIDE);
    button(Artwork,artBusy_?L"Loading game art...":L"Choose label art",D2D1::RectF(70,550,380,586),false,validated_.has_value()&&!artBusy_);
    button(Quit,L"Quit",D2D1::RectF(894,639,996,676));
    if(profile_)button(EditName,L"Edit",D2D1::RectF(909,47,997,85),false,!gameProcess_&&!updateProcess_&&!busy_);
    if (page_ == Page::Welcome) {
        button(Browse,busy_ ? L"Validating your ROM..." : L"Select ROM",D2D1::RectF(472,399,960,453),true,!busy_);
        button(Updates,L"Update program",D2D1::RectF(472,559,960,596),false,!busy_);
    } else if (page_ == Page::Profile||page_==Page::EditName) {
        MoveWindow(nameEdit_,static_cast<int>(offsetX_+472*scale_),static_cast<int>(offsetY_+309*scale_),
            static_cast<int>(488*scale_),static_cast<int>(48*scale_),TRUE);
        SendMessageW(nameEdit_,WM_SETFONT,reinterpret_cast<WPARAM>(font_),TRUE);
        SendMessageW(nameEdit_,EM_SETLIMITTEXT,24,0);
        ShowWindow(nameEdit_,SW_SHOW);
        const bool editing=page_==Page::EditName;
        button(editing?SaveName:Create,editing?L"Save username":L"Create local profile",D2D1::RectF(472,390,960,444),true);
        button(editing?Back:Browse,editing?L"Cancel":L"Choose another ROM",D2D1::RectF(472,461,960,505),false,!busy_);
    } else if(page_==Page::Updates){
        button(CheckUpdate,updateProcess_?L"Please wait...":L"Check for updates",D2D1::RectF(472,409,960,453),false,!updateProcess_&&!gameProcess_);
        button(InstallUpdate,L"Download & restart",D2D1::RectF(472,469,960,513),true,updateAvailable_&&!updateProcess_&&!gameProcess_);
        button(Back,L"Back",D2D1::RectF(472,545,960,584),false,!updateProcess_);
    } else if (page_==Page::Menu||page_==Page::NewWorld||page_==Page::HostWorld||page_==Page::JoinWorld) {
        layoutWorlds();
    } else {
        button(Details,L"ROM details & verification",D2D1::RectF(472,240,960,287));
        button(Probe,L"Open runtime log",D2D1::RectF(472,300,960,347),false,!busy_);
        button(Fullscreen,fullscreen_ ? L"Windowed mode  /  F11" : L"Borderless fullscreen  /  F11",D2D1::RectF(472,360,960,407));
        button(Browse,L"Change ROM & profile",D2D1::RectF(472,420,960,467),false,!busy_);
        button(Back,L"Back to menu",D2D1::RectF(472,487,708,534),true);
        button(Updates,L"Update program",D2D1::RectF(724,487,960,534));
    }
    InvalidateRect(window_,nullptr,FALSE);
}
void Launcher::drawButton(const DRAWITEMSTRUCT& item) {
    const auto found = buttons_.find(static_cast<int>(item.CtlID));
    if (found == buttons_.end()) return;
    const auto& b = found->second;
    const bool disabled = (item.itemState & ODS_DISABLED) != 0;
    const bool pressed = (item.itemState & ODS_SELECTED) != 0;
    auto fill = CreateSolidBrush(gdiColor(disabled ? 0xF0EEE7 : b.primary ? (pressed ? 0xAD3D31 : Red) : (pressed ? 0xE7ECED : 0xF5F6F1)));
    auto outside = CreateSolidBrush(gdiColor(Cream));
    FillRect(item.hDC,&item.rcItem,outside); DeleteObject(outside);
    auto border = CreatePen(PS_SOLID,1,gdiColor(disabled ? 0xE6E1D7 : b.primary ? Red : 0xCCD3D2));
    auto oldBrush = SelectObject(item.hDC,fill);
    auto oldPen = SelectObject(item.hDC,border);
    const int radius = static_cast<int>(12*scale_);
    RoundRect(item.hDC,0,0,item.rcItem.right,item.rcItem.bottom,radius,radius);
    auto oldFont = SelectObject(item.hDC,font_);
    SetBkMode(item.hDC,TRANSPARENT);
    SetTextColor(item.hDC,gdiColor(disabled ? 0x92968F : b.primary ? 0xFFFFFF : Ink));
    RECT content = item.rcItem;
    content.left += static_cast<int>(18*scale_);
    content.right -= static_cast<int>(18*scale_);
    DrawTextW(item.hDC,b.label.c_str(),-1,&content,DT_SINGLELINE|DT_VCENTER|DT_LEFT|DT_END_ELLIPSIS|DT_NOPREFIX);
    if (item.itemState & ODS_FOCUS) { RECT focus = item.rcItem; InflateRect(&focus,-5,-5); DrawFocusRect(item.hDC,&focus); }
    SelectObject(item.hDC,oldFont); SelectObject(item.hDC,oldPen); SelectObject(item.hDC,oldBrush);
    DeleteObject(fill); DeleteObject(border);
}
void Launcher::panel(D2D1_RECT_F rect, uint32_t fill, float radius, uint32_t border) {
    brush_->SetColor(color(fill));
    const auto rounded = D2D1::RoundedRect(rect,radius,radius);
    surface_->FillRoundedRectangle(rounded,brush_.Get());
    if (border) { brush_->SetColor(color(border)); surface_->DrawRoundedRectangle(rounded,brush_.Get(),1); }
}
void Launcher::text(std::wstring_view value, D2D1_RECT_F rect, float size, uint32_t fill, bool bold) {
    Microsoft::WRL::ComPtr<IDWriteTextFormat> format;
    check(textFactory_->CreateTextFormat(L"Pixel Operator",fontCollection_.Get(),bold ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,size,L"en-us",&format),"Could not create text format.");
    brush_->SetColor(color(fill));
    surface_->DrawText(value.data(),static_cast<UINT32>(value.size()),format.Get(),rect,brush_.Get(),D2D1_DRAW_TEXT_OPTIONS_CLIP);
}
void Launcher::bitmap(const Image& image,D2D1_RECT_F rect,bool nearest){
    if(image.empty())return;
    const auto pixels=premultipliedBgra(image);Microsoft::WRL::ComPtr<ID2D1Bitmap> bitmap;
    const auto props=D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED));
    check(surface_->CreateBitmap(D2D1::SizeU(image.width,image.height),pixels.data(),image.width*4,props,&bitmap),"Cannot create image surface");
    surface_->DrawBitmap(bitmap.Get(),rect,1,nearest?D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR:D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
}
void Launcher::selectArtwork(){
    if(!validated_||artBusy_)return;
    Microsoft::WRL::ComPtr<IFileOpenDialog> dialog;check(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&dialog)),"Cannot open image selection");
    const COMDLG_FILTERSPEC filters[]{{L"Cartridge label image (*.png;*.jpg;*.jpeg)",L"*.png;*.jpg;*.jpeg"}};
    check(dialog->SetFileTypes(1,filters),"Cannot set image filter");dialog->SetTitle(L"Choose cartridge label artwork");
    const auto hr=dialog->Show(window_);if(hr==HRESULT_FROM_WIN32(ERROR_CANCELLED))return;check(hr,"Image selection failed");
    Microsoft::WRL::ComPtr<IShellItem> item;check(dialog->GetResult(&item),"Cannot read selected image");PWSTR file=nullptr;check(item->GetDisplayName(SIGDN_FILESYSPATH,&file),"Cannot resolve image path");
    const std::filesystem::path path(file);CoTaskMemFree(file);auto image=readImage(path);
    const auto destination=dataRoot_/"artwork"/validated_->report.sha256/"label.png";
    std::filesystem::create_directories(destination.parent_path());
    std::error_code sameError;
    if(!std::filesystem::equivalent(path,destination,sameError))std::filesystem::copy_file(path,destination,std::filesystem::copy_options::overwrite_existing);art_=std::move(image);InvalidateRect(window_,nullptr,FALSE);
}
void Launcher::paint() {
    PAINTSTRUCT ps{}; BeginPaint(window_,&ps);
    try {
        if (!target_) {
            RECT rect{}; GetClientRect(window_,&rect);
            check(factory_->CreateHwndRenderTarget(D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
                D2D1::PixelFormat(),96,96),D2D1::HwndRenderTargetProperties(window_,D2D1::SizeU(rect.right,rect.bottom)),
                &target_),"Could not create Direct2D window surface.");
        }
        const auto hr = drawScene(target_.Get());
        if (hr == D2DERR_RECREATE_TARGET) { brush_.Reset(); target_.Reset(); }
        else check(hr,"Rendering failed.");
    } catch (...) { EndPaint(window_,&ps); throw; }
    EndPaint(window_,&ps);
}
HRESULT Launcher::drawScene(ID2D1RenderTarget* surface) {
    surface_ = surface;
    brush_.Reset();
    check(surface_->CreateSolidColorBrush(color(Ink),&brush_),"Could not create drawing brush.");
        surface_->BeginDraw();
        surface_->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_ALIASED);
        surface_->SetTransform(D2D1::Matrix3x2F::Identity());
        surface_->Clear(color(0xF4F0E4));
        surface_->SetTransform(D2D1::Matrix3x2F::Scale(scale_,scale_) * D2D1::Matrix3x2F::Translation(offsetX_,offsetY_));
        panel(D2D1::RectF(0,0,1040,8),Red,0);
        bitmap(logo_,D2D1::RectF(40,36,88,84));
        text(L"Pok\u00e9Multi",D2D1::RectF(105,31,400,69),30,Ink,true);
        text(L"Y O U R   G A M E S .   T O G E T H E R .",D2D1::RectF(107,71,550,94),10,Muted,true);
        if(profile_){
            text(L"ONLINE USERNAME",D2D1::RectF(678,31,895,48),12,Muted,true);
            text(widen(profile_->playerName),D2D1::RectF(678,54,895,83),16,Ink,true);
        }
        const float badgeLeft=profile_?434.f:782.f;
        panel(D2D1::RectF(badgeLeft,44,badgeLeft+215,78),0xE8EEDC,16);
        text(std::wstring(L"EXPERIMENTAL  /  ")+BuildVersionWide,D2D1::RectF(badgeLeft+23,52,badgeLeft+203,75),12,0x56634C,true);
        panel(D2D1::RectF(40,121,410,611),0x1C222C,18);
        text(L"ON YOUR SHELF",D2D1::RectF(68,147,385,171),12,0xA4AEB7,true);
        text(L"Pick up your adventure.",D2D1::RectF(68,178,390,210),22,0xF6F4ED,true);
        for(int y=230;y<484;y+=20)for(int x=70;x<390;x+=20){brush_->SetColor(color(0x303641));surface_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(float(x),float(y)),.7f,.7f),brush_.Get());}
        brush_->SetColor(color(0x131820));surface_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(225,448),116,15),brush_.Get());
        const auto style=cartridgeStyle(validated_?validated_->report.gameCode:"");
        bitmap(renderCartridge(art_,style.color,yaw_,pitch_,740,640),D2D1::RectF(40,203,410,523),false);
        text(L"DRAG TO ROTATE",D2D1::RectF(164,469,385,490),10,0x88959F,true);
        text(style.title,D2D1::RectF(70,504,390,537),21,0xF6F4ED,true);
        panel(D2D1::RectF(437,126,1002,616),0xE6E0D2,18);
        panel(D2D1::RectF(433,121,998,611),Cream,18,Line);
        if (page_ == Page::Welcome) {
            text(L"WELCOME TO POK\u00c9MULTI",D2D1::RectF(472,158,962,184),12,Red,true);
            text(L"Every journey starts here.",D2D1::RectF(472,199,962,248),28,Ink,true);
            text(L"Bring your own game.\nWe'll check the ROM and get your\nlocal profile ready.",D2D1::RectF(472,266,951,352),18,Muted);
            text(L"CURRENT SUPPORT: FIRERED US 1.0 / 1.1",D2D1::RectF(472,366,960,391),12,Muted,true);
            text(L"Your ROM stays on your computer.\nLoad a GBA file or a ZIP containing one ROM.",D2D1::RectF(472,477,948,551),14,Muted);
        } else if (page_ == Page::Profile||page_==Page::EditName) {
            text(page_==Page::EditName?L"YOUR PROFILE":L"ROM VERIFIED",D2D1::RectF(472,158,962,183),12,0x60794F,true);
            text(page_==Page::EditName?L"A new name. Same adventure.":L"Make it your adventure.",D2D1::RectF(472,199,962,248),26,Ink,true);
            text(L"ONLINE USERNAME",D2D1::RectF(472,278,962,306),12,Muted,true);
            text(L"Used in rooms, chat and your friends list.\nYour in-game trainer name and progress stay the same.",D2D1::RectF(472,529,953,580),14,Muted);
        } else if(page_==Page::Updates){
            text(L"PROGRAM UPDATES",D2D1::RectF(472,153,960,181),12,Red,true);
            text(L"Ready for the next adventure.",D2D1::RectF(472,191,960,235),26,Ink,true);
            text(std::wstring(L"INSTALLED  ")+BuildVersionWide,D2D1::RectF(472,255,960,281),16,Ink,true);
            text(updateStatus_,D2D1::RectF(472,294,960,359),16,Muted);
            text(L"Your saves and trainer identity stay in place.",D2D1::RectF(472,367,960,394),14,Muted);
        } else if (page_==Page::Menu||page_==Page::NewWorld||page_==Page::HostWorld||page_==Page::JoinWorld) {
            paintWorlds();
        } else {
            text(L"SETTINGS",D2D1::RectF(472,153,962,181),12,Red,true);
            text(L"Your setup, at a glance.",D2D1::RectF(472,191,960,235),28,Ink,true);
            text(L"F3  Diagnostics     F11  Fullscreen",D2D1::RectF(472,556,950,588),14,Muted);
        }
        text(status_,D2D1::RectF(42,639,873,688),13,Muted);
        if (overlay_) {
            panel(D2D1::RectF(58,426,394,595),0xFFF8E9,8,Line);
            const auto revision=validated_ ? widen(revisionName(validated_->report.revision)) : L"No ROM";
            text(L"F3  DEVELOPMENT STATUS\nROM: "+revision+L"\nRuntime: "+BuildVersionWide+L" / native + fallback\nOnline: 2-32 trainers / F2 in game\nSaves: trainers stored in the owner world\nFollowers / visible wild: F2 > Options",
                D2D1::RectF(71,438,387,587),13,Ink);
        }
    return surface_->EndDraw();
}
void Launcher::paintToDc(HDC dc) {
    RECT rect{}; GetClientRect(window_,&rect);
    Microsoft::WRL::ComPtr<ID2D1DCRenderTarget> capture;
    const auto properties = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_IGNORE),96,96);
    check(factory_->CreateDCRenderTarget(&properties,&capture),"Could not create window capture surface.");
    check(capture->BindDC(dc,&rect),"Could not bind window capture surface.");
    check(drawScene(capture.Get()),"Could not render window capture.");
}
void Launcher::saveCapture() {
    // In-process capture: Direct2D cannot bind a foreign process's memory DC.
    RECT rect{}; GetClientRect(window_,&rect);
    if (rect.right <= 0 || rect.bottom <= 0 || rect.right > 8192 || rect.bottom > 8192)
        throw std::runtime_error("Invalid capture size.");
    struct Surface {
        HDC dc = CreateCompatibleDC(nullptr);
        HBITMAP bitmap{};
        HGDIOBJ previous{};
        ~Surface() { if (previous) SelectObject(dc,previous); if (bitmap) DeleteObject(bitmap); if (dc) DeleteDC(dc); }
    } surface;
    if (!surface.dc) throw std::runtime_error("Could not create capture DC.");
    BITMAPINFO info{};
    info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=rect.right; info.bmiHeader.biHeight=-rect.bottom;
    info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32; info.bmiHeader.biCompression=BI_RGB;
    void* pixels{};
    surface.bitmap=CreateDIBSection(surface.dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
    if (!surface.bitmap) throw std::runtime_error("Could not allocate capture bitmap.");
    surface.previous=SelectObject(surface.dc,surface.bitmap);
    paintToDc(surface.dc);
    auto drawControl=[&](HWND control) {
        if (!(GetWindowLongPtrW(control,GWL_STYLE) & WS_VISIBLE)) return;
        RECT bounds{}; GetWindowRect(control,&bounds);
        POINT origin{bounds.left,bounds.top}; ScreenToClient(window_,&origin);
        SetViewportOrgEx(surface.dc,origin.x,origin.y,nullptr);
        SendMessageW(control,WM_PRINT,reinterpret_cast<WPARAM>(surface.dc),PRF_CLIENT|PRF_NONCLIENT|PRF_ERASEBKGND);
        SetViewportOrgEx(surface.dc,0,0,nullptr);
    };
    for (const auto& [id,b] : buttons_) { (void)id; drawControl(b.window); }
    drawControl(nameEdit_);for(auto edit:worldEdits_)if(edit)drawControl(edit);
    GdiFlush();
    const auto byteCount=static_cast<DWORD>(rect.right)*static_cast<DWORD>(rect.bottom)*4;
    BITMAPFILEHEADER header{};
    header.bfType=0x4D42; header.bfOffBits=sizeof(header)+sizeof(BITMAPINFOHEADER); header.bfSize=header.bfOffBits+byteCount;
    const auto temporary=dataRoot_/"ui-capture.bmp.tmp", destination=dataRoot_/"ui-capture.bmp";
    std::ofstream out(temporary,std::ios::binary|std::ios::trunc);
    out.write(reinterpret_cast<const char*>(&header),sizeof(header));
    out.write(reinterpret_cast<const char*>(&info.bmiHeader),sizeof(BITMAPINFOHEADER));
    out.write(static_cast<const char*>(pixels),byteCount);
    out.close();
    if (!out || !MoveFileExW(temporary.c_str(),destination.c_str(),MOVEFILE_REPLACE_EXISTING))
        throw std::runtime_error("Could not save launcher capture.");
}
void Launcher::error(const std::string& message) {
    log_.write("ROM",message);
    MessageBoxW(window_,widen(message).c_str(),L"Pok\u00e9Multi",MB_OK|MB_ICONWARNING);
}
void Launcher::selectRom() {
    if (busy_) return;
    if(artBusy_){artCancelled_=true;artPending_.wait();try{artPending_.get();}catch(const std::exception& e){log_.write("ART",e.what());}artBusy_=false;}
    Microsoft::WRL::ComPtr<IFileOpenDialog> dialog;
    check(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&dialog)),"Cannot open ROM selection.");
    const COMDLG_FILTERSPEC filter[]{ {L"Game Boy Advance ROM or ZIP (*.gba;*.zip)",L"*.gba;*.zip"} };
    check(dialog->SetFileTypes(1,filter),"Cannot set ROM filter.");
    DWORD options{}; check(dialog->GetOptions(&options),"Cannot read file dialog options.");
    check(dialog->SetOptions(options | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST | FOS_FORCEFILESYSTEM),"Cannot set file dialog options.");
    dialog->SetTitle(L"Select your ROM");
    const auto hr = dialog->Show(window_);
    if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) return;
    check(hr,"ROM selection failed.");
    Microsoft::WRL::ComPtr<IShellItem> item;
    check(dialog->GetResult(&item),"Cannot read selected ROM.");
    PWSTR path{};
    check(item->GetDisplayName(SIGDN_FILESYSPATH,&path),"Cannot resolve selected ROM path.");
    const std::filesystem::path selected(path);
    CoTaskMemFree(path);
    validateAsync(selected);
}
void Launcher::validateAsync(const std::filesystem::path& path, std::string expected) {
    if (busy_) return;
    expectedHash_ = std::move(expected);
    busy_ = true; status_ = L"Checking ROM identity and SHA-256...";
    pending_ = std::async(std::launch::async,[path] {
        Validation result; result.path = path;
        try { const auto bytes = readRom(path); result.report = inspectRom(bytes); }
        catch (const std::exception& e) { result.error = e.what(); }
        return result;
    });
    layout();
}
void Launcher::pollValidation() {
    if (!busy_ || !pending_.valid() || pending_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
    auto result = pending_.get();
    busy_ = false;
    if (!result.error.empty() || !result.report.supported() || (!expectedHash_.empty() && expectedHash_ != result.report.sha256)) {
        const auto reason = !result.error.empty() ? result.error : !result.report.supported() ? describe(result.report)
            : "The ROM at your saved path has changed. Select it again to verify a new profile.";
        status_ = L"ROM validation failed. Select a supported FireRed ROM.";
        if (!expectedHash_.empty()) { profile_.reset(); validated_.reset(); page_ = Page::Welcome; }
        expectedHash_.clear();
        layout(); error(reason); return;
    }
    validated_ = std::move(result);
    art_={};yaw_=-.40f;pitch_=-.18f;artBusy_=true;artCancelled_=false;
    const auto artRom=validated_->path,artCache=dataRoot_/"artwork"/validated_->report.sha256;
    const auto gameCode=validated_->report.gameCode;
    artPending_=std::async(std::launch::async,[this,artRom,artCache,gameCode]{return cartridgeArtwork(artRom,artCache,gameCode,&artCancelled_);});
    status_ = widen(revisionName(validated_->report.revision)) + L"  /  SHA-256 verified  /  Stored locally";
    log_.write("ROM","Validated " + revisionName(validated_->report.revision) + " SHA-256 " + validated_->report.sha256);
    if (!expectedHash_.empty() && profile_) page_ = Page::Menu;
    else {
        page_ = Page::Profile;
        if (profile_) SetWindowTextW(nameEdit_,widen(profile_->playerName).c_str());
    }
    expectedHash_.clear();
    layout();
    if (page_ == Page::Profile) SetFocus(nameEdit_);
    else SetFocus(buttons_.at(Settings).window);
}
void Launcher::command(int id) {
    try {
        if(worldCommand(id))return;
        switch (id) {
        case EditName:
            if(!profile_||gameProcess_||updateProcess_||busy_)return;
            page_=Page::EditName;SetWindowTextW(nameEdit_,widen(profile_->playerName).c_str());layout();SetFocus(nameEdit_);break;
        case SaveName: {
            if(page_!=Page::EditName||!profile_||gameProcess_)return;
            wchar_t name[25]{};GetWindowTextW(nameEdit_,name,25);
            profile_=renameProfile(dataRoot_/"profile.cfg",narrow(name));
            page_=Page::Menu;status_=L"Username saved. Your in-game trainer name and progress are unchanged.";layout();break;
        }
        case Updates: page_=Page::Updates;layout();break;
        case CheckUpdate: if(page_==Page::Updates)startUpdate(L"Check");break;
        case InstallUpdate: if(page_==Page::Updates&&updateAvailable_)startUpdate(L"Stage");break;
        case Artwork: selectArtwork(); break;
        case Browse: selectRom(); break;
        case Continue: playGame(); break;
        case Host: page_=Page::HostWorld;layout();SetWindowTextW(worldEdits_[1],widen(worldRandomId().substr(0,12)).c_str());break;
        case Join: page_=Page::JoinWorld;layout();SetWindowTextW(worldEdits_[1],L"");break;
        case Create: {
            if (page_ != Page::Profile || !validated_ || busy_) return;
            wchar_t name[25]{};
            GetWindowTextW(nameEdit_,name,25);
            Profile next{validated_->path,validated_->report.sha256,narrow(name)};
            saveProfile(dataRoot_ / "profile.cfg",next);
            profile_ = next; page_ = Page::Menu;
            worlds_.clear();refreshWorlds();
            log_.write("SAVE","Profile saved and world list prepared.");
            layout(); SetFocus(buttons_.at(Settings).window); break;
        }
        case Settings: page_ = Page::Settings; layout(); SetFocus(buttons_.at(Details).window); break;
        case Back: if(updateProcess_)return;page_ = profile_?Page::Menu:Page::Welcome; layout(); SetFocus(buttons_.at(profile_?Settings:Browse).window); break;
        case Details:
            if (validated_) MessageBoxW(window_,widen(describe(validated_->report)).c_str(),L"ROM verification",MB_OK|MB_ICONINFORMATION);
            break;
        case Probe: {
            const auto path=dataRoot_/"runtime.log";
            if(!std::filesystem::exists(path)){MessageBoxW(window_,L"Play a game once to create the runtime log.",L"Runtime log",MB_OK|MB_ICONINFORMATION);break;}
            ShellExecuteW(window_,L"open",L"notepad.exe",(L"\""+path.wstring()+L"\"").c_str(),nullptr,SW_SHOWNORMAL);
            break;
        }
        case Fullscreen: fullscreen(); break;
        case Quit: DestroyWindow(window_); break;
        default: break;
        }
    } catch (const std::exception& e) { error(e.what()); }
}
void Launcher::playGame(int mode) {
    if (!profile_ || !validated_ || busy_ || gameProcess_||updateProcess_) return;
    wchar_t exeBuffer[32768]{}; GetModuleFileNameW(nullptr,exeBuffer,32768);
    const auto exe=std::filesystem::path(exeBuffer).parent_path()/L"pokemulti_game.exe";
    if (!std::filesystem::exists(exe)) throw std::runtime_error("pokemulti_game.exe is missing. Run the build script first.");
    if(worlds_.empty())refreshWorlds();
    const auto world=worlds_.at(selectedWorld_);
    const auto save=dataRoot_/"runtime-bootstrap"/"trainer.sav";
    std::wstring hostAddress,hostKey;unsigned hostPort=38475,hostCapacity=4;
    if(mode){hostKey=worldEditText(1);if(hostKey.size()<8||hostKey.size()>64)throw std::runtime_error("Use a room key of 8-64 characters.");size_t used=0;const auto portText=worldEditText(2);hostPort=std::stoul(portText,&used);if(used!=portText.size()||!hostPort||hostPort>65535)throw std::runtime_error("Use a port from 1 to 65535.");
        if(mode==1){const auto text=worldEditText(3);hostCapacity=std::stoul(text,&used);if(used!=text.size()||hostCapacity<2||hostCapacity>MaxRoomPlayers)throw std::runtime_error("Choose 2-32 players.");}
        else{hostAddress=worldEditText(0);if(!online::validConnectionIPv4(narrow(hostAddress)))throw std::runtime_error("Enter your friend's IPv4 address.");}
    }
    std::filesystem::create_directories(save.parent_path());
    // Windows forbids quotes in file names. All argument paths end in a file name.
    std::wstring cmd=L"\""+exe.wstring()+L"\" --rom \""+profile_->romPath.wstring()+L"\" --save \""+save.wstring()+L"\" --window";
    auto quote=[](const std::wstring& value){
        std::wstring result=L"\"";unsigned slashes=0;
        for(wchar_t c:value){if(c==L'\\'){++slashes;continue;}if(c==L'"'){result.append(slashes*2+1,L'\\');result+=c;}else{result.append(slashes,L'\\');result+=c;}slashes=0;}
        result.append(slashes*2,L'\\');result+=L'"';return result;
    };
    cmd+=L" --profile-dir "+quote(dataRoot_.wstring())+L" --name "+quote(widen(profile_->playerName));
    cmd+=L" --identity-dir "+quote(dataRoot_.wstring());
    if(mode!=2)cmd+=L" --world-dir "+quote(world.folder.wstring());
    if(mode){cmd+=mode==1?L" --online host":L" --online join";cmd+=L" --room-port "+std::to_wstring(hostPort)+L" --room-key "+quote(hostKey)+L" --capacity "+std::to_wstring(hostCapacity)+L" --rewards "+std::to_wstring(worldRewards_);if(mode==2)cmd+=L" --join-address "+quote(hostAddress);}
    SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,TRUE};
    HANDLE output=CreateFileW((dataRoot_/"runtime.log").c_str(),GENERIC_WRITE,FILE_SHARE_READ,&sa,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(output==INVALID_HANDLE_VALUE) throw std::runtime_error("Could not open the game log.");
    HANDLE input=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&sa,OPEN_EXISTING,0,nullptr);
    STARTUPINFOW startup{sizeof(startup)};startup.dwFlags=STARTF_USESTDHANDLES;startup.hStdInput=input;startup.hStdOutput=output;startup.hStdError=output;
    PROCESS_INFORMATION process{};
    const BOOL ok=CreateProcessW(exe.c_str(),cmd.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,dataRoot_.c_str(),&startup,&process);
    CloseHandle(output);if(input!=INVALID_HANDLE_VALUE)CloseHandle(input);
    if(!ok) throw std::runtime_error("Could not start Pok\xc3\xa9Multi (Windows error "+std::to_string(GetLastError())+").");
    CloseHandle(process.hThread);gameProcess_=process.hProcess;
    log_.write("RUNTIME", "Started game: "+narrow(exe.wstring()));
    ShowWindow(window_,SW_HIDE);
    status_=L"World open. Progress checkpoints automatically; the host can save everyone from the game menu.";layout();
}

void Launcher::fullscreen() {
    if (!fullscreen_) {
        placement_.length = sizeof(placement_);
        GetWindowPlacement(window_,&placement_);
        MONITORINFO monitor{sizeof(MONITORINFO)};
        GetMonitorInfoW(MonitorFromWindow(window_,MONITOR_DEFAULTTONEAREST),&monitor);
        SetWindowLongPtrW(window_,GWL_STYLE,GetWindowLongPtrW(window_,GWL_STYLE) & ~WS_OVERLAPPEDWINDOW);
        SetWindowPos(window_,HWND_TOP,monitor.rcMonitor.left,monitor.rcMonitor.top,
            monitor.rcMonitor.right-monitor.rcMonitor.left,monitor.rcMonitor.bottom-monitor.rcMonitor.top,
            SWP_FRAMECHANGED|SWP_NOOWNERZORDER);
    } else {
        SetWindowLongPtrW(window_,GWL_STYLE,GetWindowLongPtrW(window_,GWL_STYLE) | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(window_,&placement_);
        SetWindowPos(window_,nullptr,0,0,0,0,SWP_FRAMECHANGED|SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOOWNERZORDER);
    }
    fullscreen_ = !fullscreen_; layout();
}
}
