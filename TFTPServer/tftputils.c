/* tftputils.c */

#include <string.h>
#include "tftputils.h"

/**
  * @brief  Extracts the opcode from a TFTP message
**/ 
tftp_opcode tftp_decode_op(char *buf)
{
  return (tftp_opcode)(buf[1]);
}

/**
  * @brief Extracts the block number from TFTP message
**/
unsigned short tftp_extract_block(char *buf)
{
  unsigned short *b = (unsigned short*)buf;
  return ntohs(b[1]);
}

/**
  * @brief Extracts the filename from TFTP message
**/ 
void tftp_extract_filename(char *fname, char *buf)
{
  strcpy(fname, buf + 2);
}

/**
  * @brief set the opcode in TFTP message: RRQ / WRQ / DATA / ACK / ERROR 
**/ 
void tftp_set_opcode(char *buffer, tftp_opcode opcode)
{

  buffer[0] = 0;
  buffer[1] = (unsigned char)opcode;
}

/**
  * @brief Set the errorcode in TFTP message
**/
void tftp_set_errorcode(char *buffer, tftp_errorcode errCode)
{

  buffer[2] = 0;
  buffer[3] = (unsigned char)errCode;
}

/**
  * @brief Sets the error message
**/
void tftp_set_errormsg(char * buffer, char* errormsg)
{
  strcpy(buffer + 4, errormsg);
}

/**
  * @brief Sets the block number
**/
void tftp_set_block(char* packet, unsigned short block)
{

  unsigned short *p = (unsigned short *)packet;
  p[1] = htons(block);
}

/**
  * @brief Set the data message
**/
void tftp_set_data_message(char* packet, char* buf, int buflen)
{
  memcpy(packet + 4, buf, buflen);
}

/**
  * @brief Check if the received acknowledgement is correct
**/
unsigned long tftp_is_correct_ack(char *buf, int block)
{
  /* first make sure this is a data ACK packet */
  if (tftp_decode_op(buf) != TFTP_ACK)
    return 0;

  /* then compare block numbers */
  if (block != tftp_extract_block(buf))
    return 0;

  return 1;
}

