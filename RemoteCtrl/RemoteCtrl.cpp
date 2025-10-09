// RemoteCtrl.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include "pch.h"
#include "framework.h"
#include "RemoteCtrl.h"
#include "ServerSocket.h"
#include "Command.h"
#include <conio.h>
#include "CEdoyunQueue.h"
#include <MSWSock.h>
#include "EdoyunServer.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif
//#pragma comment( linker, "/subsystem:windows /entry:WinMainCRTStartup" )
//#pragma comment( linker, "/subsystem:windows /entry:mainCRTStartup" )
//#pragma comment( linker, "/subsystem:console /entry:mainCRTStartup" )
//#pragma comment( linker, "/subsystem:console /entry:WinMainCRTStartup" )
// 唯一的应用程序对象   
//#define INVOKE_PATH _T("C:\\Windows\\SysWOW64\\RemoteCtrl.exe")
#define INVOKE_PATH _T("C:\\Users\\edoyun\\AppData\\Roaming\\Microsoft\\Windows\\Start Menu\\Programs\\Startup\\RemoteCtrl.exe")

CWinApp theApp;
using namespace std;

//业务和通用
bool ChooseAutoInvoke(const CString& strPath) {
	TCHAR wcsSystem[MAX_PATH] = _T("");
	if (PathFileExists(strPath)) {
		return true;
	}
	CString strInfo = _T("该程序只允许用于合法的用途！\n");
	strInfo += _T("继续运行该程序，将使得这台机器处于被监控状态！\n");
	strInfo += _T("如果你不希望这样，请按“取消”按钮，退出程序。\n");
	strInfo += _T("按下“是”按钮，该程序将被复制到你的机器上，并随系统启动而自动运行！\n");
	strInfo += _T("按下“否”按钮，程序只运行一次，不会在系统内留下任何东西！\n");
	int ret = MessageBox(NULL, strInfo, _T("警告"), MB_YESNOCANCEL | MB_ICONWARNING | MB_TOPMOST);
	if (ret == IDYES) {
		//WriteRegisterTable(strPath);
		if (!CEdoyunTool::WriteStartupDir(strPath))
		{
			MessageBox(NULL, _T("复制文件失败，是否权限不足？\r\n"), _T("错误"), MB_ICONERROR | MB_TOPMOST);
			return false;
		}
	}
	else if (ret == IDCANCEL) {
		return false;
	}
	return true;
}

void iocp()
{
	EdoyunServer server;
	server.StartService();

	// ✅ 保持服务运行，直到用户明确退出
	printf("服务已启动，按 'q' 退出...\n");
	while (true) {
		char ch = getchar();
		if (ch == 'q' || ch == 'Q') {
			printf("正在关闭服务...\n");
			break;
		}
		Sleep(100);  // 避免 CPU 空转
	}

	printf("服务已停止\n");
}

void udp_server();
void udp_client(bool ishost = true);
void initsock() {
	WSADATA wsa;
	WSAStartup(MAKEWORD(2, 2), &wsa);
}

void clearsock() {
	WSACleanup();
}



class test:public ThreadFuncBase
{
public:
	test(): m_pool(10), m_vecSendtest(this, (SENDCALLBACK)&test::threadTick2){
		m_send.resize(1024);
		m_send[0] = 'A';
		m_send[1] = 'A';
		m_send[2] = 'A';
		m_send.push_back('str');

	}
	void testqueue()
	{
		m_pool.DispatchWorker(::ThreadWorker(this, (FUNCTYPE)&test::threadTick));
		m_vecSendtest.PushBack(m_send);
		m_pool.Invoke();
		
	}

	int threadTick()
	{
		
		//m_vecSendtest.PopFront();
		return 0;
	}


	int threadTick2()
	{
		
		std::cout << m_vecSendtest.Size() << std::endl;
		return 0;
	}


	std::vector<char> m_send;
	EdoyunThreadPool m_pool;
	EdoyunSendQueue<std::vector<char>> m_vecSendtest;//发送数据队列
};



void testthread()
{
	test s;
	s.testqueue();
	while (1);
}

//int wmain(int argc, TCHAR* argv[]);
//int _tmain(int argc, TCHAR* argv[]);
int main(int argc, char* argv[])
{
	if (!CEdoyunTool::Init())return 1;
	
    // ✅ 方式1：直接在主线程运行
    iocp();
    
    return 0;
}

class COverlapped {
public:
	OVERLAPPED m_overlapped;
	DWORD m_operator;
	char m_buffer[4096];
	COverlapped() {
		m_operator = 0;
		memset(&m_overlapped, 0, sizeof(m_overlapped));
		memset(m_buffer, 0, sizeof(m_buffer));
	}
};



/*
* 1 易用性
*		a 简化参数
*		b 类型适配（参数适配）
*		c 流程简化
* 2 易移植性（高内聚，低耦合）
*		a 核心功能到底是什么？
*		b 业务逻辑是什么？
*/
#include "ENetwork.h"
int RecvFromCB(void* arg, const EBuffer& buffer, ESockaddrIn& addr){
	EServer* server = (EServer*)arg;
	return server->Sendto(addr, buffer);
}
int SendToCB(void* arg, const ESockaddrIn& addr, int ret) {
	EServer* server = (EServer*)arg;
	printf("sendto done!%p\r\n", server);
	return 0;
}
void udp_server()
{
	std::list<ESockaddrIn> lstclients;
	printf("%s(%d):%s\r\n", __FILE__, __LINE__, __FUNCTION__);
	EServerParameter param(
		"127.0.0.1", 20000, ETYPE::ETypeUDP, NULL, NULL, NULL, RecvFromCB, SendToCB
	);
	EServer server(param);
	server.Invoke(&server);
	printf("%s(%d):%s\r\n", __FILE__, __LINE__, __FUNCTION__);
	getchar();
	return;
	
}
void udp_client(bool ishost)
{
	Sleep(2000);
	sockaddr_in server, client;
	int len = sizeof(client);
	server.sin_family = AF_INET;
	server.sin_port = htons(20000);
	server.sin_addr.s_addr = inet_addr("127.0.0.1");
	SOCKET sock = socket(PF_INET, SOCK_DGRAM, 0);
	if (sock == INVALID_SOCKET) {
		printf("%s(%d):%s ERROR!!!\r\n", __FILE__, __LINE__, __FUNCTION__);
		return;
	}
	if (ishost) {//主客户端代码
		printf("%s(%d):%s\r\n", __FILE__, __LINE__, __FUNCTION__);
		EBuffer msg = "hello world!\n";
		int ret = sendto(sock, msg.c_str(), msg.size(), 0, (sockaddr*)&server, sizeof(server));
		printf("%s(%d):%s ret = %d\r\n", __FILE__, __LINE__, __FUNCTION__, ret);
		if (ret > 0) {
			msg.resize(1024);
			memset((char*)msg.c_str(), 0, msg.size());
			ret = recvfrom(sock, (char*)msg.c_str(), msg.size(), 0, (sockaddr*)&client, &len);
			printf("host %s(%d):%s ERROR(%d)!!! ret = %d\r\n", __FILE__, __LINE__, __FUNCTION__, WSAGetLastError(), ret);
			if (ret > 0) {
				printf("%s(%d):%s ip %08X port %d\r\n", __FILE__, __LINE__, __FUNCTION__, client.sin_addr.s_addr, ntohs(client.sin_port));
				printf("%s(%d):%s msg = %d\r\n", __FILE__, __LINE__, __FUNCTION__, msg.size());
			}
			ret = recvfrom(sock, (char*)msg.c_str(), msg.size(), 0, (sockaddr*)&client, &len);
			printf("host %s(%d):%s ERROR(%d)!!! ret = %d\r\n", __FILE__, __LINE__, __FUNCTION__, WSAGetLastError(), ret);
			if (ret > 0) {
				printf("%s(%d):%s ip %08X port %d\r\n", __FILE__, __LINE__, __FUNCTION__, client.sin_addr.s_addr, ntohs(client.sin_port));
				printf("%s(%d):%s msg = %s\r\n", __FILE__, __LINE__, __FUNCTION__, msg.c_str());
			}
		}
	}
	else {//从客户端代码
		printf("%s(%d):%s\r\n", __FILE__, __LINE__, __FUNCTION__);
		std::string msg = "hello world!\n";
		int ret = sendto(sock, msg.c_str(), msg.size(), 0, (sockaddr*)&server, sizeof(server));
		printf("%s(%d):%s ret = %d\r\n", __FILE__, __LINE__, __FUNCTION__, ret);
		if (ret > 0) {
			msg.resize(1024);
			memset((char*)msg.c_str(), 0, msg.size());
			ret = recvfrom(sock, (char*)msg.c_str(), msg.size(), 0, (sockaddr*)&client, &len);
			printf("host %s(%d):%s ERROR(%d)!!! ret = %d\r\n", __FILE__, __LINE__, __FUNCTION__, WSAGetLastError(), ret);
			if (ret > 0) {
				sockaddr_in addr;
				memcpy(&addr, msg.c_str(), sizeof(addr));
				sockaddr_in* paddr = (sockaddr_in*)&addr;
				printf("%s(%d):%s ip %08X port %d\r\n", __FILE__, __LINE__, __FUNCTION__, client.sin_addr.s_addr, client.sin_port);
				printf("%s(%d):%s msg = %d\r\n", __FILE__, __LINE__, __FUNCTION__, msg.size());
				printf("%s(%d):%s ip %08X port %d\r\n", __FILE__, __LINE__, __FUNCTION__, paddr->sin_addr.s_addr, ntohs(paddr->sin_port));
				msg = "hello, i am client!\r\n";
				ret = sendto(sock, (char*)msg.c_str(), msg.size(), 0, (sockaddr*)paddr, sizeof(sockaddr_in));
				printf("%s(%d):%s ip %08X port %d\r\n", __FILE__, __LINE__, __FUNCTION__, paddr->sin_addr.s_addr, ntohs(paddr->sin_port));
				printf("host %s(%d):%s ERROR(%d)!!! ret = %d\r\n", __FILE__, __LINE__, __FUNCTION__, WSAGetLastError(), ret);
			}
		}
	}
	closesocket(sock);
}

void rooyt()
{

}

/*
*  1 思路：做或者实现一个需求的过程
*		确定需求（阶段性的）、选定技术方案（依据技术点）、从框架开发到细节实现（从顶到底）、编译问题、
*       内存泄漏（线程结束，exit函数）、bug排查与功能调试（日志、断点、线程、调用堆栈、内存、监视、局部变量、自动变量）、
*       压力测试（额外写代码的）、功能上线
*  2 设计：易用性、移植性（可复用性）、安全性（线程安全、异常处理、资源处理）、稳定性（鲁棒性）、可扩展性
*       有度、有条件的（可读性、效率、易用性）
*/