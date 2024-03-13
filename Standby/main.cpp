#include <arpa/inet.h>
#include <signal.h>
#include <unistd.h>
#include <iostream>
#include <cstring>

#define SERV_PORT 9527

// 初始化网络通信资源
int initNetCommRc(int& lfd, struct sockaddr_in& standbyAddr, unsigned short port);

// 监测主用机状态，并执行业务逻辑1
int monitorActive1(int lfd);
	
// 监测主用机状态，并执行业务逻辑2
int monitorActive2(int lfd, pid_t pid);

int main()
{
	// 初始化网络通信资源
	int lfd;
	struct sockaddr_in standbyAddr;
	memset(&standbyAddr, 0 ,sizeof(standbyAddr));

	int ret = initNetCommRc(lfd, standbyAddr, SERV_PORT);
	if(ret == -1)
	{
		std::cout<<"initNetCommRc() err"<<std::endl;	

		return -1;
	}
	
	while(1)
	{
		// 开始监测
		ret = monitorActive1(lfd);
		if(ret == -1)
		{
			// 不在线超过指定时间，启动子进程
			pid_t pid = fork();
			if(pid == -1) 
			{   
				perror("fork() err");   
			
				exit(1);
			}   
			else if(pid == 0)
			{   
				// 子进程逻辑
				int ret = execl("./yoloApp/yolo", "yolo", NULL);    
				if(ret == -1) 
				{   
					perror("execl() err");

					exit(1);
				}   

			}   
			else if(pid > 0)
			{   
				// 父进程逻辑
				monitorActive2(lfd, pid);
			}
		}
	}
}

// 初始化网络通信资源
int initNetCommRc(int& lfd, struct sockaddr_in& standbyAddr, unsigned short port)
{
	// 创建套接字
	lfd = socket(AF_INET, SOCK_DGRAM, 0);
	if(lfd == -1) 
    {   
        std::cout<<"socket() err"<<std::endl;

		return -1;
    }

	// 设置备用机地址结构
	standbyAddr.sin_family = AF_INET;
	standbyAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	standbyAddr.sin_port = htons(SERV_PORT);	
	
	socklen_t standbyLen = sizeof(standbyAddr);

	// fd与备用机地址结构做绑定
	int ret = bind(lfd, (struct sockaddr*)&standbyAddr, standbyLen);
	if(ret == -1) 
    {   
        std::cout<<"bind() err"<<std::endl;

		return -1;
    }

	// 设置套接字监听超时时长
	struct timeval timeout;
	timeout.tv_sec = 10;
	timeout.tv_usec = 0;
	
	setsockopt(lfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

	return 0;
}

// 监测主用机状态，并执行业务逻辑1
int monitorActive1(int lfd)
{
	static int flag = 0;

	// 监视对端是否在线
	while(1)
	{
		char recvBuf[32]{};
		struct sockaddr_in activeAddr;
		socklen_t activeLen = sizeof(activeAddr);
		
		// activeLen为传入传出参数
		int ret = recvfrom(lfd, recvBuf, sizeof(recvBuf), 0 , (struct sockaddr*)&activeAddr , &activeLen);
		if(ret == -1)
		{
			// 接收数据超时
			if(errno == EAGAIN || errno == EWOULDBLOCK)
			{
				std::cout<<"recvfrom() timeout"<<std::endl;

				if(++flag == 3)
				{
					flag = 0;

					break;
				}

				continue;
			}
			else
			{
				// 接收数据出错
				std::cout<<"recvfrom() err"<<std::endl;

				if(++flag == 3)
				{
					flag = 0;

					break;
				}

				continue;
			}

		}

		// 成功接收数据
		if(!strcmp(recvBuf, "active is running!"))
		{
			std::cout<<"recvBuf = "<<recvBuf<<std::endl;

			continue;
		}
	}

	return -1;
}


// 监测主用机状态，并执行业务逻辑2
int monitorActive2(int lfd, pid_t pid)
{
	static int flag = 0;

	while(1)
	{
		char recvBuf[32]{};
		struct sockaddr_in activeAddr;
		socklen_t activeLen = sizeof(activeAddr);

		// activeLen为传入传出参数
		int ret = recvfrom(lfd, recvBuf, sizeof(recvBuf), 0 , (struct sockaddr*)&activeAddr , &activeLen);
		if(ret == -1)
		{
			flag = 0; 

			// 接收数据超时
			if(errno == EAGAIN || errno == EWOULDBLOCK)
			{
				std::cout<<"standby is running, recvfrom() active timeout"<<std::endl;

				continue;
			}
			else
			{
				// 接收数据出错
				std::cout<<"standby is running, recvfrom() active err"<<std::endl;

				continue;
			}
		}
		
		// 成功接收数据
		if(!strcmp(recvBuf, "active is running!"))
		{
			std::cout<<"standby is running, recvBuf = "<<recvBuf<<std::endl;
				
			// 连续三次成功接收数据，不能中断
			if(++flag == 3)
			{
				flag = 0;

				// 关闭子进程
				int ret = kill(pid, SIGTERM);
				if(ret == -1)
				{
					perror("failed to kill child process!");
					
					continue;
				}
				else if(ret == 0)
				{
					std::cout<<"successfully killed child process!"<<std::endl;
					
					return 0;
				}
			}
		}
	}
}
