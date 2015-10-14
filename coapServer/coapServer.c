#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "microcoap.h"

#define COAPSERV_PORT 5683

#define BUF_SIZE 512

uint8_t buf[BUF_SIZE];
uint8_t scratch_raw[BUF_SIZE];

void CoapServ(void* parg)
{
    int fd;
    struct sockaddr_in servaddr, cliaddr;
    coap_rw_buffer_t scratch_buf = {scratch_raw, sizeof(scratch_raw)};
	
	(void)parg;
	
    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
    {
        printf("Socket Error\r\n");
        exit(0);
    }
	
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(COAPSERV_PORT);
    memset(&(servaddr.sin_zero), 0, sizeof(servaddr.sin_zero));
   
    if ((bind(fd, (struct sockaddr *)&servaddr, sizeof(servaddr))) == -1)
    {
       printf("Bind error\r\n");
       exit(0);      
    }
	
    endpoint_setup();
    printf("Coap Server Start!\r\n");
    
	while(1)
    {
        int n, rc;
        socklen_t len = sizeof(cliaddr);
        coap_packet_t pkt;
        n = recvfrom(fd, buf, sizeof(buf), 0, (struct sockaddr *)&cliaddr, &len);
		
        if (0 != (rc = coap_parse(&pkt, buf, n)))
        {
            printf("Bad packet rc=%d\r\n", rc);
        }
        else
        {
			printf("payload : %s \r\n", pkt.payload.p);
			
			//coapServ reply client
			{
				size_t rsplen = sizeof(buf);
				coap_packet_t rsppkt;
				coap_handle_req(&scratch_buf, &pkt, &rsppkt);
				if (0 != (rc = coap_build(buf, &rsplen, &rsppkt)))
				{
					 printf("coap_build failed rc=%d\n", rc);
				}
				else
				{
					sendto(fd, buf, rsplen, 0, (struct sockaddr *)&cliaddr, sizeof(cliaddr));
				}
			}
        }
		
    }/*while(1)--END*/
}

