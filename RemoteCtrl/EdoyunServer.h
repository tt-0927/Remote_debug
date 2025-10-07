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

		//m_client->Recv（）

		std::list<CPacket> lstPackets;
		//m_mutex.lock();
		size_t ret = recv(m_client->m_sock, m_buffer.data() + m_client->m_used, m_buffer.size() - m_client->m_used, 0);
		if (ret <= 0)return -1;
		m_client->m_used += (size_t)ret;
		CEdoyunTool::Dump((BYTE*)m_buffer.data(), ret);
		ret = m_client->m_used;
		m_client->m_packet = CPacket((BYTE*)m_buffer.data(), ret);
		if (ret > 0) {
			memmove(m_buffer.data(), m_buffer.data() + ret, BUFFER_SIZE - ret);
			m_client->m_used -= ret;
			m_client->cmd.ExcuteCommand(m_client->m_packet.sCmd, lstPackets, m_client->m_packet);
			//m_callback((void*)&cmd, m_packet.sCmd, lstPackets, m_packet);
			while (lstPackets.size() > 0) {
				m_client->Send((void*)lstPackets.front().Data(), lstPackets.front().Size());
				//int ret = send(m_client->m_sock, lstPackets.front().Data(), lstPackets.front().Size(), 0);
				lstPackets.pop_front();
			}
		}
		return -1;
		//m_mutex.unlock();
		//int ret = m_client->Recv();
		//return ret;
	}
	
};

template<EdoyunOperator>
class SendOverlapped :public EdoyunOverlapped, ThreadFuncBase
{
public:
	SendOverlapped();
	int SendWorker() {
		//TODO:
		/*
		* 1 Send可能不会立即完成
		*/
		//int ret = m_client->Send();
		int ret = send(m_client->m_sock, m_client->sendbuf.data(), m_client->sendbuf.size(), 0);

		if (ret > 0)  m_client->sendbuf.clear();
		return -1;
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
	int threadIocp();
private:
	EdoyunThreadPool m_pool;
	HANDLE m_hIOCP;
	SOCKET m_sock;
	sockaddr_in m_addr;
	std::map<SOCKET, std::shared_ptr<EdoyunClient>> m_client;
};

