// C library headers
#define _DEFAULT_SOURCE
#include "logger/tserv.h"

extern int serial_port, file_port;
extern struct termios tty_opts_backup, tty_opts_raw;
extern bool raw_set2, got_time;

const char cmd_text[][32] = {
	" ",
	" AC Charger ON",
	" AC Charger OFF",
	" System Data",
};

char get_one_char(void)
{
	struct termios tty_opts_backup, tty_opts_raw;

	if (!isatty(STDIN_FILENO)) {
//		printf("Error: stdin is not a TTY\n");
//		exit(1);
		return '\0'; // return a null char
	}

	// Back up current TTY settings
	tcgetattr(STDIN_FILENO, &tty_opts_backup);
	raw_set2 = true; // restore on signal exit

	// Change TTY settings to raw mode
	cfmakeraw(&tty_opts_raw);
	tty_opts_raw.c_cc[VMIN] = 0;
	tty_opts_raw.c_cc[VTIME] = 1;
	tcsetattr(STDIN_FILENO, TCSANOW, &tty_opts_raw);

	// Read characters from stdin
	int c = '\0';

	read(STDIN_FILENO, &c, 1);

	// Restore previous TTY settings
	raw_set2 = false;
	tcsetattr(STDIN_FILENO, TCSANOW, &tty_opts_backup);
	return c;
}

/*
 * read from the serial port, check for time T command and log data to storage file
 */
int get_log(int mode)
{
	char read_buf[256], msg[256], c, k;
	int num_bytes, rcode = 0;

	num_bytes = read(serial_port, &read_buf[0], sizeof(read_buf));
	if (num_bytes < 0) {
		printf("Error reading: %s\r\n", strerror(errno));
		return errno;
	}

	file_port = open(LOG_FILE, O_RDWR | O_APPEND);
	if (file_port < 0) {
		printf("File Error %i from open: %s\r\n", errno, strerror(errno));
		return errno;
	} else {
		if (num_bytes) {
			printf("Read %i bytes. Received message: %c\r\n", num_bytes, read_buf[0]);
			if (!got_time && (read_buf[0] == 'T')) { // process the T server time command
				got_time = true;
				int k = 0;
				// Write to serial port
				sprintf(msg, "T%lut", time(0)); // format UNIX time in ASCII
				do {
					write(serial_port, &msg[k], 1); // use gaps between bytes for slow controller time processing
					usleep(1000);
				} while (k++ < strlen(msg));

				printf("Send time %s\r\n", msg);
				rcode = 1;
			} else { // just write the serial buffer to the log file
				write(file_port, &read_buf[0], num_bytes);
				got_time = false;
			}
			c = get_one_char();
			if (c == 'V' || c == 'v' || c == '#') {// check for valid commands

				switch (c) {
				case 'V':
					k = 1;
					break;
				case 'v':
					k = 2;
					break;
				case '#':
					k = 3;
					break;
				default:
					k = 0;
					break;
				}
				write(serial_port, &c, 1);
				printf("\r\nSend Command %c %s\r\n", c, cmd_text[(int) k]);
			}
		}

		close(file_port);
	}
	return rcode;
}
