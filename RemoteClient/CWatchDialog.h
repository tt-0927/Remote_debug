#pragma once
#ifndef WM_SEND_PACK_ACK
#define WM_SEND_PACK_ACK (WM_USER+2)
#endif

#define WATCH_TIMER_ID 1001  // ✅ 定时器ID

class CWatchDialog : public CDialog
{
	DECLARE_DYNAMIC(CWatchDialog)

public:
	CWatchDialog(CWnd* pParent = nullptr);
	virtual ~CWatchDialog();

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DLG_WATCH };
#endif

public:
	// ✅ 公共成员变量
	int m_nObjWidth;
	int m_nObjHeight;
	CImage m_image;
	CStatic m_picture;
	bool isFull;         // ✅ 移到 public,供外部访问
	
protected:
	// ✅ 保护成员变量
	bool m_isFull;
	UINT_PTR m_nTimerID;
	
	virtual void DoDataExchange(CDataExchange* pDX);
	DECLARE_MESSAGE_MAP()
	
public:
	// ✅ 公共成员函数
	CImage& GetImage() {
		return m_image;
	}
	
	void SetImageStatus(bool isFull = false) {
		m_isFull = isFull;
	}
	
	bool IsFull() const {  // ✅ getter 函数
		return m_isFull;
	}
	
	CPoint UserPoint2RemoteScreenPoint(CPoint& point, bool isScreen = false);
	
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	
	// ✅ 消息处理函数
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg LRESULT OnSendPackAck(WPARAM wParam, LPARAM lParam);
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnStnClickedWatch();
	afx_msg void OnBnClickedBtnLock();
	afx_msg void OnBnClickedBtnUnlock();
	afx_msg void OnBnClickedBtnWatch();
};
