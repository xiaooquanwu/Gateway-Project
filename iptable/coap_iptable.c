#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "list.h"

#include "coap_iptable.h"
#include "zlog.h"

//zlog日志句柄
extern zlog_category_t *zc;

List iplist;
FILE *coapIP_file;

void ParsTxt(void)
{
	char buff[128];
	char *p = NULL;
	char *s = NULL;
	char *e = NULL;
	coapIP *ipdata;
	
	//清空链表
	if(iplist.size>0)
		list_clean(&iplist);

	//打开文件，只读方式
	coapIP_file = fopen("IPtable.txt", "r");
	if(!coapIP_file)
	{
		zlog_error(zc, "open file IPtable error!");
		exit(-1);
	}
	
	while(!feof(coapIP_file))
	{
		//为ipdata数据开辟内存
		ipdata = malloc(sizeof(coapIP));
		if(!ipdata)
		{
			zlog_error(zc, "malloc ipdata error!");
			exit(-1);
		}
		
		memset(ipdata->ip, '\0', sizeof(ipdata->ip));
		memset(ipdata->path, '\0', sizeof(ipdata->path));
		ipdata->coapSocket = 0;
		
		//读取一行
		s = fgets(buff, sizeof(buff), coapIP_file);
		if(s)
		{
			p = strchr(s, 0x2f);
			if(p==NULL)
				continue;
			memcpy(ipdata->ip, s, p-s);

			++p;
		
			e = strchr(p, 0x23);
			if(e==NULL)
				continue;
			memcpy(ipdata->path, p, e-p);
		
			memset(buff, '\0', sizeof(buff));
		
			//将数据加入链表
			list_append(&iplist, ipdata);
		}
	}
	
	//关闭文件
	fclose(coapIP_file);
}

void PrintIPtable(List *list)
{
	ListElmt* ptr = list_head(list);
	
	for(; ptr!=NULL; ptr=ptr->next )
		printf( "ip is %s, path is \"%s\" \r\n", ((coapIP *)(ptr->data))->ip, ((coapIP *)(ptr->data))->path );
}

