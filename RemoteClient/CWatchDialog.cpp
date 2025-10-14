// CWatchDialog.cpp: 实现文件
//

#include "pch.h"
#include "RemoteClient.h"
#include "CWatchDialog.h"
#include "afxdialogex.h"
#include "ClientController.h"
#include "ClientSocket.h"  // 添加

// CWatchDialog 对话框

IMPLEMENT_DYNAMIC(CWatchDialog, CDialog)

CWatchDialog::CWatchDialog(CWnd* pParent /*=nullptr*/)
	: CDialog(IDD_DLG_WATCH, pParent)
	, m_nObjWidth(-1)
	, m_nObjHeight(-1)
	, m_isFull(false)
{
}

CWatchDialog::~CWatchDialog()
{

}

void CWatchDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_WATCH, m_picture);
}


BEGIN_MESSAGE_MAP(CWatchDialog, CDialog)
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
	ON_BN_CLICKED(IDC_BTN_WATCH, &CWatchDialog::OnBnClickedBtnWatch)
	ON_MESSAGE(WM_SEND_PACK_ACK, &CWatchDialog::OnSendPackAck)
	ON_WM_CLOSE()
	ON_WM_PAINT() // 响应OnPaint消息
END_MESSAGE_MAP()


// CWatchDialog 消息处理程序

CPoint CWatchDialog::UserPoint2RemoteScreenPoint(CPoint& point, bool isScreen)
{
	CRect clientRect;
	m_picture.GetClientRect(&clientRect); // 使用 GetClientRect 获取控件内部尺寸

	if (clientRect.IsRectEmpty() || m_nObjWidth <= 0 || m_nObjHeight <= 0) {
		return CPoint(0, 0); // 防止除零错误
	}

	// 坐标转换应该是相对于控件内部的
	// 外部传入的point已经是相对于对话框的客户区坐标
	CRect pictureRect;
	m_picture.GetWindowRect(&pictureRect);
	ScreenToClient(&pictureRect);

	// 计算鼠标点相对于picture控件左上角的位置
	CPoint pointInPicture(point.x - pictureRect.left, point.y - pictureRect.top);

	return CPoint(pointInPicture.x * m_nObjWidth / clientRect.Width(),
		pointInPicture.y * m_nObjHeight / clientRect.Height());
}

BOOL CWatchDialog::OnInitDialog()
{
	CDialog::OnInitDialog();
	m_isFull = false;

	SetWindowText(_T("远程监控 - 正在连接..."));
	if (CClientController::getInstance()->StartWatch(GetSafeHwnd()))
	{
		SetWindowText(_T("远程监控 - 连接成功"));
		// 连接成功后，立即请求第一帧屏幕数据
		CClientController::getInstance()->SendCommandWatch(6);
	}
	else
	{
		SetWindowText(_T("远程监控 - 连接失败"));
		MessageBox(_T("无法连接到远程服务器！"), _T("连接失败"), MB_ICONERROR);
		EndDialog(IDCANCEL); // 连接失败直接关闭对话框
	}

	return TRUE;
}


// 新增:开始/停止监控按钮处理
void CWatchDialog::OnBnClickedBtnWatch()
{
	if (m_isFull == false) // 如果当前是停止状态，则开始
	{
		m_isFull = true;
		SetDlgItemText(IDC_BTN_WATCH, _T("停止监控"));
		// 立即请求一帧画面，后续的请求会在OnSendPackAck中自动循环
		CClientController::getInstance()->SendCommandWatch(6);
		TRACE("请求循环已启动\r\n");
	}
	else // 如果当前是监控状态，则停止
	{
		m_isFull = false;
		SetDlgItemText(IDC_BTN_WATCH, _T("开始监控"));
		// isFull标志位会阻止OnSendPackAck中请求下一帧，从而停止循环
		TRACE("请求循环已停止\r\n");
	}
}

void CWatchDialog::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	MOUSEEV event;
	event.ptXY = UserPoint2RemoteScreenPoint(point);
	event.nButton = 0;
	event.nAction = 1; // 双击 Action 通常是 1 或一个特定值
	CClientController::getInstance()->SendCommandWatch(5, (BYTE*)&event, sizeof(event));
	CDialog::OnLButtonDblClk(nFlags, point);
}

void CWatchDialog::OnLButtonDown(UINT nFlags, CPoint point)
{
	MOUSEEV event;
	event.ptXY = UserPoint2RemoteScreenPoint(point);
	event.nButton = 0;
	event.nAction = 2; // 按下 Action
	CClientController::getInstance()->SendCommandWatch(5, (BYTE*)&event, sizeof(event));
	CDialog::OnLButtonDown(nFlags, point);
}

void CWatchDialog::OnLButtonUp(UINT nFlags, CPoint point)
{
	MOUSEEV event;
	event.ptXY = UserPoint2RemoteScreenPoint(point);
	event.nButton = 0;
	event.nAction = 3; // 弹起 Action
	CClientController::getInstance()->SendCommandWatch(5, (BYTE*)&event, sizeof(event));
	CDialog::OnLButtonUp(nFlags, point);
}

void CWatchDialog::OnRButtonDblClk(UINT nFlags, CPoint point)
{
	MOUSEEV event;
	event.ptXY = UserPoint2RemoteScreenPoint(point);
	event.nButton = 1;
	event.nAction = 1;
	CClientController::getInstance()->SendCommandWatch(5, (BYTE*)&event, sizeof(event));
	CDialog::OnRButtonDblClk(nFlags, point);
}

void CWatchDialog::OnRButtonDown(UINT nFlags, CPoint point)
{
	MOUSEEV event;
	event.ptXY = UserPoint2RemoteScreenPoint(point);
	event.nButton = 1;
	event.nAction = 2;
	CClientController::getInstance()->SendCommandWatch(5, (BYTE*)&event, sizeof(event));
	CDialog::OnRButtonDown(nFlags, point);
}

void CWatchDialog::OnRButtonUp(UINT nFlags, CPoint point)
{
	MOUSEEV event;
	event.ptXY = UserPoint2RemoteScreenPoint(point);
	event.nButton = 1;
	event.nAction = 3;
	CClientController::getInstance()->SendCommandWatch(5, (BYTE*)&event, sizeof(event));
	CDialog::OnRButtonUp(nFlags, point);
}

void CWatchDialog::OnMouseMove(UINT nFlags, CPoint point)
{
	MOUSEEV event;
	event.ptXY = UserPoint2RemoteScreenPoint(point);
	event.nButton = 8; // 移动通常用一个特殊值
	event.nAction = 0; // 移动 Action
	CClientController::getInstance()->SendCommandWatch(5, (BYTE*)&event, sizeof(event));
	CDialog::OnMouseMove(nFlags, point);
}

void CWatchDialog::OnStnClickedWatch()
{
	// 这个函数通常是单击静态控件时触发，如果需要处理单击，可以放在这里
}

void CWatchDialog::OnOK()
{
	// 阻止回车键关闭对话框
}

void CWatchDialog::OnBnClickedBtnLock()
{
	CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 7);
}

void CWatchDialog::OnBnClickedBtnUnlock()
{
	CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 8);
}

// 1. 添加 OnClose 函数，用于断开长连接
void CWatchDialog::OnClose()
{
	TRACE("CWatchDialog::OnClose - 正在关闭监控窗口，请求断开长连接。\r\n");
	CClientController::getInstance()->StopWatch();
	CDialog::OnClose();
}

// 2. 改造 OnSendPackAck 函数，使其成为新的核心处理器
LRESULT CWatchDialog::OnSendPackAck(WPARAM wParam, LPARAM lParam)
{
	if (lParam == -1) // -1 表示连接断开或出错
	{
		SetWindowText(_T("远程监控 - 连接已断开"));
		MessageBox(_T("与服务器的连接已断开！"), _T("连接错误"), MB_ICONERROR);
		EndDialog(IDCANCEL); // 关闭对话框
		return 0;
	}

	CPacket* pPacket = (CPacket*)wParam;
	if (pPacket == NULL) return 0;

	switch (pPacket->sCmd)
	{
	case 6: // 收到屏幕数据
	{
		// 使用 CImage 直接从内存加载JPG/PNG等格式的图像数据
		HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, pPacket->strData.size());
		if (hGlobal)
		{
			LPVOID pData = GlobalLock(hGlobal);
			if (pData) {
				memcpy(pData, pPacket->strData.c_str(), pPacket->strData.size());
				GlobalUnlock(hGlobal);

				IStream* pStream = NULL;
				if (CreateStreamOnHGlobal(hGlobal, TRUE, &pStream) == S_OK)
				{
					// 在加载前先销毁旧图像，防止GDI资源泄露
					if (!m_image.IsNull()) m_image.Destroy();

					if (m_image.Load(pStream) == S_OK)
					{
						// 图像加载成功，更新界面
						m_picture.Invalidate(); // 使图片控件无效，触发OnPaint
					}
					pStream->Release();
				}
			}
			// CreateStreamOnHGlobal会接管hGlobal的释放责任
		}

		// 收到一帧后，立即请求下一帧，形成流畅的刷新循环
		CClientController::getInstance()->SendCommandWatch(6);
	}
	break;
	}

	delete pPacket; // !!! 释放从后台线程传递过来的内存 !!!
	return 0;
}

void CWatchDialog::OnPaint()
{
	// 只重绘m_picture控件，而不是整个对话框
	CPaintDC dc(&m_picture);
	if (!m_image.IsNull())
	{
		CRect rect;
		m_picture.GetClientRect(&rect);
		// 将m_image绘制到m_picture控件上
		m_image.Draw(dc.GetSafeHdc(), rect);
	}
	else
	{
		// 如果没有图像，可以绘制一个背景或提示信息
		CDialog::OnPaint();
	}
}