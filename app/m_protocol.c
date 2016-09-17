#include "m_protocol.h"

void pro_finish_checksum(char *buffer,int buf_size)
{
  int sum = 0;
  for (int i = 2;i < buf_size - 1;i++)
    sum += *(buffer+i);
  sum = sum & 0xff;
  sum = ~sum;
  *(buffer+buf_size-1) = sum;
}

char pro_verify_uart_mesg(char *buffer,int length)
{
	if(buffer[0]==(char)0xff && buffer[1]==(char)0xff){
		int sum = 0;
		for (int i = 2;i < length - 1;i++)
	    sum += *(buffer + i);
	  sum = sum & 0xff;
	  sum = ~sum;
		if(buffer[length - 1] == sum)
			return 1;
	}
	return 0;
}

void send_IR_signal(void *Msg, int length)
{
  
}