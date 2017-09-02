/* get tty speed -- Initialize and serve a login-terminal for INIT
 *
 * Copyright (c) 1987,1997  Prentice Hall
 * All rights reserved.
 *
 * Redistribution and use of the MINIX operating system in source and
 * binary forms, with or without modification, are permitted provided
 * that the following conditions are met:
 * 
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 * 
 *    * Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 * 
 *    * Neither the name of Prentice Hall nor the names of the software
 *      authors or contributors may be used to endorse or promote
 *      products derived from this software without specific prior
 *      written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS, AUTHORS, AND
 * CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL PRENTICE HALL OR ANY AUTHORS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <err.h>
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

#define CTL(x)   ((x) ^ 0100)
#define print(s) (void)write(STDOUT_FILENO, s, strlen(s))


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
		print("\n");
		exit(0);
	}

	if (st < 0)
		errx(1, "getty: %s: read error", tty);

	return ch1 & 0xFF;
}

static void do_speed(speed_t speed)
{
	struct termios term;

	if (tcgetattr(0, &term))
		return;

	cfsetispeed(&term, speed);
	cfsetospeed(&term, speed);
	tcsetattr(0, TCSAFLUSH, &term);
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
			(void)write(1, s0, s - s0);
			s0 = s + 2;
			switch (*++s) {
			case 'l':
				print(tty);
				break;
			case 'm':
				print(uts->machine);
				break;
			case 'n':
				print(uts->nodename);
				break;
#ifdef _GNU_SOURCE
			case 'o':
				print(uts->domainname);
				break;
#endif
			case 'r':
				print(uts->release);
				break;
			case 's':
				print(uts->sysname);
				break;
			case 'v':
				print(uts->version);
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

	print("\n");
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
static void do_getty(char *tty, char *name, size_t len)
{
	int ch;
	char *np;

	/*
	 * Clean up tty name.
	 */
	if (!strncmp(tty, _PATH_DEV, strlen(_PATH_DEV)))
		tty += 5;

	/*
	 * Display prompt.
	 */
	ch = ' ';
	*name = '\0';
	while (ch != '\n') {
		do_issue(tty);

		np = name;
		while ((ch = readch(tty)) != '\n') {
			if (ch == CTL('U')) {
				while (np > name) {
					(void)write(1, "\b \b", 3);
					np--;
				}
				continue;
			}

			if (np < name + len)
				*np++ = ch;
		}

		*np = '\0';
		if (*name == '\0')
			ch = ' ';	/* blank line typed! */
	}

	name[len - 1] = 0;
}

/*
 * Execute the login(1) command with the current
 * username as its argument. It will reply to the
 * calling user by typing "Password: "...
 */
static int do_login(char *name)
{
	struct stat st;

	execl(_PATH_LOGIN, _PATH_LOGIN, name, NULL);

	/*
	 * Failed to exec login, should be impossible.  Try a starting a
	 * fallback shell instead.
	 *
	 * Note: Add /etc/securetty handling.
	 */
	warnx("Failed exec %s, attempting fallback to %s ...", _PATH_LOGIN, _PATH_BSHELL);
	if (fstat(0, &st) == 0 && S_ISCHR(st.st_mode))
		execl(_PATH_BSHELL, _PATH_BSHELL, NULL);

	return 1;	/* We shouldn't get here ... */
}

static int usage(int code)
{
	print("Usage: getty [-h] [SPEED]\n");
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
	speed_t speed = B38400;
	struct sigaction sa;

	if (argc > 1) {
		if (!strcmp(argv[1], "-h"))
			return usage(0);

		speed = do_parse_speed(argv[1]);
		if (speed == B0) {
			warnx("Invalid TTY speed");
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
	if (!tty) {
		warnx("getty: unknown TTY\n");
		pause();
		return 1;
	}

	/*
	 * Prepare line, read username and call login
	 */
	do_speed(speed);
	do_getty(tty, name, sizeof(name));

	return do_login(name);
}
