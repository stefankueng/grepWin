#pragma once
#include <vector>
#include <ole2.h>
#include <ShellApi.h>
#include <ShlObj.h>

class CIDropTarget : public IDropTarget
{
	DWORD m_cRefCount;
	bool m_bAllowDrop;
	struct IDropTargetHelper *m_pDropTargetHelper;
	std::vector<FORMATETC> m_formatetc;
	FORMATETC* m_pSupportedFrmt;
protected:
	HWND m_hTargetWnd;
public:
	
	CIDropTarget(HWND m_hTargetWnd);
	virtual ~CIDropTarget();
	void AddSuportedFormat(FORMATETC& ftetc) { m_formatetc.push_back(ftetc); }
	
	//return values: true - release the medium. false - don't release the medium 
	virtual bool OnDrop(FORMATETC* pFmtEtc, STGMEDIUM& medium,DWORD *pdwEffect) = 0;

	virtual HRESULT STDMETHODCALLTYPE QueryInterface( 
		/* [in] */ REFIID riid,
		/* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject);
	virtual ULONG STDMETHODCALLTYPE AddRef( void) { return ++m_cRefCount; }
	virtual ULONG STDMETHODCALLTYPE Release( void);

    bool QueryDrop(DWORD grfKeyState, LPDWORD pdwEffect);
	virtual HRESULT STDMETHODCALLTYPE DragEnter(
        /* [unique][in] */ IDataObject __RPC_FAR *pDataObj,
        /* [in] */ DWORD grfKeyState,
        /* [in] */ POINTL pt,
        /* [out][in] */ DWORD __RPC_FAR *pdwEffect);
    virtual HRESULT STDMETHODCALLTYPE DragOver( 
        /* [in] */ DWORD grfKeyState,
        /* [in] */ POINTL pt,
        /* [out][in] */ DWORD __RPC_FAR *pdwEffect);
    virtual HRESULT STDMETHODCALLTYPE DragLeave( void);    
    virtual HRESULT STDMETHODCALLTYPE Drop(
        /* [unique][in] */ IDataObject __RPC_FAR *pDataObj,
        /* [in] */ DWORD grfKeyState,
        /* [in] */ POINTL pt,
        /* [out][in] */ DWORD __RPC_FAR *pdwEffect);
};


class CFileDropTarget : public CIDropTarget
{
public:
	CFileDropTarget(HWND hTargetWnd):CIDropTarget(hTargetWnd){}
	virtual bool OnDrop(FORMATETC* pFmtEtc, STGMEDIUM& medium, DWORD * /*pdwEffect*/)
	{
		if(pFmtEtc->cfFormat == CF_TEXT && medium.tymed == TYMED_ISTREAM)
		{
			if(medium.pstm != NULL)
			{
				const int BUF_SIZE = 10000;
				TCHAR buff[BUF_SIZE+1];
				ULONG cbRead=0;
				HRESULT hr = medium.pstm->Read(buff, BUF_SIZE, &cbRead);
				if( SUCCEEDED(hr) && cbRead > 0 && cbRead < BUF_SIZE)
				{
					buff[cbRead]=0;
					LRESULT nLen = ::SendMessage(m_hTargetWnd, WM_GETTEXTLENGTH, 0, 0);
					::SendMessage(m_hTargetWnd, EM_SETSEL, nLen, -1);
					::SendMessage(m_hTargetWnd, EM_REPLACESEL, TRUE, (LPARAM)buff);
				}
				else
					for(;(hr==S_OK && cbRead >0) && SUCCEEDED(hr) ;)
					{
						buff[cbRead]=0;
						LRESULT nLen = ::SendMessage(m_hTargetWnd, WM_GETTEXTLENGTH, 0, 0);
						::SendMessage(m_hTargetWnd, EM_SETSEL, nLen, -1);
						::SendMessage(m_hTargetWnd, EM_REPLACESEL, TRUE, (LPARAM)buff);
						cbRead=0;
						hr = medium.pstm->Read(buff, BUF_SIZE, &cbRead);
					}
			}
		}
		if(pFmtEtc->cfFormat == CF_TEXT && medium.tymed == TYMED_HGLOBAL)
		{
			TCHAR* pStr = (TCHAR*)GlobalLock(medium.hGlobal);
			if(pStr != NULL)
			{
				LRESULT nLen = ::SendMessage(m_hTargetWnd, WM_GETTEXTLENGTH, 0, 0);
				::SendMessage(m_hTargetWnd, EM_SETSEL, nLen, -1);
				::SendMessage(m_hTargetWnd, EM_REPLACESEL, TRUE, (LPARAM)pStr);
			}
			GlobalUnlock(medium.hGlobal);
		}
		if(pFmtEtc->cfFormat == CF_HDROP && medium.tymed == TYMED_HGLOBAL)
		{
			HDROP hDrop = (HDROP)GlobalLock(medium.hGlobal);
			if(hDrop != NULL)
			{
				TCHAR szFileName[MAX_PATH];

				UINT cFiles = DragQueryFile(hDrop, 0xFFFFFFFF, NULL, 0); 
				for(UINT i = 0; i < cFiles; ++i)
				{
					DragQueryFile(hDrop, i, szFileName, sizeof(szFileName)); 
					::SendMessage(m_hTargetWnd, WM_SETTEXT, 0, (LPARAM)szFileName);
				}  
				//DragFinish(hDrop); // base class calls ReleaseStgMedium
			}
			GlobalUnlock(medium.hGlobal);
		}
		return true; //let base free the medium
	}

};
