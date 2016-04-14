/*
 * tsb-c Copyright (C) 2016 Ron Pedde <ron@pedde.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include <sys/ioctl.h>

#include "debug.h"

#define DEFAULT_SERIAL  "/dev/ttyUSB0"
#define DEFAULT_BAUD    9600

#define PEXIT(s) { \
        ERROR("%s: %s", (s), strerror(errno)); \
        exit(EXIT_FAILURE); }

#define FEXIT(fmt, args...) { \
        debug_printf(DBG_ERROR, "[ERROR] %s:%d (%s): " fmt "\n", __FILE__, __LINE__, __FUNCTION__, ##args); \
        exit(EXIT_FAILURE); }

typedef struct hexdata_t {
    uint8_t *data;
    uint16_t len;
    uint16_t addr;
} hexdata_t;

typedef struct deviceinfo_t {
    uint8_t magic[3];
    uint8_t firmware_date[2];
    uint8_t firmware_status;
    uint8_t signature[3];
    uint8_t page_size;
    uint16_t mem_size;
    uint16_t eeprom_size;
    uint16_t extra;
} deviceinfo_t;

void usagequit(char *a0, int exitval) {
    printf("%s: [options] <action> [action args]\n\n", a0);

    printf("Valid options:\n");
    printf("  -s <device>  serial device (default: %s)\n", DEFAULT_SERIAL);
    printf("  -b <baud>    baud (default: %d)\n", DEFAULT_BAUD);
    printf("  -d <level>   debug level 1-5 (default: 2)\n");
    printf("  -h           this\n");
    printf("\n");

    printf("Valid actions:\n");
    printf("  flash <file>      upload a hex file to flash");
    printf("\n");

    exit(exitval);
}

void hexdump (void *addr, int len) {
    int i;
    unsigned char buf[17];
    unsigned char *pc = (unsigned char*)addr;

    if(!len)
        return;

    for(i = 0; i < len; i++) {
        if((i % 16) == 0) {
            if(i != 0)
                printf("  %s\n", buf);

            printf("  %04x ", i);
        }

        printf(" %02x", pc[i]);

        if((pc[i] < 0x20) || (pc[i] > 0x7e))
            buf[i % 16] = '.';
        else
            buf[i % 16] = pc[i];
        buf[(i % 16) + 1] = '\0';
    }

    while((i % 16) != 0) {
        printf("   ");
        i++;
    }

    printf("  %s\n", buf);
}


speed_t get_speed(int speed) {
    switch(speed) {
    case 0:
        return B0;
    case 50:
        return B50;
    case 75:
        return B75;
    case 110:
        return B110;
    case 134:
        return B134;
    case 150:
        return B150;
    case 200:
        return B200;
    case 300:
        return B300;
    case 600:
        return B600;
    case 1200:
        return B1200;
    case 1800:
        return B1800;
    case 2400:
        return B2400;
    case 4800:
        return B4800;
    case 9600:
        return B9600;
    case 19200:
        return B19200;
    case 38400:
        return B38400;
    case 57600:
        return B57600;
    case 115200:
        return B115200;
    case 230400:
        return B230400;
    }

    fprintf(stderr, "Invalid baud rate\n");
    exit(EXIT_FAILURE);
    return B0;
}

void set_rts(int serial_fd, int level) {
    int status;

    if(ioctl(serial_fd, TIOCMGET, &status) == -1)
        PEXIT("TIOCMGET");

    if(level)
        status |= TIOCM_RTS;
    else
        status &= ~TIOCM_RTS;

    if(ioctl(serial_fd, TIOCMSET, &status) == -1)
        PEXIT("TIOCMSET");
}


int get_serial_fd(char *device, int baud) {
    int fd;
    struct termios term;

    DEBUG("Opening %s at %d baud", device, baud);

    if ((fd = open(device, O_RDWR | O_NOCTTY | O_NONBLOCK)) < 0)
        PEXIT("open");

    if(tcgetattr(fd, &term) < 0)
        PEXIT("tcgetattr");

    cfsetspeed(&term, get_speed(baud));
    cfmakeraw(&term);
    /* term.c_cflag |= CRTSCTS | CLOCAL; */
    /* term.c_oflag = 0; */

    tcflush(fd, TCIOFLUSH);
    if(tcsetattr(fd, 0, &term) < 0)
        PEXIT("tcsetattr");

    DEBUG("Opened as %d", fd);

    return fd;
}

hexdata_t *read_hex_file(char *hexfile) {
    hexdata_t *phex;
    int matches;
    int have_start_addr = 0;
    FILE *f;
    char buf[512];
    uint16_t current_addr;
    int line=0;

    phex = (hexdata_t*)malloc(sizeof(hexdata_t));
    if(!phex)
        PEXIT("malloc");

    phex->len = 0;
    phex->addr = 0;
    phex->data = (uint8_t*)malloc(65536);

    memset(phex->data, 0xFF, 65536);

    if(!phex->data)
        PEXIT("malloc");

    if (!(f = fopen(hexfile, "r")))
        PEXIT("fopen");

    while(fgets(buf, sizeof(buf), f) != NULL) {
        uint8_t bytes, recordtype;
        uint16_t addr;

        int i1, i2, i3;

        line++;

        if(strlen(buf) == sizeof(buf))
            FEXIT("Line %d: line too long", line);

        matches = sscanf(buf, ":%02X%04X%02X", &i1, &i2, &i3);
        bytes = i1;
        addr = i2;
        recordtype = i3;

        if(matches != 3)
            FEXIT("Line %d: Invalid syntax: %s", line, buf);

        DEBUG("Line %d: addr %04x, %02x bytes, record type %02x",
              line, addr, bytes, recordtype);

        /* set start address */
        if(!have_start_addr) {
            have_start_addr = 1;
            phex->addr = addr;
            current_addr = addr;
        } else {
            if((current_addr != addr) && (recordtype == 0))
                FEXIT("Line %d: discontiguous address: addr %04x != %04x",
                      line, addr, current_addr);
        }


        if(recordtype > 1)
            INFO("Line %d: Skipping record type %d", line, recordtype);

        if(recordtype == 0) {
            uint8_t checksum = ((addr >> 8) & 0xFF) + (addr & 0xFF) + bytes + recordtype;

            for(int i=0; i <= bytes; i++) {
                uint8_t item;

                sscanf(&buf[9 + (2*i)], "%02x", &item);
                if(i == bytes) {
                    /* checksum */
                    DEBUG("Checksum: %02x, checksum byte: %02x",
                          checksum, item);

                    checksum += item;
                    if(checksum)
                        FEXIT("Line: %d: checksum error", line);
                } else {
                    phex->data[current_addr] = item;
                    current_addr++;
                    checksum += item;
                }
            }
        }
    }

    phex->len = current_addr - phex->addr;

    INFO("Read %d bytes of data starting at address %04x",
         phex->len, phex->addr);

    fclose(f);

    return phex;
}

ssize_t device_read(int fd, char *buf, ssize_t len, int timeout) {
    fd_set readset;
    struct timeval tv;
    ssize_t bytes_read = 0;
    ssize_t total_bytes_read = 0;
    int res;

    DEBUG("Starting device read of %d bytes", len);

    while(bytes_read < len) {
        FD_ZERO(&readset);
        tv.tv_sec = timeout;
        tv.tv_usec = 0;
        FD_SET(fd, &readset);

        // should probably hook errorset too..
        while(((res = select(fd + 1, &readset, 0, 0, &tv)) == -1) && (errno == EINTR));

        if(res < 0)
            PEXIT("select");

        if(res == 0) { /* timeout */
            hexdump(buf, total_bytes_read);
            return total_bytes_read;
        }

        bytes_read = read(fd, &buf[total_bytes_read], len - total_bytes_read);
        DEBUG("Read %d bytes from device", bytes_read);

        if(bytes_read == 1) {
            DEBUG("Val: %02x", (uint8_t)buf[total_bytes_read]);
        }

        if (bytes_read < 0)
            PEXIT("read");

        if (bytes_read == 0) {
            hexdump(buf, total_bytes_read);
            return total_bytes_read;
        }

        total_bytes_read += bytes_read;
    }

    return total_bytes_read;
}

void expect_char(int serial_fd, char what) {
    char blah[64];
    ssize_t bytes_read;

    bytes_read = device_read(serial_fd, blah, 1, 10);

    DEBUG("read %d bytes", bytes_read);

    if(blah[bytes_read - 1] != what)
        FEXIT("Error waiting for '%c'", what);

    /* if((bytes_read != 1) || (rq != what)) */
    /*     FEXIT("Error waiting for '%c'", what); */
}

void expect_request(int serial_fd) {
    expect_char(serial_fd, '?');
}

void expect_confirm(int serial_fd) {
    expect_char(serial_fd, '!');
}


deviceinfo_t *enter_program_mode(int serial_fd) {
    deviceinfo_t *pdi;
    ssize_t bytes_read;
    char buf[64];

    pdi = (deviceinfo_t *)malloc(sizeof(deviceinfo_t));
    if(!pdi)
        PEXIT("malloc");

    /* wait a sec for the firmware to start up */
    sleep(1);
    if(write(serial_fd, "@@@", 3) < 3)
        PEXIT("write @");

    bytes_read = device_read(serial_fd, (char*)pdi, sizeof(deviceinfo_t), 3);
    if(bytes_read < sizeof(deviceinfo_t))
        PEXIT("read devinfo");

    if(pdi->magic[0] != 't' &&
       pdi->magic[1] != 's' &&
       pdi->magic[2] != 'b')
        FEXIT("Error talking to firmware");

    /* why? */
    pdi->mem_size *= 2;
    pdi->eeprom_size += 1;
    pdi->page_size *= 2;

    DEBUG("Firmware Date:    %02x %02x",
          pdi->firmware_date[0],
          pdi->firmware_date[1]);
    DEBUG("Firmware Status:  %02x", pdi->firmware_status);
    DEBUG("Device Signature: %02x %02x %02x",
          pdi->signature[0],
          pdi->signature[1],
          pdi->signature[2]);
    DEBUG("Page Size:        %02x", pdi->page_size);
    DEBUG("Memory Size:      %04x", pdi->mem_size);
    DEBUG("EEProm Size:      %04x", pdi->eeprom_size);

    /* soak up any trailing characters */
    DEBUG("Soaking up trailing characters");
    device_read(serial_fd, buf, sizeof(buf), 2);
    DEBUG("Ready to program");
    return pdi;
}


int upload_hex_file(int serial_fd, char *hexfile) {
    hexdata_t *phex;
    deviceinfo_t *pdi;
    ssize_t bytes_written;

    phex = read_hex_file(hexfile);
    pdi = enter_program_mode(serial_fd);

    int pages = ((phex->addr + phex->len) / pdi->page_size);

    if (phex->len % pdi->page_size)
        pages++;

    DEBUG("About to write %d pages of flash data", pages);

    DEBUG("Writing 'F'");

    if((write(serial_fd, "F", 1)) < 1)
        PEXIT("write F");

    for(int i = 0; i < pages; i++) {
        expect_request(serial_fd);

        DEBUG("Writing '!'");
        if((write(serial_fd, "!", 1)) < 1)
            PEXIT("write !");

        DEBUG("Writing %d bytes at address %d (page %d)",
              pdi->page_size, pdi->page_size * i, i);

        bytes_written = write(serial_fd,
                              (char*)&phex->data[pdi->page_size * i],
                              pdi->page_size);

        if(bytes_written != pdi->page_size)
            FEXIT("Error writing flash page");
    }

    expect_request(serial_fd);
    if((write(serial_fd, "?", 1)) < 1)
        PEXIT("write ?");

    expect_confirm(serial_fd);

    if((write(serial_fd, "a", 1)) < 1)
        PEXIT("write q");
}

int main(int argc, char *argv[]) {
    int option;
    char *serial = DEFAULT_SERIAL;
    char *action = NULL;
    char *hexfile = NULL;
    int baud = DEFAULT_BAUD;
    int debuglevel=2;
    int serial_fd;

    while((option = getopt(argc, argv, "s:b:d:h")) != -1) {
        switch(option) {
        case 's':
            serial = optarg;
            break;
        case 'b':
            baud = atoi(optarg);
            break;
        case 'd':
            debuglevel = atoi(optarg);
            break;
        case 'h':
            usagequit(argv[0], 0);
            break;
        default:
            fprintf(stderr, "Invalid argument: '%c'\n", option);
            usagequit(argv[0], 1);
            break;
        }
    }

    debug_level(debuglevel);

    if (optind == argc) {
        fprintf(stderr, "Missing action argument\n");
        usagequit(argv[0], 1);
    }

    action = argv[optind];

    if(strcmp(action, "flash") == 0) {
        if(argc - optind != 2) {
            fprintf(stderr, "Expecting 1 argument for 'flash'\n\n");
            usagequit(argv[0], 1);
        }
        hexfile = argv[optind + 1];

        INFO("Opening serial port");
        serial_fd = get_serial_fd(serial, baud);

        /* set_rts(serial_fd, 0); */
        /* usleep(200000); */
        /* set_rts(serial_fd, 1); */

        INFO("Uploading hex file");
        upload_hex_file(serial_fd, hexfile);
        INFO("Closing serial port");
        close(serial_fd);
    } else {
        fprintf(stderr, "Invalid action: %s\n\n", action);
        usagequit(argv[0], 1);
    }

    INFO("Done");
    exit(0);
}
