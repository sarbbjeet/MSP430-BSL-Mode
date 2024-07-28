# MSP430 Firmware Writer

## Description
This branch of the project facilitates writing .srec firmware files to MSP430-based microcontrollers. It leverages an FT232-based chip, with the controller connected via the TX, RX, DTR, and RTS pins. This setup enables seamless communication and firmware uploading to the microcontroller.

## Requirements to Build the Project
To build this project, follow these steps:

1. **Development Environment**: The project is designed to work with Visual Studio Code (VS Code).
2. **Build Command**: Simply press `Cmd+Shift+B` on your keyboard to build the project.

## Important Note
In the `program.c` file, a specific FT232 device based on its serial number is used. If you need to run the project with a different device, you must update the serial number in the code. Locate the following condition in the `FindAndConnectToInterface(void)` function within `program.c`:

```c
strcmp(devInfo[i].SerialNumber, "AL028VKI") == 0
```
By following these steps, you can successfully build and run the project, enabling the writing of firmware to your MSP430 microcontroller.
