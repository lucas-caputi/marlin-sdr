#ifndef TRANSMITTER_H
#define TRANSMITTER_H

#include <stdio.h>
#include <unistd.h>
#include <iio.h>
#include <signal.h>
#include <string.h>
#include <math.h>

// Helper macros
#define MHZ(x) ((long long)(x*1000000.0 + .5))
#define GHZ(x) ((long long)(x*1000000000.0 + .5))

// QPSK values
#define QPSK_NEG (int16_t)-23152
#define QPSK_POS (int16_t)23152
#define QPSK_00_I QPSK_POS
#define QPSK_00_Q QPSK_POS
#define QPSK_01_I QPSK_POS
#define QPSK_01_Q QPSK_NEG
#define QPSK_10_I QPSK_NEG
#define QPSK_10_Q QPSK_POS
#define QPSK_11_I QPSK_NEG
#define QPSK_11_Q QPSK_NEG

// 16QAM values
// Mapping is as follows:
//   (-3, 3)   (-1, 3)   (1, 3)    (3, 3)
//     0000     0001      0011      0010

//   (-3, 1)   (-1, 1)   (1, 1)    (3, 1)
//     0100     0101      0111      0110

//   (-3, -1)  (-1, -1)  (1, -1)   (3, -1)
//     1100     1101      1111      1110

//   (-3, -3)  (-1, -3)  (1, -3)   (3, -3)
//     1000     1001      1011      1010
#define SXTN_QAM_NEG_ONE    (int16_t)-7717      // -(23152/3)
#define SXTN_QAM_POS_ONE    (int16_t)7717       // (23152/3)
#define SXTN_QAM_NEG_THREE  (int16_t)-23151
#define SXTN_QAM_POS_THREE  (int16_t)23151
#define SXTN_QAM_0000_I SXTN_QAM_NEG_THREE
#define SXTN_QAM_0000_Q SXTN_QAM_POS_THREE
#define SXTN_QAM_0001_I SXTN_QAM_NEG_ONE
#define SXTN_QAM_0001_Q SXTN_QAM_POS_THREE
#define SXTN_QAM_0010_I SXTN_QAM_POS_THREE
#define SXTN_QAM_0010_Q SXTN_QAM_POS_THREE
#define SXTN_QAM_0011_I SXTN_QAM_POS_ONE
#define SXTN_QAM_0011_Q SXTN_QAM_POS_THREE
#define SXTN_QAM_0100_I SXTN_QAM_NEG_THREE
#define SXTN_QAM_0100_Q SXTN_QAM_POS_ONE
#define SXTN_QAM_0101_I SXTN_QAM_NEG_ONE
#define SXTN_QAM_0101_Q SXTN_QAM_POS_ONE
#define SXTN_QAM_0110_I SXTN_QAM_POS_THREE
#define SXTN_QAM_0110_Q SXTN_QAM_POS_ONE
#define SXTN_QAM_0111_I SXTN_QAM_POS_ONE
#define SXTN_QAM_0111_Q SXTN_QAM_POS_ONE
#define SXTN_QAM_1000_I SXTN_QAM_NEG_THREE
#define SXTN_QAM_1000_Q SXTN_QAM_NEG_THREE
#define SXTN_QAM_1001_I SXTN_QAM_NEG_ONE
#define SXTN_QAM_1001_Q SXTN_QAM_NEG_THREE
#define SXTN_QAM_1010_I SXTN_QAM_POS_THREE
#define SXTN_QAM_1010_Q SXTN_QAM_NEG_THREE
#define SXTN_QAM_1011_I SXTN_QAM_POS_ONE
#define SXTN_QAM_1011_Q SXTN_QAM_NEG_THREE
#define SXTN_QAM_1100_I SXTN_QAM_NEG_THREE
#define SXTN_QAM_1100_Q SXTN_QAM_NEG_ONE
#define SXTN_QAM_1101_I SXTN_QAM_NEG_ONE
#define SXTN_QAM_1101_Q SXTN_QAM_NEG_ONE
#define SXTN_QAM_1110_I SXTN_QAM_POS_THREE
#define SXTN_QAM_1110_Q SXTN_QAM_NEG_ONE
#define SXTN_QAM_1111_I SXTN_QAM_POS_ONE
#define SXTN_QAM_1111_Q SXTN_QAM_NEG_ONE

// Transmit configuration
#define IP_ADDRESS "ip:192.168.2.8" // Change this to the IP address of your ADALM-PLUTO
#define SAMPLE_RATE MHZ(20)
#define TX_BANDWIDTH MHZ(20)
#define TX_LO GHZ(0.915)            // Center frequency         
#define TX_RF_PORT_SELECT "A"
#define TEST_TRANSMIT_AMOUNT 65536  // Amount of complex signals to be transmitted in a buffer/packet (2^16)
#define TEST_TRANSMIT_AMOUNT_BYTES (TEST_TRANSMIT_AMOUNT * 4)   // Each complex signal is 32 bits (16 bit I + 16 bit Q)

// Packet/buffer configuration
// Packet/buffer format = preamble (1024 bits) + sync word (1024 bits) + data (2095104 bits or 261888 bytes)
#define TX_BUFFER_SIZE_BITS (TEST_TRANSMIT_AMOUNT * 32)     // Buffer size in bits (each bit pair is represented by 16-bit I value and 16-bit Q value)
#define TX_BUFFER_SIZE_FOURBITS TEST_TRANSMIT_AMOUNT        // Amount of fourbits that fit in packet/buffer
#define TX_BUFFER_SIZE_BITPAIRS TEST_TRANSMIT_AMOUNT        // Amount of bitpairs that fit in packet/buffer
#define PREAMBLE_SIZE_BYTES 18          // Preamble size in bytes
#define PREAMBLE_SIZE_FOURBITS 36       // Preamble size in fourbits
#define PREAMBLE_SIZE_BITPAIRS 72       // Preamble size in bitpairs 
#define PREAMBLE_SIZE_BITS 144          // Preamble size in bits
#define SYNC_WORD_SIZE_BYTES 2          // Sync word size in bytes
#define SYNC_WORD_SIZE_FOURBITS 4       // Sync word size in fourbits
#define SYNC_WORD_SIZE_BITPAIRS 8       // Sync word size in bitpairs
#define SYNC_WORD_SIZE_BITS 16          // Sync word size in bits
#define DATA_BYTES_PER_READ 64000       // Amount of bytes to read at a time from data file

// Maximum values
#define MAX_PATH_LENGTH 1000

// Command line interface config values
#define PROGRESS_BAR_LENGTH 36
#define SHORT_MESSAGE_DELAY 2   // Seconds
#define LONG_MESSAGE_DELAY 4    // Seconds

// Function prototypes
void handle_sig();
void shutdown();
void print_start_message();
void print_seperator();
void null_error_check(void *ptr, char *descr);
void less_than_zero_error_check(int val, char *descr);
void set_up_context();
void set_up_device();
void config_device();
void set_up_device_2();
void set_up_streaming_channels();
void enable_streaming_channels();
void set_up_buffer();
long get_file_size(FILE *fp);
void print_file_size(unsigned long long bytes);
void print_progress_bar(int progress, int total, int barWidth);
int convert_bits_to_binary(int16_t a, int16_t b);
void qpsk_modulation(int bitpair, int16_t *i, int16_t *q);
void qpsk_example();
void qpsk_transmit_test();
void qpsk_transmit(FILE *transmission_data_fp);
void operate_transmitter();
void sxtn_qam_modulation(int fourbit, int16_t *i, int16_t *q);
void sxtn_qam_example();
void sxtn_qam_transmit_test();
void sxtn_qam_transmit(FILE *transmission_data_fp);

#endif /* RADIO_H */