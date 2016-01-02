/****************** Start of $RCSfile: __packpats.c,v $  ****************
*
* $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/__packpats.c,v $
* $Id: __packpats.c,v 1.5 2011/12/19 21:24:10 alb Exp alb $
* $Date: 2011/12/19 21:24:10 $
* $Author: alb $
*
*
******* description ***********************************************
*
*
*
*******************************************************************/

#include <conf.h>
#include <version.h>

  static char * fileversion = "$RCSfile: __packpats.c,v $ $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/__packpats.c,v $ $Id: __packpats.c,v 1.5 2011/12/19 21:24:10 alb Exp alb $ " PACKAGE " " VERSION_STRING;

#include <stdio.h>
#include <unistd.h>
#include <x_types.h>
#include <genutils.h>
#include <fileutil.h>
#include <afpacker.h>
#include <afbackup.h>

#define	EM__(arg)	nomemmsgexit((arg), stderr)

void
usage(UChar * prognam)
{
  fprintf(stderr, "Usage: %s <path>\n", FN_BASENAME(prognam));

  exit(2);
}

main(int argc, char ** argv)
{
  UChar		*line = NULL, *oldline, *c2, *c3, *pat;
  Int32		len, clen, llen, nall = 0, oldall;

  if(argc > 2)
    usage(argv[0]);

  pat = argc > 1 ? argv[1] : "";
  if(pat[0] && strcmp(pat, "/"))
    EM__(pat = strapp(pat, "/"));

  len = strlen(pat);

  clen = strlen(CMDINOUTPREFIX);

  EM__(oldline = NEWP(UChar, oldall = 1024));
  strcpy(oldline, "");

  while(!feof(stdin)){
    if(fget_realloc_str(stdin, &line, &nall))
	continue;
    chop(line);

    if(!strncmp(line, CMDINOUTPREFIX, clen)){
	fprintf(stdout, "%s\n", line);
	fflush(stdout);
	continue;
    }

    if(!strncmp(pat, line, len)){
	c2 = line + len;
#if 0
	if(*c2 != FN_DIRSEPCHR)
	  continue;
#endif

	c3 = strchr(c2 + 1, FN_DIRSEPCHR);
	if(c3){
	  c2 = c3;
	  c3 = strchr(c2 + 1, FN_DIRSEPCHR);
	  if(c3)
	    *c3 = '\0';
	}

	if(!strcmp(line, oldline))
	  continue;

	fprintf(stdout, "%s\n", line);
	fflush(stdout);

	llen = strlen(line) + 1;
	if(llen > oldall)
	  oldline = RENEWP(oldline, UChar, oldall = llen);

	strcpy(oldline, line);
    }
  }

  exit(0);
}
