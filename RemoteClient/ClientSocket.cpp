#include "pch.h"
#include "ClientSocket.h"

CClientSocket* CClientSocket::m_instance = NULL;
CClientSocket::CHelper CClientSocket::m_helper;

CClientSocket* pclient = CClientSocket::getInstance();

std::string GetErrInfo(int wsaErrCode)
{
	std::string ret;
	LPVOID lpMsgBuf = NULL;
	FormatMessage(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
		NULL,
		wsaErrCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	ret = (char*)lpMsgBuf;
	LocalFree(lpMsgBuf);
	return ret;
}

void Dump(BYTE* pData, size_t nSize)
{
	std::string strOut;
	for (size_t i = 0; i < nSize; i++)
	{
		char buf[8] = "";
		if (i > 0 && (i % 16 == 0))strOut += "\n";
		snprintf(buf, sizeof(buf), "%02X ", pData[i] & 0xFF);
		strOut += buf;
	}
	strOut += "\n";
	OutputDebugStringA(strOut.c_str());
}

CClientSocket::CClientSocket(const CClientSocket& ss) {
	m_hThread = INVALID_HANDLE_VALUE;
	m_bAutoClose = ss.m_bAutoClose;
	m_sock = ss.m_sock;
	m_nIP = ss.m_nIP;
	m_nPort = ss.m_nPort;
	std::map<UINT, CClientSocket::MSGFUNC>::const_iterator it = ss.m_mapFunc.begin();
	for (; it != ss.m_mapFunc.end(); it++) {
		m_mapFunc.insert(std::pair<UINT, MSGFUNC>(it->first, it->second));
	}
}

CClientSocket::CClientSocket() :
	m_nIP(INADDR_ANY), m_nPort(0), m_sock(INVALID_SOCKET), m_bAutoClose(true),
	m_hThread(INVALID_HANDLE_VALUE)
{
	m_watchSock = INVALID_SOCKET;
	m_hWatchThread = INVALID_HANDLE_VALUE;
	m_nWatchThreadID = 0;
	m_hWatchWND = NULL;
	m_isWatchConnected = false;

	if (InitSockEnv() == FALSE) {
		MessageBox(NULL, _T("无法初始化套接字环境,请检查网络设置！"), _T("初始化错误！"), MB_OK | MB_ICONERROR);
		exit(0);
	}
	m_eventInvoke = CreateEvent(NULL, TRUE, FALSE, NULL);
	m_hThread = (HANDLE)_beginthreadex(NULL, 0, &CClientSocket::threadEntry, this, 0, &m_nThreadID);
	if (WaitForSingleObject(m_eventInvoke, 100) == WAIT_TIMEOUT) {
		TRACE("网络消息处理线程启动失败了！\r\n");
	}
	CloseHandle(m_eventInvoke);
	m_buffer.resize(BUFFER_SIZE);
	memset(m_buffer.data(), 0, BUFFER_SIZE);
	struct {
		UINT message;
		MSGFUNC func;
	}funcs[] = {
		{WM_SEND_PACK,&CClientSocket::SendPack},
		{0,NULL}
	};
	for (int i = 0; funcs[i].message != 0; i++) {
		if (m_mapFunc.insert(std::pair<UINT, MSGFUNC>(funcs[i].message, funcs[i].func)).second == false) {
			TRACE("插入失败，消息值：%d 函数值:%08X 序号:%d\r\n", funcs[i].message, funcs[i].func, i);
		}
	}
}

bool CClientSocket::InitSocket()
{
	if (m_sock != INVALID_SOCKET) CloseSocket();

	m_sock = socket(PF_INET, SOCK_STREAM, 0);
	if (m_sock == -1) return false;
	
	// 添加端口复用选项
	int opt = 1;
	setsockopt(m_sock, SOL_SOCKET, SO_REUSEADDR, 
              (const char*)&opt, sizeof(opt));
	
	// 禁用 Nagle 算法
	int nodelay = 1;
	setsockopt(m_sock, IPPROTO_TCP, TCP_NODELAY, 
              (const char*)&nodelay, sizeof(nodelay));
	
	sockaddr_in serv_adr;
	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr.sin_family = AF_INET;
	TRACE("addr %08X nIP %08X\r\n", inet_addr("127.0.0.1"), m_nIP);
	serv_adr.sin_addr.s_addr = htonl(m_nIP);
	serv_adr.sin_port = htons(m_nPort);
	if (serv_adr.sin_addr.s_addr == INADDR_NONE) {
		AfxMessageBox("指定的IP地址，不存在！");
		return false;
	}
	
	int ret = connect(m_sock, (sockaddr*)&serv_adr, sizeof(serv_adr));
	if (ret == -1) {
		int error = WSAGetLastError();
		CString msg;
		msg.Format(_T("连接失败！错误码：%d"), error);
		AfxMessageBox(msg);
		TRACE("连接失败：%d %s\r\n", error, GetErrInfo(error).c_str());
		return false;
	}
	
	TRACE("socket init done! sock=%d\r\n", m_sock);
	return true;
}

bool CClientSocket::SendPacket(HWND hWnd, const CPacket& pack, bool isAutoClosed, WPARAM wParam)
{
	UINT nMode = isAutoClosed ? CSM_AUTOCLOSE : 0;
	std::string strOut;
	pack.Data(strOut);
	PACKET_DATA* pData = new PACKET_DATA(strOut.c_str(), strOut.size(), nMode, wParam);
	bool ret = PostThreadMessage(m_nThreadID, WM_SEND_PACK, (WPARAM)pData, (LPARAM)hWnd);
	if (ret == false) {
		delete pData;
	}
	return ret;
}

/*
bool CClientSocket::SendPacket(const CPacket& pack, std::list<CPacket>& lstPacks, bool isAutoClosed)

{
	if (m_sock == INVALID_SOCKET && m_hThread == INVALID_HANDLE_VALUE) {
		/*if (InitSocket() == false)return false;* /
		m_hThread = (HANDLE)_beginthread(&CClientSocket::threadEntry, 0, this);
		TRACE("start thread\r\n");
	}
	m_lock.lock();
	auto pr = m_mapAck.insert(std::pair<HANDLE, std::list<CPacket>&>(pack.hEvent, lstPacks));
	m_mapAutoClosed.insert(std::pair<HANDLE, bool>(pack.hEvent, isAutoClosed));
	m_lstSend.push_back(pack);
	m_lock.unlock();
	TRACE("cmd:%d event %08X thread id%d\r\n", pack.sCmd, pack.hEvent, GetCurrentThreadId());
	WaitForSingleObject(pack.hEvent, INFINITE);
	TRACE("cmd:%d event %08X thread id%d\r\n", pack.sCmd, pack.hEvent, GetCurrentThreadId());
	std::map<HANDLE, std::list<CPacket>&>::iterator it;
	it = m_mapAck.find(pack.hEvent);
	if (it != m_mapAck.end()) {
		m_lock.lock();
		m_mapAck.erase(it);
		m_lock.unlock();
		return true;
	}
	return false;
}
*/
unsigned CClientSocket::threadEntry(void* arg)
{
	CClientSocket* thiz = (CClientSocket*)arg;
	thiz->threadFunc2();
	_endthreadex(0);
	return 0;
}
/*
void CClientSocket::threadFunc()
{
	std::string strBuffer;
	strBuffer.resize(BUFFER_SIZE);
	char* pBuffer = (char*)strBuffer.c_str();
	int index = 0;
	InitSocket();
	while (m_sock != INVALID_SOCKET) {
		if (m_lstSend.size() > 0) {
			TRACE("lstSend size: %d\r\n", m_lstSend.size());
			m_lock.lock();
			CPacket& head = m_lstSend.front();
			m_lock.unlock();
			if (Send(head) == false) {
				TRACE("发送失败！\r\n");
				continue;
			}
			std::map<HANDLE, std::list<CPacket>&>::iterator it;
			it = m_mapAck.find(head.hEvent);
			if (it != m_mapAck.end()) {
				std::map<HANDLE, bool>::iterator it0 = m_mapAutoClosed.find(head.hEvent);
				do {
					int length = recv(m_sock, pBuffer + index, BUFFER_SIZE - index, 0);
					TRACE("recv %d %d\r\n", length, index);
					if (length > 0 || (index > 0)) {
						index += length;
						size_t size = (size_t)index;
						CPacket pack((BYTE*)pBuffer, size);
						if (size > 0) {//TODO:对于文件夹信息获取，文件信息获取可能产生问题
							pack.hEvent = head.hEvent;
							it->second.push_back(pack);
							memmove(pBuffer, pBuffer + size, index - size);
							index -= size;
							TRACE("SetEvent %d %d\r\n", pack.sCmd, it0->second);
							if (it0->second) {
								SetEvent(head.hEvent);
								break;
							}
						}
					}
					else if (length <= 0 && index <= 0) {
						CloseSocket();
						SetEvent(head.hEvent);//等到服务器关闭命令之后，再通知事情完成
						if (it0 != m_mapAutoClosed.end()) {
							TRACE("SetEvent %d %d\r\n", head.sCmd, it0->second);
						}
						else {
							TRACE("异常的情况，没有对应的pair\r\n");
						}
						break;
					}
				} while (it0->second == false);
			}
			m_lock.lock();
			m_lstSend.pop_front();
			m_mapAutoClosed.erase(head.hEvent);
			m_lock.unlock();
			if (InitSocket() == false) {
				InitSocket();
			}
		}
		Sleep(1);
	}
	CloseSocket();
}*/

void CClientSocket::threadFunc2()
{
    TRACE("Entry threadFunc2() thread_id=%d\r\n", GetCurrentThreadId());
    SetEvent(m_eventInvoke);
    MSG msg;
    
    while (::GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        TRACE("Get Message :%08X \r\n", msg.message);
        
        if (m_mapFunc.find(msg.message) != m_mapFunc.end()) {
            TRACE("Processing message %08X\r\n", msg.message);
            (this->*m_mapFunc[msg.message])(msg.message, msg.wParam, msg.lParam);
            TRACE("Message %08X processed\r\n", msg.message);
        }
    }
    
    //  添加退出日志
    TRACE("threadFunc2 exit! GetLastError=%d\r\n", GetLastError());
}

bool CClientSocket::Send(const CPacket& pack)
{
	TRACE("m_sock = %d\r\n", m_sock);
	if (m_sock == -1)return false;
	std::string strOut;
	pack.Data(strOut);
	return send(m_sock, strOut.c_str(), strOut.size(), 0) > 0;
}

void CClientSocket::SendPack(UINT nMsg, WPARAM wParam, LPARAM lParam)
{
	//wParam 结构包含原始二进制数据和连接模式
	//lParam 是目标窗口句柄
    PACKET_DATA data = *(PACKET_DATA*)wParam;
    delete (PACKET_DATA*)wParam;
    HWND hWnd = (HWND)lParam;
    
    size_t nTemp = data.strData.size();
    CPacket current((BYTE*)data.strData.c_str(), nTemp);
    
    if (InitSocket() == true) {
        // 设置接收超时
        DWORD timeout = 5000;
        setsockopt(m_sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
        
        int ret = send(m_sock, (char*)data.strData.c_str(), (int)data.strData.size(), 0);
        if (ret > 0) {
            size_t index = 0;
            std::string strBuffer;
            strBuffer.resize(BUFFER_SIZE);
            char* pBuffer = (char*)strBuffer.c_str();
            
            bool receivedEndMarker = false;
            int packetCount = 0;
            ULONGLONG startTime = GetTickCount64();
            
            while (m_sock != INVALID_SOCKET) {
                // 检查超时
                if (GetTickCount64() - startTime > 5000) {
                    TRACE("Receive timeout after 5 seconds\r\n");
                    CloseSocket();
                    ::SendMessage(hWnd, WM_SEND_PACK_ACK, NULL, -3);
                    return;
                }
                int length = recv(m_sock, pBuffer + index, BUFFER_SIZE - index, 0);
                if (length > 0 || (index > 0)) {
                    index += (size_t)length;
                    while (index > 0) {
                        size_t nLen = index;
                        CPacket pack((BYTE*)pBuffer, nLen);
                        if (nLen > 0) {
                            TRACE("ack pack %d to hWnd %08X count=%d index=%d nLen=%d\r\n",pack.sCmd, hWnd, ++packetCount, index, nLen);
                            ::SendMessage(hWnd, WM_SEND_PACK_ACK, (WPARAM)new CPacket(pack), data.wParam);
                            // 根据命令类型判断是否结束
                            if (pack.sCmd == 2) {
                                // 命令2: 文件列表
                                if (pack.strData.size() >= sizeof(FILEINFO)) {
                                    FILEINFO* pInfo = (FILEINFO*)pack.strData.c_str();
                                    if (pInfo->HasNext == FALSE) {
                                        receivedEndMarker = true;
                                        TRACE("Received end marker for command 2\r\n");
                                    }
                                }
                            }
                            else if (pack.sCmd == 5) {
                                // 命令5: 鼠标操作，不标记为结束
                                TRACE("Received mouse response for command 5, keep connection alive\r\n");
                                receivedEndMarker = false;  //保持连接
                            }
                            else if (pack.sCmd == 6) {
                                // 命令6: 远程监控，不标记为结束
                                TRACE("Received screen capture for command 6, keep connection alive\r\n");
                                receivedEndMarker = false;  //保持连接
                            }
                            else {
                                // 其他命令(1,3,4,7,8,9,1981): 收到响应就结束
                                receivedEndMarker = true;
                                TRACE("Received response for command %d, mark as end\r\n", pack.sCmd);
                            }
                            
                            index -= nLen;
                            memmove(pBuffer, pBuffer + nLen, index);
                        }
                        else {
                            break;
                        }
                    }
                    
                    // 如果是 AUTOCLOSE 且收到结束标志,才关闭连接
                    if ((data.nMode & CSM_AUTOCLOSE) && receivedEndMarker) {
                        TRACE("All data received, closing socket\r\n");
                        CloseSocket();
                        return;
                    }
                }
                else {
                    TRACE("recv failed length %d index %d cmd %d packets=%d\r\n", length, index, current.sCmd, packetCount);
                    CloseSocket();
                    ::SendMessage(hWnd, WM_SEND_PACK_ACK, 
                                (WPARAM)new CPacket(current.sCmd, NULL, 0), 1);
                    return;
                }
            }
        }
        else {
            CloseSocket();
            ::SendMessage(hWnd, WM_SEND_PACK_ACK, NULL, -1);
        }
    }
    else {
        ::SendMessage(hWnd, WM_SEND_PACK_ACK, NULL, -2);
    }

	if (data.nMode & CSM_AUTOCLOSE) {
		CloseSocket();  // 关闭连接（短连接模式）
		return;
	}
}

// 1. 连接远程监控的专用Socket
bool CClientSocket::ConnectWatch(HWND hWnd)
{
	if (m_isWatchConnected) return true; // 如果已经连接，则直接返回

	m_hWatchWND = hWnd; // 保存监控窗口的句柄

	m_watchSock = socket(PF_INET, SOCK_STREAM, 0);
	if (m_watchSock == INVALID_SOCKET) return false;

	sockaddr_in serv_adr;
	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr.sin_family = AF_INET;
	serv_adr.sin_addr.s_addr = htonl(m_nIP);
	serv_adr.sin_port = htons(m_nPort);

	if (connect(m_watchSock, (sockaddr*)&serv_adr, sizeof(serv_adr)) == SOCKET_ERROR) {
		closesocket(m_watchSock);
		m_watchSock = INVALID_SOCKET;
		return false;
	}

	m_isWatchConnected = true;
	// 启动专门的接收线程
	m_hWatchThread = (HANDLE)_beginthreadex(NULL, 0, &CClientSocket::watchThreadEntry, this, 0, &m_nWatchThreadID);

	TRACE("远程监控长连接已建立，接收线程已启动。\r\n");
	return true;
}

// 2. 断开远程监控的Socket
void CClientSocket::DisconnectWatch()
{
	m_isWatchConnected = false; // 设置标志位，让接收线程自然退出
	if (m_watchSock != INVALID_SOCKET) {
		shutdown(m_watchSock, SD_BOTH); // 温和地关闭连接
		closesocket(m_watchSock);
		m_watchSock = INVALID_SOCKET;
	}

	if (m_hWatchThread != INVALID_HANDLE_VALUE) {
		// 等待线程结束，最多等待1秒
		if (WaitForSingleObject(m_hWatchThread, 1000) == WAIT_TIMEOUT) {
			TerminateThread(m_hWatchThread, -1); // 如果无法正常退出，则强制终止
		}
		CloseHandle(m_hWatchThread);
		m_hWatchThread = INVALID_HANDLE_VALUE;
	}
	TRACE("远程监控长连接已断开。\r\n");
}

// 3. 通过专用Socket发送数据
bool CClientSocket::SendWatchPacket(const CPacket& pack)
{
	if (!m_isWatchConnected || m_watchSock == INVALID_SOCKET) return false;

	std::string strOut;
	pack.Data(strOut);
	if (send(m_watchSock, strOut.c_str(), strOut.size(), 0) == SOCKET_ERROR) {
		// 发送失败，可能连接已断开
		DisconnectWatch();
		return false;
	}
	return true;
}

// 4. 监控线程的入口和主体函数
unsigned __stdcall CClientSocket::watchThreadEntry(void* arg)
{
	CClientSocket* thiz = (CClientSocket*)arg;
	thiz->watchThreadFunc();
	_endthreadex(0);
	return 0;
}

void CClientSocket::watchThreadFunc()
{
	std::string strBuffer;
	strBuffer.resize(BUFFER_SIZE);
	size_t index = 0;

	while (m_isWatchConnected) // 只要连接标志位为true，就持续接收
	{
		int length = recv(m_watchSock, (char*)strBuffer.c_str() + index, BUFFER_SIZE - index, 0);

		if (length <= 0) {
			// 接收失败或连接被对方关闭
			TRACE("远程监控连接 recv 失败或对方关闭，错误码: %d\r\n", WSAGetLastError());
			if (m_isWatchConnected) {
				// 如果不是主动断开，就通知UI
				::SendMessage(m_hWatchWND, WM_SEND_PACK_ACK, (WPARAM)new CPacket(0, NULL, 0), -1); // 发送一个错误消息
			}
			break; // 退出循环
		}

		index += length;

		while (index > 0)
		{
			size_t nLen = index;
			CPacket* pack = new CPacket((BYTE*)strBuffer.c_str(), nLen);

			if (nLen > 0) { // 成功解析出一个完整的数据包
				// 将数据包指针通过消息发送给UI线程
				::SendMessage(m_hWatchWND, WM_SEND_PACK_ACK, (WPARAM)pack, 0);
				index -= nLen;
				memmove((void*)strBuffer.c_str(), strBuffer.c_str() + nLen, index);
			}
			else {
				delete pack; // 数据不足以构成一个包，释放内存
				break;       // 跳出内层循环，等待接收更多数据
			}
		}
	}
	DisconnectWatch(); // 确保线程退出时连接被清理
}