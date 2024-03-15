# Makefile for MARLIN SDR
# Tested only on Ubuntu 20.04
#
# On a new system, first change the PLUTO_IP variable in this Makefile,
# then run make install_compiler and make grab_firware.
# This will set up your environment for testing and compiling
# with other Make targets. Ensure that transmitter.h also has the
# correct IP address of your Adalm Pluto. Default user and password for
# ADALM PLUTO is "root", "analog".

CC = /usr/bin/arm-linux-gnueabihf-gcc
CFLAGS1 = -mfloat-abi=hard --sysroot=pluto-0.35.sysroot -std=gnu99 -g
CFLAGS2 = -lpthread -liio -lm -Wall -Wextra -lrt
ROOT_DIR = /

# Change this to your ADALM-PLUTO's ip address
PLUTO_IP = 192.168.2.8

# Change this to the file you want copied and the target directory on ADALM-PLUTO file system
TRANSMISSION_FILE = binData.txt
TRANSMISSION_FILE_TARGET_DIR = /tmp/

install_compiler:
	cd $(ROOT_DIR) && sudo apt-get install -y gcc-arm-linux* g++-arm-linux*

grab_firmware:
	wget https://github.com/analogdevicesinc/plutosdr-fw/releases/download/v0.35/sysroot-v0.35.tar.gz
	tar zxvf sysroot-v0.35.tar.gz
	rm sysroot-v0.35.tar.gz
	if [ -d pluto-0.35.sysroot/staging ]; then \
		rm -r pluto-0.35.sysroot/staging; \
		echo "Staging directory already exists, removing it"; \
	else \
		echo "Staging directory does not exist"; \
	fi
	mv staging pluto-0.35.sysroot

test: clean_test
	wget https://raw.githubusercontent.com/analogdevicesinc/libiio/libiio-v0/examples/ad9361-iiostream.c
	$(CC) $(CFLAGS1) -o test ad9361-iiostream.c $(CFLAGS2)
	scp test root@$(PLUTO_IP):/tmp/
	ssh -t root@$(PLUTO_IP) /tmp/test

transmitter: clean_transmitter
	$(CC) $(CFLAGS1) -o transmitter transmitter.c $(CFLAGS2)
	scp $(TRANSMISSION_FILE) root@$(PLUTO_IP):$(TRANSMISSION_FILE_TARGET_DIR)
	scp transmitter root@$(PLUTO_IP):/tmp/
	ssh -t root@$(PLUTO_IP) /tmp/transmitter

receiver: clean_receiver
	$(CC) $(CFLAGS1) -o receiver receiver.c $(CFLAGS2)
	scp receiver root@$(PLUTO_IP):/tmp/
	ssh -t root@$(PLUTO_IP) /tmp/receiver

clean: clean_test clean_transmitter clean_receiver

clean_transmitter:
ifneq ("$(wildcard transmitter)","")
	rm transmitter
endif

clean_receiver:
ifneq ("$(wildcard receiver)","")
	rm receiver
endif

clean_test:
ifneq ("$(wildcard ad9361-iiostream.c)","")
	rm ad9361-iiostream.c
endif
ifneq ("$(wildcard test)","")
	rm test
endif