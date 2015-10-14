#ifndef _MAIN_H_H_
#define _MAIN_H_H_

#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <semaphore.h>

#include "mraa.h"
#include "queue.h"

#define TFTP_SIGNAL 		(SIGRTMIN+1)
#define COAP_CLOSE_SIGNAL 	(SIGRTMIN+2)
#define FIFO_CREATE_SIGNAL 	(SIGRTMIN+3)

//串口设备句柄
extern mraa_uart_context uart;

//数据缓存队列声明
extern Queue coapMsgQ;
//线程同步信号量
extern sem_t msgQSem;
extern pthread_mutex_t msgQ_mutex;

void GetfifoName(char* nameBuf, int bufLen, int key);

#endif

