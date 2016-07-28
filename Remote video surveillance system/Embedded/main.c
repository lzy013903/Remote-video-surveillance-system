#include <sys/epoll.h>
#include <stdbool.h>
#include <main.h>

struct event_extern			//使用这个结构体的原因是原来的数据保存在共用体中
{
	int fd;
	bool epolled;			//是否被添加进epoll池子
	uint32_t events;		//事件类型
	void (*handler)(int fd,void *arg);	//发生时的处理函数指针
	void *arg;				//参数

};

int epoll_add_event(int epfd , struct event_extern *ev);
int epoll_del_event(int epfd , struct event_extern* ev);
struct event_extern *epoll_event_create(int fd , uint32_t type , void (*handler)(int , void*), void* arg);

int main()
{
	struct epoll_event events[512];					//epoll池事件记录表
	int fds;										//保存epoll_wait中就绪事件的数量
	int i;
	uint32_t event;									//保存事件类型
	struct event_extern *e;							//事件附加信息
	
	//1.创建Epoll
	srv_main = calloc(1,sizeof(struct server));		//等于malloc加zero函数
	srv_main->epfd = epoll_create(512);

	//2.加入等待事件（具体工作由子系统自行添加）
	srv_main->cam = cam_sys_init();					//指向采集子系统
	srv_main->srv = srv_sys_init();					//指向传输子系统
 
	//3.等待事件发生并处理
	while(1)
	{
		fds = epoll_wait(
							srv_main->epfd	,/*int epfd*/					\
							events			,/*struct epoll_event *events*/	\
							512				,/*int maxevents*/				\
							1000			 /*int timeout*/				\
						);

		for(i=0 ; i<fds ; i++)
		{
			event = events[i].events;							//获取到事件类型
			e = events[i].data.ptr;								//获取到处理函数指针

			if((event & EPOLLIN) &&	(e->events & EPOLLIN))		//读事件
			{
				e->handler(e->fd,e->arg);
			}
			if((event & EPOLLOUT) && (e->events & EPOLLOUT))	//写事件
			{
				e->handler(e->fd,e->arg);
			}
			if((event & EPOLLERR) && (e->events & EPOLLERR))	//异常事件
			{
				e->handler(e->fd,e->arg);
			}
		}
	}
	return -1;
} 

//epoll_event_creat在函数参数传递时调用
int epoll_add_event(int epfd , struct event_extern *ev)
{
	struct epoll_event epv;
	int op;									//操作数

	//1.将这个附加结构挂载到epoll_event中
	epv.data.ptr = ev;						//这是一个联合体
	epv.events = ev->events;				//保存事件类型

	if(ev->epolled)
	{
		op = EPOLL_CTL_MOD;
	}
	else
	{
		op = EPOLL_CTL_ADD;
		ev->epolled = true;
	}

	//2.将epoll_event加入到epoll中
	epoll_ctl(		
				epfd, 		/*int epfd epoll的fd*/		 \
				op, 		/*int op 操作类型*/			 \
				ev->fd, 	/*int fd 附件事件信息的fd*/	 \
				&epv 		/*struct epoll_event *event*/\
			);

	return 0;
}

int epoll_del_event(int epfd , struct event_extern* ev)
{
	epoll_ctl(epfd , EPOLL_CTL_DEL , ev->fd , NULL);
	
	ev->epolled = false;
	
	return 0;
}

struct event_extern *epoll_event_create(	
											int fd , 						\
											uint32_t type , 				\
											void (*handler)(int , void*), 	\
											void* arg						\
										)
{
	struct event_extern *e = calloc(1,sizeof(struct event_extern));
	e->fd = fd;
	e->events = type;
	e->handler = handler;
	e->arg = arg;

	return e;
}