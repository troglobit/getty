Minix getty
===========

Initialize and serve a login-terminal for INIT.  Also, select the
correct speed.

**Usage:**

    getty [-h] [speed]

It is expected that the [init](https://github.com/troglobit/finit) that
spawns getty has first opened up the TTY and set up STDIN, OUT and ERR.


Origin & References
-------------------

The original version of this getty stems back to the early 1990's.  It
was written by Fred van Kempen.  It later found its way to Minix, which
adopted version 3.4, 02/17/90, and started stripping it down to much of
what is its current form.  The software was open sourced along with the
rest of Minix under the 3-clause BSD license in April, 2000.


ChangeLog
---------

* All the good stuff removed to get a minimal getty, because many modems
  don't like all that fancy speed detection stuff.
  03/03/91 Kees J. Bot (kjb@cs.vu.nl)

* Uname(), termios.  More nonsense removed.  (The result has only 10% of
  the original functionality, but a 10x chance of working.)
  12/12/92 Kees J. Bot

* Customizable login banner.
  11/13/95 Kees J. Bot

* Suspend/resume signals removed.
  2001-04-04 Kees J. Bot

* Removed unused custom banner and returned speed option functionality
  (by simply calling stty).
  2012-09-24 T. Veerman

* Refactored banner code to read std /etc/issue instead.  Refactored
  speed handling to use direct termios instead of stty.
  2016-07-27 J. Nilsson
