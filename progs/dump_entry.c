
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

#define __INTERNAL_CAPS_VISIBLE
#include <progs.priv.h>

#include <ctype.h>
#include "dump_entry.h"
#include "termsort.c"		/* this C file is generated */
#include "parametrized.h"	/* so is this */

MODULE_ID("$Id: dump_entry.c,v 1.15 1996/12/30 02:29:00 tom Exp $")

#define INDENT			8

static int tversion;		/* terminfo version */
static int outform;		/* output format to use */
static int sortmode;		/* sort mode to use */
static int width = 60;		/* max line width for listings */
static int column;		/* current column, limited by 'width' */
static int oldcol;		/* last value of column before wrap */
static int tracelevel;		/* level of debug output */

static char *outbuf;		/* the output-buffer */
static size_t out_used;		/* ...its current length */
static size_t out_size;		/* ...and its allocated length */

/* indirection pointers for implementing sort and display modes */
static const int *bool_indirect, *num_indirect, *str_indirect;
static char * const *bool_names, * const *num_names, * const *str_names;

static const char *separator, *trailer;

/* cover various ports and variants of terminfo */
#define V_ALLCAPS	0	/* all capabilities (SVr4, XSI, ncurses) */
#define V_SVR1		1	/* SVR1, Ultrix */
#define V_HPUX		2	/* HP/UX */
#define V_AIX		3	/* AIX */
#define V_BSD		4	/* BSD */

#define OBSOLETE(n) (n[0] == 'O' && n[1] == 'T')

#ifdef NO_LEAKS
void _nc_leaks_dump_entry(void)
{
	if (outbuf != 0) {
		free(outbuf);
		outbuf = 0;
	}
}
#endif

char *nametrans(const char *name)
/* translate a capability name from termcap to terminfo */
{
    const struct name_table_entry 	*np;

    if ((np = _nc_find_entry(name, _nc_info_hash_table)) != NULL)
        switch(np->nte_type)
	{
	case BOOLEAN:
	    if (bool_from_termcap[np->nte_index])
		return(boolcodes[np->nte_index]);
	    break;

	case NUMBER:
	    if (num_from_termcap[np->nte_index])
		return(numcodes[np->nte_index]);
	    break;

	case STRING:
	    if (str_from_termcap[np->nte_index])
		return(strcodes[np->nte_index]);
	    break;
	}

    return((char *)NULL);
}

void dump_init(const char *version, int mode, int sort, int twidth, int traceval)
/* set up for entry display */
{
    width = twidth;
    tracelevel = traceval;

    /* versions */
    if (version == (char *)NULL)
	tversion = V_ALLCAPS;
    else if (!strcmp(version, "SVr1") || !strcmp(version, "SVR1")
					|| !strcmp(version, "Ultrix"))
	tversion = V_SVR1;
    else if (!strcmp(version, "HP"))
	tversion = V_HPUX;
    else if (!strcmp(version, "AIX"))
	tversion = V_AIX;
    else if (!strcmp(version, "BSD"))
	tversion = V_BSD;
    else
	tversion = V_ALLCAPS;

    /* implement display modes */
    switch (outform = mode)
    {
    case F_LITERAL:
    case F_TERMINFO:
	bool_names = boolnames;
	num_names = numnames;
	str_names = strnames;
	separator = twidth ? ", " : ",";
	trailer = "\n\t";
	break;

    case F_VARIABLE:
	bool_names = boolfnames;
	num_names = numfnames;
	str_names = strfnames;
	separator = twidth ? ", " : ",";
	trailer = "\n\t";
	break;

    case F_TERMCAP:
    case F_TCONVERR:
	bool_names = boolcodes;
	num_names = numcodes;
	str_names = strcodes;
	separator = ":";
	trailer = "\\\n\t:";
	break;
    }

    /* implement sort modes */
    switch(sortmode = sort)
    { 
    case S_NOSORT:
	if (traceval)
	    (void) fprintf(stderr,
			   "%s: sorting by term structure order\n", _nc_progname);
	break;

    case S_TERMINFO:
	if (traceval)
	    (void) fprintf(stderr,
			   "%s: sorting by terminfo name order\n", _nc_progname);
	bool_indirect = bool_terminfo_sort;
	num_indirect = num_terminfo_sort;
	str_indirect = str_terminfo_sort;
	break;

    case S_VARIABLE:
	if (traceval)
	    (void) fprintf(stderr,
			   "%s: sorting by C variable order\n", _nc_progname);
	bool_indirect = bool_variable_sort;
	num_indirect = num_variable_sort;
	str_indirect = str_variable_sort;
	break;

    case S_TERMCAP:
	if (traceval)
	    (void) fprintf(stderr,
			   "%s: sorting by termcap name order\n", _nc_progname);
	bool_indirect = bool_termcap_sort;
	num_indirect = num_termcap_sort;
	str_indirect = str_termcap_sort;
	break;
    }

    if (traceval)
	(void) fprintf(stderr,
		       "%s: width = %d, tversion = %d, outform = %d\n",
		       _nc_progname, width, tversion, outform);
}

static int trailing_spaces(char *src)
{
	while (*src == ' ')
		src++;
	return *src == 0;
}

/* this deals with differences over whether 0x7f and 0x80..0x9f are controls */
#define CHAR_OF(s) (*(unsigned char *)(s))
#define REALCTL(s) (CHAR_OF(s) < 127 && iscntrl(CHAR_OF(s)))
#define REALPRINT(s) (CHAR_OF(s) < 127 && isprint(CHAR_OF(s)))

char *expand(char *srcp)
{
static char	buffer[1024];
int		bufp;
char		*ptr, *str = (srcp == ABSENT_STRING
			   || srcp == CANCELLED_STRING) ? "" : srcp;
bool		islong = (strlen(str) > 3);

    	bufp = 0;
    	ptr = str;
    	while (*str) {
		if (*str == '%' && REALPRINT(str+1)) {
	    		buffer[bufp++] = *str++;
	    		buffer[bufp++] = *str;
		}
		else if (*str == '\033') {
	    		buffer[bufp++] = '\\';
	    		buffer[bufp++] = 'E';
		}
		else if (*str == '\\' && (outform==F_TERMINFO) && (str == srcp || str[-1] != '^')) {
	    		buffer[bufp++] = '\\';
	    		buffer[bufp++] = '\\';
		}
		else if (*str == ' ' && (outform==F_TERMINFO) && (str == srcp || trailing_spaces(str))) {
	    		buffer[bufp++] = '\\';
	    		buffer[bufp++] = 's';
		}
		else if ((*str == ',' || *str == ':' || *str == '^') && (outform==F_TERMINFO)) {
	    		buffer[bufp++] = '\\';
	    		buffer[bufp++] = *str;
		}
		else if (REALPRINT(str) && (*str != ',' && *str != ':' && !(*str == '!' && outform!=F_TERMINFO) && *str != '^'))
		    	buffer[bufp++] = *str;
#if 0		/* FIXME: this would be more readable */
		else if (*str == '\b') {
	    		buffer[bufp++] = '\\';
	    		buffer[bufp++] = 'b';
		}
		else if (*str == '\f') {
	    		buffer[bufp++] = '\\';
	    		buffer[bufp++] = 'f';
		}
		else if (*str == '\t' && islong) {
	    		buffer[bufp++] = '\\';
	    		buffer[bufp++] = 't';
		}
#endif
		else if (*str == '\r' && (islong || (strlen(srcp) > 2 && str[1] == '\0'))) {
	    		buffer[bufp++] = '\\';
	    		buffer[bufp++] = 'r';
		}
		else if (*str == '\n' && islong) {
	    		buffer[bufp++] = '\\';
	    		buffer[bufp++] = 'n';
		}
#define UnCtl(c) ((0xff & (c)) + '@')
		else if (REALCTL(str) && *str != '\\' && (!islong || isdigit(str[1])))
		{
			(void) sprintf(&buffer[bufp], "^%c", UnCtl(*str));
			bufp += 2;
		}
		else
		{
			(void) sprintf(&buffer[bufp], "\\%03o", 0xff & *str);
			bufp += 4;
		}

		str++;
    	}

    	buffer[bufp] = '\0';
    	return(buffer);
}

static TERMTYPE	*cur_type;

static int dump_predicate(int type, int idx)
/* predicate function to use for ordinary decompilation */
{
    	switch(type) {
    	case BOOLEAN:
		return (cur_type->Booleans[idx] == FALSE)
		    ? FAIL : cur_type->Booleans[idx];

    	case NUMBER:
		return (cur_type->Numbers[idx] == ABSENT_NUMERIC)
		    ? FAIL : cur_type->Numbers[idx];

    	case STRING:
		return (cur_type->Strings[idx] != ABSENT_STRING)
		    ? (int)TRUE : FAIL;
    	}

    	return(FALSE);	/* pacify compiler */
}

static void set_obsolete_termcaps(TERMTYPE *tp);

/* is this the index of a function key string? */
#define FNKEY(i)	(((i)<= 65 && (i)>= 75) || ((i)<= 216 && (i)>= 268))

static bool version_filter(int type, int idx)
/* filter out capabilities we may want to suppress */
{
    switch (tversion)
    {
    case V_ALLCAPS:	/* SVr4, XSI Curses */
	return(TRUE);

    case V_SVR1:	/* System V Release 1, Ultrix */
	switch (type)
	{
	case BOOLEAN:
	    return (idx <= 20);	/* below and including xon_xoff */
	case NUMBER:
	    return (idx <= 7);	/* below and including width_status_line */
	case STRING:
	    return (idx <= 144);	/* below and including prtr_non */
	}
	break;

    case V_HPUX:		/* Hewlett-Packard */
	switch (type)
	{
	case BOOLEAN:
	    return (idx <= 20);	/* below and including xon_xoff */
	case NUMBER:
	    return (idx <= 10);	/* below and including label_width */
	case STRING:
	    if (idx <= 144)	/* below and including prtr_non */
		return(TRUE);
	    else if (FNKEY(idx))	/* function keys */
		return(TRUE);
	    else if (idx==147||idx==156||idx==157) /* plab_norm,label_on,label_off */
		return(TRUE);
	    else
		return(FALSE);
	}
	break;

    case V_AIX:		/* AIX */
	switch (type)
	{
	case BOOLEAN:
	    return (idx <= 20);	/* below and including xon_xoff */
	case NUMBER:
	    return (idx <= 7);	/* below and including width_status_line */
	case STRING:
	    if (idx <= 144)	/* below and including prtr_non */
		return(TRUE);
	    else if (FNKEY(idx))	/* function keys */
		return(TRUE);
	    else
		return(FALSE);
	}
	break;

    case V_BSD:		/* BSD */
	switch (type)
	{
	case BOOLEAN:
	    return bool_from_termcap[idx];
	case NUMBER:
	    return num_from_termcap[idx];
	case STRING:
	    return str_from_termcap[idx];
	}
	break;
    }

    return(FALSE);	/* pacify the compiler */
}

static
void append_output (const char *src)
{
	if (src == 0) {
		out_used = 0;
		append_output("");
	} else {
		size_t need = strlen(src);
		size_t want = need + out_used + 1;
		if (want > out_size) {
			out_size += want;	/* be generous */
			if (outbuf == 0)
				outbuf = malloc(out_size);
			else
				outbuf = realloc(outbuf, out_size);
		}
		(void)strcpy(outbuf + out_used, src);
		out_used += need;
	}
}

static
void force_wrap(void)
{
	oldcol = column;
	append_output(trailer);
	column = INDENT;
}

static
void wrap_concat(const char *src)
{
	int need = strlen(src);
	int want = strlen(separator) + need;

	if (column > INDENT
	 && column + want > width) {
		force_wrap();
	}
	append_output(src);
	append_output(separator);
	column += need;
}

#define IGNORE_SEP_TRAIL(first,last,sep_trail) \
	if ((size_t)(last - first) > sizeof(sep_trail)-1 \
	 && !strncmp(first, sep_trail, sizeof(sep_trail)-1)) \
	 	first += sizeof(sep_trail)-2

/* Returns the nominal length of the buffer assuming it is termcap format,
 * i.e., the continuation sequence is treated as a single character ":".
 *
 * There are several implementations of termcap which read the text into a
 * fixed-size buffer.  Generally they strip the newlines from the text, but may
 * not do it until after the buffer is read.  Also, "tc=" resolution may be
 * expanded in the same buffer.  This function is useful for measuring the size
 * of the best fixed-buffer implementation; the worst case may be much worse.
 */
#ifdef TEST_TERMCAP_LENGTH
static int termcap_length(const char *src)
{
	static const char pattern[] = ":\\\n\t:";

	int len = 0;
	const char *const t = src + strlen(src);

	while (*src != '\0') {
		IGNORE_SEP_TRAIL(src, t, pattern);
		src++;
		len++;
	}
	return len;
}
#else
#define termcap_length(src) strlen(src)
#endif

int fmt_entry(TERMTYPE *tterm,
			   int (*pred)(int type, int idx),
			   bool suppress_untranslatable,
			   bool infodump)
{
int	i, j;
char    buffer[MAX_TERMINFO_LENGTH];
int	predval, len;
int	num_bools = 0;
int	num_values = 0;
int	num_strings = 0;
bool	outcount = 0;

#define WRAP_CONCAT	\
	wrap_concat(buffer); \
	outcount = TRUE

    len = 12;			/* terminfo file-header */

    if (pred == NULL) {
	cur_type = tterm;
	pred = dump_predicate;
    }

    append_output(NULL);
    append_output(tterm->term_names);
    append_output(separator);
    column = out_used;
    force_wrap();

    for (j=0; j < BOOLCOUNT; j++) {
	if (sortmode == S_NOSORT)
	    i = j;
	else
	    i = bool_indirect[j];

	if (!version_filter(BOOLEAN, i))
	    continue;
	else if ((outform == F_LITERAL || outform == F_TERMINFO || outform == F_VARIABLE)
		 && (OBSOLETE(bool_names[i]) && outform != F_LITERAL))
	    continue;

	predval = pred(BOOLEAN, i);
	if (predval != FAIL) {
	    (void) strcpy(buffer, bool_names[i]);
	    if (predval <= 0)
		(void) strcat(buffer, "@");
	    else if (i + 1 > num_bools)
		num_bools = i + 1;
	    WRAP_CONCAT;
	}
    }

    if (column != INDENT)
	force_wrap();

    for (j=0; j < NUMCOUNT; j++) {
	if (sortmode == S_NOSORT)
	    i = j;
	else
	    i = num_indirect[j];

	if (!version_filter(NUMBER, i))
	    continue;
	else if ((outform == F_LITERAL || outform == F_TERMINFO || outform == F_VARIABLE)
		 && (OBSOLETE(num_names[i]) && outform != F_LITERAL))
	    continue;

	predval = pred(NUMBER, i);
	if (predval != FAIL) {
	    if (tterm->Numbers[i] < 0) {
		sprintf(buffer, "%s@", num_names[i]);
	    } else {
		sprintf(buffer, "%s#%d", num_names[i], tterm->Numbers[i]);
		if (i + 1 > num_values)
		    num_values = i + 1;
	    }
	    WRAP_CONCAT;
	}
    }

    if (column != INDENT)
	force_wrap();

    len += num_bools
    	+ num_values * 2
    	+ strlen(tterm->term_names) + 1;
    if (len & 1)
    	len++;

    for (j=0; j < STRCOUNT; j++) {
	if (sortmode == S_NOSORT)
	    i = j;
	else
	    i = str_indirect[j];

	if (!version_filter(STRING, i))
	    continue;
	else if ((outform == F_LITERAL || outform == F_TERMINFO || outform == F_VARIABLE)
		 && (OBSOLETE(str_names[i]) && outform != F_LITERAL))
	    continue;

	/*
	 * Some older versions of vi want rmir/smir to be defined
	 * for ich/ich1 to work.  If they're not defined, force
	 * them to be output as defined and empty.
	 */
	if (outform==F_TERMCAP)
#undef CUR
#define CUR tterm->
	    if (insert_character || parm_ich)
	    {
		if (&tterm->Strings[i] == &enter_insert_mode
		    && enter_insert_mode == ABSENT_STRING)
		{
		    (void) strcpy(buffer, "im=");
		    goto catenate;
		}

		if (&tterm->Strings[i] == &exit_insert_mode
		    && exit_insert_mode == ABSENT_STRING)
		{
		    (void) strcpy(buffer, "ei=");
		    goto catenate;
		}
	    }

	predval = pred(STRING, i);
	buffer[0] = '\0';
	if (predval != FAIL) {
	    if (tterm->Strings[i] != ABSENT_STRING
	     && i + 1 > num_strings)
		num_strings = i + 1;
	    if (tterm->Strings[i] == ABSENT_STRING
	     || tterm->Strings[i] == CANCELLED_STRING)
		sprintf(buffer, "%s@", str_names[i]);
	    else if (outform == F_TERMCAP || outform == F_TCONVERR)
	    {
		char *srccap = expand(tterm->Strings[i]);
		char *cv = _nc_infotocap(str_names[i], srccap,parametrized[i]);

		if (cv == (char *)NULL)
		{
		    if (outform == F_TCONVERR)
			sprintf(buffer, "%s=!!! %s WILL NOT CONVERT !!!", str_names[i], srccap);
		    else if (suppress_untranslatable)
			continue;
		    else
			sprintf(buffer, "..%s=%s", str_names[i], srccap);
		}
		else
		    sprintf(buffer, "%s=%s", str_names[i], cv);
		len += strlen(tterm->Strings[i]) + 1;
	    }
	    else
	    {
		sprintf(buffer,"%s=%s",str_names[i],expand(tterm->Strings[i]));
		len += strlen(tterm->Strings[i]) + 1;
	    }

	catenate:
	    WRAP_CONCAT;
	}
    }
    len += num_strings * 2;

    /*
     * This piece of code should be an effective inverse of the functions
     * postprocess_terminfo and postprocess_terminfo in parse_entry.c.
     * Much more work should be done on this to support dumping termcaps.
     */
    if (tversion == V_HPUX)
    {
	if (memory_lock)
	{
	    (void) sprintf(buffer, "meml=%s", memory_lock);
	    WRAP_CONCAT;
	}
	if (memory_unlock)
	{
	    (void) sprintf(buffer, "memu=%s", memory_unlock);
	    WRAP_CONCAT;
	}
    }
    else if (tversion == V_AIX)
    {
	if (acs_chars)
	{
	    bool	box_ok = TRUE;
	    const char	*acstrans = "lqkxjmwuvtn";
	    const char	*cp;
	    char	*tp, *sp, boxchars[11];

	    tp = boxchars;
	    for (cp = acstrans; *cp; cp++)
	    {
		sp = strchr(acs_chars, *cp);
		if (sp)
		    *tp++ = sp[1];
		else
		{
		    box_ok = FALSE;
		    break;
		}
	    }
	    tp[0] = '\0';

	    if (box_ok)
	    {
		(void) strcpy(buffer, "box1=");
		(void) strcat(buffer, expand(boxchars));
		WRAP_CONCAT;
	    }
	}
    }

    /*
     * kludge: trim off trailer to avoid an extra blank line
     * in infocmp -u output when there are no string differences
     */
    if (outcount)
    {
	j = out_used;
	if (j >= 2
	 && outbuf[j-1] == '\t'
	 && outbuf[j-2] == '\n') {
	    out_used -= 2;
	} else if (j >= 4
	 && outbuf[j-1] == ':'
	 && outbuf[j-2] == '\t'
	 && outbuf[j-3] == '\n'
	 && outbuf[j-4] == '\\') {
	    out_used -= 4;
	}
	outbuf[out_used] = '\0';
	column = oldcol;
    }

#if 0
    fprintf(stderr, "num_bools = %d\n", num_bools);
    fprintf(stderr, "num_values = %d\n", num_values);
    fprintf(stderr, "num_strings = %d\n", num_strings);
    fprintf(stderr, "term_names=%s, len=%d, strlen(outbuf)=%d, outbuf=%s\n",
	    tterm->term_names, len, out_used, outbuf);
#endif
    /*
     * Here's where we use infodump to trigger a more stringent length check
     * for termcap-translation purposes.
     * Return the length of the raw entry, without tc= expansions,
     * It gives an idea of which entries are deadly to even *scan past*,
     * as opposed to *use*.
     */
    return(infodump ? len : termcap_length(outbuf));
}

int dump_entry(TERMTYPE *tterm, bool limited, int (*pred)(int type, int idx))
/* dump a single entry */
{
    int	len, critlen;
    const char	*legend;
    bool	infodump;

    if (outform==F_TERMCAP || outform==F_TCONVERR)
    {
	critlen = MAX_TERMCAP_LENGTH;
	legend = "older termcap";
	infodump = FALSE;
	set_obsolete_termcaps(tterm);
    }
    else
    {
	critlen = MAX_TERMINFO_LENGTH;
	legend = "terminfo";
	infodump = TRUE;
    }

    if (((len = fmt_entry(tterm, pred, FALSE, infodump)) > critlen) && limited)
    {
	(void) printf("# (untranslatable capabilities removed to fit entry within %d bytes)\n",
		      critlen);
	if ((len = fmt_entry(tterm, pred, TRUE, infodump)) > critlen)
	{
	    /*
	     * We pick on sgr because it's a nice long string capability that
	     * is really just an optimization hack.
	     */
	    char *oldsgr = set_attributes;
	    set_attributes = ABSENT_STRING; 
	    (void) printf("# (sgr removed to fit entry within %d bytes)\n",
			  critlen);
	    if ((len = fmt_entry(tterm, pred, TRUE, infodump)) > critlen)
	    {
		int oldversion = tversion;

		tversion = V_BSD;
		(void) printf("# (terminfo-only capabilities suppressed to fit entry within %d bytes)\n",
			      critlen);

		if ((len = fmt_entry(tterm, pred, TRUE, infodump)) > critlen)
		{
		    (void) fprintf(stderr,
			       "warning: %s entry is %d bytes long\n",
			       _nc_first_name(tterm->term_names),
			       len);
		    (void) printf(
			      "# WARNING: this entry, %d bytes long, may core-dump %s libraries!\n",
			      len, legend);
		}
		tversion = oldversion;
	    }
	    set_attributes = oldsgr;
	}
    }

    (void) fputs(outbuf, stdout);
    return len;
}

int dump_uses(const char *name, bool infodump)
/* dump "use=" clauses in the appropriate format */
{
    char buffer[MAX_TERMINFO_LENGTH];

    append_output(NULL);
    (void)sprintf(buffer, "%s%s", infodump ? "use=" : "tc=", name);
    wrap_concat(buffer);
    (void) fputs(outbuf, stdout);
    return out_used;
}

void compare_entry(void (*hook)(int t, int i, const char *name))
/* compare two entries */
{
    int	i, j;

    (void) fputs("    comparing booleans.\n", stdout);
    for (j=0; j < BOOLCOUNT; j++)
    {
	if (sortmode == S_NOSORT)
	    i = j;
	else
	    i = bool_indirect[j];

	if ((outform == F_LITERAL || outform == F_TERMINFO || outform == F_VARIABLE)
		 && (OBSOLETE(bool_names[i]) && outform != F_LITERAL))
	    continue;

	(*hook)(BOOLEAN, i, bool_names[i]);
    }

    (void) fputs("    comparing numbers.\n", stdout);
    for (j=0; j < NUMCOUNT; j++)
    {
	if (sortmode == S_NOSORT)
	    i = j;
	else
	    i = num_indirect[j];

	if ((outform==F_LITERAL || outform==F_TERMINFO || outform==F_VARIABLE)
		 && (OBSOLETE(num_names[i]) && outform != F_LITERAL))
	    continue;

	(*hook)(NUMBER, i, num_names[i]);
    }

    (void) fputs("    comparing strings.\n", stdout);
    for (j=0; j < STRCOUNT; j++)
    {
	if (sortmode == S_NOSORT)
	    i = j;
	else
	    i = str_indirect[j];

	if ((outform==F_LITERAL || outform==F_TERMINFO || outform==F_VARIABLE)
		 && (OBSOLETE(str_names[i]) && outform != F_LITERAL))
	    continue;

	(*hook)(STRING, i, str_names[i]);
    }
}

#define NOTSET(s)	((s) == (char *)NULL)

/*
 * This bit of legerdemain turns all the terminfo variable names into
 * references to locations in the arrays Booleans, Numbers, and Strings ---
 * precisely what's needed.
 */
#undef CUR
#define CUR tp->

static void set_obsolete_termcaps(TERMTYPE *tp)
{
#include "capdefaults.c"
}
