#include <windows.h>
#include <stdio.h>
#include <stdint.h>


#define COM_PORT "COM3"
#define BAUD_RATE CBR_9600
unsigned int pulsetime = 10;    /* Period in ms of first positive pulse on RTS */

char response[256];

void enterBSLMode(HANDLE hMasterCOM) {

	/*
	 * Pinout
	 * FT232RL DTR ---> MSP430 RST
	 * FT232RL RTS ---> MSP430 TEST
	 */

	 EscapeCommFunction(hMasterCOM, SETDTR); //
		    EscapeCommFunction(hMasterCOM, SETRTS); // RTS high (inactive)
		    Sleep(10);
		    EscapeCommFunction(hMasterCOM, CLRRTS); // RTS low (active)
		    Sleep(10);
		    EscapeCommFunction(hMasterCOM, SETRTS); // RTS high (inactive)
		    Sleep(10);
		    EscapeCommFunction(hMasterCOM, CLRRTS); // RTS low (active)
		    Sleep(10);
		    EscapeCommFunction(hMasterCOM, CLRDTR); //

		    Sleep(20);
		    EscapeCommFunction(hMasterCOM, SETRTS); // RTS high (inactive)
		   	EscapeCommFunction(hMasterCOM, CLRRTS); // RTS low (active)

}

HANDLE openSerialPort() {
    HANDLE hComm = CreateFile(
        COM_PORT,
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
		0,
        NULL
    );

    if (hComm == INVALID_HANDLE_VALUE) {
        printf("Error opening COM port: %ld\n", GetLastError());
        return INVALID_HANDLE_VALUE;
    }

    DCB dcb = {0};
    dcb.DCBlength = sizeof(DCB);

    if (!GetCommState(hComm, &dcb)) {
        printf("Error getting COM state: %ld\n", GetLastError());
        CloseHandle(hComm);
        return INVALID_HANDLE_VALUE;
    }

    dcb.BaudRate = BAUD_RATE;
    dcb.ByteSize = 8;
    dcb.StopBits = ONESTOPBIT;
    dcb.Parity = NOPARITY;
//    dcb.fDtrControl = DTR_CONTROL_DISABLE;
//    dcb.fRtsControl = RTS_CONTROL_DISABLE;

    if (!SetCommState(hComm, &dcb)) {
        printf("Error setting COM state: %ld\n", GetLastError());
        CloseHandle(hComm);
        return INVALID_HANDLE_VALUE;
    }

    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 50;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    timeouts.WriteTotalTimeoutConstant = 50;
    timeouts.WriteTotalTimeoutMultiplier = 10;

    if (!SetCommTimeouts(hComm, &timeouts)) {
        printf("Error setting COM timeouts: %ld\n", GetLastError());
        CloseHandle(hComm);
        return INVALID_HANDLE_VALUE;
    }

    return hComm;
}

BOOL writeData(HANDLE hComm, uint8_t *data, DWORD dataSize) {
    DWORD bytesWritten;
    if (!WriteFile(hComm, data, dataSize, &bytesWritten, NULL)) {
        printf("Error writing to COM port: %ld\n", GetLastError());
        return FALSE;
    }
    if (bytesWritten != dataSize) {
        printf("Warning: Not all data was written to the COM port.\n");
        return FALSE;
    }
    return TRUE;
}


void printHex(const char *label, char *data, int length) {
    printf("%s: ", label);
    for (int i = 0; i < 2; i++) {
        printf("%02X ", (unsigned char)data[i]);
    }
    printf("\n");
}


uint16_t rc128_crc16_1(uint8_t *data, int len)
{
	uint16_t crc;
	uint32_t b;		// Holds current byte
	crc = 0xffff;

	for (; len; len--)
	{
		b = *data++;
		crc = (crc >>8) | (crc <<8);
		crc ^= b;
		crc ^= (crc & 0xff) >> 4;
		crc ^= crc << 12;
		crc ^= (crc & 0xff) << 5;
		crc = crc & 0xffff;
	}
	return crc;
}


int main() {
	DWORD bytesRead;
    HANDLE hComm = openSerialPort();
    if (hComm == INVALID_HANDLE_VALUE) {
        return 1;
    }

    enterBSLMode(hComm);  // sequence to activate BSL mode
    Sleep(350);

    ReadFile(hComm, response, sizeof(response), &bytesRead, NULL);


    // Example: Send Mass Erase command
   uint8_t  buf[256] = { 0x80,0x00,0x00};
   uint8_t *p = &buf[3];
   uint16_t crc;
   int l=1;

   *p++ = 0x19; //cmd
 //  memcpy(p, 0, 0);
   p += 0;
   l +=0;
   crc = rc128_crc16_1(&buf[3], p-&buf[3]);
   	buf[1] = l & 0xFF;
   	buf[2] = (l >> 8) & 0xFF;
   	*p++ = crc & 0xFF;
   	*p++ = (crc >> 8) & 0xFF;

    printHex("Sending", buf, p-buf);
    if (!writeData(hComm, buf, p-buf)) {
        CloseHandle(hComm);
        return 1;
    }

    ReadFile(hComm, response, sizeof(response), &bytesRead, NULL);
    if (bytesRead !=0) {
       printHex("Response", response, bytesRead); // 0x00 means positive ACK
    } else {
        printf("No response received.\n");
    }

    CloseHandle(hComm);
    return 0;
}
