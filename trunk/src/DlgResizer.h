#pragma once
#include <vector>

using namespace std;


#define RESIZER_TOPLEFT					0
#define RESIZER_TOPRIGHT				1
#define RESIZER_TOPLEFTRIGHT			2
#define RESIZER_TOPLEFTBOTTOMRIGHT		3
#define RESIZER_BOTTOMLEFT				4
#define RESIZER_BOTTOMRIGHT				5
#define RESIZER_BOTTOMLEFTRIGHT			6

struct ResizeCtrls
{
	HWND		hWnd;
	UINT		resizeType;
	RECT		origSize;
};

class CDlgResizer
{
public:
	CDlgResizer(void);
	~CDlgResizer(void);

	void	Init(HWND hWndDlg);
	void	AddControl(HWND hWndDlg, UINT ctrlId, UINT resizeType);

	void	DoResize(int width, int height);

	RECT *	GetDlgRect() {return &m_dlgRect;}
private:
	vector<ResizeCtrls>		m_controls;
	RECT					m_dlgRect;
};
