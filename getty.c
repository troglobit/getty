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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <paths.h>
#include <sys/utsname.h>
#include <stdio.h>

#ifndef _PATH_LOGIN
#define _PATH_LOGIN  "/bin/login"
#endif

/* Crude indication of a tty being physically secure: */
#define securetty(dev)		((unsigned) ((dev) - 0x0400) < (unsigned) 8)

void std_out(const char *s)
{
	write(1, s, strlen(s));
}

/*
 * Read one character from stdin.
 */
int readch(char *tty)
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
		std_out("getty: ");
		std_out(tty);
		std_out(": read error\n");
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
	char buf[BUFSIZ] = "Welcome to \\s \\v\n\n";
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
void do_getty(char *tty, char *name, size_t len, char **args)
{
	int ch;
	char *np;

	/*
	 * Clean up tty name.
	 */
	if (!strncmp(tty, "/dev/", 5))
		tty += 5;

	if (args[0] != NULL) {
		char cmd[5 + 6 + 1];	/* "stty " + "115200" + '\0' */

		snprintf(cmd, sizeof(cmd), "stty %d\n", atoi(args[0]));
		system(cmd);
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
}

/* Execute the login(1) command with the current
 * username as its argument. It will reply to the
 * calling user by typing "Password: "...
 */
void do_login(char *name)
{
	struct stat st;

	execl(_PATH_LOGIN, _PATH_LOGIN, name, NULL);

	/*
	 * Failed to exec login.  Impossible, but true.  Try a shell,
	 * but only if the terminal is more or less secure, because it
	 * will be a root shell.
	 */
	std_out("getty: can't exec ");
	std_out(_PATH_LOGIN);
	std_out("\n");
	if (fstat(0, &st) == 0 && S_ISCHR(st.st_mode) && securetty(st.st_rdev))
		execl(_PATH_BSHELL, _PATH_BSHELL, NULL);
}

int main(int argc, char **argv)
{
	char name[30], *tty;
	struct sigaction sa;

	/*
	 * Don't let QUIT dump core.
	 */
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sa.sa_handler = exit;
	sigaction(SIGQUIT, &sa, NULL);

	tty = ttyname(0);
	if (tty == NULL) {
		std_out("getty: tty name unknown\n");
		pause();
		return (1);
	}

	do_getty(tty, name, sizeof(name), argv + 1);
	name[29] = '\0';
	do_login(name);

	return 1;
}
