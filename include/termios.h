/**
 * http://pubs.opengroup.org/onlinepubs/9699919799/
 */
#include <sys/types.h>

typedef unsigned char cc_t;
typedef unsigned int speed_t;
typedef unsigned int tcflag_t;

#define NCCS 32

struct termios {
    tcflag_t c_iflag;   // Input mode flags.
    tcflag_t c_oflag;   // Output mode flags.
    tcflag_t c_cflag;   // Control mode flags.
    tcflag_t c_lflag;   // Local mode flags.
    cc_t c_cc[NCCS];    // Control characters.
};

/* c_cc characters */
#define VINTR   0
#define VQUIT   1
#define VERASE  2
#define VKILL   3
#define VEOF    4
#define VTIME   5
#define VMIN    6
#define VSWTC   7
#define VSTART  8
#define VSTOP   9
#define VSUSP   10
#define VEOL    11
#define VREPRINT 12
#define VDISCARD 13
#define VWERASE  14
#define VLNEXT   15
#define VEOL2    16

/* c_iflag bits */
#define BRKINT  0000001     // Signal interrupt on break
#define ICRNL   0000002     // Map CR to NL on input
#define IGNBRK  0000004     // Ignore break condition
#define IGNCR   0000010     // Ignore CR
#define IGNPAR  0000020     // Ignore characters with parity errors
#define INLCR   0000040     // Map NL to CR on input
#define INPCK   0000100     // Enable input parity check
#define ISTRIP  0000200     // Strip character
#define IXANY   0000400     // Enable any character to restart output
#define IXOFF   0001000     // Enable start/stop input control
#define IXON    0002000     // Enable start/stop output control
#define PARMRK  0004000     // Mark parity errors

/* c_oflag bits*/
#define OPOST   0000001     // Post-process output
#define ONLCR   0000002     // Map NL to CR-NL on output
#define OCRNL   0000004     // Map CR to NL on output
#define ONOCR   0000010     // No CR output at column 0
#define ONLRET  0000020     // NL performs CR function
#define OFDEL   0000040     // Fill is DEL
#define OFILL   0000100     // Use fill characters for delay
#define NLDLY   0000200     // Select newline delays
#define   NL0   0000000
#define   NL1   0000400
#define CRDLY   0003000     // Select carriage-return delays
#define   CR0   0000000
#define   CR1   0001000
#define   CR2   0002000
#define   CR3   0003000
#define TABDLY  0014000     // Select horizontal-tab delays
#define   TAB0  0000000
#define   TAB1  0004000
#define   TAB2  0010000
#define   TAB3  0014000     // Expand tabs to spaces
#define BSDLY   0020000     // Select backspace delays
#define   BS0   0000000
#define   BS1   0020000
#define FFDLY   0100000     // Select form-feed delays
#define   FF0   0000000
#define   FF1   0100000
#define VTDLY   0040000     // Select vertical-tab delays
#define   VT0   0000000
#define   VT1   0040000

/* baud rates */
#define B       0000000
#define B50     0000001
#define B75     0000002
#define B110    0000003
#define B134    0000004
#define B150    0000005
#define B200    0000006
#define B300    0000007
#define B600    0000010
#define B1200   0000011
#define B1800   0000012
#define B2400   0000013
#define B4800   0000014
#define B9600   0000015
#define B19200  0000016
#define B38400  0000017

/* control modes - c_cflag */
#define CSIZE   0000060     // Character size
#define   CS5   0000000     // 5 bits
#define   CS6   0000020
#define   CS7   0000040
#define   CS8   0000060     // 8 bits
#define CSTOPB  0000100     // Send two stop bits, else one
#define CREAD   0000200     // Enable receiver
#define PARENB  0000400     // Parity enable
#define PARODD  0001000     // Odd parity, else even
#define HUPCL   0002000     // Hang up on last close
#define CLOCAL  0004000     // Ignore modem status lines

/* local modes - c_lflag */
#define ISIG    0000001     // Enagle signals
#define ICANON  0000002     // Canonical input (erase and kill processing)
#define ECHO    0000010     // Enable echo
#define ECHOE   0000020     // Echo erase character as error-correcting backsp
#define ECHOK   0000040     // Echo kill
#define ECHONL  0000100     // Echo NL
#define NOFLSH  0000200     // Disable flush after interrupt or quit
#define TOSTOP  0000400     // Send SIGTTOU for background output
#define IEXTEN  0001000     // Enable extended input character processing

/* tcsetattr() */
#define TCSANOW     0
#define TCSADRAIN   1
#define TCSAFLUSH   2

/* tcflush() and TCFLSH */
#define TCIFLUSH    0
#define TCOFLUSH    1
#define TCIOFLUSH   2

/* tcflow() */
#define TCOOFF  0
#define TCOON   1
#define TCIOFF  2
#define TCION   3

/* I think these should belong to ioctl.h */
#define TCGETS		0x5401
#define TCSETS		0x5402
#define TCSETSW		0x5403
#define TCSETSF		0x5404
#define TCGETA		0x5405
#define TCSETA		0x5406
#define TCSETAW		0x5407
#define TCSETAF		0x5408
#define TCSBRK		0x5409
#define TCXONC		0x540A
#define TCFLSH		0x540B
#define TIOCEXCL	0x540C
#define TIOCNXCL	0x540D
#define TIOCSCTTY	0x540E
#define TIOCGPGRP	0x540F
#define TIOCSPGRP	0x5410
#define TIOCOUTQ	0x5411
#define TIOCSTI		0x5412
#define TIOCGWINSZ	0x5413
#define TIOCSWINSZ	0x5414
#define TIOCMGET	0x5415
#define TIOCMBIS	0x5416
#define TIOCMBIC	0x5417
#define TIOCMSET	0x5418
#define TIOCGSOFTCAR	0x5419
#define TIOCSSOFTCAR	0x541A

struct winsize {
    unsigned short ws_row;
    unsigned short ws_col;
    unsigned short ws_xpixel;
    unsigned short ws_ypixel;
};

/**
 * Gets the input baud rate stored in termios
 */
extern speed_t cfgetispeed(const struct termios *termios);

/**
 * Gets the output baud rate stored in termios
 */
extern speed_t cfgetospeed(const struct termios *termios);

/**
 * Sets the input baud rate stored in termios to speed
 */
extern int cfsetispeed(struct termios *termios, speed_t speed);

/**
 * Sets the output baud rate stored in termios to speed
 */
extern int cfsetospeed(struct termios *termios, speed_t speed);

/**
 * Wait for pending output to be written on FD
 */
extern int tcdrain(int fd);

/**
 * Suspend or restart transmission on fd
 */
extern int tcflow(int fd, int action);

/**
 * Flush pending data on fd
 */
extern int tcflush(int fildes, int queue_selector);

/**
 * Get params of terminal associated with fd and put them into termios
 */
extern int tcgetattr(int fd, struct termios *termios_p);

/**
 * Return the session id of current session that has the terminal associated
 * to fd
 */
extern pid_t tc_getsid(int fd);

/**
 * Send zero bits
 */
extern int tcsendbreak(int fildes, int duration);

/**
 * Sets parameters on terminal
 */
extern int tcsetattr(int fildes, int optional_actions, 
        const struct termios *termios);

