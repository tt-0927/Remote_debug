#pragma once
#include <MSWSock.h>
#include "EdoyunThread.h"
#include "CEdoyunQueue.h"
#include <map>
#include "Packet.h"
#include "Command.h"


enum EdoyunOperator {
	ENone,
	EAccept,
	ERecv,
	ESend,
	EError
};
typedef void (*SOCKET_CALLBACK2)(void*, int, std::list<CPacket>&, CPacket&);

class EdoyunServer;
class EdoyunClient;
typedef std::shared_ptr<EdoyunClient> PCLIENT;

class EdoyunOverlapped {
public:
	OVERLAPPED m_overlapped;
	DWORD m_operator;//操作 参见EdoyunOperator
	std::vector<char> m_buffer;//缓冲区
	ThreadWorker m_worker;//处理函数
	EdoyunServer* m_server;//服务器对象
	EdoyunClient* m_client;//对应的客户端
	WSABUF m_wsabuffer;
	
	// ✅ 添加：记录实际传输的字节数
	DWORD m_transferred;
	
	EdoyunOverlapped() : m_transferred(0) {
		memset(&m_overlapped, 0, sizeof(m_overlapped));
	}
	
	virtual ~EdoyunOverlapped() {
		m_buffer.clear();
	}
};
template<EdoyunOperator>class AcceptOverlapped;
typedef AcceptOverlapped<EAccept> ACCEPTOVERLAPPED;
template<EdoyunOperator>class RecvOverlapped;
typedef RecvOverlapped<ERecv> RECVOVERLAPPED;
template<EdoyunOperator>class SendOverlapped;
typedef SendOverlapped<ESend> SENDOVERLAPPED;

class EdoyunClient:public ThreadFuncBase {
public:
	EdoyunClient();

	~EdoyunClient() {
		m_buffer.clear();
		closesocket(m_sock);
		m_recv.reset();
		m_send.reset();
		m_overlapped.reset();
		m_vecSend.Clear();
	}

	void SetOverlapped(PCLIENT& ptr);
	operator SOCKET() {
		return m_sock;
	}
	operator PVOID() {
		return &m_buffer[0];
	}
	operator LPOVERLAPPED();

	operator LPDWORD() {
		return &m_received;
	}
	LPWSABUF RecvWSABuffer();
	LPWSAOVERLAPPED RecvOverlapped();
	LPWSABUF SendWSABuffer();
	LPWSAOVERLAPPED SendOverlapped();
	DWORD& flags() { return m_flags; }
	sockaddr_in* GetLocalAddr() { return &m_laddr; }
	sockaddr_in* GetRemoteAddr() { return &m_raddr; }
	
	size_t GetBufferSize()const { return m_buffer.size(); }
	int Recv();
	int Send();
	int Send(void* buffer, size_t nSize);
	int SendData(std::vector<char>& data);
public:
	SOCKET m_sock;
	DWORD m_received;
	DWORD m_flags;
	std::shared_ptr<ACCEPTOVERLAPPED> m_overlapped;
	std::shared_ptr<RECVOVERLAPPED> m_recv;
	std::shared_ptr<SENDOVERLAPPED> m_send;
	std::vector<char> sendbuf;
	std::vector<char> m_buffer;
	size_t m_used;//已经使用的缓冲区大小
	sockaddr_in m_laddr;
	sockaddr_in m_raddr;
	bool m_isbusy;
	CPacket m_packet;
	CCommand cmd;
	SOCKET_CALLBACK2 m_callback;
	EdoyunSendQueue<std::vector<char>> m_vecSend;//发送数据队列
};

template<EdoyunOperator>
class AcceptOverlapped :public EdoyunOverlapped, ThreadFuncBase
{
public:
	AcceptOverlapped();
	int AcceptWorker();
};

#define BUFFER_SIZE 4096
template<EdoyunOperator>
class RecvOverlapped :public EdoyunOverlapped {
public:
    std::vector<char> m_buffer;
    WSABUF m_wsabuffer;
    
    RecvOverlapped();
    
    // Recv Worker 处理函数
    int RecvWorker() {
        TRACE("[RecvWorker] 开始处理接收数据，socket=%d，this=%p，线程ID=%d\r\n",
              m_client->m_sock, this, GetCurrentThreadId());
        
        // 使用保存的传输字节数
        DWORD nBytes = m_transferred;  // 使用 threadIocp 传递的值
        
        // 打印接收到的数据
        if (nBytes > 0) {
            TRACE("[RecvWorker] 接收到 %d 字节数据\r\n", nBytes);
            CEdoyunTool::Dump((BYTE*)m_buffer.data(), nBytes);
        }
        else {
            TRACE("[RecvWorker] 接收到 0 字节，连接可能已关闭\r\n");
            return -1;  // 不继续处理
        }
        
        // 解析数据包
        std::list<CPacket> lstPackets;
        size_t nUsed = 0;
        
        while (nUsed < nBytes) {
            size_t nParsed = nBytes - nUsed;
            CPacket packet((BYTE*)(m_buffer.data() + nUsed), nParsed);
            
            if (nParsed == 0) {
                TRACE("[RecvWorker] 数据包解析失败，剩余%d字节不足\r\n", nBytes - nUsed);
                break;
            }
            
            TRACE("[RecvWorker] 解析数据包，命令=%d，解析长度=%d\r\n",  packet.sCmd, nParsed);            
            nUsed += nParsed;
            
            // 执行命令
            TRACE("[RecvWorker] 执行命令 %d\r\n", packet.sCmd);
            m_client->cmd.ExcuteCommand(packet.sCmd, lstPackets, packet);
        }
        
        TRACE("[RecvWorker] 命令执行完成，响应包数量=%d\r\n", lstPackets.size());
        
        // 发送响应包
        while (lstPackets.size() > 0) {
            CPacket& response = lstPackets.front();
            m_client->Send((void*)response.Data(), response.Size());
            lstPackets.pop_front();
        }
        
        TRACE("[RecvWorker] 已加入 %d 个响应包到发送队列\r\n", lstPackets.size());
        
        // 重新投递接收操作 (复用同一个缓冲区)
        // 重置 WSABUF 指向原始缓冲区
        m_wsabuffer.buf = m_buffer.data();
        m_wsabuffer.len = m_buffer.size();  // 恢复到完整容量
        
        DWORD dwFlags = 0;
        DWORD dwReceived = 0;
        int ret = WSARecv(m_client->m_sock, &m_wsabuffer, 1, &dwReceived, &dwFlags, &m_overlapped, NULL);
        
        if (ret == SOCKET_ERROR && (WSAGetLastError() != WSA_IO_PENDING)) {
            int error = WSAGetLastError();
            TRACE("[RecvWorker] 重新投递WSARecv失败，错误码=%d\r\n", error);
            return -1;
        }
        
        TRACE("[RecvWorker] 已重新投递接收操作，缓冲区大小=%d\r\n", m_wsabuffer.len);
        return -1;  // 返回-1，告诉线程池清理 worker
    }
};

template<EdoyunOperator>
class SendOverlapped :public EdoyunOverlapped {
public:
    std::vector<char> m_buffer;
    WSABUF m_wsabuffer;
    
    SendOverlapped();
    
    //  Send Worker 处理函数
    int SendWorker() {
        TRACE("[SendWorker] 发送完成通知，socket=%d，this=%p，线程ID=%d\r\n",m_client->m_sock, this, GetCurrentThreadId());
        
        DWORD nBytes = m_transferred;
        TRACE("[SendWorker] 已发送 %d 字节\r\n", nBytes);
        
        if (nBytes > 0) {
            CEdoyunTool::Dump((BYTE*)m_buffer.data(), nBytes);
        }
        
        // 清空客户端的发送缓冲区
        m_client->sendbuf.clear();
        
        TRACE("[SendWorker] 发送缓冲区已清空，队列剩余=%d\r\n", m_client->m_vecSend.Size());
        
        return -1;  // 返回-1，清理 worker
    }
};
typedef SendOverlapped<ESend> SENDOVERLAPPED;

template<EdoyunOperator>
class ErrorOverlapped :public EdoyunOverlapped, ThreadFuncBase
{
public:
	ErrorOverlapped() :m_operator(EError), m_worker(this, &ErrorOverlapped::ErrorWorker) {
		memset(&m_overlapped, 0, sizeof(m_overlapped));
		m_buffer.resize(1024);
	}
	int ErrorWorker() {
		//TODO:
		return -1;
	}
};
typedef ErrorOverlapped<EError> ERROROVERLAPPED;

class EdoyunServer :
	public ThreadFuncBase
{
public:
	EdoyunServer(const std::string& ip = "0.0.0.0", short port = 9527) :m_pool(4) {
		m_hIOCP = INVALID_HANDLE_VALUE;
		m_sock = INVALID_SOCKET;
		m_addr.sin_family = AF_INET;
		m_addr.sin_port = htons(port);
		m_addr.sin_addr.s_addr = inet_addr(ip.c_str());
	}
	~EdoyunServer();
	bool StartService();
	bool NewAccept();
	void BindNewSocket(SOCKET s);
private:
	void CreateSocket();
	int threadIocp();      // 改为循环处理
	std::atomic<bool> m_bRunning;  // 控制线程运行
private:
	EdoyunThreadPool m_pool;
	HANDLE m_hIOCP;
	SOCKET m_sock;
	sockaddr_in m_addr;
	std::map<SOCKET, std::shared_ptr<EdoyunClient>> m_client;
};


