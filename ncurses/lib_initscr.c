/****************************************************************************
 * Copyright (c) 1998 Free Software Foundation, Inc.                        *
 *                                                                          *
 * Permission is hereby granted, free of charge, to any person obtaining a  *
 * copy of this software and associated documentation files (the            *
 * "Software"), to deal in the Software without restriction, including      *
 * without limitation the rights to use, copy, modify, merge, publish,      *
 * distribute, distribute with modifications, sublicense, and/or sell       *
 * copies of the Software, and to permit persons to whom the Software is    *
 * furnished to do so, subject to the following conditions:                 *
 *                                                                          *
 * The above copyright notice and this permission notice shall be included  *
 * in all copies or substantial portions of the Software.                   *
 *                                                                          *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS  *
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF               *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.   *
 * IN NO EVENT SHALL THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,   *
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR    *
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR    *
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.                               *
 *                                                                          *
 * Except as contained in this notice, the name(s) of the above copyright   *
 * holders shall not be used in advertising or otherwise to promote the     *
 * sale, use or other dealings in this Software without prior written       *
 * authorization.                                                           *
 ****************************************************************************/

/****************************************************************************
 *  Author: Zeyd M. Ben-Halim <zmbenhal@netcom.com> 1992,1995               *
 *     and: Eric S. Raymond <esr@snark.thyrsus.com>                         *
 ****************************************************************************/

/*
**	lib_initscr.c
**
**	The routines initscr(), and termname().
**
*/

#include <curses.priv.h>
#include <tic.h>	/* for MAX_ALIAS */

#if HAVE_SYS_TERMIO_H
#include <sys/termio.h>	/* needed for ISC */
#endif

MODULE_ID("$Id: lib_initscr.c,v 1.22 1998/04/18 21:51:33 tom Exp $")

WINDOW *initscr(void)
{
static	bool initialized = FALSE;
NCURSES_CONST char *name;

	T((T_CALLED("initscr()")));
	/* Portable applications must not call initscr() more than once */
	if (!initialized) {
		initialized = TRUE;

		if ((name = getenv("TERM")) == 0
		 || *name == '\0')
			name = "unknown";
		if (newterm(name, stdout, stdin) == 0) {
			fprintf(stderr, "Error opening terminal: %s.\n", name);
			exit(EXIT_FAILURE);
		}

		/* allow user to set maximum escape delay from the environment */
		if ((name = getenv("ESCDELAY")) != 0)
			ESCDELAY = atoi(getenv("ESCDELAY"));

		/* def_shell_mode - done in newterm/_nc_setupscreen */
		def_prog_mode();
	}
	returnWin(stdscr);
}

char *termname(void)
{
char	*term = getenv("TERM");
static char	ret[MAX_ALIAS];

	T(("termname() called"));

	if (term != 0) {
		(void) strncpy(ret, term, sizeof(ret) - 1);
		term = ret;
	}
	return term;
}
