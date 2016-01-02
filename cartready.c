/****************** Start of $RCSfile: cartready.c,v $  ****************
*
* $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/cartready.c,v $
* $Id: cartready.c,v 1.3 2011/12/14 20:27:29 alb Exp alb $
* $Date: 2011/12/14 20:27:29 $
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

  static char * fileversion = "$RCSfile: cartready.c,v $ $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/cartready.c,v $ $Id: cartready.c,v 1.3 2011/12/14 20:27:29 alb Exp alb $ " PACKAGE " " VERSION_STRING;

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <fileutil.h>
#include <x_regex.h>

#include "afbackup.h"

#define	MODE_CARTREADY		1
#define	MODE_CHANGERREADY	2

Int32	mode = MODE_CARTREADY;

static void
usage(char * pnam)
{
  fprintf(stderr, T_("usage: %s\n"), pnam);

  exit(1);
}

main(int argc, char ** argv)
{
  UChar		*filename;

#ifdef ENABLE_NLS
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);
#endif
  
  if(argc > 1)
    usage(argv[0]);

  if(re_find_match_once((UChar *) "[Cc][Hh][Aa][Nn][Gg][Ee][Rr]",
		FN_BASENAME((UChar *) argv[0]), NULL, NULL) >= 0)
    mode = MODE_CHANGERREADY;

  filename = CARTREADY_FILE;
  if(mode == MODE_CHANGERREADY)
    filename = CHANGERREADY_FILE;

  if(unlink(filename)){
    if(errno == ENOENT){
      switch(mode){
       case MODE_CARTREADY:
	fprintf(stderr, T_("Warning: File `%s' does not exist.\n"
		" Probably a new tape has been detected automatically\n"
		" and thus this file has been removed.\n"), filename);
       case MODE_CHANGERREADY:
	fprintf(stderr, T_("Warning: File `%s' does not exist.\n"
		" Probably someone else has already issued this\n"
		" command and thus this file has been removed.\n"), filename);
      }
    }
    else{
	fprintf(stderr, T_("Error: Cannot remove file `%s': %s\n"),
				filename, strerror(errno));
    }

    exit(1);
  }

  exit(0);
}
