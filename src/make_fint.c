#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "config.h"

typedef struct
{
  int naline;
  char *fname;
  char *aline[99];
  char *text;
}
Docu;

Docu cdoc[9999], fdoc[9999];
int   ncdoc = 0, nfdoc = 0;

int doccmp(const void *s1, const void *s2)
{
  Docu *x = (Docu *) s1;
  Docu *y = (Docu *) s2;

  return (strcmp(x->fname, y->fname));
}

int strcompare(const char **s1, const char **s2)
{
  printf(">%s< >%s<\n", *s1, *s2);
  return (strcmp(*s1, *s2));
}

void doctotex(FILE *fp, Docu *doc, int ndoc)
{
  int i, k;

  for ( i = 0; i < ndoc; i++ )
    {
      fprintf(fp, "\\section*{\\tt \\htmlref{%s}{%s}}\n\n", doc[i].fname, doc[i].fname);
      fprintf(fp, "\\begin{verbatim}\n");
      for ( k = 0; k < doc[i].naline; k++ )
	fprintf(fp, "    %s\n", doc[i].aline[k]);
      fprintf(fp, "\\end{verbatim}\n");
      fprintf(fp, "\n%s.\n\n\n", doc[i].text);
    }
}

void doctotxt(FILE *fp, Docu *doc, int ndoc)
{
  int i, k;

  for ( i = 0; i < ndoc; i++ )
    {
      fprintf(fp, "%s\n\n", doc[i].fname);
      for ( k = 0; k < doc[i].naline; k++ )
	fprintf(fp, "    %s\n", doc[i].aline[k]);
      fprintf(fp, "\n  %s.\n\n", doc[i].text);
    }
}

void fortran_interface(char *fname, char *fnameinc, char *fnameint)
{
  FILE *fpin, *fpinc, *fpint;
  FILE *fp;
  char line[1024], *pline;
  char sname[128], *parname, *comment;
  char xname[128], xdes[128];
  int parvalue;
  int functype;
  int i, ii;
  int lineno = 0;
  size_t linelen, len;
  enum              {ISVOID, ISSTRING,        ISSTRINGP,       ISINT,     ISINTP,     ISREAL, ISDOUBLE, ISDOUBLEP, ISCONSTSTRING};
  char *f77name[] = {"",     "CHARACTER*(*)", "CHARACTER*(*)", "INTEGER", "INTEGER", "REAL", "DOUBLEPRECISION", "DOUBLEPRECISION", "CHARACTER*80"};
  char *cfint[] =   {"",     "STRING",        "PSTRING",       "INT",     "PINT",    "FLOAT", "DOUBLE", "PDOUBLE",  "STRING"};
  char *cname[] =   {"void", "char *",        "char *",        "int",     "int *",   "float", "double", "double *", "char *"};
  char funcname[128];
  char *funcargname[20];
  int  funcargtype[20];
  int  funcargc;
  char *strsort[99999];
  char timestr[30];
  time_t date_and_time_in_sec;
  struct tm *date_and_time;

  date_and_time_in_sec = time(NULL);
  timestr[0] = 0;

  if ( date_and_time_in_sec != -1 )
    {
      date_and_time = localtime(&date_and_time_in_sec);
      (void) strftime(timestr, sizeof(timestr), "%B %Y", date_and_time);
    }

  /*
  for ( i = 0; i < 8; i++ ) strsort[i] = cname[i];
  for ( i = 0; i < 8; i++ ) printf("%d %d >%s<\n", i, &strsort[i], strsort[i]);
  qsort(strsort, 8, sizeof(char *), strcompare);
  for ( i = 0; i < 8; i++ ) printf("%d %d >%s<\n", i, &strsort[i], strsort[i]);

  return;
  */

  fpin = fopen(fname, "r");
  if ( fpin == NULL ) { perror(fname); return; }

  fpinc = fopen(fnameinc, "w");
  if ( fpinc == NULL ) { perror(fnameinc); return; }

  fpint = fopen(fnameint, "w");
  if ( fpint == NULL ) { perror(fnameint); return; }

  /* fortran include */

  fprintf(fpinc, "! This file was automatically created, don't edit!\n");
  fprintf(fpinc, "!\n");
  fprintf(fpinc, "! Fortran interface for CDI library version %s\n", VERSION);
  fprintf(fpinc, "!\n");
  fprintf(fpinc, "! Author:\n");
  fprintf(fpinc, "! -------\n");
  fprintf(fpinc, "! Uwe Schulzweida, MPI-MET, Hamburg,   %s\n", timestr);
  fprintf(fpinc, "!\n\n");

  /* fortran interface */

  fprintf(fpint, "/* Automatically created with make_cfint.c, don't edit! */\n");
  fprintf(fpint, "\n");
  fprintf(fpint, "#if ! defined (_CDI_H)\n");
  fprintf(fpint, "#  include \"cdi.h\"\n");
  fprintf(fpint, "#endif\n");
  fprintf(fpint, "\n");
  fprintf(fpint, "#if defined (HAVE_CONFIG_H)\n");
  fprintf(fpint, "#  include \"config.h\"\n");
  fprintf(fpint, "#endif\n");
  fprintf(fpint, "\n");
  fprintf(fpint, "#if defined (HAVE_CF_INTERFACE)\n");
  fprintf(fpint, "\n");
  fprintf(fpint, "#if ! defined (__CFORTRAN_LOADED)\n");
  fprintf(fpint, "#  include \"cfortran.h\"\n");
  fprintf(fpint, "#endif\n");
  fprintf(fpint, "\n");

  while ( fgets(line, 1023, fpin) )
    {
      lineno++;
      linelen = strlen(line);
      line[linelen-1] = 0;
      linelen -= 1;
      functype = ISVOID;
      funcargc = 0;
      if ( memcmp(line, "int", 3)    == 0 || 
	   memcmp(line, "void", 4)   == 0 ||
	   memcmp(line, "char", 4)   == 0 ||
	   memcmp(line, "const", 5)  == 0 ||
	   memcmp(line, "double", 6) == 0 )
	{
	  /* printf("%s\n", line); */
	  ii = 0;
	  for ( i = 0; i < linelen; i++ )
	    {
	      if ( isspace((int) line[i]) ) continue;
	      if ( line[i] == '\n' ) continue;
	      line[ii++] = line[i];
	    }
	  line[ii] = 0;
	  /* printf("%s\n", line); */
	  pline = line;
	  if ( memcmp(line, "int", 3) == 0 )
	    {
	      functype = ISINT;
	      pline += 3;
	    }
	  else if ( memcmp(line, "void", 4) == 0 )
	    {
	      functype = ISVOID;
	      pline += 4;
	    }
	  else if ( memcmp(line, "double*", 7) == 0 )
	    {
	      printf("skip: line %3d double *\n", lineno);
	      continue;
	    }
	  else if ( memcmp(line, "double", 6) == 0 )
	    {
	      functype = ISDOUBLE;
	      pline += 6;
	    }
	  else if ( memcmp(line, "char*", 5) == 0 )
	    {
	      functype = ISCONSTSTRING;
	      pline += 5;
	    }
	  else if ( memcmp(line, "constchar*", 10) == 0 )
	    {
	      functype = ISCONSTSTRING;
	      pline += 10;
	    }
	  else
	    {
	      printf("%s not implemented\n", line);
	      continue;
	    }

	  len = 0;
	  while ( isalnum((int) pline[len]) ) len++;
	  memcpy(funcname, pline, len);
	  funcname[len] = 0;
	  /* printf("%s\n", funcname);*/
	  pline += len;
	  if ( *pline != '(' )
	    {
	      printf("%s\n>(< not found!", line);
	      return;
	    }
	  pline++;

	  linelen = strlen(pline);
	  funcargname[funcargc] = pline;
	  for ( i = 0; i < linelen; i++ )
	    {
	      if ( pline[i] == ',' )
		{
		  pline[i] = 0;
		  funcargc++;
		  funcargname[funcargc] = &pline[i+1];
		}
	      if ( pline[i] == ')' )
		{
		  pline[i] = 0;
		  funcargc++;
		  break;
		}
	    }

	  /*  printf("funcargc = %d\n", funcargc);*/

	  for ( i = 0; i < funcargc; i++ )
	    {
	      pline = funcargname[i];
	      if ( memcmp(pline, "const", 5) == 0 &&
		   memcmp(pline, "constchar*", 10) != 0 ) pline += 5;

	      if ( memcmp(pline, "void", 4) == 0 )
		{
		  pline += 4;
		  funcargtype[i] = ISVOID;
		  funcargname[i] = pline;
		}
	      else if ( memcmp(pline, "constchar*", 10) == 0 )
		{
		  pline += 10;
		  funcargtype[i] = ISSTRING;
		  funcargname[i] = pline;
		}
	      else if ( memcmp(pline, "char*", 5) == 0 )
		{
		  pline += 5;
		  funcargtype[i] = ISSTRINGP;
		  funcargname[i] = pline;
		}
	      else if ( memcmp(pline, "int*", 4) == 0 )
		{
		  pline += 4;
		  funcargtype[i] = ISINTP;
		  funcargname[i] = pline;
		}
	      else if ( memcmp(pline, "int", 3) == 0 )
		{
		  pline += 3;
		  funcargtype[i] = ISINT;
		  funcargname[i] = pline;
		}
	      else if ( memcmp(pline, "double*", 7) == 0 )
		{
		  pline += 7;
		  funcargtype[i] = ISDOUBLEP;
		  funcargname[i] = pline;
		}
	      else if ( memcmp(pline, "double", 6) == 0 )
		{
		  pline += 6;
		  funcargtype[i] = ISDOUBLE;
		  funcargname[i] = pline;
		}
	      else 
		{
		  printf("%s not implemented\n", funcargname[i]);
		  break;
		}
	    }
	  if ( i != funcargc ) continue;

	  strcpy(sname, funcname);
	  len = strlen(sname);
	  /*
	  ii = 0;
	  for ( i = 0; i < len; i++ )
	    {
	      if ( isupper((int) sname[i]) ) break;
	      if ( islower((int) sname[i]) ) sname[i] = toupper((int) sname[i]);
	    }
	  sname[len+1] = 0;
	  for ( ii = len; ii > i; ii-- )
	    {
	      sname[ii] = sname[ii-1];
	    }
	  sname[i] = '_';
	  len++;
	  */
	  
	  /* fortran include */

	  if ( functype == ISVOID )
	    fprintf(fpinc, "!     %-16s", "");
	  else
	    fprintf(fpinc, "      %-16s", f77name[functype]);

          fprintf(fpinc, "%s", sname);
	  fprintf(fpinc, "\n");
	  if ( (funcargc == 1 && funcargtype[0] == ISVOID) ) funcargc = 0;
	  for ( i = 0; i < funcargc; i++ )
	    {
	      if ( i == 0 )
		fprintf(fpinc, "!%36s(", "");
	      else
		fprintf(fpinc, ",\n!%36s ", "");
	      fprintf(fpinc, "%-16s%s", f77name[funcargtype[i]], funcargname[i]);
	    }
	  if ( funcargc )
	    fprintf(fpinc, ")\n");
	  fprintf(fpinc, "      %-16s%s\n\n", "EXTERNAL", sname);

	  /* fortran interface */

	  if ( functype == ISVOID )
	    fprintf(fpint, "FCALLSCSUB");
	  else
	    fprintf(fpint, "FCALLSCFUN");
	  fprintf(fpint, "%d ", funcargc);
	  fprintf(fpint, "(");
	  if ( functype != ISVOID )
	    fprintf(fpint, "%s, ", cfint[functype]);
	  fprintf(fpint, "%s, ", funcname);
	  for ( i = 0; i < len; i++ ) sname[i] = toupper((int) sname[i]);
	  fprintf(fpint, "%s, ", sname);
	  for ( i = 0; i < len; i++ ) sname[i] = tolower((int) sname[i]);
	  fprintf(fpint, "%s", sname);
	  for ( i = 0; i < funcargc; i++ )
	    {
	      fprintf(fpint, ", %s", cfint[funcargtype[i]]);
	    } 
	  fprintf(fpint, ")\n");


	  if ( strcmp(funcname, xname) == 0 ) 
	    {
	      char xline[128];
	      char farg[128];
	      int nch, ncha = 0, nchn;
	      
	      /* C Quick Guide */

	      cdoc[ncdoc].naline = 0;
	      cdoc[ncdoc].text   = NULL;
	      cdoc[ncdoc].fname  = strdup(funcname);

	      nch = sprintf(xline, "%s %s (", cname[functype], xname);

	      if ( (funcargc == 1 && funcargtype[0] == ISVOID) ) funcargc = 0;

	      for ( i = 0; i < funcargc; i++ )
		{
		  if ( i ) strcat(xline, ", ");

		  nchn  = sprintf(farg, "%s%s", cname[funcargtype[i]], funcargname[i]);
		  if ( (strlen(xline)+nchn) > 80 )
		    {
		      cdoc[ncdoc].aline[cdoc[ncdoc].naline++] = strdup(xline);
		      sprintf(xline, "%*s", nch, "");
		    }
		  strcat(xline, farg);
		}
	      if ( funcargc ) strcat(xline, ");");
	      cdoc[ncdoc].aline[cdoc[ncdoc].naline++] = strdup(xline);
	      cdoc[ncdoc].text  = strdup(xdes);

	      ncdoc++;

	      /* Fortran Quick Guide */

	      ncha = 0;
	      fdoc[nfdoc].naline = 0;
	      fdoc[nfdoc].text   = NULL;
	      fdoc[nfdoc].fname  = strdup(funcname);

	      if ( functype == ISVOID )
		nch = sprintf(xline, "SUBROUTINE %s (", xname);
	      else
		nch = sprintf(xline, "%s FUNCTION %s (", f77name[functype], xname);

	      if ( (funcargc == 1 && funcargtype[0] == ISVOID) ) funcargc = 0;

	      for ( i = 0; i < funcargc; i++ )
		{
		  if ( i ) strcat(xline, ", ");

		  nchn  = sprintf(farg, "%s %s", f77name[funcargtype[i]], funcargname[i]);
		  if ( (strlen(xline)+nchn) > 80 )
		    {
		      fdoc[nfdoc].aline[fdoc[nfdoc].naline++] = strdup(xline);
		      sprintf(xline, "%*s", nch, "");
		    }
		  strcat(xline, farg);
		}
	      if ( funcargc ) strcat(xline, ")");
	      fdoc[nfdoc].aline[fdoc[nfdoc].naline++] = strdup(xline);
	      fdoc[nfdoc].text  = strdup(xdes);

	      nfdoc++;
	    }
	}
      else if ( memcmp(line, "#define", 7) == 0 )
	{
	  pline = line;
	  pline += 7;
	  while ( isspace((int) *pline) ) pline++;
	  parname = pline;
	  len = strlen(pline);
	  for ( i = 0; i < len; i++ )
	    {
	      if ( isspace((int) pline[i]) ) break;
	    }
	  if ( i == len ) continue;
	  parname[i] = 0;
	  pline += i+1;
	  while ( isspace((int) *pline) ) pline++;
	  if ( ! (isdigit((int) *pline) || *pline == '-') ) continue;
	  parvalue = atoi(pline);

	  /* fortran include */
	  
	  fprintf(fpinc, "      INTEGER    %-22s\n", parname);
	  fprintf(fpinc, "      PARAMETER (%-22s = %2d)\n", parname, parvalue);
	}
      else if ( memcmp(line, "/*", 2) == 0 )
	{
	  int dpos = 0;
	  pline = line;
	  pline += 2;
	  while ( isspace((int) *pline) ) pline++;
	  comment = pline;
	  len = strlen(pline);
	  if ( len == 0 ) continue;
	  for ( i = len-1; i > 0; i-- )
	    if ( pline[i-1] == '*' && pline[i] == '/' ) break;

	  if ( i == 0 ) continue;
	  pline[i-1] = 0;

	  len -= 2;
	  for ( i = len-1; i > 0; i-- )
	    if ( pline[i] != ' ' ) break;
	  pline[i+1] = 0;

	  for ( i = 0; i < len ; i++ )
	    if ( pline[i] == ':' ) dpos = i;

	  xname[0] = 0;
	  xdes[0]  = 0;
	  if ( dpos )
	    {
	      for ( i = dpos-1; i > 0; i-- )
		if ( pline[i] != ' ' ) break;
	      pline[i+1] = 0;
	      strcpy(xname, pline);
	      pline += dpos+1;
	      while ( isspace((int) *pline) ) pline++;
	      strcpy(xdes, pline);
	    }
	  else
	    {
	      /* fortran include */
	  
	      fprintf(fpinc, "!\n!  %s\n!\n", comment);

	      /* fortran interface */

	      fprintf(fpint, "\n/*  %s  */\n\n", comment);
	    }
	}
      else
	{
	  if ( linelen > 1 )
	    printf("skip: line %3d  size %3d  %s\n", lineno, linelen, line);
	}
    }

  fprintf(fpint, "\n");
  fprintf(fpint, "#endif\n");

  fclose(fpin);
  fclose(fpinc);
  fclose(fpint);

  qsort(cdoc, ncdoc, sizeof(Docu), doccmp);
  qsort(fdoc, nfdoc, sizeof(Docu), doccmp);

  fp = fopen("../doc/c_quick_ref.txt", "w");
  if ( fp )
    {
      fprintf(fp, "C Quick Reference\n");
      fprintf(fp, "-----------------\n\n");

      doctotxt(fp, cdoc, ncdoc);
      fclose(fp);
    }

  fp = fopen("../doc/f_quick_ref.txt", "w");
  if ( fp )
    {
      fprintf(fp, "Fortran Quick Reference\n");
      fprintf(fp, "-----------------------\n\n");

      doctotxt(fp, fdoc, nfdoc);
      fclose(fp);
    }

  fp = fopen("../doc/tex/c_quick_ref.tex", "w");
  if ( fp )
    {
      fprintf(fp, "\\chapter*{Quick Reference}\n");
      fprintf(fp, "\\addcontentsline{toc}{chapter}{Quick Reference}\n");
      fprintf(fp, "\n");
      fprintf(fp, "This appendix provide a brief listing of the C language bindings of the\n");
      fprintf(fp, "CDI library routines:\n");
      fprintf(fp, "\n");

      doctotex(fp, cdoc, ncdoc);
      fclose(fp);
    }

  fp = fopen("../doc/tex/f_quick_ref.tex", "w");
  if ( fp )
    {
      fprintf(fp, "\\chapter*{Quick Reference}\n");
      fprintf(fp, "\\addcontentsline{toc}{chapter}{Quick Reference}\n");
      fprintf(fp, "\n");
      fprintf(fp, "This appendix provide a brief listing of the Fortran language bindings of the\n");
      fprintf(fp, "CDI library routines:\n");
      fprintf(fp, "\n");

      doctotex(fp, fdoc, nfdoc);
      fclose(fp);
    }
}

int main(int argc, char *argv[])
{
  char *fname;
  char *cp;
  char fnameinc[128], fnameint[128];
  size_t len;

  if ( argc != 2 )
    {
      printf("Usage:  %s  includefile\n", argv[0]);
      return (1);
    }

  fname = argv[1];

  cp = strrchr(fname, '.');
  if ( cp == NULL ) len = strlen(fname);
  else              len = cp - fname;
  
  memcpy(fnameinc, fname, len);
  memcpy(fnameint, fname, len);

  strcpy(fnameinc+len, ".inc");
  strcpy(fnameint+len, "Fortran.c");
  
  fortran_interface(fname, fnameinc, fnameint);

  return (0);
}
