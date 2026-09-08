#include <Windows.h>
#include <setupapi.h>
#include <shellapi.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <stdexcept>
#include "payload.h"
namespace fs=std::filesystem;
struct Handle {
    HANDLE value=nullptr;
    ~Handle(){if(value && value!=INVALID_HANDLE_VALUE)CloseHandle(value);}
    Handle(const Handle&)=delete;
    Handle& operator=(const Handle&)=delete;
    explicit Handle(HANDLE handle):value(handle){}
};
struct CabinetContext { fs::path root; DWORD error=ERROR_SUCCESS; };
UINT CALLBACK extract(void* data,UINT notification,UINT_PTR first,UINT_PTR) {
    auto& context=*static_cast<CabinetContext*>(data);
    try {
        if(notification==SPFILENOTIFY_FILEINCABINET) {
            auto& file=*reinterpret_cast<FILE_IN_CABINET_INFO_W*>(first);
            const fs::path relative(file.NameInCabinet);
            if(relative.empty() || relative.is_absolute() || relative.has_root_name())throw std::runtime_error("Invalid payload path");
            for(const auto& part:relative)if(part==L"..")throw std::runtime_error("Invalid payload path");
            const auto target=context.root/relative;
            if(target.native().size()>=MAX_PATH)throw std::runtime_error("Portable directory path is too long");
            fs::create_directories(target.parent_path());
            wcscpy_s(file.FullTargetName,target.c_str());return FILEOP_DOIT;
        }
        if(notification==SPFILENOTIFY_FILEEXTRACTED) {
            context.error=reinterpret_cast<FILEPATHS_W*>(first)->Win32Error;return context.error;
        }
        if(notification==SPFILENOTIFY_NEEDNEWCABINET)return ERROR_INVALID_DATA;
    } catch(...) {context.error=ERROR_INVALID_DATA;return FILEOP_ABORT;}
    return NO_ERROR;
}
std::wstring quote(const std::wstring& value) {
    std::wstring result=L"\"";size_t slashes=0;
    for(wchar_t c:value) {
        if(c==L'\\'){++slashes;continue;}
        result.append(c==L'"'?slashes*2+1:slashes,L'\\');slashes=0;result+=c;
    }
    result.append(slashes*2,L'\\');return result+L'"';
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int) {
    try {
        std::wstring executable(32768,L'\0');
        const auto count=GetModuleFileNameW(nullptr,executable.data(),DWORD(executable.size()));
        if(!count || count>=executable.size())throw std::runtime_error("Cannot locate portable application");
        executable.resize(count);const auto root=fs::path(executable).parent_path();
        const auto cache=root/L".mousewheel"/PayloadId;
        fs::create_directories(cache);
        SetFileAttributesW((root/L".mousewheel").c_str(),FILE_ATTRIBUTE_HIDDEN);
        Handle mutex(CreateMutexW(nullptr,FALSE,(std::wstring(L"Local\\MouseWheelPortable-")+PayloadId).c_str()));
        if(!mutex.value)throw std::runtime_error("Cannot prepare runtime lock");
        const auto wait=WaitForSingleObject(mutex.value,30000);
        if(wait!=WAIT_OBJECT_0 && wait!=WAIT_ABANDONED)throw std::runtime_error("Another launch is preparing the runtime. Please retry.");
        struct Unlock {HANDLE value;~Unlock(){ReleaseMutex(value);}} unlock{mutex.value};
        const auto child=cache/L"MouseWheel.exe";
        if(!fs::exists(cache/L"ready") || !fs::exists(child) || !fs::exists(cache/L"platforms"/L"qwindows.dll")) {
            const auto resource=FindResourceW(instance,MAKEINTRESOURCEW(101),RT_RCDATA);
            const auto loaded=resource?LoadResource(instance,resource):nullptr;
            const auto bytes=loaded?LockResource(loaded):nullptr;
            const auto size=resource?SizeofResource(instance,resource):0;
            if(!bytes || !size)throw std::runtime_error("The portable payload is missing");
            const auto cabinet=cache/L"payload.cab";
            {std::ofstream output(cabinet,std::ios::binary|std::ios::trunc);output.write(static_cast<const char*>(bytes),size);if(!output)throw std::runtime_error("Cannot write runtime files. Move the EXE to a writable folder.");}
            CabinetContext context{cache};
            if(!SetupIterateCabinetW(cabinet.c_str(),0,extract,&context) || context.error || !fs::exists(child))throw std::runtime_error("Cannot unpack runtime files. Move the EXE to a short writable path and retry.");
            fs::remove(cabinet);
            std::ofstream ready(cache/L"ready");ready<<"ready";if(!ready)throw std::runtime_error("Cannot finalize runtime");
        }
        int argc=0;auto** argv=CommandLineToArgvW(GetCommandLineW(),&argc);
        if(!argv)throw std::runtime_error("Cannot read command line");
        struct Arguments {LPWSTR* value;~Arguments(){LocalFree(value);}} arguments{argv};
        std::wstring command=quote(child.native());bool config=false,waitForChild=false;
        for(int i=1;i<argc;++i) {
            const std::wstring value=argv[i];config=config || value==L"--config" || value.starts_with(L"--config=");
            waitForChild=waitForChild || value==L"--smoke-test" || value==L"--version" || value==L"--help";
            command+=L" "+quote(value);
        }
        if(!config)command+=L" --config "+quote((root/L"config.json").native());
        STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION process{};
        if(!CreateProcessW(child.c_str(),command.data(),nullptr,nullptr,FALSE,0,nullptr,root.c_str(),&startup,&process))throw std::runtime_error("Cannot start MouseWheel");
        Handle processHandle(process.hProcess),threadHandle(process.hThread);
        if(waitForChild){WaitForSingleObject(process.hProcess,INFINITE);DWORD code=1;GetExitCodeProcess(process.hProcess,&code);return int(code);}
        return 0;
    } catch(const std::exception& error) {
        const std::string message=error.what();
        MessageBoxA(nullptr,message.c_str(),"MouseWheel",MB_OK|MB_ICONERROR);return 1;
    }
}
