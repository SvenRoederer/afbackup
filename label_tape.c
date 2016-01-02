/****************** Start of $RCSfile: label_tape.c,v $  ****************
*
* $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/label_tape.c,v $
* $Id: label_tape.c,v 1.14 2011/12/19 21:24:10 alb Exp alb $
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

  static char * fileversion = "$RCSfile: label_tape.c,v $ $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/label_tape.c,v $ $Id: label_tape.c,v 1.14 2011/12/19 21:24:10 alb Exp alb $ " PACKAGE " " VERSION_STRING;

#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/utsname.h>
#ifdef  HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifdef	HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#include <genutils.h>
#include <x_regex.h>
#include <fileutil.h>
#include <afpacker.h>
#include <mvals.h>
#include <sysutils.h>

#include "afbackup.h"

Flag		interactive;

struct utsname	unam;

UChar		*configfile = NULL;

UChar		*programfile = NULL;
UChar		*programdir = NULL;
UChar		*bindir = NULL;
UChar		*libdir = NULL;
UChar		*confdir = NULL;
UChar		*vardir = NULL;

ReplSpec	replacements[] = {
	{ "%B",	NULL, &bindir },
	{ "%L",	NULL, &libdir },
	{ "%C",	NULL, &confdir },
	{ "%V",	NULL, &vardir },
	};

UChar		*mail_program = NULL;
UChar		*user_to_inform = NULL;

UChar		*devicesstr = NULL;

UChar		*devicename = NULL;
/* for cart_ctl */
Int32		pos_in_changer = 0;

UChar		*changername = NULL;
Int32		num_changer_slots = 0;
Int32		num_changer_loadports = 0;
UChar		*changerconffile = NULL;

StreamerDevice	*streamers = NULL;
Int32		num_streamers = 0;
ChangerDevice	*changers = NULL;
Int32		num_changers = 0;
Int32		curdevidx = 0;

#define	LOC_DRIVE	0
#define	LOC_SLOT	1
#define	LOC_LOADPORT	2
#define	LOC_EXTERNAL	99
#define	LOC_UNKNOWN	100
#define	NUM_LOC_CMD	3

#define	MOVE_UNSUPPORTED	-99


UChar		*cartmove_cmds[NUM_LOC_CMD /* source */][NUM_LOC_CMD /* target */];
UChar		*freeslots_cmd = NULL;
UChar		*freeloadports_cmd = NULL;
UChar		*lslabels_cmd = NULL;
UChar		*lslabelsdrive_cmd = NULL;
UChar		*lslabelsslot_cmd = NULL;
UChar		*lslabelsloadport_cmd = NULL;
UChar		*cartloc_file = NULL;
UChar		*cartnames_file = NULL;

typedef	struct _cartlabelent_ {
  UChar		*label;
  Int32		num;
  Int32		pos;
  Int8		loctype;
} CartLabelEnt;

CartLabelEnt	*hwcartlabels = NULL;
Int32		num_hwcartlabels = 0;

Flag		cctl_inventory = NO;	/* operation flags from args */
Flag		cctl_move = NO;
Flag		cctl_label = NO;
Flag		cctl_list = NO;
Flag		cctl_eject = NO;
Flag		cctl_names = NO;
Flag		cctl_invlbl = NO;
Flag		cctl_attrs = NO;
Int8		list_mode = 0;
#define		LIST_CARTLOCS	0
#define		LIST_CARTNAMES	1
#define		LIST_HWINV	2
#define		LIST_CARTATTRS	8
#define		LIST_CARTRESRVS	16

Int32		have_loc_arg = 0;
Int32		have_drive_arg = 0;
Int32		have_loadp_arg = 0;
Int32		have_slot_arg = 0;
UChar		*locstr = NULL;			/* arg strings */

UChar		*cartstr = NULL;		/* from command line */
UChar		*slotstr = NULL;
UChar		*loadportstr = NULL;
UChar		*drivestr = NULL;
UChar		**cartnames = NULL;
Int32		num_cartnames = 0;
UChar		*attrstr = NULL;
Uns32Range	*cartridges = NULL;		/* the really used ranges */
Uns32Range	*slots = NULL;
Uns32Range	*loadports = NULL;
Uns32Range	*drives = NULL;
Int32		n_slots = 0;		/* n_ means number of given */
Int32		n_drives = 0;
Int32		n_cartridges = 0;
Int32		n_loadports = 0;
UChar		*given_changer = NULL;

Int32		cartis_wcart = -1;
Int32		cartis_wfile = -1;
Int32		have_cartis_cart = 0;
Int32		cartis_cart = -1;
Int32		cartridge_set = 1;

Uns32		cartins_gracetime = DEFAULT_CARTGRACETIME;
Uns32		devunavail_send_mail = DEFAULT_DEVUNAVSENDMAIL;
Uns32		devunavail_give_up = DEFAULT_DEVUNAVGIVEUP;

UChar		*bytesontape_file = NULL;

UChar		*precious_tapes_file = NULL;
UChar		*cartorder_file = NULL;
UChar		*ro_tapes_file = NULL;
UChar		*used_blocksizes_file = NULL;
UChar		*tapeposfile = NULL;
UChar		*tape_usage_file = NULL;
UChar		*client_message_file = NULL;

Flag		forced = NO;
Flag		query = NO;
Flag		dont_ask = NO;
Flag		delfromhd = NO;

Flag		only_read_label = NO;

struct stat	default_stat;

struct stat	devstatb;

UChar		*setfilecmd = NULL;

Int32		tapeblocksize = DEFAULT_TAPEBLOCKSIZE;

Int32		num_cartridges = 1;

Int32		label = -1;
Int32		scnd_label = -1;
Int32		label_given = 0;
Int32		scnd_label_arg = 0;

UChar		*comment = "<labeled manually>";

Int32		actcart = 0;

UChar		*infobuffer;
Int32		infobuffersize;

UChar		*loggingfile = NULL;
UChar		*syslog_ident = NULL;

UChar		*remoteuser = NULL;
UChar		*clientuname = NULL;

UGIDS		vardirowner;
UGIDS		deviceowner;

#define	TMPBUFSIZ	4096

UChar		tmpbuf[TMPBUFSIZ];

Int32		mode = -1;

#define	MODE_LABEL_TAPE	0
#define	MODE_CART_CTL	1
#define	MODE_CART_IS	2

UChar	*cmd_res[] = {
	"[Ll][Aa][Bb][Ee][Ll]",
	"[Cc][Aa][Rr][Tt].*[Cc][Tt]\\([Rr]\\)?[Ll]",
	"[Cc][Aa][Rr][Tt].*[Ii][Ss]",
		};

#define	BU_NO_LOCK		0
#define	BU_LOCKED		1
#define	BU_GOT_LOCK		2
#define	BU_CANT_LOCK		99

typedef	struct __lockData	{
  UChar		*lockfile;
  int		lockfd;
  UChar		locked;
}	LockData;

LockData	devicelock = { NULL, -1, BU_NO_LOCK };
LockData	changerlock = { NULL, -1, BU_NO_LOCK };

#define	NUM_CHCF_ENTRIES	15
#define	NUM_PRESET_CHCF_ENTRIES	6

static	ParamFileEntry	changerconf[NUM_CHCF_ENTRIES] = {
	{ &freeslots_cmd, NULL,
	(UChar *) "^[ \t]*\\([Ll]ist[ \t]*\\)?\\([Ff]ree\\|[Ee]mpty\\)[-_ \t]*[Ss]lots?[-_ \t]*[Cc]o?mm?a?n?d:?[ \t]*",
		TypeUCharPTR	},
	{ &freeloadports_cmd, NULL,
	(UChar *) "^[ \t]*\\([Ll]ist[ \t]*\\)?\\([Ff]ree\\|[Ee]mpty\\)[-_ \t]*[Ll]oad[-_ \t]*\\([Pp]ort\\|[Bb]ay\\)s?[-_ \t]*[Cc]o?mm?a?n?d:?[ \t]*",
		TypeUCharPTR	},
	{ &lslabels_cmd, NULL,
	(UChar *) "^[ \t]*\\([Ll]ist[ \t]*\\)?[Tt]ape[-_ \t]*[Ll]abels[-_ \t]*[Cc]o?mm?a?n?d:?[ \t]*",
		TypeUCharPTR	},
	{ &lslabelsdrive_cmd, NULL,
	(UChar *) "^[ \t]*\\([Ll]ist[ \t]*\\)?[Tt]ape[-_ \t]*[Ll]abels[-_ \t]*\\([Ii][nN][-_ \t]*\\)?[Dd]rives?[-_ \t]*[Cc]o?mm?a?n?d:?[ \t]*",
		TypeUCharPTR	},
	{ &lslabelsslot_cmd, NULL,
	(UChar *) "^[ \t]*\\([Ll]ist[ \t]*\\)?[Tt]ape[-_ \t]*[Ll]abels[-_ \t]*\\([Ii][nN][-_ \t]*\\)?[Ss]lots?[-_ \t]*[Cc]o?mm?a?n?d:?[ \t]*",
		TypeUCharPTR	},
	{ &lslabelsloadport_cmd, NULL,
	(UChar *) "^[ \t]*\\([Ll]ist[ \t]*\\)?[Tt]ape[-_ \t]*[Ll]abels[-_ \t]*\\([Ii][nN][-_ \t]*\\)?[Ll]oad[-_ \t]*[Pp]orts?[-_ \t]*[Cc]o?mm?a?n?d:?[ \t]*",
		TypeUCharPTR	},
};

/* the indexes of the following 2 must correspond to the macros LOC_XXX above */
static UChar	*loc_re_strings[] = {
		"[Dd][Rr][Ii][Vv][Ee]", "[Ss][Ll][Oo][Tt]",
		"[Ll][Oo][Aa][Dd][-_ \t]*\\([Pp][Oo][Rr][Tt]\\|[Bb][Aa][Yy]\\)",
};
static UChar	*loc_strings[] = {
		TN_("drive"), TN_("slot"), TN_("loadport"),
};

UChar		*default_configfilenames[] = {	\
			DEFAULT_SERVER_CONFIGFILES, NULL, NULL };

ParamFileEntry	entries[] = {
	{ &devicesstr, NULL,
	(UChar *) "[Bb]ackup[-_ \t]*[Dd]evi?c?e?:?[ \t]*",
		TypeUCharPTR	},
	{ &devicelock.lockfile, NULL,
	(UChar *) "^[ \t]*[Ll]ocki?n?g?[-_ \t]*[Ff]ile:?[ \t]*",
		TypeUCharPTR	},
	{ &changerlock.lockfile, NULL,
	(UChar *) "^[ \t]*[Cc][Hh][Aa][Nn][Gg][Ee][Rr][-_ \t]*[Ll][Oo][Cc][Kk]\\([Ii][Nn][Gg]\\)?[-_ \t]*[Ff][Ii][Ll][Ee]:?[ \t]*",
		TypeUCharPTR	},
	{ &tapeblocksize, NULL,
	(UChar *) "[Tt]ape[-_ \t]*[Bb]lock[-_ \t]*[Ss]ize:?",
		TypeInt32	},
	{ &setfilecmd, NULL,
	(UChar *) "[Ss]et[-_ \t]*[Ff]ile[-_ \t]*[Cc]o?mm?a?n?d:?[ \t]*",
		TypeUCharPTR	},
	{ &num_cartridges, NULL,
	(UChar *) "[Nn]umb?e?r?[-_ \t]*[Oo]f[-_ \t]*[Cc]artr?i?d?g?e?s[-_ \t]*:?",
		TypeInt32	},
	{ &vardir, NULL,
	(UChar *) "^[ \t]*[Vv][Aa][Rr][-_ \t]*[Dd]ire?c?t?o?r?y?:?[ \t]*",
		TypeUCharPTR	},
	{ &user_to_inform, NULL,
	(UChar *) "^[ \t]*[Uu]ser[-_ \t]*[Tt]o[-_ \t]*[Ii]nfor?m?:?[ \t]*",
		TypeUCharPTR	},
	{ &mail_program, NULL,
	(UChar *) "^[ \t]*[Mm]ail[-_ \t]*[Pp]rogram:?[ \t]*",
		TypeUCharPTR	},
	{ &changerconffile, NULL,
	(UChar *) "^[ \t]*[Cc][Hh][Aa][Nn][Gg][Ee][Rr][-_ \t]*[Cc][Oo][Nn][Ff][Ii][Gg]\\([Uu][Rr][Aa][Tt][Ii][Oo][Nn]\\)?[-_ \t]*\\([Ff][Ii][Ll][Ee][-_ \t]*\\)?:?[ \t]*",
		TypeUCharPTR	},
	{ &tapeposfile, NULL,
	(UChar *) "^[ \t]*[Tt]ape[-_ \t]*[Pp]osi?t?i?o?n?[-_ \t]*[Ff]ile:?[ \t]*",
		TypeUCharPTR	},
	{ &cartins_gracetime, NULL,
	(UChar *) "^[ \t]*[Cc]artr?i?d?g?e?[-_ \t]*[Ii]nse?r?t?[-_ \t]*[Gg]race[-_ \t]*[Tt]ime:?",
		TypeUns32	},
	{ &devunavail_send_mail, NULL,
	(UChar *) "^[ \t]*[Dd]evi?c?e?[-_ \t]*[Uu]n[Aa]vaila?b?l?e?[-_ \t]*[Ss]end[-_ \t]*[Mm]ail[-_ \t]*\\([Aa]fter[-_ \t]*\\)?[Mm]inu?t?e?s?:?",
		TypeUns32	},
	{ &devunavail_give_up, NULL,
	(UChar *) "^[ \t]*[Dd]evi?c?e?[-_ \t]*[Uu]n[Aa]vaila?b?l?e?[-_ \t]*[Gg]ive[-_ \t]*[Uu]p[-_ \t]*\\([Aa]fter[-_ \t]*\\)?[Mm]inu?t?e?s?:?",
		TypeUns32	},
	{ &loggingfile, NULL,
	(UChar *) "^[ \t]*[Ll]ogg?i?n?g?[-_ \t]*[Ff]ile:?[ \t]*",
		TypeUCharPTR	},
	};

UChar	*lockdirs[] = DEFAULT_LOCKDIRS;

int		waitlockfd = -1;

#define	EM__(nmem)	{ nomemfatal(nmem); }
#define	ER__(func, i)	{ if( (i = func) ) return(i); }
#define	EEM__(st)	{ if(st) nomemfatal(NULL); }

#define	repl_dirs(string)	repl_substrings((string), replacements,	\
		sizeof(replacements) / sizeof(replacements[0]))

static	int	cart_ctl(Int32, UChar **);
static	int	label_tape(Int32, UChar **);
static	int	cart_is(Int32, UChar **);
static	void	report_problem(UChar *, ...);
static	Int32	move_cartridge_to(Int32, Int32, UChar *, Int32);

void
logmsg(int prio, UChar * fmt, ...)
{
  va_list	args;
  FILE		*fp, *lfp = NULL;

  if(loggingfile)
    lfp = fopen(loggingfile, "a");

  fp = (lfp ? lfp : stderr);

  va_start(args, fmt);
  fprintf(fp, "%s, ", actimestr());
  vfprintf(fp, fmt, args);
  fflush(fp);
  va_end(args);

  if(lfp)
    fclose(lfp);

  if(syslog_ident){
    va_start(args, fmt);
    gvsyslog(syslog_ident, LOG_CONS | LOG_PID, LOG_DAEMON, prio, fmt, args);
    va_end(args);
  }
}

void
release_lock(LockData * the_lock)
{
  if(the_lock->locked == BU_GOT_LOCK){
    close(the_lock->lockfd);

    unlink(the_lock->lockfile);

    the_lock->lockfd = -1;
    the_lock->locked = BU_NO_LOCK;
  }
}

void
do_exit(int exitst)
{
  release_lock(&devicelock);
  release_lock(&changerlock);

  exit(exitst);
}

void
fatal(UChar * msg)
{
  fprintf(stderr, "%s", msg);

  do_exit(1);
}

void
nomemfatal(void * ptr)
{
  if(!ptr)
    fatal(T_("Error: Cannot allocate memory.\n"));
}

Int32
message_to_operator(FILE * mfp, Flag issue_clientmsg)
{
  FILE		*pp = NULL;
  UChar		*mp, *cptr, *cptr2, msg_buf[1024];
  Int32		r = 0, n;
  int		fd;
  time_t	starttime, time_now;
  struct stat	statb;

  forever{		/* pseudo-loop for easily skipping code with break */
    if(strlen(remoteuser) > 0){
      mp = repl_substring(mail_program, "%U", remoteuser);
      if(!mp){
	r |= 1;
	break;
      }
    }
    else{
      mp = strdup(mail_program);
      if(!mp){
	r |= 1;
	break;
      }

      while( (cptr = strstr(mp, "%U")) ){	/* remove entire word with %U */
	cptr2 = first_space(cptr);
	cptr2 = first_nospace(cptr2);
	while(cptr > mp && !isspace(*(cptr - 1)))
	  cptr--;
	memmove(cptr, cptr2, (strlen(cptr2) + 1) * sizeof(UChar));
      }
    }

    if(strlen(clientuname) > 0){
      cptr = repl_substring(mp, "%H", clientuname);
      free(mp);
      mp = cptr;
      if(!mp){
	r |= 1;
	break;
      }
    }
    else{
      while( (cptr = strstr(mp, "%H")) ){	/* remove entire word with %H */
	cptr2 = first_space(cptr);
	cptr2 = first_nospace(cptr2);
	while(cptr > mp && !isspace(*(cptr - 1)))
	  cptr--;
	memmove(cptr, cptr2, (strlen(cptr2) + 1) * sizeof(UChar));
      }
    }

    pp = popen(mp, "w");
    free(mp);

    break;	/* always break */
  }

  fd = -1;
  starttime = time(NULL);
  while(issue_clientmsg){
	if(!stat(client_message_file, &statb)){
	  time_now = time(NULL);
	  if(statb.st_mtime >= time_now){
	    ms_sleep((statb.st_mtime - time_now + 1) * 1000);
	    continue;
	  }
	}	/* in fact we have a race here, but this is uncricital */

	fd = set_wlock(client_message_file);
	if(fd >= 0){
	  ftruncate(fd, 0);
	  break;
	}

	if(time(NULL) > starttime + 60){
	  r |= 2;
	  break;
	}

	ms_sleep(200 + (Int32)(drandom() * 500.0));
  }

  fseek(mfp, 0, SEEK_SET);

  while((n = fread(msg_buf, 1, 1024, mfp)) > 0){
    if(fd >= 0)
	write_forced(fd, msg_buf, n);
    if(pp)
	fwrite(msg_buf, 1, n, pp);
  }

  fclose(mfp);

  if(fd >= 0)
    close(fd);
  if(pp)
    pclose(pp);

  return(r);
}

Int32
set_lock(LockData * the_lock, Flag optional)
{
  struct stat	statb;
  int		i;
  char		buf[20];
  struct flock	lockb;

  if(the_lock->locked == BU_GOT_LOCK)
    return((Int32) the_lock->locked);

  i = lstat(the_lock->lockfile, &statb);
  if(!i && !IS_REGFILE(statb)){
    if(unlink(the_lock->lockfile)){
	fprintf(stderr, T_("Error: Cannot remove lock file entry `%s', that is not a file.\n"), the_lock->lockfile);
	return( (Int32) (the_lock->locked = BU_CANT_LOCK) );
    }

    i = 1;
  }

  if(!i){
    the_lock->lockfd = open(the_lock->lockfile, O_WRONLY | O_SYNC);
    if(the_lock->lockfd < 0){
      fprintf(stderr, T_("Warning: Lock file `%s' exists, but can't open it.\n"),
			the_lock->lockfile);
      return( (Int32) (the_lock->locked = BU_CANT_LOCK) );
    }
  }
  else{
    the_lock->lockfd = open(the_lock->lockfile, O_WRONLY | O_CREAT | O_SYNC, 0644);

    if(the_lock->lockfd < 0){
      fprintf(stderr, "%s: %s `%s'.\n",
			optional ? T_("Warning") : T_("Error"),
			T_("Cannot create lock file"), the_lock->lockfile);
      return( (Int32) (the_lock->locked = BU_CANT_LOCK) );
    }
  }

  SETZERO(lockb);
  lockb.l_type = F_WRLCK;
  i = fcntl(the_lock->lockfd, F_SETLK, &lockb);
  if(i){
    if(!optional || interactive)
	fprintf(stderr, "%s: %s `%s'.\n",
			optional ? T_("Warning") : T_("Error"),
			T_("Cannot lock file"), the_lock->lockfile);
    return( (Int32) (the_lock->locked = BU_LOCKED) );
  }

  sprintf(buf, "%d\n", (int) getpid());
  write(the_lock->lockfd, buf, strlen(buf));

  return( (Int32) (the_lock->locked = BU_GOT_LOCK) );
}

static void
set_devices(Int32 idx)
{
  devicename = streamers[idx].devicename;

  if(streamers[idx].changer){
    changername = streamers[idx].changer->changername;
    pos_in_changer = streamers[idx].pos_in_changer + 1;
    num_changer_slots = streamers[idx].changer->num_slots;
    num_changer_loadports = streamers[idx].changer->num_loadports;
  }
  else{
    changername = NULL;
    pos_in_changer = num_changer_slots = num_changer_loadports = 0;
  }
}

Int32
find_tape_blocksize(UChar * devicen, UChar ** read_block)
{
  static Int32	usual_blocksizes[] = { USUAL_BLOCKSIZES };

  int		fd;
  UChar		*buf = NULL;
  Int32		*tryable_blocksizes = NULL;
  Int32		found_blocksize, i, j, n, n_usual;

  n_usual = sizeof(usual_blocksizes) / sizeof(usual_blocksizes[0]);
  found_blocksize = -1;
  fd = -1;

  tryable_blocksizes = get_list_from_int_list_file(used_blocksizes_file, &n);
  if(tryable_blocksizes){
    tryable_blocksizes = RENEWP(tryable_blocksizes, Int32, 1 + n + n_usual);
    memmove(tryable_blocksizes + 1, tryable_blocksizes, sizeof(Int32) * n);
  }
  else{
    tryable_blocksizes = NEWP(Int32, 1 + n_usual);
    n = 0;
  }
  if(!tryable_blocksizes)
    CLEANUP;

  tryable_blocksizes[0] = tapeblocksize;
  memcpy(tryable_blocksizes + n + 1, usual_blocksizes, sizeof(Int32) * n_usual);
  n += n_usual + 1;

  for(i = 0; i < n; i++){
    for(j = i + 1; j < n; j++){		/* remove duplicates */
	if(tryable_blocksizes[i] == tryable_blocksizes[j]){
	  memmove(tryable_blocksizes + j, tryable_blocksizes + j + 1,
			(n - j - 1) * sizeof(tryable_blocksizes[0]));
	  n--;
	  j--;
	}
    }
  }

  for(i = 0; i < n; i++){
    buf = ZRENEWP(buf, UChar, tryable_blocksizes[i]);
    if(!buf)
	CLEANUP;

    if(!IS_REGFILE(devstatb) && !IS_DIRECTORY(devstatb)){
	j = system(setfilecmd);
	if(j < 0 || WEXITSTATUS(j))
	  return(-ABS(j));
    }

    fd = open(devicen, O_RDONLY);
    if(fd < 0)
	CLEANUP;

    j = read(fd, buf, tryable_blocksizes[i]);
    if(j > 0){
	found_blocksize = j;
	if(read_block){
	  *read_block = buf;
	  buf = NULL;
	}

	break;
    }

    close(fd);
    fd = -1;
  }

 cleanup:
  if(fd >= 0)
    close(fd);
  ZFREE(tryable_blocksizes);
  ZFREE(buf);

  return(found_blocksize);
}


Int32
set_lock_msg(LockData * lockdata, Flag optional, Flag wait_for_lock)
{
  Int32		res;

  while((res = set_lock(lockdata, optional)) != BU_GOT_LOCK){
    if(!wait_for_lock){
      if(res != BU_LOCKED){
	report_problem(T_("Error: Cannot set lock on file `%s'.\n"),
				lockdata->lockfile);
	exit(5);
      }

      if(!optional){
	report_problem("%s: %s.\n %s",
		T_("Error"),
		T_("An application using the device seems to be running"),
		T_("(The application's lock may be overridden using the flag -F)\n"));
	exit(5);
      }
      else{
	if(interactive)
	  report_problem("%s: %s.\n",
		T_("Warning"),
		T_("An application using the device seems to be running"));
      }
      break;
    }

    /* TODO: handle timeouts */

    ms_sleep(3 * 1000);
  }

  if(res != BU_GOT_LOCK && !optional){
    report_problem(T_("Error: Cannot set lock on lockfile. Device seems to be already in use by the afbackup service.\n"));
  }

  return(res);
}

static Int32
reuse_tapes(UChar * str, Flag keep_ro)
{
  UChar		**lines = NULL, *cptr, nbuf[32], *w3ptr;
  Uns32Range	*tapes_to_reuse = NULL, *acttapes = NULL, *rgptr;
  Int32		r = 0, i, w, num_entries = 0, num_lines = 0;
  int		lockfd, i1;
  struct stat	statb;
  KeyValuePair	*allentries = NULL;

  lockfd = -1;

  massage_string(str);

  tapes_to_reuse = sscan_Uns32Ranges__(str, 1, num_cartridges, NULL, NULL);
  if(!tapes_to_reuse)
    return(-1);

  /* remove from precious tapes file */

  allentries = kfile_getall(precious_tapes_file, &num_entries, KFILE_LOCKED);
  if(allentries){
    for(i = 0; i < num_entries; i++){
      w = word_count(allentries[i].value);
      if(w < 1){
#if 0	/* don't complain, just throw out */
	logmsg(LOG_WARNING, T_("Warning: Unrecognized line in file `%s': %s ...\n"),
				precious_tapes_file, allentries[i].key);
#endif
	kfile_delete(precious_tapes_file, allentries[i].key, KFILE_LOCKED);
	continue;
      }

      ZFREE(acttapes);
      acttapes = sscan_Uns32Ranges__(allentries[i].value, 1, num_cartridges, NULL, NULL);
      if(!acttapes)
	CLEANUPR(-3);

      if(!overlap_Uns32Ranges(acttapes, tapes_to_reuse))
	continue;

      acttapes = del_range_from_Uns32Ranges(acttapes, tapes_to_reuse);
      if(!acttapes)
	CLEANUPR(-4);

      if(len_Uns32Ranges(acttapes) > 0){
	cptr = str_Uns32Ranges(acttapes, 0);
	if(!cptr)
	  CLEANUPR(-5);

	r = kfile_insert(precious_tapes_file, allentries[i].key, cptr,
				KFILE_SORT | KFILE_LOCKED);
	free(cptr);
      }
      else{
	r = kfile_delete(precious_tapes_file, allentries[i].key,
				KFILE_LOCKED);
      }
      if(r){
	logmsg(LOG_ERR, T_("Error: Cannot rewrite file `%s' to save the precious tapes"),
			precious_tapes_file);
	CLEANUP;
      }
    }

    kfile_freeall(allentries, num_entries);
  }

  if(!keep_ro){		/* remove from read only tapes file */
    if((lockfd = set_wlock_forced(ro_tapes_file)) < 0)
	CLEANUPR(-1);

    lines = read_asc_file(ro_tapes_file, & num_lines);
    if(!lines)
	CLEANUPR(-2);

    for(i = 0; i < num_lines; i++){
	ZFREE(acttapes);

	acttapes = sscan_Uns32Ranges__(lines[i], 1, num_cartridges, NULL, NULL);
	if(!acttapes)
	  CLEANUPR(-7);

	acttapes = del_range_from_Uns32Ranges(acttapes, tapes_to_reuse);
	if(!acttapes)
	  CLEANUPR(-8);

	free(lines[i]);
	if(len_Uns32Ranges(acttapes) > 0){
	  lines[i] = str_Uns32Ranges(acttapes, 0);
	}
	else{
	  memmove(lines + i, lines + i + 1, sizeof(UChar *) * (num_lines - i));
	  num_lines--;
	  i--;
	}
    }

    i = stat(ro_tapes_file, &statb);
    if(write_asc_file_safe(ro_tapes_file, lines, 0)){
	fprintf(stderr,
		T_("Error: Cannot rewrite file `%s' to save the read-only tapes\n"),
			ro_tapes_file);
    }
    if(i)
      chown(ro_tapes_file, default_stat.st_uid, default_stat.st_gid);
    free_asc_file(lines, 0);
    lines = NULL;
    close(lockfd);
    lockfd = -1;
  }

  /* remove from cartridge order file */

  allentries = kfile_getall(cartorder_file, &num_entries, KFILE_LOCKED);
  if(allentries){
    for(i = 0; i < num_entries; i++){
      cptr = strstr(allentries[i].value, "->");
      if(cptr && cptr == allentries[i].value){
	cptr = first_nospace(cptr + 2 /* strlen("->") */);
	w = sscanf(cptr, "%d", &i1);
	if(w > 0){
	  if(in_Uns32Ranges(tapes_to_reuse, (Int32) i1)){
	    *(cptr++) = '0';
	    while(isdigit(*cptr) && *cptr){
		*(cptr++) = ' ';
	    }

	    r = kfile_insert(cartorder_file, allentries[i].key,
			allentries[i].value, KFILE_SORTN | KFILE_LOCKED);
	    if(r){
		logmsg(LOG_ERR, T_("Error: Cannot rewrite file `%s' to save the cartridge order"),
			cartorder_file);
		CLEANUP;
	    }
	  }
	}
      }
    }
  }

  for(rgptr = tapes_to_reuse; rgptr->first <= rgptr->last; rgptr++){
    for(i = rgptr->first; i <= rgptr->last; i++){
	sprintf(nbuf, "%d:", (int) i);
	cptr = kfile_get(bytesontape_file, nbuf, KFILE_LOCKED);
	if(cptr){
	  w3ptr = strword(cptr, 3);
	  free(cptr);
	  EM__(cptr = strapp("0 0 0 ", w3ptr ? w3ptr : (UChar *) "0"));
	  ZFREE(w3ptr);
	  kfile_insert(bytesontape_file, nbuf, cptr, KFILE_LOCKED);
	  free(cptr);
	}
	else{
	  kfile_delete(bytesontape_file, nbuf, KFILE_LOCKED);
	}
    }
  }

 cleanup:
  if(lockfd >= 0)
    close(lockfd);

  ZFREE(tapes_to_reuse);
  ZFREE(acttapes);
  kfile_freeall(allentries, num_entries);
  free_asc_file(lines, 0);

  return(r);
}

static Int32		/* convert to new format */
rewrite_tapeposfile()
{
  FILE		*fp;
  int		i1, i2;
  UChar		numstr[64];
  KeyValuePair	*tapeposfile_entries;
  Int32		num_tapeposfile_entries, i, r;

  fp = fopen(tapeposfile, "r");
  if(fp){
    if(fscanf(fp, "%d%d", &i1, &i2) >= 2){
      EM__(tapeposfile_entries = NEWP(KeyValuePair, 1));
      EM__(tapeposfile_entries[0].key = strdup(devicename));
      sprintf(numstr, "%d %d", i1, i2);
      EM__(tapeposfile_entries[0].value = strdup(numstr));
      num_tapeposfile_entries = 1;

      while(fscanf(fp, "%d%d", &i1, &i2) >= 2){
	EM__(tapeposfile_entries = RENEWP(tapeposfile_entries,
			KeyValuePair, num_tapeposfile_entries + 1));
	sprintf(numstr, "%d:", (int) num_tapeposfile_entries - 1 + 1);
	EM__(tapeposfile_entries[num_tapeposfile_entries].key = strdup(numstr));
	sprintf(numstr, "%d %d", i1, i2);
	EM__(tapeposfile_entries[num_tapeposfile_entries].value = strdup(numstr));
	num_tapeposfile_entries++;
      }

      fclose(fp);
      truncate(tapeposfile, 0);
      for(i = 0; i < num_tapeposfile_entries; i++){
	ER__(kfile_insert(tapeposfile, tapeposfile_entries[i].key,
		tapeposfile_entries[i].value, KFILE_SORTN | KFILE_LOCKED), r);
      }

      kfile_freeall(tapeposfile_entries, num_tapeposfile_entries);
    }
    else
	fclose(fp);
  }

  return(0);
}

static Int32
rewrite_tapeposfile_msg()
{
  Int32		r;

  if( (r = rewrite_tapeposfile()) )
    fprintf(stderr, T_("Error: Rewriting the tapepos-file failed.\n"));

  return(r);
}

static Int32
reset_in_tapepos(Int32 cartno)
{
  KeyValuePair	*tapeposfile_entries;
  Int32		num_tapeposfile_entries, i, r;
  int		i1, i2;

  ER__(rewrite_tapeposfile_msg(), r);

  tapeposfile_entries = kfile_getall(tapeposfile, &num_tapeposfile_entries,
				KFILE_LOCKED);

  for(i = 0; i < num_tapeposfile_entries; i++){
    if(sscanf(tapeposfile_entries[i].value, "%d%d", &i1, &i2) >= 2){
      if(i1 == cartno){
	sprintf(tapeposfile_entries[i].value, "%d %d", i1, 1);

	ER__(kfile_insert(tapeposfile, tapeposfile_entries[i].key,
		tapeposfile_entries[i].value, KFILE_SORTN | KFILE_LOCKED), r);
      }
    }
  }

  kfile_freeall(tapeposfile_entries, num_tapeposfile_entries);

  return(0);
}

static Int32
delete_tape(Int32 cartno)
{
  char	buf[32];

  sprintf(buf, "%d", (int) cartno);

  return(reuse_tapes(buf, NO));
}

Uns32Range *
get_readonly_tapes()
{
  Uns32Range	*alltapes, *newtapes = NULL;
  FILE		*fp = NULL;
  UChar		*line = NULL, *rangestr = NULL;
  int		fd;

  alltapes = empty_Uns32Ranges();
  if(!alltapes)
    return(NULL);

  fd = set_rlock_forced(ro_tapes_file);
  if(fd < 0)
    CLEANUP;
  fp = fopen(ro_tapes_file, "r");
  if(fp){
    while(!feof(fp)){
	ZFREE(line);
	line = fget_alloc_str(fp);
	if(!line)
	  continue;

	ZFREE(rangestr);
	rangestr = strdup(line);
	if(!rangestr)
	  GETOUT;

	newtapes = sscan_Uns32Ranges__(rangestr, 1, num_cartridges,
							NULL, NULL);
	if(!newtapes)
	  break;

	merge_Uns32Ranges(&alltapes, newtapes);
	ZFREE(newtapes);
    }
  }

 cleanup:
  if(fd >= 0)
    close(fd);
  if(fp)
    fclose(fp);

  ZFREE(newtapes);
  ZFREE(rangestr);
  ZFREE(line);

  return(alltapes);

 getout:
  ZFREE(alltapes);
  CLEANUP;
}

static UChar *
select_lockfile(UChar * lfbasen)
{
  UChar		**cpptr, *lockf = NULL;
  struct stat	statb;

  for(cpptr = lockdirs; *cpptr; cpptr++){
    if(!stat(*cpptr, &statb)){
	if(! eaccess(*cpptr, W_OK)){
	  lockf = strchain(*cpptr, FN_DIRSEPSTR, lfbasen, NULL);
	  break;
	}
    }
  }

  if(!lockf){
    fprintf(stderr,
	T_("Error: Cannot find an appropriate directory for lockfile `%s'.\n"),
		lfbasen);
  }

  return(lockf);
}

static void
usage(char * pnam, Int32 mode)
{
  pnam = FN_BASENAME((UChar *) pnam);

  switch(mode){
   case MODE_LABEL_TAPE:
    fprintf(stderr, T_("usage: %s <label-number> [ -rfF ] \\\n"), pnam);
    fprintf(stderr, T_("                  [ -c <configfile> ] \\\n"));
    fprintf(stderr, T_("                  [ -S <sec-label-number> ] \\\n"));
    fprintf(stderr, T_("                  [ -d <devicename> ] \\\n"));
    fprintf(stderr, T_("                  [ -b <blocksize> ] \\\n"));
    fprintf(stderr, T_("                  [ -s <set-file-cmd> ] \\\n"));
    fprintf(stderr, T_("                  [ -C <num-cartridges> ] \\\n"));
    fprintf(stderr, T_("                  [ -n <comment> ]\n"));
    fprintf(stderr, "       %s -q \\\n", pnam);
    fprintf(stderr, T_("                  [ -c <configfile> ] \\\n"));
    fprintf(stderr, T_("                  [ -d <devicename> ] \\\n"));
    fprintf(stderr, T_("                  [ -b <blocksize> ] \\\n"));
    fprintf(stderr, T_("                  [ -s <set-file-cmd> ] \\\n"));

    break;

   case MODE_CART_CTL:
    fprintf(stderr, T_("usage: %s [ -ilmtefFrN ] \\\n"), pnam);
    fprintf(stderr, T_("                  [ -P [ <location> ] ] \\\n"));
    fprintf(stderr, T_("                  [ -s <serverconfigfile> ] \\\n"));
    fprintf(stderr, T_("                  [ -c <changerconfigfile> ] \\\n"));
    fprintf(stderr, T_("                  [ -d <changerdevice> ] \\\n"));
    fprintf(stderr, T_("                  [ -C <cartridges> ] \\\n"));
    fprintf(stderr, T_("                  [ -S [ <slots> ] ] \\\n"));
    fprintf(stderr, T_("                  [ -D [ <drives> ] ] \\\n"));
    fprintf(stderr, T_("                  [ -L [ <loadports> ] ] \\\n"));
    fprintf(stderr, T_("                  [ -a [ <cartridge-attribute-specs> ] ] \\\n"));
    fprintf(stderr, T_("                  [ -b <blocksize> ] \\\n"));
    fprintf(stderr, T_("                  [ -n <comment> ] \\\n"));
    fprintf(stderr, T_("                  [ <cartridge-descriptions> ... ]\n"));

    break;

   case MODE_CART_IS:
    fprintf(stderr, T_("usage: %s { <cartridge-number> | \\\n"), pnam);
    fprintf(stderr, T_("               -i <cartridge-number> <tapefile-number> } \\\n"));
    fprintf(stderr, T_("               [ -S <cartridge-set> ]\n"));

    break;
  }

  exit(1);
}

Int32
get_prog_options(int argc, char ** argv, Int32 mode)
{
  Int32		i;

  switch(mode){
    case MODE_LABEL_TAPE:
	i = goptions(-argc, (UChar **) argv,
		"s:c;i-1:;s:d;i:b;s:s;i:C;b:q;s:n;i:S;b:r;b:f;b:F",
		&configfile, &label_given, &label, &devicename, &tapeblocksize,
		&setfilecmd, &num_cartridges, &query, &comment, &scnd_label,
		&delfromhd, &dont_ask, &forced);
	if(i || (!label_given && !query) || (label_given && query))
	  usage(argv[0], mode);

	break;

    case MODE_CART_CTL:
	have_loc_arg = have_loadp_arg = have_drive_arg = have_slot_arg = -1;
	i = goptions(-argc, (UChar **) argv,
		"s:s;b:i;b:m;b:l;b:t;b:e;s-1:P;s-1:S;s-1:L;s-1:D;s:C;s:d;"
		"b:F;b:f;s:c;s:n;i:b;b:r;b:a;b:N;*",
		&configfile, &cctl_inventory, &cctl_move, &cctl_list,
		&cctl_label, &cctl_eject, &have_loc_arg, &locstr,
		&have_slot_arg, &slotstr,
		&have_loadp_arg, &loadportstr,
		&have_drive_arg, &drivestr,
		&cartstr, &given_changer, &forced, &dont_ask,
		&changerconffile, &comment, &tapeblocksize, &delfromhd,
		&cctl_attrs, &cctl_names, &num_cartnames, &cartnames);
	if(i)
	  usage(argv[0], mode);

	if(cctl_list){
	  list_mode = LIST_CARTLOCS;
	  if(cctl_inventory){
	    list_mode |= LIST_HWINV;
	    cctl_inventory = NO;
	  }
	  if(cctl_names){
	    list_mode |= LIST_CARTNAMES;
	    cctl_names = NO;
	  }
	  if(cctl_attrs){
	    list_mode |= LIST_CARTATTRS;
	    cctl_attrs = NO;
	  }
	  if(delfromhd){
	    list_mode |= LIST_CARTRESRVS;
	    delfromhd = NO;
	  }
	}
	if(cctl_inventory && cctl_names){
	  cctl_invlbl = YES;
	  cctl_inventory = NO;
	}

	break;

    case MODE_CART_IS:
	i = goptions(-argc, (UChar **) argv,
		"s:c;s:d;i2:i;i-1:;i:S",
		&configfile, &devicename, &cartis_wcart, &cartis_wfile,
		&have_cartis_cart, &cartis_cart, &cartridge_set);
	if(i)
	  usage(argv[0], mode);

	break;

  }

  remoteuser = getenv(REMOTEUSERENVVAR);
  if(!remoteuser)
    remoteuser = "";
  clientuname = getenv(REMOTEHOSTENVVAR);
  if(!clientuname)
    clientuname = "";

  return(0);
}

main(int argc, char ** argv)
{
  Int32		i;
  FILE		*fp;
  struct stat	statb;
  UChar		*backuphome, **cpptr, *line = NULL, *cptr;

#ifdef ENABLE_NLS
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);
#endif
  
  programfile = find_program(argv[0]);
  if(!programfile){
    cptr = FN_LASTDIRDELIM(argv[0]);
    if(cptr)
	cptr++;
    else
	cptr = argv[0];
    EM__(programfile = strapp(DEFSERVBINDIR FN_DIRSEPSTR, cptr));
    if(eaccess(programfile, X_OK)){
	fprintf(stderr, T_("Error: Cannot find program file of `%s'"), argv[0]);
	exit(4);
    }
  }

  cptr = FN_BASENAME((UChar *) argv[0]);
  for(i = 0; i < sizeof(cmd_res) / sizeof(*cmd_res); i++){
    if(re_find_match_once(cmd_res[i], cptr, NULL, NULL) >= 0){
	mode = i;
	break;
    }
  }
  if(mode < 0){
    fprintf(stderr, T_("Error: Command name %s not recognized.\n"), cptr);
    do_exit(1);
  }

  interactive = isatty(0);

  get_prog_options(argc, argv, mode);

  if(scnd_label == -1)
    scnd_label = scnd_label_arg = -2;	/* no 2nd label given */

  cptr = mkabspath(programfile, NULL);
  programdir = resolve_path__(cptr, NULL);
  free(cptr);

  SETZERO(default_stat);

  if(!stat(programdir, &statb))
    COPYVAL(default_stat, statb);
  else
    fprintf(stderr, T_("Warning: Cannot find program file of `%s'"), argv[0]);

  /* determine home directory */
  backuphome = getenv("BACKUP_HOME");
  if(!backuphome){
     /* construct file- and dirnames */

#ifdef	ORIG_DEFAULTS

    backuphome = programdir;

    if( (cptr = FN_LASTDIRDELIM(programdir)) )
	*(cptr) = '\0';
    if( (cptr = FN_LASTDIRDELIM(programdir)) )
	*(cptr) = '\0';
    bindir = strapp(programdir, FN_DIRSEPSTR "bin");
    libdir = strapp(programdir, FN_DIRSEPSTR "lib");
    confdir = strapp(programdir, FN_DIRSEPSTR "etc");

#else	/* defined(ORIG_DEFAULTS) */

     bindir = strdup(DEFSERVBINDIR);
     libdir = strdup(DEFSERVLIBDIR);
     confdir = strdup(DEFSERVCONFDIR);

#endif	/* if else defined(ORIG_DEFAULTS) */

  }
  else {
     EM__(backuphome = strdup(backuphome));
     /* construct file- and dirnames */
     bindir = strapp(backuphome, FN_DIRSEPSTR "bin");
     libdir = strapp(backuphome, FN_DIRSEPSTR "lib");
     confdir = strapp(backuphome, FN_DIRSEPSTR "etc");
  }

  if(!bindir || !libdir || !confdir)
    nomemfatal(NULL);

  if(!configfile){
    for(cpptr = default_configfilenames; *cpptr; cpptr++);
    EM__(*cpptr = strapp(confdir, FN_DIRSEPSTR DEFSERVERCONF));

    for(cpptr = default_configfilenames; *cpptr; cpptr++){
      configfile = *cpptr;
      if(!stat(*cpptr, &statb) && eaccess(*cpptr, R_OK)){
	while(*cpptr) cpptr++;
	break;
      }
      if(!stat(*cpptr, &statb))
	break;
    }

    configfile = *cpptr;
  }

  if(!configfile){
    fp = fopen("/etc/inetd.conf", "r");
    if(fp){
      forever{
	if(line)
	  ZFREE(line);

	line = fget_alloc_str(fp);
	if(!line)
	  break;

	if(strstr(line, bindir)){
	  cptr = first_nospace(line);
	  if(*cptr == '#')
	    continue;

	  cptr = line + strlen(line) - 1;
	  while(isspace(*cptr) && cptr > line)
	    cptr--;
	  *(cptr + 1) = '\0';
	  while(!isspace(*cptr) && cptr > line)
	    cptr--;
	  cptr++;

	  if(!eaccess(cptr, R_OK))
	    EM__(configfile = strdup(cptr));

	  break;
	}
      }
      fclose(fp);
    }
    else{
     fp = fopen("/etc/xinetd.conf", "r");
     if(fp){
      forever{
	if(line)
	  ZFREE(line);

	line = fget_alloc_str(fp);
	if(!line)
	  break;

	if(strstr(line, bindir) && re_find_match_once("^[ \t]*server_args[ \t]*=", line, NULL, NULL)){
	  cptr = first_nospace(line);
	  if(*cptr == '#')
	    continue;

	  cptr = line + strlen(line) - 1;
	  while(isspace(*cptr) && cptr > line)
	    cptr--;
	  *(cptr + 1) = '\0';
	  while(!isspace(*cptr) && cptr > line)
	    cptr--;
	  cptr++;

	  if(!eaccess(cptr, R_OK))
	    EM__(configfile = strdup(cptr));

	  break;
	}
      }
      fclose(fp);
     }
    }
  }

  if(!configfile){
    fprintf(stderr, T_("Warning: Cannot determine server side configuration file, using defaults.\n"));
    configfile = "<configuration file not specified>";
  }
  else{
    i = read_param_file(configfile, entries,
		sizeof(entries) / sizeof(entries[0]), NULL, NULL);
    if(i){
      fprintf(stderr, T_("Error: Cannot read configuration file `%s'.\n"),
				configfile);
      exit(3);
    }
  }

  get_prog_options(argc, argv, mode);

  if(devicelock.lockfile)
    if(empty_string(devicelock.lockfile))
      devicelock.lockfile = NULL;
  if(vardir)
    if(empty_string(vardir))
      vardir = NULL;
  if(devicelock.lockfile)
    massage_string(devicelock.lockfile);
  if(vardir)
    massage_string(vardir);
  if(!setfilecmd)
    EM__(setfilecmd = strdup(DEFAULT_SETFILECMD));
  if(devicesstr)
    if(empty_string(devicesstr))
	ZFREE(devicesstr);
  if(!devicename){
    if(!devicesstr)
	EM__(devicesstr = strdup(DEFAULT_TAPE_DEVICE));

    if(devicesstr){
	if( (i = devices_from_confstr_msg(devicesstr, &streamers, &num_streamers,
			&changers, &num_changers, stderr)) )
	  exit(27);
	set_devices(curdevidx);
    }
  }

  if(strlen(comment) > 256){
    fprintf(stderr,
		T_("Warning: Comment longer than 256 characters, truncated.\n"));
    comment[256] = '\0';
  }

  if(!vardir)
#ifndef	ORIG_DEFAULTS
	vardir = strdup(DEFSERVVARDIR);
#else
	vardir = strapp(backuphome, FN_DIRSEPSTR "var");
#endif

  nomemfatal(vardir);

  bytesontape_file = strapp(vardir, FN_DIRSEPSTR BYTESONTAPE_FILE);
  tape_usage_file = strapp(vardir, FN_DIRSEPSTR TAPE_USAGE_FILE);
  precious_tapes_file = strapp(vardir, FN_DIRSEPSTR PRECIOUS_TAPES_FILE);
  cartorder_file = strapp(vardir, FN_DIRSEPSTR CARTRIDGE_ORDER_FILE);
  ro_tapes_file = strapp(vardir, FN_DIRSEPSTR READONLY_TAPES_FILE);
  cartloc_file = strapp(vardir, FN_DIRSEPSTR CARTRIDGE_LOCATION_FILE);
  cartnames_file = strapp(vardir, FN_DIRSEPSTR CARTRIDGE_NAMES_FILE);
  used_blocksizes_file = strapp(vardir, FN_DIRSEPSTR USED_BLOCKSIZES_FILE);
  client_message_file = strapp(vardir, FN_DIRSEPSTR CLIENT_MESSAGE_FILE);

  if(!bytesontape_file || !precious_tapes_file || !cartloc_file
	|| !ro_tapes_file || !cartorder_file || !used_blocksizes_file
	|| !client_message_file || !cartnames_file || !tape_usage_file){
    nomemfatal(NULL);
  }

  if(!tapeposfile)
    EM__(tapeposfile = strdup(DEFAULT_TAPEPOSFILE));
  if(!user_to_inform)
    EM__(user_to_inform = strdup(DEFAULT_USERTOINFORM));
  if(!mail_program)
    EM__(mail_program = strdup(DEFAULT_MAILPROGRAM));
  cptr = mail_program;
  EM__(mail_program = repl_substring(mail_program, "%u", user_to_inform));
  free(cptr);

  i = uname(&unam);
  if(i < 0)
    strcpy(unam.nodename, "<unknown>");

  infobuffersize = (INFOBLOCKSIZE / tapeblocksize + 1) * tapeblocksize;
  infobuffer = (UChar *) malloc_forced(infobuffersize * sizeof(UChar));
  if(!infobuffer){
    nomemfatal(NULL);
  }

  if(stat(vardir, &statb)){
    fprintf(stderr,
	T_("Installation error, var directory `%s' does not exit\n"),
		vardir);
    do_exit(38);
  }
  vardirowner.uid = statb.st_uid;
  vardirowner.gid = statb.st_gid;
  if(stat(devicename, &statb)){
    fprintf(stderr,
	T_("Installation error, cannot access device `%s'\n"),
		devicename);
    do_exit(39);
  }
  deviceowner.uid = statb.st_uid;
  deviceowner.gid = statb.st_gid;
  if(vardirowner.uid != deviceowner.uid)
    fprintf(stderr, T_("Warning: Owner of device `%s' is different from owner of the var directory `%s'. Preferring device owner. Please check ownerships of files in `%s' later.\n"),
		devicename, vardir, vardir);

  setuid(deviceowner.uid);	/* workaround: some glibc eaccess(2) is buggy */
  to_other_user(deviceowner.uid, -1, NULL);
	/* we do not switch to other GID because of the typical
	 * locking directory permissions (hopefully ...) */

  if(!devicelock.lockfile)
    devicelock.lockfile = select_lockfile(DEFAULT_SERVERLOCKFILE);
  EEM__(repl_dirs(&devicelock.lockfile) || repl_dirs(&mail_program)
		|| repl_dirs(&setfilecmd) || repl_dirs(&tapeposfile));
  if(loggingfile)
    EEM__(repl_dirs(&loggingfile));

  cptr = repl_substring(setfilecmd, "%d", devicename);
  free(setfilecmd);
  setfilecmd = repl_substring(cptr, "%n", "1");
  free(cptr);
  cptr = setfilecmd;
  setfilecmd = repl_substring(cptr, "%m", "0");
  free(cptr);

  if(mode == MODE_CART_CTL)
    do_exit(cart_ctl(argc, (UChar **) argv));

  if(mode == MODE_LABEL_TAPE)
    do_exit(label_tape(argc, (UChar **) argv));

  if(mode == MODE_CART_IS)
    do_exit(cart_is(argc, (UChar **) argv));

  fprintf(stderr, "Internal Implementation error, Please report: label_tape.c, mode\n");

  exit(37);
}

static int
label_tape(Int32 argc, UChar ** argv)
{
  unsigned long	omode;
  Uns32Range	*tape_ranges;
  UChar		cbuf[30], *cptr;
  Flag		ask_ow = NO, title_pr = NO;
  Int32		num_entries = 0, i, n;
  int		fd, i1;
  KeyValuePair	*allentries = NULL;
  struct stat	statb;

  memset(infobuffer, 0, sizeof(UChar) * infobuffersize);

  if(!query){
    if(label < 1 || label > num_cartridges){
      fprintf(stderr,
	T_("Error: Illegal cartridge label (must be in the range 1 - %d).\n"),
		(int) num_cartridges);
      return(7);
    }

    if(scnd_label < 1){
      if(!(scnd_label == -2 && scnd_label_arg))		/* argument is given */
	fprintf(stderr,
		T_("Warning: Illegal secondary label (must be >= 1), ignored.\n"));

      scnd_label = label;
    }
  }

		/* The following is to keep the server from possible */
		/* probing, whether there is a tape in the drive, cause */
		/* this would probably make the tape being ejected, */
		/* while we want to write a label to it ... */
		/* if we don't get the lock, c'est la vie */
		/* F_OK is often 0 (?), so an or-ed mask can't be used */
  if(!query && !eaccess(CARTREADY_FILE, F_OK) && !eaccess(CARTREADY_FILE, W_OK))
    waitlockfd = set_wlock_timeout(CARTREADY_FILE, 2000);

  if(!query && !dont_ask){
    printf(T_("You are about to write a label at the beginning of the tape.\n"));
    printf(T_("All data on this tape will be lost and it will become very\n"));
    printf(T_("expensive to get anything back, possibly nothing at all.\n"));
    printf(T_("\nDo you want to continue ? [y/n] "));
    fflush(stdout);
    if(!interactive){
	fprintf(stderr, T_("\nCannot read confirmation from TTY. Exiting.\n"));
	return(13);
    }
    if(wait_for_input(0, 120) < 1){
	fprintf(stderr, T_("\nTimeout on input, exiting.\n"));
	return(15);
    }

    cbuf[0] = '\0';
    while(!fgets(cbuf, 20, stdin));
    if(cbuf[0] != 'y' && cbuf[0] != 'Y')
	return(0);

    fprintf(stdout, "\n");
  }

  if(set_lock_msg(&devicelock, forced, !interactive) != BU_GOT_LOCK && !forced)
    return(9);

  i = system(setfilecmd);
  if(i < 0 || WEXITSTATUS(i)){
    fprintf(stderr, T_("Warning: Cannot rewind tape.\n"));
  }

  i = stat(devicename, &statb);
  if(i){
    fprintf(stderr, T_("Error: Cannot stat tape device %s\n"), devicename);
    return(11);
  }
  COPYVAL(devstatb, statb);

  if(IS_DIRECTORY(statb)){
    EM__(cptr = strapp(devicename, FN_DIRSEPSTR "data.0"));

    free(devicename);
    devicename = cptr;
  }

  label_given = 0;

  if((i = find_tape_blocksize(devicename, &cptr)) > 0){
    memcpy(infobuffer, cptr, MIN(i, infobuffersize) * sizeof(UChar));
    free(cptr);

    label_given = 1;
    if(!strstr(infobuffer, INFOHEADER))
	  label_given = 0;

    infobuffer[infobuffersize - 1] = '\0';
  }
  else{
    if(query){
      if(!IS_DIRECTORY(statb) && !IS_REGFILE(statb)){
	fprintf(stderr, T_("Error: Cannot read from tape device `%s'.\n"),
			devicename);
	return(8);
      }
    }
  }

  i = system(setfilecmd);
  if(i < 0 || WEXITSTATUS(i)){
    fprintf(stderr, T_("Warning: Cannot rewind tape.\n"));
  }

  if(query && !only_read_label){
    if(label_given)
	fprintf(stdout, "%s\n", infobuffer);
    else
	fprintf(stderr, T_("<Cannot read label>\n"));
  }

  if(!query){
    ask_ow = NO;
    if(label_given && (cptr = strstr(infobuffer, CARTNOTEXT))){
	if(cptr){
	  n = sscanf(cptr + strlen(CARTNOTEXT), "%d", &i1);
	  if(n > 0){
	    actcart = i1;

	    tape_ranges = get_readonly_tapes();
	    if(tape_ranges){
		if(in_Uns32Ranges(tape_ranges, actcart)){
		  fprintf(stdout,
			T_("Cartridge %d is among the read-only tapes\n"),
				(int) actcart);
		  ask_ow = YES;
		}

		free(tape_ranges);
	    }

	    allentries = kfile_getall(precious_tapes_file, &num_entries,
				KFILE_LOCKED);
	    if(allentries){
		title_pr = NO;

		for(i = 0; i < num_entries; i++){
		  tape_ranges = sscan_Uns32Ranges(allentries[i].value,
					1, num_cartridges, NULL, NULL);
		  if(in_Uns32Ranges(tape_ranges, actcart)){
		    if(!title_pr){
			fprintf(stdout, T_("Cartridge %d is needed by the"
				" following clients for restore:\n"),
				(int) actcart);
			title_pr = YES;
		    }
		    ask_ow = YES;
		    fprintf(stdout, "%s\n", allentries[i].key);
		  }

		  free(tape_ranges);
		}

		kfile_freeall(allentries, num_entries);
	    }

	    if(ask_ow && !delfromhd && !dont_ask){
		printf(T_("Do you want to delete cartridge %d \nfrom the"
			" cartridge handling database ?\n"), (int) actcart);
		fflush(stdout);
		if(!interactive){
		  fprintf(stderr, T_("Cannot read confirmation from TTY. Exiting.\n"));
		  return(14);
		}
		if(wait_for_input(0, 120) < 1){
		  fprintf(stderr, T_("Timeout on input, exiting.\n"));
		  return(15);
		}
		cbuf[0] = '\0';
		while(!fgets(cbuf, 20, stdin));
		if(cbuf[0] == 'y' || cbuf[0] == 'Y')
		  delfromhd = YES;
	    }

	    if(delfromhd){
		delete_tape(actcart);

		fprintf(stdout, T_("Cartridge %d is %s in cartridge management.\n"),
			(int) actcart, (actcart == label ? T_("reinitialized") : T_("deleted")));
	    }
	  }
	}
    }

    omode = O_WRONLY | O_BINARY;
    if(IS_DIRECTORY(statb))
	omode |= O_CREAT;

    fd = open(devicename, omode, 0600);
    if(fd < 0){
	fprintf(stderr, T_("Error: Cannot open tape device `%s'.\n"),
			devicename);
	return(4);
    }

    sprintf(infobuffer,
		"\n%s\n\n%s%d\n\n%s%d\n\n%s%s\n\n%s%d\n\n%s\n\n%s%s\n",
		INFOHEADER, CARTNOTEXT, (int) label,
		CART2NOTEXT, (int) scnd_label,
		DATETEXT, actimestr(), BLOCKSIZETEXT, (int) tapeblocksize,
		VERSIONTEXT, COMMENTTEXT, comment);

    if(write(fd, infobuffer, infobuffersize) < infobuffersize){
	fprintf(stderr, T_("Error: The write to tape failed.\n"));
	return(6);
    }

    reset_in_tapepos(label);

    i = stat(bytesontape_file, &statb);
    if(save_bytes_per_tape(bytesontape_file, label, infobuffersize, 0, NO,
			time(NULL))){
	fprintf(stderr, T_("Error: Cannot write file %s to save the number of bytes on tape.\n"),
			bytesontape_file);
	return(10);
    }
    if(i)
	chown(bytesontape_file, default_stat.st_uid, default_stat.st_gid);

    close(fd);

    i = stat(used_blocksizes_file, &statb);
    add_to_int_list_file(used_blocksizes_file, tapeblocksize);
    if(i)
	chown(used_blocksizes_file, default_stat.st_uid, default_stat.st_gid);
  }

  i = system(setfilecmd);
  if(i < 0 || WEXITSTATUS(i)){
    fprintf(stderr, T_("Warning: Cannot rewind tape.\n"));
  }

  release_lock(&devicelock);

  if(waitlockfd >= 0)
    close(waitlockfd);

  return(0);
}

Int32
poll_changer_cmd(UChar * cmd, Flag wait_success, Flag quiet)
{
  int		res, i;
  time_t	t, t_mail, t_giveup;
  FILE		*fp;

  res = system(cmd);

  if((res < 0 || WEXITSTATUS(res)) && wait_success){
    t = time(NULL);
    t_mail = t + devunavail_send_mail * 60;
    t_giveup = t + devunavail_give_up * 60;

    do{
      if(t_mail > 0 && t > t_mail && devunavail_send_mail > 0){
	fp = tmp_fp(NULL);
	if(fp){
	  fprintf(fp, T_("The device %s on host %s is not ready for use.\n"),
			changername, unam.nodename);
	  fprintf(fp, T_("You are requested to check the changer for possible\n"));
	  fprintf(fp, T_("errors and to correct them.\n\n"));
	  fprintf(fp, T_("Best regards from your backup service.\n"));

	  i = message_to_operator(fp, YES);
	}

	if(i || !fp){
	  fprintf(stderr, T_("Error: Unable to ask user %s to check device availability.\n"),
			user_to_inform);
	}
	t_mail = 0;
      }

      if(t_giveup > 0 && t > t_giveup && devunavail_give_up > 0){
	fprintf(stderr, T_("Warning: Changer command timed out.\n"));
	return((Int32) res);
      }

      ms_sleep(1000 * 30);
      t += 30;
      res = system(cmd);
    } while(res < 0 || WEXITSTATUS(res));
  }

  if(res && !quiet){
    if(interactive){
	fp = stderr;
    }
    else{
	fp = tmp_fp(NULL);
	if(!fp){
	  fprintf(stderr, T_("Error: Cannot send mail to report operate cartridge error.\n"));
	}
	else{
	  fprintf(fp, "%s\n\n", T_("Hello,"));
	}
    }

    fprintf(fp,
	T_("The command to operate the cartridge in changer %s\n"
		"failed with status %d.\n"), changername, (int) res);

    if(!interactive){
	fprintf(fp, T_("You are requested to check the changer for possible\n"));
	fprintf(fp, T_("errors and to correct them.\n\n"));
	fprintf(fp, T_("Best regards from your backup service.\n"));

	if(message_to_operator(fp, YES))
	  fprintf(stderr, T_("Error: Cannot send mail to report operate cartridge error.\n"));
    }
  }

  return(res);
}

static void
report_problem(UChar * fmt, ...)
{
  va_list	args;
  FILE		*fp;

  va_start(args, fmt);

  if(interactive){
    fp = stderr;
  }
  else{
    fp = tmp_fp(NULL);
    if(!fp){
	fprintf(stderr, T_("Error: Cannot send mail to report problem.\n"));
	fp = stderr;
    }
    else{
	fprintf(fp, "%s\n\n", T_("Hello,"));
    }
  }

  vfprintf(fp, fmt, args);

  if(!interactive)
    fprintf(fp, "\n%s\n", T_("Regards from your backup system"));

  va_end(args);

  if(!interactive)
    if(message_to_operator(fp, YES))
      fprintf(stderr, T_("Error: Cannot send mail to report problem.\n"));
}

static Uns32Range *
read_ranges_from_cmd(UChar * cmd)
{
  Uns32Range	*ranges = NULL, *more_ranges = NULL;
  UChar		*line = NULL;
  FILE		*fp;

  if(!cmd || empty_string(cmd))
    return(NULL);

  fp = popen(cmd, "r");
  if(!fp)
    GETOUT;

  while(!feof(fp)){
    ZFREE(line);
    line = fget_alloc_str(fp);
    if(!line)
	continue;

    ZFREE(more_ranges);
    more_ranges = sscan_Uns32Ranges__(line, 1, MAXINT, NULL, NULL);

    merge_Uns32Ranges(&ranges, more_ranges);
  }

 cleanup:
  if(fp){
    pclose(fp);
  }
  ZFREE(line);
  ZFREE(more_ranges);

  return(ranges);

 getout:
  ZFREE(ranges);
  CLEANUP;
}

static Uns32Range *
get_free_slots(UChar * changer)
{
  return(read_ranges_from_cmd(freeslots_cmd));
}

static Uns32Range *
get_free_loadports(UChar * changer)
{
  return(read_ranges_from_cmd(freeloadports_cmd));
}

static Int32
get_first_from_range(Uns32Range * range)
{
  Int32		f;

  if(!range)
    return(-1);

  if(num_Uns32Ranges(range) < 1)
    f = 0;
  else
    f = range[0].first;

  free(range);

  return(f);
}

#define	get_first_free_slot(changer)		\
			get_first_from_range(get_free_slots(changer))
#define	get_first_free_loadport(changer)		\
			get_first_from_range(get_free_loadports(changer))

static UChar *
changerlocstr(UChar * changer, Int8 loctype, Int32 pos)
{
  UChar		nbuf[32];

  if(loctype == LOC_EXTERNAL)
    return(strdup(changer));

  if(loctype != LOC_SLOT && loctype != LOC_DRIVE && loctype != LOC_LOADPORT)
    return(NULL);

  sprintf(nbuf, "%d", pos);

  return(strchain(changer ? (char *) changer : "",
		changer ? " " : "", loc_strings[loctype],
		" ", nbuf, NULL));
}

static Int32
set_location(
  Int32		cartnum,
  Int32		type,
  UChar		*changer_or_text,
  Int32		num)
{
  UChar		*lbuf = NULL, nbuf[30];
  Int32		r;

  sprintf(nbuf, "%d:", (int) cartnum);

  if(type == LOC_UNKNOWN)
    return(kfile_delete(cartloc_file, nbuf, KFILE_SORTN | KFILE_LOCKED));

  if(type != LOC_EXTERNAL){
    EM__(lbuf = NEWP(UChar, strlen(changer_or_text) + 60));

    sprintf(lbuf, "%s %s %d", changer_or_text, loc_strings[type], (int) num);

    changer_or_text = lbuf;
  }

  r = kfile_insert(cartloc_file, nbuf, changer_or_text,
					KFILE_SORTN | KFILE_LOCKED);
  ZFREE(lbuf);

  return(r);
}

static Int32
set_location_msg(
  Int32		cartnum,
  Int32		type,
  UChar		*changer_or_text,
  Int32		num)
{
  Int32		i;

  i = set_location(cartnum, type, changer_or_text, num);
  if(i){
    report_problem(T_("Error: Writing the new location to file `%s' failed.\n"),
				cartloc_file);
  }

  return(i);
}

static Int32
get_location(
  Int32		cartnum,
  Int32		*rtype,
  UChar		**changer_or_text,
  Int32		*rnum)
{
  Int32		type;	/* uninitialized OK */
  Int32		num, i;
  UChar		*entry, nbuf[30], *cptr, *r_chgr_txt = NULL;
  int		i1, n1;
  struct stat	statb;

  sprintf(nbuf, "%d:", (int) cartnum);

  entry = kfile_get(cartloc_file, nbuf, KFILE_LOCKED);

  if(!entry){
    if(rtype)
	*rtype = LOC_UNKNOWN;
    if(changer_or_text)
	EM__(*changer_or_text = strdup(T_("unknown")));
    return(0);
  }

  num = -1;

  if(word_count(entry) == 3){
    r_chgr_txt = strword(entry, 0);
    if(r_chgr_txt){
      i = stat(r_chgr_txt, &statb);
      if(!i){
	cptr = strword(entry, 1);
	if(cptr){
	  for(i = 0; i < NUM_LOC_CMD; i++){
	    if(!strcmp(cptr, loc_strings[i])){
		type = i;
		break;
	    }
	  }
	  if(i < NUM_LOC_CMD){
	    free(cptr);
	    cptr = strword(entry, 2);
	    if(cptr){
	      i = sscanf(cptr, "%d%n", &i1, &n1);
	      if(i > 0 && n1 == strlen(cptr))
		num = i1;	/* poooh */
	    }
	  }

	  ZFREE(cptr);
	}
      }
    }
  }

  if(num > 0){
    if(rtype)
	*rtype = type;
    if(rnum)
	*rnum = num;
    if(changer_or_text)
	*changer_or_text = r_chgr_txt;
  }
  else{
    ZFREE(r_chgr_txt);

    if(changer_or_text)
	*changer_or_text = entry;
    if(rtype)
	*rtype = LOC_EXTERNAL;
  }

  return(0);
}

static Int32
genmove(
  Int32		cartnum,	/* this is just to inform a maintainer */
  Int32		sourcetype,
  Int32		sourcenum,
  UChar		*src_changer_or_text,
  Int32		targettype,
  Int32		targetnum,
  UChar		*tgt_changer_or_text)
{
  UChar		*cmd, *cptr, c, nbuf[30];
  Int32		i, j, free_loadport = 0, free_slot = 0;
  FILE		*fp;
  int		fd;

  if(sourcetype == targettype
		&& (sourcenum == targetnum || sourcetype == LOC_EXTERNAL
					|| sourcetype == LOC_UNKNOWN)
		&& !strcmp(src_changer_or_text, tgt_changer_or_text))
    return(0);

  if(sourcetype == LOC_EXTERNAL || sourcetype == LOC_UNKNOWN
		|| targettype == LOC_EXTERNAL || targettype == LOC_UNKNOWN){
    if(num_changer_loadports)
	free_loadport = get_first_free_loadport(changername);
    if(num_changer_slots)
	free_slot = get_first_free_slot(changername);
  }

  if(sourcetype == LOC_EXTERNAL || sourcetype == LOC_UNKNOWN
					|| targettype == LOC_EXTERNAL){
    if(targettype == LOC_EXTERNAL){
			/* if target is external and there is a loadport, */
			/* first move the cartridge to the load port */
      if(sourcetype != LOC_EXTERNAL && sourcetype != LOC_LOADPORT
					&& sourcetype != LOC_UNKNOWN){
	if(cartmove_cmds[sourcetype][LOC_LOADPORT] && free_loadport > 0){
	  ER__(genmove(cartnum, sourcetype, sourcenum, src_changer_or_text,
			LOC_LOADPORT, free_loadport, src_changer_or_text), i);
	  sourcetype = LOC_LOADPORT;
	  sourcenum = free_loadport;
	}
	else if(sourcetype == LOC_DRIVE
		&& cartmove_cmds[sourcetype][LOC_SLOT] && free_slot > 0){
	  ER__(genmove(cartnum, sourcetype, sourcenum, src_changer_or_text,
			LOC_SLOT, free_slot, src_changer_or_text), i);
	  sourcetype = LOC_SLOT;
	  sourcenum = free_slot;
	}
      }
    }

    if(interactive){
	fp = stdout;
    }
    else{
	fp = tmp_fp(NULL);
	if(!fp){
	  fprintf(stderr, T_("Error: Cannot send mail to request manual cartridge operation.\n"));
	  return(24);
	}
	else{
	  fprintf(fp, "%s\n\n", T_("Hello,"));
	}
    }

    fprintf(fp, T_("The cartridge %d must be moved manually from location\n"),
			(int) cartnum);

    switch(sourcetype){
      case LOC_UNKNOWN:
      case LOC_EXTERNAL:
	fprintf(fp, " %s\n", src_changer_or_text);
	break;

      case LOC_DRIVE:
      case LOC_SLOT:
      case LOC_LOADPORT:
	fprintf(fp, T_(" %s %d in changer `%s'\n"),
			T_(loc_strings[sourcetype]), (int) sourcenum,
			changername);
	break;
    }

    fprintf(fp, T_("to the location\n"));

    switch(targettype){
      case LOC_EXTERNAL:
	fprintf(fp, " %s\n", tgt_changer_or_text);
	break;

      case LOC_DRIVE:
	if(cartmove_cmds[LOC_LOADPORT][targettype] && free_loadport > 0){
	  fprintf(fp, T_(" %s %d\n"),
			T_(loc_strings[LOC_LOADPORT]), (int) free_loadport);
	  break;
	}

	if(cartmove_cmds[LOC_SLOT][targettype] && free_slot > 0){
	  fprintf(fp, T_(" %s %d\n"),
			T_(loc_strings[LOC_SLOT]), (int) free_slot);
	  break;
	}

	fprintf(fp, T_(" %s %d\n"),
			T_(loc_strings[targettype]), (int) targetnum);
	fprintf(fp, T_("If this is not possible, please remove a cartridge from a slot,\n"
		"if there is no free one, and put cartridge %d into a free slot.\n"
		"Then tell the backup system, in what slot the cartridge is, using\n"
		"cart_ctl with options -P, -C and -S.\n"),
				(int) cartnum);
	break;

      case LOC_SLOT:
	if(cartmove_cmds[LOC_LOADPORT][targettype] && free_loadport > 0){
	  fprintf(fp, T_(" %s %d\n"),
			T_(loc_strings[LOC_LOADPORT]), (int) free_loadport);
	  break;
	}

      case LOC_LOADPORT:
	fprintf(fp, T_(" %s %d\n"),
			T_(loc_strings[targettype]), (int) targetnum);
	break;
    }

    fprintf(fp, T_("Please perform the requested operation and when done:\n"));
    if(interactive){
	fprintf(fp, T_("Press the Return key.\n"));

	forever{
	  i = getinchr(&c, 0);
	  if(i > 0 && c == '\n')
	    break;

	  if(i <= 0)
	    return(23);
	}	  
    }
    else{
	fd = open(CHANGERREADY_FILE, O_CREAT | O_WRONLY, 0600);
	if(fd < 0){
	  fprintf(stderr, T_("Error: Cannot create flag file `%s' to wait for cartridge ready"),
			CHANGERREADY_FILE);
	  return(23);
	}
	close(fd);

	fprintf(fp, T_("Run the command:\n\n changerready\n\n on host %s\n"),
			unam.nodename);
	if(message_to_operator(fp, YES))
	  fprintf(stderr, T_("Error: Cannot send mail to ask operator to move a cartridge manually.\n"));

	while(!eaccess(CHANGERREADY_FILE, F_OK))
	  ms_sleep(1000);
    }

    i = get_location(cartnum, &j, &src_changer_or_text, &sourcenum);
    if(i){
	fprintf(stderr, T_("Error: Cannot determine location of cartridge %d after"
		" requesting manual interaction.\n"), (int) cartnum);
	return(i);
    }

    if(j != sourcetype){	/* user moved the tape manually to a slot */
	if(j == LOC_SLOT)		/* and told us about it */
	  free_slot = sourcenum;
    }

    if(sourcetype == LOC_EXTERNAL || sourcetype == LOC_UNKNOWN){
      if(targettype != LOC_EXTERNAL && targettype != LOC_LOADPORT){
	if(cartmove_cmds[LOC_LOADPORT][targettype] && free_loadport > 0){
	  ER__(genmove(cartnum,
			LOC_LOADPORT, free_loadport, tgt_changer_or_text,
			targettype, targetnum, tgt_changer_or_text), i);
	}
	else if(targettype == LOC_DRIVE
		&& cartmove_cmds[LOC_SLOT][targettype] && free_slot > 0){
	  ER__(genmove(cartnum,
			LOC_SLOT, free_slot, tgt_changer_or_text,
			targettype, targetnum, tgt_changer_or_text), i);
	}
      }
    }

    return(0);
  }

  cptr = cartmove_cmds[sourcetype][targettype];

  if(!cptr){
    if(interactive){
	fp = stderr;
    }
    else{
	fp = tmp_fp(NULL);
    }
    fprintf(fp, T_("Error: No command configured to perform the "
			"requested cartridge move from %s to %s.\n"),
		T_(loc_strings[sourcetype]), T_(loc_strings[targettype]));
    if(!interactive)
	message_to_operator(fp, YES);

    return(22);
  }

  EM__(cptr = repl_substring(cptr, "%d", devicename));
  EM__(cmd = repl_substring(cptr, "%D", changername));
  free(cptr);
  sprintf(nbuf, "%d", (int) sourcenum - 1);
  EM__(cptr = repl_substring(cmd, "%m", nbuf));
  free(cmd);
  sprintf(nbuf, "%d", (int) sourcenum);
  EM__(cmd = repl_substring(cptr, "%n", nbuf));
  free(cptr);
  sprintf(nbuf, "%d", (int) targetnum - 1);
  EM__(cptr = repl_substring(cmd, "%M", nbuf));
  free(cmd);
  sprintf(nbuf, "%d", (int) targetnum);
  EM__(cmd = repl_substring(cptr, "%N", nbuf));
  free(cptr);

  i = poll_changer_cmd(cmd, !interactive, NO);
  if(!i && cartnum > 0)
    i = set_location(cartnum, targettype, tgt_changer_or_text, targetnum);

  return(i);
}

static Int32		/* genmove with lock on streamer, if necessary */
genmove_drlock(
  Int32		cartnum,	/* this is just to inform a maintainer */
  Int32		sourcetype,
  Int32		sourcenum,
  UChar		*src_changer_or_text,
  Int32		targettype,
  Int32		targetnum,
  UChar		*tgt_changer_or_text)
{
  Int32		i;
  Flag		dr;

  dr = (sourcetype == LOC_DRIVE || targettype == LOC_DRIVE);

  if(dr){
    i = set_lock(&devicelock, forced);
    if(i != BU_GOT_LOCK && !forced){
	report_problem("%s: %s.\n",
		T_("Error"),
		T_("An application using the device seems to be running"));
	return(19);
    }
  }

  i = genmove(cartnum, sourcetype, sourcenum, src_changer_or_text,
			targettype, targetnum, tgt_changer_or_text);

  if(dr)
    release_lock(&devicelock);

  return(i);
}

static Int32
free_hwcartlabels()
{
  CartLabelEnt	*cartentptr;

  if(hwcartlabels){
    for(cartentptr = hwcartlabels; cartentptr->pos > 0; cartentptr++)
	ZFREE(cartentptr->label);

    free(hwcartlabels);
  }
  num_hwcartlabels = 0;

  return(0);
}

static Int32
del_hwcartent(CartLabelEnt * cartent)
{
  CartLabelEnt	*cartentptr;

  if(hwcartlabels){
    for(cartentptr = cartent; (cartentptr + 1)->pos > 0; cartentptr++);

    free(cartent->label);
    if(cartentptr > cartent)
	memcpy(cartent, cartentptr, sizeof(CartLabelEnt));

    cartentptr->pos = -1;
    num_hwcartlabels--;
  }

  return(0);
}

static Int32
cmp_hwcartlabel_by_num(void * ptr1, void * ptr2)
{
  Int32		num1, num2;

  num1 = ((CartLabelEnt *) ptr1)->num;
  num2 = ((CartLabelEnt *) ptr2)->num;

  return(num1 > num2 ? 1 : (num1 < num2 ? -1 : 0));
}

static Int32
cmp_hwcartlabel_by_loc(void * ptr1, void * ptr2)
{
  Int32		pos1, pos2;
  Int8		loc1, loc2;

  pos1 = ((CartLabelEnt *) ptr1)->pos;
  pos2 = ((CartLabelEnt *) ptr2)->pos;
  loc1 = ((CartLabelEnt *) ptr1)->loctype;
  loc2 = ((CartLabelEnt *) ptr2)->loctype;

  if(loc1 != loc2)
    return(loc1 > loc2 ? 1 : -1);

  return(pos1 > pos2 ? 1 : (pos1 < pos2 ? -1 : 0));
}

static Int32
get_hwcartlabels(UChar * changer)
{
  Int32		ncall = 0, r = 0;
  UChar		*line = NULL, *cmds[4], **cpptr, *curcmd = NULL, *cptr;
  int		i1;
  Int8		loctype;
  FILE		*fp = NULL;

  free_hwcartlabels();

  cpptr = &(cmds[0]);
  *cpptr = NULL;
  if(lslabels_cmd){
    *(cpptr++) = lslabels_cmd;
    *(cpptr++) = NULL;
  }
  else{
    if(lslabelsdrive_cmd)
	*(cpptr++) = lslabelsdrive_cmd;
    if(lslabelsslot_cmd)
	*(cpptr++) = lslabelsslot_cmd;
    if(lslabelsloadport_cmd)
	*(cpptr++) = lslabelsloadport_cmd;
    *cpptr = NULL;
  }

  for(cpptr = &(cmds[0]); *cpptr; cpptr++){
    ZFREE(curcmd);
    curcmd = repl_substring(*cpptr, "%D", changer);
    if(!curcmd)
	GETOUT;

    fp = popen(curcmd, "r");
    if(!fp){
	fprintf(stderr,
		T_("WARNING: Cannot start command `%s' to read the tape labels\n"),
			curcmd);
    }
    else{
      while(!feof(fp)){
	if(fget_realloc_str(fp, &line, &ncall))
	  continue;
	while(chop(line));

	loctype = LOC_UNKNOWN;
	if(lslabels_cmd){
	  cptr = word_start(line, 1);
	  if(!*cptr)
	    continue;
	  sscanword(line, line);
	  if(!strcasecmp(line, "drive"))
	    loctype = LOC_DRIVE;
	  else if(!strcasecmp(line, "slot"))
	    loctype = LOC_SLOT;
	  else if(!strcasecmp(line, "loadport"))
	    loctype = LOC_LOADPORT;
	  else
	    continue;

	  memmove(line, cptr, (strlen(cptr) + 1) * sizeof(UChar));
	}
	else{
	  if(*cpptr == lslabelsdrive_cmd)
	    loctype = LOC_DRIVE;
	  else if(*cpptr == lslabelsslot_cmd)
	    loctype = LOC_SLOT;
	  else if(*cpptr == lslabelsloadport_cmd)
	    loctype = LOC_LOADPORT;
	  else
	    continue;
	}

	cptr = word_start(line, 1);
	if(!*cptr)
	  continue;
	if(sscanf(line, "%d", &i1) < 1)
	  continue;

	memmove(line, cptr, (strlen(cptr) + 1) * sizeof(UChar));

	hwcartlabels = ZRENEWP(hwcartlabels, CartLabelEnt, num_hwcartlabels + 2);
	if(!hwcartlabels)
	  GETOUT;

	hwcartlabels[num_hwcartlabels].pos = i1;
	hwcartlabels[num_hwcartlabels].loctype = loctype;
	hwcartlabels[num_hwcartlabels].label = line;
	hwcartlabels[num_hwcartlabels].num = -1;	/* initialize as unknown */
	line = NULL;
	ncall = 0;
	num_hwcartlabels++;
	hwcartlabels[num_hwcartlabels].pos = -1;
      }
      pclose(fp);
    }
  }

 cleanup:
  ZFREE(curcmd);
  ZFREE(line);

  return(r);

 getout:
  free_hwcartlabels();
  CLEANUPR(-2);
}

/* if label is set, return the label, not the name */
static UChar *
get_cartname(Int32 cartnum, Flag label)
{
  UChar		nbuf[32], *value = NULL, *cptr;
  UChar		*r = NULL;

  sprintf(nbuf, "%d:", cartnum);
  value = kfile_get(cartnames_file, nbuf, KFILE_LOCKED);
  if(!value)
    return(NULL);

  cptr = sscanwordq(value, NULL);
  if(!cptr)
    CLEANUPR(NULL);

  if(label){
    sscanwordq(value, value);
    if(value[0])
	r = value;
  }
  else{
    if(cptr){
	memmove(value, cptr, (strlen(cptr) + 1) * sizeof(UChar));
	if(value[0])
	  r = value;
    }
  }
  if(!r)
    free(value);
  value = NULL;

 cleanup:
  ZFREE(value);
  return(r);
}

static Int32
get_cartbyname(UChar * name, Flag label)
{
  KeyValuePair	*namelist = NULL;
  Int32		num_names = 0, i, f, fidx, r;
  UChar		*cptr, *value;
  int		i1;

  namelist = kfile_getall(cartnames_file, &num_names, KFILE_LOCKED);
  if(!namelist)
    return(-1);

  fidx = f = r = -1;
  for(i = 0; i < num_names; i++){
    value = namelist[i].value;
    cptr = sscanwordq(value, value);
    f = -1;

    if(label && value)
      if(value[0] && !strcmp(name, value))
	f = i;

    if(!label && cptr)
      if(!strcmp(name, cptr))
	f = i;

    if(f >= 0){
	if(fidx >= 0)
	  CLEANUPR(-10);

	fidx = f;
    }
  }

  if(fidx >= 0)
    if(sscanf(namelist[fidx].key, "%d", &i1) > 0)
	r = i1;

 cleanup:
  kfile_freeall(namelist, num_names);

  return(r);
}

/* if label is set, set the label, not the name */
static Int32
set_cartname(Int32 cartnum, UChar * name, Flag label)
{
  UChar		nbuf[32], *value = NULL, *cptr, *newval;
  Int32		r = 0, newlen;

  if(!name)
    name = "";

  sprintf(nbuf, "%d:", cartnum);
  value = kfile_get(cartnames_file, nbuf, KFILE_LOCKED);

  newlen = 4 * strlen(name) + 4;
  if(value)
    newlen += strlen(value);
  newval = NEWP(UChar, newlen);
  if(!newval)
    CLEANUPR(-2);

  if(value){
    cptr = sscanwordq(value, NULL);

    if(label){
	sprintwordq(newval, name);
    }
    else{
	sscanwordq(value, value);
	sprintwordq(newval, value);
	cptr = name;
    }
    strcat(newval, " ");
    strcat(newval, cptr);
  }
  else{
    sprintwordq(newval, label ? (char *) name : "");
    strcat(newval, " ");
    strcat(newval, label ? "" : (char *) name);
  }

  r = kfile_insert(cartnames_file, nbuf, newval, KFILE_SORT | KFILE_LOCKED);

 cleanup:
  ZFREE(value);
  free(newval);
  return(r);
}

Int32
cart_in_location(
  Int32		ltype,
  Int32		num,
  UChar		*changer)
{
  KeyValuePair	*list = NULL;
  UChar		*word = NULL, *locstr, *cptr;
  Int32		i, n, r = 0;
  int		i1;

  if(ltype == LOC_EXTERNAL || ltype == LOC_UNKNOWN){
    errno = EINVAL;
    return(-1);
  }

  locstr = loc_strings[ltype];

  list = kfile_getall(cartloc_file, &n, KFILE_LOCKED);
  if(!list)
    return(0);

  for(i = 0; i < n; i++){
    if(word_count(list[i].value) != 3)
	continue;

    word = ZRENEWP(word, UChar, strlen(list[i].value) + 1);
    if(!word)
	CLEANUPR(-2);

    cptr = sscanword(list[i].value, word);
    if(strcmp(word, changer))
	continue;

    cptr = sscanword(cptr, word);
    if(strcmp(word, locstr))
	continue;

    sscanf(cptr, "%d", &i1);
    if(i1 == num){
	if(sscanf(list[i].key, "%d", &i1) < 1)
	  continue;

	r = i1;
	break;
    }
  }

 cleanup:
  ZFREE(word);
  kfile_freeall(list, n);

  return(r);
}

static CartLabelEnt *
get_cartentbyhwloc(Int8 loctype, Int32 pos, UChar * changername)
{
  CartLabelEnt	*cartentptr;

  if(!hwcartlabels)
    return(NULL);

  for(cartentptr = hwcartlabels; cartentptr->pos > 0; cartentptr++)
    if(cartentptr->pos == pos && cartentptr->loctype == loctype)
	break;

  if(cartentptr->pos < 0)
    return(NULL);

  return(cartentptr);
}

/* Return value:
 *  > 0  found known cartidge
 *  = 0  got label, but unknown
 *  < 0  not found
 */
static Int32
get_cartbyhwloc(Int8 loctype, Int32 pos, UChar * changername)
{
  CartLabelEnt	*cartent;
  Int32		cartnum;

  if(!hwcartlabels)
    return(-2);

  cartent = get_cartentbyhwloc(loctype, pos, changername);
  if(!cartent)
    return(-1);

  cartnum = get_cartbyname(cartent->label, YES);

  return(cartnum > 0 ? cartnum : -1);
}

static Int32	/* we assume, that cart is in drive, it is not checked here */
unload_cart_to_free_slot(Int32 cart, UChar * changer)
{
  Int32		first_free_slot;

  first_free_slot = get_first_free_slot(changer);
  if(first_free_slot < 1){
    report_problem(T_("Error: No free slot found to unload drive.\n"));
    return(-4);
  }

  return(move_cartridge_to(cart, LOC_SLOT, changer, first_free_slot));
}

static Int32
move_cartridge_to(
  Int32		cartnum,
  Int32		tgttype,
  UChar		*tgt_changer_or_text,
  Int32		tgtnum)
{
  Int32		i;
  UChar		*src_changer_or_text;
  Int32		srctype;
  Int32		srcnum;

  ER__(get_location(cartnum, &srctype, &src_changer_or_text, &srcnum), i);

  if(srctype == tgttype
		&& (srcnum == tgtnum || srctype == LOC_EXTERNAL
					|| srctype == LOC_UNKNOWN)
		&& !strcmp(src_changer_or_text, tgt_changer_or_text))
    return(0);

  if(set_lock_msg(&changerlock, forced, !interactive) != BU_GOT_LOCK && !forced)
    return(-3);

  if(tgttype == LOC_DRIVE){
    i = cart_in_location(tgttype, tgtnum, tgt_changer_or_text);
    if(i > 0){
	ER__(unload_cart_to_free_slot(i, tgt_changer_or_text), i);
    }
  }
  if(tgttype == LOC_LOADPORT || tgttype == LOC_SLOT){
    i = cart_in_location(tgttype, tgtnum, tgt_changer_or_text);
    if(i > 0){
	report_problem(T_("Error: Target location %s %d is not empty.\n"),
			T_(loc_strings[tgttype]), (int) tgtnum);
	return(-2);
    }
  }
	
  i = genmove_drlock(cartnum, srctype, srcnum, src_changer_or_text,
				tgttype, tgtnum, tgt_changer_or_text);
  if(i){
    if(tgttype == LOC_EXTERNAL)
	report_problem(T_("Error: Moving cartridge %d to location `%s' failed with status %d\n"),
				(int) cartnum, tgt_changer_or_text, (int) i);
    else
	report_problem(T_("Error: Moving cartridge %d to %s %d in %s failed with status %d\n"),
			(int) cartnum, T_(loc_strings[tgttype]), (int) tgtnum,
			changername, (int) i);
  }
  if(!i){
    forever{
	i = cart_in_location(tgttype, tgtnum, tgt_changer_or_text);
	if(i == cartnum || i <= 0)
	  break;		/* This happens, when some bad guy modifies */
				/* the database manually during our move */
	report_problem(T_("Warning: The location of the cartridge %d is set to %s.\n"),
				(int) i, T_("unknown"));
	set_location_msg(i, LOC_UNKNOWN, NULL, 0);
    }

    i = set_location_msg(cartnum, tgttype, tgt_changer_or_text, tgtnum);
  }

  return(i);
}

/* cart <= 0 means: not given */
static Flag
specified_by_args(Int32 cart, Int8 loctype, Int32 pos)
{
  if(cartridges)
    if((cart > 0 && !in_Uns32Ranges(cartridges, cart)) || cart <= 0)
	return(NO);

  if(have_slot_arg >= 0 || have_loadp_arg >= 0 || have_drive_arg >= 0){
    if(loctype == LOC_SLOT)
	if((slots && !in_Uns32Ranges(slots, pos)) || have_slot_arg < 0)
	  return(NO);

    if(loctype == LOC_DRIVE)
	if((drives && !in_Uns32Ranges(drives, pos)) || have_drive_arg < 0)
	  return(NO);

    if(loctype == LOC_LOADPORT)
	if((loadports && !in_Uns32Ranges(loadports, pos)) || have_loadp_arg < 0)
	  return(NO);
  }

  return(YES);
}

static int
cart_ctl(Int32 argc, UChar ** argv)
{
  Int32		i, j, k, n, cart, defval;
  Int32		tgtnum, res;
  Int32		tgtrange;	/* uninitialized OK */
  Int32		tgttype;
  UChar		*cptr, nbuf[32], rbuf[64], *tstr, *rstr;
  Uns32Range	*targets, *overset, *free_slots, *free_loadports;
  Uns32Range	*unlabeled_in_loadports, *unlabeled_in_slots;
  Uns32Range	*alltapes, *prec_tapes = NULL, *tmpr, *ro_tapes;
  KeyValuePair	*cartlocs, *namelist = NULL;
  KeyValuePair	*tape_uses, *bytesontape, *prec_tapesf;
  KeyValuePair	*keyvallist[2], *kvptr;
  CartLabelEnt	*cartentptr = NULL;
  int		i1, n1, fd;
  Flag		label_valid, need_changerconf;
  time_t	lastwrtime;

  if(given_changer){
    for(i = 0; i < num_changers; i++)
	if(!strcmp(changers[i].changername, given_changer))
	  break;
    if(i >= num_changers){
	fprintf(stderr, T_("Error: Changer `%s' not configured in `%s'.\n"),
			given_changer, configfile);
	return(48);
    }

    ZFREE(changername);
    changername = given_changer;
  }

  need_changerconf = (cctl_inventory || cctl_move || cctl_label || cctl_eject
		|| cctl_invlbl || (cctl_list && (list_mode & LIST_HWINV)));

  if(changerconffile)			/* read configuration */
    if(empty_string(changerconffile))
	ZFREE(changerconffile);
  if(changerconffile)
    massage_string(changerconffile);
  if(!changerconffile){
    if(need_changerconf){
	fprintf(stderr, T_("Error: No changer configuration file configured.\n"));
	return(20);
    }
  }
  else{
    EEM__(repl_dirs(&changerconffile));
    if(eaccess(changerconffile, R_OK)){
      fprintf(stderr, T_("Error: Cannot read changer configuration file `%s'.\n"),
			changerconffile);
      return(21);
    }

    n = NUM_PRESET_CHCF_ENTRIES;

    for(i = 0; i < 3; i++){	/* prepare commands entry data to be read */
      for(j = 0; j < 3; j++){
	sprintf(tmpbuf, "[Mm][Oo][Vv][Ee][-_ \t]*%s[-_ \t]*[Tt][Oo]"
		"[-_ \t]*%s[-_ \t]*[Cc][Oo]?[Mm][Mm]?[Aa]?[Nn]?[Dd]:?[ \t]*",
		loc_re_strings[i], loc_re_strings[j]);

	SETZERO(changerconf[n]);
	changerconf[n].entry_ptr = &(cartmove_cmds[i][j]);
	EM__(changerconf[n].pattern = strdup(tmpbuf));
	changerconf[n].type = TypeUCharPTR;

	cartmove_cmds[i][j] = NULL;

	n++;
      }
    }

    if(n != NUM_CHCF_ENTRIES){
	perror("Internal Error: Implementation of changerconfig-entries");
	return(2);
    }
					/* read configuration file */
    i = read_param_file(changerconffile, changerconf, NUM_CHCF_ENTRIES,
			NULL, NULL);
    if(i){
	fprintf(stderr,
		T_("Error: Cannot read changer configuration file `%s'.\n"),
			changerconffile);
	return(3);
    }
  }
					/* check configuration consistency */
  if(!changername){
    if(need_changerconf){
	fprintf(stderr, T_("Error: No media changer is configured in `%s'.\n"),
		configfile);
	return(27);
    }
  }
  else{
    if(eaccess(changername, F_OK)){
	fprintf(stderr, T_("Error: The media changer device `%s' does not exist.\n"),
		changername);
	return(28);
    }
    if(num_changer_loadports > 0){
      if(!freeloadports_cmd){
	fprintf(stderr, T_("Error: No command configured in `%s' to determine the free loadports.\n"),
				changerconffile);
	return(35);
      }
      else{
	EM__(cptr = repl_substring(freeloadports_cmd, "%D", changername));
	free(freeloadports_cmd);
	freeloadports_cmd = cptr;
      }
    }
    if(!freeslots_cmd){
	fprintf(stderr, T_("Error: No command configured in `%s' to determine the free slots.\n"),
				changerconffile);
	return(29);
    }
    else{
	EM__(cptr = repl_substring(freeslots_cmd, "%D", changername));
	free(freeslots_cmd);
	freeslots_cmd = cptr;
    }
  }
				/* convert argument strings to data */
  if(cartstr){
    EM__(cartridges = sscan_Uns32Ranges_(cartstr, 1, num_cartridges, NULL, NULL));
    n_cartridges = num_Uns32Ranges(cartridges);
  }

  if(have_drive_arg > 0 && drivestr){
    EM__(drives = sscan_Uns32Ranges_(drivestr, 1, MAXINT, NULL, NULL));
    n_drives = num_Uns32Ranges(drives);
    if(n_drives < 1){
	massage_string(drivestr);
	if(!strcmp(drivestr, devicename)){
	  ZFREE(drives);
	  have_drive_arg = 0;	/* this makes the next if being true */
	}
	else{
	  fprintf(stderr, T_("Error: The argument to specify the drives could not be evaluated.\n"));
	  return(30);
	}
    }
  }
  if(have_drive_arg == 0){
    EM__(drives = add_to_Uns32Ranges(drives, 1, 1));	/* evtl eject all */
    n_drives = 1;
  }

  if(have_slot_arg > 0 && slotstr){
    EM__(slots = sscan_Uns32Ranges_(slotstr, 1, num_changer_slots, NULL, NULL));
    n_slots = num_Uns32Ranges(slots);
  }

  if(have_slot_arg == 0 && cctl_move){
    slots = get_free_slots(changername);
    i = num_Uns32Ranges(slots);
    if(n_cartridges > i){
	fprintf(stderr, T_("Error: Not enough free slots found.\n"));
	return(38);
    }
    while(num_Uns32Ranges(slots) > n_cartridges){
	EM__(slots = del_one_from_Uns32Ranges(slots,
			prev_in_Uns32Ranges(slots, 1, YES)));
    }
    n_slots = n_cartridges;
  }

  if(have_loadp_arg > 0 && loadportstr){
    EM__(loadports = sscan_Uns32Ranges_(loadportstr, 1, MAXINT, NULL, NULL));
    n_loadports = num_Uns32Ranges(loadports);
  }
  if(have_loadp_arg == 0){
    if(cctl_move){
	loadports = get_free_loadports(changername);
	i = num_Uns32Ranges(loadports);
	if(n_cartridges > i){
	  fprintf(stderr, T_("Error: Not enough free loadports found.\n"));
	  return(38);
	}
	while(num_Uns32Ranges(loadports) > n_cartridges){
	  EM__(loadports = del_one_from_Uns32Ranges(loadports,
			prev_in_Uns32Ranges(loadports, 1, YES)));
	}
	n_loadports = n_cartridges;
    }
    else{
	if(!cctl_list){
	  EM__(loadports = add_to_Uns32Ranges(loadports, 1, 1));
	  n_loadports = 1;
	}
    }
  }

  if(changerlock.lockfile)
    if(empty_string(changerlock.lockfile))
      changerlock.lockfile = NULL;
  if(changerlock.lockfile)
    massage_string(changerlock.lockfile);
  if(!changerlock.lockfile)
    changerlock.lockfile = select_lockfile(DEFAULT_CHANGERLOCKFILE);
  EEM__(repl_dirs(&changerlock.lockfile));

					/* check command semantics */
  if(cctl_move + cctl_inventory + cctl_label + cctl_list + cctl_eject
		+ cctl_names + cctl_attrs < 1 && have_loc_arg < 0){
    fprintf(stderr, T_("Error: At least one of the following flags must be supplied: iIlmPtaeN\n"));
    usage(argv[0], mode);
  }
  if(cctl_names && !cctl_list && num_cartnames && !cartstr){
    fprintf(stderr, T_("Error: If cartridges names are to be set, cartridges must be given with option -C\n"));
    usage(argv[0], mode);
  }
  if(cctl_inventory && cartstr){
    fprintf(stderr, T_("Error: The flags -%s and -%s do not make sense together.\n"),
		"i", "C");
    usage(argv[0], mode);
  }
  if(cctl_inventory && cctl_label){
    fprintf(stderr, T_("Error: The flags -%s and -%s are not allowed together.\n"),
		"i", "t");
    usage(argv[0], mode);
  }
  if(!cctl_names && num_cartnames && !cctl_attrs){
    fprintf(stderr,
	T_("Error: Additional arguments are allowed only in combination with -N.\n"));
    usage(argv[0], mode);
  }

  if(cctl_attrs){
    if(num_cartnames < 1){
	fprintf(stderr, T_("Error: -a requires an argument.\n"));
	usage(argv[0], mode);
    }
    attrstr = cartnames[0];
    cartnames++;
    num_cartnames--;
  }

  if(cctl_eject){
    if(cartstr){
	fprintf(stderr, T_("Error: The flags -%s and -%s are not allowed together.\n"),
		"e", "C");
	usage(argv[0], mode);
    }

    cctl_move = YES;	/* just set up the parameters to do moves */
    if(!drives){
	EM__(drives = add_to_Uns32Ranges(drives, 1, 1));
	n_drives = num_Uns32Ranges(drives);
    }					/* later: all drives */

    cartridges = NULL;
    for(i = 0; drives[i].first <= drives[i].last; i++){
      for(j = drives[i].first; j <= drives[i].last; j++){
	n = cart_in_location(LOC_DRIVE, j, changername);
	if(n > 0)
	  EM__(cartridges = add_to_Uns32Ranges(cartridges, n, n));
      }
    }

    if((n_cartridges = num_Uns32Ranges(cartridges)) < 1)
	return(0);				/* all drives empty */

    if(!slots){
	slots = get_free_slots(changername);
	while((n_slots = num_Uns32Ranges(slots)) > n_cartridges){
	  EM__(slots = del_one_from_Uns32Ranges(slots,
				prev_in_Uns32Ranges(slots, 1, YES)));
	}

	if(n_slots < n_cartridges){
	  fprintf(stderr, T_("Error: Not enough free slots found.\n"));
	  return(40);
	}
    }
  }

  if(have_loadp_arg >= 0 && num_changer_loadports < 1){
    fprintf(stderr, T_("Error: Changer does not have any loadport.\n"));
    return(34);
  }

  if(cctl_move || have_loc_arg >= 0){
    if(!n_slots && !n_loadports && !n_drives && have_loc_arg < 0){
	fprintf(stderr, T_("Error: Targets to move the cartridges to must be given using one of the flags: DLSP\n"));
	usage(argv[0], mode);
    }
  }
  if(cctl_move || have_loc_arg >= 0){
    if(!cartridges){
	fprintf(stderr, T_("Error: The cartridges must be specified using the flag -C\n"));
	usage(argv[0], mode);
    }

    if(slots && n_cartridges != n_slots){
	fprintf(stderr, T_("Error: Number of given slots (%d) is unequal to the number of given cartridges (%d).\n"),
				(int) n_slots, (int) n_cartridges);
	return(26);
    }
  }
  if(cctl_move || have_loc_arg >= 0){
    if(drives && n_cartridges != n_drives){
	fprintf(stderr, T_("Error: Number of given drives (%d) is unequal to the number of given cartridges (%d).\n"),
				(int) n_drives, (int) n_cartridges);
	return(26);
    }
    if(loadports && n_cartridges != n_loadports){
	fprintf(stderr, T_("Error: Number of given loadports (%d) is unequal to the number of given cartridges (%d).\n"),
			(int) n_loadports, (int) n_cartridges);
	return(26);
    }
  }

  if(cctl_label){
    if(!n_slots && !n_cartridges){
	fprintf(stderr, T_("Error: The slots or cartridges must be given using the -S or -C option.\n"));
	usage(argv[0], mode);
    }
    if(!n_slots){
	ZFREE(slots);
	for(i = 0; i < len_Uns32Ranges(cartridges); i++){
	  for(j = cartridges[i].first; j <= cartridges[i].last; j++){
	    get_location(j, &tgttype, NULL, &tgtnum);
	    if(tgttype != LOC_SLOT){
		fprintf(stderr,
			T_("Error: Cartridge %d is not in any media changer slot.\n"),
				(int) j);
		return(36);
	    }
	    EM__(slots = add_to_Uns32Ranges(slots, tgtnum, tgtnum));
	  }
	}
    }
    if(!n_cartridges){
	ZFREE(cartridges);
	for(i = 0; i < len_Uns32Ranges(slots); i++){
	  for(j = slots[i].first; j <= slots[i].last; j++){
	    tgtnum = cart_in_location(LOC_SLOT, j, changername);
	    if(tgtnum <= 0){
		fprintf(stderr,
			T_("Error: Cannot determine cartridge in slot %d.\n"),
				(int) j);
		return(39);
	    }
	    EM__(cartridges = add_to_Uns32Ranges(cartridges, tgtnum, tgtnum));
	  }
	}
    }

    if(n_slots != n_cartridges){
	fprintf(stderr, T_("Error: Number of given slots (%d) is unequal to the number of given cartridges (%d).\n"),
				(int) n_slots, (int) n_cartridges);
	return(26);
    }
  }
  if(cctl_inventory){
    if(n_cartridges || n_loadports){
	fprintf(stderr, T_("Error: The flags -C and -L are not allowed with -i.\n"));
	usage(argv[0], mode);
    }
    if(!n_slots){
	EM__(slots = add_to_Uns32Ranges(NULL, 1, num_changer_slots));
	n_slots = num_changer_slots;
    }
  }
  if(cctl_label){
    if(n_loadports){
	fprintf(stderr, T_("Error: The flag -L is not allowed with -t.\n"));
	usage(argv[0], mode);
    }
  }
					/* check against configuration */
  if(slots){
    if(next_in_Uns32Ranges(slots, num_changer_slots, NO) > num_changer_slots){
	fprintf(stderr, T_("Error: Changer has only %d slots.\n"),
					(int) num_changer_slots);
	return(35);
    }
  }
  if(loadports){
    if(next_in_Uns32Ranges(loadports, num_changer_loadports, NO)
					> num_changer_loadports){
	fprintf(stderr, T_("Error: Changer has only %d loadports.\n"),
					(int) num_changer_loadports);
	return(35);
    }
  }

  targets = NULL;		/* now we finally start to do something */

  if(cctl_move || have_loc_arg >= 0){
    tgttype = n_slots ? LOC_SLOT
		: (n_loadports ? LOC_LOADPORT
			: (n_drives ? LOC_DRIVE
				: LOC_EXTERNAL));

    switch(tgttype){
      case LOC_EXTERNAL:

	break;

      case LOC_SLOT:
	targets = slots;

	break;

      case LOC_DRIVE:
	targets = drives;

	break;

      case LOC_LOADPORT:
	targets = loadports;

	break;
    }
  }

  if((cctl_move || !targets) && have_loc_arg == 0){
	fprintf(stdout, T_("Please enter a line to describe the new location for the cartridges:\n"));

	while(!(locstr = fget_alloc_str(stdin)));
	while(chop(locstr));
  }

  if(cctl_move || have_loc_arg >= 0){
    if(tgttype != LOC_EXTERNAL){
	ZFREE(locstr);
	locstr = changername;
	tgtrange = 0;
	tgtnum = targets[tgtrange].first;
    }
  }

  if(cctl_inventory || cctl_invlbl || (list_mode & LIST_HWINV)
			|| (cctl_label && cctl_names))
    get_hwcartlabels(changername);

  if(!cctl_move && have_loc_arg >= 0){
    if(tgttype != LOC_EXTERNAL){
      overset = NULL;

      for(i = 0; i < len_Uns32Ranges(targets); i++){
	for(j = targets[i].first; j <= targets[i].last; j++){
	  cart = cart_in_location(tgttype, j, changername);

	  if(cart > 0)
	    EM__(overset = add_to_Uns32Ranges(overset, cart, cart));
	}
      }

      if(overset){
	overset = del_range_from_Uns32Ranges(overset, cartridges);
	if(len_Uns32Ranges(overset) > 0){
	  EM__(cptr = str_Uns32Ranges(overset, 0));

	  report_problem(T_("Warning: The location of the cartridges %s is set to %s.\n"),
				cptr, T_("unknown"));
	  free(cptr);

	  n = 0;
	  for(i = 0; i < len_Uns32Ranges(overset); i++){
	    for(j = overset[i].first; j <= overset[i].last; j++){
	      if(set_location(j, LOC_UNKNOWN, NULL, 0))
		n++;
	    }
	  }

	  if(n)
	    report_problem(T_("Error: Could not set locations correctly.\n"));

	  free(overset);
	}
      }
    }
  }
	
  if(cctl_move || have_loc_arg >= 0){
    for(i = 0; i < len_Uns32Ranges(cartridges); i++){
      for(j = cartridges[i].first; j <= cartridges[i].last; j++){
	if(cctl_move){
	  res = move_cartridge_to(j, tgttype, locstr, tgtnum);
	  if(res){
	    report_problem(T_("Error: Moving cartridge %d failed.\n"), j);
	  }
	}
	else{
	  res = set_location_msg(j, tgttype, locstr, tgtnum);
	}

	if(tgttype != LOC_EXTERNAL){
	  tgtnum++;
	  if(tgtnum > targets[tgtrange].last){
	    tgtrange++;
	    tgtnum = targets[tgtrange].first;
	  }
	}
      }
    }
  }

  if(cctl_inventory || cctl_label){
    free_slots = get_free_slots(changername);
    overset = NULL;

    for(i = 0; i < len_Uns32Ranges(slots); i++){
      for(j = slots[i].first; j <= slots[i].last; j++){
	if(in_Uns32Ranges(free_slots, j))
	  EM__(overset = add_to_Uns32Ranges(overset, j, j));
      }
    }
    if(len_Uns32Ranges(overset) > 0){
	EM__(cptr = str_Uns32Ranges(overset, 0));
	fprintf(stderr, T_("Warning: The following slots are empty: %s\n"), cptr);
	free(cptr);
	EM__(slots = del_range_from_Uns32Ranges(slots, overset));
    }
    ZFREE(overset);

    for(i = 0; i < len_Uns32Ranges(slots); i++){	/* which carts should be */
      for(j = slots[i].first; j <= slots[i].last; j++){		/* in the slots */
	cart = cart_in_location(LOC_SLOT, j, changername);
	if(cart > 0)
	  EM__(overset = add_to_Uns32Ranges(overset, cart, cart));
      }
    }

    if(cctl_label){
	cart = 0;
	tgtrange = 0;
	tgtnum = cartridges[tgtrange].first;
    }

    if(hwcartlabels && cctl_inventory){	/* check and correct the location data */
      n = 0;
      for(cartentptr = hwcartlabels; cartentptr->pos > 0; cartentptr++){
	cptr = NULL;
	i = cart_in_location(cartentptr->loctype, cartentptr->pos, changername);
	if(!specified_by_args(i, cartentptr->loctype, cartentptr->pos))
	  continue;
	j = get_cartbyname(cartentptr->label, YES);
	if(i > 0 && i != j)
	  n = set_location_msg(i, LOC_UNKNOWN, NULL, 0);
	if(j > 0 && i != j)
	  n = set_location_msg(j, cartentptr->loctype, changername, cartentptr->pos);
      }
      if(n)
	report_problem(T_("Error: Could not set locations correctly.\n"));
    }

    for(i = 0; i < len_Uns32Ranges(slots); i++){
      for(j = slots[i].first; j <= slots[i].last; j++){
	if(hwcartlabels && cctl_inventory && !forced){
	  if((k = get_cartbyhwloc(LOC_SLOT, j, changername)) >= 0){
	    if(overset)
		EM__(overset = del_one_from_Uns32Ranges(overset, k));
	    continue;				/* location already updated */
	  }
	}

	cart = cart_in_location(LOC_DRIVE, pos_in_changer, changername);
	if(cart > 0){
	  if(unload_cart_to_free_slot(cart, changername)){
	    fprintf(stderr, T_("Error: Cannot unload drive %d to a free slot.\n"),
				(int) pos_in_changer);
	    return(31);
	  }
	}

	if(genmove_drlock(0, LOC_SLOT, j, changername,
				LOC_DRIVE, pos_in_changer, changername)){
	  fprintf(stderr, T_("Error: Cannot move cartridge from slot %d to drive %d in %s, skipping this slot.\n"),
			(int) j, (int) pos_in_changer, changername);
	  continue;
	}

	if(cartins_gracetime > 0)
	  ms_sleep(cartins_gracetime * 1000);

	n = cart_in_location(LOC_SLOT, j, changername);
	if(n > 0){
	  n = set_location_msg(n, LOC_DRIVE, changername, pos_in_changer);
	}

	if(cctl_inventory){
	  query = only_read_label = YES;

	  cart = 0;
	}

	if(cctl_label){
	  if(!interactive)
	    dont_ask = YES;

	  forced = dont_ask;

	  label = scnd_label = cart = tgtnum;

	  tgtnum++;
	  if(tgtnum > cartridges[tgtrange].last){
	    tgtrange++;
	    tgtnum = cartridges[tgtrange].first;
	  }
	}

	label_valid = YES;
	if(label_tape(0, NULL)){	/* args are currently dummies */
	  if(cctl_inventory)
	    fprintf(stderr, T_("Info: Cannot read label of tape in slot %d.\n"),
				(int) j);
	  if(cctl_label)
	    fprintf(stderr, T_("Info: Labeling tape in slot %d failed.\n"),
				(int) j);
	  label_valid = NO;
	}
	else{
	  if(cctl_inventory){
	    cptr = strstr(infobuffer, CARTNOTEXT);
	    if(cptr){
	      cptr += strlen(CARTNOTEXT);
	      n = sscanf(cptr, "%d", &i1);
	      if(n > 0)
		cart = i1;
	    }
	  }

	  if(cart > 0){		/* cart is set in label_tape() */
	    n = set_location_msg(cart, LOC_DRIVE, changername, pos_in_changer);
	    if(!n){
		if(move_cartridge_to(cart, LOC_SLOT, changername, j))
		  return(32);

		if(overset)
		  EM__(overset = del_one_from_Uns32Ranges(overset, cart));

		k = get_cartbyhwloc(LOC_SLOT, j, changername);
		if(k > 0 && k != cart)
		  set_cartname(k, NULL, YES);
		cartentptr = get_cartentbyhwloc(LOC_SLOT, j, changername);
		if(cartentptr)
		  if(cart != get_cartbyname(cartentptr->label, YES))
		    set_cartname(cart, cartentptr->label, YES);
	    }
	  }
	  else{
	    fprintf(stderr, T_("Info: Cannot read label of tape in slot %d.\n"),
				(int) j);
	    label_valid = NO;
	  }	/* if else label could be evaluated */
	}	/* if else label could be read */

	if(!label_valid){	/* raw unload necessary */
	  if(genmove_drlock(cart, LOC_DRIVE, pos_in_changer, changername,
						LOC_SLOT, j, changername)){
		fprintf(stderr, T_("Error: Cannot unload drive %d to slot %d in %s.\n"),
				(int) pos_in_changer, (int) j, changername);
		return(33);
	  }
	}
      }		/* loop over range slots[i] */
    }		/* loop over range array slots */

    if((n = len_Uns32Ranges(overset)) > 0){
	EM__(cptr = str_Uns32Ranges(overset, 0));
	fprintf(stderr, T_("Info: Location of the cartridges %s is set to unknown.\n"),
					cptr);
	free(cptr);
	for(i = 0; i < n; i++){
	  for(j = overset[i].first; j <= overset[i].last; j++){
	    set_location_msg(j, LOC_UNKNOWN, NULL, 0);

	    n = get_cartbyhwloc(LOC_SLOT, j, changername);
	    if(n > 0)
		set_cartname(n, NULL, YES);
	  }
	}
    }
    ZFREE(overset);
  }		/* if cctl_inventory */

  if(cctl_attrs){
    cptr = attrstr;
    forever{
	n = j = 0;
	ZFREE(targets);
	cptr = rstr = first_nospace(cptr);
	if(*cptr == '*'){
	  cptr = first_nospace(++cptr);
	  rstr = "1-";
	  j = 1;
	}
	targets = sscan_Uns32Ranges(rstr, 1, num_cartridges, &n, j ? NULL : &cptr);
	if((!n && *cptr) || (n && !strchr(":=", *cptr))){
	  fprintf(stderr, T_("Error: Syntax error in `%s' at position %d.\n"),
			attrstr, cptr - attrstr + 1);
	  exit(45);
	}

	cptr = first_nospace(cptr + 1);
	defval = 1;
	if(*cptr == '!'){
	  defval = 0;
	  cptr = first_nospace(cptr + 1);
	}
	if(!strchr("rfo", tolower(*cptr))){
	  fprintf(stderr,
		T_("Error: Unknown specifier `%c' in `%s' at position %d.\n"),
			*cptr, attrstr, cptr - attrstr + 1);
	  exit(45);
	}
	while(*cptr && strchr("rfo", tolower(*cptr))){
	  k = *cptr;
	  cptr++;
	  tstr = first_nospace(cptr);
	  i = defval;
	  if(*tstr == '='){
	    cptr = first_nospace(tstr + 1);
	    if(!*cptr){
		fprintf(stderr, T_("Error: Syntax error in `%s' at position %d.\n"),
			attrstr, cptr - attrstr + 1);
		exit(42);
	    }
	    if(k == 'f'){
		i1 = n1 = 0;
		sscanf(cptr, "%d%n", &i1, &n1);
		i = i1;
		cptr += n1;
	    }
	    else{
		for(j = 0; j < strlen(cptr) && isalpha(cptr[j]); j++);
		i = is_yes(cptr, j);
		cptr += j;
	    }
	  }

	  switch(k){
	    case 'f':
		for(n = 0; n < len_Uns32Ranges(targets); n++){
		  for(j = targets[n].first; j <= targets[n].last; j++){
		    sprintf(nbuf, "%d", (int) i);
		    sprintf(tmpbuf, "%d:", (int) j);
		    if(kfile_insert(tape_usage_file, tmpbuf, nbuf,
				KFILE_LOCKED | KFILE_SORTN))
			fprintf(stderr, T_("Error: Cannot write the tape full counter to file\n"));
		  }
		}
		break;

	    case 'r':
		ro_tapes = get_readonly_tapes();
		if(i){
		  i = merge_Uns32Ranges(&ro_tapes, targets);
		}
		else{
		  i = (ro_tapes ? 1 : 0);
		  ro_tapes = del_range_from_Uns32Ranges(ro_tapes, targets);
		  i = (i && !ro_tapes);
		}
		if(!i){
		  i++;
		  tstr = str_Uns32Ranges(ro_tapes, 0);
		  if(tstr){
		    fd = set_wlock_forced(ro_tapes_file);
		    if(fd >= 0){
			i = write_asc_file_safe(ro_tapes_file, &tstr, 1);
			close(fd);
		    }
		    free(tstr);
		  }
		}
		ZFREE(ro_tapes);
		if(i){
		  fprintf(stderr,
			T_("Error: Cannot rewrite file `%s' to save the read-only tapes\n"),
				ro_tapes_file);
		}
		break;

	    case 'o':
		if(i){
		  i = reuse_tapes(rstr, YES);
		}
		else{
		  tstr = kfile_get(precious_tapes_file,
				"< * protected by admin * >", KFILE_LOCKED);
		  if(tstr){
		    prec_tapes = sscan_Uns32Ranges__(tstr,
					1, num_cartridges, NULL, NULL);
		    free(tstr);
		  }
		  EEM__(merge_Uns32Ranges(&prec_tapes, targets));
		  EM__(tstr = str_Uns32Ranges(prec_tapes, 0));
		  if(kfile_insert(precious_tapes_file,
			"< * protected by admin * >", tstr, KFILE_LOCKED | KFILE_SORT)){
		    fprintf(stderr, T_("Error: Cannot rewrite file `%s' to save the precious tapes\n"),
					precious_tapes_file);
		    exit(41);
		  }
		  free(tstr);
		  ZFREE(prec_tapes);
		}
		break;
	  }
	}

	if(isspace(*cptr))
	  continue;

	if(!*cptr)
	  break;
	if(*cptr != ',' && *cptr != ';'){
	  fprintf(stderr, T_("Error: Syntax error in `%s' at position %d.\n"),
			attrstr, cptr - attrstr + 1);
	  exit(43);
	}
	cptr++;
    }      
    ZFREE(targets);
  }

  if(cctl_invlbl){
    if(!hwcartlabels){
	fprintf(stderr, T_("Warning: Could not read any label in changer `%s'\n"),
		changername);
    }
    else{
	n = 0;
	for(cartentptr = hwcartlabels; cartentptr->pos > 0; cartentptr++){
	  i = cart_in_location(cartentptr->loctype, cartentptr->pos, changername);
	  if(!specified_by_args(i, cartentptr->loctype, cartentptr->pos))
	    continue;

	  j = get_cartbyname(cartentptr->label, YES);
	  if(j > 0 && j != i)
	    n |= set_cartname(j, NULL, YES);

	  if(i > 0 && i != j)
	    n |= set_cartname(i, cartentptr->label, YES);
	}
	if(n)
	  fprintf(stderr, T_("Error: Could not write labels to file.\n"));
    }
  }

  if(cctl_names && !cctl_list && num_cartnames){
    for(i = k = 0; i < len_Uns32Ranges(cartridges); i++){
      for(j = cartridges[i].first;
		j <= cartridges[i].last && k < num_cartnames; j++){
	sprintf(nbuf, "%d", (int) j);
	EM__(cptr = repl_substring(cartnames[k], "%C", nbuf));
	n = set_cartname(j, cptr, NO);
	free(cptr);
	if(n){
	  fprintf(stderr, T_("Error: Could not write names to file.\n"));
	  return(47);
	}
	if(k < num_cartnames - 1)
	  k++;
      }
    }
  }

  if(cctl_list){
    i = fsentry_access(cartloc_file, F_OK);
    j = fsentry_access(cartloc_file, R_OK);
    if(i == DONT_KNOW || (i == YES && j != YES))
	fprintf(stderr, T_("Warning: Cannot read file `%s' containing the cartridge locations.\n"),
			cartloc_file);
    i = fsentry_access(bytesontape_file, F_OK);
    j = fsentry_access(bytesontape_file, R_OK);
    if(i == DONT_KNOW || (i == YES && j != YES))
	fprintf(stderr, T_("Warning: Cannot read file `%s' containing usage related cartridge information.\n"),
			bytesontape_file);
    i = fsentry_access(tape_usage_file, F_OK);
    j = fsentry_access(tape_usage_file, R_OK);
    if(i == DONT_KNOW || (i == YES && j != YES))
	fprintf(stderr, T_("Warning: Cannot read file `%s' containing the cartridge full counters.\n"),
			tape_usage_file);
    i = fsentry_access(cartnames_file, F_OK);
    j = fsentry_access(cartnames_file, R_OK);
    if(i == DONT_KNOW || (i == YES && j != YES))
	fprintf(stderr, T_("Warning: Cannot read file `%s' containing the cartridge names and descriptions.\n"),
			cartnames_file);
    i = fsentry_access(ro_tapes_file, F_OK);
    j = fsentry_access(ro_tapes_file, R_OK);
    if(i == DONT_KNOW || (i == YES && j != YES))
	fprintf(stderr, T_("Warning: Cannot read file `%s' containing the readonly cartridges.\n"),
			ro_tapes_file);
    i = fsentry_access(precious_tapes_file, F_OK);
    j = fsentry_access(precious_tapes_file, R_OK);
    if(i == DONT_KNOW || (i == YES && j != YES))
	fprintf(stderr, T_("Warning: Cannot read file `%s' containing the cartridges protected from overwrite by client request.\n"),
			precious_tapes_file);

    if(list_mode == LIST_CARTLOCS){
      if(have_slot_arg < 0 && have_loadp_arg < 0){
	fprintf(stdout, "%-20s    %s\n------------------------------------------\n",
				T_("Cartridge"), T_("Location"));

	n = -1;
	cartlocs = kfile_getall(cartloc_file, &n, KFILE_LOCKED);
	for(i = 0; i < n; i++){
	  if(sscanf(cartlocs[i].key, "%d%n", &i1, &n1) > 0){
	    if(given_changer){
		cptr = strwordq(cartlocs[i].value, 0);
		if(cptr){
		  j = strcmp(given_changer, cptr);
		  free(cptr);
		  if(j)
		    continue;
		}
	    }

	    fprintf(stdout, "%-20d    %s\n", i1, cartlocs[i].value);
	  }
	}
      }

      if(have_slot_arg >= 0){
	free_slots = get_free_slots(changername);
	EM__(cptr = strapp(T_("Slots in "), changername));
	fprintf(stdout, "%-30s   %s\n-----------------------------------------------\n",
				cptr, T_("Cartridge"));
	free(cptr);

	tgtrange = 0;
	if(slots)
	  tgtnum = slots[tgtrange].first;
	else
	  tgtnum = 1;

	forever{
	  i = cart_in_location(LOC_SLOT, tgtnum, changername);
	  if(i > 0){
	    if(in_Uns32Ranges(free_slots, tgtnum))
		fprintf(stdout, "%-30d   - (%d?)\n", (int) tgtnum, (int) i);
	    else
		fprintf(stdout, "%-30d   %d\n", (int) tgtnum, (int) i);
	  }
	  else{
	    fprintf(stdout, "%-30d   %s\n", (int) tgtnum,
			(in_Uns32Ranges(free_slots, tgtnum) ? "-" : "?"));
	  }

	  if(slots){
	    tgtnum++;
	    if(tgtnum > slots[tgtrange].last){
		tgtrange++;
		if(tgtrange >= len_Uns32Ranges(slots))
		  break;
		tgtnum = slots[tgtrange].first;
	    }
	  }
	  else{
	    tgtnum++;
	    if(tgtnum > num_changer_slots)
		break;
	  }
	}
	ZFREE(free_slots);
      }

      if(have_loadp_arg >= 0){
	free_slots = get_free_loadports(changername);

	EM__(cptr = strapp(T_("Loadports in "), changername));
	fprintf(stdout, "%-30s   %s\n-----------------------------------------------\n",
				cptr, T_("Cartridge"));
	free(cptr);

	tgtrange = 0;
	if(loadports)
	  tgtnum = loadports[tgtrange].first;
	else
	  tgtnum = 1;

	forever{
	  i = cart_in_location(LOC_LOADPORT, tgtnum, changername);
	  if(i > 0){
	    if(in_Uns32Ranges(free_slots, tgtnum))
		fprintf(stdout, "%-30d   - (%d?)\n", (int) tgtnum, (int) i);
	    else
		fprintf(stdout, "%-30d   %d\n", (int) tgtnum, (int) i);
	  }
	  else{
	    fprintf(stdout, "%-30d   %s\n", (int) tgtnum,
			(in_Uns32Ranges(free_slots, tgtnum) ? "-" : "?"));
	  }

	  if(loadports){
	    tgtnum++;
	    if(tgtnum > loadports[tgtrange].last){
		tgtrange++;
		if(tgtrange >= len_Uns32Ranges(loadports))
		  break;
		tgtnum = loadports[tgtrange].first;
	    }
	  }
	  else{
	    tgtnum++;
	    if(tgtnum > num_changer_loadports)
		break;
	  }
	}
      }
    }
    else{
      if(list_mode & (LIST_HWINV | LIST_CARTNAMES | LIST_CARTATTRS)){
	UChar	***allstrings;
	Int32	num_allstr, ncols, maxlengths[32], spacing = 3;
	double	d1;

	memset(maxlengths, 0, sizeof(maxlengths));

	ncols = 4;
	n = 0;
	if(list_mode & LIST_CARTNAMES)
	  namelist = kfile_getall(cartnames_file, &n, KFILE_LOCKED | KFILE_SORT);

	if(list_mode & LIST_HWINV){
	  free_slots = get_free_slots(changername);
	  free_loadports = get_free_loadports(changername);
	  if(hwcartlabels){
	    EM__(unlabeled_in_slots = add_to_Uns32Ranges(NULL,
				1, streamers[curdevidx].changer->num_slots));
	    EM__(unlabeled_in_slots = del_range_from_Uns32Ranges(unlabeled_in_slots, free_slots));
	    if(streamers[curdevidx].changer->num_loadports > 0){
		EM__(unlabeled_in_loadports = add_to_Uns32Ranges(NULL,
				1, streamers[curdevidx].changer->num_loadports));
	    }
	    else{
		EM__(unlabeled_in_loadports = empty_Uns32Ranges());
	    }
	    EM__(unlabeled_in_loadports = del_range_from_Uns32Ranges(unlabeled_in_loadports, free_loadports));
	    for(i = 0; i < num_hwcartlabels; i++){
		hwcartlabels[i].num = get_cartbyname(hwcartlabels[i].label, YES);
		if(hwcartlabels[i].loctype == LOC_SLOT)
		  EM__(unlabeled_in_slots = del_one_from_Uns32Ranges(unlabeled_in_slots, hwcartlabels[i].pos));
		if(hwcartlabels[i].loctype == LOC_LOADPORT)
		  EM__(unlabeled_in_loadports = del_one_from_Uns32Ranges(unlabeled_in_loadports, hwcartlabels[i].pos));
	    }
	    n = num_Uns32Ranges(unlabeled_in_slots) + num_Uns32Ranges(unlabeled_in_loadports);
	    EM__(hwcartlabels = RENEWP(hwcartlabels, CartLabelEnt, num_hwcartlabels + 2 + n));
	    k = num_hwcartlabels;
	    for(i = 0; i < len_Uns32Ranges(unlabeled_in_slots); i++){
	      for(j = unlabeled_in_slots[i].first; j <= unlabeled_in_slots[i].last; j++){
		hwcartlabels[k].loctype = LOC_SLOT;
		hwcartlabels[k].pos = j;
		hwcartlabels[k].label = T_("<none>");
		hwcartlabels[k].num = cart_in_location(LOC_SLOT, j, changername);
		k++;
	      }
	    }
	    for(i = 0; i < len_Uns32Ranges(unlabeled_in_loadports); i++){
	      for(j = unlabeled_in_loadports[i].first; j <= unlabeled_in_loadports[i].last; j++){
		hwcartlabels[k].loctype = LOC_LOADPORT;
		hwcartlabels[k].pos = j;
		hwcartlabels[k].label = T_("<unlabeled>");
		hwcartlabels[k].num = cart_in_location(LOC_LOADPORT, j, changername);
		k++;
	      }
	    }
	    /* TODO: Drives !?! */

	    num_hwcartlabels = k;
	    hwcartlabels[k].pos = -1;

	    q_sort(hwcartlabels, num_hwcartlabels, sizeof(CartLabelEnt), cmp_hwcartlabel_by_loc);
	  }
	}

	num_allstr = n + num_hwcartlabels + 1;
	EM__(allstrings = NEWP(UChar **, num_allstr));
	for(i = 0; i < num_allstr; i++){
	  EM__(allstrings[i] = NEWP(UChar *, ncols));
	  memset(allstrings[i], 0, ncols * sizeof(UChar *));
	}
	i = 0;
	if(list_mode & LIST_HWINV){
	  allstrings[0][i++] = T_("Location");
	  if(!(list_mode & LIST_CARTNAMES))
	    allstrings[0][i++] = T_("Label");
	}
	if(list_mode & LIST_CARTNAMES){
	  allstrings[0][i++] = T_("Cartridge");
	  allstrings[0][i++] = T_("Label");
	  allstrings[0][i++] = T_("Description");
	}

	j = 1;
	if((list_mode & LIST_HWINV)){
	  for(i = 0; i < num_hwcartlabels; i++){
	    i1 = hwcartlabels[i].num;
	    if(i1 > 0)
		sprintf(nbuf, "%d", i1);
	    else
		strcpy(nbuf, T_("?"));

	    if(specified_by_args(i1, hwcartlabels[i].loctype,
				hwcartlabels[i].pos)){
		k = 0;
		EM__(allstrings[j][k++] = changerlocstr(changername,
			hwcartlabels[i].loctype, hwcartlabels[i].pos));
		if(list_mode & LIST_CARTNAMES){
		  EM__(allstrings[j][k++] = strdup(nbuf));
		  strcat(nbuf, ":");
		}
		allstrings[j][k++] = hwcartlabels[i].label;
		if((list_mode & LIST_CARTNAMES) && i1 > 0){
		  if( (cptr = get_var(namelist, nbuf, NO)) )
		    if( (cptr = sscanwordq(cptr, cptr)) )
			allstrings[j][k++] = cptr;
		}
		j++;
	    }
	  }
	}
	if((list_mode & LIST_CARTNAMES) && !(list_mode & LIST_HWINV)){
	  q_sort(namelist, n, sizeof(namelist[0]), cmp_KeyValue_bykeyn);
	  for(i = 0; i < n; i++){
	    if(sscanf(cptr = namelist[i].key, "%d%n", &i1, &n1) > 0){
	      if(specified_by_args(i1, -1, -1)){
		if(word_count(namelist[i].value) > 0){
		  allstrings[j][0] = namelist[i].key;
		  allstrings[j][0][n1] = '\0';
		  allstrings[j][1] = cptr = namelist[i].value;
		  if( (cptr = sscanwordq(cptr, cptr)) )
		    allstrings[j][2] = cptr;
		  if(word_count(allstrings[j][1]) > 0
				|| word_count(allstrings[j][2]) > 0)
		    j++;
		  else
		    memset(allstrings[j], 0, ncols * sizeof(UChar *));
		}
	      }
	    }
	  }
	}

	if(list_mode & LIST_CARTATTRS){
	  ZFREE(targets);	/* collect tape numbers to report about */
	  ro_tapes = get_readonly_tapes();
	  alltapes = dup_Uns32Ranges(ro_tapes);
	  tape_uses = kfile_getall(tape_usage_file, &n, KFILE_LOCKED | KFILE_SORT);
	  bytesontape = kfile_getall(bytesontape_file, &n, KFILE_LOCKED | KFILE_SORT);
	  keyvallist[0] = tape_uses;
	  keyvallist[1] = bytesontape;
	  for(i = 0; i < 2; i++){
	    if(keyvallist[i]){
	      for(kvptr = keyvallist[i]; kvptr->key; kvptr++){
		if(sscanf(kvptr->key, "%d", &i1) > 0)
		  EM__(alltapes = add_to_Uns32Ranges(alltapes, i1, i1));
	      }
	    }
	  }
	  prec_tapesf = kfile_getall(precious_tapes_file, &n, KFILE_LOCKED | KFILE_SORT);
	  if(prec_tapesf){
	    for(kvptr = prec_tapesf; kvptr->key; kvptr++){
	      tmpr = sscan_Uns32Ranges__(kvptr->value, 1, num_cartridges, NULL, NULL);
	      if(tmpr){
		EEM__(merge_Uns32Ranges(&prec_tapes, tmpr));
		free(tmpr);
	      }
	    }
	  }
	  kfile_freeall(prec_tapesf, n);

	  n = num_Uns32Ranges(alltapes);

	  ncols = 7;
	  for(i = 0; i < num_allstr; i++)
	    ZFREE(allstrings[i]);
	  num_allstr = n + 1;
	  EM__(allstrings = RENEWP(allstrings, UChar **, num_allstr));
	  memset(allstrings, 0, sizeof(UChar **) * num_allstr);
	  EM__(allstrings[0] = NEWP(UChar *, ncols));
	  memset(allstrings[0], 0, sizeof(UChar *) * ncols);
	  allstrings[0][0] = T_("Cartridge");
	  allstrings[0][1] = T_("Used");
	  allstrings[0][2] = T_("Full");
	  allstrings[0][3] = T_("Read-only");
	  allstrings[0][4] = T_("Overwrite");
	  allstrings[0][5] = T_("Times Full");
	  allstrings[0][6] = T_("Last Writing Time");
	  j = 1;
	  q_sort(alltapes, len_Uns32Ranges(alltapes), sizeof(alltapes[0]),
				cmp_Uns32Ranges);
	  for(i = 0; i < len_Uns32Ranges(alltapes); i++){
	    for(cart = alltapes[i].first; cart <= alltapes[i].last; cart++){
		EM__(allstrings[j] = NEWP(UChar *, ncols));
		memset(allstrings[j], 0, sizeof(UChar *) * ncols);
		get_location(cart, &tgttype, NULL, &tgtnum);
		if(!specified_by_args(cart, tgttype, tgtnum))
		  continue;

		sprintf(nbuf, "%d", (int) cart);
		EM__(allstrings[j][0] = strdup(nbuf));
		strcat(nbuf, ":");
		cptr = get_var(bytesontape, nbuf, NO);
		n = n1 = i1 = -1;
		if(cptr){
		  n = sscanf(cptr, "%lf%*d%d%n", &d1, &i1, &n1);
		  if(n > 0){
		    si_symvalstr(rbuf, d1, 4, 0.5, NO, YES);
		    EM__(allstrings[j][1] = strdup(rbuf));
		  }
		}
		if(n > 1){
		  allstrings[j][2] = (i1 ? T_("Yes") : T_("No"));
		  lastwrtime = strint2time(cptr + n1);
		  allstrings[j][6] = (lastwrtime == UNSPECIFIED_TIME ?
				T_("?") : strdup(timestr(lastwrtime)));
		}
		else
		  allstrings[j][2] = T_("?");

		if(!allstrings[j][6])
		  allstrings[j][6] = T_("?");

		allstrings[j][3] = (in_Uns32Ranges(ro_tapes, cart) ?
				T_("Yes") : T_("No"));

		allstrings[j][4] = (in_Uns32Ranges(prec_tapes, cart) ?
				T_("No") : T_("Yes"));

		if( (allstrings[j][5] = get_var(tape_uses, nbuf, NO)) ){
		  massage_string(allstrings[j][5]);
		}
		else{
		  sprintf(nbuf, "%d", 0);
		  EM__(allstrings[j][5] = strdup(nbuf));
		}

		j++;
	    }
	  }
	  spacing = 1;
	}

	for(; j < num_allstr; j++)
	  ZFREE(allstrings[j]);

	for(i = 0; i < num_allstr; i++){
	  if(allstrings[i]){
	    for(j = 0; j < ncols; j++){
	      if(allstrings[i][j]){
		k = strlen(allstrings[i][j]);
		if(k > maxlengths[j])
		  maxlengths[j] = k;
	      }
	    }
	  }
	}

	for(i = k = n1 = 0; i < ncols; i++){
	  if(allstrings[0][i]){
	    k = i + 1;
	    n1 += maxlengths[i] + spacing;
	    printf("%*s%-*s", i ? spacing : 0, "",
			maxlengths[i], allstrings[0][i]);
	  }
	}
	printf("\n");
	for(i = spacing; i < n1; i++)
	  printf("-");
	printf("\n");
	for(i = 1; i < num_allstr; i++){
	  if(allstrings[i]){
	    for(j = 0; j < k; j++){
		printf("%*s%-*s", j ? spacing : 0, "", maxlengths[j],
			(allstrings[i][j] ? (char *) allstrings[i][j] : ""));
	    }
	    printf("\n");
	  }
	}

	ZFREE(prec_tapes);
      }
      if(list_mode & LIST_CARTRESRVS){
	cptr = T_("Cartridge Reservations");
	printf("%s\n", cptr);
	n = strlen(cptr);
	for(i = 0; i < n; i++)
	  printf("-");
	printf("\n");

	prec_tapesf = kfile_getall(precious_tapes_file, &n, KFILE_LOCKED | KFILE_SORT);
	if(prec_tapesf){
	  for(kvptr = prec_tapesf; kvptr->key; kvptr++){
	    tmpr = sscan_Uns32Ranges__(kvptr->value, 1, num_cartridges, NULL, NULL);
	    if(tmpr){
		EEM__(merge_Uns32Ranges(&prec_tapes, tmpr));
		free(tmpr);
	    }
	  }
	  if(cartridges){
	    tmpr = common_Uns32Ranges(cartridges, prec_tapes);
	    free(prec_tapes);
	    prec_tapes = tmpr;
	  }

	  for(i = 0; prec_tapes[i].first <= prec_tapes[i].last; i++){
	    for(j = prec_tapes[i].first; j <= prec_tapes[i].last; j++){
		printf("%d:\n", (int) j);
		for(kvptr = prec_tapesf; kvptr->key; kvptr++){
		  tmpr = sscan_Uns32Ranges__(kvptr->value, 1, num_cartridges, NULL, NULL);
		  if(tmpr){
		    if(in_Uns32Ranges(tmpr, j))
			printf("\t%s\n", kvptr->key);
		    free(tmpr);
		  }
		}
	    }
	  }

	  kfile_freeall(prec_tapesf, n);
	}
      }
    }
  }

  return(0);
}

static int
cart_is(Int32 argc, UChar ** argv)
{
  UChar		keystr[64], numstr[64];

  if(!have_cartis_cart && cartis_wcart <= 0)
    usage(argv[0], mode);

  if(rewrite_tapeposfile_msg())
    return(2);

  if(have_cartis_cart && cartis_cart > 0){
    sprintf(numstr, "%d %d", (int) cartis_cart, 1);
    if(kfile_insert(tapeposfile, devicename, numstr, KFILE_LOCKED)){
	fprintf(stderr, T_("Error: Cannot write file %s to save the tape position.\n"),
			tapeposfile);
	return(3);
    }
  }
  if(cartis_wcart > 0 && cartis_wfile > 0){
    sprintf(keystr, "%d:", (int) cartridge_set);
    sprintf(numstr, "%d %d", (int) cartis_wcart, (int) cartis_wfile);
    if(kfile_insert(tapeposfile, keystr, numstr, KFILE_LOCKED)){
	fprintf(stderr, T_("Error: Cannot write file %s to save the tape position.\n"),
			tapeposfile);
	return(3);
    }
  }

  return(0);
}
