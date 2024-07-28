#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "FTD2XX.h"
#include "msp430_funcs.h"

#define TRUE 1
#define FALSE 0
#define ON 1
#define OFF 0
#define HIGH 1
#define LOW 0

typedef unsigned char ubyte;
typedef unsigned int uint;
typedef unsigned short uword;

ubyte g_msp_unlocked = FALSE;
FT_HANDLE g_ftHandle;

static ubyte passwd[32] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

static const ubyte ff_passwd[32] =
   {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

// Function prototypes
int SetRC218LED(char state);
void AddDebug(const char* msg);

ubyte WriteBytesToMSP(unsigned char *bytes, unsigned char num_bytes) {
    FT_STATUS ftStatus;
    DWORD BytesWritten;

    SetRC218LED(OFF);

    ftStatus = FT_Write(g_ftHandle, bytes, num_bytes, &BytesWritten);
    if (ftStatus == FT_OK) {
        SetRC218LED(ON);
        return BytesWritten;
    } else {
        AddDebug("ERROR: Failed to write to device");
        return 0;
    }
}

int SendCmd5xx(uint cmd, uint a, uint l, uint param_len, const ubyte *params) {
    ubyte buf[256];
    int core_len;
    uword crc;

    if (param_len > (sizeof(buf) - 16)) {
        AddDebug("Cmd too big for buffer send_cmd_5xx");
        return -1;
    }

    switch (cmd) {
        case 0x12:
            buf[3] = 0x10;
            buf[4] = a;
            buf[5] = a >> 8;
            buf[6] = a >> 16;
            memcpy(buf + 7, params, param_len);
            core_len = param_len + 4;
            break;
        case 0x10:
            buf[3] = 0x11;
            memcpy(buf + 4, params, param_len);
            core_len = param_len + 1;
            break;
        case 0x18:
            buf[3] = 0x15;
            core_len = 1;
            break;
        case 0x20:
            buf[3] = 0x52;
            buf[4] = a;
            core_len = 2;
            break;
        case 0x1e:
            buf[3] = 0x19;
            core_len = 1;
            break;
        case 0x14:
            buf[3] = 0x18;
            buf[4] = a;
            buf[5] = a >> 8;
            buf[6] = a >> 16;
            buf[7] = l;
            buf[8] = l >> 8;
            core_len = 6;
            break;
        case 0x1a:
            buf[3] = 0x17;
            buf[4] = a;
            buf[5] = a >> 8;
            buf[6] = a >> 16;
            core_len = 4;
            break;
        default:
            AddDebug("Unsupported send_cmd_5xx");
            return -1;
    }

    buf[0] = 0x80;
    buf[1] = core_len;
    buf[2] = core_len >> 8;
    crc = crc16_block(buf + 3, core_len);
    buf[core_len + 3] = crc;
    buf[core_len + 4] = crc >> 8;

    if (WriteBytesToMSP(buf, core_len + 5) != core_len + 5) {
        AddDebug("ERROR: USB Write");
        return -1;
    }

    for (crc = 0;; crc++) {
        int i;
        i = GetByteFromMSP();
        if (i == -1) {
            AddDebug("Timeout awaiting ACK");
            if (crc > 10) return -1;
        }
        if (i == 0) break;
        AddDebug("Error while awaiting ACK");
        return -1;
    }
    return 0;
}

int GetByteFromMSP(void) {
    FT_STATUS ftStatus;
    unsigned char RxBuffer[128];
    DWORD BytesReceived, RxBytes = 1;

    ftStatus = FT_Read(g_ftHandle, RxBuffer, RxBytes, &BytesReceived);
    if (ftStatus == FT_OK) {
        if (BytesReceived == RxBytes) {
            return RxBuffer[0];
        } else {
            AddDebug("Read Timeout");
            return -1;
        }
    } else {
        AddDebug("ERROR: read of MSP430 failed");
        return 0;
    }
}

void AddDebug(const char* msg) {
    printf("%s\n", msg);
}

void check_msp430_ver(void) {
    AddDebug("Check version command");

    if (SendCmd5xx(0x1e, 0xff0, 16, 0, NULL) < 0) {
        AddDebug("Failed to send check version command");
        return;
    }

    byte buf[5];
    if (get_msg_5xx(buf, sizeof(buf)) < 0) return;
    if (buf[0] == 0x3b) {
        if (buf[1] == 0x04) {
            AddDebug("BSL Locked");
        } else {
            AddDebug("Read error");
        }
    } else if (buf[0] != 0x3a) {
        AddDebug("Error result");
    } else {
        AddDebug("BSL version");
    }

    if (SendCmd5xx(0x20, 0x05, 0, 0, NULL) >= 0) {
        AddDebug("Set baud rate to 57600");
        FT_SetBaudRate(g_ftHandle, 57600);
        Sleep(10);
    }
}

static int get_msg_5xx(byte *buf, int maxlen) {
    int i, len;
    unsigned crc, crc_check;
    byte *p;
    i = GetByteFromMSP();
    if (i != 0x80) {
        AddDebug("Got unexpected response");
        return -1;
    }
    len = GetByteFromMSP();
    len |= GetByteFromMSP() << 8;
    if ((len > maxlen) || len < 0) {
        AddDebug("Bad response length");
        return -1;
    }
    for (p = buf, i = 0; i < len; i++)
        *p++ = GetByteFromMSP();
    crc = GetByteFromMSP();
    crc |= GetByteFromMSP() << 8;
    crc_check = crc16_block(buf, len);
    if (crc != crc_check) {
        AddDebug("Bad response CRC");
        return -1;
    }
    return len;
}

static uint16_t crc16_block(uint8_t *data, int len) {
    uint16_t crc = 0xffff;
    uint32_t b;

    for (; len; len--) {
        b = *data++;
        crc = (crc >> 8) | (crc << 8);
        crc ^= b;
        crc ^= (crc & 0xff) >> 4;
        crc ^= crc << 12;
        crc ^= (crc & 0xff) << 5;
        crc &= 0xffff;
    }
    return crc;
}

static unsigned int peek(unsigned int addr) {
    if (SendCmd5xx(0x14, addr, 2, 0, NULL) < 0) return 0;

    byte buf[4];
    if (get_msg_5xx(buf, sizeof(buf)) < 0) return 0;
    if (buf[0] == 0x3b) {
        AddDebug("BSL locked");
        return 0;
    }
    if (buf[0] != 0x3a) {
        AddDebug("Read result");
        return 0;
    }
    return (buf[1] | (buf[2] << 8));
}

static void set_pc(int addr) {
    SendCmd5xx(0x1a, addr, 0, 0, NULL);
}

int poke_block(unsigned int addr, const unsigned char *data, int len) {
    int blklen;

    if (((addr + len) > 0xffe0) && (addr < 0x10000)) {
        unsigned first_addr = max(addr, 0xffe0);
        unsigned last_addr = min(addr + len, 0x10000);
        AddDebug("Saving to password");
        memcpy(passwd + (first_addr - 0xffe0), data + first_addr - addr, last_addr - first_addr);
    }

    while (len > 0) {
        blklen = (len > 250) ? 250 : len;

        if (SendCmd5xx(0x12, addr, blklen, blklen, data) < 0) return -1;
        data += blklen;
        addr += blklen;
        len -= blklen;
        if (wait_ack() < 0) return -1;
    }
    return 0;
}

int unlock_msp430(void) {
    AddDebug("Unlocking");
    g_msp_unlocked = FALSE;
    SendCmd5xx(0x10, 0, 0, 32, passwd);
    if (wait_ack() < 0) {
        AddDebug("Trying all 0xff unlock password");
        SendCmd5xx(0x10, 0, 0, 32, ff_passwd);
        if (wait_ack() == 0) {
            AddDebug("Unlock successful");
        } else {
            AddDebug("Unlock unsuccessful");
            return -1;
        }
    }

    g_msp_unlocked = TRUE;
    return 0;
}

int wait_ack(void) {
    int i;

    byte buf[2];
    i = get_msg_5xx(buf, sizeof(buf));
    if (i != 2) {
        AddDebug("Bad response length");
        return -1;
    }
    if (buf[0] != 0x3b) {
        AddDebug("Bad response");
        return -1;
    }
    if (buf[1] == 0)
        return 0;

    AddDebug("Command failed");
    return -1;
}

int xdigit(char c) {
    if (isdigit(c)) return c - '0';
    return toupper(c) - 'A' + 10;
}

int xbyte(char ** pp, unsigned int * psum) {
    char * p = *pp;
    byte b;

    if (!isxdigit(p[0]) || !isxdigit(p[1]))
        return -1;
    b = xdigit(*p++) << 4;
    b += xdigit(*p++);
    *pp = p;
    *psum += b;
    return b;
}

int ResetMSP430(void) {
    int error = 0;

    error = SetMSPReset(1);
    Sleep(100);
    error = SetMSPReset(0);
    Sleep(100);
    error = SetMSPReset(1);

    return error;
}

void RunMSP430Code(void) {
    FT_STATUS ftStatus;
    uint32_t addr;

    AddDebug("Run MSP430 Code");

    if (!g_msp_unlocked) {
        AddDebug("Not unlocked - unlocking");
        unlock_msp430();
    }

    addr = peek(0xfffe);
    if (addr == 0 || addr == 0xffff)
        addr = 0x5c00;

    AddDebug("Jumping to address");
    set_pc(addr);

    ftStatus = FT_SetDataCharacteristics(g_ftHandle, FT_BITS_8, FT_STOP_BITS_2, FT_PARITY_NONE);
    if (ftStatus != FT_OK) {
        AddDebug("ERROR: Could not set data characteristics");
    }

    ftStatus = FT_SetBaudRate(g_ftHandle, 1000000);
    if (ftStatus != FT_OK) {
        AddDebug("ERROR: Failed to set baud rate");
    }

    FT_SetTimeouts(g_ftHandle, 500, 0);
    AddDebug("-----Run mode-----");
}

void RunMSP430CodeWithoutUnlock(void) {
    FT_STATUS ftStatus;
    uint32_t addr;

    AddDebug("Run MSP430 Code without unlock");

    addr = peek(0xfffe);
    if (addr == 0 || addr == 0xffff)
        addr = 0x5c00;

    AddDebug("Jumping to address");
    set_pc(addr);

    ftStatus = FT_SetDataCharacteristics(g_ftHandle, FT_BITS_8, FT_STOP_BITS_2, FT_PARITY_NONE);
    if (ftStatus != FT_OK) {
        AddDebug("ERROR: Could not set data characteristics");
    }

    ftStatus = FT_SetBaudRate(g_ftHandle, 1000000);
    if (ftStatus != FT_OK) {
        AddDebug("ERROR: Failed to set baud rate");
    }

    FT_SetTimeouts(g_ftHandle, 500, 0);
    AddDebug("-----Run mode-----");
}

void SetInterfaceBaudForBeaconComms(void) {
    FT_STATUS ftStatus;

    AddDebug("Setting interface baud for beacon comms");

    ftStatus = FT_SetBaudRate(g_ftHandle, 1000000);
    if (ftStatus != FT_OK) {
        AddDebug("ERROR: Failed to set baud rate");
    }

    FT_SetTimeouts(g_ftHandle, 500, 0);

    AddDebug("Flushing serial input...");

    char ascii_buf[256];
    int i, backstop_count = 0, msg_ix = 0;
    unsigned char ch;
    while ((i = GetByteFromMSP()) > -1 && backstop_count < 100) {
        ch = (unsigned char)i;
        if (isascii(ch)) {
            ascii_buf[msg_ix++] = ch;
            if (ch == '\n' || ch == '\r') {
                ascii_buf[msg_ix - 1] = 0;
                AddDebug("Flush");
                msg_ix = 0;
            }
        } else {
            ascii_buf[msg_ix++] = '.';
        }
        backstop_count++;
    }
    ascii_buf[msg_ix] = 0;
    AddDebug("Flush end");

    if (backstop_count < 100) {
        AddDebug("Finished Flush");
    } else {
        AddDebug("ERROR: Aborted Flush");
    }
}

int EnterBSLOnMSP430(void) {
    FT_STATUS ftStatus;
    int error = 0;

    ftStatus = FT_SetDataCharacteristics(g_ftHandle, FT_BITS_8, FT_STOP_BITS_1, FT_PARITY_EVEN);
    if (ftStatus != FT_OK) {
        AddDebug("ERROR: Initial baud rate set failed");
    }

    SetMSPReset(0);

    ftStatus = FT_SetBaudRate(g_ftHandle, 9600);
    if (ftStatus != FT_OK) {
        AddDebug("ERROR: setting baud rate to 9600");
    }

    AddDebug("**Resetting**");

    ftStatus = FT_SetBaudRate(g_ftHandle, 1000000);
    if (ftStatus == FT_OK) {
        ftStatus = FT_SetDataCharacteristics(g_ftHandle, FT_BITS_8, FT_STOP_BITS_1, FT_PARITY_NONE);
        if (ftStatus == FT_OK) {
            Sleep(1);
            SetMSPReset(1);
            WriteBytesToMSP("\0", 1);
            Sleep(5);
            SetMSPReset(0);
            Sleep(5);
            WriteBytesToMSP("\x10", 1);
            WriteBytesToMSP("\x10", 1);
            Sleep(1);
            SetMSPReset(1);
            Sleep(2);
            WriteBytesToMSP("\0", 1);

            ftStatus = FT_SetDataCharacteristics(g_ftHandle, FT_BITS_8, FT_STOP_BITS_1, FT_PARITY_EVEN);
            if (ftStatus != FT_OK) {
                AddDebug("ERROR: setting data characteristics");
            }
            ftStatus = FT_SetBaudRate(g_ftHandle, 9600);
            if (ftStatus != FT_OK) {
                AddDebug("ERROR: setting baud rate");
            }

            FT_SetTimeouts(g_ftHandle, 500, 0);

            AddDebug("Flushing serial input");

            Sleep(100);

            int i, x = 0;
            while ((i = GetByteFromMSP()) > -1 && x < 50) {
                AddDebug("Flush");
                x++;
            }

            if (x < 50) {
                AddDebug("Finished Flush");
            } else {
                AddDebug("ERROR: Aborted Flush");
                error = 1;
            }
        } else {
            error = 2;
            AddDebug("ERROR: Set Data Bits / Parity");
        }
    } else {
        error = 3;
        AddDebug("ERROR: Set Baud Rate Failed");
    }

    if (error) {
        SetRC218LED(OFF);
    } else {
        AddDebug("Reset/flush successful");
        SetRC218LED(ON);
    }

    g_msp_unlocked = FALSE;
    return error;
}

int SetMSPReset(char hi_low_state) {
    FT_STATUS ftStatus;
    int error = 0;

    if (hi_low_state == HIGH) {
        ftStatus = FT_ClrRts(g_ftHandle);
        if (ftStatus != FT_OK) {
            AddDebug("ERROR: MSP Reset: Set RTS Failed");
            error = 1;
        }
    } else {
        ftStatus = FT_SetRts(g_ftHandle);
        if (ftStatus != FT_OK) {
            AddDebug("ERROR: MSP Reset: Clr RTS Failed");
            error = 1;
        }
    }

    return error;
}

int SetRC218LED(char led_on_state) {
    FT_STATUS ftStatus;
    int error = 0;

    if (led_on_state == ON) {
        ftStatus = FT_SetDtr(g_ftHandle);
        if (ftStatus != FT_OK) {
            AddDebug("ERROR: LED: Set DTR Failed");
            error = 1;
        }
    } else {
        ftStatus = FT_ClrDtr(g_ftHandle);
        if (ftStatus != FT_OK) {
            AddDebug("ERROR: LED: Clr DTR Failed");
            error = 1;
        }
    }

    return error;
}
