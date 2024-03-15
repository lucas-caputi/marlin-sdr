/* Transmitter for MARLIN SDR */

#include "transmitter.h"

// IIO structs required for transmitting
static struct iio_context *adalm_pluto = NULL;
static struct iio_device *ad9361 = NULL;
static struct iio_device *tx = NULL;
static struct iio_channel *tx_i = NULL;
static struct iio_channel *tx_q = NULL;
static struct iio_buffer *tx_buf = NULL;

// Global running flag
int running = true;

// Handles SIGINT
void handle_sig() {
    printf("\n");
    running = false;
}

// Cleans up IIO structures on shutdown
void shutdown() {
    printf("\nShutting down program\n");
    iio_buffer_destroy(tx_buf);
    if(tx_i) { iio_channel_disable(tx_i); }
    if(tx_q) { iio_channel_disable(tx_q); }
    iio_context_destroy(adalm_pluto);
}

// Prints a start message to stdout
void print_start_message() {
    unsigned int major;
    unsigned int minor;

    iio_library_get_version(&major, &minor, NULL);
    printf("\nRunning transmitter.exe on ADALM-PLUTO\n");
    printf("Using Libiio Version: %d.%d\n", major, minor);
    printf("Transmitter settings:\n");
    printf("- center frequency = %lld Hz\n", TX_LO);
    printf("- bandwidth = %lld Hz\n", TX_BANDWIDTH);
}

// Prints a line seperator to stdout
void print_seperator() {
    printf("\n-----------------------------------------------\n");
}

// Checks if given pointer is NULL, if so prints error and exits program
void null_error_check(void *ptr, char *descr) {
    if(ptr == NULL) {
        printf("error: %s null value error\n", descr);
        exit(0);
    }
}

// Checks if given value is less than zero, if so prints error and exits program
void less_than_zero_error_check(int val, char *descr) {
    if(val < 0) {
        printf("error: %s less than zero error\n", descr);
        exit(0);
    }
}

// Sets up context (ADALM-PLUTO)
void set_up_context() {
    printf("\nSetting up context (ADALM-PLUTO)\n");
    adalm_pluto = iio_create_context_from_uri(IP_ADDRESS);
    null_error_check((void *)adalm_pluto, "Adalm Pluto context");
}

// Sets up device (AD9361)
void set_up_device() {
    printf("Setting up device (AD9361 Physical Layer)\n");
    ad9361 = iio_context_find_device(adalm_pluto, "ad9361-phy");
    null_error_check((void *)ad9361, "AD9361 Physical Layer device");
}

// Configures device (AD9361 Physical Layer) according to transmitter settings defined in header file
void config_device() {
    printf("Configuring device (AD9361 Physical Layer)\n");
    // Get channels
    struct iio_channel *chn_1 = iio_device_find_channel(ad9361, "voltage0", true);
    null_error_check((void *)chn_1, "voltage0");
    struct iio_channel *chn_2 = iio_device_find_channel(ad9361, "altvoltage1", true);
    null_error_check((void *)chn_2, "altvoltage1");

    // Get attributes 
    const char *attr_1 = iio_channel_find_attr(chn_1, "rf_port_select");
    null_error_check((void *)attr_1, "rf_port_select");
    const char *attr_2 = iio_channel_find_attr(chn_1, "rf_bandwidth");
    null_error_check((void *)attr_2, "rf_bandwidth");
    const char *attr_3 = iio_channel_find_attr(chn_1, "sampling_frequency");
    null_error_check((void *)attr_3, "sampling_frequency");
    const char *attr_4 = iio_channel_find_attr(chn_2, "frequency");
    null_error_check((void *)attr_4, "frequency");
    
    // Set attributes
    iio_channel_attr_write_raw(chn_1, attr_1, TX_RF_PORT_SELECT, sizeof(TX_RF_PORT_SELECT));
    iio_channel_attr_write_longlong(chn_1, attr_2, TX_BANDWIDTH);
    iio_channel_attr_write_longlong(chn_1, attr_3, SAMPLE_RATE);
    iio_channel_attr_write_longlong(chn_2, attr_4, TX_LO);
}

// Sets up device (AD9361 Tx Output Driver)
void set_up_device_2() {
    printf("Setting up device (AD9361 Tx Output Driver)\n");
    tx = iio_context_find_device(adalm_pluto, "cf-ad9361-dds-core-lpc");
    null_error_check(tx, "AD9361 Tx Output Driver");
}

// Sets up streaming channels (tx_i and tx_q)
void set_up_streaming_channels() {
    printf("Setting up streaming channels (Transmit i and q))\n");
    tx_i = iio_device_find_channel(tx, "voltage0", true);
    null_error_check((void *)tx_i, "tx_i");
    tx_q = iio_device_find_channel(tx, "voltage1", true);
    null_error_check((void *)tx_q, "tx_q");
}

// Enables streaming channels (tx_i and tx_q)
void enable_streaming_channels() {
    printf("Enabling streaming channels (Transmit i and q)\n");
    iio_channel_enable(tx_i);
    iio_channel_enable(tx_q);
}

// Set up a buffer (tx_buf)
void set_up_buffer() {
    printf("Setting up buffer (Tx buffer)\n");
    tx_buf = iio_device_create_buffer(tx, TEST_TRANSMIT_AMOUNT_BYTES, false); // CYCLIC = FALSE 
    null_error_check((void *)tx_buf, "tx_buf");
}

// Returns the size of an open file. Assumes file cursor is at beginning of file.
// Assuming file was opened in binary mode, this function will return size in bytes.
long get_file_size(FILE *fp) {
    if (fp == NULL) {
        return -1;
    }
    if (fseek(fp, 0, SEEK_END) < 0) {
        fclose(fp);
        return -1;
    }

    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    return size;
}

// Converts bytes to appropriate unit and prints out result.
void print_file_size(unsigned long long bytes) {
    double result;
    const char* unit;

    if (bytes < 1024) {
        result = (double)bytes;
        unit = "bytes";
    } else if (bytes < 1024 * 1024) {
        result = (double)bytes / 1024;
        unit = "KB";
    } else if (bytes < 1024 * 1024 * 1024) {
        result = (double)bytes / (1024 * 1024);
        unit = "MB";
    } else if (bytes < 1024ULL * 1024 * 1024 * 1024) {
        result = (double)bytes / (1024 * 1024 * 1024);
        unit = "GB";
    } else {
        result = (double)bytes / (1024ULL * 1024 * 1024 * 1024);
        unit = "TB";
    }

    printf("File size is %.2f %s. ", result, unit);
}

// Prints a progress bar
void print_progress_bar(int progress, int total, int barWidth) {
    // Calculate the percentage of progress
    float percentage = (float)progress / total;
    
    // Calculate the number of characters to fill in the progress bar
    int numBars = percentage * barWidth;
    
    // Print the progress bar
    printf("[");
    for (int i = 0; i < barWidth; i++) {
        if (i < numBars) {
            printf("=");
        } else {
            printf(" ");
        }
    }
    printf("] %.2f%%\r", percentage * 100); // Print percentage
    fflush(stdout); // Flush stdout to ensure the progress bar is displayed immediately
}

// Treats two integers as bits and returns their binary value
int convert_bits_to_binary(int16_t a, int16_t b) {
    if (a == 0 && b == 0) {
        return 0;
    } else if (a == 0 && b == 1) {
        return 1;
    } else if (a == 1 && b == 0) {
        return 2;
    } else if (a == 1 && b == 1) {
        return 3;
    }
        
    return -1; // Default or error value 
}

// Takes in a bitpair and set the corresponding i and q values.
void qpsk_modulation(int bitpair, int16_t *i, int16_t *q) {
    switch(bitpair) {
        case 0x00:
            *i = QPSK_00_I;
            *q = QPSK_00_Q;
            break;
        case 0x01:
            *i = QPSK_01_I;
            *q = QPSK_01_Q;
            break;
        case 0x02:
            *i = QPSK_10_I;
            *q = QPSK_10_Q;
            break;
        case 0x03:
            *i = QPSK_11_I;
            *q = QPSK_11_Q;
            break;
    }
}

// A simple i and q example that prints out i and q values for dummy data
void qpsk_example() {
    printf("\nQPSK example using a data byte = 00011011.\n");
    printf("\nIn QPSK, two bits are scanned at a time, and assigned one\n");
    printf("of four I and Q pairs based on their value. Each I and Q pair\n");
    printf("represents a phase-shifted signal.\n\n");
    printf("1st bit pair = 00 -> i = %d, q = %d\n", QPSK_00_I, QPSK_00_Q);
    printf("2nd bit pair = 01 -> i = %d, q = %d\n", QPSK_01_I, QPSK_01_Q);
    printf("3rd bit pair = 10 -> i = %d, q = %d\n", QPSK_10_I, QPSK_10_Q);
    printf("4th bit pair = 11 -> i = %d, q = %d\n", QPSK_11_I, QPSK_11_Q);
    sleep(LONG_MESSAGE_DELAY);
}

// A QPSK test that cycles transmitting the four bitpairs. Meant to be viewed on a vector analyzer for testing.
void qpsk_transmit_test() {
    printf("\nBeginning QPSK transmission test, press ctrl+c to stop.\n");
    printf("This test will cycle between sending the four QPSK signals\n");
    printf("and is meant to be viewed on a vector analyzer for testing.\n\n");
    sleep(LONG_MESSAGE_DELAY);
    char *p_dat, *p_end;
    ptrdiff_t p_inc;
    ssize_t nbytes_tx;

    while(running) {
        // Transmit 00
        printf("Transmitting 00\r");
        fflush(stdout);
        p_inc = iio_buffer_step(tx_buf);
        p_end = iio_buffer_end(tx_buf);
        for (p_dat = (char *)iio_buffer_first(tx_buf, tx_i); p_dat < p_end; p_dat += p_inc) {
            ((int16_t*)p_dat)[0] = QPSK_00_I;      // Real (i)
            ((int16_t*)p_dat)[1] = QPSK_00_Q;      // Imag (q)
        }
        nbytes_tx = iio_buffer_push(tx_buf);
        less_than_zero_error_check((int)nbytes_tx, "nbytes_tx");
        if(!running) break;

        // Transmit 01
        printf("Transmitting 01\r");
        fflush(stdout);
        p_inc = iio_buffer_step(tx_buf);
        p_end = iio_buffer_end(tx_buf);
        for (p_dat = (char *)iio_buffer_first(tx_buf, tx_i); p_dat < p_end; p_dat += p_inc) {
            ((int16_t*)p_dat)[0] = QPSK_01_I;      // Real (i)
            ((int16_t*)p_dat)[1] = QPSK_01_Q;      // Imag (q)
        }
        nbytes_tx = iio_buffer_push(tx_buf);
        less_than_zero_error_check((int)nbytes_tx, "nbytes_tx");
        if(!running) break;

        // Transmit 10
        printf("Transmitting 10\r");
        fflush(stdout);
        p_inc = iio_buffer_step(tx_buf);
        p_end = iio_buffer_end(tx_buf);
        for (p_dat = (char *)iio_buffer_first(tx_buf, tx_i); p_dat < p_end; p_dat += p_inc) {
            ((int16_t*)p_dat)[0] = QPSK_10_I;      // Real (i)
            ((int16_t*)p_dat)[1] = QPSK_10_Q;      // Imag (q)
        }
        less_than_zero_error_check((int)nbytes_tx, "nbytes_tx");
        nbytes_tx = iio_buffer_push(tx_buf);
        if(!running) break;

        // Transmit 11
        printf("Transmitting 11\r");
        fflush(stdout);
        p_inc = iio_buffer_step(tx_buf);
        p_end = iio_buffer_end(tx_buf);
        for (p_dat = (char *)iio_buffer_first(tx_buf, tx_i); p_dat < p_end; p_dat += p_inc) {
            ((int16_t*)p_dat)[0] = QPSK_11_I;      // Real (i)
            ((int16_t*)p_dat)[1] = QPSK_11_Q;      // Imag (q)
        }
        nbytes_tx = iio_buffer_push(tx_buf);
        less_than_zero_error_check((int)nbytes_tx, "nbytes_tx");
    }
}

// Transmit data with QPSK modulation scheme. This function continuously transmits packets 
// until entirity of data has been transmitted. Requires file pointer to open file which contains data to transmit.
void qpsk_transmit(FILE *transmission_data_fp) {
    printf("\nBeginning qpsk transmission, press ctrl+c to stop\n\n");
    char *p_dat;
    ptrdiff_t p_inc;
    ssize_t nbytes_tx;
    int16_t i, q;
    int bitpair, bitpairs_filled, bitpairs_remaining, num_bytes_to_read, bytes_read, transmission_data_complete, packet_num;
    long file_size, total_data_bytes_transmitted;

    // Get file size to determine progress percentages, then print message
    file_size = get_file_size(transmission_data_fp);
    print_file_size(file_size);
    printf("Transmitting...\n");

    // Set up local array with preamble 
    unsigned char preamble[PREAMBLE_SIZE_BYTES] = {
        0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 
        0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33
    };

    // Set up local array with sync word 
    unsigned char sync_word[SYNC_WORD_SIZE_BYTES] = {
        0x33, 0xF7
    };

    // Set up local memory to store file data upon reading file
    // We will read a maximum of DATA_BYTES_PER_READ bytes at a time
    unsigned char file_data[DATA_BYTES_PER_READ];

    // Initialize some variables
    total_data_bytes_transmitted = 0;
    transmission_data_complete = false;
    packet_num = 0;
    
    // Transmit packets until interrupt occurs or data transmission is complete
    while(running && !transmission_data_complete) {
        // Get buffer step sizeand pointer to start of buffer
        p_inc = iio_buffer_step(tx_buf);
        p_dat = (char *)iio_buffer_first(tx_buf, tx_i);

        // Fill buffer with preamble, a bitpair at a time
        for(bitpairs_filled = 0; bitpairs_filled < PREAMBLE_SIZE_BITPAIRS; bitpairs_filled++, p_dat += p_inc) {
            bitpair = (preamble[bitpairs_filled / 4] >> ((3 - bitpairs_filled % 4) * 2)) & 0b11;
            qpsk_modulation(bitpair, &i, &q);
            ((int16_t*)p_dat)[0] = i;
            ((int16_t*)p_dat)[1] = q;
        }

        // Fill buffer with sync word, a bitpair at a time
        for(bitpairs_filled = 0; bitpairs_filled < SYNC_WORD_SIZE_BITPAIRS; bitpairs_filled++, p_dat += p_inc) {
            bitpair = (sync_word[bitpairs_filled / 4] >> ((3 - bitpairs_filled % 4) * 2)) & 0b11;
            qpsk_modulation(bitpair, &i, &q);
            ((int16_t*)p_dat)[0] = i;
            ((int16_t*)p_dat)[1] = q;
        }

        // Fill the rest of the buffer with data, a bitpair at a time
        bitpairs_filled = 0;
        bitpairs_remaining = TX_BUFFER_SIZE_BITPAIRS - PREAMBLE_SIZE_BITPAIRS - SYNC_WORD_SIZE_BITPAIRS;
        while(bitpairs_remaining > 0) {
            // Determine how many bytes should be read from file
            num_bytes_to_read = ((bitpairs_remaining / 4) < DATA_BYTES_PER_READ) ? (bitpairs_remaining / 4) : DATA_BYTES_PER_READ;

            // Read bytes from file
            bytes_read = fread(file_data, 1, num_bytes_to_read, transmission_data_fp);  
            
            // Increment total data bytes read
            total_data_bytes_transmitted += bytes_read;

            // Check if end of transmission data was reached
            if(bytes_read != num_bytes_to_read) {
                // TODO: Check for error
                // Fill packet with remaining data
                while((bitpairs_filled / 4) < bytes_read) {
                    bitpair = (file_data[bitpairs_filled / 4] >> ((3 - bitpairs_filled % 4) * 2)) & 0b11;
                    qpsk_modulation(bitpair, &i, &q);
                    ((int16_t*)p_dat)[0] = i;
                    ((int16_t*)p_dat)[1] = q;
                    bitpairs_filled++;
                    bitpairs_remaining--;
                    p_dat += p_inc;  
                }
                
                // Pad rest of packet with 0's
                while(bitpairs_remaining > 0) {
                    ((int16_t*)p_dat)[0] = QPSK_00_I;
                    ((int16_t*)p_dat)[1] = QPSK_00_Q;
                    p_dat += p_inc;
                    bitpairs_remaining--;
                }

                // Set flag and break out of loop
                transmission_data_complete = true;
                break;
            }

            // End of transmission data was not reached, so continue to fill the the packet with data
            while((bitpairs_filled / 4) < bytes_read) {
                bitpair = (file_data[bitpairs_filled / 4] >> ((3 - bitpairs_filled % 4) * 2)) & 0b11;
                qpsk_modulation(bitpair, &i, &q);
                ((int16_t*)p_dat)[0] = i;
                ((int16_t*)p_dat)[1] = q;
                bitpairs_filled++;
                bitpairs_remaining--;
                p_dat += p_inc;  
            }
        }

        // Transmit packet and perform error check
        packet_num++;
        print_progress_bar(total_data_bytes_transmitted, file_size, PROGRESS_BAR_LENGTH);
        nbytes_tx = iio_buffer_push(tx_buf);
        less_than_zero_error_check((int)nbytes_tx, "nbytes_tx");
    }
    printf("\n");   // Needed since print_progress_bar does not print a newline character
}

// Takes in a fourbit and sets corresponding i and q values.
void sxtn_qam_modulation(int fourbit, int16_t *i, int16_t *q) {
    switch(fourbit) {
        case 0x00:
            *i = SXTN_QAM_0000_I;
            *q = SXTN_QAM_0000_Q;
            break;
        case 0x01:
            *i = SXTN_QAM_0001_I;
            *q = SXTN_QAM_0001_Q;
            break;
        case 0x02:
            *i = SXTN_QAM_0010_I;
            *q = SXTN_QAM_0010_Q;
            break;
        case 0x03:
            *i = SXTN_QAM_0011_I;
            *q = SXTN_QAM_0011_Q;
            break;
        case 0x04:
            *i = SXTN_QAM_0100_I;
            *q = SXTN_QAM_0100_Q;
            break;
        case 0x05:
            *i = SXTN_QAM_0101_I;
            *q = SXTN_QAM_0101_Q;
            break;
        case 0x06:
            *i = SXTN_QAM_0110_I;
            *q = SXTN_QAM_0110_Q;
            break;
        case 0x07:
            *i = SXTN_QAM_0111_I;
            *q = SXTN_QAM_0111_Q;
            break;
        case 0x08:
            *i = SXTN_QAM_1000_I;
            *q = SXTN_QAM_1000_Q;
            break;
        case 0x09:
            *i = SXTN_QAM_1001_I;
            *q = SXTN_QAM_1001_Q;
            break;
        case 0x0a:
            *i = SXTN_QAM_1010_I;
            *q = SXTN_QAM_1010_Q;
            break;
        case 0x0b:
            *i = SXTN_QAM_1011_I;
            *q = SXTN_QAM_1011_Q;
            break;
        case 0x0c:
            *i = SXTN_QAM_1100_I;
            *q = SXTN_QAM_1100_Q;
            break;
        case 0x0d:
            *i = SXTN_QAM_1101_I;
            *q = SXTN_QAM_1101_Q;
            break;
        case 0x0e:
            *i = SXTN_QAM_1110_I;
            *q = SXTN_QAM_1110_Q;
            break;
        case 0x0f:
            *i = SXTN_QAM_1111_I;
            *q = SXTN_QAM_1111_Q;
            break;
    }
}

// A simple 16QAM test that prints out i and q values for dummy data
void sxtn_qam_example() {
    printf("\n16QAM example using a data byte = 00011011.\n");
    printf("\nIn 16QAM, four bits are scanned at a time, and assigned one\n");
    printf("of sixteen I and Q pairs based on their value. Each I and Q pair\n");
    printf("represents a phase-shifted signal.\n\n");
    printf("1st four bits = 0001 -> i = %d, q = %d\n", SXTN_QAM_0001_I, SXTN_QAM_0001_Q);
    printf("2nd four bits = 1011 -> i = %d, q = %d\n", SXTN_QAM_1011_I, SXTN_QAM_1011_Q);
    sleep(LONG_MESSAGE_DELAY);
}

// A 16QAM test that cycles transmitting the sixteen fourbits. Meant to be viewed on a vector analyzer for testing.
void sxtn_qam_transmit_test() {
    printf("\nBeginning 16QAM transmission test, press ctrl+c to stop.\n");
    printf("This test will cycle between sending the sixteen 16QAM signals\n");
    printf("and is meant to be viewed on a vector analyzer for testing.\n\n");
    sleep(LONG_MESSAGE_DELAY);
    char *p_dat, *p_end;
    ptrdiff_t p_inc;
    ssize_t nbytes_tx;

    while(running) {
        // Transmit 0000
        printf("Transmitting 0000\r");
        fflush(stdout);
        p_inc = iio_buffer_step(tx_buf);
        p_end = iio_buffer_end(tx_buf);
        for (p_dat = (char *)iio_buffer_first(tx_buf, tx_i); p_dat < p_end; p_dat += p_inc) {
            ((int16_t*)p_dat)[0] = SXTN_QAM_0000_I;      // Real (i)
            ((int16_t*)p_dat)[1] = SXTN_QAM_0000_Q;      // Imag (q)
        }
        nbytes_tx = iio_buffer_push(tx_buf);
        less_than_zero_error_check((int)nbytes_tx, "nbytes_tx");
        if(!running) break;

        // Transmit 0001
        printf("Transmitting 0001\r");
        fflush(stdout);
        p_inc = iio_buffer_step(tx_buf);
        p_end = iio_buffer_end(tx_buf);
        for (p_dat = (char *)iio_buffer_first(tx_buf, tx_i); p_dat < p_end; p_dat += p_inc) {
            ((int16_t*)p_dat)[0] = SXTN_QAM_0001_I;      // Real (i)
            ((int16_t*)p_dat)[1] = SXTN_QAM_0001_Q;      // Imag (q)
        }
        nbytes_tx = iio_buffer_push(tx_buf);
        less_than_zero_error_check((int)nbytes_tx, "nbytes_tx");
        if(!running) break;

        // Transmit 0011
        printf("Transmitting 0011\r");
        fflush(stdout);
        p_inc = iio_buffer_step(tx_buf);
        p_end = iio_buffer_end(tx_buf);
        for (p_dat = (char *)iio_buffer_first(tx_buf, tx_i); p_dat < p_end; p_dat += p_inc) {
            ((int16_t*)p_dat)[0] = SXTN_QAM_0011_I;      // Real (i)
            ((int16_t*)p_dat)[1] = SXTN_QAM_0011_Q;      // Imag (q)
        }
        nbytes_tx = iio_buffer_push(tx_buf);
        less_than_zero_error_check((int)nbytes_tx, "nbytes_tx");
        if(!running) break;

        // Transmit 0010
        printf("Transmitting 0010\r");
        fflush(stdout);
        p_inc = iio_buffer_step(tx_buf);
        p_end = iio_buffer_end(tx_buf);
        for (p_dat = (char *)iio_buffer_first(tx_buf, tx_i); p_dat < p_end; p_dat += p_inc) {
            ((int16_t*)p_dat)[0] = SXTN_QAM_0010_I;      // Real (i)
            ((int16_t*)p_dat)[1] = SXTN_QAM_0010_Q;      // Imag (q)
        }
        nbytes_tx = iio_buffer_push(tx_buf);
        less_than_zero_error_check((int)nbytes_tx, "nbytes_tx");
        if(!running) break;

        // Transmit 0100
        printf("Transmitting 0100\r");
        fflush(stdout);
        p_inc = iio_buffer_step(tx_buf);
        p_end = iio_buffer_end(tx_buf);
        for (p_dat = (char *)iio_buffer_first(tx_buf, tx_i); p_dat < p_end; p_dat += p_inc) {
            ((int16_t*)p_dat)[0] = SXTN_QAM_0100_I;      // Real (i)
            ((int16_t*)p_dat)[1] = SXTN_QAM_0100_Q;      // Imag (q)
        }
        nbytes_tx = iio_buffer_push(tx_buf);
        less_than_zero_error_check((int)nbytes_tx, "nbytes_tx");
        if(!running) break;

        // Transmit 0101
        printf("Transmitting 0101\r");
        fflush(stdout);
        p_inc = iio_buffer_step(tx_buf);
        p_end = iio_buffer_end(tx_buf);
        for (p_dat = (char *)iio_buffer_first(tx_buf, tx_i); p_dat < p_end; p_dat += p_inc) {
            ((int16_t*)p_dat)[0] = SXTN_QAM_0101_I;      // Real (i)
            ((int16_t*)p_dat)[1] = SXTN_QAM_0101_Q;      // Imag (q)
        }
        nbytes_tx = iio_buffer_push(tx_buf);
        less_than_zero_error_check((int)nbytes_tx, "nbytes_tx");
        if(!running) break;

        // Transmit 0111
        printf("Transmitting 0111\r");
        fflush(stdout);
        p_inc = iio_buffer_step(tx_buf);
        p_end = iio_buffer_end(tx_buf);
        for (p_dat = (char *)iio_buffer_first(tx_buf, tx_i); p_dat < p_end; p_dat += p_inc) {
            ((int16_t*)p_dat)[0] = SXTN_QAM_0111_I;      // Real (i)
            ((int16_t*)p_dat)[1] = SXTN_QAM_0111_Q;      // Imag (q)
        }
        nbytes_tx = iio_buffer_push(tx_buf);
        less_than_zero_error_check((int)nbytes_tx, "nbytes_tx");
        if(!running) break;

        // Transmit 0110
        printf("Transmitting 0110\r");
        fflush(stdout);
        p_inc = iio_buffer_step(tx_buf);
        p_end = iio_buffer_end(tx_buf);
        for (p_dat = (char *)iio_buffer_first(tx_buf, tx_i); p_dat < p_end; p_dat += p_inc) {
            ((int16_t*)p_dat)[0] = SXTN_QAM_0110_I;      // Real (i)
            ((int16_t*)p_dat)[1] = SXTN_QAM_0110_Q;      // Imag (q)
        }
        nbytes_tx = iio_buffer_push(tx_buf);
        less_than_zero_error_check((int)nbytes_tx, "nbytes_tx");
        if(!running) break;

        // Transmit 1100
        printf("Transmitting 1100\r");
        fflush(stdout);
        p_inc = iio_buffer_step(tx_buf);
        p_end = iio_buffer_end(tx_buf);
        for (p_dat = (char *)iio_buffer_first(tx_buf, tx_i); p_dat < p_end; p_dat += p_inc) {
            ((int16_t*)p_dat)[0] = SXTN_QAM_1100_I;      // Real (i)
            ((int16_t*)p_dat)[1] = SXTN_QAM_1010_Q;      // Imag (q)
        }
        nbytes_tx = iio_buffer_push(tx_buf);
        less_than_zero_error_check((int)nbytes_tx, "nbytes_tx");
        if(!running) break;

        // Transmit 1101
        printf("Transmitting 1101\r");
        fflush(stdout);
        p_inc = iio_buffer_step(tx_buf);
        p_end = iio_buffer_end(tx_buf);
        for (p_dat = (char *)iio_buffer_first(tx_buf, tx_i); p_dat < p_end; p_dat += p_inc) {
            ((int16_t*)p_dat)[0] = SXTN_QAM_1101_I;      // Real (i)
            ((int16_t*)p_dat)[1] = SXTN_QAM_1101_Q;      // Imag (q)
        }
        nbytes_tx = iio_buffer_push(tx_buf);
        less_than_zero_error_check((int)nbytes_tx, "nbytes_tx");
        if(!running) break;

        // Transmit 1111
        printf("Transmitting 1111\r");
        fflush(stdout);
        p_inc = iio_buffer_step(tx_buf);
        p_end = iio_buffer_end(tx_buf);
        for (p_dat = (char *)iio_buffer_first(tx_buf, tx_i); p_dat < p_end; p_dat += p_inc) {
            ((int16_t*)p_dat)[0] = SXTN_QAM_1111_I;      // Real (i)
            ((int16_t*)p_dat)[1] = SXTN_QAM_1111_Q;      // Imag (q)
        }
        nbytes_tx = iio_buffer_push(tx_buf);
        less_than_zero_error_check((int)nbytes_tx, "nbytes_tx");
        if(!running) break;

        // Transmit 1110
        printf("Transmitting 1110\r");
        fflush(stdout);
        p_inc = iio_buffer_step(tx_buf);
        p_end = iio_buffer_end(tx_buf);
        for (p_dat = (char *)iio_buffer_first(tx_buf, tx_i); p_dat < p_end; p_dat += p_inc) {
            ((int16_t*)p_dat)[0] = SXTN_QAM_1110_I;      // Real (i)
            ((int16_t*)p_dat)[1] = SXTN_QAM_1110_Q;      // Imag (q)
        }
        nbytes_tx = iio_buffer_push(tx_buf);
        less_than_zero_error_check((int)nbytes_tx, "nbytes_tx");
        if(!running) break;

        // Transmit 1000
        printf("Transmitting 1000\r");
        fflush(stdout);
        p_inc = iio_buffer_step(tx_buf);
        p_end = iio_buffer_end(tx_buf);
        for (p_dat = (char *)iio_buffer_first(tx_buf, tx_i); p_dat < p_end; p_dat += p_inc) {
            ((int16_t*)p_dat)[0] = SXTN_QAM_1000_I;      // Real (i)
            ((int16_t*)p_dat)[1] = SXTN_QAM_1000_Q;      // Imag (q)
        }
        nbytes_tx = iio_buffer_push(tx_buf);
        less_than_zero_error_check((int)nbytes_tx, "nbytes_tx");
        if(!running) break;

        // Transmit 1001
        printf("Transmitting 1001\r");
        fflush(stdout);
        p_inc = iio_buffer_step(tx_buf);
        p_end = iio_buffer_end(tx_buf);
        for (p_dat = (char *)iio_buffer_first(tx_buf, tx_i); p_dat < p_end; p_dat += p_inc) {
            ((int16_t*)p_dat)[0] = SXTN_QAM_1001_I;      // Real (i)
            ((int16_t*)p_dat)[1] = SXTN_QAM_1001_Q;      // Imag (q)
        }
        nbytes_tx = iio_buffer_push(tx_buf);
        less_than_zero_error_check((int)nbytes_tx, "nbytes_tx");
        if(!running) break;

        // Transmit 1011
        printf("Transmitting 1011\r");
        fflush(stdout);
        p_inc = iio_buffer_step(tx_buf);
        p_end = iio_buffer_end(tx_buf);
        for (p_dat = (char *)iio_buffer_first(tx_buf, tx_i); p_dat < p_end; p_dat += p_inc) {
            ((int16_t*)p_dat)[0] = SXTN_QAM_1011_I;      // Real (i)
            ((int16_t*)p_dat)[1] = SXTN_QAM_1011_Q;      // Imag (q)
        }
        nbytes_tx = iio_buffer_push(tx_buf);
        less_than_zero_error_check((int)nbytes_tx, "nbytes_tx");
        if(!running) break;

        // Transmit 1010
        printf("Transmitting 1010\r");
        fflush(stdout);
        p_inc = iio_buffer_step(tx_buf);
        p_end = iio_buffer_end(tx_buf);
        for (p_dat = (char *)iio_buffer_first(tx_buf, tx_i); p_dat < p_end; p_dat += p_inc) {
            ((int16_t*)p_dat)[0] = SXTN_QAM_1010_I;      // Real (i)
            ((int16_t*)p_dat)[1] = SXTN_QAM_1010_Q;      // Imag (q)
        }
        nbytes_tx = iio_buffer_push(tx_buf);
        less_than_zero_error_check((int)nbytes_tx, "nbytes_tx");
    }
}

// Transmit data with 16QAM modulation scheme. This function continuously transmits packets 
// until entirity of data has been transmitted. Requires file pointer to open file which contains data to transmit.
void sxtn_qam_transmit(FILE *transmission_data_fp) {
    printf("\nBeginning 16QAM transmission, press ctrl+c to stop\n\n");
    char *p_dat;
    ptrdiff_t p_inc;
    ssize_t nbytes_tx;
    int16_t i, q;
    int fourbit, fourbits_filled, fourbits_remaining, num_bytes_to_read, bytes_read, transmission_data_complete, packet_num;
    long file_size, total_data_bytes_transmitted;

    // Get file size to determine progress percentages, then print message
    file_size = get_file_size(transmission_data_fp);
    print_file_size(file_size);
    printf("Transmitting...\n");

    // Set up local array with preamble 
    unsigned char preamble[PREAMBLE_SIZE_BYTES] = {
        0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 
        0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33
    };

    // Set up local array with sync word 
    unsigned char sync_word[SYNC_WORD_SIZE_BYTES] = {
        0x33, 0xF7
    };

    // Set up local memory to store file data upon reading file
    // We will read a maximum of DATA_BYTES_PER_READ bytes at a time
    unsigned char file_data[DATA_BYTES_PER_READ];

    // Initialize some variables
    total_data_bytes_transmitted = 0;
    transmission_data_complete = false;
    packet_num = 0;

    // Transmit packets until interrupt occurs or data transmission is complete
    while(running && !transmission_data_complete) {
        // Get buffer step size and pointer to start of buffer
        p_inc = iio_buffer_step(tx_buf);
        p_dat = (char *)iio_buffer_first(tx_buf, tx_i);

        // Fill buffer with preamble, four bits at a time
        for(fourbits_filled = 0; fourbits_filled < PREAMBLE_SIZE_FOURBITS; fourbits_filled++, p_dat += p_inc) {
            fourbit = (preamble[fourbits_filled / 2] >> ((1 - (fourbits_filled % 2)) * 4)) & 0b1111;
            sxtn_qam_modulation(fourbit, &i, &q);
            ((int16_t*)p_dat)[0] = i;
            ((int16_t*)p_dat)[1] = q;
        }

        // Fill buffer with sync word, four bits at a time
        for(fourbits_filled = 0; fourbits_filled < SYNC_WORD_SIZE_FOURBITS; fourbits_filled++, p_dat += p_inc) {
            fourbit = (sync_word[fourbits_filled / 2] >> ((1 - (fourbits_filled % 2)) * 4)) & 0b1111;
            sxtn_qam_modulation(fourbit, &i, &q);
            ((int16_t*)p_dat)[0] = i;
            ((int16_t*)p_dat)[1] = q;
        }

        // Fill the rest of the buffer with data, four bits at a time
        fourbits_filled = 0;
        fourbits_remaining = TX_BUFFER_SIZE_FOURBITS - PREAMBLE_SIZE_FOURBITS - SYNC_WORD_SIZE_FOURBITS;
        while(fourbits_remaining > 0) {
            // Determine how many bytes should be read from file
            num_bytes_to_read = ((fourbits_remaining / 2) < DATA_BYTES_PER_READ) ? (fourbits_remaining / 2) : DATA_BYTES_PER_READ;

            // Read bytes from file
            bytes_read = fread(file_data, 1, num_bytes_to_read, transmission_data_fp);  
            
            // Increment total data bytes read
            total_data_bytes_transmitted += bytes_read;

            // Check if end of transmission data was reached
            if(bytes_read != num_bytes_to_read) {
                // TODO: Check for error
                // Fill packet with remaining data
                while((fourbits_filled / 2) < bytes_read) {
                    fourbit = (file_data[fourbits_filled / 2] >> ((1 - (fourbits_filled % 2)) * 4)) & 0b1111;
                    sxtn_qam_modulation(fourbit, &i, &q);
                    ((int16_t*)p_dat)[0] = i;
                    ((int16_t*)p_dat)[1] = q;
                    fourbits_filled++;
                    fourbits_remaining--;
                    p_dat += p_inc;  
                }
                
                // Pad rest of packet with 0's
                while(fourbits_remaining > 0) {
                    ((int16_t*)p_dat)[0] = SXTN_QAM_0000_I;
                    ((int16_t*)p_dat)[1] = SXTN_QAM_0000_Q;
                    p_dat += p_inc;
                    fourbits_remaining--;
                }

                // Set flag and break out of loop
                transmission_data_complete = true;
                break;
            }

            // End of transmission data was not reached, so continue to fill the the packet with data
            while((fourbits_filled / 2) < bytes_read) {
                fourbit = (file_data[fourbits_filled / 2] >> ((1 - (fourbits_filled % 2)) * 4)) & 0b1111;
                sxtn_qam_modulation(fourbit, &i, &q);
                ((int16_t*)p_dat)[0] = i;
                ((int16_t*)p_dat)[1] = q;
                fourbits_filled++;
                fourbits_remaining--;
                p_dat += p_inc;  
            }
        }

        // Transmit packet and perform error check
        packet_num++;
        print_progress_bar(total_data_bytes_transmitted, file_size, PROGRESS_BAR_LENGTH);
        nbytes_tx = iio_buffer_push(tx_buf);
        less_than_zero_error_check((int)nbytes_tx, "nbytes_tx");
    }
    printf("\n");   // Needed since print_progress_bar does not print a newline character
}

// Takes in user command to operate transmitter until transmitter is shut down
void operate_transmitter() {
    int mode;
    int terminate = false;
    char file_path[MAX_PATH_LENGTH];
    FILE *transmission_data_fp;
    while(!terminate) {
        printf("\nTransmitter operation menu.\n");
        printf("\nPlease enter a number to select transmitter mode:\n \
        1 - QPSK example\n \
        2 - QPSK transmission test\n \
        3 - QPSK transmission of data\n \
        4 - 16QAM example\n \
        5 - 16QAM transmission test\n \
        6 - 16QAM transmission of data\n \
        7 - Shutdown transmitter\n\n");
        scanf("%d", &mode);
        print_seperator();
        switch(mode) {
            case 1:
                qpsk_example();               // Perform QPSK example
                running = true;               // Reset SIGINT interrupt flag
                print_seperator();
                break;
            case 2:
                qpsk_transmit_test();         // Transmit test data with qpsk scheme
                running = true;               // Reset SIGINT interrupt flag
                print_seperator();
                break;
            case 3:
                while(1) {
                    printf("\nPlease enter the full path to a file on the ADALM-PLUTO file system that you want to transmit.\n");
                    printf("Max path length is %d characters. If you want to exit back to operation menu, type 'exit'.\n\n", MAX_PATH_LENGTH);
                    scanf("%s", file_path);
                    if(strcmp("exit", file_path) == 0) {
                        running = false;
                        break;
                    }
                    // Zip file ???
                    transmission_data_fp = fopen(file_path, "rb");      // Open file for reading
                    if(transmission_data_fp) {                          // Break if valid file is opened, otherwise print error and prompt again
                        break;
                    }
                    printf("\nNo valid file exists at the provided path. Please try again.");
                }
                if (running) {
                    qpsk_transmit(transmission_data_fp);                // Transmit data 
                    fclose(transmission_data_fp);                       // Close file
                }
                running = true;
                print_seperator();
                break;
            case 4:
                sxtn_qam_example();         // Perform 16QAM example
                running = true;             // Reset SIGINT interrupt flag
                print_seperator();
                break;
            case 5:
                sxtn_qam_transmit_test();   // Transmit test data with 16QAM scheme
                running = true;             // Reset SIGINT interrupt flag
                print_seperator();
                break;
            case 6:
                while(1) {
                    printf("\nPlease enter the full path to a file on the ADALM-PLUTO file system that you want to transmit.\n");
                    printf("Max path length is %d characters. If you want to exit back to operation menu, type 'exit'.\n\n", MAX_PATH_LENGTH);
                    scanf("%s", file_path);
                    if(strcmp("exit", file_path) == 0) {
                        running = false;
                        break;
                    }
                    // Zip file ???
                    transmission_data_fp = fopen(file_path, "rb");      // Open file for reading
                    if(transmission_data_fp) {                          // Break if valid file is opened, otherwise print error and prompt again
                        break;
                    }
                    printf("\nNo valid file exists at the provided path. Please try again.");
                }
                if (running) {
                    sxtn_qam_transmit(transmission_data_fp);            // Transmit data 
                    fclose(transmission_data_fp);                       // Close file
                }
                running = true;
                print_seperator();
                break;
            case 7:
                terminate = true;
                break;
            default:
                printf("Invalid selection, please try again.\n");
                while(getchar() != '\n');       // Clear input buffer
                break;
        }
    }
}

int main() {
    signal(SIGINT, handle_sig);     // Set up ctrl + c signal interrupt
    print_seperator();              // Print a seperator to stdout
    print_start_message();          // Prints start message to console
    print_seperator();              // Print a seperator to stdout
    sleep(SHORT_MESSAGE_DELAY);     // Delay between CLI messages
    set_up_context();               // Set up context (ADALM-PLUTO)
    set_up_device();                // Set up device (AD9361 Physical Layer)
    config_device();                // Configure device (AD9361 Physical Layer)
    set_up_device_2();              // Set up device 2 (AD9361 Tx Output Driver)
    set_up_streaming_channels();    // Set up streaming channels (tx_i and tx_q)
    enable_streaming_channels();    // Enable streaming channels (tx_i and tx_q)
    set_up_buffer();                // Set up buffer (tx_buf)
    print_seperator();              // Print a seperator to stdout
    sleep(SHORT_MESSAGE_DELAY);     // Delay between CLI messages
    operate_transmitter();          // Transmitter operation via user input       

    // Clean up 
    shutdown();

    return 0; 
}
