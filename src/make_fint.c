#define _XOPEN_SOURCE 700
#include <ctype.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>


//#include "config.h"
#define VERSION "1.5.6.1"
typedef struct
{
  size_t naline;
  char *fname;
  char *aline[99];
  char *text;
}
Docu;

static Docu cdoc[9999], fdoc[9999];
static size_t ncdoc = 0, nfdoc = 0;
static int debug = 0;

static int doccmp(const void *s1, const void *s2)
{
  Docu *x = (Docu *) s1;
  Docu *y = (Docu *) s2;

  return (strcmp(x->fname, y->fname));
}

static void doctotex(FILE *fp, Docu *doc, size_t ndoc)
{
  size_t i, k;

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

static void doctotxt(FILE *fp, Docu *doc, size_t ndoc)
{
  size_t i, k;

  for ( i = 0; i < ndoc; i++ )
    {
      fprintf(fp, "%s\n\n", doc[i].fname);
      for ( k = 0; k < doc[i].naline; k++ )
	fprintf(fp, "    %s\n", doc[i].aline[k]);
      fprintf(fp, "\n  %s.\n\n", doc[i].text);
    }
}

enum cftype {ISVOID, ISCONSTSTRING, ISINT, ISREAL, ISDOUBLE, ISMPI_COMM,
             ISXT_IDXLIST, ISCHOICE, ISINTP, ISINTV, ISINTVV, ISREALP,
             ISDOUBLEP, ISSTRING, ISSTRINGP,
             NUM_KNOWN_ARG_TYPES};

enum conversionType { CONV_ARG, CONV_RET };


typedef int (*cfConversionEmitter)(FILE *outfp, const char *argName,
                                   size_t argNameLen, enum conversionType part);

static int cfMPICommConvert(FILE *outfp, const char *argName,
                            size_t argNameLen, enum conversionType part);

static int cfXtIdxlistConvert(FILE *outfp, const char *argName,
                            size_t argNameLen, enum conversionType part);

struct symbol {
  const char *f77name, *cfint, *cname, *parseRE;
  size_t nameMatch;
  int needsExtraWrapper;
  cfConversionEmitter convert;
  const char *convcname;
  regex_t preg;
};

/* C symbol names */
#define SYMRE "([A-Za-z_][A-Za-z_0-9]*)"
/* white-space */
#define WS "[[:blank:]\n]"
#define NWS "[^[:blank:]\n]"
static struct symbol funArgSym[]
  = { { "",                "",        "void",
        "^"WS"*void"WS"*)", 0, 0 },
      { "CHARACTER*80",    "STRING",  "char *",
        "^"WS"*const"WS"+char"WS"+\\*"SYMRE WS"*\\(", 1, 0 },
      { "INTEGER",         "INT",     "int",
        "^"WS"*(const"WS"+)?int("WS"+"SYMRE")?"WS"*[,\\)]", 3, 0 },
      { "REAL",            "FLOAT",   "float",
        "^"WS"*(const"WS"+)?float"WS"+"SYMRE"?"WS"*[,\\)]", 2, 0 },
      { "DOUBLEPRECISION", "DOUBLE",  "double",
        "^"WS"*(const"WS"+)?double"WS"+"SYMRE"?"WS"*[,\\)]", 2, 0 },
      { "INTEGER",         "INT", "MPI_Comm",
        "^"WS"*MPI_Comm"WS"+"SYMRE"?"WS"*[,\\)]", 1, 1,
        cfMPICommConvert, "int" },
      { "TYPE(XT_IDXLIST)", "PVOID", "Xt_idxlist",
        "^"WS"*Xt_idxlist"WS"+"SYMRE"?"WS"*[,\\)]", 1, 1,
        cfXtIdxlistConvert, "void *" },
      { "CHOICE", "PVOID", "const void *",
        "^"WS"*const"WS"+void"WS"*\\*"WS"*"SYMRE"?"WS"*[,\\)]", 1, 0 },
      { "INTEGER",         "PINT",    "int *",
        "^"WS"*(const"WS"+)?int"WS"+\\*"SYMRE"?"WS"*[,\\)]", 2, 0 },
      { "INTEGER",         "INTV",    "int[]",
        "^"WS"*(const"WS"+)?int("WS"+"SYMRE")?"WS"*\\[[^]]*\\]"
        WS"*[,\\)]", 3, 0 },
      { "INTEGER",         "INTVV",    "int[][]",
        "^"WS"*(const"WS"+)?int("WS"+"SYMRE")?"WS"*\\[[^]]*\\]"
        WS"*\\[[^]]*\\]"WS"*[,\\)]", 3, 0 },
      { "REAL",            "PFLOAT",  "float *",
        "^"WS"*(const"WS"+)?float"WS"+\\*"SYMRE"?"WS"*[,\\)]", 2, 0 },
      { "DOUBLEPRECISION", "PDOUBLE", "double *",
        "^"WS"*(const"WS"+)?double"WS"+\\*"SYMRE"?"WS"*[,\\)]", 2, 0 },
      { "CHARACTER*(*)",   "STRING",  "char *",
        "^"WS"*const"WS"+char"WS"+\\*"WS"*"SYMRE"?"WS"*[,\\)]", 1, 0 },
      { "CHARACTER*(*)",   "PSTRING", "char *",
        "^"WS"*char"WS"+\\*"SYMRE"?"WS"*[,\\)]", 1, 0 },
};

static struct symbol funRet[] = {
  { "",                "",        "void",
    "void"WS"+"SYMRE WS"*\\(", 1, 0 },
  { "CHARACTER",       "STRING",  "(const) char *",
    "(const"WS"+)?char"WS"+\\*"WS"*"SYMRE WS"*\\(", 2, 0 },
  { "INTEGER",         "INT",     "int",
    "(const"WS"+)?int"WS"+"SYMRE WS"*\\(", 2, 0 },
  { "REAL",            "FLOAT",   "float",
    "(const"WS"+)?float"WS"+"SYMRE WS"*\\(", 2, 0 },
  { "DOUBLEPRECISION", "DOUBLE",  "double",
    "(const"WS"+)?double"WS"+"SYMRE WS"*\\(", 2, 0 },
  { "INTEGER",         "INT",     "MPI_Comm",
    "MPI_Comm"WS"+"SYMRE WS"*\\(", 1, 0, cfMPICommConvert, "int" },
};

enum { NUM_RET_TYPES = sizeof (funRet) / sizeof (funRet[0]) };
enum decl { UNKNOWN_DECL, FUNC_DECL, PARAM_DECL };

enum {
  MAX_FUNC_ARGS = 200,
  MAX_FUNC_NAME_LEN = 127,
};

static inline size_t
compress_whitespace(size_t len, char str[]);

static size_t
symRegexCompile(size_t numSyms, struct symbol symList[],
                char **line, size_t *lineBufSize);


static void fortran_interface(char *fname, char *fnameinc, char *fnameint)
{
  FILE *fpin, *fpinc, *fpint;
  FILE *fp;
  char *line = NULL, *pline;
  size_t lineBufSize = 0;
  char sname[128], *parname;
  char xname[128], xdes[128];
  xname[0] = 0;
  int parvalue;
  enum cftype functype;
  int lineno = 0;
  size_t len;

  char funcname[MAX_FUNC_NAME_LEN];
  regmatch_t funcargfull[MAX_FUNC_ARGS];
  regmatch_t funcargname[MAX_FUNC_ARGS];
  int  funcargtype[MAX_FUNC_ARGS];
  /* char *strsort[99999]; */
  char timestr[30];
  time_t date_and_time_in_sec;
  struct tm *date_and_time;
  regmatch_t *reMatch = NULL;
  size_t maxMatch = 0;

  date_and_time_in_sec = time(NULL);
  timestr[0] = 0;

  if ( date_and_time_in_sec != -1 )
    {
      date_and_time = localtime(&date_and_time_in_sec);
      (void) strftime(timestr, sizeof(timestr), "%B %Y", date_and_time);
    }

  fpin = fopen(fname, "r");
  if ( fpin == NULL ) { perror(fname); return; }

  fpinc = fopen(fnameinc, "w");
  if ( fpinc == NULL ) { perror(fnameinc); return; }

  fpint = fopen(fnameint, "w");
  if ( fpint == NULL ) { perror(fnameint); return; }

  /* complete symbol table data */
  {
    maxMatch = symRegexCompile(NUM_KNOWN_ARG_TYPES, funArgSym,
                             &line, &lineBufSize);
    size_t maxFunMatch = symRegexCompile(NUM_RET_TYPES, funRet,
                                         &line, &lineBufSize);
    if (maxFunMatch > maxMatch)
      maxMatch = maxFunMatch;
  }
  ++maxMatch;
  reMatch = malloc((size_t)maxMatch * sizeof (reMatch[0]));
  /* compile comment regular expression */
  regex_t commentRE;
  {
    int errcode;
    char commentREString[] = "^"WS"*/\\*"WS"*(.*"NWS")"WS"*\\*/";
    if ((errcode = regcomp(&commentRE, commentREString, REG_EXTENDED)))
      {
        line = realloc(line, lineBufSize = 1024);
        if (line)
          {
            regerror(errcode, &commentRE, line, lineBufSize);
            fprintf(stderr, "Error compiling regular expression: %s: %s\n",
                    commentREString, line);
            exit(EXIT_FAILURE);
          }
      }
  }
  /* compile documentation comment regular expression */
  regex_t docCommentRE;
  {
    int errcode;
    char docCommentREString[] = "^"WS"*/\\*"WS"*"SYMRE":"
      WS"*("NWS".*"NWS")"WS"*\\*/";
    if ((errcode = regcomp(&docCommentRE, docCommentREString, REG_EXTENDED)))
      {
        line = realloc(line, lineBufSize = 1024);
        if (line)
          {
            regerror(errcode, &commentRE, line, lineBufSize);
            fprintf(stderr, "Error compiling regular expression: %s: %s\n",
                    docCommentREString, line);
            exit(EXIT_FAILURE);
          }
      }
  }
  /* fortran include */

  fprintf(fpinc, "! This file was automatically generated, don't edit!\n");
  fprintf(fpinc, "!\n");
  fprintf(fpinc, "! Fortran interface for CDI library version %s\n", VERSION);
  fprintf(fpinc, "!\n");
  fprintf(fpinc, "! Author:\n");
  fprintf(fpinc, "! -------\n");
  fprintf(fpinc, "! Uwe Schulzweida, MPI-MET, Hamburg,   %s\n", timestr);
  fprintf(fpinc, "!\n\n");

  /* fortran interface */

  fprintf(fpint, "/* Automatically generated by make_fint.c, don't edit! */\n");
  fprintf(fpint, "\n");
  fprintf(fpint, "#if defined (HAVE_CONFIG_H)\n");
  fprintf(fpint, "#  include \"config.h\"\n");
  fprintf(fpint, "#endif\n");
  fprintf(fpint, "\n");
  fputs("#if USE_MPI\n"
        "#  include <mpi.h>\n"
        "#  include <yaxt.h>\n"
        "#else\n"
        "#define MPI_Comm int\n"
        "#define MPI_Comm_f2c(c) (c)\n"
        "#define MPI_Comm_c2f(c) (c)\n"
        "#endif\n"
        "\n"
        "#if ! defined (_CDI_H)\n"
        "#  include \"cdi.h\"\n"
        "#endif\n"
        "\n", fpint);
  fprintf(fpint, "#if defined (HAVE_CF_INTERFACE)\n");
  fprintf(fpint, "\n");
  fprintf(fpint, "#if ! defined (__CFORTRAN_LOADED)\n");
  fprintf(fpint, "#  include \"cfortran.h\"\n");
  fprintf(fpint, "#endif\n");
  fprintf(fpint, "\n");

  ssize_t lineLen;
  while ((lineLen = getline(&line, &lineBufSize, fpin)) >= 0)
    {
      lineno++;
      functype = ISVOID;
      size_t funcargc = 0;
      pline = line;
      int needsExtraWrapper = 0;
      enum decl declType = UNKNOWN_DECL;
      do {
        for (int retType = 0; retType < NUM_RET_TYPES; ++retType)
          if (!regexec(&funRet[retType].preg, pline, maxMatch, reMatch, 0))
            {
              functype = retType;
              declType = FUNC_DECL;
              needsExtraWrapper
                = needsExtraWrapper || funRet[retType].needsExtraWrapper;
              break;
            }
        if (declType == UNKNOWN_DECL)
          break;
        regmatch_t *nameMatch = reMatch + funRet[functype].nameMatch;
        if (debug)
          printf("Found: %.*s\n",
                 nameMatch->rm_eo - nameMatch->rm_so,
                 pline + nameMatch->rm_so);
        ssize_t funNameLast = reMatch[0].rm_eo - 1;
        ssize_t nameLen = nameMatch->rm_eo - nameMatch->rm_so;
        if ( pline[funNameLast] != '(' )
          {
            printf("%s\n>(< not found!", line);
            return;
          }
        memcpy(funcname, pline + nameMatch->rm_so, (size_t)nameLen);
        funcname[nameLen] = 0;
        pline += reMatch[0].rm_eo;
      } while (0);
      if (declType == FUNC_DECL)
        {
	  funcargname[funcargc].rm_so = (regoff_t)(pline - line);
          {
            ssize_t i = 0;
            do {
              ssize_t restLen = lineLen - (ssize_t)(pline - line);
              for (; i < restLen; i++ )
                {
                  if ( pline[i] == ',' )
                    {
                      funcargc++;
                      funcargname[funcargc].rm_so
                        = (regoff_t)(pline - line + i + 1);
                    }
                  if ( pline[i] == ')' )
                    {
                      funcargc++;
                      funcargname[funcargc].rm_so
                        = (regoff_t)(pline - line + i + 1);
                      break;
                    }
                }
              if (i < restLen)
                break;
              char *lineExtension = NULL;
              size_t extSize = 0, plineOff = (size_t)(pline - line);
              ssize_t extLen;
              if ((extLen = getline(&lineExtension, &extSize, fpin)) <= 0)
                break;
              if ((size_t)(lineLen + extLen) >= lineBufSize)
                if (!(line = realloc(line, (size_t)(lineLen + extLen + 1))))
                  exit(EXIT_FAILURE);
              memcpy(line + lineLen, lineExtension, (size_t)extLen + 1);
              lineLen += extLen;
              pline = line + plineOff;
            } while (1);
          }

	  /*  printf("funcargc = %d\n", funcargc);*/
          {
            size_t i;
            for (i = 0; i < funcargc; ++i )
              {
                pline = line + funcargname[i].rm_so;
                int argtype;
                regoff_t argStart = (regoff_t)(pline - line);
                for (argtype = ISVOID;
                     argtype < NUM_KNOWN_ARG_TYPES;
                     ++argtype)
                  if (!regexec(&funArgSym[argtype].preg, pline, maxMatch,
                               reMatch, 0))
                    {
                      funcargtype[i] = argtype;
                      funcargfull[i].rm_so = reMatch[0].rm_so + argStart;
                      funcargfull[i].rm_eo = reMatch[0].rm_eo + argStart;
                      regmatch_t *nameMatch = reMatch + funArgSym[argtype].nameMatch;
                      funcargname[i].rm_so = nameMatch->rm_so + argStart;
                      funcargname[i].rm_eo = nameMatch->rm_eo + argStart;
                      needsExtraWrapper
                        = needsExtraWrapper || funArgSym[argtype].needsExtraWrapper;
                      break;
                    }
                if (argtype == NUM_KNOWN_ARG_TYPES)
                  {
                    printf("%s not implemented\n", line + funcargname[i].rm_so);
                    break;
                  }
              }
            if ( i != funcargc )
              {
                printf("problem parsing line: %s\n", line);
                continue;
              }
          }

	  strcpy(sname, funcname);
	  len = strlen(sname);

	  /* fortran include */

	  if ( functype == ISVOID )
	    fprintf(fpinc, "!     %-16s", "");
	  else
	    fprintf(fpinc, "      %-16s", funArgSym[functype].f77name);

          fprintf(fpinc, "%s", sname);
	  fprintf(fpinc, "\n");
	  if ( (funcargc == 1 && funcargtype[0] == ISVOID) ) funcargc = 0;
	  for (size_t i = 0; i < funcargc; i++ )
	    {
	      if ( i == 0 )
		fprintf(fpinc, "!%36s(", "");
	      else
		fprintf(fpinc, ",\n!%36s ", "");
	      fprintf(fpinc, "%-16s%.*s", funArgSym[funcargtype[i]].f77name,
                      (int)(funcargname[i].rm_eo - funcargname[i].rm_so),
                      line + funcargname[i].rm_so);
	    }
	  if ( funcargc )
	    fprintf(fpinc, ")\n");
	  fprintf(fpinc, "      %-16s%s\n\n", "EXTERNAL", sname);

	  /* fortran interface */
          const char *delegateName;
          char delegateNameBuf[MAX_FUNC_NAME_LEN + 7];
          /* emit wrapper for type conversions if needed */
          if (needsExtraWrapper)
            {
              strcpy(delegateNameBuf, funcname);
              strcat(delegateNameBuf, "_fwrap");
              delegateName = delegateNameBuf;
              fprintf(fpint, "static %s %s(",
                      (funRet[functype].convert
                       ?funRet[functype].convcname:funRet[functype].cname),
                      delegateName);
              for (size_t i = 0; i < funcargc; i++ )
                {
                  fprintf(fpint, "%s%s %.*s", (i > 0?", ":""),
                          (funArgSym[funcargtype[i]].convert
                           ?funArgSym[funcargtype[i]].convcname
                           :funArgSym[funcargtype[i]].cname),
                          (int)(funcargname[i].rm_eo - funcargname[i].rm_so),
                          line + funcargname[i].rm_so);
                }
              fputs(")\n{\n", fpint);
              if (functype != ISVOID)
                fprintf(fpint, "  %s v;\n"
                        "  v = %s(", funRet[functype].cname,
                        funcname);
              else
                fprintf(fpint, "  %s(", funcname);
              for (size_t i = 0; i < funcargc; i++ )
                {
                  if (funArgSym[funcargtype[i]].convert)
                    {
                      if (i > 0)
                        fputs(", ", fpint);
                      funArgSym[funcargtype[i]]
                        .convert(fpint,
                                 line + funcargname[i].rm_so,
                                 (size_t)(funcargname[i].rm_eo
                                          - funcargname[i].rm_so), CONV_ARG);
                    }
                  else
                    fprintf(fpint, "%s %.*s", (i > 0?", ":""),
                            (int)(funcargname[i].rm_eo - funcargname[i].rm_so),
                            line + funcargname[i].rm_so);
                }
              fputs(");\n", fpint);
              if (functype != ISVOID)
                {
                  fputs("  return ", fpint);
                  if (funRet[functype].convert)
                    funRet[functype].convert(fpint, "v", 1, CONV_RET);
                  else
                    fputc('v', fpint);
                  fputs(";\n", fpint);
                }
              fputs("}\n", fpint);
            }
          else
            delegateName = funcname;
	  if ( functype == ISVOID )
	    fprintf(fpint, "FCALLSCSUB");
	  else
	    fprintf(fpint, "FCALLSCFUN");
	  fprintf(fpint, "%zd ", funcargc);
	  fprintf(fpint, "(");
	  if ( functype != ISVOID )
	    fprintf(fpint, "%s, ", funRet[functype].cfint);
	  fprintf(fpint, "%s, ", delegateName);
	  for (size_t i = 0; i < len; i++ ) sname[i] = (char)toupper((int) sname[i]);
	  fprintf(fpint, "%s, ", sname);
	  for (size_t i = 0; i < len; i++ ) sname[i] = (char)tolower((int) sname[i]);
	  fprintf(fpint, "%s", sname);
	  for (size_t i = 0; i < funcargc; i++ )
	    {
	      fprintf(fpint, ", %s", funArgSym[funcargtype[i]].cfint);
	    }
	  fprintf(fpint, ")\n");


	  if ( strcmp(funcname, xname) == 0 )
	    {
	      char xline[128];
	      int nch;

	      /* C Quick Guide */

	      cdoc[ncdoc].naline = 0;
	      cdoc[ncdoc].text   = NULL;
	      cdoc[ncdoc].fname  = strdup(funcname);

	      nch = sprintf(xline, "%s %s (", funArgSym[functype].cname, xname);

	      if ( (funcargc == 1 && funcargtype[0] == ISVOID) ) funcargc = 0;

	      for (size_t i = 0; i < funcargc; i++ )
		{
		  if ( i ) strcat(xline, ", ");

                  /* extract full argument text from match */
                  char farg[128];
                  /* - 1 to omit closing paren ) or comma , */
                  int nchn = snprintf(farg, sizeof (farg), "%.*s",
                                      (int)(funcargfull[i].rm_eo
                                            - funcargfull[i].rm_so - 1),
                                      line + funcargfull[i].rm_so);
                  if (nchn < 0)
                    abort();
                  /* compress white-space */
                  nchn = (int)compress_whitespace((size_t)nchn, farg);
		  if ( (strlen(xline)+(size_t)nchn) > (size_t)80 )
		    {
                      if (i) xline[strlen(xline) - 1] = 0;
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

	      fdoc[nfdoc].naline = 0;
	      fdoc[nfdoc].text   = NULL;
	      fdoc[nfdoc].fname  = strdup(funcname);

	      if ( functype == ISVOID )
		nch = sprintf(xline, "SUBROUTINE %s (", xname);
	      else
		nch = sprintf(xline, "%s FUNCTION %s (", funArgSym[functype].f77name, xname);

	      if ( (funcargc == 1 && funcargtype[0] == ISVOID) ) funcargc = 0;

	      for (size_t i = 0; i < funcargc; i++ )
		{
		  if ( i ) strcat(xline, ", ");

                  char farg[128];
                  /* FIXME: optional empty argument name unhandled */
		  int nchn
                    = snprintf(farg, sizeof (farg), "%s %.*s",
                               funArgSym[funcargtype[i]].f77name,
                               (int)(funcargname[i].rm_eo
                                     - funcargname[i].rm_so),
                               line + funcargname[i].rm_so);
                  if (nchn < 0)
                    abort();
		  if ( (strlen(xline)+(size_t)nchn) > 80 )
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
          size_t i = 0;
	  for (; i < len; i++ )
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
	  fprintf(fpinc, "      INTEGER    %-22s\n"
                  "      PARAMETER (%-22s = %2d)\n", parname, parname,
                  parvalue);
	}
      else if (!regexec(&docCommentRE, line, maxMatch, reMatch, 0))
        {
          /* found documentation comment */
          size_t nameMatchLen = (size_t)(reMatch[1].rm_eo - reMatch[1].rm_so),
            docMatchLen = (size_t)(reMatch[2].rm_eo - reMatch[2].rm_so);
          memcpy(xname, line + reMatch[1].rm_so, nameMatchLen);
          xname[nameMatchLen] = 0;
          memcpy(xdes, line + reMatch[2].rm_so, docMatchLen);
          xdes[docMatchLen] = 0;
          printf("Found documentation for \"%s\": \"%s\"\n", xname,
                 xdes);
        }
      else if (!regexec(&commentRE, line, maxMatch, reMatch, 0))
	{
          size_t commentLen = (size_t)(reMatch[1].rm_eo - reMatch[1].rm_so);
          const char *comment = line + reMatch[1].rm_so;
          /* fortran include */
          fprintf(fpinc, "!\n!  %.*s\n!\n", (int)commentLen, comment);

          /* fortran interface */
          fprintf(fpint, "\n/*  %.*s  */\n\n", (int)commentLen, comment);
	}
      else
	{
	  if ( lineLen > 1 )
	    printf("skip: line %3d  size %3zd  %s%s", lineno, lineLen, line,
                   line[lineLen-1]=='\n'?"":"missing new-line\n");
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
  else              len = (size_t)(cp - fname);
  
  memcpy(fnameinc, fname, len);
  memcpy(fnameint, fname, len);

  strcpy(fnameinc+len, ".inc");
  strcpy(fnameint+len, "Fortran.c");
  
  fortran_interface(fname, fnameinc, fnameint);

  return (0);
}

static inline size_t
compress_whitespace(size_t len, char str[])
{
  size_t wpos = 0;
  size_t i = 0;
  /* skip leading white-space */
  while (i < len && (isblank(str[i]) || str[i] == '\n'))
    ++i;
  /* after the leading white-space the following is
   * an alternation of white- and non-white-space
   * characters, where sequences of the latter will
   * be compressed to a single space */
  while (i < len)
    {
      /* handle white-space */
      while (i < len && !(isblank(str[i]) || str[i] == '\n'))
        str[wpos++] = str[i++];
      /* skip non-white-space */
      size_t wscount = 0;
      while (i < len && (isblank(str[i]) || str[i] == '\n'))
        ++i, ++wscount;
      if (wscount)
        str[wpos++] = ' ';
    }
  str[wpos] = '\0';
  return wpos;
}

enum {
  REGEX_MAX_ERRSTRLEN = 1024,
};


static size_t
symRegexCompile(size_t numSyms, struct symbol symList[],
                char **line, size_t *lineBufSize)
{
  size_t maxMatch = 0;
  for (size_t sym = 0; sym < numSyms; ++sym)
    {
      int errcode;
      if ((errcode = regcomp(&symList[sym].preg, symList[sym].parseRE,
                             REG_EXTENDED)))
        {
          if (*lineBufSize < REGEX_MAX_ERRSTRLEN)
            line = realloc(line, *lineBufSize = REGEX_MAX_ERRSTRLEN);
          if (line)
            {
              regerror(errcode, &symList[sym].preg, *line, *lineBufSize);
              fprintf(stderr, "Error compiling regular expression: %s: %s\n",
                      symList[sym].parseRE, *line);
            }
          exit(EXIT_FAILURE);
        }
      if (symList[sym].nameMatch > maxMatch)
        maxMatch = symList[sym].nameMatch;
    }
  return maxMatch;
}

/* emit conversion code for MPI_Comm argument */
static int cfMPICommConvert(FILE *outfp, const char *argName,
                            size_t argNameLen, enum conversionType part)
{
  int retval = 0;
  switch (part)
    {
    case CONV_ARG:
      retval
        = fprintf(outfp, "MPI_Comm_f2c(%.*s)", (int)argNameLen, argName);
      break;
    case CONV_RET:
      retval = fprintf(outfp, "MPI_Comm_c2f(%.*s)", (int)argNameLen, argName);
      break;
    }
  return retval;
}

/* emit conversion code for Xt_idxlist argument */
static int cfXtIdxlistConvert(FILE *outfp, const char *argName,
                              size_t argNameLen, enum conversionType part)
{
  int retval = 0;
  switch (part)
    {
    case CONV_ARG:
      retval
        = fprintf(outfp, "(*(Xt_idxlist *)%.*s)", (int)argNameLen, argName);
      break;
    case CONV_RET:
      abort();
      break;
    }
  return retval;
}

/*
 * Local Variables:
 * c-file-style: "Java"
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * show-trailing-whitespace: t
 * require-trailing-newline: t
 * End:
 */
