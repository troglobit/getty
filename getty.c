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
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/utsname.h>
#include <stdio.h>

char LOGIN[] = "/usr/bin/login";
char SHELL[] = "/bin/sh";

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
 * Handle the process of a GETTY.
 */
void do_getty(char *tty, char *name, size_t len, char **args)
{
	int ch;
	char *np, *s, *s0;
	char **banner, *t;
	struct utsname utsname;
	static char *def_banner[] = { "%s  Release %r Version %v  (%t)\n\n%n login: ", 0 };

	/*
	 * Clean up tty name.
	 */
	if (!strcmp(tty, "/dev/"))
		tty += 5;

	if (args[0] != NULL) {
		char cmd[5 + 6 + 1];	/* "stty " + "115200" + '\0' */

		snprintf(cmd, sizeof(cmd), "stty %d\n", atoi(args[0]));
		system(cmd);
	}

	/*
	 * Display prompt.
	 */
	ch = ' ';
	*name = '\0';
	while (ch != '\n') {
		/*
		 * Get data about this machine.
		 */
		uname(&utsname);

		/*
		 * Print the banner.
		 */
		for (banner = def_banner; *banner != NULL; banner++) {
			std_out(banner == args ? "\n" : " ");
			s0 = *banner;
			for (s = *banner; *s != 0; s++) {
				if (*s == '\\') {
					write(1, s0, s - s0);
					s0 = s + 2;
					switch (*++s) {
					case 'n':
						std_out("\n");
						break;
					case 's':
						std_out(" ");
						break;
					case 't':
						std_out("\t");
						break;
					case 0:
						goto leave;
					default:
						s0 = s;
					}
				} else if (*s == '%') {
					write(1, s0, s - s0);
					s0 = s + 2;
					switch (*++s) {
					case 's':
						std_out(utsname.sysname);
						break;
					case 'n':
						std_out(utsname.nodename);
						break;
					case 'r':
						std_out(utsname.release);
						break;
					case 'v':
						std_out(utsname.version);
						break;
					case 'm':
						std_out(utsname.machine);
						break;
					case 't':
						std_out(tty);
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

	execl(LOGIN, LOGIN, name, (char *)NULL);

	/*
	 * Failed to exec login.  Impossible, but true.  Try a shell,
	 * but only if the terminal is more or less secure, because it
	 * will be a root shell.
	 */
	std_out("getty: can't exec ");
	std_out(LOGIN);
	std_out("\n");
	if (fstat(0, &st) == 0 && S_ISCHR(st.st_mode) && securetty(st.st_rdev))
		execl(SHELL, SHELL, (char *)NULL);
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
