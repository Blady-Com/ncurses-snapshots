
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
 * Termcap compatibility support
 *
 * If your OS integrator didn't install a terminfo database, you can
 * call _nc_read_termcap_entry() to support reading and translating
 * capabilities from the system termcap file.  This is a kluge; it
 * will bulk up and slow down every program that uses ncurses, and
 * translated termcap entries cannot use full terminfo capabilities.
 * Don't use it unless you absolutely have to; instead, get your
 * system people to run tic(1) from root on the terminfo master
 * included with ncurses to translate it into a terminfo database.
 */

#include "curses.priv.h"
#include <stdlib.h>
#include <string.h>

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "term.h"
#include "tic.h"
#include "term_entry.h"

#define TERMTMP	"/tmp/tcXXXXXX"

int _nc_read_termcap_entry(const char *tn, TERMTYPE *tp)
{
    /*
     * Here is what the BSD termcap(3) page prescribes:
     *
     * It will look in the environment for a TERMCAP variable.  If
     * found, and the value does not begin with a slash, and the
     * terminal type name is the same as the environment string
     * TERM, the TERMCAP string is used instead of reading a termcap
     * file.  If it does begin with a slash, the string is used as a
     * path name of the termcap file to search.  If TERMCAP does not
     * begin with a slash and name is different from TERM, tgetent()
     * searches the files $HOME/.termcap and /usr/share/misc/termcap,
     * in that order, unless the environment variable TERMPATH exists,
     * in which case it specifies a list of file pathnames (separated
     * by spaces or colons) to be searched instead.
     *
     * It goes on to state:
     *
     * Whenever multiple files are searched and a tc field occurs in the
     * requested entry, the entry it names must
     * be found in the same file or one of the succeeding files.
     *
     * However, this restriction is relaxed in ncurses; tc references to
     * previous files are permitted.
     */
    FILE	*fp;
    ENTRY	*ep;
#define MAXPATHS	32
    char	*tc, *termpaths[MAXPATHS], pathbuf[BUFSIZ];
    int    	filecount = 0;
    bool	use_buffer = FALSE;

    if ((tc = getenv("TERMCAP")) != (char *)NULL)
    {
	if (tc[0] == '/')	/* interpret as a filename */
	{
	    termpaths[0] = tc;
	    termpaths[filecount = 1] = (char *)NULL;
	}
	else if (_nc_name_match(tc, tn, "|:"))    /* treat as a capability file */
	{
 	    use_buffer = TRUE;
	    (void) strcat(tc, "\n");
	}
	else if ((tc = getenv("TERMPATH")) != (char *)NULL)
	{
	    char    *cp;

	    for (cp = tc; *cp; cp++)
	    {
		if (*cp == ':')
		    *cp = '\0';
		else if (cp == tc || cp[-1] == '\0')
		{
		    if (filecount >= MAXPATHS - 1)
			return(ERR);

		    termpaths[filecount++] = cp;
		}
	    }

	    termpaths[filecount] = (char *)NULL;
	}
    }
    else	/* normal case */
    {
	filecount = 0;

	/*
	 * Probably /etc/termcap is a symlink to /usr/share/misc/termcap.
	 * Avoid reading the same file twice.
	 */
	if (access("/etc/termcap", R_OK) == 0)
	    termpaths[filecount++] = "/etc/termcap";
	else if (access("/usr/share/misc/termcap", R_OK) == 0)
	    termpaths[filecount++] = "/usr/share/misc/termcap";

	/* user's .termcap, if any, should override it */
	(void) sprintf(pathbuf, "%s/.termcap", getenv("HOME"));
	termpaths[filecount++] = pathbuf;

	termpaths[filecount] = (char *)NULL;
    }

    /* get the data from all designated files or the buffer */
    _nc_make_hash_table(_nc_info_table, _nc_info_hash_table);
    _nc_make_hash_table(_nc_cap_table, _nc_cap_hash_table);

    /* parse the sources */
    if (use_buffer)
    {
	_nc_set_source("TERMCAP");

	/*
	 * We don't suppress warning messages here.  The presumption is
	 * that since it's just a single entry, they won't be a pain.
	 */
	_nc_read_entry_source((FILE *)NULL, tc, FALSE, FALSE);
    }
    else
    {
	int    	i;

	for (i = 0; i < filecount; i++)
	{
	    T(("Looking for %s in %s", tn, termpaths[i]));
	    if ((fp = fopen(termpaths[i], "r")) != (FILE *)NULL)
	    {
		_nc_set_source(termpaths[i]);

		/*
		 * Suppress warning messages.  Otherwise you get 400
		 * lines of crap from archaic termcap files as ncurses
		 * complains about all the obsolete capabilities.
		 */
		_nc_read_entry_source(fp, (char*)NULL, FALSE, TRUE);

		(void) fclose(fp);
	    }
	}
    }

    if (_nc_head == (ENTRY *)NULL)
	return(ERR);

    /* resolve all use references */
    _nc_resolve_uses();

    /* find a terminal matching tn, if we can */
    for_entry_list(ep)
	if (_nc_name_match(ep->tterm.term_names, tn, "|:"))
	{
	    memcpy(tp, &ep->tterm, sizeof(TERMTYPE));
	    ep->tterm.str_table = (char *)NULL;
	    return(OK);
	}

    /*
     * Now that we have a local copy of the terminal capabilities.
     * free all entry storage except the string table for the loaded
     * type (which we disconnected from the list by NULLing out
     * ep->tterm.str_table above).
     */
    _nc_free_entries();

    return(ERR);
}
