#include <main.h>
#include <stdlib.h>
#include <sys/epoll.h>

int build_ack(	unsigned char *rsp,	unsigned char type,	unsigned char id,	
				unsigned char len,	unsigned char* data)
{
	//printf("run build_ack()\n");
	rsp[LEN_POS]  = len;
    rsp[CMD0_POS] = type;
    rsp[CMD1_POS] = id;
	memcpy(&rsp[DAT_POS], data, len);

	return len + FRAME_HDR_SZ;
}

void net_send(struct tcp_cli * c, void *buf, int len)
{
    //printf("run net_send()\n");
    epoll_del_event(srv_main->epfd, c->ev_rx);
    c->buf = buf;
    c->len = len;
    epoll_add_event(srv_main->epfd, c->ev_tx);
}

static void rx_app_handler(int sock , void *arg)
{
	
	struct tcp_cli *c = arg;					//接收参数
	unsigned char* pbuf = &c->req[0];			//重定义信息保存位置

	read(c->sock,pbuf,FRAME_HDR_SZ);			//读取帧头信息

	process_incoming(c);

}

static void tx_app_handler(int sock, void *arg)
{
    //printf("run tx_app_handler()\n");
    struct tcp_cli *c = arg;

    send(sock, c->buf, c->len, 0);

	epoll_del_event(srv_main->epfd, c->ev_tx);
	epoll_add_event(srv_main->epfd, c->ev_rx);
}

struct cam* srv_sys_init()
{
//    printf("run srv_sys_init()\n");
    struct tcp_srv *s;
    struct sockaddr_in addr;					//保存服务器地址
    struct sockaddr_in sin;						//保存初始化的客户端地址
    int len;
    struct tcp_cli *c;							//保存客户端accept后的地址信息

    s = calloc(1, sizeof(struct tcp_srv));
    c = calloc(1, sizeof(struct tcp_cli));

//1. 初始化传输子系统
	//1.1 创建socket
	s->sock = socket(AF_INET, SOCK_STREAM, 0);
	
	//1.2 初始化地址
	addr.sin_addr.s_addr = INADDR_ANY;			//任何IP地址都可以通信
	addr.sin_family = AF_INET;					//协议为IPv4
	addr.sin_port = htons (DEF_TCP_SRV_PORT);	//端口(主机字节序转换为网络字节序)
	
	//1.3 bind地址
	bind(s->sock, (struct sockaddr*)&addr, sizeof(struct sockaddr));

	//1.4 listen
	listen(s->sock,5);							//连接数为5

	//1.5 连接
	c->sock = accept(s->sock, (struct sockaddr*)&sin, &len);	//len为结果参数，指明addr机构所占用的字节个数
	memcpy(&(c->addr),&sin,len);									//len=??????

	//printf("accept is of s->sock = %d\n", c->sock);

//2. 将传输子系统中的事件加入epoll池
    c->ev_rx = epoll_event_create(	c->sock , EPOLLIN  , rx_app_handler , c);
    c->ev_tx = epoll_event_create(	c->sock , EPOLLOUT , tx_app_handler , c);

	epoll_add_event(srv_main->epfd ,c->ev_rx);
	
	srv_main->srv = s;	
	
    return 0;
}