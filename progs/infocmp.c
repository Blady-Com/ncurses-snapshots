
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
 *	infocmp.c -- decompile an entry, or compare two entries
 *		written by Eric S. Raymond
 *
 */

#include <config.h>

#include <stdlib.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/param.h>		/* for MAXPATHLEN */
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif /* HAVE_GETOPT_H */

#include "term.h"
#include "tic.h"
#include "term_entry.h"
#include "dump_entry.h"

#define MAXTERMS	32	/* max # terminal arguments we can handle */

char *_nc_progname = "infocmp";

typedef char	path[MAXPATHLEN];

/***************************************************************************
 *
 * The following control variables, together with the contents of the
 * terminfo entries, completely determine the actions of the program.
 *
 ***************************************************************************/

static char *tname[MAXTERMS];	/* terminal type names */
static TERMTYPE term[MAXTERMS];	/* terminfo entries */
static int termcount;		/* count of terminal entries */

static int outform;		/* output format */
static int sortmode;		/* sort_mode */
static int itrace;		/* trace flag for debugging */
static int mwidth = 60;

/* main comparison mode */
static int compare;
#define C_DEFAULT	0	/* don't force comparison mode */
#define C_DIFFERENCE	1	/* list differences between two terminals */
#define C_COMMON	2	/* list common capabilities */
#define C_NAND		3	/* list capabilities in neither terminal */
#define C_USEALL	4	/* generate relative use-form entry */
static bool ignorepads;		/* ignore pad prefixes when diffing */

/***************************************************************************
 *
 * Predicates for dump function
 *
 ***************************************************************************/

static int use_predicate(int type, int idx)
/* predicate function to use for use decompilation */
{
	TERMTYPE *tp;

	switch(type)
	{
	case BOOLEAN: {
		int is_set = FALSE;

		/*
		 * This assumes that multiple use entries are supposed
		 * to contribute the logical or of their boolean capabilities.
		 * This is true if we take the semantics of multiple uses to
		 * be 'each capability gets the first non-default value found
		 * in the sequence of use entries'.
		 */
		for (tp = &term[1]; tp < term + termcount; tp++)
			if (tp->Booleans[idx]) {
				is_set = TRUE;
				break;
			}
			if (is_set != term->Booleans[idx])
				return(!is_set);
			else
				return(FAIL);
		}

	case NUMBER: {
		int	value = -1;

		/*
		 * We take the semantics of multiple uses to be 'each
		 * capability gets the first non-default value found
		 * in the sequence of use entries'.
		 */
		for (tp = &term[1]; tp < term + termcount; tp++)
			if (tp->Numbers[idx] >= 0) {
				value = tp->Numbers[idx];
				break;
			}

		if (value != term->Numbers[idx])
			return(value != -1);
		else
			return(FAIL);
		}

	case STRING: {
		char *termstr, *usestr = (char *)NULL;

		termstr = term->Strings[idx];

		/*
		 * We take the semantics of multiple uses to be 'each
		 * capability gets the first non-default value found
		 * in the sequence of use entries'.
		 */
		for (tp = &term[1]; tp < term + termcount; tp++)
			if (tp->Strings[idx])
			{
				usestr = tp->Strings[idx];
				break;
			}

		if (usestr == (char *)NULL && termstr == (char *)NULL)
			return(FAIL);
		else if (!usestr||!termstr||strcmp(usestr,termstr)!=0)
			return(TRUE);
		else
			return(FAIL);
	    }
	}

	return(FALSE);	/* pacify compiler */
}

static bool entryeq(TERMTYPE *t1, TERMTYPE *t2)
/* are two terminal types equal */
{
    int	i;

    for (i = 0; i < BOOLCOUNT; i++)
	if (t1->Booleans[i] != t2->Booleans[i])
	    return(FALSE);

    for (i = 0; i < NUMCOUNT; i++)
	if (t1->Numbers[i] != t2->Numbers[i])
	    return(FALSE);

    for (i = 0; i < STRCOUNT; i++)
	if (_nc_capcmp(t1->Strings[i], t2->Strings[i]))
	    return(FALSE);

    return(TRUE);
}

static void compare_predicate(int type, int idx, const char *name)
/* predicate function to use for ordinary decompilation */
{
	register TERMTYPE *t1 = &term[0];
	register TERMTYPE *t2 = &term[1];
	char *s1, *s2;

	switch(type)
	{
	case BOOLEAN:
		switch(compare)
		{
		case C_DIFFERENCE:
			if (t1->Booleans[idx] != t2->Booleans[idx])
			(void) printf("\t%s: %c:%c.\n", 
					  name,
					  t1->Booleans[idx] ? 'T' : 'F',
					  t2->Booleans[idx] ? 'T' : 'F');
			break;

		case C_COMMON:
			if (t1->Booleans[idx] && t2->Booleans[idx])
			(void) printf("\t%s= T.\n", name);
			break;

		case C_NAND:
			if (!t1->Booleans[idx] && !t2->Booleans[idx])
			(void) printf("\t!%s.\n", name);
			break;
		}
		break;

	case NUMBER:
		switch(compare)
		{
		case C_DIFFERENCE:
			if (t1->Numbers[idx] != t2->Numbers[idx])
			(void) printf("\t%s: %d:%d.\n", 
					  name, t1->Numbers[idx], t2->Numbers[idx]);
			break;

		case C_COMMON:
			if (t1->Numbers[idx]!=-1 && t2->Numbers[idx]!=-1
				&& t1->Numbers[idx] == t2->Numbers[idx])
			(void) printf("\t%s= %d.\n", name, t1->Numbers[idx]);
			break;

		case C_NAND:
			if (t1->Numbers[idx]==-1 && t2->Numbers[idx] == -1)
			(void) printf("\t!%s.\n", name);
			break;
		}
	break;

	case STRING:
		s1 = t1->Strings[idx];
		s2 = t2->Strings[idx];
		switch(compare)
		{
		case C_DIFFERENCE:
			if (_nc_capcmp(s1, s2))
			{
				char	buf1[BUFSIZ], buf2[BUFSIZ];

				if (s1 == (char *)NULL)
					(void) strcpy(buf1, "NULL");
				else
				{
					(void) strcpy(buf1, "'"); 
					(void) strcat(buf1, expand(s1));
					(void) strcat(buf1, "'"); 
				}

				if (s2 == (char *)NULL)
					(void) strcpy(buf2, "NULL");
				else
				{
					(void) strcpy(buf2, "'"); 
					(void) strcat(buf2, expand(s2));
					(void) strcat(buf2, "'"); 
				}

				(void) printf("\t%s: %s, %s.\n",
					      name, buf1, buf2);
			}
			break;

		case C_COMMON:
			if (s1 && s2 && !_nc_capcmp(s1, s2))
				(void) printf("\t%s= '%s'.\n",name,expand(s1));
			break;

		case C_NAND:
			if (!s1 && !s2)
				(void) printf("\t!%s.\n", name);
			break;
		}
		break;
	}

}

/***************************************************************************
 *
 * Main sequence
 *
 ***************************************************************************/

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else
extern char *optarg;
extern int optind;
#endif /* HAVE_GETOPT_H */

int main(int argc, char *argv[])
{
	char *terminal, *firstdir, *restdir;
	path tfile[MAXTERMS];
	int saveoptind, c, i;
	bool filecompare = FALSE;
	bool typelist = FALSE;

	if ((terminal = getenv("TERM")) == NULL)
	{
		(void) fprintf(stderr,
			"infocmp: environment variable TERM not set\n");
		exit(1);
	}

	/* where is the terminfo database location going to default to? */
	if ((firstdir = getenv("TERMINFO")) == NULL)
		firstdir = TERMINFO;
	restdir = firstdir;

	while ((c = getopt(argc, argv, "dcCFnlLprs:TuvVw:A:B:1")) != EOF)
		switch (c)
		{
		case 'd':
			compare = C_DIFFERENCE;
			break;

		case 'c':
			compare = C_COMMON;
			break;

		case 'C':
			outform = F_TERMCAP;
			if (sortmode == S_DEFAULT)
			sortmode = S_TERMCAP;
			break;

		case 'F':
			filecompare = TRUE;
			break;

		case 'l':
			outform = F_TERMINFO;
			break;

		case 'L':
			outform = F_VARIABLE;
			if (sortmode == S_DEFAULT)
			sortmode = S_VARIABLE;
			break;

		case 'n':
			compare = C_NAND;
			break;

		case 'p':
			ignorepads = TRUE;
			break;

		case 'r':
			outform = F_TCONVERR;
			break;

		case 's':
			if (*optarg == 'd')
				sortmode = S_NOSORT;
			else if (*optarg == 'i')
				sortmode = S_TERMINFO;
			else if (*optarg == 'l')
				sortmode = S_VARIABLE;
			else if (*optarg == 'c')
				sortmode = S_TERMCAP;
			else
			{
				(void) fprintf(stderr,
					       "infocmp: unknown sort mode\n");
				exit(1);
			}
			break;

		case 'T':
			typelist = TRUE;
			break;

		case 'u':
			compare = C_USEALL;
			break;

		case 'v':
			itrace = 1;
			break;

		case 'V':
			(void) fputs(NCURSES_VERSION, stdout);
			exit(0);

		case 'w':
			mwidth = atoi(optarg);
			break;

		case 'A':
			firstdir = optarg;
			break;

		case 'B':
			restdir = optarg;
			break;

		case '1':
			mwidth = 0;
			break;
		}

	/* user may want a simple terminal type listing */
	if (typelist)
	{
	    DIR	*termdir;
	    struct dirent *subdir;

	    if ((termdir = opendir(firstdir)) == (DIR *)NULL)
	    {
		(void) fprintf(stderr,
			"infocmp: can't open terminfo directory %s\n",
			firstdir);
		return(0);
	    }

	    while ((subdir = readdir(termdir)) != NULL)
	    {
		char	buf[256];
		DIR	*entrydir;
		struct dirent *entry;

		if (!strcmp(subdir->d_name, ".")
			|| !strcmp(subdir->d_name, ".."))
		    continue;

		(void) strcpy(buf, firstdir);
		(void) strcat(buf, "/");
		(void) strcat(buf, subdir->d_name);
		(void) strcat(buf, "/");
		chdir(buf);
		entrydir = opendir(".");
		while ((entry = readdir(entrydir)) != NULL)
		{
		    TERMTYPE	lterm;
		    char	*cn, *desc;

		    if (!strcmp(entry->d_name, ".")
				|| !strcmp(entry->d_name, ".."))
			continue;

		    if (_nc_read_file_entry(entry->d_name, &lterm)==-1)
		    {
			(void) fprintf(stderr,
				       "couldn't open terminfo file %s.\n",
				       tfile[termcount]);
			return(1);
		    }

		    /* only list things once, by primary name */
		    cn = canonical_name(lterm.term_names, (char *)NULL);
		    if (strcmp(cn, entry->d_name))
			continue;

		    /* get a description for the type */
		    if ((desc = strrchr(lterm.term_names,'|')) == (char *)NULL)
			desc = "(No description)";
		    else
			++desc;

		    (void) printf("%-10s\t%s\n", cn, desc);
		}
	    }

	    return 0;
	}

	/* by default, sort by terminfo name */
	if (sortmode == S_DEFAULT)
		sortmode = S_TERMINFO;

	/* set up for display */
	dump_init(outform, sortmode, mwidth, itrace);

	/* make sure we have at least one terminal name to work with */
	if (optind >= argc)
		argv[argc++] = terminal;

	/* if user is after a comparison, make sure we have two entries */
	if (compare != C_DEFAULT && optind >= argc - 1)
		argv[argc++] = terminal;

	/* exactly two terminal names with no options means do -d */
	if (argc - optind == 2 && compare == C_DEFAULT)
		compare = C_DIFFERENCE;

	if (!filecompare)
	{
	    /* grab the entries */
	    termcount = 0;
	    for (saveoptind = optind; optind < argc; optind++)
		if (termcount >= MAXTERMS)
		{
		    (void) fprintf(stderr,
			   "infocmp: too many terminal type arguments\n");
		    exit(1);
		}
		else
		{
		    char	*directory = termcount ? restdir : firstdir;

		    tname[termcount] = argv[optind];
		    (void) sprintf(tfile[termcount], "%s/%c/%s",
				   directory,
				   *argv[optind], argv[optind]);
		    if (itrace)
			(void) fprintf(stderr,
			       "infocmp: reading entry %s from file %s\n",
			       argv[optind], tfile[termcount]);
		    if (_nc_read_file_entry(tfile[termcount],&term[termcount])==-1)
		    {
			(void) fprintf(stderr,
				       "couldn't open terminfo file %s.\n",
				       tfile[termcount]);
			(void) fprintf(stderr,
				       "The terminal you are using is not defined.\n");
			exit(1);
		    }
		    termcount++;
		}

	    /*
	     * Here's where the real work gets done
	     */
	    switch (compare)
	    {
	    case C_DEFAULT:
		if (itrace)
		    (void) fprintf(stderr,
				   "infocmp: about to dump %s\n",
				   tname[0]);
		(void) printf("#\tReconstructed via infocmp from file: %s\n",
			      tfile[0]);
		dump_entry(&term[0], NULL);
		putchar('\n');
		break;

	    case C_DIFFERENCE:
		if (itrace)
		    (void)fprintf(stderr,"infocmp: dumping differences\n");
		(void) printf("comparing %s to %s.\n", tname[0], tname[1]);
		compare_entry(compare_predicate);
		break;

	    case C_COMMON:
		if (itrace)
		    (void) fprintf(stderr,
				   "infocmp: dumping common capabilities\n");
		(void) printf("comparing %s to %s.\n", tname[0], tname[1]);
		compare_entry(compare_predicate);
		break;

	    case C_NAND:
		if (itrace)
		    (void) fprintf(stderr,
				   "infocmp: dumping differences\n");
		(void) printf("comparing %s to %s.\n", tname[0], tname[1]);
		compare_entry(compare_predicate);
		break;

	    case C_USEALL:
		if (itrace)
		    (void) fprintf(stderr, "infocmp: dumping use entry\n");
		dump_entry(&term[0], use_predicate);
		putchar('\n');
		for (i = 1; i < termcount; i++)
		    if (outform==F_TERMCAP
			|| outform==F_TCONVERT
			|| outform==F_TCONVERR)
			(void) printf("\ttc=%s,\n", tname[i]);
		    else
			(void) printf("\tuse=%s,\n", tname[i]);
		break;
	    }
	}
	else if (compare == C_USEALL)
	    (void) fprintf(stderr, "Sorry, -u doesn't work with -F\n");
	else if (compare == C_DEFAULT)
	    (void) fprintf(stderr, "Use `tic -[CI] <file>' for this.\n");
	else if (argc - optind != 2)
	    (void) fprintf(stderr,
		"File comparison needs exactly two file arguments.\n");	    
	else
	{
	    /* someday we may allow comparisons on more files */
	    int	filecount = 0;
	    ENTRY	*heads[2];
	    ENTRY	*tails[2];
	    ENTRY	*qp, *rp;

	    dump_init(F_LITERAL, S_TERMINFO, 0, itrace);

	    for (saveoptind = optind; optind < argc; optind++)
	    {
		if (freopen(argv[optind], "r", stdin) == NULL)
		    _nc_err_abort("Can't open %s", argv[optind]);

		_nc_head = _nc_tail = (ENTRY *)NULL;

		/* parse entries out of the source file */
		_nc_set_source(argv[optind]);
		_nc_read_entry_source(stdin, NULL, TRUE, FALSE);

		/* do use resolution */
		if (!_nc_resolve_uses())
		{
		    (void) fprintf(stderr,
			"There are unresolved use entries in %s:\n",
			argv[optind]);
		    for_entry_list(qp)
			if (qp->nuses)
			{
			    (void) fputs(qp->tterm.term_names, stderr);
			    (void) fputc('\n', stderr);
			}
		    exit(1);
		}

		heads[filecount] = _nc_head;
		tails[filecount] = _nc_tail;
		filecount++;
	    }

	    /* OK, all entries are in core.  Ready to do the comparison */
	    if (itrace)
		(void) fprintf(stderr, "Entries are now in core...\n");

	    /*
	     * The entry-matching loop.  We're not using the use[]
	     * slots any more (they got zeroed out by resolve_uses) so
	     * we stash each entry's matches in the other file there.
	     * Sigh, this is intrinsically quadratic.
	     */
	    for (qp = heads[0]; qp; qp = qp->next)
	    {
		for (rp = heads[1]; rp; rp = rp->next)
		    if (_nc_entry_match(qp->tterm.term_names,rp->tterm.term_names))
		    {
			/*
			 * This is why the uses[] array is (void *) -- so we
			 * can have either (char *) for names or entry 
			 * structure pointers in them and still be type-safe.
			 */
			if (qp->nuses < MAX_USES)
			    qp->uses[qp->nuses] = (void *)rp;
			qp->nuses++;
			
			if (rp->nuses < MAX_USES)
			    rp->uses[rp->nuses] = (void *)qp;
			rp->nuses++;
		    }
	    }

	    /* now we have two circular lists with crosslinks */
	    if (itrace)
		(void) fprintf(stderr, "Name matches are done...\n");

	    for (qp = heads[0]; qp; qp = qp->next)
		if (qp->nuses > 1)
		{
		    (void) fprintf(stderr,
				   "%s in file 1 (%s) has %d matches in file 2 (%s):\n",
				   canonical_name(qp->tterm.term_names, NULL),
				   argv[saveoptind],
				   qp->nuses,
				   argv[saveoptind+1]);
		    for (i = 0; i < qp->nuses; i++)
			(void) fprintf(stderr,
				       "\t%s\n",
				       canonical_name(((ENTRY *)qp->uses[i])->tterm.term_names, NULL));
		}
	    for (rp = heads[1]; rp; rp = rp->next)
		if (rp->nuses > 1)
		{
		    (void) fprintf(stderr,
				   "%s in file 2 (%s) has %d matches in file 1 (%s):\n",
				   canonical_name(rp->tterm.term_names, NULL),
				   argv[saveoptind+1],
				   rp->nuses,
				   argv[saveoptind]);
		    for (i = 0; i < rp->nuses; i++)
			(void) fprintf(stderr,
				       "\t%s\n",
				       canonical_name(((ENTRY *)rp->uses[i])->tterm.term_names, NULL));
		}

	    (void) printf("In file 1 (%s) only:\n", argv[saveoptind]);
	    for (qp = heads[0]; qp; qp = qp->next)
		if (qp->nuses == 0)
		    (void) printf("\t%s\n",
				  canonical_name(qp->tterm.term_names, NULL));

	    (void) printf("In file 2 (%s) only:\n", argv[saveoptind+1]);
	    for (rp = heads[1]; rp; rp = rp->next)
		if (rp->nuses == 0)
		    (void) printf("\t%s\n",
				  canonical_name(rp->tterm.term_names, NULL));

	    (void) printf("The following entries are equivalent:\n");
	    for (qp = heads[0]; qp; qp = qp->next)
	    {
		rp = (ENTRY *)qp->uses[0];    

		if (qp->nuses == 1 && entryeq(&qp->tterm, &rp->tterm))
		{
		    char name1[NAMESIZE], name2[NAMESIZE];

		    (void) canonical_name(qp->tterm.term_names, name1);
		    (void) canonical_name(rp->tterm.term_names, name2);

		    (void) printf("%s = %s\n", name1, name2);
		}
	    }

	    (void) printf("Differing entries:\n");
	    termcount = 2;
	    for (qp = heads[0]; qp; qp = qp->next)
	    {
		rp = (ENTRY *)qp->uses[0];

		if (qp->nuses == 1 && !entryeq(&qp->tterm, &rp->tterm))
		{
		    char name1[NAMESIZE], name2[NAMESIZE];

		    memcpy(&term[0], &qp->tterm, sizeof(TERMTYPE));
		    memcpy(&term[1], &rp->tterm, sizeof(TERMTYPE));

		    (void) canonical_name(qp->tterm.term_names, name1);
		    (void) canonical_name(rp->tterm.term_names, name2);

		    switch (compare)
		    {
		    case C_DIFFERENCE:
			if (itrace)
			    (void)fprintf(stderr,"infocmp: dumping differences\n");
			(void) printf("comparing %s to %s.\n", name1, name2);
			compare_entry(compare_predicate);
			break;

		    case C_COMMON:
			if (itrace)
			    (void) fprintf(stderr,
					   "infocmp: dumping common capabilities\n");
			(void) printf("comparing %s to %s.\n", name1, name2);
			compare_entry(compare_predicate);
			break;

		    case C_NAND:
			if (itrace)
			    (void) fprintf(stderr,
					   "infocmp: dumping differences\n");
			(void) printf("comparing %s to %s.\n", name1, name2);
			compare_entry(compare_predicate);
			break;

		    }
		}
	    }
	}

	exit(0);
}

/* infocmp.c ends here */
