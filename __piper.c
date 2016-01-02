/****************** Start of $RCSfile: __piper.c,v $  ****************
*
* $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/__piper.c,v $
* $Id: __piper.c,v 1.3 2011/12/14 20:27:27 alb Exp alb $
* $Date: 2011/12/14 20:27:27 $
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

  static char * fileversion = "$RCSfile: __piper.c,v $ $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/__piper.c,v $ $Id: __piper.c,v 1.3 2011/12/14 20:27:27 alb Exp alb $ " PACKAGE " " VERSION_STRING;

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#ifdef	HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#include <genutils.h>
#include <afbackup.h>

Int32
nomemerrexit()
{
  fprintf(stderr, "Error: No memory.\n");
  exit(1);
}

main(int argc, char ** argv)
{
  UChar		*allargs = NULL;
  Int32		arglen;
  UChar		**procs = NULL;
  int		*pids, pst, p, gpst = 0;
  Int32		numprocs;
  UChar		*cptr, *cptr2;
  Int32		i;
  int		po[2];
  char		**nargv, **cargv;
  Flag		lastone;

  if(argc < 2)
    exit(0);

  cargv = NEWP(char *, argc + 1);
  if(!cargv)
    nomemerrexit();
  for(i = 0; i < argc; i++){
    cargv[i] = strdup(argv[i]);
    if(!cargv[i])
	nomemerrexit();
    memset(argv[i], 0, strlen(argv[i]));	/* try to hide commandline */
  }
  cargv[i] = NULL;
  argv = cargv;

  for(arglen = 1, i = 1; i < argc; i++)
    arglen += strlen(argv[i]) + 1;

  allargs = NEWP(UChar, arglen);
  if(!allargs)
    nomemerrexit();

  strcpy(allargs, argv[1]);	/* chain all arguments */
  for(i = 2; i < argc; i++){
    strcat(allargs, " ");
    strcat(allargs, argv[i]);
  }

  if(word_count(allargs) < 1)		/* empty command */
    exit(0);

  numprocs = 1;
  cptr = allargs;
  forever{				/* count commands in pipe */
    cptr = strchr(cptr + 1, '|');
    if(!cptr)
	break;

    while(escaped(allargs, cptr, '\\')){
	cptr = strchr(cptr + 1, '|');
	if(!cptr)
	  break;
    }

    if(!cptr)
	break;

    numprocs++;
  }

  procs = NEWP(UChar *, numprocs);
  if(!procs)
    nomemerrexit();

  cptr = allargs;
  for(i = 0; i < numprocs; i++, cptr = cptr2 + 1){
    cptr2 = strchr(cptr + 1, '|');
    if(cptr2){
	while(escaped(allargs, cptr2, '\\')){
	  cptr2 = strchr(cptr2 + 1, '|');
	  if(!cptr2)
	    break;
	}

	if(cptr2)
	  *cptr2 = '\0';
    }

    procs[i] = strdup(cptr);
    if(!procs[i])
	nomemerrexit();

    if(!cptr2)
	break;

    *cptr2 = '|';
  }

  pids = NEWP(int, numprocs);

  for(i = 0; i < numprocs; i++){
    lastone = (i == numprocs - 1);

    if(!lastone){
      if(pipe(po)){
	fprintf(stderr, "Error: cannot create pipe\n");
	exit(1);
      }
    }

    pids[i] = fork_forced();

    if(pids[i] < 0){
	fprintf(stderr, "Error: Cannot fork.\n");
	exit(2);
    }

    if(pids[i]){	/* parent */
      if(!lastone){
	close(po[1]);
	dup2(po[0], 0);
	close(po[0]);
      }
    }
    else{		/* child */
	int	efd;
	FILE	*efp;

	if(!lastone)
	  close(po[0]);

	if(cmd2argvq((char ***) (&nargv), procs[i]))
	  nomemerrexit();

	efd = dup(2);
	fcntl(efd, F_SETFD, 1);

	if(!lastone)
	  dup2(po[1], 1);

	execvp(nargv[0], nargv + 1);

	efp = fdopen(efd, "w");
	fprintf(efp, "Error: Cannot execute %s\n", nargv[0]);
	exit(99);
    }
  }

  close(0);

  while(numprocs > 0){
    p = wait(&pst);
    if(WEXITSTATUS(pst))
	gpst = WEXITSTATUS(pst);

    numprocs--;
  }

  exit(gpst);
}
