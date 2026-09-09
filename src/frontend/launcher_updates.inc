void Launcher::startUpdate(const std::wstring& mode) {
    if(updateProcess_||gameProcess_||busy_)return;
    if(mode==L"Stage"&&!std::filesystem::exists(executableFolder()/"package-manifest.json"))throw std::runtime_error("Run an extracted release package to install updates. Development builds use the build script.");
    const auto script=executableFolder()/L"tools"/L"update.ps1";
    if(!std::filesystem::exists(script))throw std::runtime_error("The update helper is missing. Extract a complete Pok\xc3\xa9Multi release package first.");
    updateWork_=dataRoot_/L"updates"/widen("job-"+worldRandomId());
    std::filesystem::create_directories(updateWork_);
    std::filesystem::copy_file(script,updateWork_/L"update.ps1");
    launchUpdateWorker(mode);
    updateStatus_=mode==L"Stage"?L"Downloading and verifying the update...":L"Checking GitHub for a newer release...";
    layout();
}
void Launcher::launchUpdateWorker(const std::wstring& mode) {
    auto quote=[](const std::wstring& value){
        std::wstring result=L"\"";unsigned slashes=0;
        for(wchar_t c:value){if(c==L'\\'){++slashes;continue;}result.append(c==L'\"'?slashes*2+1:slashes,L'\\');result+=c;slashes=0;}
        result.append(slashes*2,L'\\');return result+L'\"';
    };
    wchar_t system[32768]{};if(!GetSystemDirectoryW(system,32768))throw std::runtime_error("Cannot locate Windows PowerShell.");
    const auto powershell=std::filesystem::path(system)/L"WindowsPowerShell/v1.0/powershell.exe";
    auto args=quote(powershell.wstring())+L" -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "+quote((updateWork_/L"update.ps1").wstring())+
        L" -Mode "+mode+L" -CurrentVersion "+BuildVersionWide+L" -Installation "+quote(executableFolder().wstring())+
        L" -DataDirectory "+quote(dataRoot_.wstring())+L" -WorkDirectory "+quote(updateWork_.wstring());
    if(mode==L"Apply")args+=L" -ParentPid "+std::to_wstring(GetCurrentProcessId());
    STARTUPINFOW startup{sizeof(startup)};startup.dwFlags=STARTF_USESHOWWINDOW;startup.wShowWindow=SW_HIDE;
    PROCESS_INFORMATION process{};
    if(!CreateProcessW(powershell.c_str(),args.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,updateWork_.c_str(),&startup,&process))throw std::runtime_error("Could not start the updater.");
    CloseHandle(process.hThread);
    if(mode==L"Apply"){CloseHandle(process.hProcess);DestroyWindow(window_);return;}
    updateProcess_=process.hProcess;
}
void Launcher::pollUpdate(){
    if(!updateProcess_||WaitForSingleObject(updateProcess_,0)!=WAIT_OBJECT_0)return;
    DWORD code=1;GetExitCodeProcess(updateProcess_,&code);CloseHandle(updateProcess_);updateProcess_=nullptr;
    std::ifstream input(updateWork_/L"result.txt",std::ios::binary);std::string kind,message;
    if(!input||!std::getline(input,kind)||!std::getline(input,message)||message.size()>2048||code!=0){updateStatus_=L"The update helper stopped. Please try again.";updateAvailable_=false;layout();return;}
    updateAvailable_=false;
    if(kind=="available"||kind=="ready"){
        if(message.find_first_not_of("0123456789.")!=std::string::npos||message.empty()||message.size()>24){updateStatus_=L"Invalid release response.";layout();return;}
        latestVersion_=widen(message);
        if(kind=="ready"){updateStatus_=L"Update verified. Restarting...";launchUpdateWorker(L"Apply");return;}
        updateAvailable_=true;updateStatus_=L"Version "+latestVersion_+L" is ready to install.";
    }else updateStatus_=widen(message);
    layout();
}
