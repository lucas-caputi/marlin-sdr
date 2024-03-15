# MARLIN SDR (CPE Capstone)

<div align="center">
  <kbd>
    <img src="images/marlin_logo_blue.png" />
  </kbd>
</div>

## Description

This project contains a custom C transmitter to be used with Analog Devices ADALM-PLUTO.

### Features

- QPSK modulation tranmission mode.
- 16QAM modulation transmission mode.
- 915 MHz transmission.
- 20 MHz bandwidth.

### Authors

- Lucas Caputi
- Brandon Parker
- Junior Perez
- Spencer Seltzer
- Saumitra Tiwari

## Getting started

### Prerequisites

- A machine running Ubuntu 20.04 (WSL or a virtual box is adequate).
- An Analog Devices ADALM-PLUTO Software-Defined Radio Active Learning Module.

### Instructions

- Clone this repository using `git clone https://github.com/satiwari26/SDR_capstone.git`.
- Modify the `PLUTO_IP` variable in `Makefile` and the `IP_ADDRESS` variable in `transmitter.h` to match your ADALM-PLUTO's IP address. Refer to this article for finding the IP address of your ADALM-PLUTO: [link](https://wiki.analog.com/university/tools/pluto/users/customizing).
- Run `make install_compiler` and `make grab_firmware`.
- Run `make transmitter` to start the transmitter on your ADALM-PLUTO. If prompted for the password, the default password for the ADALM-PLUTO is `analog`.
- Follow the instructions on the command line interface to operate the transmitter.

## Acknowledgements

Thanks to Cameron Priest of Special Technologies Laboratory (STL) for his guidance on this project.
Additionally, thanks to Professor Murray and all his help throughout CPE350 and CPE450.
