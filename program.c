#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include "FTD2XX.h"
#include "msp430_funcs.h"

typedef unsigned char ubyte;

// Error Codes Enumeration
enum {
    ERR_NONE = 0,
    ERR_ERASE_FAIL,
    ERR_FILE_OPEN_FAIL,
    ERR_SREC_FILE_BAD,
    ERR_SREC_BAD,
    ERR_FILE_READ,
    ERR_BAD_CSUM,
    ERR_BAD_ADDRESS,
    ERR_PROG_FAIL_1,
    ERR_PROG_FAIL_2,
    ERR_BSL_ENTRY,
    ERR_UNLOCK_FAIL
};

// Global FTDI handle
FT_HANDLE g_ftHandle;
int g_device_type;
#define DT_RC218 1
#define DT_NONE 0

// Function prototypes
int SendCmd5xx(uint cmd, uint a, uint l, uint param_len, const ubyte *params);
int wait_ack(void);
int poke_block(unsigned int addr, const unsigned char *data, int len);
int unlock_msp430(void);
int EnterBSLOnMSP430(void);
void check_msp430_ver(void);
int xbyte(char **pp, unsigned int *psum);
ubyte FindAndConnectToInterface(void);
char* PCharToWS(const char* input);
ubyte ConnectToDevice(const char* description);

char* PCharToWS(const char* input) {
    static char buffer[256];
    snprintf(buffer, sizeof(buffer), "%s", input);
    return buffer;
}

ubyte ConnectToDevice(const char* description) {
    FT_STATUS ftStatus = FT_OpenEx((PVOID)description, FT_OPEN_BY_DESCRIPTION, &g_ftHandle);
    if (ftStatus != FT_OK) {
        AddDebug("Failed to connect to device");
        return DT_NONE;
    }
    return strcmp(description, "RC218") == 0 ? DT_RC218 : DT_NONE;
}

bool ResetAndConnectToBeacon(void) {
    FT_STATUS ftStatus;
    bool ok = false;

    AddDebug("ResetAndConnectToBeacon");

    // Reset the device : are we still talking to it?
    // If not, find and connect 1st
    //ftStatus = FT_ResetDevice(g_ftHandle);

    // printf("now status = %d\n", ftStatus);
    if (ftStatus != FT_OK) {
        g_device_type = FindAndConnectToInterface();
    }

    // if we've found a suitable device - now get permissions from the
    // web based on the device's serial number
    if (g_device_type == DT_RC218) {
        AddDebug("Device found - access allowed.");

        SetRC218LED(1);  // ON

        // Config the interface and reset the beacon
        ftStatus = FT_SetDataCharacteristics(g_ftHandle, FT_BITS_8,
            FT_STOP_BITS_1, FT_PARITY_NONE);
        if (ftStatus == FT_OK) {
            // Reset LOW
            SetMSPReset(0);

            ftStatus = FT_SetBaudRate(g_ftHandle, 9600);
            if (ftStatus == FT_OK) {
                if (EnterBSLOnMSP430() == 0) {
                    check_msp430_ver();
                    ok = true;
                }
                else {
                    AddDebug("ERROR: RC218 Entering BSL");
                }
            }
            else {
                AddDebug("ERROR: RC218 Set Baud Rate");
            }
        }
        else {
            AddDebug("ERROR: RC218 Set Data Characteristics");
        }
    }
    else {
        AddDebug("ERROR: Can't connect to interface device");
    }

    return ok;
}

int GetNumBytesInSRecFile(FILE *fp) {
    unsigned int data_bytes_on_line, total_data_bytes = 0;
    char line[512], *p;
    byte buf[256], *q;
    unsigned int i, type, len, addr, csum;
    bool error = false;

    while (!feof(fp)) {
        if (!fgets(line, sizeof(line), fp)) {
            if (ferror(fp)) {
                AddDebug("File read error");
                error = true;
            }
            break;
        }

        // SxNnAaAaDdDdDdDdDd.....DdCc
        // x = record type
        // Nn = num bytes following, i.e. hex digit pairs (incl address, data and checksum fields)
        // AaAa = address bytes (depends on S-Rec type : see below)
        // DdDd...Dd = data bytes
        // Cc = checksum byte

        // S0 & S1 = 2 address bytes
        // S2 = 3 address bytes
        // S3 = 4 address bytes
        p = line;
        csum = 0;

        if (*p++ != 'S' || !isdigit(type = *p++) || (len = xbyte(&p, &csum)) < 0) {
        bad_srec:
            AddDebug("Bad S-Record");
            error = true;
            break;
        }
        for (i = 0; i < 2 * len; i++)
            if (!isxdigit(p[i]))
                goto bad_srec;
        if (type < '1' || type > '3')
            continue;

        type = (type - '0') + 1; // number of address bytes
        if (type + 1 > len)
            goto bad_srec;
        for (i = 0, addr = 0; i < type; i++)
            addr = addr * 256 + xbyte(&p, &csum);

        data_bytes_on_line = len - (type) - 1; // length - address bytes - csum bytes

        for (i = 0, len -= (type + 1), q = buf; i < len; i++)
            *q++ = xbyte(&p, &csum);

        if (xbyte(&p, &csum), (csum & 255) != 255) {
            AddDebug("Bad Checksum");
            error = true;
            break;
        }

        total_data_bytes += data_bytes_on_line;
    }

    // reset file pointer
    rewind(fp);

    if (!error) {
        return total_data_bytes;
    }
    else
        return -1;
}

ubyte FindAndConnectToInterface(void) {
    FT_STATUS ftStatus;
    FT_DEVICE_LIST_INFO_NODE *devInfo;
    DWORD numDevs;
    ubyte device_type = DT_NONE;
    char dbg_msg[256];
    // create device information list
    ftStatus = FT_CreateDeviceInfoList(&numDevs);

    if (ftStatus == FT_OK) {
        snprintf(dbg_msg, sizeof(dbg_msg), "Number of FTDI Devices = %lu", numDevs);
        AddDebug(dbg_msg);
    } else {
        numDevs = 0;
    }

    if (numDevs > 0) {
        // allocate storage for list based on numDevs
        devInfo = (FT_DEVICE_LIST_INFO_NODE*)malloc(sizeof(FT_DEVICE_LIST_INFO_NODE) * numDevs);

        // get the device info list
        ftStatus = FT_GetDeviceInfoList(devInfo, &numDevs);
        if (ftStatus == FT_OK) {
            AddDebug("Dev Info List:");

            for (unsigned int i = 0; i < numDevs; i++) {
                snprintf(dbg_msg, sizeof(dbg_msg), "Dev %u:", i);
                AddDebug(dbg_msg);

                snprintf(dbg_msg, sizeof(dbg_msg), "Flags = 0x%X", devInfo[i].Flags);
                AddDebug(dbg_msg);

                snprintf(dbg_msg, sizeof(dbg_msg), "Type = 0x%X", devInfo[i].Type);
                AddDebug(dbg_msg);

                snprintf(dbg_msg, sizeof(dbg_msg), "ID = 0x%X", devInfo[i].ID);
                AddDebug(dbg_msg);

                snprintf(dbg_msg, sizeof(dbg_msg), "LocID = 0x%X", devInfo[i].LocId);
                AddDebug(dbg_msg);

                snprintf(dbg_msg, sizeof(dbg_msg), "SerialNumber = [%s]", PCharToWS(devInfo[i].SerialNumber));
                AddDebug(dbg_msg);

                snprintf(dbg_msg, sizeof(dbg_msg), "Description = [%s]", PCharToWS(devInfo[i].Description));
                AddDebug(dbg_msg);

                if (strcmp(devInfo[i].Description, "MT222") == 0 || strcmp(devInfo[i].Description, "RC218") == 0   || strcmp(devInfo[i].SerialNumber, "AL028VKI") == 0 ) {
                    // device_type = ConnectToDevice(devInfo[i].Description);
                    device_type =1; 
                }
            }
        }
        free(devInfo);
    }

    return device_type;
}

void flash_srec_file(const char* filePath) {
    FILE *fp;
    char line[512], *p;
    byte buf[256], *q;
    byte gather_buf[256];
    unsigned int gather_addr = 0, gather_len = 0;
    unsigned int i, type, len, addr, csum;
    int bytes_sent = 0, total_data_bytes = 0;
    int error = ERR_NONE;

    if (!ResetAndConnectToBeacon()) {
        printf("Failed to reset and connect to beacon\n");
        return;
    }

    g_msp_unlocked = false;

    printf("**PROGRAM**\n");

    if (!g_msp_unlocked) {
        printf("Locked - bulk erase required\n");

        if ((SendCmd5xx(0x18, 0, 0, 0, NULL) < 0) || (wait_ack() < 0)) {
            printf("Erase fail\n");
            error = ERR_ERASE_FAIL;
        } else {
            printf("Erased\n");
            EnterBSLOnMSP430();
            check_msp430_ver();

            if (!g_msp_unlocked) {
                printf("Unlocking...\n");
                unlock_msp430();
            }
        }
    }

    Sleep(100);

    if (!g_msp_unlocked) {
        printf("Locked - bulk erase required\n");

        if ((SendCmd5xx(0x18, 0, 0, 0, NULL) < 0) || (wait_ack() < 0)) {
            printf("Erase fail\n");
            error = ERR_ERASE_FAIL;
        } else {
            printf("Erased\n");
            if (!g_msp_unlocked) {
                printf("Unlocking...\n");
                unlock_msp430();
            }
        }
    }

    if (error == ERR_NONE) {
        fp = fopen(filePath, "rt");
        if (!fp) {
            printf("Unable to read file: [%s]\n", filePath);
            error = ERR_FILE_OPEN_FAIL;
        } else if (!g_msp_unlocked) {
            printf("Unable to unlock\n");
            error = ERR_UNLOCK_FAIL;
        } else {
            printf("File Opened\n");

            total_data_bytes = GetNumBytesInSRecFile(fp);
            if (total_data_bytes == -1) {
                error = ERR_SREC_FILE_BAD;
                fclose(fp);
                return;
            }

            while (!feof(fp)) {
                if (!fgets(line, sizeof(line), fp)) {
                    if (ferror(fp)) {
                        printf("File read error\n");
                        error = ERR_FILE_READ;
                    }
                    break;
                }
                p = line;
                csum = 0;
                if (*p++ != 'S' || !isdigit(type = *p++) || (len = xbyte(&p, &csum)) < 0) {
                    error = ERR_SREC_BAD;
                    printf("Bad S-Record : [%s]\n", line);
                    break;
                }
                for (i = 0; i < 2 * len; i++)
                    if (!isxdigit(p[i])) {
                        error = ERR_SREC_BAD;
                        printf("Bad S-Record : [%s]\n", line);
                        break;
                    }
                if (type < '1' || type > '3')
                    continue;

                type = (type - '0') + 1;
                if (type + 1 > len) {
                    error = ERR_SREC_BAD;
                    printf("Bad S-Record : [%s]\n", line);
                    break;
                }
                for (i = 0, addr = 0; i < type; i++)
                    addr = addr * 256 + xbyte(&p, &csum);

                for (i = 0, len -= (type + 1), q = buf; i < len; i++)
                    *q++ = xbyte(&p, &csum);

                if (xbyte(&p, &csum), (csum & 255) != 255) {
                    printf("Bad Checksum : [%s]\n", line);
                    error = ERR_BAD_CSUM;
                    break;
                } else if (addr > 0xfffff || addr + len > 0xf0000) {
                    printf("Bad Address : [%s]\n", line);
                    error = ERR_BAD_ADDRESS;
                    break;
                }
                if (gather_len == 0) gather_addr = addr;
                if (((gather_len + len) > 240) || ((gather_addr + gather_len) != addr)) {
                    if (poke_block(gather_addr, gather_buf, gather_len) < 0) {
                        printf("***** Program fail 1\n");
                        gather_len = 0;
                        error = ERR_PROG_FAIL_1;
                        break;
                    }
                    bytes_sent += gather_len;
                    printf("%d bytes of %d\n", bytes_sent, total_data_bytes);
                    gather_len = 0;
                    gather_addr = addr;
                }
                memcpy(gather_buf + gather_len, buf, len);
                gather_len += len;
            }
            if (gather_len) {
                if (poke_block(gather_addr, gather_buf, gather_len) < 0) {
                    printf("***** Program fail 2\n");
                    error = ERR_PROG_FAIL_2;
                }
                bytes_sent += gather_len;
                printf("%d bytes of %d\n", bytes_sent, total_data_bytes);
            }
            fclose(fp);
        }
    }

    if (error == ERR_NONE) {
        printf("**PROGRAMMING SUCCEEDED**\n");
        SetRC218LED(1);  // ON
        RunMSP430Code();
    } else {
        printf("!!PROGRAMMING FAILED!!\n");
        SetRC218LED(0);  // OFF
    }

    FT_STATUS ftStatus = FT_Purge(g_ftHandle, FT_PURGE_RX | FT_PURGE_TX);
    if (ftStatus != FT_OK) {
        printf("Purge after program FAILED\n");
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: %s <srec_file_path>\n", argv[0]);
        return -1;
    }

    // Initialize the FTDI device handle
    FT_STATUS ftStatus = FT_Open(0, &g_ftHandle);
    if (ftStatus != FT_OK) {
        printf("Failed to open FTDI device\n");
        return -1;
    }

    flash_srec_file(argv[1]);

    // Close the FTDI device handle
    FT_Close(g_ftHandle);

    return 0;
}