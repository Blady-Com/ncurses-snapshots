
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

#include "form.priv.h"

typedef struct {
  int width;
} alphaARG;

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  static void *Make_Alpha_Type(va_list *ap)
|   
|   Description   :  Allocate structure for alpha type argument.
|
|   Return Values :  Pointer to argument structure or NULL on error
+--------------------------------------------------------------------------*/
static void *Make_Alpha_Type(va_list * ap)
{
  alphaARG *argp = (alphaARG *)malloc(sizeof(alphaARG));
  if (argp)
    {
      argp->width = va_arg(*ap,int);
    }
  return ((void *)argp);
}

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  static void *Copy_Alpha_Type(const void * argp)
|   
|   Description   :  Copy structure for alpha type argument.  
|
|   Return Values :  Pointer to argument structure or NULL on error.
+--------------------------------------------------------------------------*/
static void *Copy_Alpha_Type(const void * argp)
{
  alphaARG *ap  = (alphaARG *)argp;
  alphaARG *new = (alphaARG *)malloc(sizeof(alphaARG));
  
  if (new)
    {
      *new = *ap;
    }
  return ((void *)new);
}

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  static void Free_Alpha_Type( void * argp )
|   
|   Description   :  Free structure for alpha type argument.
|
|   Return Values :  -
+--------------------------------------------------------------------------*/
static void Free_Alpha_Type(void * argp)
{
  if (argp) 
    free(argp);
}

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  static bool Check_Alpha_Field(
|                                      FIELD * field,
|                                      const void * argp)
|   
|   Description   :  Validate buffer content to be a valid alpha value
|
|   Return Values :  TRUE  - field is valid
|                    FALSE - field is invalid
+--------------------------------------------------------------------------*/
static bool Check_Alpha_Field(FIELD * field, const void * argp)
{
  int width = ((alphaARG *)argp)->width;
  unsigned char *bp  = (unsigned char *)field_buffer(field,0);
  int  l = -1;
  unsigned char *s;

  while(*bp && *bp==' ') 
    bp++;
  if (*bp)
    {
      s = bp;
      while(*bp && isalpha(*bp)) 
	bp++;
      l = (int)(bp-s);
      while(*bp && *bp==' ') 
	bp++;
    }
  return ((*bp || (l < width)) ? FALSE : TRUE);
}

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  static bool Check_Alpha_Character(
|                                      int c,
|                                      const void * argp)
|   
|   Description   :  Check a character for the alpha type.
|
|   Return Values :  TRUE  - character is valid
|                    FALSE - character is invalid
+--------------------------------------------------------------------------*/
static bool Check_Alpha_Character(int c, const void * argp)
{
  return (isalpha(c) ? TRUE : FALSE);
}

static FIELDTYPE const typeALPHA = {
  _HAS_ARGS | _RESIDENT,
  1,
  (FIELDTYPE *)0,
  (FIELDTYPE *)0,
  Make_Alpha_Type,
  Copy_Alpha_Type,
  Free_Alpha_Type,
  Check_Alpha_Field,
  Check_Alpha_Character,
  NULL,
  NULL
};

FIELDTYPE const * TYPE_ALPHA = &typeALPHA;

/* fty_alpha.c ends here */
