
/***************************************************************************
*                            COPYRIGHT NOTICE                              *
****************************************************************************
*                ncurses is copyright (C) 1992-1995                        *
*                          Zeyd M. Ben-Halim                               *
*                          zmbenhal@netcom.com                             *
*                          Eric S. Raymond                                 *
*                          esr@snark.thyrsus.com                           *
*                                                                          *
*        Permission is hereby granted to reproduce and distribute ncurses  *
*        by any means and for any fee, whether alone or as part of a       *
*        larger distribution, in source or in binary form, PROVIDED        *
*        this notice is included with any such distribution, and is not    *
*        removed from any of its header files. Mention of ncurses in any   *
*        applications linked with it is highly appreciated.                *
*                                                                          *
*        ncurses comes AS IS with no warranty, implied or expressed.       *
*                                                                          *
***************************************************************************/



/*
**	lib_scroll.c
**
**	The routine wscrl(win, n).
**  positive n scroll the window up (ie. move lines down)
**  negative n scroll the window down (ie. move lines up)
**
*/

#include <stdlib.h>
#include <string.h>
#include "curses.priv.h"

void scroll_window(WINDOW *win, int n, int top, int bottom)
{
int	line, j;
chtype	blank = wrenderchar(win, ' ', BLANK, TRUE);

	TR(TRACE_MOVE, ("scroll_window(%p, %d, %d, %d)", win, n, top,bottom)); 

	/*
	 * This used to do a line-text pointer-shuffle instead of text copies.
	 * That (a) doesn't work when the window is derived and doesn't have
	 * its own storage, (b) doesn't save you a lot on modern machines
	 * anyway.  Your typical memcpy implementations are coded in
	 * assembler using a tight BLT loop; for the size of copies we're
	 * talking here, the total execution time is dominated by the one-time
	 * setup cost.  So there is no point in trying to be excessively
	 * clever -- esr.
	 */

	/* shift n lines downwards */
    	if (n < 0) {
		for (line = bottom; line >= top-n; line--)
		{
		    	memcpy(win->_line[line].text,
			       win->_line[line+n].text,
			       sizeof(chtype) * win->_maxx);
			win->_line[line].oldindex=win->_line[line+n].oldindex;
		}
		for (line = top; line < top-n; line++)
		{
			for (j = 0; j < win->_maxx; j ++)
				win->_line[line].text[j] = blank;
			win->_line[line].oldindex = NEWINDEX;
		}
    	}

	/* shift n lines upwards */
    	if (n > 0) {
		for (line = top; line <= bottom-n; line++)
		{
		    	memcpy(win->_line[line].text,
			       win->_line[line+n].text,
			       sizeof(chtype) * win->_maxx);
			win->_line[line].oldindex = win->_line[line+n].oldindex;
		}
		for (line = bottom; line > bottom-n; line--)
		{
			for (j = 0; j < win->_maxx; j ++)
				win->_line[line].text[j] = blank;
			win->_line[line].oldindex = NEWINDEX;
		}
	}
}

int
wscrl(WINDOW *win, int n)
{
	T(("wscrl(%p,%d) called", win, n));

	if (! win->_scroll)
		return ERR;

	if (n == 0)
		return OK;

	scroll_window(win, n, win->_regtop, win->_regbottom);
	touchline(win, win->_regtop, win->_regbottom - win->_regtop + 1);

	wchangesync(win);
    	return OK;
}
