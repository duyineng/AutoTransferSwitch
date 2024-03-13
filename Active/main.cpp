#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/wait.h>
#include <chrono>
#include <thread>
#include <iostream>

int main(int argc,char* argv[])
{
	// 带参数启动程序提示
	if(argc < 3)
	{
		std::cout<<"eg:./active 192.168.18.10 9527"<<std::endl;

		exit(1);
	}

	// 获取备用机的IP和端口
	char ip[16];
	strncpy(ip, argv[1], sizeof(ip) - 1);
	ip[sizeof(ip) - 1] = '\0';
	unsigned short port = static_cast<unsigned short>(atoi(argv[2]));

	// 创建套接字
	int fd = socket(AF_INET, SOCK_DGRAM, 0);		
	if(fd == -1)
	{
		std::cout<<"socket() err"<<std::endl;

		exit(1);
	}

	// 定义备用机地址结构
	struct sockaddr_in standbyAddr;		
	memset(&standbyAddr, 0, sizeof(standbyAddr));

	// 设置备用机地址结构
	standbyAddr.sin_family = AF_INET;
	inet_pton(AF_INET, ip, &standbyAddr.sin_addr.s_addr);	
	standbyAddr.sin_port = htons(port);		// 主机字节序转网络字节序，即小端序转大端序

	socklen_t standbyLen = sizeof(standbyAddr);

	// 创建子进程
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
		while(1)
		{
			// 睡3秒
			std::this_thread::sleep_for(std::chrono::seconds(3));

			// 获取子进程状态
			pid_t wpidRet = waitpid(pid, NULL, WNOHANG);
			if(wpidRet == 0)
			{
				// 子进程正常运行	
				sendto(fd, "active is running!", sizeof("active is running!"), 0, (struct sockaddr*)&standbyAddr, standbyLen);

				printf("active is running!\n");
			}
			else if(wpidRet == pid)
			{
				// 子进程已死亡
				sendto(fd, "active has crashed!", sizeof("active has crashed!"), 0, (struct sockaddr*)&standbyAddr, standbyLen);

				printf("active has crashed!\n");
			}
			else if(wpidRet == -1)
			{
				// 函数出错
				perror("waitpid() err");

				// 退出程序
				exit(1);
			}
		}
	}

	return 0;
}
