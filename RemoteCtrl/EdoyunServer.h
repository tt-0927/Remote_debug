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
class RecvOverlapped :public EdoyunOverlapped, ThreadFuncBase
{
public:
    RecvOverlapped();
    int RecvWorker() {
        TRACE("[RecvWorker] 开始处理接收数据，socket=%d，this=%p，线程ID=%d\r\n", 
              m_client->m_sock, this, GetCurrentThreadId());
        
        std::list<CPacket> lstPackets;
        
        // ✅ 使用 m_transferred 而不是 m_wsabuffer.len
        size_t nLen = m_transferred;
        TRACE("[RecvWorker] 接收到 %d 字节数据\r\n", nLen);
        
        if (nLen > 0) {
            // ✅ 2. 输出接收到的原始数据
            CEdoyunTool::Dump((BYTE*)m_buffer.data(), nLen);
            
            // ✅ 3. 累加到客户端缓冲区
            if (m_client->m_used + nLen > m_client->m_buffer.size()) {
                TRACE("[RecvWorker] 缓冲区溢出，m_used=%d，nLen=%d\r\n", 
                      m_client->m_used, nLen);
                return -1;
            }
            
            memcpy(m_client->m_buffer.data() + m_client->m_used, 
                   m_buffer.data(), nLen);
            m_client->m_used += nLen;
            
            // ✅ 4. 尝试解析数据包
            size_t parseLen = m_client->m_used;
            m_client->m_packet = CPacket((BYTE*)m_client->m_buffer.data(), parseLen);
            
            TRACE("[RecvWorker] 解析数据包，命令=%d，解析长度=%d\r\n", 
                  m_client->m_packet.sCmd, parseLen);
            
            if (parseLen > 0) {
                // ✅ 5. 移除已解析的数据
                memmove(m_client->m_buffer.data(), 
                       m_client->m_buffer.data() + parseLen, 
                       m_client->m_used - parseLen);
                m_client->m_used -= parseLen;
                
                // ✅ 6. 执行命令
                TRACE("[RecvWorker] 执行命令 %d\r\n", m_client->m_packet.sCmd);
                m_client->cmd.ExcuteCommand(m_client->m_packet.sCmd, 
                                           lstPackets, 
                                           m_client->m_packet);
                
                TRACE("[RecvWorker] 命令执行完成，响应包数量=%d\r\n", lstPackets.size());
                
                // ✅ 7. 将所有响应包加入发送队列
                int queueCount = 0;
                while (lstPackets.size() > 0) {
                    const char* pData = lstPackets.front().Data();
                    int nSize = lstPackets.front().Size();
                    
                    int ret = m_client->Send((void*)pData, nSize);
                    if (ret < 0) {
                        TRACE("[RecvWorker] 加入发送队列失败\r\n");
                        return -1;
                    }
                    queueCount++;
                    lstPackets.pop_front();
                }
                TRACE("[RecvWorker] 已加入 %d 个响应包到发送队列\r\n", queueCount);
            }
            else {
                TRACE("[RecvWorker] 数据包不完整，等待更多数据\r\n");
            }
        }
        else {
            // ✅ 接收到 0 字节，说明连接关闭
            TRACE("[RecvWorker] 接收到 0 字节，连接已关闭\r\n");
            return -1;
        }
        
        // ✅ 8. 重新投递异步接收操作
        // ✅ 注意：不要修改 m_wsabuffer.len！它必须保持为缓冲区大小
        m_wsabuffer.buf = m_buffer.data();
        m_wsabuffer.len = m_buffer.size();  // ✅ 始终是缓冲区大小
        DWORD dwFlags = 0;
        DWORD dwReceived = 0;
        
        int ret = WSARecv(m_client->m_sock, &m_wsabuffer, 1, &dwReceived, 
                         &dwFlags, &m_overlapped, NULL);
        if (ret == SOCKET_ERROR) {
            int error = WSAGetLastError();
            if (error != WSA_IO_PENDING) {
                TRACE("[RecvWorker] WSARecv 失败，错误码=%d\r\n", error);
                return -1;
            }
        }
        
        TRACE("[RecvWorker] 已重新投递接收操作，缓冲区大小=%d\r\n", m_wsabuffer.len);
        return -1;
    }
};

template<EdoyunOperator>
class SendOverlapped :public EdoyunOverlapped, ThreadFuncBase
{
public:
	SendOverlapped();
	int SendWorker() {
		TRACE("[SendWorker] 发送完成通知，socket=%d，this=%p，线程ID=%d\r\n", 
		      m_client->m_sock, this, GetCurrentThreadId());
		
		// ✅ 1. 当前数据已发送完成，清空缓冲区
		if (m_client->sendbuf.size() > 0) {
			TRACE("[SendWorker] 已发送 %d 字节\r\n", m_client->sendbuf.size());
			CEdoyunTool::Dump((BYTE*)m_client->sendbuf.data(), 
                             m_client->sendbuf.size());
			m_client->sendbuf.clear();
		}
		
		// ✅ 2. 检查发送队列，继续发送下一个数据包
		if (m_client->m_vecSend.Size() > 0) {
			TRACE("[SendWorker] 发送队列还有 %d 个包，触发下一次发送\r\n", 
                  m_client->m_vecSend.Size());
			// 注意：不要在这里调用 PopFront，让队列的 threadTick 自动处理
		}
		else {
			TRACE("[SendWorker] 发送队列为空\r\n");
		}
		
		return -1; // 任务完成
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


