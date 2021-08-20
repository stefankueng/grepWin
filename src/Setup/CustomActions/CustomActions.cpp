// grepWin - regex search and replace for Windows

// Copyright (C) 2021 - Stefan Kueng

// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//

/* BIG FAT WARNING: Do not use any functions which require the C-Runtime library
   in this custom action dll! The runtimes might not be installed yet!
*/

#include <windows.h>
#include <msi.h>
#include <msiquery.h>
#include <shlwapi.h>

#include <winrt/Windows.Management.Deployment.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.ApplicationModel.h>

#pragma comment(lib, "windowsapp.lib")

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Management::Deployment;

BOOL APIENTRY DllMain(HANDLE /*hModule*/,
                      DWORD /*ul_reason_for_call*/,
                      LPVOID /*lpReserved*/
)
{
    return TRUE;
}

UINT __stdcall MsgBox(MSIHANDLE /*hModule*/)
{
    MessageBox(nullptr, L"CustomAction \"MsgBox\" running", L"Installer", MB_ICONINFORMATION);
    return ERROR_SUCCESS;
}

UINT __stdcall RegisterSparsePackage(MSIHANDLE hModule)
{
    DWORD len = 0;
    MsiGetPropertyW(hModule, L"APPLICATIONFOLDER", L"", &len);
    auto sparseExtPath = std::make_unique<wchar_t[]>(len + 1LL);
    len += 1;
    MsiGetPropertyW(hModule, L"APPLICATIONFOLDER", sparseExtPath.get(), &len);

    len = 0;
    MsiGetPropertyW(hModule, L"SPARSEPACKAGEFILE", L"", &len);
    auto sparsePackageFile = std::make_unique<wchar_t[]>(len + 1LL);
    len += 1;
    MsiGetPropertyW(hModule, L"SPARSEPACKAGEFILE", sparsePackageFile.get(), &len);

    std::wstring sSparsePackagePath = sparseExtPath.get();
    sSparsePackagePath += L"\\";
    sSparsePackagePath += sparsePackageFile.get();

    PackageManager    manager;
    AddPackageOptions options;
    Uri               externalUri(sparseExtPath.get());
    Uri               packageUri(sSparsePackagePath.c_str());
    options.ExternalLocationUri(externalUri);
    auto deploymentOperation = manager.AddPackageByUriAsync(packageUri, options);
    auto deployResult = deploymentOperation.get();

    if (!SUCCEEDED(deployResult.ExtendedErrorCode()))
    {
        // Deployment failed
        std::wstring text = sparseExtPath.get();
        text += L"\r\n";
        text += sparsePackageFile.get();
        text += L"\r\n";
        text += deployResult.ErrorText();
        return deployResult.ExtendedErrorCode();
    }
    std::wstring text = sparseExtPath.get();
    text += L"\r\n";
    text += sparsePackageFile.get();
    return ERROR_SUCCESS;
}

UINT __stdcall UnregisterSparsePackage(MSIHANDLE hModule)
{
    DWORD len = 0;
    MsiGetPropertyW(hModule, L"SPARSEPACKAGENAME", L"", &len);
    auto sparsePackageName = std::make_unique<wchar_t[]>(len + 1LL);
    len += 1;
    MsiGetPropertyW(hModule, L"SPARSEPACKAGENAME", sparsePackageName.get(), &len);

    PackageManager packageManager;

    auto           packages = packageManager.FindPackages();
    winrt::hstring fullName = sparsePackageName.get();
    std::wstring   s1, s2, s3;
    for (const auto& package : packages)
    {
        s1 += L"\r\n";
        s1 += package.Id().Publisher();
        s2 += L"\r\n";
        s2 += package.Id().PublisherId();
        s3 += L"\r\n";
        s3 += package.Id().Name();
        if (package.Id().Name() == sparsePackageName.get())
            fullName = package.Id().FullName();
    }

    auto deploymentOperation = packageManager.RemovePackageAsync(fullName, RemovalOptions::None);
    auto deployResult        = deploymentOperation.get();
    if (!SUCCEEDED(deployResult.ExtendedErrorCode()))
    {
        // Deployment failed
        std::wstring text = L"unregistered failed\r\n";
        text += deployResult.ErrorText();
        return deployResult.ExtendedErrorCode();
    }

    return ERROR_SUCCESS;
}