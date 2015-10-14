#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <unistd.h>

#include "picocoap.h"
#include "zlog.h"
#include "coap_iptable.h"
#include "uart.h"
#include "main.h"

//zlog日志句柄
extern zlog_category_t *zc;

#define MSG_BUF_LEN		512
#define COAP_PORT		5683

void hex_dump(uint8_t* bytes, size_t len);
void coap_pretty_print(coap_pdu*);

//char coapPayload[128] = "$1234;5678;3;*0002:37.71#;Tue Jun 23 13:04:08 2015&";


void Close_Sig(int sig)
{
	printf("Recv main clos coap client signal %d\r\n", sig);
	
	//退出进程
	exit(0);
	//忽略信号
	(void)signal(COAP_CLOSE_SIGNAL, SIG_IGN);
}

void Fifo_Create_Sig(int sig)
{
	printf("fifo creat signal %d\r\n", sig);
	
	//忽略信号
	(void)signal(FIFO_CREATE_SIGNAL, SIG_IGN);
}

void* CoapClient(void* parg)
{
	socklen_t sin_size;
	struct sockaddr_in addr_remote;
	uint16_t message_id_counter;
	ssize_t bytes_sent;
	ssize_t bytes_recv;
	uint8_t msg_buf[MSG_BUF_LEN];
	int msg_len;
	
	int fifo_fd;
	char fifo[64] = {'\0'};
	
	struct timeval timeout;
	timeout.tv_sec = 3;
	timeout.tv_usec = 0;
	
	coapIP *ipdata = (coapIP *)parg;
	
	//注册信号
	(void)signal(COAP_CLOSE_SIGNAL, Close_Sig);
	(void)signal(FIFO_CREATE_SIGNAL, Fifo_Create_Sig);
	
	// CoAP Message Setup
	uint8_t msg_send_buf[MSG_BUF_LEN];
	coap_pdu msg_send = {msg_send_buf, 0, MSG_BUF_LEN};
	uint8_t msg_recv_buf[MSG_BUF_LEN];
	coap_pdu msg_recv = {msg_recv_buf, 0, MSG_BUF_LEN};
	
	srand(time(NULL));
	message_id_counter = rand();
	
	//创建套接字
	if( (ipdata->coapSocket = socket(AF_INET, SOCK_DGRAM, 0))==-1 )
	{
		zlog_error(zc, "create socket error!");
		exit(-1);
	}
	
	addr_remote.sin_family = AF_INET;
	addr_remote.sin_port = htons(COAP_PORT);
	addr_remote.sin_addr.s_addr = inet_addr( ipdata->ip );
 	memset(addr_remote.sin_zero, 0, 8);
 	
 	zlog_debug(zc, "coap client socket is %d, ip is %s,path is %s", ipdata->coapSocket, ipdata->ip, ipdata->path);
 	
 	//设置接收超时
 	setsockopt(ipdata->coapSocket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(struct timeval));
 	
 	GetfifoName( fifo, sizeof(fifo), getpid() );
	
	//等待fifo创建好的信号
	//pause();
	
	//以只读方式打开FIFO，返回文件描述符fd 
	fifo_fd = open(fifo,O_RDONLY);   
	if( fifo_fd == -1 )
	{
		printf("Open fifo error!\n");
		exit(1);
	}
	
	printf("coap client %d open fifo %s\r\n", getpid(), fifo);
 	 	
	while(1) 
	{
		//清空缓存
		memset(msg_send.buf, '\0', MSG_BUF_LEN);
		msg_send.len = 0;
		
		//从管道读数据
		msg_len = read(fifo_fd, msg_buf, MSG_BUF_LEN);
		//printf("fifo read is %s", msg_buf);
		
		// Build Message
		coap_init_pdu(&msg_send);
		coap_set_version(&msg_send, COAP_V1);
		coap_set_type(&msg_send, CT_CON);
		coap_set_code(&msg_send, CC_GET); // or POST to write
		coap_set_mid(&msg_send, message_id_counter++);
		coap_set_token(&msg_send, rand(), 2);
		coap_add_option(&msg_send, CON_URI_PATH, (uint8_t*)ipdata->path, strlen(ipdata->path));
	
		// to write, set payload:
		coap_set_payload( &msg_send, (uint8_t *)msg_buf, msg_len );
		
		// Send Message
		if ( (bytes_sent = sendto(ipdata->coapSocket, msg_send.buf, msg_send.len, 0, 
		(struct sockaddr*)&addr_remote, sizeof(struct sockaddr_in))) == -1 )
			zlog_error(zc, "Failed to Send Message!");
		else
		{
			//发送成功
			//printf("Sent.\n");
			//coap_pretty_print(&msg_send);
		
			sin_size = sizeof(struct sockaddr);
			// Wait for Response
			bytes_recv = recvfrom(ipdata->coapSocket, (void *)msg_recv.buf, msg_recv.max, 0, (struct sockaddr*)&addr_remote, &sin_size);
			if (bytes_recv < 0) 
			{
				zlog_error(zc, "Failed to socket recv Message!");
				continue;
			}
/*
			msg_recv.len = bytes_recv;

			if(coap_validate_pkt(&msg_recv) == CE_NONE)
			{
				printf("Got Valid CoAP Packet\n");
				if(coap_get_mid(&msg_recv) == coap_get_mid(&msg_send) && coap_get_token(&msg_recv) == coap_get_token(&msg_send))
				{
					printf("Is Response to Last Message\n");
					coap_pretty_print(&msg_recv);
				}
			}
			else
			{
				printf("Received %zi Bytes, Not Valid CoAP\n", msg_recv.len);
				hex_dump(msg_recv.buf, msg_recv.len);
			}
*/		
		}
	}
}

void hex_dump(uint8_t* bytes, size_t len)
{
  size_t i, j;
  for (i = 0; i < len; i+=16){
    printf("  0x%.3zx    ", i);
    for (j = 0; j < 16; j++){
      if (i+j < len)
        printf("%02hhx ", bytes[i+j]);
      else
        printf("%s ", "--");
    }
    printf("   %.*s\n", (int)(16 > len-i ? len-i : 16), bytes+i);
  }
}

void coap_pretty_print(coap_pdu *pdu)
{
  coap_option opt;

  opt.num = 0;

  if (coap_validate_pkt(pdu) == CE_NONE){
      printf(" ------ Valid CoAP Packet (%zi) ------ \n", pdu->len);
      printf("Type:  %i\n",coap_get_type(pdu));
      printf("Code:  %i.%02i\n", coap_get_code_class(pdu), coap_get_code_detail(pdu));
      printf("MID:   0x%X\n", coap_get_mid(pdu));
      printf("Token: 0x%llX\n", coap_get_token(pdu));

      while ((opt = coap_get_option(pdu, &opt)).num != 0){
        if (opt.num == 0)
          break;

        printf("Option: %i\n", opt.num);
        if (opt.val != NULL && opt.len != 0)
          printf(" Value: %.*s (%zi)\n", (int)opt.len, opt.val, opt.len);
      }
      // Note: get_option returns payload pointer when it finds the payload marker
      if (opt.val != NULL && opt.len != 0)
        printf("Payload: %.*s (%zi)\n", (int)opt.len, opt.val, opt.len);
    } else {
      printf(" ------ Non-CoAP Message (%zi) ------ \n", pdu->len);
      hex_dump(pdu->buf, pdu->len);
    }
}
