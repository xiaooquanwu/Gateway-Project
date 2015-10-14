/* tftpserver.c */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "tftpserver.h"
#include "tftputils.h"
#include "zlog.h"

#include "main.h"

#define TFTP_PORT 69
char tftpRecvBuffer[1024];

//zlog日志句柄
extern zlog_category_t *zc;

typedef struct
{
  int op;    /* RRQ/WRQ */

  /* last block read */
  char data[TFTP_DATA_PKT_LEN_MAX];
  uint32_t  data_len;

  /* destination ip:port */
	struct sockaddr_in to;
		
  /* next block number */
  int block;

  /* total number of bytes transferred */
  int tot_bytes;

  /* timer interrupt count when last packet was sent */
  /* this should be used to resend packets on timeout */
  unsigned long long last_time;

}tftp_connection_args;

/* tftp_errorcode error strings */
char *tftp_errorcode_string[] = {
                                  "not defined",
                                  "file not found",
                                  "access violation",
                                  "disk full",
                                  "illegal operation",
                                  "unknown transfer id",
                                  "file already exists",
                                  "no such user",
                                };

FILE *file;


/**
  * @brief  sends a TFTP message
  */
int tftp_send_message(int sock, struct sockaddr_in *to, char *buf, unsigned short buflen)
{
	int err;
	
	//  int lwip_sendto(int s, const void *data, size_t size, int flags, const struct sockaddr *to, socklen_t tolen)
	err = sendto(sock, buf, buflen, 0, (struct sockaddr *)to, sizeof(struct sockaddr));

	return err;
}


/**
  * @brief construct an error message into buf
  * @param buf: pointer on buffer where to create the message  
  * @param err: error code of type tftp_errorcode
  * @retval 
  */
int tftp_construct_error_message(char *buf, tftp_errorcode err)
{
  int errorlen;
  /* Set the opcode in the 2 first bytes */
  tftp_set_opcode(buf, TFTP_ERROR);
  /* Set the errorcode in the 2 second bytes  */
  tftp_set_errorcode(buf, err);
  /* Set the error message in the last bytes */
  tftp_set_errormsg(buf, tftp_errorcode_string[err]);
  /* Set the length of the error message  */
  errorlen = strlen(tftp_errorcode_string[err]);

  /* return message size */
  return 4 + errorlen + 1;
}


/**
  * @brief Sends a TFTP error message
  */
int tftp_send_error_message(int sock, struct sockaddr_in *to, tftp_errorcode err)
{
  char buf[512];
  int error_len;

  /* construct error */
  error_len = tftp_construct_error_message(buf, err);
  /* send error message */
  return tftp_send_message(sock, to, buf, error_len);
}


/**
  * @brief  Sends TFTP data packet
  */
int tftp_send_data_packet(int sock, struct sockaddr_in *to, unsigned short block,
                          char *buf, int buflen)
{
	char packet[TFTP_DATA_PKT_LEN_MAX]; /* (512+4) bytes */

	/* Set the opcode 3 in the 2 first bytes */
	tftp_set_opcode(packet, TFTP_DATA);
	/* Set the block numero in the 2 second bytes */
	tftp_set_block(packet, block);
	/* Set the data message in the n last bytes */
	tftp_set_data_message(packet, buf, buflen);
	
	//printf("buf is %s, buflen is %d, block is %d\r\n", buf, buflen, block);

	/* Send DATA packet */
	return tftp_send_message(sock, to, packet, buflen + 4);
}

/**
  * @brief  Sends TFTP ACK packet
  */
int tftp_send_ack_packet(int sock, struct sockaddr_in *to, unsigned short block)
{

	/* create the maximum possible size packet that a TFTP ACK packet can be */
	char packet[TFTP_ACK_PKT_LEN];

	/* define the first two bytes of the packet */
	tftp_set_opcode(packet, TFTP_ACK);

	/* Specify the block number being ACK'd.
	* If we are ACK'ing a DATA pkt then the block number echoes that of the DATA pkt being ACK'd (duh)
	* If we are ACK'ing a WRQ pkt then the block number is always 0
	* RRQ packets are never sent ACK pkts by the server, instead the server sends DATA pkts to the
	* host which are, obviously, used as the "acknowledgement" */
	tftp_set_block(packet, block);

	return tftp_send_message(sock, to, packet, TFTP_ACK_PKT_LEN);
}


/**
  * @brief Cleanup after end of TFTP read operation
  */
void tftp_cleanup_rd(int sock, tftp_connection_args *args)
{
	/* close the filesystem */
	fclose(file);

	/* Free the tftp_connection_args structure reserverd for */
	free(args);
	
	close(sock);
}

/**
  * @brief Cleanup after end of TFTP write operation
  * @param upcb: pointer on udp pcb
  * @param  args: pointer on a structure of type tftp_connection_args
  * @retval None
  */
void tftp_cleanup_wr(int sock, tftp_connection_args *args)
{
	/* close the filesystem */
	fclose(file);

	/* Free the tftp_connection_args structure reserverd for */
	free(args);
	close(sock);
}

/**
  * @brief  sends next data block during TFTP READ operation
  */
void tftp_send_next_block(int sock, tftp_connection_args *args,
                          struct sockaddr_in *to)
{
	/* Function to read 512 bytes from the file to send (IptableFile), put them
	* in "args->data" and return the number of bytes read */
	args->data_len = fread((uint8_t*)args->data,  1, TFTP_DATA_LEN_MAX, file);
	/*   NOTE: We need to send data packet even if args->data_len = 0*/
	
	//printf("fread file is %s\r\n", (uint8_t*)args->data);
	
	/* sEndTransferthe data */
	tftp_send_data_packet(sock, to, args->block, args->data, args->data_len);

}

/**
  * @brief  processes tftp read operation
  * @param  upcb: pointer on udp pcb 
  * @param  to: pointer on remote IP address
  * @param  to_port: pointer on remote udp port
  * @param  FileName: pointer on filename to be read
  * @retval error code
  */
int tftp_process_read(int sock, struct sockaddr_in *to, char* FileName)
{
	socklen_t sin_size;                // to store struct size
	tftp_connection_args *args = NULL;
	
	/* If Could not open the file which will be transmitted  */
	file = fopen((const char*)FileName, "r");
	if (!file)
	{
		tftp_send_error_message(sock, to, TFTP_ERR_FILE_NOT_FOUND);

		tftp_cleanup_rd(sock, args);

		return 0;
	}

	args = (tftp_connection_args *)malloc( sizeof(tftp_connection_args) );
	
	/* If we aren't able to allocate memory for a "tftp_connection_args" */
	if (!args)
	{
		/* unable to allocate memory for tftp args  */
		tftp_send_error_message(sock, to, TFTP_ERR_NOTDEFINED);

		/* no need to use tftp_cleanup_rd because no "tftp_connection_args" struct has been malloc'd   */
		tftp_cleanup_rd(sock, args);

		return 0;
	}

	/* initialize connection structure  */
	args->op = TFTP_RRQ;
	args->to = *to;
	args->block = 1; /* block number starts at 1 (not 0) according to RFC1350  */
	args->tot_bytes = 0;
	
	/* initiate the transaction by sending the first block of data,
	further blocks will be sent when ACKs are received */
	tftp_send_next_block(sock, args, to);
	
	sin_size = sizeof(struct sockaddr);
	memset(tftpRecvBuffer, '\0', sizeof(tftpRecvBuffer));
	
	recvfrom(sock, tftpRecvBuffer, sizeof(tftpRecvBuffer), 0, (struct sockaddr *)&to, &sin_size);
	if (tftp_is_correct_ack(tftpRecvBuffer, args->block))
	{
		args->block++;
	}
	else
	{
		/* we did not receive the expected ACK, so
		do not update block #. This causes the current block to be resent. */
		tftp_cleanup_rd(sock, args);
		
	}
	
	if (args->data_len < TFTP_DATA_LEN_MAX)
	{
		tftp_cleanup_rd(sock, args);
	}

	/* if the whole file has not yet been sent then continue  */
//	tftp_send_next_block(sock, args, to);

	return 1;
}

/**
  * @brief  processes tftp write operation
  */
int tftp_process_write(int sock, struct sockaddr_in *to, char* FileName)
{
	socklen_t sin_size;                // to store struct size
	tftp_connection_args *args = NULL;
	int recvn_num = 0;
	int n = 0;
	int res = 0;
	
	/* If Could not open the file which will be transmitted  */
	file = fopen((const char*)FileName, "w+b");
	if (!file)
	{
		//add log
		printf("open %s file err!\r\n", FileName);
		
		tftp_send_error_message(sock, to, TFTP_ERR_FILE_NOT_FOUND);

		tftp_cleanup_rd(sock, args);

		return 0;
	}

	args = (tftp_connection_args *)malloc( sizeof(tftp_connection_args) );
	if (!args)
	{
		/* unable to allocate memory for tftp args  */
		tftp_send_error_message(sock, to, TFTP_ERR_NOTDEFINED);

		/* no need to use tftp_cleanup_rd because no "tftp_connection_args" struct has been malloc'd   */
		tftp_cleanup_rd(sock, args);

		return 0;
	}

	/* initialize connection structure  */
	args->op = TFTP_RRQ;
	args->to = *to;
	/* the block # used as a positive response to a WRQ is _always_ 0!!! (see RFC1350)  */
	args->block = 0;
	args->tot_bytes = 0;
	
	/*send ack */
	tftp_send_ack_packet(sock, to, args->block);
	
	sin_size = sizeof(struct sockaddr);
	memset(tftpRecvBuffer, '\0', sizeof(tftpRecvBuffer));
	//接收数据
	recvn_num = recvfrom(sock, tftpRecvBuffer, sizeof(tftpRecvBuffer), 0, (struct sockaddr *)to, &sin_size);
	
	zlog_debug(zc, "recvbuf is %s, recvnum is %d", 
			tftpRecvBuffer+TFTP_DATA_PKT_HDR_LEN, recvn_num);
	
	/* Does this packet have any valid data to write? */
	if ( (recvn_num > TFTP_DATA_PKT_HDR_LEN) && (tftp_extract_block(tftpRecvBuffer) == (args->block + 1)) )
	{
		/* write the received data to the file */
		n = fwrite( (char*)tftpRecvBuffer+TFTP_DATA_PKT_HDR_LEN, sizeof(char), recvn_num-TFTP_DATA_PKT_HDR_LEN, file );
		if (n <= 0)
		{
			/* unable to allocate memory for tftp args  */
			tftp_send_error_message(sock, to, TFTP_ERR_FILE_NOT_FOUND);

			/* no need to use tftp_cleanup_rd because no "tftp_connection_args" struct has been malloc'd   */
			tftp_cleanup_rd(sock, args);
		}

		/* update our block number to match the block number just received */
		args->block++;
		/* update total bytes  */
		(args->tot_bytes) += (recvn_num - TFTP_DATA_PKT_HDR_LEN);
	}
	else if (tftp_extract_block(tftpRecvBuffer) == (args->block + 1))
	{
			/* update our block number to match the block number just received  */
			args->block++;
	}
	
	//send ack
	tftp_send_ack_packet(sock, to, args->block);
	
	/* If the last write returned less than the maximum TFTP data pkt length,
	* then we've received the whole file and so we can quit (this is how TFTP
	* signals the end of a transfer!)
	*/
	if (recvn_num < TFTP_DATA_PKT_LEN_MAX)
	{
		tftp_cleanup_wr(sock, args);
	}
	
	
	{
		//向父进程发送信号
		res = kill( getppid(), TFTP_SIGNAL);
		if(res == -1)
		{
			printf("kill signal error!\r\n");
			zlog_error(zc, "kill signal error!");
		}
		else
		{
			zlog_debug(zc, "kill signal success!");
		}
	}
	
	return 0;
		
}


/**
  * @brief  processes the tftp request on port 69
  */
void process_tftp_request(int sock, char *pkt_buf, struct sockaddr_in *remote_addr)
{
	tftp_opcode op = tftp_decode_op(pkt_buf);
	char FileName[30];
	struct sockaddr_in addr_local;
	
	int sockfd; 
	unsigned int timeout = 3000;
	
	if( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1 )  
	{
		printf ("ERROR: Failed to obtain Socket Despcritor.\r\n");
		
		/* unable to allocate memory for tftp args  */
		tftp_send_error_message(sock, remote_addr, TFTP_ERR_NOTDEFINED);
		return;
	} 
	else 
	{
		//printf ("OK: Obtain Socket: %d Despcritor sucessfully.\r\n", sockfd);
	}


	setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
	
	addr_local.sin_family = AF_INET;           			// Protocol Family
	addr_local.sin_port = htons(0);         			// Port number 0
	addr_local.sin_addr.s_addr  = htonl(INADDR_ANY);  	// AutoFill local address
	memset (addr_local.sin_zero,0,8);          			// Flush the rest of struct
	
	if( bind(sockfd, (struct sockaddr*)&addr_local, sizeof(struct sockaddr)) == -1 )
	{  
		printf ("ERROR: Failed to bind Port %d.\r\n",0);
		return;
	}
	else 
	{
		//printf("OK: Bind the Port %d sucessfully.\r\n",0);
	}
	
	
	switch (op)
	{
		case TFTP_RRQ:/* TFTP RRQ (read request)  */
		{
			//obtian file name
			tftp_extract_filename(FileName, pkt_buf);
			
			//printf("file name is %s\r\n", FileName);
			
			/* Start the TFTP read mode*/
			tftp_process_read(sockfd, remote_addr, FileName);
			break;
		} 

		case TFTP_WRQ:    /* TFTP WRQ (write request)   */
		{

			tftp_extract_filename(FileName, pkt_buf);
			
			//printf("file name is %s\r\n", FileName);
			
			/* Start the TFTP write mode*/
			tftp_process_write(sockfd, remote_addr, FileName);
			break;
		}
		
		default: /* TFTP unknown request op */
		{
			/* send generic access violation message */
			tftp_send_error_message( sockfd, remote_addr, TFTP_ERR_ACCESS_VIOLATION);
			
			close(sockfd);

			break;
		}
	
	}
	
}

/**
  * @brief  Initializes the udp pcb for TFTP 
  */
void* tftpd_init(void * parg)
{
	int sockfd;                        // Socket file descriptor
	int num;
	socklen_t sin_size;                // to store struct size
	struct sockaddr_in addr_local;     
	struct sockaddr_in addr_remote;
	unsigned int timeout = 3000;
	
	(void)parg;
	
	/* Get the Socket file descriptor */  
	if( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1 )  
	{   
		printf ("ERROR: Failed to obtain Socket Despcritor.\r\n");
		exit(-1);
	} 
	else 
	{
		//printf ("OK: Obtain Socket: %d Despcritor sucessfully.\r\n", sockfd);
	}

	/* Fill the local socket address struct */
	addr_local.sin_family = AF_INET;           		// Protocol Family
	addr_local.sin_port = htons(TFTP_PORT);         		// Port number
	addr_local.sin_addr.s_addr  = htonl(INADDR_ANY);  	// AutoFill local address
	memset (addr_local.sin_zero,0,8);          			// Flush the rest of struct

	/*  Blind a special Port */
	if( bind(sockfd, (struct sockaddr*)&addr_local, sizeof(struct sockaddr)) == -1 )
	{  
		printf ("ERROR: Failed to bind Port %d.\r\n",TFTP_PORT);
		exit(-1);
	}
	else 
	{
		//printf("OK: Bind the Port %d sucessfully.\r\n",TFTP_PORT);
	}

	//设置延时
	setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
	
	while(1)
	{
		sin_size = sizeof(struct sockaddr);
		
		//接收数据
		num = recvfrom(sockfd, tftpRecvBuffer, sizeof(tftpRecvBuffer), 0, (struct sockaddr *)&addr_remote, &sin_size);
		
		if ( num <= 0) 
		{
			//add log
			//printf("recvfrom data null!\r\n");
			continue;
		} 
		else 
		{
			process_tftp_request(sockfd, tftpRecvBuffer, &addr_remote);
		}
		
	}

}
