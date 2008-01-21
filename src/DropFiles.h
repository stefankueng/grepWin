#pragma once

#include <shlobj.h>
#include <string>
#include <vector>

using namespace std;

/**
 * Use this class to create the DROPFILES structure which is needed to
 * support drag and drop of file names to other applications.
 */
class CDropFiles
{
public:
	CDropFiles();
	~CDropFiles();

	/**
	 * Add a file with an absolute file name. This file will later be
	 * included the DROPFILES structure.
	 */
	void AddFile(const wstring& sFile);

	/**
	 * Returns the number of files which have been added
	 */
	size_t GetCount();

	/**
	 * Call this method when dragging begins. It will fill
	 * the DROPFILES structure with the files previously
	 * added with AddFile(...)
	 */
	void CreateStructure(HWND hWnd);

protected:
	vector<wstring> m_arFiles;
};

class CIDropSource : public IDropSource
{
	long m_cRefCount;
public:
	bool m_bDropped;

	CIDropSource::CIDropSource():m_cRefCount(0),m_bDropped(false) {}
	//IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(
		/* [in] */ REFIID riid,
		/* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject);        
	virtual ULONG STDMETHODCALLTYPE AddRef( void);
	virtual ULONG STDMETHODCALLTYPE Release( void);
	//IDropSource
	virtual HRESULT STDMETHODCALLTYPE QueryContinueDrag( 
		/* [in] */ BOOL fEscapePressed,
		/* [in] */ DWORD grfKeyState);

	virtual HRESULT STDMETHODCALLTYPE GiveFeedback( 
		/* [in] */ DWORD dwEffect);
};


extern 	CLIPFORMAT	CF_FILECONTENTS;
extern	CLIPFORMAT	CF_FILEDESCRIPTOR;
extern	CLIPFORMAT	CF_PREFERREDDROPEFFECT;

/**
 * Represents a path as an IDataObject.
 * This can be used for drag and drop operations to other applications like
 * the shell itself.
 */
class FileDataObject : public IDataObject, public IAsyncOperation
{
public:
	/**
	 * Constructs the FileDataObject.
	 * \param paths    a list of paths.
	 */
	FileDataObject(const vector<wstring>& paths);
	~FileDataObject();

	//IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);        
	virtual ULONG STDMETHODCALLTYPE AddRef(void);
	virtual ULONG STDMETHODCALLTYPE Release(void);

	//IDataObject
	virtual HRESULT STDMETHODCALLTYPE GetData(FORMATETC* pformatetcIn, STGMEDIUM* pmedium);
	virtual HRESULT STDMETHODCALLTYPE GetDataHere(FORMATETC* pformatetc, STGMEDIUM* pmedium);
	virtual HRESULT STDMETHODCALLTYPE QueryGetData(FORMATETC* pformatetc);
	virtual HRESULT STDMETHODCALLTYPE GetCanonicalFormatEtc(FORMATETC* pformatectIn, FORMATETC* pformatetcOut);
	virtual HRESULT STDMETHODCALLTYPE SetData(FORMATETC* pformatetc, STGMEDIUM* pmedium, BOOL fRelease);
	virtual HRESULT STDMETHODCALLTYPE EnumFormatEtc(DWORD dwDirection, IEnumFORMATETC** ppenumFormatEtc);
	virtual HRESULT STDMETHODCALLTYPE DAdvise(FORMATETC* pformatetc, DWORD advf, IAdviseSink* pAdvSink, DWORD* pdwConnection);
	virtual HRESULT STDMETHODCALLTYPE DUnadvise(DWORD dwConnection);
	virtual HRESULT STDMETHODCALLTYPE EnumDAdvise(IEnumSTATDATA** ppenumAdvise);

	//IAsyncOperation
	virtual HRESULT STDMETHODCALLTYPE SetAsyncMode(BOOL fDoOpAsync);	
	virtual HRESULT STDMETHODCALLTYPE GetAsyncMode(BOOL* pfIsOpAsync);
	virtual HRESULT STDMETHODCALLTYPE StartOperation(IBindCtx* pbcReserved);
	virtual HRESULT STDMETHODCALLTYPE InOperation(BOOL* pfInAsyncOp);	
	virtual HRESULT STDMETHODCALLTYPE EndOperation(HRESULT hResult, IBindCtx* pbcReserved, DWORD dwEffects);

private:
	void CopyMedium(STGMEDIUM* pMedDest, STGMEDIUM* pMedSrc, FORMATETC* pFmtSrc);

private:

private:
	vector<wstring>				m_allPaths;
	long						m_cRefCount;
	BOOL						m_bInOperation;
	BOOL						m_bIsAsync;
	vector<FORMATETC*>			m_vecFormatEtc;
	vector<STGMEDIUM*>			m_vecStgMedium;
};


/**
* Helper class for the FileDataObject class: implements the enumerator
* for the supported clipboard formats of the FileDataObject class.
*/
class CSVNEnumFormatEtc : public IEnumFORMATETC
{
public:
	CSVNEnumFormatEtc(const vector<FORMATETC*>& vec);
	CSVNEnumFormatEtc(const vector<FORMATETC>& vec);
	//IUnknown members
	STDMETHOD(QueryInterface)(REFIID, void**);
	STDMETHOD_(ULONG, AddRef)(void);
	STDMETHOD_(ULONG, Release)(void);

	//IEnumFORMATETC members
	STDMETHOD(Next)(ULONG, LPFORMATETC, ULONG*);
	STDMETHOD(Skip)(ULONG);
	STDMETHOD(Reset)(void);
	STDMETHOD(Clone)(IEnumFORMATETC**);
private:
	void						Init();
private:
	vector<FORMATETC>			m_vecFormatEtc;
	FORMATETC					m_formats[6];
	ULONG						m_cRefCount;
	size_t						m_iCur;
};

class CDragSourceHelper
{
	IDragSourceHelper* pDragSourceHelper;
public:
	CDragSourceHelper()
	{
		if(FAILED(CoCreateInstance(CLSID_DragDropHelper,
			NULL,
			CLSCTX_INPROC_SERVER,
			IID_IDragSourceHelper,
			(void**)&pDragSourceHelper)))
			pDragSourceHelper = NULL;
	}
	virtual ~CDragSourceHelper()
	{
		if( pDragSourceHelper!= NULL )
		{
			pDragSourceHelper->Release();
			pDragSourceHelper=NULL;
		}
	}

	// IDragSourceHelper
	HRESULT InitializeFromBitmap(HBITMAP hBitmap, 
		POINT& pt,	// cursor position in client coords of the window
		RECT& rc,	// selected item's bounding rect
		IDataObject* pDataObject,
		COLORREF crColorKey=GetSysColor(COLOR_WINDOW)// color of the window used for transparent effect.
		)
	{
		if(pDragSourceHelper == NULL)
			return E_FAIL;

		SHDRAGIMAGE di;
		BITMAP      bm;
		GetObject(hBitmap, sizeof(bm), &bm);
		di.sizeDragImage.cx = bm.bmWidth;
		di.sizeDragImage.cy = bm.bmHeight;
		di.hbmpDragImage = hBitmap;
		di.crColorKey = crColorKey; 
		di.ptOffset.x = pt.x - rc.left;
		di.ptOffset.y = pt.y - rc.top;
		return pDragSourceHelper->InitializeFromBitmap(&di, pDataObject);
	}
	HRESULT InitializeFromWindow(HWND hwnd, POINT& pt,IDataObject* pDataObject)
	{		
		if(pDragSourceHelper == NULL)
			return E_FAIL;
		return pDragSourceHelper->InitializeFromWindow(hwnd, &pt, pDataObject);
	}
};


