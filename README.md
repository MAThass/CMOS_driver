# x86 CMOS and Real-Time Clock (RTC) Driver

A low-level system driver written in **C** for interacting with the **CMOS** memory and the **Real-Time Clock (RTC)** on x86 architectures. This project demonstrates bare-metal hardware communication through direct I/O port manipulation.

## Overview

The CMOS (Complementary Metal-Oxide-Semiconductor) is a small amount of battery-backed memory that stores BIOS settings and powers the Real-Time Clock. This driver provides an interface to read CMOS registers and retrieve precise system time/date information directly from the hardware.



## Features

* **Direct Port I/O:** Implementation of hardware communication using `WRITE_PORT_UCHAR` and `READ_PORT_UCHAR` instructions (via inline assembly) to access ports `0x70` (Address) and `0x71` (Data).
* **RTC Data Retrieval:** Reads seconds, minutes, hours, day of the month, month, and year.
* **BCD to Binary Conversion:** Handles the conversion of Binary Coded Decimal (BCD) values returned by the RTC into standard binary integers.

## Technical Details

### Hardware Interface
The driver communicates with the CMOS through two primary I/O ports:
- **Port 0x70 (Select):** Used to select the CMOS register index. 
- **Port 0x71 (Data):** Used to read from or write to the selected register.

## UserApp
A diagnostic command-line utility designed to interface with the **x86 CMOS/RTC Driver**. This application acts as the user-space bridge, providing a text file to display real-time hardware data.
