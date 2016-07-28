#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <net.h>
#include <netinet/in.h>

struct server
{
	int epfd;				//指向创建的epoll
	struct cam* cam;		//指向采集子系统
	struct tcp_srv* srv; 	//指向传输子系统
	struct cfg* cfg; 		//指向配置子系统	
};


struct server *srv_main;	//实例化server结构体

