// CWatchDialog.cpp: 实现文件
//

#include "pch.h"
#include "RemoteClient.h"
#include "CWatchDialog.h"
#include "afxdialogex.h"
#include "ClientController.h"
#include "ClientSocket.h"  // ✅ 添加

// CWatchDialog 对话框

IMPLEMENT_DYNAMIC(CWatchDialog, CDialog)

CWatchDialog::CWatchDialog(CWnd* pParent /*=nullptr*/)
	: CDialog(IDD_DLG_WATCH, pParent)
	, m_nObjWidth(-1)
	, m_nObjHeight(-1)
	, m_isFull(false)
	, isFull(false)        // ✅ 初始化
	, m_nTimerID(0)        // ✅ 初始化
{
}

CWatchDialog::~CWatchDialog()
{
	// ✅ 清理定时器
	if (m_nTimerID != 0) {
		KillTimer(m_nTimerID);
		m_nTimerID = 0;
	}
}

void CWatchDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_WATCH, m_picture);
}


BEGIN_MESSAGE_MAP(CWatchDialog, CDialog)
	ON_WM_TIMER()
	ON_WM_LBUTTONDBLCLK()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_RBUTTONDBLCLK()
	ON_WM_RBUTTONDOWN()
	ON_WM_RBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_STN_CLICKED(IDC_WATCH, &CWatchDialog::OnStnClickedWatch)
	ON_BN_CLICKED(IDC_BTN_LOCK, &CWatchDialog::OnBnClickedBtnLock)
	ON_BN_CLICKED(IDC_BTN_UNLOCK, &CWatchDialog::OnBnClickedBtnUnlock)
	ON_BN_CLICKED(IDC_BTN_WATCH, &CWatchDialog::OnBnClickedBtnWatch)  // ✅ 添加
	ON_MESSAGE(WM_SEND_PACK_ACK, &CWatchDialog::OnSendPackAck)
END_MESSAGE_MAP()


// CWatchDialog 消息处理程序

CPoint CWatchDialog::UserPoint2RemoteScreenPoint(CPoint& point, bool isScreen)
{
	CRect clientRect;
	if (!isScreen) ClientToScreen(&point);
	m_picture.ScreenToClient(&point);
	TRACE("x=%d y=%d\r\n", point.x, point.y);
	m_picture.GetWindowRect(clientRect);
	TRACE("x=%d y=%d\r\n", clientRect.Width(), clientRect.Height());
	return CPoint(point.x * m_nObjWidth / clientRect.Width(), 
	              point.y * m_nObjHeight / clientRect.Height());
}

BOOL CWatchDialog::OnInitDialog()
{
	CDialog::OnInitDialog();
	m_isFull = false;
	return TRUE;
}

void CWatchDialog::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == WATCH_TIMER_ID)
	{
		// 请求新的截图
		CClientController::getInstance()->SendCommandPacket(
			m_hWnd, 6, false);  // ✅ false = 不自动关闭
	}
	
	CDialog::OnTimer(nIDEvent);
}

// ✅ 新增:开始/停止监控按钮处理
void CWatchDialog::OnBnClickedBtnWatch()
{
	if (isFull == false)
	{
		isFull = true;
		SetDlgItemText(IDC_BTN_WATCH, _T("停止监控"));
		
		// 启动定时器,持续获取图像
		m_nTimerID = SetTimer(WATCH_TIMER_ID, 50, NULL);  // 20fps
		TRACE("开始监控,定时器ID=%d\r\n", m_nTimerID);
	}
	else
	{
		isFull = false;
		SetDlgItemText(IDC_BTN_WATCH, _T("开始监控"));
		
		// ✅ 停止定时器
		if (m_nTimerID != 0) {
			KillTimer(m_nTimerID);
			m_nTimerID = 0;
		}
		
		// ✅ 关闭Socket连接
		CClientSocket::getInstance()->StopMonitoring();
		TRACE("停止监控\r\n");
	}
}

LRESULT CWatchDialog::OnSendPackAck(WPARAM wParam, LPARAM lParam)
{
	if (lParam == -1 || (lParam == -2)) {
		//TODO:错误处理
	}
	else if (lParam == 1) {
		//对方关闭了套接字
	}
	else {
		CPacket* pPacket = (CPacket*)wParam;
		if (pPacket != NULL) {
			CPacket head = *pPacket;
			delete pPacket;
			
			switch (head.sCmd) {
			case 6:
			{
				CEdoyunTool::Bytes2Image(m_image, head.strData);
				CRect rect;
				m_picture.GetWindowRect(rect);
				m_nObjWidth = m_image.GetWidth();
				m_nObjHeight = m_image.GetHeight();
				m_image.StretchBlt(
					m_picture.GetDC()->GetSafeHdc(), 0, 0, 
					rect.Width(), rect.Height(), SRCCOPY);
				m_picture.InvalidateRect(NULL);
				TRACE("更新图片完成%d %d %08X\r\n", 
				      m_nObjWidth, m_nObjHeight, (HBITMAP)m_image);
				m_image.Destroy();
				break;
			}
			case 5:
				TRACE("远程端应答了鼠标操作\r\n");
				break;
			case 7:
			case 8:
			default:
				break;
			}
		}
	}
	
	return 0;
}

void CWatchDialog::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	if ((m_nObjWidth != -1) && (m_nObjHeight != -1)) {
		CPoint remote = UserPoint2RemoteScreenPoint(point);
		MOUSEEV event;
		event.ptXY = remote;
		event.nButton = 0;
		event.nAction = 2;
		CClientController::getInstance()->SendCommandPacket(
			GetSafeHwnd(), 5, true, (BYTE*)&event, sizeof(event));
	}
	CDialog::OnLButtonDblClk(nFlags, point);
}

void CWatchDialog::OnLButtonDown(UINT nFlags, CPoint point)
{
	if ((m_nObjWidth != -1) && (m_nObjHeight != -1)) {
		TRACE("x=%d y=%d\r\n", point.x, point.y);
		CPoint remote = UserPoint2RemoteScreenPoint(point);
		TRACE("remote:%d %d\r\n", remote.x, remote.y);
		MOUSEEV event;
		event.ptXY = remote;
		event.nButton = 0;
		event.nAction = 2;
		CClientController::getInstance()->SendCommandPacket(
			GetSafeHwnd(), 5, true, (BYTE*)&event, sizeof(event));
	}
	CDialog::OnLButtonDown(nFlags, point);
}

void CWatchDialog::OnLButtonUp(UINT nFlags, CPoint point)
{
	if ((m_nObjWidth != -1) && (m_nObjHeight != -1)) {
		CPoint remote = UserPoint2RemoteScreenPoint(point);
		MOUSEEV event;
		event.ptXY = remote;
		event.nButton = 0;
		event.nAction = 3;
		CClientController::getInstance()->SendCommandPacket(
			GetSafeHwnd(), 5, true, (BYTE*)&event, sizeof(event));
	}
	CDialog::OnLButtonUp(nFlags, point);
}

void CWatchDialog::OnRButtonDblClk(UINT nFlags, CPoint point)
{
	if ((m_nObjWidth != -1) && (m_nObjHeight != -1)) {
		CPoint remote = UserPoint2RemoteScreenPoint(point);
		MOUSEEV event;
		event.ptXY = remote;
		event.nButton = 1;
		event.nAction = 1;
		CClientController::getInstance()->SendCommandPacket(
			GetSafeHwnd(), 5, true, (BYTE*)&event, sizeof(event));
	}
	CDialog::OnRButtonDblClk(nFlags, point);
}

void CWatchDialog::OnRButtonDown(UINT nFlags, CPoint point)
{
	if ((m_nObjWidth != -1) && (m_nObjHeight != -1)) {
		CPoint remote = UserPoint2RemoteScreenPoint(point);
		MOUSEEV event;
		event.ptXY = remote;
		event.nButton = 1;
		event.nAction = 2;
		CClientController::getInstance()->SendCommandPacket(
			GetSafeHwnd(), 5, true, (BYTE*)&event, sizeof(event));
	}
	CDialog::OnRButtonDown(nFlags, point);
}

void CWatchDialog::OnRButtonUp(UINT nFlags, CPoint point)
{
	if ((m_nObjWidth != -1) && (m_nObjHeight != -1)) {
		CPoint remote = UserPoint2RemoteScreenPoint(point);
		MOUSEEV event;
		event.ptXY = remote;
		event.nButton = 1;
		event.nAction = 3;
		CClientController::getInstance()->SendCommandPacket(
			GetSafeHwnd(), 5, true, (BYTE*)&event, sizeof(event));
	}
	CDialog::OnRButtonUp(nFlags, point);
}

void CWatchDialog::OnMouseMove(UINT nFlags, CPoint point)
{
	if ((m_nObjWidth != -1) && (m_nObjHeight != -1)) {
		CPoint remote = UserPoint2RemoteScreenPoint(point);
		MOUSEEV event;
		event.ptXY = remote;
		event.nButton = 8;
		event.nAction = 0;
		CClientController::getInstance()->SendCommandPacket(
			GetSafeHwnd(), 5, true, (BYTE*)&event, sizeof(event));
	}
	CDialog::OnMouseMove(nFlags, point);
}

void CWatchDialog::OnStnClickedWatch()
{
	if ((m_nObjWidth != -1) && (m_nObjHeight != -1)) {
		CPoint point;
		GetCursorPos(&point);
		CPoint remote = UserPoint2RemoteScreenPoint(point, true);
		MOUSEEV event;
		event.ptXY = remote;
		event.nButton = 0;
		event.nAction = 0;
		CClientController::getInstance()->SendCommandPacket(
			GetSafeHwnd(), 5, true, (BYTE*)&event, sizeof(event));
	}
}

void CWatchDialog::OnOK()
{
	// 不调用基类，避免回车关闭对话框
	//CDialog::OnOK();
}

void CWatchDialog::OnBnClickedBtnLock()
{
	CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 7);
}

void CWatchDialog::OnBnClickedBtnUnlock()
{
	CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 8);
}
