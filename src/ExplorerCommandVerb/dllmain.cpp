// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"

#include <wrl/module.h>
#include <wrl/implements.h>
#include <wrl/client.h>
#include <shobjidl_core.h>
#include <wil\resource.h>
#include <comutil.h>
#include <shellapi.h>
#include <Shlobj.h>
#include <string>
#include <vector>
#include <sstream>

#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "comsupp.lib")

using namespace Microsoft::WRL;

HMODULE       hDll = nullptr;

BOOL APIENTRY DllMain(HMODULE hModule,
                      DWORD   ulReasonForCall,
                      LPVOID /*lpReserved*/)
{
    hDll = hModule;
    switch (ulReasonForCall)
    {
        case DLL_PROCESS_ATTACH:
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
        case DLL_PROCESS_DETACH:
            break;
    }
    return TRUE;
}

// These variables are not exposed as any path name handling probably
// should be a function in here rather than be manipulating strings directly / inline.
constexpr wchar_t thisOsPathSeparator  = L'\\';
constexpr wchar_t otherOsPathSeparator = L'/';
constexpr wchar_t DeviceSeparator      = L':';

// Check if the character given is either type of folder separator.
// if we want to remove support for "other"separators we can just
// change this function and force callers to use NormalizeFolderSeparators on
// filenames first at first point of entry into a program.
inline bool       IsFolderSeparator(wchar_t c)
{
    return (c == thisOsPathSeparator || c == otherOsPathSeparator);
}

std::wstring GetModulePath(HMODULE hMod /*= nullptr*/)
{
    DWORD                      len       = 0;
    DWORD                      bufferLen = MAX_PATH; // MAX_PATH is not the limit here!
    std::unique_ptr<wchar_t[]> path;
    do
    {
        bufferLen += MAX_PATH; // MAX_PATH is not the limit here!
        path = std::make_unique<wchar_t[]>(bufferLen);
        len  = GetModuleFileName(hMod, path.get(), bufferLen);
    } while (len == bufferLen);
    std::wstring sPath = path.get();
    return sPath;
}

std::wstring GetParentDirectory(const std::wstring& path)
{
    static std::wstring noParent;
    size_t              pathLen = path.length();
    size_t              pos;

    for (pos = pathLen; pos > 0;)
    {
        --pos;
        if (IsFolderSeparator(path[pos]))
        {
            size_t fileNameLen = pathLen - (pos + 1);
            // If the path in it's entirety is just a root, i.e. "\", it has no parent.
            if (pos == 0 && fileNameLen == 0)
                return noParent;
            // If the path in it's entirety is server name, i.e. "\\x", it has no parent.
            if (pos == 1 && IsFolderSeparator(path[0]) && IsFolderSeparator(path[1]) && fileNameLen > 0)
                return noParent;
            // If the parent begins with a device and root, i.e. "?:\" then
            // include both in the parent.
            if (pos == 2 && path[pos - 1] == DeviceSeparator)
            {
                // If the path is just a device i.e. not followed by a filename, it has no parent.
                if (fileNameLen == 0)
                    return noParent;
                ++pos;
            }
            // In summary, return everything before the last "\" of a filename unless the
            // whole path given is:
            // a server name, a device name, a root directory, or
            // a device followed by a root directory, in which case return "".
            std::wstring parent = path.substr(0, pos);
            return parent;
        }
    }
    // The path doesn't have a directory separator, we must be looking at either:
    // 1. just a name, like "apple"
    // 2. just a device, like "c:"
    // 3. a device followed by a name "c:apple"

    // 1. and 2. specifically have no parent,
    // For 3. the parent is the device including the separator.
    // We'll return just the separator if that's all there is.
    // It's an odd corner case but allow it through so the caller
    // yields an error if it uses it concatenated with another name rather
    // than something that might work.
    pos = path.find_first_of(DeviceSeparator);
    if (pos != std::wstring::npos)
    {
        // A device followed by a path. The device is the parent.
        std::wstring parent = path.substr(0, pos + 1);
        return parent;
    }
    return noParent;
}

std::wstring GetModuleDir(HMODULE hMod /*= nullptr*/)
{
    return GetParentDirectory(GetModulePath(hMod));
}

class ExplorerCommandBase : public RuntimeClass<RuntimeClassFlags<WinRtClassicComMix | InhibitRoOriginateError>, IExplorerCommand, IObjectWithSite>
{
public:
    virtual const wchar_t* Title(IShellItemArray* items) = 0;
    virtual EXPCMDFLAGS    Flags() { return ECF_DEFAULT; }
    virtual EXPCMDSTATE    State(_In_opt_ IShellItemArray* selection) { return ECS_ENABLED; }

    // IExplorerCommand
    IFACEMETHODIMP         GetTitle(_In_opt_ IShellItemArray* items, _Outptr_result_nullonfailure_ PWSTR* name) override
    {
        *name      = nullptr;
        auto title = wil::make_cotaskmem_string_nothrow(Title(items));
        RETURN_IF_NULL_ALLOC(title);
        *name = title.release();
        return S_OK;
    }
    IFACEMETHODIMP GetIcon(_In_opt_ IShellItemArray*, _Outptr_result_nullonfailure_ PWSTR* icon) override
    {
        *icon = nullptr;
        return E_NOTIMPL;
    }
    IFACEMETHODIMP GetToolTip(_In_opt_ IShellItemArray*, _Outptr_result_nullonfailure_ PWSTR* infoTip) override
    {
        *infoTip = nullptr;
        return E_NOTIMPL;
    }
    IFACEMETHODIMP GetCanonicalName(_Out_ GUID* guidCommandName) override
    {
        *guidCommandName = GUID_NULL;
        return S_OK;
    }
    IFACEMETHODIMP GetState(_In_opt_ IShellItemArray* selection, _In_ BOOL okToBeSlow, _Out_ EXPCMDSTATE* cmdState) override
    {
        *cmdState = State(selection);
        return S_OK;
    }
    IFACEMETHODIMP Invoke(_In_opt_ IShellItemArray* selection, _In_opt_ IBindCtx*) noexcept override
    try
    {
        HWND parent = nullptr;
        if (m_site)
        {
            ComPtr<IOleWindow> oleWindow;
            RETURN_IF_FAILED(m_site.As(&oleWindow));
            RETURN_IF_FAILED(oleWindow->GetWindow(&parent));
        }

        std::wostringstream title;
        title << Title(selection);

        if (selection)
        {
            DWORD count;
            RETURN_IF_FAILED(selection->GetCount(&count));
            title << L" (" << count << L" selected items)";
        }
        else
        {
            title << L"(no selected items)";
        }

        MessageBox(parent, title.str().c_str(), L"TestCommand", MB_OK);
        return S_OK;
    }
    CATCH_RETURN();

    IFACEMETHODIMP GetFlags(_Out_ EXPCMDFLAGS* flags) override
    {
        *flags = Flags();
        return S_OK;
    }
    IFACEMETHODIMP EnumSubCommands(_COM_Outptr_ IEnumExplorerCommand** enumCommands) override
    {
        *enumCommands = nullptr;
        return E_NOTIMPL;
    }

    // IObjectWithSite
    IFACEMETHODIMP SetSite(_In_ IUnknown* site) noexcept override
    {
        m_site = site;
        return S_OK;
    }
    IFACEMETHODIMP GetSite(_In_ REFIID riid, _COM_Outptr_ void** site) noexcept override { return m_site.CopyTo(riid, site); }

protected:
    ComPtr<IUnknown> m_site;
};

class __declspec(uuid("3C557AFF-6181-4BBC-937D-E2FE8844DD49")) GrepWinExplorerCommandHandler final : public ExplorerCommandBase
{
public:
    const wchar_t* Title(IShellItemArray*) override
    {
        return L"Search with grepWin";
    }

    EXPCMDSTATE State(_In_opt_ IShellItemArray*) override
    {
        if (m_site)
        {
            ComPtr<IOleWindow> oleWindow;
            m_site.As(&oleWindow);
            if (oleWindow)
            {
                // We don't want to show the menu on the classic context menu.
                // The classic menu provides an IOleWindow, but the main context
                // menu of the left treeview in explorer does too.
                // So we check the window class name: if it's "NamespaceTreeControl",
                // then we're dealing with the main context menu of the tree view.
                // If it's not, then we're dealing with the classic context menu
                // and there we hide this menu entry.
                HWND hWnd = nullptr;
                oleWindow->GetWindow(&hWnd);
                wchar_t szWndClassName[MAX_PATH] = {0};
                GetClassName(hWnd, szWndClassName, _countof(szWndClassName));
                if (wcscmp(szWndClassName, L"NamespaceTreeControl"))
                    return ECS_HIDDEN;
            }
        }
        return ECS_ENABLED;
    }

    IFACEMETHODIMP GetIcon(_In_opt_ IShellItemArray*, _Outptr_result_nullonfailure_ PWSTR* icon) override
    {
        auto bpPath = GetModuleDir(hDll);
        bpPath += L"\\grepWin.exe,-107";
        auto iconPath = wil::make_cotaskmem_string_nothrow(bpPath.c_str());
        RETURN_IF_NULL_ALLOC(iconPath);
        *icon = iconPath.release();
        return S_OK;
    }

    IFACEMETHODIMP Invoke(_In_opt_ IShellItemArray* selection, _In_opt_ IBindCtx* pCtx) noexcept override
    {
        try
        {
            auto gwPath = GetModuleDir(hDll);
            gwPath += L"\\grepWin.exe";

            if (selection)
            {
                DWORD fileCount = 0;
                RETURN_IF_FAILED(selection->GetCount(&fileCount));
                std::wstring path;
                for (DWORD i = 0; i < fileCount; i++)
                {
                    IShellItem* shellItem = nullptr;
                    selection->GetItemAt(i, &shellItem);
                    LPWSTR itemName = nullptr;
                    shellItem->GetDisplayName(SIGDN_FILESYSPATH, &itemName);
                    if (itemName)
                    {
                        if (path.empty())
                            path = itemName;
                        else
                        {
                            path += L"|";
                            path += itemName;
                        }
                        CoTaskMemFree(itemName);
                    }
                }

                path                        = L"/searchpath:\"" + path + L"\"";

                                        // try to launch the exe with the explorer instance:
                // this avoids that the exe is started with the identity of this dll,
                // starting it as if it was started the normal way.
                bool                     execSucceeded = false;
                ComPtr<IServiceProvider> serviceProvider;
                if (SUCCEEDED(m_site.As(&serviceProvider)))
                {
                    ComPtr<IShellBrowser> shellBrowser;
                    if (SUCCEEDED(serviceProvider->QueryService(SID_SShellBrowser, IID_IShellBrowser, &shellBrowser)))
                    {
                        ComPtr<IShellView> shellView;
                        if (SUCCEEDED(shellBrowser->QueryActiveShellView(&shellView)))
                        {
                            ComPtr<IDispatch> spdispView;
                            if (SUCCEEDED(shellView->GetItemObject(SVGIO_BACKGROUND, IID_PPV_ARGS(&spdispView))))
                            {
                                ComPtr<IShellFolderViewDual> spFolderView;
                                if (SUCCEEDED(spdispView.As(&spFolderView)))
                                {
                                    ComPtr<IDispatch> spdispShell;
                                    if (SUCCEEDED(spFolderView->get_Application(&spdispShell)))
                                    {
                                        ComPtr<IShellDispatch2> spdispShell2;
                                        if (SUCCEEDED(spdispShell.As(&spdispShell2)))
                                        {
                                            // without this, the launched app is not moved to the foreground
                                            AllowSetForegroundWindow(ASFW_ANY);

                                            if (SUCCEEDED(spdispShell2->ShellExecute(_bstr_t{gwPath.c_str()},
                                                                                     _variant_t{path.c_str()},
                                                                                     _variant_t{L""},
                                                                                     _variant_t{L"open"},
                                                                                     _variant_t{SW_NORMAL})))
                                            {
                                                execSucceeded = true;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                if (!execSucceeded)
                {
                    // just in case the shell execute with explorer failed
                    SHELLEXECUTEINFO shExecInfo = {sizeof(SHELLEXECUTEINFO)};

                    shExecInfo.hwnd             = nullptr;
                    shExecInfo.lpVerb           = L"open";
                    shExecInfo.lpFile           = gwPath.c_str();
                    shExecInfo.lpParameters     = path.c_str();
                    shExecInfo.nShow            = SW_NORMAL;
                    ShellExecuteEx(&shExecInfo);
                }
            }

            return S_OK;
        }
        CATCH_RETURN();
    }
};

CoCreatableClass(GrepWinExplorerCommandHandler);

CoCreatableClassWrlCreatorMapInclude(GrepWinExplorerCommandHandler);

STDAPI DllGetActivationFactory(_In_ HSTRING activatableClassId, _COM_Outptr_ IActivationFactory** factory)
{
    return Module<ModuleType::InProc>::GetModule().GetActivationFactory(activatableClassId, factory);
}

STDAPI DllCanUnloadNow()
{
    return Module<InProc>::GetModule().GetObjectCount() == 0 ? S_OK : S_FALSE;
}

STDAPI DllGetClassObject(_In_ REFCLSID rclsid, _In_ REFIID riid, _COM_Outptr_ void** instance)
{
    return Module<InProc>::GetModule().GetClassObject(rclsid, riid, instance);
}
