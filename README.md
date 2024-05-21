## Description
<p>This code is written in C, and its main goal is to enable MSP430's BSL mode so that a boot hex or.srec file loaded into the MSP430 over an FT232 USB TTL connection.</p> 


## Pinout
<div>
<p>FT232 RTS --> MSP430 TEST</p>
<p>FT232 DTR --> MSP430 RST</p>
<p>FT232 TX --> MSP430 RX</p>
<p>FT232 RX --> MSP430 TX</p>
<p>FT232 3v3 --> MSP430 3v3</p>
<p>FT232 GND --> MSP430 GND</p>
</div>

## Waveform
<p>The waveform below illustrates the high/low sequence of the RTS and DTR pins used to enable BSL mode.</p>



![WhatsApp Image 2024-05-21 at 16 33 01](https://github.com/sarbbjeet/MSP430-BSL-Mode/assets/9445093/e1683e57-d812-42c3-9c4d-bf9a2b8c0ca8)

## Commands 
<p>Follow the official MSP430 documentation linked below to learn more about the various command sets used to read, write, and erase flash, among other topics.</p>

https://www.ti.com/lit/ug/slau319af/slau319af.pdf?ts=1716238285686

