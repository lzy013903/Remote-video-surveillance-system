#include <stdio.h>
#include <string.h>
#include <unistd.h>	//文件操作
#include <fcntl.h>  
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <linux/types.h>
#include <main.h>

//每一个映射之后帧缓冲区
struct buf 
{
    void *  start;
    size_t  length;
};

struct cam
{
	struct v4l2_dev *v4_dev; 
    struct buf tran_frm;            //保存用于网络传输的图像
    __u32 tran_frm_max_size;        //指明保存图像最大的值
    //int len;
};

//保存摄像头设备的相关信息
struct v4l2_dev
{
	int fd;
	__u8 name[32];
	__u8 drv[16];
	struct buf *buf;
	struct event_extern *ev;
    struct cam *arg;
};

static void handle_jpeg_proc(void *p,int size,void *arg)       //参数1：图片的地址
{                                                       //参数2：图片的长度
    struct cam *v =arg;         //获取cam的信息
    v->tran_frm.start   = p;
    v->tran_frm.length  = size;
}

//事件处理函数
void cam_handler(int fd,void *arg)
{
	struct v4l2_buffer buf;
	struct v4l2_dev *v = arg;		

	//int file_fd = open("test.jpg", O_RDWR | O_CREAT, 0777);
	
	/*帧出列*/
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    ioctl (v->fd, VIDIOC_DQBUF, &buf);

    //printf("v->buf[buf.index].length = %d \nbuf.bytesused =  %d\n",v->buf[buf.index].length ,buf.bytesused);
    //write(file_fd,v->buf[buf.index].start,v->buf[buf.index].length);
    //handle_jpeg_proc(v->buf[buf.index].start,v->buf[buf.index].length,v->arg);
    
    //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    handle_jpeg_proc(v->buf[buf.index].start,buf.bytesused,v->arg);
    //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

    //close(file_fd); 
    /*buf入列*/
    ioctl(fd, VIDIOC_QBUF, &buf);
}

//摄像头初始化
struct v4l2_dev *v4l2_init()
{
	struct v4l2_capability cap;		   //保存从设备文件中读取的信息
	struct v4l2_dev *v;				   //需要配置的摄像头设备信息
	struct v4l2_format fmt;			   //设置V4L2的图像格式
	struct v4l2_requestbuffers req;	   //需要的帧缓冲区信息
  	struct v4l2_buffer buf; 		   //帧缓冲信息
  	int i;

	//1. 初始化摄像头
  	v = calloc(1,sizeof(struct v4l2_dev));
	v->fd = open ("/dev/video2", O_RDWR | O_NONBLOCK, 0);
	//1.1 获取驱动信息
	ioctl (v->fd, VIDIOC_QUERYCAP, &cap);

	strcpy((char *)v->name , (char *)cap.card);			//复制设备名称
	strcpy((char *)v->drv  , (char *)cap.driver);		//复制设备驱动名称
   
	//1.2 设置图像格式
    fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width       = 640;
    fmt.fmt.pix.height      = 480;
    fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;           //MJPEG???

    ioctl (v->fd, VIDIOC_S_FMT, &fmt) ;

	//1.3 申请图像缓冲区
  	req.count               = 4;
    req.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory              = V4L2_MEMORY_MMAP;
    ioctl (v->fd, VIDIOC_REQBUFS, &req);

	//1.4 将内核空间中的数据映射到用户空间
    v->buf = calloc (4, sizeof (struct buf));
    
    for (i = 0; i < 4; ++i)
    { 
        /*获取图像缓冲区的信息*/
        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory      = V4L2_MEMORY_MMAP;
        buf.index       = i;
 
        ioctl (v->fd, VIDIOC_QUERYBUF, &buf); 
             
        v->buf[i].length = buf.length; 
           
        // 把内核空间中的图像缓冲区映射到用户空间
       	v->buf[i].start = mmap (	NULL ,    //通过mmap建立映射关系
                                    buf.length,
                                    PROT_READ | PROT_WRITE ,
                                    MAP_SHARED ,
                                    v->fd,
                                    buf.m.offset);
    }

	//1.5 图像入队列
   	for (i = 0; i < 4; ++i)
   	{
       	buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
       	buf.memory      = V4L2_MEMORY_MMAP;
       	buf.index       = i; 
       	ioctl (v->fd, VIDIOC_QBUF, &buf);
   	}

	//2. 注册事件到epoll
    v->ev = epoll_event_create(	v->fd , EPOLLIN , cam_handler , v);
	epoll_add_event(srv_main->epfd , v->ev);

	return v;
}

void v4l2_start_capture(struct v4l2_dev *v)
{
	enum v4l2_buf_type type;
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  	ioctl (v->fd, VIDIOC_STREAMON, &type);
}

struct cam *cam_sys_init()
{
	struct cam *cam = calloc(1,sizeof(struct cam));
	//1. 初始化采集子系统
	cam->v4_dev = v4l2_init();
    cam->v4_dev->arg =cam;

	//2. 开始采集
	v4l2_start_capture(cam->v4_dev);

	return cam;
}
                            
static void cam_get_fmt(__u8 *data)
{
    //printf("run cam_get_fmt()\n");

    __u32 fmt = V4L2_PIX_FMT_JPEG;
    memcpy(data, &fmt, 4);
}

static __u32 cam_get_trans_frame(struct cam *v_1, __u8 *data)
{
    //printf("run cam_get_trans_frame(start)\n");
    //printf("v_1 = %p \n",v_1);

    memcpy(data, v_1->tran_frm.start, v_1->tran_frm.length); 

    //printf("run cam_get_trans_frame(over)\n");
    return v_1->tran_frm.length;
}

int process_incoming(struct tcp_cli *c)
{   
    struct cam      *v      = srv_main->cam;
    __u8            *req    = c->req;
    __u8            *rsp    = c->rsp;
    __u8            id      = req[CMD1_POS];
    __u8            status  = ERR_SUCCESS;
    __u8            data[FRAME_DAT_MAX];
    __u32           pos, len, size;

    //printf("run process_incoming()\n    srv_main->cam=%x \n",srv_main->cam);
    //printf("run process_incoming()\n    v=%x \n",v);

    switch(id)
    {
        case REQUEST_ID(VID_GET_FMT):
            //获取到图像格式
            cam_get_fmt(data);
            //构造返回数据
            build_ack(rsp,(TYPE_SRSP << TYPE_BIT_POS) | SUBS_VID,id,4,data);
            //发送返回数据
            net_send(c, rsp, 4 + FRAME_HDR_SZ);
            break;
        case REQUEST_ID(VID_REQ_FRAME):
            //获取图像（一帧）
            pos = FRAME_HDR_SZ + 4;

            //printf("\nright\n");

            //printf("v = %p \n",&v);
            size = cam_get_trans_frame(v, &rsp[pos]);
   
            //构造返回数据
            build_ack(rsp,(TYPE_SRSP << TYPE_BIT_POS) | SUBS_VID,id,4,(__u8*)&size);
            //发送返回数据
            net_send(c, rsp, 4 + FRAME_HDR_SZ + size);
            break;

        default:
            break;
    }
}

