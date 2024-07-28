//---------------------------------------------------------------------------

#ifndef msp430_funcsH
#define msp430_funcsH

#include "customtypes.h"

int ResetMSP430(void);
void RunMSP430Code(void);
void RunMSP430CodeWithoutUnlock(void);
void SetInterfaceBaudForBeaconComms(void);
int EnterBSLOnMSP430(void);
int SetMSPReset(char hi_low_state);
int SetRC218LED(char led_on_state);
ubyte WriteBytesToMSP(unsigned char *bytes, unsigned char num_bytes);
int GetByteFromMSP(void);
int SendCmd5xx(uint cmd, uint a, uint l, uint param_len, const ubyte *params);
void check_msp430_ver(void);
static uint16_t crc16_block(uint8_t *data, int len);
static int get_msg_5xx(byte *buf, int maxlen);

int wait_ack(void);
int unlock_msp430(void);
static unsigned int peek(unsigned int addr);
static void set_pc(int addr);
int poke_block(unsigned int addr, const unsigned char *data, int len);
int xdigit(char c);
int xbyte(char ** pp, unsigned int * psum);
void AddDebug(const char* msg);

extern ubyte   g_msp_unlocked;


//---------------------------------------------------------------------------
#endif
