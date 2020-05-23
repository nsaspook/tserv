/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   main.c
 * Author: root
 *
 * Created on April 16, 2020, 12:28 PM
 */
#define _DEFAULT_SOURCE // for BSDisms in GCC
#include "tserv.h"

int serial_port, file_port;
struct termios tty, tty_backup;
struct termios tty_opts_backup, tty_opts_raw;
bool raw_set1 = false, raw_set2 = false, got_time=false;

void reset_input_mode(void)
{
	if (raw_set1)
		tcsetattr(serial_port, TCSANOW, &tty_backup);
	if (raw_set2)
		tcsetattr(STDIN_FILENO, TCSANOW, &tty_opts_backup);
}

/*
 * MBMC UNIX time server and data logger
 */
int main(int argc, char** argv)
{
	int t_set = false;
	char junk;

#ifdef __x86_64__ // for PC with REAL hardware serial port
	serial_port = open("/dev/ttyS0", O_RDWR);
#else // for RPi with USB converter serial port
	serial_port = open("/dev/ttyUSB0", O_RDWR);
#endif
	if (serial_port < 0) {
		printf("Port Error %i from open: %s\r\n", errno, strerror(errno));
		return errno;
	}
	memset(&tty, 0, sizeof tty); // make sure it's clear
	memset(&tty_backup, 0, sizeof tty_backup);

	// Read in existing settings, and handle any error
	if (tcgetattr(serial_port, &tty) != 0) {
		printf("TTY Error %i from tcgetattr tty: %s\r\n", errno, strerror(errno));
		return errno;
	}
	if (tcgetattr(serial_port, &tty_backup) != 0) {
		printf("TTY Error %i from tcgetattr tty_backup: %s\r\n", errno, strerror(errno));
		return errno;
	}
	raw_set1 = true; // restore on signal exit
	atexit(reset_input_mode); // restore the original terminal modes before exiting or terminating with a signal.

	/*
	 * most of this could be done using cfmakeraw like in tserv.c
	 */

	tty.c_cflag &= ~PARENB;
	tty.c_cflag &= ~CSTOPB;
	tty.c_cflag &= ~CSIZE;
	tty.c_cflag |= CS8;
	tty.c_cflag |= CREAD | CLOCAL;

	tty.c_lflag &= ~ICANON;
	tty.c_lflag &= ~ECHO;
	tty.c_lflag &= ~ECHOE;
	tty.c_lflag &= ~ECHONL;
	tty.c_lflag &= ~ISIG;
	tty.c_iflag &= ~(IXON | IXOFF | IXANY);
	tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);

	tty.c_oflag &= ~OPOST;
	tty.c_oflag &= ~ONLCR;

	tty.c_cc[VTIME] = 10; // Wait for up to 1s, returning as soon as any data is received.
	tty.c_cc[VMIN] = 0;

	// Set in/out baud rate to be 115200
	cfsetispeed(&tty, B115200);
	cfsetospeed(&tty, B115200);

	// Save tty settings, also checking for error
	if (tcsetattr(serial_port, TCSANOW, &tty) != 0) {
		printf("TTY set Error %i from tcsetattr: %s\n", errno, strerror(errno));
		return errno;
	}
	// clear the receive buffer
	while (read(serial_port, &junk, 1));

	while (true) {
		if (t_set) {
			get_log(1);
		} else {
			t_set = get_log(0);
		}
	}
	raw_set1 = false;
	close(serial_port);
	return(EXIT_SUCCESS);
}

