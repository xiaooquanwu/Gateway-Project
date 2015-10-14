#include <unistd.h>
#include <stdio.h>

#include "mraa.h"

#define UART_RD_BUF_SIZE 	512

void Uart_Init( int baud );
void Uart_Stop( void );

//串口读数据线程
void* Uart_ReadThread(void *parg);
