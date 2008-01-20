#include "stdafx.h"
#include "DropFiles.h"
#include "UnicodeUtils.h"


CLIPFORMAT CF_FILECONTENTS = (CLIPFORMAT)RegisterClipboardFormat(CFSTR_FILECONTENTS);
CLIPFORMAT CF_FILEDESCRIPTOR = (CLIPFORMAT)RegisterClipboardFormat(CFSTR_FILEDESCRIPTOR);
CLIPFORMAT CF_PREFERREDDROPEFFECT = (CLIPFORMAT)RegisterClipboardFormat(CFSTR_PREFERREDDROPEFFECT);

FileDataObject::FileDataObject(const vector<wstring>& paths) : m_bInOperation(FALSE)
	, m_bIsAsync(TRUE)
	, m_cRefCount(0)
{
	m_allPaths = paths;
}

FileDataObject::~FileDataObject()
{
	for (size_t i = 0; i < m_vecStgMedium.size(); ++i)
	{
		ReleaseStgMedium(m_vecStgMedium[i]);
		delete m_vecStgMedium[i];
	}

	for (size_t j = 0; j < m_vecFormatEtc.size(); ++j)
		delete m_vecFormatEtc[j];
}

//////////////////////////////////////////////////////////////////////////
// IUnknown
//////////////////////////////////////////////////////////////////////////

STDMETHODIMP FileDataObject::QueryInterface(REFIID riid, void** ppvObject)
{
	*ppvObject = NULL;
	if (IID_IUnknown==riid || IID_IDataObject==riid)
		*ppvObject=this;
	if (riid == IID_IAsyncOperation)
		*ppvObject = (IAsyncOperation*)this;

	if (NULL!=*ppvObject)
	{
		((LPUNKNOWN)*ppvObject)->AddRef();
		return S_OK;
	}
	return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) FileDataObject::AddRef(void)
{
	return ++m_cRefCount;
}

STDMETHODIMP_(ULONG) FileDataObject::Release(void)
{
	--m_cRefCount;
	if (m_cRefCount == 0)
	{
		delete this;
		return 0;
	}
	return m_cRefCount;
}

//////////////////////////////////////////////////////////////////////////
// IDataObject
//////////////////////////////////////////////////////////////////////////
STDMETHODIMP FileDataObject::GetData(FORMATETC* pformatetcIn, STGMEDIUM* pmedium)
{
	if (pformatetcIn == NULL || pmedium == NULL)
		return E_INVALIDARG;
	pmedium->hGlobal = NULL;

	if ((pformatetcIn->tymed & TYMED_ISTREAM) && (pformatetcIn->dwAspect == DVASPECT_CONTENT) && (pformatetcIn->cfFormat == CF_FILECONTENTS))
	{
		// supports the IStream format.
		// The lindex param is the index of the file to return
		if (m_allPaths.size() && (pformatetcIn->lindex < (LONG)m_allPaths.size()))
		{
			IStream * pIStream = NULL;
			HRESULT res = SHCreateStreamOnFile(m_allPaths[pformatetcIn->lindex].c_str(), STGM_READ, &pIStream);
			if (res == S_OK)
			{
				pmedium->pstm = pIStream;
				pmedium->tymed = TYMED_ISTREAM;
				return S_OK;
			}
			return res;
		}
		return E_INVALIDARG;
	}
	else if ((pformatetcIn->tymed & TYMED_HGLOBAL) && (pformatetcIn->dwAspect == DVASPECT_CONTENT) && (pformatetcIn->cfFormat == CF_FILEDESCRIPTOR))
	{
		size_t dataSize = sizeof(FILEGROUPDESCRIPTOR) + ((m_allPaths.size() - 1) * sizeof(FILEDESCRIPTOR));
		HGLOBAL data = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE | GMEM_ZEROINIT, dataSize);

		FILEGROUPDESCRIPTOR* files = (FILEGROUPDESCRIPTOR*)GlobalLock(data);
		files->cItems = m_allPaths.size();
		int i = 0;
		for (vector<wstring>::const_iterator it = m_allPaths.begin(); it != m_allPaths.end(); ++it)
		{
			wstring name = it->substr(it->find_last_of('\\')+1);
			_tcscpy_s(files->fgd[i].cFileName, MAX_PATH, name.c_str());
			files->fgd[i].dwFlags = FD_ATTRIBUTES | FD_PROGRESSUI | FD_FILESIZE | FD_LINKUI;
			files->fgd[i].dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
			HANDLE hFile = CreateFile(it->c_str(), FILE_READ_EA, FILE_SHARE_DELETE|FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			if (hFile != INVALID_HANDLE_VALUE)
			{
				LARGE_INTEGER fs;
				GetFileSizeEx(hFile, &fs); 
				files->fgd[i].nFileSizeLow = fs.LowPart;
				files->fgd[i].nFileSizeHigh = fs.HighPart;
				CloseHandle(hFile);
			}
			else
			{
				files->fgd[i].nFileSizeLow = 0;
				files->fgd[i].nFileSizeHigh = 0;
			}
			++i;
		}

		GlobalUnlock(data);

		pmedium->hGlobal = data;
		pmedium->tymed = TYMED_HGLOBAL;
		return S_OK;
	}
	// handling CF_PREFERREDDROPEFFECT is necessary to tell the shell that it should *not* ask for the
	// CF_FILEDESCRIPTOR until the drop actually occurs. If we don't handle CF_PREFERREDDROPEFFECT, the shell
	// will ask for the file descriptor for every object (file/folder) the mouse pointer hovers over and is
	// a potential drop target.
	else if ((pformatetcIn->tymed & TYMED_HGLOBAL) && (pformatetcIn->cfFormat == CF_PREFERREDDROPEFFECT))
	{
		HGLOBAL data = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE | GMEM_ZEROINIT, sizeof(DWORD));
		DWORD* effect = (DWORD*) GlobalLock(data);
		(*effect) = DROPEFFECT_COPY;
		GlobalUnlock(data);
		pmedium->hGlobal = data;
		pmedium->tymed = TYMED_HGLOBAL;
		return S_OK;
	}
	else if ((pformatetcIn->tymed & TYMED_HGLOBAL) && (pformatetcIn->dwAspect == DVASPECT_CONTENT) && (pformatetcIn->cfFormat == CF_TEXT))
	{
		// caller wants text
		// create the string from the pathlist
		wstring text;
		if (m_allPaths.size())
		{
			// create a single string where the URLs are separated by newlines
			for (size_t i=0; i<m_allPaths.size(); ++i)
			{
				text += m_allPaths[i].c_str();
				text += _T("\r\n");
			}
		}
		string texta = CUnicodeUtils::StdGetUTF8(text);
		pmedium->tymed = TYMED_HGLOBAL;
		pmedium->hGlobal = GlobalAlloc(GHND, (texta.size()+1)*sizeof(char));
		if (pmedium->hGlobal)
		{
			char* pMem = (char*)GlobalLock(pmedium->hGlobal);
			strcpy_s(pMem, texta.size()+1, texta.c_str());
			GlobalUnlock(pmedium->hGlobal);
		}
		pmedium->pUnkForRelease = NULL;
		return S_OK;
	}
	else if ((pformatetcIn->tymed & TYMED_HGLOBAL) && (pformatetcIn->dwAspect == DVASPECT_CONTENT) && (pformatetcIn->cfFormat == CF_UNICODETEXT))
	{
		// caller wants Unicode text
		// create the string from the pathlist
		wstring text;
		if (m_allPaths.size())
		{
			// create a single string where the URLs are separated by newlines
			for (size_t i=0; i<m_allPaths.size(); ++i)
			{
				text += m_allPaths[i].c_str();
				text += _T("\r\n");
			}
		}
		pmedium->tymed = TYMED_HGLOBAL;
		pmedium->hGlobal = GlobalAlloc(GHND, (text.size()+1)*sizeof(TCHAR));
		if (pmedium->hGlobal)
		{
			TCHAR* pMem = (TCHAR*)GlobalLock(pmedium->hGlobal);
			_tcscpy_s(pMem, text.size()+1, text.c_str());
			GlobalUnlock(pmedium->hGlobal);
		}
		pmedium->pUnkForRelease = NULL;
		return S_OK;
	}

	for (size_t i=0; i<m_vecFormatEtc.size(); ++i)
	{
		if ((pformatetcIn->tymed == m_vecFormatEtc[i]->tymed) &&
			(pformatetcIn->dwAspect == m_vecFormatEtc[i]->dwAspect) &&
			(pformatetcIn->cfFormat == m_vecFormatEtc[i]->cfFormat))
		{
			CopyMedium(pmedium, m_vecStgMedium[i], m_vecFormatEtc[i]);
			return S_OK;
		}
	}

	return DV_E_FORMATETC;
}

STDMETHODIMP FileDataObject::GetDataHere(FORMATETC* /*pformatetc*/, STGMEDIUM* /*pmedium*/)
{
	return E_NOTIMPL;
}

STDMETHODIMP FileDataObject::QueryGetData(FORMATETC* pformatetc)
{ 
	if (pformatetc == NULL)
		return E_INVALIDARG;

	if (!(DVASPECT_CONTENT & pformatetc->dwAspect))
		return DV_E_DVASPECT;

	if ((pformatetc->tymed & TYMED_ISTREAM) &&
		(pformatetc->dwAspect == DVASPECT_CONTENT) &&
		(pformatetc->cfFormat == CF_FILECONTENTS))
	{
		return S_OK;
	}
	if ((pformatetc->tymed & TYMED_HGLOBAL) &&
		(pformatetc->dwAspect == DVASPECT_CONTENT) &&
		((pformatetc->cfFormat == CF_TEXT)||(pformatetc->cfFormat == CF_UNICODETEXT)||(pformatetc->cfFormat == CF_FILEDESCRIPTOR)||(pformatetc->cfFormat == CF_PREFERREDDROPEFFECT)))
	{
		return S_OK;
	}

	for (size_t i=0; i<m_vecFormatEtc.size(); ++i)
	{
		if ((pformatetc->tymed == m_vecFormatEtc[i]->tymed) &&
			(pformatetc->dwAspect == m_vecFormatEtc[i]->dwAspect) &&
			(pformatetc->cfFormat == m_vecFormatEtc[i]->cfFormat))
			return S_OK;
	}

	return DV_E_TYMED;
}

STDMETHODIMP FileDataObject::GetCanonicalFormatEtc(FORMATETC* /*pformatectIn*/, FORMATETC* pformatetcOut)
{ 
	if (pformatetcOut == NULL)
		return E_INVALIDARG;
	return DATA_S_SAMEFORMATETC;
}

STDMETHODIMP FileDataObject::SetData(FORMATETC* pformatetc, STGMEDIUM* pmedium, BOOL fRelease)
{ 
	if ((pformatetc == NULL) || (pmedium == NULL))
		return E_INVALIDARG;

	FORMATETC* fetc = new FORMATETC;
	STGMEDIUM* pStgMed = new STGMEDIUM;

	if ((fetc == NULL) || (pStgMed == NULL))
		return E_OUTOFMEMORY;

	ZeroMemory(fetc,sizeof(FORMATETC));
	ZeroMemory(pStgMed,sizeof(STGMEDIUM));

	*fetc = *pformatetc;
	m_vecFormatEtc.push_back(fetc);

	if (fRelease)
		*pStgMed = *pmedium;
	else
	{
		CopyMedium(pStgMed, pmedium, pformatetc);
	}
	m_vecStgMedium.push_back(pStgMed);

	return S_OK;
}

STDMETHODIMP FileDataObject::EnumFormatEtc(DWORD dwDirection, IEnumFORMATETC** ppenumFormatEtc)
{ 
	if (ppenumFormatEtc == NULL)
		return E_POINTER;

	*ppenumFormatEtc = NULL;
	switch (dwDirection)
	{
	case DATADIR_GET:
		*ppenumFormatEtc= new CSVNEnumFormatEtc(m_vecFormatEtc);
		if (*ppenumFormatEtc == NULL)
			return E_OUTOFMEMORY;
		(*ppenumFormatEtc)->AddRef(); 
		break;
	default:
		return E_NOTIMPL;
	}
	return S_OK;
}

STDMETHODIMP FileDataObject::DAdvise(FORMATETC* /*pformatetc*/, DWORD /*advf*/, IAdviseSink* /*pAdvSink*/, DWORD* /*pdwConnection*/)
{ 
	return OLE_E_ADVISENOTSUPPORTED;
}

STDMETHODIMP FileDataObject::DUnadvise(DWORD /*dwConnection*/)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE FileDataObject::EnumDAdvise(IEnumSTATDATA** /*ppenumAdvise*/)
{
	return OLE_E_ADVISENOTSUPPORTED;
}

void FileDataObject::CopyMedium(STGMEDIUM* pMedDest, STGMEDIUM* pMedSrc, FORMATETC* pFmtSrc)
{
	switch(pMedSrc->tymed)
	{
	case TYMED_HGLOBAL:
		pMedDest->hGlobal = (HGLOBAL)OleDuplicateData(pMedSrc->hGlobal,pFmtSrc->cfFormat, NULL);
		break;
	case TYMED_GDI:
		pMedDest->hBitmap = (HBITMAP)OleDuplicateData(pMedSrc->hBitmap,pFmtSrc->cfFormat, NULL);
		break;
	case TYMED_MFPICT:
		pMedDest->hMetaFilePict = (HMETAFILEPICT)OleDuplicateData(pMedSrc->hMetaFilePict,pFmtSrc->cfFormat, NULL);
		break;
	case TYMED_ENHMF:
		pMedDest->hEnhMetaFile = (HENHMETAFILE)OleDuplicateData(pMedSrc->hEnhMetaFile,pFmtSrc->cfFormat, NULL);
		break;
	case TYMED_FILE:
		pMedSrc->lpszFileName = (LPOLESTR)OleDuplicateData(pMedSrc->lpszFileName,pFmtSrc->cfFormat, NULL);
		break;
	case TYMED_ISTREAM:
		pMedDest->pstm = pMedSrc->pstm;
		pMedSrc->pstm->AddRef();
		break;
	case TYMED_ISTORAGE:
		pMedDest->pstg = pMedSrc->pstg;
		pMedSrc->pstg->AddRef();
		break;
	case TYMED_NULL:
	default:
		break;
	}
	pMedDest->tymed = pMedSrc->tymed;
	pMedDest->pUnkForRelease = NULL;
	if(pMedSrc->pUnkForRelease != NULL)
	{
		pMedDest->pUnkForRelease = pMedSrc->pUnkForRelease;
		pMedSrc->pUnkForRelease->AddRef();
	}
}


//////////////////////////////////////////////////////////////////////////
// IAsyncOperation
//////////////////////////////////////////////////////////////////////////
HRESULT STDMETHODCALLTYPE FileDataObject::SetAsyncMode(BOOL fDoOpAsync)
{
	m_bIsAsync = fDoOpAsync;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE FileDataObject::GetAsyncMode(BOOL* pfIsOpAsync)
{
	if (!pfIsOpAsync)
		return E_FAIL;

	*pfIsOpAsync = m_bIsAsync;

	return S_OK;
}

HRESULT STDMETHODCALLTYPE FileDataObject::StartOperation(IBindCtx* /*pbcReserved*/)
{
	m_bInOperation = TRUE;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE FileDataObject::InOperation(BOOL* pfInAsyncOp)
{
	if (!pfInAsyncOp)
		return E_FAIL;

	*pfInAsyncOp = m_bInOperation;

	return S_OK;
}

HRESULT STDMETHODCALLTYPE FileDataObject::EndOperation(HRESULT /*hResult*/, IBindCtx* /*pbcReserved*/, DWORD /*dwEffects*/)
{
	m_bInOperation = FALSE;
	Release();
	return S_OK;
}



void CSVNEnumFormatEtc::Init()
{
	m_formats[0].cfFormat = CF_UNICODETEXT;
	m_formats[0].dwAspect = DVASPECT_CONTENT;
	m_formats[0].lindex = -1;
	m_formats[0].ptd = NULL;
	m_formats[0].tymed = TYMED_HGLOBAL;

	m_formats[1].cfFormat = CF_TEXT;
	m_formats[1].dwAspect = DVASPECT_CONTENT;
	m_formats[1].lindex = -1;
	m_formats[1].ptd = NULL;
	m_formats[1].tymed = TYMED_HGLOBAL;

	m_formats[2].cfFormat = CF_FILECONTENTS;
	m_formats[2].dwAspect = DVASPECT_CONTENT;
	m_formats[2].lindex = -1;
	m_formats[2].ptd = NULL;
	m_formats[2].tymed = TYMED_ISTREAM;

	m_formats[3].cfFormat = CF_FILEDESCRIPTOR;
	m_formats[3].dwAspect = DVASPECT_CONTENT;
	m_formats[3].lindex = -1;
	m_formats[3].ptd = NULL;
	m_formats[3].tymed = TYMED_HGLOBAL;

	m_formats[4].cfFormat = CF_PREFERREDDROPEFFECT;
	m_formats[4].dwAspect = DVASPECT_CONTENT;
	m_formats[4].lindex = -1;
	m_formats[4].ptd = NULL;
	m_formats[4].tymed = TYMED_HGLOBAL;
}

CSVNEnumFormatEtc::CSVNEnumFormatEtc(const vector<FORMATETC>& vec) : m_cRefCount(0)
	, m_iCur(0)
{
	for (size_t i = 0; i < vec.size(); ++i)
		m_vecFormatEtc.push_back(vec[i]);
	Init();
}

CSVNEnumFormatEtc::CSVNEnumFormatEtc(const vector<FORMATETC*>& vec) : m_cRefCount(0)
	, m_iCur(0)
{
	for (size_t i = 0; i < vec.size(); ++i)
		m_vecFormatEtc.push_back(*vec[i]);
	Init();
}

STDMETHODIMP  CSVNEnumFormatEtc::QueryInterface(REFIID refiid, void** ppv)
{
	*ppv = NULL;
	if (IID_IUnknown==refiid || IID_IEnumFORMATETC==refiid)
		*ppv=this;

	if (*ppv != NULL)
	{
		((LPUNKNOWN)*ppv)->AddRef();
		return S_OK;
	}
	return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) CSVNEnumFormatEtc::AddRef(void)
{
	return ++m_cRefCount;
}

STDMETHODIMP_(ULONG) CSVNEnumFormatEtc::Release(void)
{
	--m_cRefCount;
	if (m_cRefCount == 0)
	{
		delete this;
		return 0;
	}
	return m_cRefCount; 
}

STDMETHODIMP CSVNEnumFormatEtc::Next(ULONG celt, LPFORMATETC lpFormatEtc, ULONG* pceltFetched)
{
	if (pceltFetched != NULL)
		*pceltFetched = 0;

	ULONG cReturn = celt;

	if (celt <= 0 || lpFormatEtc == NULL || m_iCur >= 5)
		return S_FALSE;

	if (pceltFetched == NULL && celt != 1) // pceltFetched can be NULL only for 1 item request
		return S_FALSE;

	while (m_iCur < (5 + m_vecFormatEtc.size()) && cReturn > 0)
	{
		if (m_iCur < 5)
			*lpFormatEtc++ = m_formats[m_iCur++];
		else
			*lpFormatEtc++ = m_vecFormatEtc[m_iCur++ - 5];
		--cReturn;
	}

	if (pceltFetched != NULL)
		*pceltFetched = celt - cReturn;

	return (cReturn == 0) ? S_OK : S_FALSE;
}

STDMETHODIMP CSVNEnumFormatEtc::Skip(ULONG celt)
{
	if ((m_iCur + int(celt)) >= (5 + m_vecFormatEtc.size()))
		return S_FALSE;
	m_iCur += celt;
	return S_OK;
}

STDMETHODIMP CSVNEnumFormatEtc::Reset(void)
{
	m_iCur = 0;
	return S_OK;
}

STDMETHODIMP CSVNEnumFormatEtc::Clone(IEnumFORMATETC** ppCloneEnumFormatEtc)
{
	if (ppCloneEnumFormatEtc == NULL)
		return E_POINTER;

	CSVNEnumFormatEtc *newEnum = new CSVNEnumFormatEtc(m_vecFormatEtc);
	if (newEnum == NULL)
		return E_OUTOFMEMORY;

	newEnum->AddRef();
	newEnum->m_iCur = m_iCur;
	*ppCloneEnumFormatEtc = newEnum;
	return S_OK;
}





CDropFiles::CDropFiles()
{
}

CDropFiles::~CDropFiles()
{
}

void CDropFiles::AddFile(const wstring& sFile)
{
	m_arFiles.push_back(sFile);
}

size_t CDropFiles::GetCount()
{
	return m_arFiles.size();
}

void CDropFiles::CreateStructure(HWND hWnd)
{
	CIDropSource* pdsrc = new CIDropSource;
	if (pdsrc == NULL)
		return;
	pdsrc->AddRef();

	FileDataObject* pdobj = new FileDataObject(m_arFiles);
	if (pdobj == NULL)
	{
		delete pdsrc;
		return;
	}
	pdobj->AddRef();

	CDragSourceHelper dragsrchelper;
	POINT pt;
	GetCursorPos(&pt);
	dragsrchelper.InitializeFromWindow(hWnd, pt, pdobj);
	// Initiate the Drag & Drop
	DWORD dwEffect;
	::DoDragDrop(pdobj, pdsrc, DROPEFFECT_MOVE|DROPEFFECT_COPY, &dwEffect);
	pdsrc->Release();
}

//////////////////////////////////////////////////////////////////////
// CIDropSource Class
//////////////////////////////////////////////////////////////////////

STDMETHODIMP CIDropSource::QueryInterface(/* [in] */ REFIID riid,
										  /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject)
{
	*ppvObject = NULL;
	if (IID_IUnknown==riid || IID_IDropSource==riid)
		*ppvObject=this;

	if (*ppvObject != NULL)
	{
		((LPUNKNOWN)*ppvObject)->AddRef();
		return S_OK;
	}
	return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) CIDropSource::AddRef( void)
{
	return ++m_cRefCount;
}

STDMETHODIMP_(ULONG) CIDropSource::Release( void)
{
	long nTemp;
	nTemp = --m_cRefCount;
	if(nTemp==0)
		delete this;
	return nTemp;
}

STDMETHODIMP CIDropSource::QueryContinueDrag( 
	/* [in] */ BOOL fEscapePressed,
	/* [in] */ DWORD grfKeyState)
{
	if(fEscapePressed)
		return DRAGDROP_S_CANCEL;
	if(!(grfKeyState & (MK_LBUTTON|MK_RBUTTON)))
	{
		m_bDropped = true;
		return DRAGDROP_S_DROP;
	}

	return S_OK;

}

STDMETHODIMP CIDropSource::GiveFeedback(
										/* [in] */ DWORD /*dwEffect*/)
{
	return DRAGDROP_S_USEDEFAULTCURSORS;
}