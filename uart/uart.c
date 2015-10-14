#include "uart.h"
#include "zlog.h"
#include "string.h"
#include "coap_iptable.h"
#include "sqlite.h"
#include "main.h"

#define MAX_TIMEOUT 		65535

#define UART_START_FLAG 	'$'
#define UART_END_FLAG 		'&'


//串口设备句柄
mraa_uart_context uart;

//串口接收buf
char uartRdBuf[UART_RD_BUF_SIZE];

//串口线程互斥句柄
extern pthread_mutex_t uart_mutex;

//zlog日志句柄
extern zlog_category_t *zc;

//维护IP的链表
extern List iplist;
//数据库
extern sqlite3 *db;

/*
uart is device ID, uart=0 is oK
uart=1 is console, can not use it
*/
void Uart_Init( int baud )
{
    uart = mraa_uart_init(0);
	if (uart == NULL) 
	{
		zlog_error(zc, "UART failed to setup!");
    }
	
	//清除缓存
	if( mraa_uart_flush(uart)!=MRAA_SUCCESS )
	{
		printf("UART failed to flush!");
		zlog_error(zc, "UART failed to flush!");
	}
	//设置波特率
	if( mraa_uart_set_baudrate(uart, baud)!=MRAA_SUCCESS )
	{
		printf("UART failed to set baudrate!");
		zlog_error(zc, "UART failed to set baudrate!");
	}
	
	//设置8位数据位，无奇偶校验，1位停止位
	if( mraa_uart_set_mode(uart, 8, MRAA_UART_PARITY_NONE, 1)!=MRAA_SUCCESS )
	{
		printf("UART failed to set mode!");
		zlog_error(zc, "UART failed to set mode!");
	}
	//设置RTS DTR
	if( mraa_uart_set_flowcontrol(uart, 0, 0)!=MRAA_SUCCESS )
	{
		printf("UART failed to set flowcontrol!");
		zlog_error(zc, "UART failed to set flowcontrol!");
	}
}

//停用设备
void Uart_Stop(void)
{
	mraa_uart_stop(uart);
}

//串口读数据线程
void* Uart_ReadThread(void *parg)
{
	char ch = '\0';
	int count = 0;
	char* tmpMsg = NULL;
	
	(void)parg;

	while(1)
	{
		
		//如果有数据
		while( mraa_uart_data_available(uart, MAX_TIMEOUT) )
		{
			//读取数据
			//pthread_mutex_lock(&uart_mutex);//加锁
			mraa_uart_read(uart, &ch, 1);//读取一个字节
			//pthread_mutex_unlock(&uart_mutex);//解锁
			
			uartRdBuf[count++] = ch;

			//判断结束符
			if(uartRdBuf[count-1] == UART_END_FLAG)
			{
				if(uartRdBuf[0] == UART_START_FLAG)
				{
					//到这来已经接收一帧完整数据
					//printf("uart recv:%s\r\n", uartRdBuf);

					//往数据库中写数据
					Insert_db(&db, uartRdBuf);

					//将数据加入队列中
					tmpMsg = malloc(sizeof(uartRdBuf));//重新分配内存
					if(tmpMsg==NULL)
					{
						zlog_error(zc, "malloc tmpMsg error!");
						exit(-1);
					}
					else
					{
						//copy数据
						memcpy(tmpMsg, uartRdBuf, sizeof(uartRdBuf));

						//入队
						pthread_mutex_lock(&msgQ_mutex);
						queue_enqueue(&coapMsgQ, tmpMsg);
						pthread_mutex_unlock(&msgQ_mutex);

						//增加信号量
						sem_post(&msgQSem);
					}

					//清空
					tmpMsg = NULL;
					count = 0;
					memset(uartRdBuf, '\0', UART_RD_BUF_SIZE);
				}
				else
				{
					//数据不完整，抛弃
					memset(uartRdBuf, '\0', UART_RD_BUF_SIZE);
					count = 0;
				}
		
			}
			
		}//while(mraa_uart_data_available(uart, MAX_TIMEOUT))--end
		
	}//while(1)--end

	return NULL;
}



