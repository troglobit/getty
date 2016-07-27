/* getty - get tty speed			Author: Fred van Kempen */

/*
 * GETTY  -     Initialize and serve a login-terminal for INIT.
 *		Also, select the correct speed. The STTY() code
 *		was taken from stty(1).c; which was written by
 *		Andrew S. Tanenbaum.
 *
 * Usage:	getty [speed]
 *
 * Version:	3.4	02/17/90
 *
 * Author:	F. van Kempen, MicroWalt Corporation
 *
 * Modifications:
 *		All the good stuff removed to get a minimal getty, because
 *		many modems don't like all that fancy speed detection stuff.
 *		03/03/91	Kees J. Bot (kjb@cs.vu.nl)
 *
 *		Uname(), termios.  More nonsense removed.  (The result has
 *		only 10% of the original functionality, but a 10x chance of
 *		working.)
 *		12/12/92	Kees J. Bot
 *
 *		Customizable login banner.
 *		11/13/95	Kees J. Bot
 *
 *		Suspend/resume signals removed.
 *		2001-04-04	Kees J. Bot
 *
 *		Removed unused custom banner and returned speed option
 *		functionality (by simply calling stty).
 *		2012-09-24	T. Veerman
 *
 *		Refactored banner code to read std /etc/issue instead
 *		2016-07-27	J. Nilsson
 */

#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <termios.h>
#include <unistd.h>

#ifndef _PATH_LOGIN
#define _PATH_LOGIN  "/bin/login"
#endif

/* Crude indication of a tty being physically secure: */
#define securetty(dev)		((unsigned) ((dev) - 0x0400) < (unsigned) 8)

static void std_out(const char *s)
{
	write(1, s, strlen(s));
}

static void std_err(const char *s)
{
	write(2, s, strlen(s));
}

/*
 * Read one character from stdin.
 */
static int readch(char *tty)
{
	int st;
	char ch1;

	/*
	 * read character from TTY
	 */
	st = read(0, &ch1, 1);
	if (st == 0) {
		std_out("\n");
		exit(0);
	}

	if (st < 0) {
		std_err("getty: ");
		std_err(tty);
		std_err(": read error\n");
		exit(1);
	}

	return ch1 & 0xFF;
}

/*
 * Parse and display a line from /etc/issue
 */
static void do_parse(char *line, struct utsname *uts, char *tty)
{
	char *s, *s0;

	s0 = line;
	for (s = line; *s != 0; s++) {
		if (*s == '\\') {
			write(1, s0, s - s0);
			s0 = s + 2;
			switch (*++s) {
			case 'l':
				std_out(tty);
				break;
			case 'm':
				std_out(uts->machine);
				break;
			case 'n':
				std_out(uts->nodename);
				break;
#ifdef _GNU_SOURCE
			case 'o':
				std_out(uts->domainname);
				break;
#endif
			case 'r':
				std_out(uts->release);
				break;
			case 's':
				std_out(uts->sysname);
				break;
			case 'v':
				std_out(uts->version);
				break;
			case 0:
				goto leave;
			default:
				s0 = s - 1;
			}
		}
	}

leave:
	write(1, s0, s - s0);
}

/*
 * Parse and display /etc/issue
 */
static void do_issue(char *tty)
{
	FILE *fp;
	char buf[BUFSIZ] = "Welcome to \\s \\v \\n \\l\n\n";
	struct utsname uts;

	/*
	 * Get data about this machine.
	 */
	uname(&uts);

	std_out("\n");
	fp = fopen("/etc/issue", "r");
	if (fp) {
		while (fgets(buf, sizeof(buf), fp))
			do_parse(buf, &uts, tty);

		fclose(fp);
	} else {
		do_parse(buf, &uts, tty);
	}

	do_parse("\\n login: ", &uts, tty);
}

/*
 * Handle the process of a GETTY.
 */
static void do_getty(char *tty, char *name, size_t len, speed_t speed)
{
	int ch;
	char *np;
	struct termios term;

	/*
	 * Clean up tty name.
	 */
	if (!strncmp(tty, "/dev/", 5))
		tty += 5;

	/*
	 * Set speed, if any
	 */
	if (!tcgetattr(0, &term)) {
		if (cfsetispeed(&term, speed))
			std_err("Failed setting TTY inbound speed\n");
		if (cfsetospeed(&term, speed))
			std_err("Failed setting TTY outbound speed\n");
		tcsetattr(0, TCSAFLUSH, &term);
	} else {
		std_err("Failed reading TTY settings\n");
	}

	/*
	 * Display prompt.
	 */
	do_issue(tty);

	/*
	 * Read username.
	 */
	ch = ' ';
	*name = '\0';
	while (ch != '\n') {
		np = name;
		while ((ch = readch(tty)) != '\n') {
			if (np < name + len)
				*np++ = ch;
		}

		*np = '\0';
		if (*name == '\0')
			ch = ' ';	/* blank line typed! */
	}

	name[len - 1] = 0;
}

/* Execute the login(1) command with the current
 * username as its argument. It will reply to the
 * calling user by typing "Password: "...
 */
static int do_login(char *name)
{
	struct stat st;

	execl(_PATH_LOGIN, _PATH_LOGIN, name, NULL);

	/*
	 * Failed to exec login.  Impossible, but true.  Try a shell,
	 * but only if the terminal is more or less secure, because it
	 * will be a root shell.
	 */
	std_err("getty: can't exec ");
	std_err(_PATH_LOGIN);
	std_err("\n");
	if (fstat(0, &st) == 0 && S_ISCHR(st.st_mode) && securetty(st.st_rdev))
		execl(_PATH_BSHELL, _PATH_BSHELL, NULL);

	return 1;	/* We shouldn't get here ... */
}

static int usage(int code)
{
	std_out("Usage: getty [SPEED]\n");
	return code;
}

static speed_t do_parse_speed(char *arg)
{
	char *ptr;
	size_t i;
	unsigned long val;
	struct { unsigned long val; speed_t speed; } v2s[] = {
		{       0, B0        },
		{      50, B50       },
		{      75, B75       },
		{     110, B110      },
		{     134, B134      },
		{     150, B150      },
		{     200, B200      },
		{     300, B300      },
		{     600, B600      },
		{    1200, B1200     },
		{    1800, B1800     },
		{    2400, B2400     },
		{    4800, B4800     },
		{    9600, B9600     },
		{   19200, B19200    },
		{   38400, B38400    },
		{   57600, B57600    },
		{  115200, B115200   },
		{  230400, B230400   },
		{  460800, B460800   },
		{  500000, B500000   },
		{  576000, B576000   },
		{  921600, B921600   },
		{ 1000000, B1000000  },
		{ 1152000, B1152000  },
		{ 1500000, B1500000  },
		{ 2000000, B2000000  },
		{ 2500000, B2500000  },
		{ 3000000, B3000000  },
		{ 3500000, B3500000  },
		{ 4000000, B4000000  },
	};

	errno = 0;
	val = strtoul(arg, &ptr, 10);
	if (errno || ptr == arg)
		return B0;

	for (i = 0; i < sizeof(v2s) / sizeof(v2s[0]); i++) {
		if (v2s[i].val == val)
			return v2s[i].speed;
	}

	return B0;
}

int main(int argc, char **argv)
{
	char name[30], *tty;
	speed_t speed = TTYDEF_SPEED; //B38400;
	struct sigaction sa;

	if (argc > 1) {
		if (!strcmp(argv[1], "-h"))
			return usage(0);

		speed = do_parse_speed(argv[1]);
		if (speed == B0) {
			std_err("Invalid TTY speed\n");
			return 1;
		}
	}

	/*
	 * Don't let QUIT dump core.
	 */
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sa.sa_handler = exit;
	sigaction(SIGQUIT, &sa, NULL);

	tty = ttyname(0);
	if (tty == NULL) {
		std_err("getty: unknown TTY\n");
		pause();
		return (1);
	}

	/*
	 * Prepare line, read username and call login
	 */
	do_getty(tty, name, sizeof(name), speed);

	return do_login(name);
}
