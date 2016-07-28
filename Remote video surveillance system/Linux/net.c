#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <linux/types.h>
#include "protocol.h"
#include "wcam.h"

int tcp_init_net(char *ip, int port)
{
    int sock;
    struct sockaddr_in addr;
    sock = socket(AF_INET,SOCK_STREAM,0);

    addr.sin_addr.s_addr = inet_addr(ip);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    
    if (-1 == connect(sock,(struct sockaddr *)&addr,sizeof(struct sockaddr)))
    {
        close(sock);
        return -1;	
    }
    return sock;
}

int make_request(__u8 *buf,enum request req, __u8 *dat)
{
    __u32 hdr = req;
    __u8 *p = buf;
    __u8 len;
    memcpy(p,&hdr,FRAME_HDR_SZ);
    p += FRAME_HDR_SZ;
    len = REQUEST_LEN(req);
    if (len>0 && dat)
    	memcpy(p,dat,len);
    return len+FRAME_HDR_SZ;
}

void * video_thread(void *arg)
{
    int len;
    struct wcam_cli *client = (struct wcam_cli *)arg;
    __u8 *rsp = client->rsp;
    int size;
    while(!(client->stop))
    {
   	    //1. ����ͼ������
        //1.1 ����ͼ������
        len = make_request(client->req, VID_REQ_FRAME, NULL);
        //1.2 ����ͼ������
        send(client->sock,client->req, len ,0);
    	//2. ����ͼ��
    	len = FRAME_HDR_SZ;
    	recv(client->sock,rsp,len,MSG_WAITALL);
    	rsp += FRAME_HDR_SZ;
    	len = client->rsp[LEN_POS];
    	recv(client->sock,rsp,len,MSG_WAITALL);
     	memcpy(&size,rsp,len);
    	rsp += len;
    	recv(client->sock,rsp,size,MSG_WAITALL);
    	//3.�ύͼ�����ʾ��ϵͳ,�ú���ר�����ڻ���ͼ��
        //����1ͼ��ָ��;
        //����2����;
        //����3struct wcam_win����ָ�룬
        //��ָ������ָ���������ģ�Ҳ���ǵײ�Ĵ������ĸ�������
    	draw_video_frame(rsp,size,client->arg);
    	usleep(10000);
    }	
}

void graph_sys_init(struct wcam_win *c)
{
    pthread_t tid;
    struct wcam_cli *client;        //����һ���ṹ
    client = calloc(1,sizeof(struct wcam_cli));
    client->stop = false;
    client->arg = c;                
    client->sock = c->entry_win->sock;
    c->client = client;

    //2. ���������߳�
    pthread_create(&tid,NULL,video_thread,client);
}