#include "conf.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#ifdef	HAVE_STRING_H
#include <string.h>
#else
#ifdef	HAVE_STRINGS_H
#include <strings.h>
#endif
#endif
#include <fcntl.h>
#include <errno.h>
#ifdef HAVE_TERMIO_H
# include <termio.h>
#else
# include <termios.h>
#endif /* HAVE_TERMIO_H */
#include <stdarg.h>
#include <netdb.h>
#include <pwd.h>
#include <grp.h>
#ifdef  HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef  HAVE_TIME_H
#ifdef  TIME_WITH_SYS_TIME
#include <time.h>
#endif
#endif
#include <signal.h>
#include <sys/types.h>
#ifdef	HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#include <genutils.h>
#include <fileutil.h>
#include <netutils.h>

#define	EM__(statem)	nomemerrexit(statem)

static void
nomemerrexit(void * ptr)
{
  if(!ptr){
    fprintf(stderr, T_("Error: Cannot allocate memory.\n"));
    exit(4);
  }
}

/***************** af-lockfile ******************/

static	UChar	*filename = NULL;
static	int	lfd;
static	int	pid;
static	int	pst;
static	int	mpid;
static	int	ppid;
static	Flag	no_unlink_after = NO;

static void af_lockfile_sighand(int sig)
{
  Int32	i;

  if(sig == SIGUSR1){
    printf("%d\n", pid);
    fflush(stdout);

    exit(0);
  }

  if(sig == SIGCHLD){
    i = waitpid_forced(pid, &pst, 0);
    pst = WEXITSTATUS(pst);
    exit(pst);
  }

  close(lfd);
  if(!no_unlink_after)
    unlink(filename);

  exit(0);
}

static void af_lockfile_usage(UChar * pnam)
{
  fprintf(stderr, T_("Usage: %s [ -HU ] [ -p [ <PID> ] ] [ -m <message> ] [ -t <timeout-ms> ] \\\n"),
		FN_BASENAME(pnam));
  fprintf(stderr, T_("                   <lockfilename>\n"));
  exit(2);
}

int
albi_af_lockfile(int argc, char ** argv)
{
  Int32		i, n, remaining_ms, timeout_ms = 60000;
  Int32		num_parent_pids, parent_pid;
  UChar		*message = NULL, pbuf[100], *myhn = NULL, *cptr;
  Flag		last_try = NO, ignore_hup = NO;
  int		fd, kr;
  struct timeval	tv, ntv, etv;
  sigset_t	sigs, osigs;

  num_parent_pids = -1;

  i = goptions(-argc, (UChar **) argv, "s:m;i-1:p;i:t;b:U;b:H;s:",
			&message, &num_parent_pids, &parent_pid,
			&timeout_ms, &no_unlink_after, &ignore_hup, &filename);
  if(i)
    af_lockfile_usage(argv[0]);

  myhn = get_my_off_hn();

  if(access(filename, F_OK)){	/* If the file does not exist, try, if we */
    fd = open(filename, O_WRONLY | O_CREAT, 0644);	/* can create it */
    if(fd < 0){			/* If not, the directory is not writable */
      fprintf(stderr, T_("Error: Cannot create file `%s'.\n"), filename);
      exit(3);		/* or does not exist, so this will never work */
    }
    close(fd);
  }
	/* If the file exists, no check and continue trying to set the lock */

  signal(SIGUSR1, af_lockfile_sighand);

  gettimeofday(&tv, NULL);
  etv.tv_usec = tv.tv_usec + 1000 * (timeout_ms % 1000);
  etv.tv_sec = tv.tv_sec + (timeout_ms / 1000);
  if(etv.tv_usec > 1000000){
    etv.tv_sec++;
    etv.tv_usec -= 1000000;
  }

  mpid = getpid();
  ppid = getppid();

  sigemptyset(&sigs);			/* pid must be assigned before */
  sigaddset(&sigs, SIGUSR1);		/* SIGUSR1 may be evaluated */
  sigprocmask(SIG_BLOCK, &sigs, &osigs);

  pid = fork_forced();

  sigprocmask(SIG_SETMASK, &osigs, NULL);

  if(pid < 0){
    fprintf(stderr, T_("Error: Cannot fork.\n"));
    exit(5);
  }

  if(pid > 0){
    signal(SIGCHLD, af_lockfile_sighand);

    forever
	pause();
  }

  if( (cptr = message) ){
    sprintf(pbuf, "%d", (int) getpid());
    cptr = repl_substring(message, "%p", pbuf);
    if(!cptr){
	exit(6);
    }
    free(message);
    message = cptr;
    if(myhn){
	message = repl_substring(message, "%h", myhn);
	if(!message){
	  exit(6);
	}
	free(cptr);
    }
  }

  forever{
    lfd = set_wlock(filename);
    if(lfd >= 0){
	signal(SIGTERM, af_lockfile_sighand);
	signal(SIGINT, af_lockfile_sighand);
	if(ignore_hup)
	  signal(SIGHUP, SIG_IGN);
	else
	  signal(SIGHUP, af_lockfile_sighand);

	if(!message){
	  sprintf(pbuf, "%d %s", (int) getpid(), myhn);
	  message = pbuf;
	}

	i = strlen(message);
	if(i > 0){		/* if empty message: leave file alone */
	  ftruncate(lfd, 0);

	  n = write_forced(lfd, message, i);
	  if(n < i || write_forced(lfd, "\n", 1) < 1){
	    fprintf(stderr, T_("Error: Cannot write message to file `%s'.\n"),
				filename);
	    unlink(filename);
	    exit(4);
	  }

	  fsync(lfd);
	}

	kill(mpid, SIGUSR1);

	close(0);
	close(1);
	close(2);

	if(num_parent_pids > 0)
	  ppid = parent_pid;

	forever{
	  if(num_parent_pids >= 0){
	    kr = kill(ppid, 0);

	    if(kr && errno != EPERM){
		af_lockfile_sighand(SIGTERM);
	    }

	    ms_sleep(200);
	  }
	  else
	    pause();
	}
    }

    if(last_try){
	printf("0\n");
	fflush(stdout);
	exit(5);
    }

    remaining_ms = 1000 * (etv.tv_sec - tv.tv_sec) +
			(etv.tv_usec - tv.tv_usec) / 1000;
    if(remaining_ms > 300)
	remaining_ms = (Int32)(drandom() * 200.0) + 100;
    else
	last_try = YES;
	
    ms_sleep(remaining_ms);

    gettimeofday(&ntv, NULL);
    if(ntv.tv_sec < tv.tv_sec)
	ntv.tv_sec += 24 * 60 * 60;
    tv.tv_sec = ntv.tv_sec;
    tv.tv_usec = ntv.tv_usec;
  }
}

/******************* bufrate ******************/

/* Usage:

 $0 [ -b <blocksize> ] [ -a <num-blocks> ]
    { [ -r <blocks per second> ] |
      [ -R <percentage of initial throughput> ]
      [ -m <num first blocks for measurement> ] }

-b	the blocksize for read and write.
	Data type: 32 bit integer. Default: 1 KB

-a	normally the program tries to achieve an average maximum
	throughput over the entire runtime. That is, if data cannot
	be read or written during some time, the program will catch
	up piping the following data through as fast as possible to
	reach the average throughput again. It will continue to
	read/write slower, when the average maximum is reached again.
	If this behaviour is not desired, -a can be used to limit
	the undelayed data thorughput to the given number of blocks
	per second. The measurement interval will then start again
	each time, the given quantity of data has passed the program.
	Data type: floating point. Default: not set
      
-r	the maximum number of blocks per second to pass the program.
	Data type: floating point. Default: 8.0

-R	Instead of -r this option can be used. Then initially the
	program will pipe a number of blocks through without delay.
	The elapsed time is measured and the maximum throughput for
	the rest of the data is set to the measured value multiplied
	with the percentage given with -R
	Data type: percent floating point. Default: not set

-m	The number of blocks piped through the program without delay
	ot measure the achiavable throughput. This argument is only
	evaluated, if option -R is given.
	Data type: 32 bit integer. Default: 10

Typical uses;

$0 -r 20 -b 1024

$0 -b 1024 -m 20 -R 10.0

The latter example measures over 20 KB of data and sets the
throughput for the remaining data to 10 % of the measured rate

*/

static void
bufrate_usage(UChar * pnam)
{
  fprintf(stderr, T_("Usage: %s [ -b blocksize ] [ -a <num blocks to average over> ] \\\n"),
			FN_BASENAME(pnam));
  fprintf(stderr, T_("               { [ -r <blocks per second> ] | \\\n"));
  fprintf(stderr, T_("                 [ -R <percentage of initial throughput ] \\\n"));
  fprintf(stderr, T_("                 [ -m <first blocks for measurement> ] }\n"));

  exit(2);
}

static void
timediff(
  time_t	*sec,
  Int32		*usec,
  time_t	sec1,
  Int32		usec1,
  time_t	sec2,
  Int32		usec2)
{
  *sec = sec1 - sec2;

  if(usec1 >= usec2){
    *usec = usec1 - usec2;
  }
  else{
    *usec = usec1 + 1000000 - usec2;
    (*sec)--;
  }
}

static void
timenow(time_t * sec, Int32 * usec)
{
  time_t		sec1, sec2;
  struct timeval	tv;

  do{
    sec1 = time(NULL);
    gettimeofday(&tv, NULL);
    sec2 = time(NULL);
  }while(sec1 != sec2);

  *sec = sec1;
  *usec = tv.tv_usec;
}

int
albi_bufrate(int argc, char ** argv)
{
  Int32		blocksize;
  Real64	blocks_per_sec;
  Real64	percent_max_th;
  Int32		num_blocks_for_meas;
  Real64	blocks_av;
  UChar		*buffer = NULL;
  Int32		i, j, n, bytes_output = 0;
  Real64	time_til_rate, real_time, rtdiff;
  time_t	starttime_sec, curtime_sec, timev_sec;
  Int32		starttime_usec, curtime_usec, timev_usec;
  struct timeval	tv;

  blocksize = 1024;
  blocks_per_sec = 8;
  percent_max_th = -1.0;
  num_blocks_for_meas = 10;
  blocks_av = -1.0;

  i = goptions(-argc, (UChar **) argv,
		"i:b;d:r;d:a;d:R;i:m",
		&blocksize, &blocks_per_sec, &blocks_av,
		&percent_max_th, &num_blocks_for_meas);
  if(i)
    bufrate_usage(argv[0]);

  EM__(buffer = NEWP(UChar, blocksize));

  if(percent_max_th > 0.0){
    timenow(&starttime_sec, &starttime_usec);

    for(n = 0; n < num_blocks_for_meas; n++){
	i = read_forced(0, buffer, blocksize);
	POSZ(i);
	j = write_forced(1, buffer, i);
	POSZ(j);

	if(i < blocksize || j < i)
	  exit(0);
    }

    timenow(&curtime_sec, &curtime_usec);

    timediff(&timev_sec, &timev_usec,
		curtime_sec, curtime_usec, starttime_sec, starttime_usec);

    blocks_per_sec = (Real) num_blocks_for_meas
			/ ((Real64) timev_sec + (Real64) timev_usec / 1e6)
		* percent_max_th / 100.0;
  }

  timenow(&starttime_sec, &starttime_usec);

  forever{
    i = read_forced(0, buffer, blocksize);
    POSZ(i);
    j = write_forced(1, buffer, i);
    POSZ(j);

    if(i < blocksize || j < i){
	break;
    }

    bytes_output += j;

    if(blocks_av > 0.0 && bytes_output > (Int32)((Real64) blocksize * blocks_av)){
	timenow(&starttime_sec, &starttime_usec);
	bytes_output = 0;
    }

    timenow(&timev_sec, &timev_usec);
    timediff(&curtime_sec, &curtime_usec,
		timev_sec, timev_usec, starttime_sec, starttime_usec);

    real_time = (Real64) curtime_sec + (Real64) curtime_usec / 1.0e6;

    time_til_rate = (Real64) bytes_output
			/ blocks_per_sec / (Real64) blocksize;
    rtdiff = time_til_rate - real_time;
    if(rtdiff > 0.0){
	tv.tv_sec = (time_t) rtdiff;
	tv.tv_usec = (int) ((rtdiff - (Real64) tv.tv_sec) * 1e6);

	select(1, NULL, NULL, NULL, &tv);
    }
  }

  exit(0);
}

/********************** choice ***************************/

static void
choice_usage(UChar * pnam)
{
  fprintf(stderr, T_("Usage: %s [ -p <prompt> ] [ -c <choice-chars> ] \\\n"
		  "              [ -d <default-choice> ] [ -t <timeout> ]\n"),
				FN_BASENAME(pnam));
  exit(1);
}

int
albi_choice(int argc, char ** argv)
{
  UChar		*choices = "yn", choice;
  UChar		*msg, *pmsg;
  UChar		*defchoice, nbuf[100];
  Int32		maxtimeout = 60, i, timeout;

  defchoice = NEWP(UChar, 5);
  memset(defchoice, 0, 5 * sizeof(UChar));

  msg = TN_("Please choose one of (%c) [%d after %t sec]: ");

  i = goptions(-argc, (UChar **) argv, "s:p;s:c;s:d;i:t",
			&msg, &choices, &defchoice, &maxtimeout);
  if(i)
    choice_usage(argv[0]);

  msg = T_(msg);

  if(! defchoice[0])
    defchoice[0] = choices[0];
  if(maxtimeout < 0)
    maxtimeout = 0;
  timeout = maxtimeout;

  if(!strchr(choices, defchoice[0])){
    fprintf(stderr, T_("Error: Default choice not in choice list.\n"));
    exit(2);
  }

  defchoice[1] = '\0';

  msg = repl_substring(msg, "%c", choices);
  msg = repl_substring(msg, "%d", defchoice);

  forever{
    if(maxtimeout)
	sprintf(nbuf, "%d", timeout);
    else
	strcpy(nbuf, "inf");

    pmsg = repl_substring(msg, "%t", nbuf);
    fprintf(stderr, " \r%s \b", pmsg);
    free(pmsg);
    fflush(stderr);

    choice = defchoice[0];

    i = getinchr(&choice, 10);
    if(i > 0){
	if(strchr(choices, choice))
	  break;
	fprintf(stderr, "\a");
	fflush(stderr);
    }

    if(maxtimeout){
	timeout--;
	if(timeout < 1)
	  break;
    }
  }

  fprintf(stderr, " \r%s", repl_substring(msg, "%t", "0"));
  fflush(stderr);

  printf("%c\n", choice);

  exit(0);
}

/*************************** getent ****************************/

static void
print_hosts_entry(struct hostent *hent)
{
  char		*abuf, **apptr;

  if(!hent)
    return;

  abuf = addr_to_string(hent->h_addrtype, hent->h_addr_list[0]);
  if(abuf){
    fprintf(stdout, "%s\t%s", abuf, hent->h_name);
    free(abuf);
  }
  if(hent->h_aliases)
    for(apptr = hent->h_aliases; *apptr; apptr++)
      fprintf(stdout, " %s", *apptr);
  fprintf(stdout, "\n");
}

static void
print_passwd_entry(struct passwd *pwent)
{
  fprintf(stdout, "%s:%s:%d:%d:%s:%s:%s\n", pwent->pw_name,
		pwent->pw_passwd, (int) pwent->pw_uid,
		(int) pwent->pw_gid, pwent->pw_gecos,
		pwent->pw_dir, pwent->pw_shell);
}

static void
print_group_entry(struct group *grent)
{
  int	i;
  char	**cpptr;

  fprintf(stdout, "%s:%s:%d:", grent->gr_name,
		grent->gr_passwd, (int) grent->gr_gid);
  for(i = 0, cpptr = grent->gr_mem; *cpptr; cpptr++, i++){
    if(i > 0)
	fprintf(stdout, ",");
    fprintf(stdout, "%s", *cpptr);
  }
  fprintf(stdout, "\n");
}

static void
print_serv_entry(struct servent *svc)
{
  char	**cpptr;

  fprintf(stdout, "%-20s %d/%s", svc->s_name,
		(int) svc->s_port, svc->s_proto);
  for(cpptr = svc->s_aliases; *cpptr; cpptr++){
    fprintf(stdout, " %s", *cpptr);
  }
  fprintf(stdout, "\n");
}

static void
print_proto_entry(struct protoent *proto)
{
  char	**cpptr;

  fprintf(stdout, "%-20s %d", proto->p_name, (int) proto->p_proto);
  for(cpptr = proto->p_aliases; *cpptr; cpptr++){
    fprintf(stdout, " %s", *cpptr);
  }
  fprintf(stdout, "\n");
}

static void
getent_usage(UChar * pname)
{
  fprintf(stderr, T_("Usage: %s [ -ri ] <db-name> [ <db-key> ]\n"),
		FN_BASENAME(pname));
  exit(1);
}

int
albi_getent(int argc, char ** argv)
{
  Int32		i;
  UChar		**cpptr;
  nodeaddr	addr;
  int		exitst = 0, n1, l;
  Flag		reverse = NO, acceptip = NO;
  Int32		numargs = 0;
  UChar		**args = NULL;
  UChar		*dbname = NULL;
  UChar		**dbkeys = NULL;
  UChar		*str;

  i = goptions(-argc, (UChar **) argv, "b:r;b:i;*",
		&reverse, &acceptip, &numargs, &args);
  if(i || numargs < 1)
    getent_usage(argv[0]);
  dbname = args[0];
  if(numargs > 1)
    dbkeys = args + 1;

  if(!strcmp(dbname, "hosts") || !strcmp(dbname, "ipnodes")){
    struct hostent	*hent;

    if(dbkeys){
      for(cpptr = dbkeys; *cpptr; cpptr++){
	i = sizeof(addr);
	if(string_to_addr(*cpptr, &addr, &i, &n1)){
	  hent = get_host_by_addr(&addr, i, n1);
	  if(!hent && acceptip){
	    if( (str = addr_to_string(n1, &addr)) ){
		fprintf(stdout, "%s\n", str);
		continue;
	    }
	  }
	}
	else{
	  hent = get_host_by_name(*cpptr);
	  if(hent && reverse){
	    if(set_tcp_sockaddr_hp((struct sockaddr *)(&addr), hent, 0) < 0)
		hent = NULL;
	    else
		hent = get_host_by_sockaddr((struct sockaddr *)(&addr));
	  }
	}

	if(!hent){
	  exitst = 2;
	  continue;
	}

	print_hosts_entry(hent);
      }
    }
    else{
      sethostent(1);
      if(1){
	while( (hent = gethostent()) )
	  print_hosts_entry(hent);

	endhostent();
      }
      else{
	exitst = 2;
      }
    }
  }
  else if(!strcmp(dbname, "passwd")){
    struct passwd	*pwent;
    int		uid;

    if(dbkeys){
      for(cpptr = dbkeys; *cpptr; cpptr++){
	n1 = -1;
	i = sscanf(*cpptr, "%d%n", &uid, &n1);
	l = strlen(*cpptr);
	if(n1 == l && n1 > 0)
	  reverse = YES;
	if(reverse){
	  if(i < 1 || n1 < l){
	    fprintf(stderr,
			T_("Error: %s is not a valid numerical UID\n"), *cpptr);
	    exitst = 1;
	    continue;
	  }
	  pwent = getpwuid(uid);
	}
	else{
	  pwent = getpwnam(*cpptr);
	}

	if(!pwent){
	  exitst = 2;
	  continue;
	}
	print_passwd_entry(pwent);
      }
    }
    else{
	setpwent();
	while( (pwent = getpwent()) )
	  print_passwd_entry(pwent);

	endpwent();
    }
  }
  else if(!strcmp(dbname, "group")){
    struct group	*grent;
    int		gid;

    if(dbkeys){
      for(cpptr = dbkeys; *cpptr; cpptr++){
	n1 = -1;
	i = sscanf(*cpptr, "%d%n", &gid, &n1);
	l = strlen(*cpptr);
	if(n1 == l && n1 > 0)
	  reverse = YES;
	if(reverse){
	  if(i < 1 || n1 < l){
	    fprintf(stderr,
		T_("Error: %s is not a valid numerical GID\n"), *cpptr);
	    exitst = 1;
	    continue;
	  }
	  grent = getgrgid(gid);
	}
	else{
	  grent = getgrnam(*cpptr);
	}

	if(!grent){
	  exitst = 2;
	  continue;
	}
	print_group_entry(grent);
      }
    }
    else{
	setgrent();
	while( (grent = getgrent()) )
	  print_group_entry(grent);

	endgrent();
    }
  }
  else if(!strcmp(dbname, "services")){
    struct servent	*svcent;
    int		port;
    char	*proto = NULL, *cptr;

    if(dbkeys){
      for(cpptr = dbkeys; *cpptr; cpptr++){
	if(reverse){
	  n1 = -1;
	  i = sscanf(*cpptr, "%d%n", &port, &n1);
	  if(i < 1 || n1 < 1){
	    fprintf(stderr,
		T_("Error: %s does not start with a valid numerical port number\n"),
				*cpptr);
	    exitst = 1;
	    continue;
	  }
	  if((*cpptr)[n1] == '/'){
	    proto = first_nospace(*cpptr + n1 + 1);
	  }
	  svcent = getservbyport(port, proto);
	}
	else{
	  if( (cptr = strchr(*cpptr, '/')) ){
	    *cptr = '\0';
	    proto = first_nospace(cptr + 1);
	  }
	  svcent = getservbyname(*cpptr, proto);
	}

	if(!svcent){
	  exitst = 2;
	  continue;
	}
	print_serv_entry(svcent);
      }
    }
    else{
	setservent(1);
	while( (svcent = getservent()) )
	  print_serv_entry(svcent);

	endservent();
    }
  }
  else if(!strcmp(dbname, "protocols")){
    struct protoent	*protoent;
    int		proton;

    if(dbkeys){
      for(cpptr = dbkeys; *cpptr; cpptr++){
	if(reverse){
	  n1 = -1;
	  i = sscanf(*cpptr, "%d%n", &proton, &n1);
	  if(i < 1 || n1 < 1){
	    fprintf(stderr, T_("Error: %s does not start with a valid numerical protocol number\n"), *cpptr);
	    exitst = 1;
	    continue;
	  }
	  protoent = getprotobynumber(proton);
	}
	else{
	  protoent = getprotobyname(*cpptr);
	}

	if(!protoent){
	  exitst = 2;
	  continue;
	}
	print_proto_entry(protoent);
      }
    }
    else{
	setprotoent(1);
	while( (protoent = getprotoent()) )
	  print_proto_entry(protoent);

	endprotoent();
    }
  }
  else{
    fprintf(stderr, T_("Unknown database: %s\n"), dbname);
    getent_usage(argv[0]);
  }

  exit(exitst);
}

/***************************** numset ********************************/

static void
numset_usage(UChar * pnam)
{
  fprintf(stderr, T_("Usage: %s <set> {+|-|in} <set_to_add_or_remove>\n"
			"      %s # <set>\n"),
	FN_BASENAME(pnam), FN_BASENAME(pnam));
  exit(2);
}

int
albi_numset(int argc, char ** argv)
{
  Int32		i, j, n;
  Flag		calc_num;
  Uns32Range	*ranges_opnd, *ranges_opor;
  Uns32Range	*result;	/* uninitialized OK */

  calc_num = (argc == 3 && !strcmp(argv[1], "#"));

  if(argc != 4 && !calc_num)
    numset_usage(argv[0]);

  if(strcmp(argv[2], "-") && strcmp(argv[2], "+") && strcasecmp(argv[2], "in")
		&& strcmp(argv[1], "#"))
    numset_usage(argv[0]);

  ranges_opnd = sscan_Uns32Ranges__(argv[calc_num ? 2 : 1], 1, 1, NULL, NULL);
  if(!calc_num)
    ranges_opor = sscan_Uns32Ranges__(argv[3], 1, 1, NULL, NULL);

  if(!ranges_opnd || (!ranges_opor && !calc_num)){
    fprintf(stderr, T_("Error: Cannot read argument 1 or 3 as set of numbers.\n"));
    exit(1);
  }

  pack_Uns32Ranges(ranges_opnd, 0);
  if(calc_num){
    fprintf(stdout, "%d\n", (int) num_Uns32Ranges(ranges_opnd));
    exit(0);
  }

  pack_Uns32Ranges(ranges_opor, 0);

  if(argv[2][0] == '+'){
    i = merge_Uns32Ranges(&ranges_opnd, ranges_opor);
    if(i){
	fprintf(stderr, T_("Error: Merging sets failed: %s.\n"),
			strerror(errno));
	exit(1);
    }
    result = ranges_opnd;
  }
  if(argv[2][0] == '-'){
    result = del_range_from_Uns32Ranges(ranges_opnd, ranges_opor);
    if(!result){
	fprintf(stderr, T_("Error: Removing numbers from sets failed: %s.\n"),
			strerror(errno));
	exit(1);
    }
  }
  if(!strcasecmp(argv[2], "in")){
    for(i = n = 0; i < len_Uns32Ranges(ranges_opnd); i++){
      for(j = ranges_opnd[i].first; j <= ranges_opnd[i].last; j++){
	if(in_Uns32Ranges(ranges_opor, j))
	  n++;
      }
    }

    fprintf(stdout, "%d\n", (int) n);
    exit(0);
  }

  fprint_Uns32Ranges(stdout, result, 0);
  fprintf(stdout, "\n");

  exit(0);
}

/***************************** rotor ****************************/

static UChar	*rotor = "/-\\|";

int
albi_rotor(int argc, char ** argv)
{
  char		**nargv;
  int		pe[2], po[2], pid, pst;
  Int32		i, nfds, l, p;
  UChar		linebuf[1000], rotbuf[2];
  fd_set	ifds;
  struct timeval	tv;

  if(argc < 2)
    exit(0);

  nargv = NEWP(char *, argc + 2);

  for(i = 1; i < argc; i++)
    nargv[i] = argv[i];
  argv[i] = NULL;
  argv[0] = strdup(argv[1]);

  pipe(pe);
  pipe(po);

  pid = fork_forced();
  if(pid < 0){
    perror("fork");
    exit(2);
  }

  if(!pid){
    close(po[0]);
    close(pe[0]);

    dup2(po[1], 1);
    dup2(pe[1], 2);

    execvp(argv[0], argv + 1);

    perror("execvp");
    exit(99);
  }

  close(po[1]);
  close(pe[1]);

  rotbuf[1] = '\b';
  l = strlen(rotor);
  p = 0;

  nfds = MAX(po[0], pe[0]);

#if 0
  write(1, rotor, 1);
  fflush(stdout);
#endif

  forever{
    FD_ZERO(&ifds);
    FD_SET(po[0], &ifds);
    FD_SET(pe[0], &ifds);
    SETZERO(tv);

    i = select(nfds + 1, &ifds, NULL, NULL, &tv);

    if(FD_ISSET(po[0], &ifds)){
	i = read(po[0], linebuf, 1000);
	if(i > 0){
#if 0
	  write(1, "\b", 2);
#endif
	  write(1, linebuf, i);

#if 0
	  write(1, rotor + p, 1);
	  fflush(stdout);
#endif
	}
    }
    if(FD_ISSET(pe[0], &ifds)){
	i = read(pe[0], linebuf, 1000);
	if(i > 0){
#if 0
	  write(2, "\b", 2);
#endif
	  write(2, linebuf, i);
	  fflush(stderr);

#if 0
	  write(1, rotor + p, 1);
	  fflush(stdout);
#endif
	}
    }

    i = waitpid(pid, &pst, WNOHANG);

    if(i == pid)
	break;

    rotbuf[0] = rotor[p];

    write(1, rotbuf, 2);
    fflush(stdout);

    p++;
    if(p >= l)
	p = 0;

    ms_sleep(80);
  }

  close(po[0]);
  close(pe[0]);

  write(1, " \b", 2);

  exit(WEXITSTATUS(pst));
}

/***************************** ssu *****************************/

static void
ssu_usage(char * pnam)
{
  char	*cptr;

  cptr = strrchr(pnam, '/');
  if(cptr)
    pnam = cptr + 1;

  fprintf(stderr, T_("Usage: %s [ -e ] <username>[:<groupname>[:<sec-groupname>[:...]]] <command> [ <args>... ]\n"), pnam);

  exit(1);
}

int
albi_ssu(int argc, char ** argv)
{
  char			*the_shell, *groupname = NULL, *secgroups = NULL;
  char			*cptr, *username;
  struct passwd		*pwdentry = NULL;
  struct group		*grentry;
  gid_t			*gr = NULL;
  int			uid, gid, n, e, ngr;
  Flag			have_uid = NO, have_gid = NO;

  if(argc < 2)
    ssu_usage(argv[0]);
  e = strcmp(argv[1], "-e");
  if((e || argc < 4) && (!e || argc != 3))
    ssu_usage(argv[0]);

  if(getuid()){
    fprintf(stderr, T_("Error: Only root can run this program.\n"));
    exit(2);
  }

  if(switch_to_user_str(argv[e ? 1 : 2])){
    fprintf(stderr, "Error: %s\n", strerror(errno));
    exit(4);
  }

  if(e){
    the_shell = "/bin/sh";
#if 0
    if(pwdentry)
      if(pwdentry->pw_shell)
	the_shell = pwdentry->pw_shell;
#endif

    execlp(the_shell, the_shell, "-c", argv[2], NULL);
    fprintf(stderr, T_("Error: Cannot execute `%s'\n"), the_shell);
  }
  else{
    execvp(argv[3], argv + 3);
    fprintf(stderr, T_("Error: Cannot execute `%s'\n"), argv[3]);
  }

  exit(3);
}

/*************************** ttywrap ******************************/

static	int	spid;

static void
ttywrap_sighand(int sig)
{
  kill(spid, sig);
}

int
albi_ttywrap(int argc, char ** argv)
{
  char	ptys[20];
  UChar	*outst_to_master;
  UChar	*outst_to_stdout;
  Int32	noutst_to_master, noutst_to_stdout;
  Int32	nall_to_master, nall_to_stdout;
  int	fl, master, slave, pid, pst, exited = 0, pp[2], nlcr = 0;
  Int32	i, j, nwr;
  Flag	eofwritten;
  struct timeval	tv;
#ifdef HAVE_TERMIO_H
  static struct termio	Termio;
#else
  static struct termios	Termio;
#endif
  fd_set infds;
  char	iobuffer[2048], eofchr;
  struct sigaction oldsigs[32];
  sigset_t	sig_mask;

  if(argc > 1){
    if(! strcmp(argv[1], "-r"))
      nlcr = 1;
    else if(! strcmp(argv[1], "-rr"))
      nlcr = 2;
    if(nlcr){
      argv++;
      argc--;
    }
  }

  outst_to_master = NEWP(UChar, 1000);
  outst_to_stdout = NEWP(UChar, 1000);
  if(!outst_to_master || !outst_to_stdout){
    fprintf(stderr, T_("Error: no memory\n"));
    exit(3);
  }
  nall_to_master = nall_to_stdout = 1000;
  noutst_to_master = noutst_to_stdout = 0;

  master = open_pty_master(ptys);

  pipe(pp);	/* to pass the eof control character */

  pid = fork();
  if(pid < 0){
    fprintf(stderr, T_("Error: Cannot start subprocess.\n"));
    exit(3);
  }
  if(pid){
#if 0
    fcntl(master, F_SETFL, fcntl(master, F_GETFL) | NONBLOCKING_FLAGS);
#endif
    close(pp[1]);
  }
  else{
#ifdef HAVE_TERMIO_H
    struct termio	ttyattr;
#else
    struct termios	ttyattr;
#endif
  
    close(pp[0]);

    setsid();

    slave = open_pty_slave(master, ptys);
    close(master);

#ifdef	HAVE_TERMIO_H
    ioctl(slave, TCGETA, &Termio);
#else
    ioctl(slave, TIOCGETA, &Termio);
#endif

    Termio.c_lflag &= ~ECHO;	/* No echo */
    Termio.c_oflag &= ~ONLCR;	/* Do not map NL to CR-NL on output */

#ifdef	HAVE_TERMIO_H
    ioctl(slave, TCSETA, &Termio);
    i = ioctl(slave, TCGETA, &ttyattr);
#else
    ioctl(slave, TIOCSETA, &Termio);
    i = ioctl(slave, TIOCGETA, &ttyattr);
#endif

    if(!i)
	eofchr = ttyattr.c_cc[VEOF];
    else
	eofchr = '\004';	/* Notnagel */

    write(pp[1], &eofchr, 1);
    close(pp[1]);

    dup2(slave, 0);
    dup2(slave, 1);
    dup2(slave, 2);
    if(slave > 2)
	close(slave);

    /*fcntl(1, F_SETFL, O_APPEND);
    setbuf(stdout, NULL);*/
    
    execvp(argv[1], argv + 1);
    perror(T_("Cannot execute subprogram\n"));
    exit(123);
  }

  spid = pid;

  read(pp[0], &eofchr, 1);
  close(pp[0]);

  for(i = 1; i < 32; i++){
    struct sigaction	sigs;

    if(i == SIGCHLD || i == SIGPIPE)
	continue;

    memset(&sigs, 0, sizeof(sigs));
    sigs.sa_handler = ttywrap_sighand;
#ifdef	SA_RESTART
    sigs.sa_flags = SA_RESTART;
#endif
    sigaction(i, &sigs, oldsigs + i);
  }
  sigemptyset(&sig_mask);
#ifdef	SIGTTIN
  sigaddset(&sig_mask, SIGTTIN);
#endif
#ifdef	SIGTTOU
  sigaddset(&sig_mask, SIGTTOU);
#endif
  sigprocmask(SIG_BLOCK, &sig_mask, NULL);

  eofwritten = NO;

  for(;;){
    i = waitpid(pid, &pst, WNOHANG);
    if(i == pid){
      exited = 1;
      break;
    }

    FD_ZERO(&infds);
    FD_SET(0, &infds);
    FD_SET(master, &infds);

    tv.tv_sec = 0;
    tv.tv_usec = 200000;

    i = select(master + 1, &infds, NULL, NULL, &tv);
    if(i < 1)
      continue;

    if(FD_ISSET(0, &infds)){
	i = read(0, iobuffer, 1024);
	if(i < 1 && noutst_to_master < 1)
          break;

	if(nlcr == 1){	/* convert line feed to CR */
	  for(j = 0; j < i; j++)
	    if(iobuffer[j] == '\n')
		iobuffer[j] = '\r';
	}
	else if(nlcr == 2){	/* convert line feed to CRLF */
	  for(j = 0; j < i; j++){
	    if(iobuffer[j] == '\n'){
		memmove(iobuffer + j + 1, iobuffer + j,
				sizeof(UChar) * (i - j));
		iobuffer[j] = '\r';
		j++;
		i++;
	    }
	  }
	}

	fl = fcntl(master, F_GETFL);
	fcntl(master, F_SETFL, fl | NONBLOCKING_FLAGS);

	while(noutst_to_master > 0){
	  nwr = write(master, outst_to_master, noutst_to_master);
	  if(nwr > 0){
	    memmove(outst_to_master, outst_to_master + nwr,
			(noutst_to_master - nwr) * sizeof(UChar));
	    noutst_to_master -= nwr;
	  }
	  else{
	    break;
	  }
	}

	if(i < 0)
	  i = 0;
	nwr = 0;
	if(noutst_to_master == 0){
	  if(i > 0)
            nwr = write(master, iobuffer, i);
	}

	fcntl(master, F_SETFL, fl);

	if(nwr < i){
	  if(nwr < 0)
	    nwr = 0;

	  if(noutst_to_master + i - nwr > nall_to_master){
	    nall_to_master = noutst_to_master + i - nwr;
	    outst_to_master = RENEWP(outst_to_master, UChar,
					nall_to_master);
	    if(!outst_to_master){
		fprintf(stderr, T_("Error: No more memory\n"));
		exit(8);
	    }
	  }

	  memcpy(outst_to_master + noutst_to_master,
			iobuffer + nwr, (i - nwr) * sizeof(UChar));
	  noutst_to_master += i - nwr;
	}
    }
    if(FD_ISSET(master, &infds)){
      i = read(master, iobuffer, 1023);
      if(i > 0){
	fl = fcntl(1, F_GETFL);
	fcntl(1, F_SETFL, fl | NONBLOCKING_FLAGS);

	while(noutst_to_stdout > 0){
	  nwr = write(1, outst_to_stdout, noutst_to_stdout);
	  if(nwr > 0){
	    memmove(outst_to_stdout, outst_to_stdout + nwr,
			(noutst_to_stdout - nwr) * sizeof(UChar));
	    noutst_to_stdout -= nwr;
	  }
	  else{
	    break;
	  }
	}

	if(noutst_to_stdout == 0)
          nwr = write(1, iobuffer, i);
	else
	  nwr = 0;

	fcntl(1, F_SETFL, fl);

	if(nwr < i){
	  if(nwr < 0)
	    nwr = 0;

	  if(noutst_to_stdout + i - nwr > nall_to_stdout){
	    nall_to_stdout = noutst_to_stdout + i - nwr;
	    outst_to_stdout = RENEWP(outst_to_stdout, UChar,
					nall_to_stdout);
	    if(!outst_to_stdout){
		fprintf(stderr, T_("Error: No more memory\n"));
		exit(8);
	    }
	  }

	  memcpy(outst_to_stdout + noutst_to_stdout,
			iobuffer + nwr, (i - nwr) * sizeof(UChar));
	  noutst_to_stdout += i - nwr;
	}
      }
    }
    /* fflush(NULL); */
  }

  if(noutst_to_stdout > 0)
    write(1, outst_to_stdout, noutst_to_stdout);

  fl = fcntl(master, F_GETFL);
  fcntl(master, F_SETFL, fl | NONBLOCKING_FLAGS);

  if(!exited){
    if(write(master, &eofchr, 1) >= 1)
      eofwritten = YES;

#if 0
    i = fcntl(master, F_GETFL);
    i &= INV_NONBLOCKING_FLAGS;
    fcntl(master, F_SETFL, i);
#endif

    while(waitpid(pid, &pst, WNOHANG) != pid){
      i = read(master, iobuffer, 2048);
      if(i > 0)
	write(1, iobuffer, i);
      else
	ms_sleep(150);

      if(!eofwritten)
	if(write(master, &eofchr, 1) >= 1)
	  eofwritten = YES;
    }
  }

#ifdef	hpux
  if(wait_for_input(master, 1) > 0)
#endif

  while((i = read(master, iobuffer, 2048)) > 0){
    write(1, iobuffer, i > 0 ? i : 0);

    if(!eofwritten)
	if(write(master, &eofchr, 1) >= 1)
	  eofwritten = YES;
  }

  pst = WEXITSTATUS(pst);

#if 0
  for(i = 1; i < 32; i++){
    if(i == SIGCHLD || i == SIGPIPE)
	continue;

    sigaction(i, oldsigs + i, NULL);
  }
#endif

  exit(pst);
}

/******************************** udf *****************************/

static UChar	*coltitles[] = {
	TN_("FILESYSTEM"), TN_("DEVICE"), TN_("TYPE"), TN_("SIZE"), TN_("FREE"),
	TN_("AVAIL"), TN_("USED"), TN_("CAPACITY"), TN_("BLOCKSIZE"), };
static UChar	*colpats[] = {
	"%m", "%d", "%t", "%s", "%f", "%U", "%u", "%c", "%b", };

static UChar	*defskipfs[] = { "autofs", "ignore", "auto", NULL, };

static void
udf_usage(UChar * pnam)
{
  fprintf(stderr, T_("Usage: %s [ -afUusbmdtch ] [ -F <formatstring> ] [ {+|-}T <types> ] [ <directories>... ]\n"),
		FN_BASENAME(pnam));
  exit(2);
}

int
albi_udf(int argc, char ** argv)
{
  UChar		*cptr, **cpptr, *line;
  UChar		**dirs = NULL;
  Int32		ndirs = 0;
  UChar		*formatstring = NULL;
  Int32		i, n, l, numfs;
  MntEnt	*mounts = NULL, *mptr;
  Int32		nmounts = 0, blocksize;
  UChar		*dirname;
  UChar		*devstr;
  UChar		*typestr;
  UChar		dbuf[40];
  Real64	bfree, bavail, bsize, d;
  struct stat	statb;
  Flag		show_allfs = NO, show_size = NO, show_usable = NO,
		show_inuse = NO, show_free = NO, show_blocksz = NO,
		show_mountp = NO, show_dev = NO, show_cap = NO,
		show_type = NO, no_header = NO, plustypes;
  Flag		givehelp = NO;
  UChar		*typearg = NULL, **typelist = NULL;
  UChar		***output_fields = NULL;
  Int32		num_rows = 0;
  Int32		num_cols = sizeof(coltitles) / sizeof(coltitles[0]);
  Int32		*fieldlengths;
  Flag		*prows;

  i = goptions(-argc, (UChar **) argv,
		"b:?;s:F;*;b:a;b:f;b:U;b:u;b:s;b:b;b:m;b:d;b:t;b:c;b:h;+s:T",
		&givehelp, &formatstring, &ndirs, &dirs, &show_allfs,
		&show_free, &show_usable, &show_inuse, &show_size,
		&show_blocksz, &show_mountp, &show_dev, &show_type,
		&show_cap, &no_header, &plustypes, &typearg);
  if(i || givehelp)
    udf_usage(argv[0]);

  if(!show_size && !show_usable && !show_inuse && !show_free
		&& !show_blocksz && !show_mountp && !show_dev
		&& !show_type && !show_cap)
    show_size = show_usable = show_inuse = show_free
		= show_blocksz = show_mountp = show_dev
		= show_type = show_cap = YES;

  EM__(fieldlengths = NEWP(Int32, num_cols));
  memset(fieldlengths, 0, num_cols * sizeof(Int32));
  EM__(output_fields = NEWP(UChar **, 2));
  EM__(output_fields[0] = NEWP(UChar *, num_cols));
  for(n = 0; n < num_cols; n++)
    EM__(output_fields[0][n] = strdup(T_(coltitles[n])));
  output_fields[1] = NULL;
  num_rows = 1;

  if(typearg){
    while( (cptr = strchr(typearg, ',')) )
	*cptr = ' ';

    if(str2words(&typelist, typearg) < 0)
	EM__(NULL);
  }
  else{
    if(!show_allfs)
	typelist = defskipfs;
  }

#if 0
  if(ndirs < 1){
#endif
    mounts = get_all_mounts(&nmounts);
    if(!mounts){
	fprintf(stderr, T_("Error: Cannot determine mounts.\n"));
	exit(3);
    }
#if 0
  }
#endif

  numfs = (ndirs > 0 ? ndirs : nmounts);

  for(i = 0; i < numfs; i++){
    dirname = (ndirs > 0 ? dirs[i] : mounts[i].dir);

    if(ndirs > 0)
	l = stat(dirname, &statb);
    else
	l = lstat(dirname, &statb);
    if(l < 0){
	fprintf(stderr, T_("Error: Cannot stat directory `%s'.\n"), dirname);
	continue;
    }

    mptr = find_mnt_by_devno_dir(mounts, nmounts, statb.st_dev, dirname);
    if(!mptr){
	free_mounts(mounts);
	mounts = get_all_mounts(&nmounts);
	if(!mounts){
	  fprintf(stderr, T_("Error: Cannot determine mounts.\n"));
	  exit(3);
	}

	mptr = find_mnt_by_devno_dir(mounts, nmounts, statb.st_dev, dirname);
	if(!mptr){
	  fprintf(stderr, T_("Error: Cannot determine mountpoint for %s\n"),
			dirname);
	  continue;
	}
    }


    devstr = (ndirs > 0 ? mptr->devstr : mounts[i].devstr);
    typestr = (ndirs > 0 ? mptr->typestr : mounts[i].typestr);
    dirname = (ndirs > 0 ? mptr->dir : mounts[i].dir);

    get_fs_status(dirname, &blocksize, &bsize, &bfree, &bavail);

    bsize *= blocksize;
    bfree *= blocksize;
    bavail *= blocksize;

    EM__(output_fields = RENEWP(output_fields, UChar **, num_rows + 2));
    output_fields[num_rows + 1] = NULL;
    EM__(output_fields[num_rows] = NEWP(UChar *, num_cols));

    EM__(output_fields[num_rows][0] = strdup(dirname));

    EM__(output_fields[num_rows][1] = strdup(devstr));

    EM__(output_fields[num_rows][2] = strdup(typestr));

    sprintf(dbuf, "%.0lf", (double) bsize / 1024.0 + 0.5);
    EM__(output_fields[num_rows][3] = strdup(dbuf));

    sprintf(dbuf, "%.0lf", (double) bfree / 1024.0 + 0.5);
    EM__(output_fields[num_rows][4] = strdup(dbuf));

    sprintf(dbuf, "%.0lf", (double) bavail / 1024.0 + 0.5);
    EM__(output_fields[num_rows][5] = strdup(dbuf));

    sprintf(dbuf, "%.0lf", (double) (bsize - bfree) / 1024.0 + 0.5);
    EM__(output_fields[num_rows][6] = strdup(dbuf));

    d = bsize - bfree + bavail;
    if(d > 0.0)
	sprintf(dbuf, "%.1lf%%", (double) (bsize - bfree) / (bsize - bfree + bavail) * 100.0 + 0.05);
    else
	strcpy(dbuf, "-%");
    EM__(output_fields[num_rows][7] = strdup(dbuf));

    sprintf(dbuf, "%ld", (long int) blocksize);
    EM__(output_fields[num_rows][8] = strdup(dbuf));

    num_rows++;
  }

  EM__(prows = NEWP(Flag, num_rows));

  prows[0] = !no_header;

  for(i = 1; i < num_rows; i++)
    prows[i] = typearg ? (!plustypes) : YES;

  for(i = 1; i < num_rows; i++){
    if(typelist){
      for(cpptr = typelist; *cpptr; cpptr++){
	if(!strcmp(*cpptr, output_fields[i][2])){
	  prows[i] = !prows[i];
	}
      }
    }
  }

  for(i = 0; i < num_rows; i++){
    for(n = 0; n < num_cols; n++){
      if(prows[i]){
	l = strlen(output_fields[i][n]);
	if(l > fieldlengths[n])
	  fieldlengths[n] = l;
      }
    }
  }

  if(!show_mountp)
    fieldlengths[0] = 0;
  if(!show_dev)
    fieldlengths[1] = 0;
  if(!show_type)
    fieldlengths[2] = 0;
  if(!show_size)
    fieldlengths[3] = 0;
  if(!show_free)
    fieldlengths[4] = 0;
  if(!show_usable)
    fieldlengths[5] = 0;
  if(!show_inuse)
    fieldlengths[6] = 0;
  if(!show_cap)
    fieldlengths[7] = 0;
  if(!show_blocksz)
    fieldlengths[8] = 0;

  if(!formatstring){
    for(i = 8; i >= 0; i--){
	if(!fieldlengths[i])
	  num_cols--;
	else
	  break;
    }
  }

  for(i = 0; i < num_rows; i++){
    if(prows[i]){
      if(!formatstring){
	for(n = 0; n < num_cols; n++){
	  if(fieldlengths[n] > 0){
	    if(n < num_cols - 1)
		fprintf(stdout, "%-*s", (int) fieldlengths[n] + 1, output_fields[i][n]);
	    else
		fprintf(stdout, "%s", output_fields[i][n]);
	  }
	}
	fprintf(stdout, "\n");
      }
      else{
	EM__(cptr = strdup(formatstring));
	for(n = 0; n < num_cols; n++){
	  EM__(line = repl_substring(cptr, colpats[n],
					output_fields[i][n]));
	  free(cptr);
	  cptr = line;
	}
	repl_esc_seq(cptr, '\\');
	fprintf(stdout, "%s", cptr);
	free(cptr);
      }
    }
  }

  exit(0);
}

/******************************** secbydate *****************************/

int
albi_secbydate(int argc, char ** argv)
{
  UChar		*argstr, intstr[64];
  time_t	t;

  if(argc < 2){
    fprintf(stderr, T_("Usage: Arguments to parse and convert to the time must be given.\n"));
    exit(2);
  }

  EM__(argstr = stringlist_cat((UChar **)(argv + 1), argc - 1, NULL));

  t = time_from_datestr(argstr);

  if(t == UNSPECIFIED_TIME){
    fprintf(stderr,
	T_("ERROR: Cannot determine the time from the given arguments.\n"));
    exit(1);
  }

  time_t_to_intstr(t, intstr);
  printf("%s\n", intstr);
  exit(0);
}

/******************************** datebysec *****************************/

int
albi_datebysec(int argc, char ** argv)
{
  if(argc != 2){
    fprintf(stderr, T_("Usage: %s <secs-since-epoch>\n"),
				FN_BASENAME((UChar *) argv[0]));
    exit(2);
  }

  printf("%s\n", timestr(strint2time(argv[1])));
  exit(0);
}

/*********************** base64 en/decode *************************/

int
albi_base64dec(int argc, char ** argv)
{
  Int32		i, nargs = 0, n;
  Uns32		state;
  UChar		*result = NULL, **args, *line = NULL;

  i = goptions(argc, (UChar **) argv, "*", &nargs, &args);
  if(i){
    fprintf(stderr, T_("Usage: %s [ <base64string> [ <base64string>... ] ]\n"),
			FN_BASENAME((UChar *) argv[0]));
    exit(2);
  }

  state = 0;
  for(i = 0; i < nargs; i++){
    if( (n = base64_decode(args[i], &result, -1, &state)) < 0){
	fprintf(stderr, T_("Error: error decoding base64 string `%s'\n"), args[i]);
	exit(2);
    }

    write(1, result, n);
    ZFREE(result);
  }

  if(nargs == 0){
    while(!feof(stdin)){
	ZFREE(line);
	line = fget_alloc_str(stdin);
	if(!line)
	  continue;

	if( (n = base64_decode(line, &result, -1, &state)) < 0){
	  fprintf(stderr, T_("Error: error decoding base64 string `%s'\n"), line);
	  exit(2);
	}

	write_forced(1, result, n);
	ZFREE(result);
    }
  }

  exit(0);
}

int
albi_base64enc(int argc, char ** argv)
{
  Int32		i, nargs = 0, n, p = 0, nchars = 0, nwr = 0, rem;
  Uns32		state;
  UChar		*result = NULL, **args, *line = NULL, buf[4096];

  i = goptions(argc, (UChar **) argv, "i:n;*", &nchars, &nargs, &args);
  if(i){
    fprintf(stderr,
		T_("Usage: %s [ -n <charsperline> ] [ <data> [ <data>... ] ]\n"),
			FN_BASENAME((UChar *) argv[0]));
    exit(2);
  }

  state = 0;
  rem = nchars;
  for(i = 0; i < nargs; i++){
    n = base64_encode(args[i], &result, strlen(args[i]), i == nargs - 1, &state);
    if(n < 0){
	fprintf(stderr, T_("Error: error encoding string `%s'\n"), args[i]);
	exit(2);
    }
    p = 0;

    if(nchars > 0){
	while(p < n){
	  nwr = n - p;
	  if(nwr > rem)
	    nwr = rem;
	  write_forced(1, result + p, nwr);
	  p += nwr;
	  rem -= nwr;
	  if(rem == 0){
	    write_forced(1, "\n", 1);
	    rem = nchars;
	  }
	}
    }
    else{
	write_forced(1, result, n);
    }

    ZFREE(result);
  }

  if(nargs == 0){
    do{
	i = read_forced(0, buf, 4096);
	if(i < 0)
	  break;

	if( (n = base64_encode(buf, &result, i, i < 4096, &state)) < 0){
	  fprintf(stderr, T_("Error: error encoding string `%s'\n"), line);
	  exit(2);
	}
	p = 0;

	if(nchars > 0){
	  while(p < n){
	    nwr = n - p;
	    if(nwr > rem)
		nwr = rem;
	    write_forced(1, result + p, nwr);
	    p += nwr;
	    rem -= nwr;
	    if(rem == 0){
		write_forced(1, "\n", 1);
		rem = nchars;
	    }
	  }
	}
	else{
	  write_forced(1, result, n);
	}

	ZFREE(result);
    }while(i >= 4096);
  }

  if(rem < nchars || nchars == 0)
    write_forced(1, "\n", 1);

  exit(0);
}

/*********************** descrypt *************************/

#ifdef	HAVE_DES_ENCRYPTION

#include <crptauth.h>

#include "cryptkey.h"
#include "des_aux.h"

#ifndef XOR
#define XOR(a,b)	(((a) & ~(b)) | (~(a) & (b)))
#endif

#ifndef	ENCRYPT				/* should be defined in des.h */
#define	ENCRYPT			1
#endif
#define	WEAK_ENCRYPTION		(ENCRYPT << 1)

static	UChar	*initstring = "Albi's encryption initialization string";

static	des_key_schedule	accesskeys[2];

static Int32
set_crptkey(UChar * filename)
{
  struct stat	statb;
  Int32		i, fd, ret = 0;
  Uns32		factor, prevval, newval;
  UChar		buf[100], *kptr, *endptr, *bufptr;
  des_cblock	rawkeys[2];

  buf[0] = '\0';

  if(filename){
    i = stat(filename, &statb);
    if(i)
      ret = ENOENT;
    else{
      fd = open(filename, O_RDONLY);
      if(fd < 0){
	ret = EACCES;
      }
      else{
	i = read(fd, buf, 99);
	close(fd);

	if(i < 0)
	  i = 0;
	buf[i] = '\0';
      }
    }
  }

  if(ret || !filename)
    strncpy(buf, ACCESSKEYSTRING, 99);
  if(!buf[0])
    memset(buf, 0x5a, 99);
  buf[99] = '\0';

  memset(rawkeys, 0x5a, 2 * sizeof(des_cblock));
  kptr = (UChar *) &(rawkeys[0]);
  endptr = kptr + 2 * sizeof(des_cblock);
  bufptr = buf + 1;

  prevval = buf[0] % 0x40;
  factor = 0x40;
  forever{
    if(! *bufptr)
	break;

    newval = prevval + factor * ((*bufptr) % 0x40);
    factor *= 0x40;

    if((newval & 0xff) == (prevval & 0xff) && factor >= 0x0100){
	*kptr = (prevval & 0xff);
	kptr++;
	if(kptr >= endptr)
	  break;

	factor /= 0x0100;
	newval /= 0x0100;
    }

    prevval = newval;
    bufptr++;
  }
  while(kptr < endptr && prevval){
    *(kptr++) = prevval & 0xff;
    prevval /= 0x0100;
  }

  if(des_set_key(rawkeys, accesskeys[0])
			|| des_set_key(rawkeys + 1, accesskeys[1]))
    ret = -1;

  return(ret);
}

static Uns32
crpt(UChar * outbuf, UChar * buf, Int32 len, Int8 flags, Int8 in)
{
  static Int8		initialized = 0;
  static des_cblock	ivec[2];

  UChar		mask, *cptr;
  Int32		i;
  Int8		encrypt, weak;

  encrypt = (flags & ENCRYPT) ? 1 : 0;
  weak = (flags & WEAK_ENCRYPTION) ? 1 : 0;

  if(in)
    initialized = 0;

  if(! initialized){
    cptr = (UChar *) ivec;
    mask = 0xdf;

    for(i = 0; i < sizeof(des_cblock) * 2; i++, cptr++){
      *cptr = XOR(mask, initstring[i]);
      mask = XOR(mask, initstring[i]);
    }

    initialized = 1;
  }

  if(weak)
    des_cbc_encrypt((des_cblock *) buf, (des_cblock *) outbuf,
			len * sizeof(UChar),
			accesskeys[0], ivec, encrypt);
  else
#ifndef	LIBDESBUG_COMPATIBILITY
    des_ede2_cbc_encrypt((des_cblock *) buf, (des_cblock *) outbuf,
			len * sizeof(UChar),
			accesskeys[0], accesskeys[1], ivec, encrypt);
#else
    des_3cbc_encrypt((des_cblock *) buf, (des_cblock *) outbuf,
			len * sizeof(UChar),
			accesskeys[0], accesskeys[1],
			ivec, ivec + 1, encrypt);
#endif
}

void
descrypt_usage(UChar * pname)
{
  fprintf(stderr, "Usage: %s [ -dw ] [ -k <cryptkeyfile> ]\n",
		FN_BASENAME(pname));
  exit(2);
}

int
albi_descrypt(int argc, char ** argv)
{
  UChar		ibuf[256], obuf[256];
  UChar		decrypt = 0, encrypt = 0, *cryptfile = NULL, weak = 0;
  Int32		i, nsh = 4, n;

  i = goptions(-argc, (UChar **) argv, "b:e;b:d;s:k;b:w",
			&encrypt, &decrypt, &cryptfile, &weak);
  if(i || (encrypt && decrypt))
    descrypt_usage(argv[0]);

  if(weak)
    weak = WEAK_ENCRYPTION;

  set_crptkey(cryptfile);

  if(! decrypt){
    while((i = read_forced(0, ibuf + 1, 255)) > 0){
      ibuf[0] = (UChar) i;

      n = ((((i + 1) - 1) >> nsh) + 1) << nsh;

      crpt(obuf, ibuf, n, ENCRYPT | weak, 0);

      write_forced(1, obuf, n);
    }
  }
  else{
    while((i = read_forced(0, ibuf, 256)) > 0){
      n = (((i - 1) >> nsh) + 1) << nsh;

      crpt(obuf, ibuf, n, weak, 0);

      write_forced(1, obuf + 1, obuf[0]);
    }
  }

  exit(0);
}

#endif	/* defined(HAVE_DES_ENCRYPTION) */

/*********************** main *************************/

int
albi_main(int argc, char ** argv)
{
  UChar		*programname;

  programname = FN_BASENAME((UChar *) argv[0]);

  /* albiutils */
  if(strstr(programname, "af-lockfile"))
    albi_af_lockfile(argc, argv);
  else if(strstr(programname, "bufrate"))
    albi_bufrate(argc, argv);
  else if(strstr(programname, "choice"))
    albi_choice(argc, argv);
  else if(strstr(programname, "getent") || strstr(programname, "get_ent"))
    albi_getent(argc, argv);
  else if(strstr(programname, "numset"))
    albi_numset(argc, argv);
  else if(strstr(programname, "rotor"))
    albi_rotor(argc, argv);
  else if(strstr(programname, "ssu"))
    albi_ssu(argc, argv);
  else if(strstr(programname, "ttywrap"))
    albi_ttywrap(argc, argv);
  else if(strstr(programname, "udf"))
    albi_udf(argc, argv);
  else if(strstr(programname, "secbydate"))
    albi_secbydate(argc, argv);
  else if(strstr(programname, "datebysec"))
    albi_datebysec(argc, argv);
  else if(strstr(programname, "base64enc"))
    albi_base64enc(argc, argv);
  else if(strstr(programname, "base64dec"))
    albi_base64dec(argc, argv);
#ifdef	HAVE_DES_ENCRYPTION
  else if(strstr(programname, "descrypt"))
    albi_descrypt(argc, argv);
#endif

  return(1);
}
