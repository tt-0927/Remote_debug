// RemoteClientDlg.h: 头文件
//

#pragma once
#include "ClientSocket.h"
#include "StatusDlg.h"
#ifndef WM_SEND_PACK_ACK
#define WM_SEND_PACK_ACK (WM_USER+2) //发送包数据应答
#endif
#include <set>
#include <map>

// CRemoteClientDlg 对话框
class CRemoteClientDlg : public CDialogEx
{
	// 构造
public:
	CRemoteClientDlg(CWnd* pParent = nullptr);	// 标准构造函数
	~CRemoteClientDlg();

	// ✅ 添加清理方法
	void ClearTreeCache();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_REMOTECLIENT_DIALOG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持
public:
	void LoadFileInfo();
	
private:
	bool m_isClosed;//监视是否关闭
	
private://TODO:代码即文档
	void DealCommand(WORD nCmd, const std::string& strData, LPARAM lParam);
	void InitUIData();
	void LoadFileCurrent();
	void Str2Tree(const std::string& drivers, CTreeCtrl& tree);
	void UpdateFileInfo(const FILEINFO& finfo, HTREEITEM hParent);
	void UpdateDownloadFile(const std::string& strData, FILE* pFile);
	CString GetPath(HTREEITEM hTree);
	void DeleteTreeChildrenItem(HTREEITEM hTree);
	
	// ✅ 添加缺失的函数声明
	int LoadFilesInfo(HTREEITEM hTreeSelected);
	CString GetTreePath(HTREEITEM hTree);
	
	// 实现
protected:
	HICON m_hIcon;
	CStatusDlg m_dlgStatus;

	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
	
public:
	afx_msg void OnBnClickedBtnTest();
	DWORD m_server_address;
	CString m_nPort;
	afx_msg void OnBnClickedBtnFileinfo();
	CTreeCtrl m_Tree;
	afx_msg void OnNMDblclkTreeDir(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnNMClickTreeDir(NMHDR* pNMHDR, LRESULT* pResult);
	// 显示文件
	CListCtrl m_List;
	afx_msg void OnNMRClickListFile(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDownloadFile();
	afx_msg void OnDeleteFile();
	afx_msg void OnRunFile();
	afx_msg void OnBnClickedBtnStartWatch();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnIpnFieldchangedIpaddressServ(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnEnChangeEditPort();
	afx_msg LRESULT OnSendPackAck(WPARAM wParam, LPARAM lParam);
	
	// ✅ 添加缺失的消息处理函数声明
	afx_msg void OnTvnItemexpandedTree(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnNMDblclkTree(NMHDR* pNMHDR, LRESULT* pResult);

private:
    // ✅ 添加：记录已加载的节点
    std::map<HTREEITEM, bool> m_mapLoadedItems;
    
    // ✅ 添加：记录正在加载的节点（防止重复请求）
    std::set<HTREEITEM> m_setLoadingItems;
    
    // ✅ 添加：节点路径缓存
    std::map<HTREEITEM, CString> m_mapItemPath;
    
    // ✅ 添加：互斥锁保护并发访问
    CRITICAL_SECTION m_csTreeAccess;

    // ✅ 添加防抖动定时器
    UINT_PTR m_nLoadTimer;
    HTREEITEM m_hPendingLoadItem;
    
    // ✅ 定时器ID
    static const UINT_PTR TIMER_LOAD_DELAY = 100;
};
