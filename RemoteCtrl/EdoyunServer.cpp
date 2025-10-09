#include "pch.h"
#include "EdoyunServer.h"
#include "EdoyunTool.h"
#pragma warning(disable:4407)
template<EdoyunOperator op>
AcceptOverlapped<op>::AcceptOverlapped() {
	m_worker = ThreadWorker(this, (FUNCTYPE)&AcceptOverlapped<op>::AcceptWorker);
	m_operator = EAccept;
	memset(&m_overlapped, 0, sizeof(m_overlapped));
	m_buffer.resize(1024);
	m_server = NULL;
	TRACE("[线程] AcceptOverlapped构造函数：初始化Accept工作线程，this=%p\r\n", this);
}

template<EdoyunOperator op>
int AcceptOverlapped<op>::AcceptWorker() {
	TRACE("[线程] AcceptWorker开始执行，处理客户端连接，this=%p，线程ID=%d\r\n", 
	      this, GetCurrentThreadId());
	
	INT lLength = 0, rLength = 0;
	if (m_client->GetBufferSize() > 0) {
		sockaddr* plocal = NULL, * premote = NULL;
		GetAcceptExSockaddrs(*m_client, 0,
			sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16,
			(sockaddr**)&plocal, &lLength,
			(sockaddr**)&premote, &rLength
		);
		memcpy(m_client->GetLocalAddr(), plocal, sizeof(sockaddr_in));
		memcpy(m_client->GetRemoteAddr(), premote, sizeof(sockaddr_in));
		
		// 绑定到IOCP
		m_server->BindNewSocket(*m_client);
		
		// ✅ 初始化接收缓冲区
		m_client->RecvWSABuffer()->buf = m_client->m_recv->m_buffer.data();
		m_client->RecvWSABuffer()->len = m_client->m_recv->m_buffer.size();
		
		// ✅ 投递异步接收操作
		DWORD dwFlags = 0;
		DWORD dwReceived = 0;
		int ret = WSARecv((SOCKET)*m_client, 
		                 m_client->RecvWSABuffer(), 1, 
		                 &dwReceived, &dwFlags, 
		                 m_client->RecvOverlapped(), NULL);
		
		if (ret == SOCKET_ERROR && (WSAGetLastError() != WSA_IO_PENDING)) {
			int error = WSAGetLastError();
			TRACE("[AcceptWorker] WSARecv 失败，错误码=%d\r\n", error);
			return -1;
		}
		
		TRACE("[AcceptWorker] 已投递接收操作\r\n");
		
		// 创建新的 Accept
		if (!m_server->NewAccept()) {
			TRACE("[线程] AcceptWorker执行失败：无法创建新的Accept操作，this=%p\r\n", this);
			return -2;
		}
	}
	
	TRACE("[线程] AcceptWorker执行完成，this=%p\r\n", this);
	return -1;
}

template<EdoyunOperator op>
inline SendOverlapped<op>::SendOverlapped() {
	m_operator = op;
	m_worker = ThreadWorker(this, (FUNCTYPE)&SendOverlapped<op>::SendWorker);
	memset(&m_overlapped, 0, sizeof(m_overlapped));
	m_buffer.resize(1024 * 256);
	TRACE("[线程] SendOverlapped构造函数：初始化Send工作线程，this=%p\r\n", this);
}

template<EdoyunOperator op>
inline RecvOverlapped<op>::RecvOverlapped() {
	m_operator = op;
	m_worker = ThreadWorker(this, (FUNCTYPE)&RecvOverlapped<op>::RecvWorker);
	memset(&m_overlapped, 0, sizeof(m_overlapped));
	
	// ✅ 缓冲区大小
	m_buffer.resize(1024 * 256);
	
	// ✅ 初始化 WSABUF
	m_wsabuffer.buf = m_buffer.data();
	m_wsabuffer.len = m_buffer.size();  // ✅ 缓冲区容量
	
	m_transferred = 0;  // ✅ 初始化传输字节数
	
	TRACE("[线程] RecvOverlapped构造函数：初始化Recv工作线程，this=%p，缓冲区大小=%d\r\n", 
	      this, m_wsabuffer.len);
}


EdoyunClient::EdoyunClient()
    :m_isbusy(false), m_flags(0), m_used(0),  // ✅ 初始化 m_used
     m_overlapped(new ACCEPTOVERLAPPED()),
     m_recv(new RECVOVERLAPPED()),
     m_send(new SENDOVERLAPPED()),
     m_vecSend(this, (SENDCALLBACK)&EdoyunClient::SendData)
{
    m_callback = &CCommand::RunCommand;
    m_sock = WSASocket(PF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
    m_buffer.resize(1024 * 256);  // ✅ 足够大的缓冲区
    memset(&m_laddr, 0, sizeof(m_laddr));
    memset(&m_raddr, 0, sizeof(m_raddr));
    TRACE("[线程] EdoyunClient构造函数：创建客户端，socket=%d，this=%p\r\n", 
          m_sock, this);
}
void EdoyunClient::SetOverlapped(PCLIENT& ptr) {
	m_overlapped->m_client = ptr.get();
	m_recv->m_client = ptr.get();
	m_send->m_client = ptr.get();
}
EdoyunClient::operator LPOVERLAPPED() {
	return &m_overlapped->m_overlapped;
}

LPWSABUF EdoyunClient::RecvWSABuffer()
{
	return &m_recv->m_wsabuffer;
}

LPWSAOVERLAPPED EdoyunClient::RecvOverlapped()
{
	return &m_recv->m_overlapped;
}

LPWSABUF EdoyunClient::SendWSABuffer()
{
	return &m_send->m_wsabuffer;
}

LPWSAOVERLAPPED EdoyunClient::SendOverlapped()
{
	return &m_send->m_overlapped;
}
#define BUFFER_SIZE 4096
int EdoyunClient::Recv()
{
	static std::mutex m_mutex;
	static std::list<CPacket> lstPackets;
	m_mutex.lock();
	size_t ret = recv(m_sock, m_buffer.data() + m_used, m_buffer.size() - m_used, 0);
	if (ret <= 0)return -1;
	m_used += (size_t)ret;
	CEdoyunTool::Dump((BYTE*)m_buffer.data(), ret);
	ret = m_used;
	m_packet = CPacket((BYTE*)m_buffer.data(), ret);
	if (ret > 0) {
		memmove(m_buffer.data(), m_buffer.data() + ret, BUFFER_SIZE - ret);
		m_used -= ret;
		cmd.ExcuteCommand(m_packet.sCmd, lstPackets, m_packet);
		//m_callback((void*)&cmd, m_packet.sCmd, lstPackets, m_packet);
		while (lstPackets.size() > 0) {
			//Send((void*)lstPackets.front().Data(), lstPackets.front().Size());
			int ret = send(m_sock, lstPackets.front().Data(), lstPackets.front().Size(), 0);
			lstPackets.pop_front();
		}
	}
	m_mutex.unlock();
	return -1;
}

int EdoyunClient::Send(void* buffer, size_t nSize)
{
	std::vector<char> data(nSize);
	memcpy(data.data(), buffer, nSize);

	TRACE("[Send] Queuing data packet, size=%d, queue_size=%d\r\n",
		nSize, m_vecSend.Size());

	if (m_vecSend.PushBack(data)) {
		return 0;
	}
	return -1;
}

int EdoyunClient::SendData(std::vector<char>& data)
{
	// ✅ 检查是否有正在发送的数据
	if (sendbuf.size() > 0) {
		TRACE("[SendData] 上一个数据包还在发送中，等待完成，当前队列=%d\r\n", 
		      m_vecSend.Size());
		return 0; // 返回0表示等待，不从队列移除
	}
	
	if (data.size() == 0) {
		TRACE("[SendData] 数据为空，跳过\r\n");
		return 0;
	}
	
	// ✅ 复制数据到发送缓冲区
	sendbuf.resize(data.size());
	memcpy(sendbuf.data(), data.data(), data.size());
	
	TRACE("[SendData] 准备发送 %d 字节，队列剩余=%d\r\n", 
	      sendbuf.size(), m_vecSend.Size());
	
	// ✅ 设置 WSABUF
	m_send->m_wsabuffer.buf = sendbuf.data();
	m_send->m_wsabuffer.len = sendbuf.size();
	
	// ✅ 投递异步发送操作
	DWORD dwSent = 0;
	int ret = WSASend(m_sock, &m_send->m_wsabuffer, 1, &dwSent, 
	                 0, &m_send->m_overlapped, NULL);
	
	if (ret == SOCKET_ERROR) {
		int error = WSAGetLastError();
		if (error != WSA_IO_PENDING) {
			TRACE("[SendData] WSASend 失败，错误码=%d\r\n", error);
			CEdoyunTool::ShowError();
			sendbuf.clear();
			return -1; // 发送失败，从队列移除
		}
	}
	
	TRACE("[SendData] WSASend 已投递，等待完成通知\r\n");
	return -1; // 成功投递，从队列移除（等待 SendWorker 完成通知）
}

int EdoyunClient::Send()
{
	
	int ret = send(m_sock, sendbuf.data(), sendbuf.size(),0);
	CEdoyunTool::Dump((BYTE*)sendbuf.data(), ret);
	if (ret > 0) sendbuf.clear();
	return -1;
}

bool EdoyunServer::StartService()
{
	TRACE("[线程] StartService：开始启动服务，this=%p\r\n", this);
	CreateSocket();
	
	if (bind(m_sock, (sockaddr*)&m_addr, sizeof(m_addr)) == -1) {
		closesocket(m_sock);
		m_sock = INVALID_SOCKET;
		TRACE("[线程] StartService：绑定端口失败，this=%p\r\n", this);
		return false;
	}
	
	if (listen(m_sock, 3) == -1) {
		closesocket(m_sock);
		m_sock = INVALID_SOCKET;
		TRACE("[线程] StartService：监听失败，this=%p\r\n", this);
		return false;
	}
	
	m_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 4);
	if (m_hIOCP == NULL) {
		closesocket(m_sock);
		m_sock = INVALID_SOCKET;
		m_hIOCP = INVALID_HANDLE_VALUE;
		TRACE("[线程] StartService：创建IOCP失败，this=%p\r\n", this);
		return false;
	}
	
	CreateIoCompletionPort((HANDLE)m_sock, m_hIOCP, (ULONG_PTR)this, 0);
	
	// ✅ 设置运行标志
	m_bRunning = true;
	
	TRACE("[线程] StartService：启动线程池，创建4个工作线程，this=%p\r\n", this);
	m_pool.Invoke();
	
	TRACE("[线程] StartService：分配IOCP监听线程(第1个线程)，this=%p\r\n", this);
	m_pool.DispatchWorker(ThreadWorker(this, (FUNCTYPE)&EdoyunServer::threadIocp));
	
	if (!NewAccept()) {
		TRACE("[线程] StartService：创建Accept操作失败，this=%p\r\n", this);
		return false;
	}
	
	TRACE("[线程] StartService：服务启动成功，线程池已就绪(共4个工作线程)，this=%p\r\n", this);
	return true;
}

void EdoyunServer::CreateSocket()
{
	WSADATA WSAData;
	WSAStartup(MAKEWORD(2, 2), &WSAData);
	m_sock = WSASocket(PF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	int opt = 1;
	setsockopt(m_sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
	TRACE("[线程] CreateSocket：创建监听Socket，socket=%d，this=%p\r\n", m_sock, this);
}

bool EdoyunServer::NewAccept() {
    PCLIENT pClient(new EdoyunClient());
    pClient->SetOverlapped(pClient);
    SOCKET clientSock = *pClient;
    
    TRACE("[线程] NewAccept：准备接受新连接，客户端socket=%d，this=%p\r\n", 
          clientSock, this);
    
    // ✅ 正确的 AcceptEx 调用
    BOOL ret = AcceptEx(
        m_sock,
        clientSock,
        pClient->m_overlapped->m_buffer.data(),
        0,  // 不立即接收数据
        sizeof(sockaddr_in) + 16,
        sizeof(sockaddr_in) + 16,
        NULL,  // ✅ IOCP 模式必须为 NULL
        (LPOVERLAPPED)(*pClient)
    );
    
    if (!ret) {
        int error = WSAGetLastError();
        TRACE("[线程] NewAccept：AcceptEx返回 FALSE，错误码=%d\r\n", error);
        
        if (error != WSA_IO_PENDING) {
            TRACE("[线程] NewAccept：AcceptEx失败，this=%p\r\n", this);
            return false;
        }
    }
    
    // ✅ 加入客户端列表
    m_client.insert(std::pair<SOCKET, PCLIENT>(clientSock, pClient));
    
    TRACE("[线程] NewAccept：Accept操作已投递，等待客户端连接，this=%p\r\n", this);
    return true;
}

void EdoyunServer::BindNewSocket(SOCKET s)
{
	CreateIoCompletionPort((HANDLE)s, m_hIOCP, (ULONG_PTR)this, 0);
	TRACE("[线程] BindNewSocket：将socket=%d绑定到IOCP，this=%p\r\n", s, this);
}

// ✅ 修改后的 threadIocp - 永久循环
int EdoyunServer::threadIocp()
{
    TRACE("[线程] threadIocp：IOCP监听线程启动，this=%p，线程ID=%d\r\n", 
          this, GetCurrentThreadId());
    
    // ✅ 循环处理所有 IOCP 事件
    while (m_bRunning) {
        DWORD tranferred = 0;
        ULONG_PTR CompletionKey = 0;
        OVERLAPPED* lpOverlapped = NULL;
        
        TRACE("[线程] threadIocp：等待IO完成通知...\r\n");
        
        BOOL ret = GetQueuedCompletionStatus(m_hIOCP, &tranferred, &CompletionKey, 
                                            &lpOverlapped, INFINITE);
        
        if (!ret) {
            int error = WSAGetLastError();
            TRACE("[线程] threadIocp：GetQueuedCompletionStatus 失败，错误码=%d\r\n", error);
            
            if (lpOverlapped != NULL) {
                EdoyunOverlapped* pOverlapped = CONTAINING_RECORD(lpOverlapped, 
                                                                 EdoyunOverlapped, 
                                                                 m_overlapped);
                TRACE("[线程] threadIocp：操作类型=%d 失败\r\n", pOverlapped->m_operator);
                
                // ✅ 如果是接收失败且字节数为0，说明连接关闭
                if (pOverlapped->m_operator == ERecv && tranferred == 0) {
                    TRACE("[线程] threadIocp：客户端连接已关闭\r\n");
                    // 清理客户端
                    if (pOverlapped->m_client) {
                        SOCKET sock = pOverlapped->m_client->m_sock;
                        auto it = m_client.find(sock);
                        if (it != m_client.end()) {
                            m_client.erase(it);
                            TRACE("[线程] threadIocp：已移除客户端 socket=%d\r\n", sock);
                        }
                    }
                }
            }
            continue;  // 继续处理下一个事件
        }
        
        // ✅ 检查退出信号
        if (CompletionKey == 0) {
            TRACE("[线程] threadIocp：收到退出信号\r\n");
            break;
        }
        
        EdoyunOverlapped* pOverlapped = CONTAINING_RECORD(lpOverlapped, 
                                                         EdoyunOverlapped, 
                                                         m_overlapped);
        
        TRACE("[线程] threadIocp：收到IO完成通知，操作类型=%d，传输字节=%d\r\n", 
              pOverlapped->m_operator, tranferred);
        
        pOverlapped->m_server = this;
        // ✅ 正确传递：将字节数保存到 m_transferred
        pOverlapped->m_transferred = tranferred;
        
        switch (pOverlapped->m_operator) {
        case EAccept:
        {
            ACCEPTOVERLAPPED* pOver = (ACCEPTOVERLAPPED*)pOverlapped;
            TRACE("[线程] threadIocp：分配Accept处理线程，this=%p\r\n", this);
            m_pool.DispatchWorker(pOver->m_worker);
        }
        break;
        case ERecv:
        {
            RECVOVERLAPPED* pOver = (RECVOVERLAPPED*)pOverlapped;
            // ✅ 不再修改 m_wsabuffer.len！
            TRACE("[线程] threadIocp：分配Recv处理线程，接收=%d字节，this=%p\r\n", 
                  tranferred, this);
            
            // ✅ 如果接收到 0 字节，说明连接关闭
            if (tranferred == 0) {
                TRACE("[线程] threadIocp：客户端连接关闭，socket=%d\r\n", 
                      pOver->m_client->m_sock);
                // 不再投递 worker，直接清理
                SOCKET sock = pOver->m_client->m_sock;
                auto it = m_client.find(sock);
                if (it != m_client.end()) {
                    m_client.erase(it);
                    TRACE("[线程] threadIocp：已移除客户端 socket=%d\r\n", sock);
                }
            }
            else {
                m_pool.DispatchWorker(pOver->m_worker);
            }
        }
        break;
        case ESend:
        {
            SENDOVERLAPPED* pOver = (SENDOVERLAPPED*)pOverlapped;
            TRACE("[线程] threadIocp：分配Send处理线程，发送=%d字节，this=%p\r\n", 
                  tranferred, this);
            m_pool.DispatchWorker(pOver->m_worker);
        }
        break;
        default:
            TRACE("[线程] threadIocp：未知操作类型=%d\r\n", pOverlapped->m_operator);
            break;
        }
        
        TRACE("[线程] threadIocp：本次IO事件处理完成，继续等待下一个事件\r\n");
    }
    
    TRACE("[线程] threadIocp：IOCP监听线程退出，this=%p\r\n", this);
    return -1;  // 返回-1，清理工作线程的任务
}

// ✅ 析构函数中停止 IOCP 线程
EdoyunServer::~EdoyunServer()
{
    TRACE("[线程] EdoyunServer析构函数：开始停止服务和线程池，this=%p\r\n", this);
    
    // ✅ 设置停止标志
    m_bRunning = false;
    
    // ✅ 向IOCP投递退出信号
    if (m_hIOCP != NULL && m_hIOCP != INVALID_HANDLE_VALUE) {
        PostQueuedCompletionStatus(m_hIOCP, 0, 0, NULL);
    }
    
    // 等待线程池停止
    m_pool.Stop();
    
    closesocket(m_sock);
    
    std::map<SOCKET, PCLIENT>::iterator it = m_client.begin();
    for (; it != m_client.end(); it++) {
        it->second.reset();
    }
    m_client.clear();
    
    if (m_hIOCP != NULL && m_hIOCP != INVALID_HANDLE_VALUE) {
        CloseHandle(m_hIOCP);
    }
    
    WSACleanup();
    TRACE("[线程] EdoyunServer析构函数：服务已停止，线程池已关闭，this=%p\r\n", this);
}
