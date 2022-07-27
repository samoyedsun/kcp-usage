#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>

#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <time.h>
#include <semaphore.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <utime.h>
#include <stdarg.h>
#include <sys/socket.h>

#include "ikcp.h"

#include <sys/time.h>

typedef struct
{
    int socket_fd;
    ikcpcb *kcp_ptr;
    struct sockaddr_in addr;
} UserData;

int udp_output(const char *buf, int len, ikcpcb *kcp, void *user)
{
    UserData *ud_ptr = (UserData *)user;

    //发送信息
    socklen_t addrlen = sizeof(ud_ptr->addr);
    int n = sendto(ud_ptr->socket_fd, buf, len, 0, (struct sockaddr *)&ud_ptr->addr, addrlen);
    if (n >= 0)
    {
        //会重复发送，因此牺牲带宽
        printf("udpOutPut-send: 字节 =%d bytes   内容=[%s]\n", n, buf + 24); //24字节的KCP头部
        return n;
    }
    else
    {
        printf("error: %d bytes send, error\n", n);
        return -1;
    }
}

uint64_t iclock64(void)
{
    long s, u;
    uint64_t value;
    struct timeval time;
    gettimeofday(&time, NULL);
    s = time.tv_sec;
    u = time.tv_usec;
    value = ((uint64_t)s) * 1000 + (u / 1000);
    return value;
}

uint32_t iclock()
{
    return (uint32_t)(iclock64() & 0xfffffffful);
}
        
int main(int argc, char* argv[])
{
    UserData ud;
    ikcpcb *pkcp = ikcp_create(0x1, (void *)&ud);
    ikcp_nodelay(pkcp, 1, 10, 2, 1);
	ikcp_wndsize(pkcp, 128, 128);
    pkcp->output = udp_output;
    ud.kcp_ptr = pkcp;

    // 创建udp socket
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    ud.socket_fd = sock;
    
    // 服务器udp网络信息
    ud.addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    ud.addr.sin_port = htons(8899);
    ud.addr.sin_family = AF_INET;


    int hr;
    char send_buf[1024] = "hello udp";
    char recv_buf[1024] = {0};

    while (true)
    {
        ikcp_send(ud.kcp_ptr, send_buf, strlen(send_buf)+1); 
        usleep(10000);
		ikcp_update(ud.kcp_ptr, iclock()); //不是调用一次两次就起作用，要loop调用
        
        
		// 处理虚拟网络：检测是否有udp包从p2->p1
		while (1) {
            struct sockaddr peer_addr;
            socklen_t addrlen = sizeof(peer_addr);
            int n = recvfrom(ud.socket_fd, recv_buf, sizeof(recv_buf),
                MSG_DONTWAIT, (struct sockaddr*)&peer_addr, &addrlen);
			if (n < 0) break;
			// 如果 p1收到udp，则作为下层协议输入到kcp1
			ikcp_input(ud.kcp_ptr, recv_buf, n);
		}

        while (1)
		{
			//kcp将接收到的kcp数据包还原成之前kcp发送的buffer数据
			int n = ikcp_recv(ud.kcp_ptr, recv_buf, sizeof(recv_buf)); //从 buf中 提取真正数据，返回提取到的数据大小
			if (n < 0)						 //检测ikcp_recv提取到的数据
				break;
            printf("real recv n:%d, %s\n", n, recv_buf);
		}
    }

    return 0;
}

