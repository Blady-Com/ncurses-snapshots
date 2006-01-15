/****************************************************************************
 * Copyright (c) 1998-2005,2006 Free Software Foundation, Inc.              *
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
 *     and: Thomas E. Dickey                        1996-on                 *
 ****************************************************************************/

/*
 *	read_entry.c -- Routine for reading in a compiled terminfo file
 *
 */

#include <curses.priv.h>

#include <tic.h>
#include <term_entry.h>

MODULE_ID("$Id: read_entry.c,v 1.84 2006/01/14 23:19:33 tom Exp $")

#if !HAVE_TELL
#define tell(fd) lseek(fd, 0, SEEK_CUR)		/* lseek() is POSIX, but not tell() */
#endif

#define TYPE_CALLOC(type,elts) typeCalloc(type, (unsigned)(elts))

/*
 *	int
 *	_nc_read_file_entry(filename, ptr)
 *
 *	Read the compiled terminfo entry in the given file into the
 *	structure pointed to by ptr, allocating space for the string
 *	table.
 */

#undef  BYTE
#define BYTE(p,n)	(unsigned char)((p)[n])

#define IS_NEG1(p)	((BYTE(p,0) == 0377) && (BYTE(p,1) == 0377))
#define IS_NEG2(p)	((BYTE(p,0) == 0376) && (BYTE(p,1) == 0377))
#define LOW_MSB(p)	(BYTE(p,0) + 256*BYTE(p,1))

static bool have_tic_directory = FALSE;
static bool keep_tic_directory = FALSE;

/*
 * Record the "official" location of the terminfo directory, according to
 * the place where we're writing to, or the normal default, if not.
 */
NCURSES_EXPORT(const char *)
_nc_tic_dir(const char *path)
{
    static const char *result = TERMINFO;

    if (!keep_tic_directory) {
	if (path != 0) {
	    result = path;
	    have_tic_directory = TRUE;
	} else if (!have_tic_directory && use_terminfo_vars()) {
	    char *envp;
	    if ((envp = getenv("TERMINFO")) != 0)
		return _nc_tic_dir(envp);
	}
    }
    return result;
}

/*
 * Special fix to prevent the terminfo directory from being moved after tic
 * has chdir'd to it.  If we let it be changed, then if $TERMINFO has a
 * relative path, we'll lose track of the actual directory.
 */
NCURSES_EXPORT(void)
_nc_keep_tic_dir(const char *path)
{
    _nc_tic_dir(path);
    keep_tic_directory = TRUE;
}

static void
convert_shorts(char *buf, short *Numbers, int count)
{
    int i;
    for (i = 0; i < count; i++) {
	if (IS_NEG1(buf + 2 * i))
	    Numbers[i] = ABSENT_NUMERIC;
	else if (IS_NEG2(buf + 2 * i))
	    Numbers[i] = CANCELLED_NUMERIC;
	else
	    Numbers[i] = LOW_MSB(buf + 2 * i);
	TR(TRACE_DATABASE, ("get Numbers[%d]=%d", i, Numbers[i]));
    }
}

static void
convert_strings(char *buf, char **Strings, int count, int size, char *table)
{
    int i;
    char *p;

    for (i = 0; i < count; i++) {
	if (IS_NEG1(buf + 2 * i)) {
	    Strings[i] = ABSENT_STRING;
	} else if (IS_NEG2(buf + 2 * i)) {
	    Strings[i] = CANCELLED_STRING;
	} else if (LOW_MSB(buf + 2 * i) > size) {
	    Strings[i] = ABSENT_STRING;
	} else {
	    Strings[i] = (LOW_MSB(buf + 2 * i) + table);
	    TR(TRACE_DATABASE, ("Strings[%d] = %s", i, _nc_visbuf(Strings[i])));
	}

	/* make sure all strings are NUL terminated */
	if (VALID_STRING(Strings[i])) {
	    for (p = Strings[i]; p <= table + size; p++)
		if (*p == '\0')
		    break;
	    /* if there is no NUL, ignore the string */
	    if (p > table + size)
		Strings[i] = ABSENT_STRING;
	}
    }
}

#define read_shorts(fd, buf, count) \
	(read(fd, buf, (unsigned) (count)*2) == (int) (count)*2)

#define even_boundary(value) \
    if ((value) % 2 != 0) read(fd, buf, 1)

static int
read_termtype(int fd, TERMTYPE *ptr)
/* return 1 if read, 0 if not found or garbled */
{
    int name_size, bool_count, num_count, str_count, str_size;
    int i;
    char buf[MAX_ENTRY_SIZE + 1];
    unsigned want, have;

    TR(TRACE_DATABASE, ("READ termtype header @%ld", (long) tell(fd)));

    memset(ptr, 0, sizeof(*ptr));

    /* grab the header */
    if (!read_shorts(fd, buf, 6)
	|| LOW_MSB(buf) != MAGIC) {
	return (TGETENT_NO);
    }

    name_size = LOW_MSB(buf + 2);
    bool_count = LOW_MSB(buf + 4);
    num_count = LOW_MSB(buf + 6);
    str_count = LOW_MSB(buf + 8);
    str_size = LOW_MSB(buf + 10);

    TR(TRACE_DATABASE,
       ("TERMTYPE name_size=%d, bool=%d/%d, num=%d/%d str=%d/%d(%d)",
	name_size, bool_count, BOOLCOUNT, num_count, NUMCOUNT,
	str_count, STRCOUNT, str_size));
    if (name_size < 0
	|| bool_count < 0
	|| num_count < 0
	|| str_count < 0
	|| str_size < 0) {
	return (TGETENT_NO);
    }

    if (str_size) {
	/* try to allocate space for the string table */
	if (str_count * 2 >= (int) sizeof(buf)
	    || (ptr->str_table = typeMalloc(char, (unsigned) str_size)) == 0) {
	    return (TGETENT_NO);
	}
    } else {
	str_count = 0;
    }

    /* grab the name (a null-terminated string) */
    want = min(MAX_NAME_SIZE, (unsigned) name_size);
    if ((have = read(fd, buf, want)) != want) {
	memset(buf + have, 0, want - have);
    }
    buf[want] = '\0';
    ptr->term_names = TYPE_CALLOC(char, strlen(buf) + 1);
    if (ptr->term_names == NULL) {
	return (TGETENT_NO);
    }
    (void) strcpy(ptr->term_names, buf);
    if (have > MAX_NAME_SIZE)
	lseek(fd, (off_t) (have - MAX_NAME_SIZE), 1);

    /* grab the booleans */
    if ((ptr->Booleans = TYPE_CALLOC(char, max(BOOLCOUNT, bool_count))) == 0
	|| read(fd, ptr->Booleans, (unsigned) bool_count) < bool_count) {
	return (TGETENT_NO);
    }

    /*
     * If booleans end on an odd byte, skip it.  The machine they
     * originally wrote terminfo on must have been a 16-bit
     * word-oriented machine that would trap out if you tried a
     * word access off a 2-byte boundary.
     */
    even_boundary(name_size + bool_count);

    /* grab the numbers */
    if ((ptr->Numbers = TYPE_CALLOC(short, max(NUMCOUNT, num_count))) == 0
	|| !read_shorts(fd, buf, num_count)) {
	return (TGETENT_NO);
    }
    convert_shorts(buf, ptr->Numbers, num_count);

    if ((ptr->Strings = TYPE_CALLOC(char *, max(STRCOUNT, str_count))) == 0)
	  return (TGETENT_NO);

    if (str_count) {
	/* grab the string offsets */
	if (!read_shorts(fd, buf, str_count)) {
	    return (TGETENT_NO);
	}
	/* finally, grab the string table itself */
	if (read(fd, ptr->str_table, (unsigned) str_size) != str_size)
	    return (TGETENT_NO);
	convert_strings(buf, ptr->Strings, str_count, str_size, ptr->str_table);
    }
#if NCURSES_XNAMES

    ptr->num_Booleans = BOOLCOUNT;
    ptr->num_Numbers = NUMCOUNT;
    ptr->num_Strings = STRCOUNT;

    /*
     * Read extended entries, if any, after the normal end of terminfo data.
     */
    even_boundary(str_size);
    TR(TRACE_DATABASE, ("READ extended_header @%ld", (long) tell(fd)));
    if (_nc_user_definable && read_shorts(fd, buf, 5)) {
	int ext_bool_count = LOW_MSB(buf + 0);
	int ext_num_count = LOW_MSB(buf + 2);
	int ext_str_count = LOW_MSB(buf + 4);
	int ext_str_size = LOW_MSB(buf + 6);
	int ext_str_limit = LOW_MSB(buf + 8);
	unsigned need = (ext_bool_count + ext_num_count + ext_str_count);
	int base = 0;

	if (need >= sizeof(buf)
	    || ext_str_size >= (int) sizeof(buf)
	    || ext_str_limit >= (int) sizeof(buf)
	    || ext_bool_count < 0
	    || ext_num_count < 0
	    || ext_str_count < 0
	    || ext_str_size < 0
	    || ext_str_limit < 0)
	    return (TGETENT_NO);

	ptr->num_Booleans = BOOLCOUNT + ext_bool_count;
	ptr->num_Numbers = NUMCOUNT + ext_num_count;
	ptr->num_Strings = STRCOUNT + ext_str_count;

	ptr->Booleans = typeRealloc(char, ptr->num_Booleans, ptr->Booleans);
	ptr->Numbers = typeRealloc(short, ptr->num_Numbers, ptr->Numbers);
	ptr->Strings = typeRealloc(char *, ptr->num_Strings, ptr->Strings);

	TR(TRACE_DATABASE, ("extended header is %d/%d/%d(%d:%d)",
			    ext_bool_count, ext_num_count, ext_str_count,
			    ext_str_size, ext_str_limit));

	TR(TRACE_DATABASE, ("READ %d extended-booleans @%ld",
			    ext_bool_count, (long) tell(fd)));
	if ((ptr->ext_Booleans = ext_bool_count) != 0) {
	    if (read(fd, ptr->Booleans + BOOLCOUNT, (unsigned)
		     ext_bool_count) != ext_bool_count)
		return (TGETENT_NO);
	}
	even_boundary(ext_bool_count);

	TR(TRACE_DATABASE, ("READ %d extended-numbers @%ld",
			    ext_num_count, (long) tell(fd)));
	if ((ptr->ext_Numbers = ext_num_count) != 0) {
	    if (!read_shorts(fd, buf, ext_num_count))
		return (TGETENT_NO);
	    TR(TRACE_DATABASE, ("Before converting extended-numbers"));
	    convert_shorts(buf, ptr->Numbers + NUMCOUNT, ext_num_count);
	}

	TR(TRACE_DATABASE, ("READ extended-offsets @%ld", (long) tell(fd)));
	if ((ext_str_count || need)
	    && !read_shorts(fd, buf, ext_str_count + need))
	    return (TGETENT_NO);

	TR(TRACE_DATABASE, ("READ %d bytes of extended-strings @%ld",
			    ext_str_limit, (long) tell(fd)));

	if (ext_str_limit) {
	    if ((ptr->ext_str_table = typeMalloc(char, ext_str_limit)) == 0)
		  return (TGETENT_NO);
	    if (read(fd, ptr->ext_str_table, (unsigned) ext_str_limit) != ext_str_limit)
		return (TGETENT_NO);
	    TR(TRACE_DATABASE, ("first extended-string is %s", _nc_visbuf(ptr->ext_str_table)));
	}

	if ((ptr->ext_Strings = ext_str_count) != 0) {
	    TR(TRACE_DATABASE,
	       ("Before computing extended-string capabilities str_count=%d, ext_str_count=%d",
		str_count, ext_str_count));
	    convert_strings(buf, ptr->Strings + str_count, ext_str_count,
			    ext_str_limit, ptr->ext_str_table);
	    for (i = ext_str_count - 1; i >= 0; i--) {
		TR(TRACE_DATABASE, ("MOVE from [%d:%d] %s",
				    i, i + str_count,
				    _nc_visbuf(ptr->Strings[i + str_count])));
		ptr->Strings[i + STRCOUNT] = ptr->Strings[i + str_count];
		if (VALID_STRING(ptr->Strings[i + STRCOUNT]))
		    base += (strlen(ptr->Strings[i + STRCOUNT]) + 1);
		TR(TRACE_DATABASE, ("... to    [%d] %s",
				    i + STRCOUNT,
				    _nc_visbuf(ptr->Strings[i + STRCOUNT])));
	    }
	}

	if (need) {
	    if ((ptr->ext_Names = TYPE_CALLOC(char *, need)) == 0)
		  return (TGETENT_NO);
	    TR(TRACE_DATABASE,
	       ("ext_NAMES starting @%d in extended_strings, first = %s",
		base, _nc_visbuf(ptr->ext_str_table + base)));
	    convert_strings(buf + (2 * ext_str_count),
			    ptr->ext_Names,
			    (int) need,
			    ext_str_limit, ptr->ext_str_table + base);
	}

	T(("...done reading terminfo bool %d(%d) num %d(%d) str %d(%d)",
	   ptr->num_Booleans, ptr->ext_Booleans,
	   ptr->num_Numbers, ptr->ext_Numbers,
	   ptr->num_Strings, ptr->ext_Strings));

	TR(TRACE_DATABASE, ("extend: num_Booleans:%d", ptr->num_Booleans));
    } else
#endif /* NCURSES_XNAMES */
    {
	T(("...done reading terminfo bool %d num %d str %d",
	   bool_count, num_count, str_count));
#if NCURSES_XNAMES
	TR(TRACE_DATABASE, ("normal: num_Booleans:%d", ptr->num_Booleans));
#endif
    }

    for (i = bool_count; i < BOOLCOUNT; i++)
	ptr->Booleans[i] = FALSE;
    for (i = num_count; i < NUMCOUNT; i++)
	ptr->Numbers[i] = ABSENT_NUMERIC;
    for (i = str_count; i < STRCOUNT; i++)
	ptr->Strings[i] = ABSENT_STRING;

    return (TGETENT_YES);
}

NCURSES_EXPORT(int)
_nc_read_file_entry(const char *const filename, TERMTYPE *ptr)
/* return 1 if read, 0 if not found or garbled */
{
    int code, fd = -1;

    if (_nc_access(filename, R_OK) < 0
	|| (fd = open(filename, O_RDONLY | O_BINARY)) < 0) {
	T(("cannot open terminfo %s (errno=%d)", filename, errno));
	code = TGETENT_NO;
    } else {
	T(("read terminfo %s", filename));
	if ((code = read_termtype(fd, ptr)) == 0) {
	    _nc_free_termtype(ptr);
	}
	close(fd);
    }

    return (code);
}

/*
 * Build a terminfo pathname and try to read the data.  Returns 1 on success,
 * 0 on failure.
 */
static int
_nc_read_tic_entry(char *filename,
		   unsigned limit,
		   const char *const dir,
		   const char *name,
		   TERMTYPE *const tp)
{
    unsigned need = 4 + strlen(dir) + strlen(name);

    if (need > limit)
	return TGETENT_NO;

    (void) sprintf(filename, "%s/%c/%s", dir, *name, name);
    return _nc_read_file_entry(filename, tp);
}

/*
 * Process the list of :-separated directories, looking for the terminal type.
 * We don't use strtok because it does not show us empty tokens.
 */

static char *this_db_list = 0;
static int size_db_list;

/*
 * Cleanup.
 */
NCURSES_EXPORT(void)
_nc_last_db(void)
{
    FreeAndNull(this_db_list);
    size_db_list = 0;
}

/* The TERMINFO_DIRS value, if defined by the configure script, begins with a
 * ":", which will be interpreted as TERMINFO.
 */
static char *
next_list_item(char *source, int *offset)
{
    if (source != 0) {
	FreeIfNeeded(this_db_list);
	this_db_list = strdup(source);
	size_db_list = strlen(source);
    }

    if (this_db_list != 0 && size_db_list && *offset < size_db_list) {
	char *result = this_db_list + *offset;
	char *marker = strchr(result, NCURSES_PATHSEP);

	/*
	 * Put a null on the marker if a separator was found.  Set the offset
	 * to the next position after the marker so we can call this function
	 * again, using the data at the offset.
	 */
	if (marker == 0) {
	    *offset += strlen(result) + 1;
	    marker = result + *offset;	// FIXME
	} else {
	    *marker++ = 0;
	    *offset = marker - this_db_list;
	}
	if (*result == 0 && result != (this_db_list + size_db_list))
	    result = TERMINFO;
	return result;
    }
    return 0;
}

#define NEXT_DBD(var, offset) next_list_item((*offset == 0) ? var : 0, offset)

/*
 * This is a simple iterator which allows the caller to step through the
 * possible locations for a terminfo directory.  ncurses uses this to find
 * terminfo files to read.
 */
NCURSES_EXPORT(const char *)
_nc_next_db(DBDIRS * state, int *offset)
{
    const char *result;
    char *envp;

    while (*state < dbdLAST) {
	DBDIRS next = *state + 1;

	result = 0;

	switch (*state) {
	case dbdTIC:
	    if (have_tic_directory)
		result = _nc_tic_dir(0);
	    break;
	case dbdEnvOnce:
	    if (use_terminfo_vars()) {
		if ((envp = getenv("TERMINFO")) != 0)
		    result = _nc_tic_dir(envp);
	    }
	    break;
	case dbdHome:
	    if (use_terminfo_vars()) {
		result = _nc_home_terminfo();
	    }
	    break;
	case dbdEnvList:
	    if (use_terminfo_vars()) {
		if ((result = NEXT_DBD(getenv("TERMINFO_DIRS"), offset)) != 0)
		    next = *state;
	    }
	    break;
	case dbdCfgList:
#ifdef TERMINFO_DIRS
	    if ((result = NEXT_DBD(TERMINFO_DIRS, offset)) != 0)
		next = *state;
#endif
	    break;
	case dbdCfgOnce:
#ifndef TERMINFO_DIRS
	    result = TERMINFO;
#endif
	    break;
	case dbdLAST:
	    break;
	}
	if (*state != next) {
	    *state = next;
	    *offset = 0;
	    _nc_last_db();
	}
	if (result != 0) {
	    return result;
	}
    }
    return 0;
}

NCURSES_EXPORT(void)
_nc_first_db(DBDIRS * state, int *offset)
{
    *state = dbdTIC;
    *offset = 0;
}

/*
 *	_nc_read_entry(char *name, char *filename, TERMTYPE *tp)
 *
 *	Find and read the compiled entry for a given terminal type,
 *	if it exists.  We take pains here to make sure no combination
 *	of environment variables and terminal type name can be used to
 *	overrun the file buffer.
 */

NCURSES_EXPORT(int)
_nc_read_entry(const char *const name, char *const filename, TERMTYPE *const tp)
{
    if (strlen(name) == 0
	|| strcmp(name, ".") == 0
	|| strcmp(name, "..") == 0
	|| _nc_pathlast(name) != 0) {
	T(("illegal or missing entry name '%s'", name));
	return TGETENT_NO;
    } else {
	DBDIRS state = dbdTIC;
	int offset = 0;
	const char *path;

	while ((path = _nc_next_db(&state, &offset)) != 0) {
	    if (_nc_read_tic_entry(filename, PATH_MAX, path, name, tp) == 1) {
		_nc_last_db();
		return TGETENT_YES;
	    }
	}
    }
    return TGETENT_NO;
}
