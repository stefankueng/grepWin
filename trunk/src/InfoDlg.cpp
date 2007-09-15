#include "stdafx.h"
#include "InfoDlg.h"

#include <mshtmhst.h>

#pragma comment(lib, "Urlmon.lib")

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CInfoDlg::CInfoDlg()
{

}

CInfoDlg::~CInfoDlg()
{

}

//Function which takes input of An HTML Resource Id
BOOL CInfoDlg::ShowDialog(UINT idAboutHTMLID, HINSTANCE hInstance)
{
	//Load the IE Specific MSTML Interface DKK
	HINSTANCE   hinstMSHTML = LoadLibrary(TEXT("MSHTML.DLL"));
	BOOL bSuccess = FALSE;
	if(hinstMSHTML)
	{
		SHOWHTMLDIALOGFN  *pfnShowHTMLDialog;
		//Locate The Function ShowHTMLDialog in the Loaded MSHTML.DLL 
		pfnShowHTMLDialog =     (SHOWHTMLDIALOGFN*)GetProcAddress(hinstMSHTML, "ShowHTMLDialog");
		if(pfnShowHTMLDialog)
		{
			LPTSTR lpszModule = new TCHAR[_MAX_PATH];
			//Get The Application Path
			if (GetModuleFileName(hInstance, lpszModule, _MAX_PATH))
			{
				//Add the IE Res protocol
				TCHAR strResourceURL[MAX_PATH*4];
				_stprintf_s(strResourceURL, MAX_PATH*4, _T("res://%s/%d"), lpszModule, idAboutHTMLID);
				int iLength = _tcslen(strResourceURL);
			    LPWSTR lpWideCharStr = NULL;
				lpWideCharStr =  new wchar_t[iLength+1];
				//Attempt to Create the URL Moniker to the specified in the URL String
				IMoniker *pmk;
				if(SUCCEEDED(CreateURLMoniker(NULL,strResourceURL,&pmk)))
				{
					//Invoke the ShowHTMLDialog function by pointer
					//passing the HWND of your Application , the Moniker,
					//the remaining parameters can be set to NULL
					pfnShowHTMLDialog(NULL,pmk,NULL,NULL,NULL);
					bSuccess = TRUE;
				}
				delete lpWideCharStr;
			}
			delete [] lpszModule;
		}
		FreeLibrary(hinstMSHTML);
	}
	return bSuccess;
}
