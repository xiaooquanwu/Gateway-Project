#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/wait.h>

#include "uart.h"
#include "zlog.h"
#include "coap_iptable.h"
#include "list.h"
#include "tftpserver.h"
#include "sqlite.h"
#include "microcoap.h"
#include "main.h"

//串口读取线程句柄
pthread_t uartRd_Thread;
//串口线程互斥句柄
pthread_mutex_t uart_mutex = PTHREAD_MUTEX_INITIALIZER;

//从队列中读取数据线程句柄
pthread_t transMsg_Thread;

//zlog日志句柄
zlog_category_t *zc;

//维护IP的链表
extern List iplist;

//数据库
sqlite3 *db;

//tftp进程句柄
pid_t tftp_pid;

//coap server进程句柄
pid_t coap_serv_pid;

//数据缓存队列
Queue coapMsgQ;
sem_t msgQSem;
pthread_mutex_t msgQ_mutex;
void* TransMsgThread(void *parg);

extern void CoapServ(void* parg);
extern void* CoapClient(void* parg);
void CreatCoapclient(List *list);
void UpdateCoapclient(List *list);
void Tftp_Sig(int sig);

void Tftp_Sig(int sig)
{
	printf("Recv tftp signal %d\r\n", sig);
	
	//update coap client
	UpdateCoapclient(&iplist);
	
	(void)signal(TFTP_SIGNAL, Tftp_Sig);
}

int main(int argc, char** argv)
{
	int rc;
	pid_t pid;
	
	//注册信号
	(void)signal(TFTP_SIGNAL, Tftp_Sig);
	
	//初始化缓存队列
	queue_init(&coapMsgQ, free);
	
	//初始化信号量，初始值为0
	rc = sem_init(&msgQSem, 0, 0);
	if(rc == -1)
	{
		printf("init msgQSem error!\r\n");
		exit(-1);
	}
	
	//初始化互斥量
	rc = pthread_mutex_init(&msgQ_mutex, NULL);
	if(rc != 0)
	{
		printf("msgQ_mutex init error!\r\n");
		exit(-1);
	}
	
	
	//打开数据库
	Open_db("CoapMsg.db", &db);
	
	rc = zlog_init("log.conf");
	if (rc) 
	{
		printf("init failed\n");
		return -1;
	}

	zc = zlog_get_category("my_cat");
	if (!zc) 
	{
		printf("get cat fail\n");
		zlog_fini();
		return -2;
	}
	
	zlog_info(zc, "Start!");

	zlog_info(zc, "Uart_Init!");
	//初始化串口
	Uart_Init(115200);

	//创建串口读取数据线程
	rc = pthread_create(&uartRd_Thread, NULL, Uart_ReadThread, NULL);
	if(0!=rc)
	{
		zlog_error(zc, "uartRd_Thread create error!");
		exit(-1);
	}
	else
	{
		zlog_debug(zc, "uartRd_Thread create success!");
	}
	
	//创建分发数据线程
	rc = pthread_create(&transMsg_Thread, NULL, TransMsgThread, NULL);
	if(0!=rc)
	{
		zlog_error(zc, "TransMsgThread create error!");
		exit(-1);
	}
	else
	{
		zlog_debug(zc, "TransMsgThread create success!");
	}
	
	//初始化链表
	list_init( &iplist, free);
	zlog_debug(zc, "list_init success!");
	
	//创建tftp Server进程
	pid = fork();
	if(pid == 0)
	{
		printf("enter tftp server,pid = %d\r\n", getpid());
		zlog_debug(zc, "enter tftp server,pid = %d", getpid());
		tftpd_init(NULL);
	}
	else if(pid<0)
	{
		printf("create tftp server process error!\r\n");
		zlog_error(zc, "create tftp server process error!");
	}
	else if(pid > 0)
	{
		//记录tftp子进程的pid
		tftp_pid = pid;
	}
	
	//创建coapclient任务
	CreatCoapclient(&iplist);
	
	//创建coap Server进程
	pid = fork();
	if(pid == 0)
	{
		printf("enter coap server,pid = %d\r\n", getpid());
		zlog_debug(zc, "enter coap server,pid = %d", getpid());
		CoapServ(NULL);
	}
	else if(pid<0)
	{
		printf("create coap server process error!\r\n");
		zlog_error(zc, "create coap server process error!");
	}
	else if(pid > 0)
	{
		//记录coap server子进程的pid
		coap_serv_pid = pid;
	}
	
	while(1)
	{
		sleep(1);
	}

	return 0;
}

void CreatCoapclient(List *list)
{
	ListElmt* ptr=NULL;
	pid_t pid;
	char fifo[64] = {'\0'};
	
	//读取iptable文件，并建立链表
	ParsTxt();
	//PrintIPtable(&iplist);
	
	for(ptr = list_head(list); ptr!=NULL; ptr=ptr->next )
	{
		//创建coap client进程
		pid = fork();
		if(pid == 0)
		{
			//coap client 子进程
			printf("enter coap client,pid = %d\r\n", getpid());
			zlog_debug(zc, "enter coap client,pid = %d", getpid());
		
			CoapClient(ptr->data);
		}
		else if(pid<0)
		{
			printf("create coap client process error!\r\n");
			zlog_error(zc, "create coap client process error!");
		}
		else if(pid > 0)
		{
			GetfifoName( fifo, sizeof(fifo), pid );
			
			//记录coap client子进程的pid
			((coapIP*)(ptr->data))->pid = pid;
			
			//创建fifo
			if( access(fifo,F_OK)==-1 )
			{
				if( mkfifo(fifo,0777) != 0 )
				{
					printf("Can NOT create fifo file!\n");
					exit(1);
				}
			}
			zlog_debug(zc, "create %s ok!", fifo);
			
			/*
			//发送fifo创建好的信号
			int res = kill( pid, FIFO_CREATE_SIGNAL );
			if(res == -1)
			{
				printf("kill signal error!\r\n");
				zlog_error(zc, "kill fifo create signal error!");
			}
			else
			{
				zlog_debug(zc, "kill fifo create signal success!");
			}
			*/
			//以写方式打开FIFO，返回文件描述符
			((coapIP*)(ptr->data))->fifo_fd = open(fifo, O_WRONLY);   
			if( ((coapIP*)(ptr->data))->fifo_fd == -1 )
			{
				printf("Open fifo error!\n");
				exit(1);
			}
			zlog_debug(zc, "open %s ok!", fifo);
		}
	}
}

void UpdateCoapclient(List *list)
{
	ListElmt* ptr;
	pid_t pid;
	
	if(list->size > 0)
	{
	
		for(ptr = list_head(list); ptr!=NULL; ptr=ptr->next )
		{
			//向子进程发送终止信号
			int res = kill( ((coapIP*)(ptr->data))->pid, COAP_CLOSE_SIGNAL );
			if(res == -1)
			{
				printf("kill signal error!\r\n");
				zlog_error(zc, "kill process stop signal error!");
			}
			else
			{
				zlog_debug(zc, "kill process stop signal success!");
			}
			
			//等待进程退出
			pid = wait(NULL);
			printf("%d process exit!\r\n", pid);
		}	
	}

	CreatCoapclient(list);
}

void* TransMsgThread(void *parg)
{
	char *coapMsg = NULL;
	ListElmt* ptr=NULL;
	
	(void)parg;
	
	while(1)
	{
		//等待信号量
		sem_wait(&msgQSem);
		
		//从队列中取消息
		pthread_mutex_lock(&msgQ_mutex);
		queue_dequeue(&coapMsgQ, (void**)&coapMsg);
		pthread_mutex_unlock(&msgQ_mutex);
		
		if(iplist.size > 0)
		{
			for(ptr = list_head(&iplist); ptr!=NULL; ptr=ptr->next )
			{
				//向管道写数据
				write( ((coapIP*)(ptr->data))->fifo_fd, coapMsg, strlen(coapMsg));
			}
			
			//释放内存
			free(coapMsg);
			coapMsg = NULL;	
		}
	}
	
	return NULL;
}


void GetfifoName(char* nameBuf, int bufLen, int key)
{
	memset(nameBuf, '\0', bufLen);
	sprintf(nameBuf, "/tmp/%d_fifo", key);
}


