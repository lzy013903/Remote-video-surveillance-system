#include <netinet/in.h>
#include "protocol.h"

#define DEF_TCP_SRV_PORT 19868

struct tcp_srv
{
	int sock;					//保存套接字的文件号
};


struct tcp_cli					//保存客户端的信息
{
	int sock;					//accept连接之后的文件号
	struct sockaddr_in addr;	//客户端地址

	struct event_extern* ev_rx;	//保存epoll网络接收注册信息
	struct event_extern* ev_tx;	//保存epoll网络发送注册信息

	char	*buf;           
    int 	len; 

	unsigned char req[FRAME_MAX_SZ];					//用于保存网络接收数据时收到的帧头数据
	unsigned char rsp[FRAME_MAX_SZ + VID_FRAME_MAX_SZ];	//用于保存网络发送数据时收到的帧头数据

};

