#include "hal_types.h"
#include <ioCC2530.h>
#define IR_FREQ 38000
#define Out_pin P1_1
#define Hi_time 10
void pro_finish_checksum(uint8 *buffer,int buf_size);
char pro_verify_uart_mesg(uint8 *buffer,int length);
void send_IR_signal(uint8 *Msg, int length);
static inline void send_moded_forxms(unsigned int xms);
void Delayms(unsigned int xms);
void Delayus(float xus);
void init_m_clock(void);