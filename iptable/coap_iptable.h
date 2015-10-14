#ifndef _COAP_IPTABLE_H_H_
#define _COAP_IPTABLE_H_H_

#define COAP_PATH_LEN 	128
#define COAP_IP_LEN 	64

#include "list.h"

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

struct _coapIP
{
	//socket
	int coapSocket;
	
	//path
	char path[COAP_PATH_LEN];
	
	//ip
	char ip[COAP_IP_LEN];
	
	//进程pid
	pid_t pid;
	
	//fifo fd
	int fifo_fd;
};

typedef struct _coapIP coapIP;

void ParsTxt(void);
void PrintIPtable(List *list);

#endif
