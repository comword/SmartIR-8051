#include "m_protocol.h"
#include "OnBoard.h"

void pro_finish_checksum(uint8 *buffer,int buf_size)
{
  int sum = 0;
  for (int i = 2;i < buf_size - 1;i++)
    sum += *(buffer+i);
  sum = sum & 0xff;
  sum = ~sum;
  *(buffer+buf_size-1) = sum;
}

char pro_verify_uart_mesg(uint8 *buffer,int length)
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

void send_IR_signal(uint8 *Msg, int length)
{
  unsigned int pos;
  while(pos)
  {
    if(pos)
      send_moded_forxms(Hi_time);
    else {
      Out_pin = 0;
      Delayms(Hi_time);
    }
  }
}

void Delayms(unsigned int xms)
{
  Delayus(xms*1000);
}

void Delayus(float xus)
{
  unsigned int i;
  for (i = (unsigned int)(xus * 8);i>0;i--)
    asm("NOP");
}

static inline void send_moded_forxms(unsigned int xms)
{
  float p_us = 1000000/IR_FREQ;
  unsigned int i;
  for(i = (unsigned int)(xms*1000/p_us);i>0;i--)
  if(Out_pin == 1)
    Out_pin = 0;
  else
    Out_pin = 1;
  Delayus(p_us);
}

void init_m_clock()
{
  SLEEPCMD &= ~OSC_PD;                       /* turn on 16MHz RC and 32MHz XOSC */
  while (!(SLEEPSTA & XOSC_STB));            /* wait for 32MHz XOSC stable */
  asm("NOP");                                /* chip bug workaround */
  CLKCONCMD &= ~0x40;
  while(CLKCONSTA & 0x40);
  CLKCONCMD &= ~0x47;
}