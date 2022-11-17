
// PlayerDlg.h : header file
//

#pragma once
#include "player_audio.h"

// CPlayerDlg dialog
class CPlayerDlg : public CDialogEx
{
// Construction
public:
	CPlayerDlg(CWnd* pParent = nullptr);	// standard constructor

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_PLAYER_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support


// Implementation
protected:
	HICON m_hIcon;

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	int m_iWid;
	int m_iHei;
	CStringA m_strFileName;
	HBITMAP m_hbitmap;
	afx_msg void OnBnClickedOk();
	afx_msg void OnBnClickedAudio();
	afx_msg void OnBnClickedEncodeVideo();
	int MainFun();
	int ShowInDlg(AVFrame* pFrameRGB, int width, int height, int index, int bpp);
	int ShowWithSDL();
	int Play_Audio_Start();
	
};

static  Uint8* audio_chunk;  //音频块
static  Uint32  audio_len;    //音频剩下的长度
static  Uint8* audio_pos;    //音频当前的位置
