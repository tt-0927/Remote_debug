// RemoteClientDlg.cpp: 实现文件
//

#include "pch.h"
#include "framework.h"
#include "RemoteClient.h"
#include "RemoteClientDlg.h"
#include "afxdialogex.h"
#include "ClientController.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif
#include "CWatchDialog.h"


// 用于应用程序“关于”菜单项的 CAboutDlg 对话框

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

	// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

// 实现
protected:
	DECLARE_MESSAGE_MAP()
public:

};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CRemoteClientDlg 对话框



CRemoteClientDlg::CRemoteClientDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_REMOTECLIENT_DIALOG, pParent)
	, m_server_address(0)
	, m_nPort(_T(""))
	, m_nLoadTimer(0)
	, m_hPendingLoadItem(NULL)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	// ✅ 初始化临界区
	InitializeCriticalSection(&m_csTreeAccess);
}

CRemoteClientDlg::~CRemoteClientDlg()
{
	// ✅ 清理临界区
	DeleteCriticalSection(&m_csTreeAccess);
}

void CRemoteClientDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_IPAddress(pDX, IDC_IPADDRESS_SERV, m_server_address);
	DDX_Text(pDX, IDC_EDIT_PORT, m_nPort);
	DDX_Control(pDX, IDC_TREE_DIR, m_Tree);
	DDX_Control(pDX, IDC_LIST_FILE, m_List);
}


BEGIN_MESSAGE_MAP(CRemoteClientDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BTN_TEST, &CRemoteClientDlg::OnBnClickedBtnTest) //WM_COMMAND
	ON_BN_CLICKED(IDC_BTN_FILEINFO, &CRemoteClientDlg::OnBnClickedBtnFileinfo) //WM_COMMAND
	ON_NOTIFY(NM_DBLCLK, IDC_TREE_DIR, &CRemoteClientDlg::OnNMDblclkTreeDir) //WM_NOTIFY
	ON_NOTIFY(NM_CLICK, IDC_TREE_DIR, &CRemoteClientDlg::OnNMClickTreeDir) //WM_NOTIFY
	ON_NOTIFY(NM_RCLICK, IDC_LIST_FILE, &CRemoteClientDlg::OnNMRClickListFile) //WM_NOTIFY
	ON_COMMAND(ID_DOWNLOAD_FILE, &CRemoteClientDlg::OnDownloadFile) //WM_COMMAND
	ON_COMMAND(ID_DELETE_FILE, &CRemoteClientDlg::OnDeleteFile) //WM_COMMAND
	ON_COMMAND(ID_RUN_FILE, &CRemoteClientDlg::OnRunFile) //WM_COMMAND
	ON_BN_CLICKED(IDC_BTN_START_WATCH, &CRemoteClientDlg::OnBnClickedBtnStartWatch) //WM_COMMAND
	ON_WM_TIMER()
	ON_NOTIFY(IPN_FIELDCHANGED, IDC_IPADDRESS_SERV, &CRemoteClientDlg::OnIpnFieldchangedIpaddressServ)
	ON_EN_CHANGE(IDC_EDIT_PORT, &CRemoteClientDlg::OnEnChangeEditPort)
	ON_MESSAGE(WM_SEND_PACK_ACK, &CRemoteClientDlg::OnSendPackAck)
END_MESSAGE_MAP()


// CRemoteClientDlg 消息处理程序

BOOL CRemoteClientDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();
	// 将“关于...”菜单项添加到系统菜单中。
	// IDM_ABOUTBOX 必须在系统命令范围内。
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);
	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}
	InitUIData();
	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

void CRemoteClientDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。  对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CRemoteClientDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CRemoteClientDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}



void CRemoteClientDlg::OnBnClickedBtnTest()
{
	CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 1981);
}


void CRemoteClientDlg::OnBnClickedBtnFileinfo()
{
	ClearTreeCache();  // ✅ 清除缓存
	
	CTreeCtrl& tree = (CTreeCtrl&)m_Tree;
    tree.DeleteAllItems();
	
	std::list<CPacket> lstPackets;
	bool ret = CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 1, true, NULL, 0);
	if (!ret) {  // ✅ 修改: 检查 bool 值
		AfxMessageBox(_T("命令处理失败!!!"));
		return;
	}
}

void CRemoteClientDlg::DealCommand(WORD nCmd, const std::string& strData, LPARAM lParam)
{
	switch (nCmd) {
	case 1://获取驱动信息
		Str2Tree(strData, m_Tree);
		break;
	case 2://获取文件信息
		UpdateFileInfo(*(PFILEINFO)strData.c_str(), (HTREEITEM)lParam);
		break;
	case 3:
		MessageBox("打开文件完成！", "操作完成", MB_ICONINFORMATION);
		break;
	case 4:
		UpdateDownloadFile(strData, (FILE*)lParam);
		break;
	case 9:
		MessageBox("删除文件完成！", "操作完成", MB_ICONINFORMATION);
		break;
	case 1981:
		MessageBox("连接测试成功！", "连接成功", MB_ICONINFORMATION);
		break;
	default:
		TRACE("unknow data received! %d\r\n", nCmd);
		break;
	}
}

void CRemoteClientDlg::InitUIData()
{
	// 设置此对话框的图标。  当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标
	UpdateData();
	m_server_address = 0x7F000001;//127.0.0.1  0xC0A80167;//192.168.1.103
	m_nPort = _T("9527");
	CClientController* pController = CClientController::getInstance();
	pController->UpdateAddress(m_server_address, atoi((LPCTSTR)m_nPort));
	UpdateData(FALSE);
	m_dlgStatus.Create(IDD_DLG_STATUS, this);
	m_dlgStatus.ShowWindow(SW_HIDE);
}

void CRemoteClientDlg::LoadFileCurrent()
{
	HTREEITEM hTree = m_Tree.GetSelectedItem();
	CString strPath = GetPath(hTree);
	m_List.DeleteAllItems();
	int nCmd = CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 2, false, (BYTE*)(LPCTSTR)strPath, strPath.GetLength());
	PFILEINFO pInfo = (PFILEINFO)CClientSocket::getInstance()->GetPacket().strData.c_str();
	while (pInfo->HasNext) {
		TRACE("[%s] isdir %d\r\n", pInfo->szFileName, pInfo->IsDirectory);
		if (!pInfo->IsDirectory) {
			m_List.InsertItem(0, pInfo->szFileName);
		}
		int cmd = CClientController::getInstance()->DealCommand();
		TRACE("ack:%d\r\n", cmd);
		if (cmd < 0)break;
		pInfo = (PFILEINFO)CClientSocket::getInstance()->GetPacket().strData.c_str();
	}
	//CClientController::getInstance()->CloseSocket();
}

void CRemoteClientDlg::Str2Tree(const std::string& drivers, CTreeCtrl& tree)
{
	std::string dr;
	tree.DeleteAllItems();
	for (size_t i = 0; i < drivers.size(); i++)
	{
		if (drivers[i] == ',') {
			dr += ":";
			HTREEITEM hTemp = tree.InsertItem(dr.c_str(), TVI_ROOT, TVI_LAST);
			tree.InsertItem("", hTemp, TVI_LAST);
			dr.clear();
			continue;
		}
		dr += drivers[i];
	}
	if (dr.size() > 0) {
		dr += ":";
		HTREEITEM hTemp = tree.InsertItem(dr.c_str(), TVI_ROOT, TVI_LAST);
		tree.InsertItem("", hTemp, TVI_LAST);
	}
}

void CRemoteClientDlg::UpdateFileInfo(const FILEINFO& finfo, HTREEITEM hParent)
{
	TRACE("hasnext %d isdirectory %d %s\r\n", finfo.HasNext, finfo.IsDirectory, finfo.szFileName);
	if (finfo.HasNext == FALSE)return;
	if (finfo.IsDirectory) {
		if (CString(finfo.szFileName) == "." || (CString(finfo.szFileName) == ".."))
			return;
		TRACE("hselected %08X %08X\r\n", hParent, m_Tree.GetSelectedItem());
		HTREEITEM hTemp = m_Tree.InsertItem(finfo.szFileName, hParent);
		m_Tree.InsertItem("", hTemp, TVI_LAST);
		m_Tree.Expand(hParent, TVE_EXPAND);
	}
	else {
		m_List.InsertItem(0, finfo.szFileName);
	}
}

void CRemoteClientDlg::UpdateDownloadFile(const std::string& strData, FILE* pFile)
{
	static LONGLONG length = 0, index = 0;
	TRACE("length %d index %d\r\n", length, index);
	if (length == 0) {
		length = *(long long*)strData.c_str();
		if (length == 0) {
			AfxMessageBox("文件长度为零或者无法读取文件！！！");
			CClientController::getInstance()->DownloadEnd();
		}
	}
	else if (length > 0 && (index >= length)) {
		fclose(pFile);
		length = 0;
		index = 0;
		CClientController::getInstance()->DownloadEnd();
	}
	else {
		fwrite(strData.c_str(), 1, strData.size(), pFile);
		index += strData.size();
		TRACE("index = %d\r\n", index);
		if (index >= length) {
			fclose(pFile);
			length = 0;
			index = 0;
			CClientController::getInstance()->DownloadEnd();
		}
	}
}

void CRemoteClientDlg::LoadFileInfo()
{
	CPoint ptMouse;
	GetCursorPos(&ptMouse);
	m_Tree.ScreenToClient(&ptMouse);
	HTREEITEM hTreeSelected = m_Tree.HitTest(ptMouse, 0);
	
	if (hTreeSelected == NULL) {
		TRACE("LoadFileInfo: 没有命中任何节点\r\n");
		return;
	}
	
	// ✅ 添加调试日志
	TRACE("LoadFileInfo: hTreeSelected = %p\r\n", hTreeSelected);
	
	DeleteTreeChildrenItem(hTreeSelected);
	m_List.DeleteAllItems();
	
	CString strPath = GetPath(hTreeSelected);
	TRACE("LoadFileInfo: path = %s\r\n", (LPCTSTR)strPath);
	
	// ✅ 确保传递节点句柄
	int ret = CClientController::getInstance()->SendCommandPacket(
		GetSafeHwnd(), 
		2,              // 命令: 获取文件列表
		false,          // 不自动关闭
		(BYTE*)(LPCTSTR)strPath, 
		strPath.GetLength(), 
		(WPARAM)hTreeSelected  // ✅ 传递节点句柄
	);
	
	TRACE("LoadFileInfo: SendCommandPacket returned %d\r\n", ret);
}

void CRemoteClientDlg::OnTvnItemexpandedTree(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMTREEVIEW pNMTreeView = reinterpret_cast<LPNMTREEVIEW>(pNMHDR);
	*pResult = 0;
	
	if (pNMTreeView->action == TVE_EXPAND) {
		CTreeCtrl& tree = (CTreeCtrl&)m_Tree;
		HTREEITEM hSelected = pNMTreeView->itemNew.hItem;
		
		TRACE("OnTvnItemexpandedTree: 展开节点 %p\r\n", hSelected);
		
		// ✅ 检查是否已经有子项（排除占位符）
		HTREEITEM hChild = tree.GetChildItem(hSelected);
		if (hChild != NULL) {
			CString strChild = tree.GetItemText(hChild);
			if (!strChild.IsEmpty()) {
				// ✅ 已有实际子项，不需要重新加载
				TRACE("OnTvnItemexpandedTree: 节点已有子项，跳过加载\r\n");
				return;
			}
		}
		
		LoadFilesInfo(hSelected);
	}
}

// ✅ 修复双击事件处理
void CRemoteClientDlg::OnNMDblclkTree(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = 0;
	
	CTreeCtrl& tree = (CTreeCtrl&)m_Tree;
	HTREEITEM hSelected = tree.GetSelectedItem();
	
	if (hSelected == NULL) {
		return;
	}
	
	// ✅ 获取节点数据判断是否为目录
	DWORD_PTR dwData = tree.GetItemData(hSelected);
	bool isDirectory = (dwData != 0);
	
	if (!isDirectory) {
		// ✅ 文件：打开文件
		CString strPath = GetTreePath(hSelected);
		TRACE("OnNMDblclkTree: 打开文件 %s\r\n", (LPCTSTR)strPath);
		// TODO: 实现文件打开逻辑
	}
	else {
		// ✅ 目录：展开/折叠（系统会自动处理，不需要手动加载）
		UINT nState = tree.GetItemState(hSelected, TVIS_EXPANDED);
		if (nState & TVIS_EXPANDED) {
			tree.Expand(hSelected, TVE_COLLAPSE);
		}
		else {
			tree.Expand(hSelected, TVE_EXPAND);
		}
	}
}

// ✅ 添加 LoadFilesInfo 函数
int CRemoteClientDlg::LoadFilesInfo(HTREEITEM hTreeSelected)
{
	TRACE("LoadFilesInfo: hTreeSelected %p\r\n", hTreeSelected);
	
	if (hTreeSelected == NULL) {
		return -1;
	}
	
	// ✅ 检查是否正在加载
	EnterCriticalSection(&m_csTreeAccess);
	bool isLoading = (m_setLoadingItems.find(hTreeSelected) != m_setLoadingItems.end());
	bool isLoaded = (m_mapLoadedItems.find(hTreeSelected) != m_mapLoadedItems.end() 
					 && m_mapLoadedItems[hTreeSelected]);
	LeaveCriticalSection(&m_csTreeAccess);
	
	if (isLoading) {
		TRACE("LoadFilesInfo: 节点 %p 正在加载中，跳过重复请求\r\n", hTreeSelected);
		return 0;
	}
	
	if (isLoaded) {
		TRACE("LoadFilesInfo: 节点 %p 已经加载过，跳过\r\n", hTreeSelected);
		return 0;
	}
	
	// ✅ 标记为正在加载
	EnterCriticalSection(&m_csTreeAccess);
	m_setLoadingItems.insert(hTreeSelected);
	LeaveCriticalSection(&m_csTreeAccess);
	
	CTreeCtrl& tree = (CTreeCtrl&)m_Tree;
	CString strPath = GetTreePath(hTreeSelected);
	
	TRACE("LoadFilesInfo: 请求路径 %s\r\n", (LPCTSTR)strPath);
	
	// ✅ 缓存路径
	EnterCriticalSection(&m_csTreeAccess);
	m_mapItemPath[hTreeSelected] = strPath;
	LeaveCriticalSection(&m_csTreeAccess);
	
	int ret = CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 2, false, (BYTE*)(LPCSTR)strPath, strPath.GetLength(), (WPARAM)hTreeSelected);
	
	if (!ret) {  // ✅ 修改: 检查 bool 值
		// ✅ 发送失败，移除加载标记
		EnterCriticalSection(&m_csTreeAccess);
		m_setLoadingItems.erase(hTreeSelected);
		LeaveCriticalSection(&m_csTreeAccess);
		
		TRACE("LoadFilesInfo: 发送请求失败\r\n");
		return -1;
	}
	
	return 0;
}

// ✅ 清理缓存函数
void CRemoteClientDlg::ClearTreeCache()
{
	EnterCriticalSection(&m_csTreeAccess);
	m_mapLoadedItems.clear();
	m_setLoadingItems.clear();
	m_mapItemPath.clear();
	LeaveCriticalSection(&m_csTreeAccess);
}

LRESULT CRemoteClientDlg::OnSendPackAck(WPARAM wParam, LPARAM lParam)
{
	TRACE("=== OnSendPackAck: wParam=%p, lParam=%p ===\r\n", wParam, lParam);
	
	if (lParam == -1 || lParam == -2) {
		AfxMessageBox(_T("连接失败"));
		return 1;
	}
	
	CPacket* pPacket = (CPacket*)wParam;
	if (pPacket == NULL) {
		TRACE("OnSendPackAck: 空数据包\r\n");
		return 0;
	}
	
	int cmd = pPacket->sCmd;
	TRACE("OnSendPackAck: cmd=%d, dataSize=%d\r\n", cmd, pPacket->strData.size());
	
	switch (cmd) {
	case 1:  // 驱动器信息
		TRACE("OnSendPackAck: 处理驱动器信息\r\n");
		Str2Tree(pPacket->strData, m_Tree);
		break;
		
	case 2:  // 文件列表
	{
		TRACE("OnSendPackAck: 处理文件列表, lParam=%p\r\n", lParam);
		
		if (pPacket->strData.size() < sizeof(FILEINFO)) {
			TRACE("OnSendPackAck: 数据包大小不足 %d < %d\r\n", 
				  pPacket->strData.size(), sizeof(FILEINFO));
			break;
		}
		
		FILEINFO finfo;
		memcpy(&finfo, pPacket->strData.c_str(), sizeof(FILEINFO));
		
		TRACE("OnSendPackAck: HasNext=%d, IsDirectory=%d, FileName='%s'\r\n", 
			  finfo.HasNext, finfo.IsDirectory, finfo.szFileName);
		
		CTreeCtrl& tree = (CTreeCtrl&)m_Tree;
		CListCtrl& list = (CListCtrl&)m_List;  // 添加ListView引用
		HTREEITEM hSelected = (HTREEITEM)lParam;
		
		if (hSelected == NULL || hSelected == (HTREEITEM)1) {
			TRACE("OnSendPackAck: lParam 无效 (%p), 尝试使用当前选中节点\r\n", lParam);
			hSelected = tree.GetSelectedItem();
		}
		
		if (hSelected == NULL) {
			TRACE("OnSendPackAck: 没有有效的节点句柄,放弃处理\r\n");
			break;
		}
		
		TRACE("OnSendPackAck: 使用节点句柄 %p\r\n", hSelected);
		
		static std::set<HTREEITEM> deletedPlaceholder;
		
		if (finfo.HasNext == 1 && finfo.IsDirectory >= 0) {
			CString strFileName(finfo.szFileName);
			
			if (strFileName == "." || strFileName == "..") {
				TRACE("OnSendPackAck: 跳过 '%s'\r\n", (LPCTSTR)strFileName);
				break;
			}
			
			// ✅ 关键修改: 根据 IsDirectory 决定添加到Tree还是List
			if (finfo.IsDirectory == 1) {
				// ========== 目录: 添加到TreeView ==========
				
				// 删除占位符 (只执行一次)
				if (deletedPlaceholder.find(hSelected) == deletedPlaceholder.end()) {
					HTREEITEM hFirstChild = tree.GetChildItem(hSelected);
					if (hFirstChild != NULL) {
						CString strFirstChild = tree.GetItemText(hFirstChild);
						if (strFirstChild.IsEmpty()) {
							tree.DeleteItem(hFirstChild);
							deletedPlaceholder.insert(hSelected);
							TRACE("OnSendPackAck: 删除占位符节点\r\n");
						}
					}
				}
				
				// 检查是否已存在
				HTREEITEM hChild = tree.GetChildItem(hSelected);
				bool bExists = false;
				
				while (hChild != NULL) {
					CString strExisting = tree.GetItemText(hChild);
					if (strExisting == strFileName) {
						bExists = true;
						TRACE("OnSendPackAck: 目录 '%s' 已存在\r\n", (LPCTSTR)strFileName);
						break;
					}
					hChild = tree.GetNextSiblingItem(hChild);
				}
				
				if (!bExists) {
					HTREEITEM hTmp = tree.InsertItem(strFileName, hSelected);
					tree.SetItemData(hTmp, (DWORD_PTR)1);  // 标记为目录
					tree.InsertItem("", hTmp);  // 添加占位符
					
					TRACE("OnSendPackAck: 成功添加目录 '%s' 到TreeView\r\n", 
						  (LPCTSTR)strFileName);
				}
			}
			else {
				// ========== 文件: 添加到ListView ==========
				
				// 检查是否已存在
				int nCount = list.GetItemCount();
				bool bExists = false;
				
				for (int i = 0; i < nCount; i++) {
					CString strExisting = list.GetItemText(i, 0);
					if (strExisting == strFileName) {
						bExists = true;
						TRACE("OnSendPackAck: 文件 '%s' 已存在\r\n", (LPCTSTR)strFileName);
						break;
					}
				}
				
				if (!bExists) {
					int nIndex = list.InsertItem(nCount, strFileName);
					// 可以添加更多列信息,例如文件大小、修改时间等
					// list.SetItemText(nIndex, 1, strFileSize);
					// list.SetItemText(nIndex, 2, strModifyTime);
					
					TRACE("OnSendPackAck: 成功添加文件 '%s' 到ListView\r\n", 
						  (LPCTSTR)strFileName);
				}
			}
		}
		else if (finfo.HasNext == 0) {
			// 结束标记
			EnterCriticalSection(&m_csTreeAccess);
			m_mapLoadedItems[hSelected] = true;
			m_setLoadingItems.erase(hSelected);
			LeaveCriticalSection(&m_csTreeAccess);
			
			deletedPlaceholder.erase(hSelected);
			
			// 自动展开节点
			tree.Expand(hSelected, TVE_EXPAND);
			
			// 统计
			HTREEITEM hChild = tree.GetChildItem(hSelected);
			int childCount = 0;
			while (hChild != NULL) {
				childCount++;
				hChild = tree.GetNextSiblingItem(hChild);
			}
			
			int fileCount = list.GetItemCount();
			
			TRACE("OnSendPackAck: 节点 %p 加载完成, %d 个子目录, %d 个文件, 已展开\r\n", 
				  hSelected, childCount, fileCount);
		}
	}
	break;
	
	case 3:
		MessageBox("打开文件完成！", "操作完成", MB_ICONINFORMATION);
		break;
		
	case 4:
		UpdateDownloadFile(pPacket->strData, (FILE*)lParam);
		break;
		
	case 9:
		MessageBox("删除文件完成！", "操作完成", MB_ICONINFORMATION);
		break;
		
	case 1981:
		MessageBox("连接测试成功！", "连接成功", MB_ICONINFORMATION);
		break;
		
	default:
		TRACE("OnSendPackAck: 未知命令 %d\r\n", cmd);
		break;
	}
	
	delete pPacket;
	return 0;
}

CString CRemoteClientDlg::GetPath(HTREEITEM hTree)
{
	CString strRet, strTmp;
	do {
		strTmp = m_Tree.GetItemText(hTree);
		strRet = strTmp + '\\' + strRet;
		hTree = m_Tree.GetParentItem(hTree);
	} while (hTree != NULL);
	return strRet;
}

CString CRemoteClientDlg::GetTreePath(HTREEITEM hTree)
{
	return GetPath(hTree);  // 复用现有函数
}

void CRemoteClientDlg::DeleteTreeChildrenItem(HTREEITEM hTree)
{
	TRACE("DeleteTreeChildrenItem: 删除节点 %p 的所有子项\r\n", hTree);
	
	int count = 0;
	HTREEITEM hSub = NULL;
	do {
		hSub = m_Tree.GetChildItem(hTree);
		if (hSub != NULL) {
			CString strChild = m_Tree.GetItemText(hSub);
			TRACE("DeleteTreeChildrenItem: 删除子节点[%d] '%s'\r\n", 
				  ++count, (LPCTSTR)strChild);
			m_Tree.DeleteItem(hSub);
		}
	} while (hSub != NULL);
	
	TRACE("DeleteTreeChildrenItem: 共删除 %d 个子节点\r\n", count);
}

void CRemoteClientDlg::OnNMDblclkTreeDir(NMHDR* pNMHDR, LRESULT* pResult)
{
	// TODO: 在此添加控件通知处理程序代码
	*pResult = 0;
	LoadFileInfo();
}

void CRemoteClientDlg::OnNMClickTreeDir(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = 0;
	
	CTreeCtrl& tree = (CTreeCtrl&)m_Tree;
	
	// 获取点击的节点
	CPoint ptMouse;
	GetCursorPos(&ptMouse);
	tree.ScreenToClient(&ptMouse);
	HTREEITEM hClickedItem = tree.HitTest(ptMouse, 0);
	
	if (hClickedItem == NULL) {
		TRACE("OnNMClickTreeDir: 没有命中任何节点\r\n");
		return;
	}
	
	CString strItemText = tree.GetItemText(hClickedItem);
	TRACE("OnNMClickTreeDir: 点击节点 %p '%s'\r\n", hClickedItem, (LPCTSTR)strItemText);
	
	// 选中节点
	tree.SelectItem(hClickedItem);
	
	// ✅ 判断是否有真实子节点
	HTREEITEM hFirstChild = tree.GetChildItem(hClickedItem);
	bool bHasRealChildren = false;
	
	if (hFirstChild != NULL) {
		CString strFirstChild = tree.GetItemText(hFirstChild);
		if (!strFirstChild.IsEmpty()) {
			bHasRealChildren = true;
		}
	}
	
	UINT nState = tree.GetItemState(hClickedItem, TVIS_EXPANDED);
	bool bIsExpanded = (nState & TVIS_EXPANDED) != 0;
	
	TRACE("OnNMClickTreeDir: bIsExpanded=%d, bHasRealChildren=%d\r\n", 
		  bIsExpanded, bHasRealChildren);
	
	if (bIsExpanded && bHasRealChildren) {
		// ========== 节点已展开 -> 折叠并重置 ==========
		TRACE("OnNMClickTreeDir: 折叠并重置节点 '%s'\r\n", (LPCTSTR)strItemText);
		
		// 删除所有子节点
		HTREEITEM hChild = tree.GetChildItem(hClickedItem);
		while (hChild != NULL) {
			HTREEITEM hNext = tree.GetNextSiblingItem(hChild);
			tree.DeleteItem(hChild);
			hChild = hNext;
		}
		
		// 重新添加占位符
		tree.InsertItem("", hClickedItem);
		
		// 清除加载缓存
		EnterCriticalSection(&m_csTreeAccess);
		m_mapLoadedItems[hClickedItem] = false;
		m_setLoadingItems.erase(hClickedItem);
		LeaveCriticalSection(&m_csTreeAccess);
		
		// 清空右侧ListView
		m_List.DeleteAllItems();
		TRACE("OnNMClickTreeDir: 已清空ListView并重置节点\r\n");
	}
	else {
		// ========== 节点未展开 -> 展开并加载 ==========
		TRACE("OnNMClickTreeDir: 展开节点 '%s'\r\n", (LPCTSTR)strItemText);
		
		if (!bHasRealChildren) {
			LoadFileInfo();
		}
		else {
			tree.Expand(hClickedItem, TVE_EXPAND);
		}
	}
}

void CRemoteClientDlg::OnNMRClickListFile(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
	// TODO: 在此添加控件通知处理程序代码
	*pResult = 0;
	CPoint ptMouse, ptList;
	GetCursorPos(&ptMouse);
	ptList = ptMouse;
	m_List.ScreenToClient(&ptList);
	int ListSelected = m_List.HitTest(ptList);
	if (ListSelected < 0)return;
	CMenu menu;
	menu.LoadMenu(IDR_MENU_RCLICK);
	CMenu* pPupup = menu.GetSubMenu(0);
	if (pPupup != NULL) {
		pPupup->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, ptMouse.x, ptMouse.y, this);
	}
}

void CRemoteClientDlg::OnDownloadFile()
{
	int nListSelected = m_List.GetSelectionMark();
	CString strFile = m_List.GetItemText(nListSelected, 0);
	HTREEITEM hSelected = m_Tree.GetSelectedItem();
	strFile = GetPath(hSelected) + strFile;
	int ret = CClientController::getInstance()->DownFile(strFile);
	if (ret != 0) {
		MessageBox(_T("下载失败！"));
		TRACE("下载失败 ret = %d\r\n", ret);
	}
}

void CRemoteClientDlg::OnDeleteFile()
{
	HTREEITEM hSelected = m_Tree.GetSelectedItem();
	CString strPath = GetPath(hSelected);
	int nSelected = m_List.GetSelectionMark();
	CString strFile = m_List.GetItemText(nSelected, 0);
	strFile = strPath + strFile;
	bool ret = CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 9, true, (BYTE*)(LPCSTR)strFile, strFile.GetLength());
	if (!ret) {
		AfxMessageBox("删除文件命令执行失败！！！");
	}
	LoadFileCurrent();
}

void CRemoteClientDlg::OnRunFile()
{
	HTREEITEM hSelected = m_Tree.GetSelectedItem();
	CString strPath = GetPath(hSelected);
	int nSelected = m_List.GetSelectionMark();
	CString strFile = m_List.GetItemText(nSelected, 0);
	strFile = strPath + strFile;
	bool ret = CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 3, true, (BYTE*)(LPCSTR)strFile, strFile.GetLength());
	if (!ret) {
		AfxMessageBox("打开文件命令执行失败！！！");
	}
}

void CRemoteClientDlg::OnBnClickedBtnStartWatch()
{
	CClientController::getInstance()->StartWatchScreen();
}

void CRemoteClientDlg::OnTimer(UINT_PTR nIDEvent)
{
	// TODO: 在此添加消息处理程序代码和/或调用默认值
	CDialogEx::OnTimer(nIDEvent);
}

void CRemoteClientDlg::OnIpnFieldchangedIpaddressServ(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMIPADDRESS pIPAddr = reinterpret_cast<LPNMIPADDRESS>(pNMHDR);
	// TODO: 在此添加控件通知处理程序代码
	*pResult = 0;
	UpdateData();
	CClientController* pController = CClientController::getInstance();
	pController->UpdateAddress(m_server_address, atoi((LPCTSTR)m_nPort));
}

void CRemoteClientDlg::OnEnChangeEditPort()
{
	UpdateData();
	CClientController* pController = CClientController::getInstance();
	pController->UpdateAddress(m_server_address, atoi((LPCTSTR)m_nPort));
}
