
BEGIN		{
			print  "/* This file was generated by MKnames.awk */" > "boolnames"
			print  ""				> "boolnames"
			print  "#include \"curses.priv.h\""	> "boolnames"
			print  ""				> "boolnames"
			print  "#define IT char *"		> "boolnames"
			print  ""				> "boolnames"
			print  "#if BROKEN_LINKER"		> "boolnames"
			print  "#include <term.h>"		> "boolnames"
			print  "#define DCL(it) static const IT data##it[]" > "boolnames"
			print  "#else"				> "boolnames"
			print  "#define DCL(it) IT it[]"	> "boolnames"
			print  "#endif"				> "boolnames"
			print  ""				> "boolnames"
			print  "/*"				> "boolnames"
			print  " *	names.c - Arrays of capability names and codes"  > "boolnames"
			print  " *"				> "boolnames"
			print  " */"				> "boolnames"
			print  ""				> "boolnames"
			print  "DCL(boolnames)  = {"		> "boolnames"
			print  "DCL(boolfnames) = {"		> "boolfnames"
			print  "DCL(boolcodes)  = {"		> "boolcodes"
			print  "DCL(numnames)   = {"		> "numnames"
			print  "DCL(numfnames)  = {"		> "numfnames"
			print  "DCL(numcodes)   = {"		> "numcodes"
			print  "DCL(strnames)   = {"		> "strnames"
			print  "DCL(strfnames)  = {"		> "strfnames"
			print  "DCL(strcodes)   = {"		> "strcodes"
		}

$3 == "bool"	{
			printf "\t\t%s,\n", $2 > "boolnames"
			printf "\t\t\"%s\",\n", $1 > "boolfnames"
			printf "\t\t%s,\n", $4 > "boolcodes"
		}

$3 == "num"	{
			printf "\t\t%s,\n", $2 > "numnames"
			printf "\t\t\"%s\",\n", $1 > "numfnames"
			printf "\t\t%s,\n", $4 > "numcodes"
		}

$3 == "str"	{
			printf "\t\t%s,\n", $2 > "strnames"
			printf "\t\t\"%s\",\n", $1 > "strfnames"
			printf "\t\t%s,\n", $4 > "strcodes"
		}

END		{
			print  "\t\t(char *)0," > "boolnames"
			print  "};" > "boolnames"
			print  "" > "boolnames"
			print  "\t\t(char *)0," > "boolfnames"
			print  "};" > "boolfnames"
			print  "" > "boolfnames"
			print  "\t\t(char *)0," > "boolcodes"
			print  "};" > "boolcodes"
			print  "" > "boolcodes"
			print  "\t\t(char *)0," > "numnames"
			print  "};" > "numnames"
			print  "" > "numnames"
			print  "\t\t(char *)0," > "numfnames"
			print  "};" > "numfnames"
			print  "" > "numfnames"
			print  "\t\t(char *)0," > "numcodes"
			print  "};" > "numcodes"
			print  "" > "numcodes"
			print  "\t\t(char *)0," > "strnames"
			print  "};" > "strnames"
			print  "" > "strnames"
			print  "\t\t(char *)0," > "strfnames"
			print  "};" > "strfnames"
			print  "" > "strfnames"
			print  "\t\t(char *)0," > "strcodes"
			print  "};"				> "strcodes"
			print  ""				> "strcodes"
			print  "#if BROKEN_LINKER"		> "strcodes"
			print  "#define FIX(it) IT *_nc_##it(void) { return data##it; }" > "strcodes"
			print  "FIX(boolnames)"			> "strcodes"
			print  "FIX(boolfnames)"		> "strcodes"
			print  "FIX(boolcodes)"			> "strcodes"
			print  "FIX(numnames)"			> "strcodes"
			print  "FIX(numfnames)"			> "strcodes"
			print  "FIX(numcodes)"			> "strcodes"
			print  "FIX(strnames)"			> "strcodes"
			print  "FIX(strfnames)"			> "strcodes"
			print  "FIX(strcodes)"			> "strcodes"
			print  "#endif /* BROKEN_LINKER */"	> "strcodes"
		}
