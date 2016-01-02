/****************** Start of $RCSfile: goptions.c,v $  ****************
*
* $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/goptions.c,v $
* $Id: goptions.c,v 1.5 2012/11/01 09:53:05 alb Exp alb $
* $Date: 2012/11/01 09:53:05 $
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

  static char * fileversion = "$RCSfile: goptions.c,v $ $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/goptions.c,v $ $Id: goptions.c,v 1.5 2012/11/01 09:53:05 alb Exp alb $ " PACKAGE " " VERSION_STRING;

#include <string.h>
#include <stdarg.h>
#include <x_types.h>
#include <x_errno.h>
#include <genutils.h>
#include <fileutil.h>
#include <ctype.h>

struct optdesc {
  Int32		minargs;
  Int32		maxargs;
  UChar		*optname;
  Int32		*argnumptr;
  void		**arglist;
  UChar		*plusoptptr;
  X_Type	argtype;
  Flag		plusopt;
  Flag		countflag;
};

#define	newarg	\
	{	args = ZRENEWP(args, struct optdesc, num_args + 1); \
		if(!args) \
		  GETOUT; \
		SETZERO(args[num_args]); \
		args[num_args].optname = NULL; \
		actarg = num_args; \
		num_args++; \
	}

#define	act	optstring[i]
#define	errexit(num)	{ errno = r = num ; GETOUT ; }
#define	nextc	{ i++; while(isspace(optstring[i]) && optstring[i]) i++; \
			if(!optstring[i]) errexit(BAD_STRING_FORMAT); }

#define	NOT_USED	-1
#define	OPTION_FLAGS	-2
#define	END_OF_OPTS	-3

#define	TypeUNUSED_ARG	TypeReal32PTR

static Int32	all_flag_args(UChar *, struct optdesc *, Int32);

Int32
goptions(
  Int32		argc,
  UChar		**argv,
  UChar		*optionstring,
  ...
)
{
  va_list       	add_args;
  struct optdesc	*args = NULL;
  Int32			num_args = 0, actarg, used_args, idx;
  Int32			nonames, i, j, argoffset = 0, ii;
  Int32			unused_args_idx = -1;
  UChar			*cptr;
  Int32			*argtable = NULL, r = NO_ERROR;
  UChar			**argstrings = NULL, *optstring = NULL;
  Flag			no_msgs = NO;
  int			n;
  long int		ibuf;

  if(!optionstring || !argv){
    errno = EINVAL;
    return(EINVAL);
  }

  if(argc < 0){
    argc = -argc;
    no_msgs = YES;
  }
			    /* get memory for some stuff and initialize it */
  argstrings = NEWP(UChar *, argc + argc);	 /* 2*argc is safety space */
  argtable = NEWP(Int32, argc);
  optstring = strdup(optionstring);	    /* work on duplicate, cause it */
  if(!argstrings || !argtable || !optstring)	      /* is to be modified */
    errexit(errno);
  for(i = 0; i < argc; i++){
    argtable[i] = NOT_USED;
  }
	      /* build a record of possible options from the option-string */
  i = 0;	 /* i points to the current character in the option-string */
  forever{
    while(act == ';')				      /* skip separator(s) */
	i++;
    if(!act)			/* act is defined as the current character */
	break;
    newarg;		 /* increment i, skip whitespace and check for EOS */
    if(act == '+'){			       /* option starts with a '+' */
	args[actarg].plusopt = 1;
	nextc;
    }
    switch(act){			/* select format character -> type */
	case 'f':
		args[actarg].argtype = TypeReal32;
		break;
	case 'd':
		args[actarg].argtype = TypeReal64;
		break;
	case 'i':
		args[actarg].argtype = TypeInt32;
		break;
	case 'u':
		args[actarg].argtype = TypeUns32;
		break;
	case 'b':
		args[actarg].argtype = TypeUChar;
		break;
	case 'c':
		args[actarg].argtype = TypeInt32;
		args[actarg].countflag = YES;
		break;
	case 's':
		args[actarg].argtype = TypeUCharPTR;
		break;
	case '*':
		args[actarg].argtype = TypeUNUSED_ARG;
		break;
	default:
		errexit(BAD_STRING_FORMAT);
    }

    if(args[actarg].argtype != TypeUNUSED_ARG)
	nextc;

    if(args[actarg].argtype == TypeUNUSED_ARG){
	args[actarg].minargs = 2;
	args[actarg].maxargs = 2;
	unused_args_idx = actarg;
    }
    else if(act == ':'){		     /* separator ? -> no range of */
	args[actarg].minargs = 1;	       /* option arguments defined */
	args[actarg].maxargs = 1;	       /* -> 1 argument for option */
    }
    else{
	if(args[actarg].argtype == TypeUChar)
	  errexit(BAD_STRING_FORMAT);

	if(act == '-'){					  /* opening range */
	  args[actarg].minargs = 0;		  /* get maximum # of args */
	  nextc;
	  if(1 > sscanf(optstring + i, "%ld%n", &ibuf, &n))
	    errexit(BAD_STRING_FORMAT);

	  args[actarg].maxargs = ibuf;
	  if(args[actarg].maxargs < 0)	     /* illegal value: maximum < 0 */
	    errexit(ILLEGAL_VALUE);
	  i += n - 1;
	  nextc;
	}
	else{					       /* get first number */
	  if(1 > sscanf(optstring + i, "%ld%n", &ibuf, &n))
	    errexit(BAD_STRING_FORMAT);
	  args[actarg].minargs = ibuf;

	  i += n - 1;
	  nextc;
	  if(act == '-'){		 /* range given: get second number */
	    nextc;

	    if(1 > sscanf(optstring + i, "%ld%n", &ibuf, &n))
	      errexit(BAD_STRING_FORMAT);
	    args[actarg].maxargs = ibuf;

	    i += n - 1;
	    nextc;
	    if(act != ':')		  /* syntax check, ':' must follow */
	      errexit(BAD_STRING_FORMAT);

						 /* exchange, if min > max */
	    if(args[actarg].maxargs < args[actarg].minargs)
		memswap(&args[actarg].maxargs, &args[actarg].minargs,
							sizeof(Int32));

	    if(args[actarg].minargs < 0 || args[actarg].maxargs < 0)
		errexit(ILLEGAL_VALUE);	 /* illegal values: min || max < 0 */
	  }
	  else		      /* no range: exact number of arguments given */
	    args[actarg].maxargs = args[actarg].minargs;
	}

	if(act != ':')			  /* syntax check, ':' must follow */
	  errexit(BAD_STRING_FORMAT);
    }

    i++;						/* skip whitespace */
    while(optstring[i] && isspace(optstring[i]))
	i++;
    cptr = strchr(optstring + i, ';');		/* find end of option name */
    if(cptr)
	*cptr = '\0';

    if(args[actarg].argtype != TypeUNUSED_ARG)
	args[actarg].optname = strdup(optstring + i);	/* get option name */
    else
	args[actarg].optname = strdup("\004");

			/* check for multiple appearance of option names */
    for(j = 0; j < num_args - 1; j++){
	if(!strcmp(args[j].optname, args[actarg].optname)){
	  if(!args[j].optname[0] && ((args[j].argtype == TypeUChar
				&& args[actarg].argtype != TypeUChar) ||
			(args[j].argtype != TypeUChar
				&& args[actarg].argtype == TypeUChar)))
	    continue;	/* This is the exception: The empty name may */
			/* appear two times: as a boolean option and */
							/* one other */
	  errexit(MULTIPLE_USE_ILLEGAL);
	}
    }

    if(cptr){
	i = cptr - optstring;		/* restore end of option name, */
	*cptr = ';';		/* if not at the end of the optionstring */
    }
    else
	i = strlen(optstring);	/* now we're done with the optionstring */
  }

  for(i = 0; i < num_args; i++){	/* get memory for the pointers */
    j = args[i].maxargs;			/* to the arguments */
    if(j < 1)				/* for safety we always use >= 1 */
	j = 1;

    args[i].arglist = NEWP(void *, j);	/* they can be of several types */
    if(!args[i].arglist)
	errexit(errno);
  }

  va_start(add_args, optionstring);	/* collect the argument pointers */

  for(i = 0; i < num_args; i++){	/* first, if we have a '+'-option */
    if(args[i].plusopt && args[i].argtype != TypeUChar)
	args[i].plusoptptr = va_arg(add_args, UChar *);

    if(args[i].minargs != args[i].maxargs){	/* for the # of arguments */
	args[i].argnumptr = va_arg(add_args, Int32 *);
    }

    for(j = 0; j < args[i].maxargs; j++)	  /* now for the arguments */
	args[i].arglist[j] = va_arg(add_args, void *);
  }

  va_end(add_args);		       /* users arg-pointers are processed */

  for(i = 1; i < argc; i++){
    if(!strcmp(argv[i], "--")){
	argtable[i] = END_OF_OPTS;
	break;
    }					/* step through the command line and */
    switch(argv[i][0]){		/* connect the argv with the options' record */
      case '-':			/* an option always starts with - or + */
      case '+':		/* 1st: the options with names longer than 1 char */
	for(j = 0; j < num_args; j++){
	  if(((args[j].argtype != TypeUChar && !args[j].countflag)
		|| strlen(args[j].optname) > 1) && strlen(argv[i]) > 1/*2*/){
	    if(!strncmp(argv[i] + 1, args[j].optname,
						strlen(argv[i] + 1))){
		if(strlen(argv[i]) == 2){
		  for(ii = 0; ii < num_args; ii++){
		    if(strlen(args[ii].optname) == 1 && ii != j
				&& args[ii].optname[0] == argv[i][1])
		      break;
		  }
		  if(ii < num_args){
			if(! no_msgs)
			  fprintf(stderr, T_("Warning: one-letter flag %s ambigous, ignored.\n"), argv[i] + 1); 
			break;		 /* the name has already been used */
		  }
		}
		if(argtable[i] >= 0){
		  if(! no_msgs)
		    fprintf(stderr, T_("Warning: Ambigous argument: %s. Second occurrence ignored.\n"), argv[i] + 1); 
		  break;		 /* the name has already been used */
		}
		argtable[i] = j;	   /* mark the index to the record */
		if(args[j].argnumptr)
		  *args[j].argnumptr = 0;		    /* reset the # */
		break;
	    }
	  }
	}
	if(j >= num_args){	 /* 2nd: the non-bool options, 1 char leng */
	  for(j = 0; j < num_args; j++){
	    if(args[j].argtype != TypeUChar && !args[j].countflag
					&& strlen(args[j].optname) == 1){
		if(argv[i][1] == args[j].optname[0]){
		  if(argtable[i] >= 0){
		    if(! no_msgs)
		      fprintf(stderr, T_("Warning: Ambigous argument: %s. Second occurrence ignored.\n"), argv[i] + 1); 
		    break;		 /* the name has already been used */
		  }
		  argtable[i] = j;	   /* mark the index to the record */
		  if(args[j].argnumptr)
		    *args[j].argnumptr = 0;		    /* reset the # */
		  break;
		}
	    }
	  }
	}		   /* 3rd: the options - and + without any letters */
	if(j >= num_args && argv[i][1] == '\0'){
	  for(j = 0; j < num_args; j++){
	    if(! args[j].optname[0] && args[j].argtype == TypeUChar){
		argtable[i] = j;
		if(args[j].argnumptr)
		  *args[j].argnumptr = 0;		    /* reset the # */
		break;
	    }
	  }
	}	 /* 4th: the bool options, 1 char length, can be clustered */
	if(j >= num_args){
	  if(all_flag_args(argv[i] + 1, args, num_args)){
	    argtable[i] = OPTION_FLAGS;
	  }
	}
	break;
    }
  }

  for(i = 0; i < num_args; i++){	/* now some variables must be */
    if(args[i].countflag)			/* initialized */
      if(args[i].arglist[0])
	*((Int32 *) args[i].arglist[0]) = 0;
  }
	    /* now step through the command line and process the arguments */
		      /* of the options with names longer than 1 character */
  for(i = 1; i < argc; i++){
    if(argtable[i] == END_OF_OPTS)
	break;

    idx = argtable[i];	      /* get the connection to the option's record */
    if(idx != NOT_USED && idx != OPTION_FLAGS && idx != END_OF_OPTS){
      if(args[idx].argtype == TypeUChar){	  /* 1st case: bool option */
	if(args[idx].plusopt){				     /* +/- Option */
	  if(args[idx].arglist[0])
	    *((UChar *) args[idx].arglist[0]) = (argv[i][0] == '+' ? 1 : 0);
	}
	else{					     /* simple -sth - flag */
	  if(argv[i][0] == '+'){	      /* used with +: only warning */
	    if(! no_msgs)
	      fprintf(stderr, T_("Warning: option `%s' should not be used with a '+'.\n"),
			args[idx].optname);
	  }
	  if(args[idx].arglist[0])
	    *((UChar *) args[idx].arglist[0]) = 1;
	}

	continue;		    /* that's it - bool option has no args */
      }

      if(args[idx].countflag){		/* 2nd case: bool counter option */
	if(args[idx].arglist[0])
	  (*((Int32 *) args[idx].arglist[0]))++;

	continue;	/* that's it - bool counter option has no args */
      }

      if(args[idx].minargs > 0 || args[idx].maxargs > 0){
	j = 0;			   	    /* other option with arguments */
	argoffset = 1;	    /* this indicates, whether the 1st argument is */
        if(strlen(args[idx].optname) == 1){	/* clutched to the option, */
	  if(strlen(argv[i] + 1) > 1){	/* what is possible, if the option */
	    j = 1;			  /* name is consisting of 1 char. */
	    argstrings[0] = argv[i] + 2;	   /* here we have the arg */
	    argoffset = 0;
	  }
	}
        for(; j < args[idx].minargs; j++){	    /* these are mandatory */
	  if(argtable[i + j + argoffset] != NOT_USED){
	    if(! no_msgs)
	      fprintf(stderr, T_("Error: option `%s' has not enough arguments.\n"),
			args[argtable[i]].optname);
	    r = -1;
	    CLEANUP;
	  }
	  argstrings[j] = argv[i + j + argoffset];
        }
        for(; j < args[idx].maxargs; j++){	     /* these are optional */
	  if(argtable[i + j + argoffset] != NOT_USED)
	    break;
	  argstrings[j] = argv[i + j + argoffset];
	}
      }

      used_args = 0;			/* count the really used arguments */
      for(j = 0; j < args[idx].maxargs; j++){	/* step thru the arguments */
	if(argtable[i + j + argoffset] == END_OF_OPTS)
	  break;
	if(argtable[i + j + argoffset] != NOT_USED && j + argoffset > 0)
	  break;		   /* here we can stop: next option starts */
	if(args[idx].argtype == TypeUCharPTR){	  /* option wants a string */
	  if(args[idx].arglist[j]){
	    *((UChar **) args[idx].arglist[j]) =
				strdup((UChar *) argstrings[j]);
	    if(! *((UChar **) args[idx].arglist[j]))
		errexit(errno);
	  }
	}
	else{				 /* option wants a float, int, ... */
	  n = sscanXValue(argstrings[j], args[idx].arglist[j],
						args[idx].argtype, NULL);
	  if(n < 1){			/* the argument can't be converted */
	    if(j >= args[idx].minargs - 1)	      /* but it's optional */
		break;

	    if(! no_msgs)
	      fprintf(stderr,	    /* critical: the argument is necessary */
		T_("Warning: cannot convert `%s' into requested type.\n"),
			argstrings[j]);
	    break;
	  }
	}
	argtable[i + j + argoffset] = argtable[i];	   /* mark as used */
	if(args[idx].argnumptr)		       /* increment user's counter */
	  (*args[idx].argnumptr)++;
	used_args++;			      /* increment our own counter */
      }

      if(args[idx].plusopt){	     /* if a + is allowed: set user's flag */
	if(args[idx].plusoptptr)
	  *args[idx].plusoptptr = (argv[i][0] == '+' ? 1 : 0);
      }
      else{
	if(argv[i][0] == '+')	       /* if not, but used: just warn user */
	  if(! no_msgs)
	    fprintf(stderr, T_("Warning: option `%s' should not be used with a '+'.\n"), args[idx].optname);
      }

      i += used_args - 1 + argoffset;	/* set i to the next option in the */
    }							   /* command line */
  }

  for(i = 1; i < argc; i++){	/* step again through the command line and */
    idx = argtable[i];		       /* process the boolean options with */
    if(idx != OPTION_FLAGS)		 /* a name with 1 character length */
	continue;

    for(cptr = argv[i] + 1; *cptr; cptr++){	 /* step through a cluster */
	for(j = 0; j < num_args; j++){			     /* of options */
	  if(strlen(args[j].optname) == 1 && args[j].optname[0] == *cptr
		&& (args[j].argtype == TypeUChar || args[j].countflag))
	    break;
	}

	if(j < num_args){
	  if(args[j].countflag){
	    if(args[j].arglist[0])
		(*((Int32 *) args[j].arglist[0]))++;
	  }
	  else if(args[j].plusopt){
	    if(args[j].arglist[0])
		*((UChar *) args[j].arglist[0]) = (argv[i][0] == '+' ? 1 : 0);
	  }
	  else{
	    if(argv[i][0] == '+'){
		if(! no_msgs)
		  fprintf(stderr, T_("Warning: option `%s' should not be used with a '+'.\n"),
					args[j].optname);
	    }
	    if(args[j].arglist[0])
		*((UChar *) args[j].arglist[0]) = 1;
	  }
	}
	else
	  fprintf(stderr, T_("Internal error: option classified as flag, but not in list.\n"));
    }
  }

		  /* look for unprocessed command line arguments, that may */
	  /* be assigned to non-boolean options with no/empty option names */
  for(j = 0; j < num_args; j++){
    if(strlen(args[j].optname) == 0 && args[j].argtype != TypeUChar){
	if(args[j].argnumptr)
	  *args[j].argnumptr = 0;
	break;
    }
  }
  if(j < num_args){		/* there are some "absolute" cmd-line args */
    nonames = 0;			   /* this is the counter for them */

    for(i = 1; i < argc; i++){
	if(nonames >= args[j].maxargs)	 /* we have enough of them -> stop */
	  break;
	if(argtable[i] != NOT_USED)		 /* this is already in use */
	  continue;

	if(args[j].argtype == TypeUCharPTR){		/* we want strings */
	  if(args[j].arglist[nonames]){
	    *((UChar **)args[j].arglist[nonames]) = strdup(argv[i]);
	    if(! *((UChar **)args[j].arglist[nonames]))
		errexit(errno);
	  }
	}
	else{			   /* we want some other (float, int ... ) */
	  n = sscanXValue(argv[i], args[j].arglist[nonames],
						args[j].argtype, NULL);
	  if(n < 1){
	    if(unused_args_idx >= 0)	 /* user wants unused be collected */
		continue;

	    if(! no_msgs)
		fprintf(stderr, T_("Warning: cannot convert `%s' into the requested type.\n"), argv[i]);

	    continue;
	  }
	}

	if(args[j].argnumptr)	/* increment the users pointer, if present */
	  (*args[j].argnumptr)++;

	nonames++;		 /* increment our pointer of absolute args */
	argtable[i] = 1;     /* mark the argument as in use (with dummy 1) */
    }
    if(nonames < args[j].minargs){	     /* not enough have been found */
	if(! no_msgs)
	  fprintf(stderr,
		T_("Error: not enough absolute arguments (must be%s %ld).\n"),
		(args[j].minargs < args[j].maxargs ? T_(" at least") : ""),
		(long int) args[j].minargs);
	r = -1;
	CLEANUP;
    }
  }

  for(i = 1, j = 0; i < argc; i++)	   /* look for unprocessed command */
    if(argtable[i] == NOT_USED){			 /* line arguments */
	j++;
	n = i;
    }

  if(j > 0){
    if(unused_args_idx < 0){
      if(!no_msgs){
	if(j == 1){	  /* if any are found: print them out as error msg */
	  fprintf(stderr, T_("Error: Invalid argument `%s'.\n"), argv[n]);
	}
	else{
	  fprintf(stderr, T_("Error: %ld invalid arguments:"), (long int) j);
	  for(i = 1; i < argc; i++)
	    if(argtable[i] == NOT_USED)
		fprintf(stderr, " %s", argv[i]);
	  fprintf(stderr, "\n");
	}
      }
      r = -1;

      CLEANUP;
    }
    else{
	UChar	**unused_args;

	unused_args = NEWP(UChar *, j + 1);
	if(! unused_args){
	  r = errno;
	  GETOUT;
	}
	unused_args[j] = NULL;
	for(i = 0, n = 1; i < j; i++, n++){
	  while(argtable[n] != NOT_USED)
	    n++;

	  unused_args[i] = strdup(argv[n]);
	  if(! unused_args[i]){
	    r = errno;
	    for(i--; i >= 0; i--)
		free(unused_args[i]);
	    free(unused_args);
	    GETOUT;
	  }
	}

	if(args[unused_args_idx].arglist[1])
	  *((UChar ***) args[unused_args_idx].arglist[1]) = unused_args;

	if(args[unused_args_idx].arglist[0])
	  *((Int32 *) args[unused_args_idx].arglist[0]) = j;
    }
  }

 cleanup:
  ZFREE(argtable);		/* normal cleanup */
  ZFREE(argstrings);
  ZFREE(optstring);
  if(args){
    for(i = 0; i < num_args; i++){
	ZFREE(args[i].optname);
	ZFREE(args[i].arglist);
    }
    free(args);
  }

  return(r);

 getout:			     /* clean up in case of emergency exit */
  r = errno;
  CLEANUP;
}

static Int32		    /* look in options' record, whether the argstr */
all_flag_args(			       /* 1-char option cluster is regular */
  UChar			*argstr,
  struct optdesc	*args,
  Int32			num_args)
{
  Int32		i;
  Flag		this_is_a_flag;

  if(! *argstr)		/* empty -> standalone + or - -> no option cluster */
    return(0);

  while(*argstr){
    this_is_a_flag = NO;

    for(i = 0; i < num_args; i++){
	if((args[i].argtype == TypeUChar || args[i].countflag)
				&& strlen(args[i].optname) == 1){
	  if(*argstr == args[i].optname[0]){
	    this_is_a_flag = YES;	 /* option name character is found */
	    break;
	  }
	}
    }

    if(!this_is_a_flag)			     /* not found: tilt, game over */
	return(0);

    argstr++;
  }

  return(1);
}
