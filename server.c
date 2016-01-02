/****************** Start of $RCSfile: server.c,v $  ****************
*
* $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/server.c,v $
* $Id: server.c,v 1.19 2011/12/19 21:24:09 alb Exp alb $
* $Date: 2011/12/19 21:24:09 $
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

  static char * fileversion = "$RCSfile: server.c,v $ $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/server.c,v $ $Id: server.c,v 1.19 2011/12/19 21:24:09 alb Exp alb $ " PACKAGE " " VERSION_STRING;

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/utsname.h>
#ifdef	HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef	HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif
#include <sys/types.h>
#ifdef  HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#include <sys/socket.h>
#include <poll.h>
#include <netinet/in.h>
#ifdef	HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#include <netdb.h>
#ifdef  HAVE_NETINET_IN_SYSTM_H
#include <netinet/in_systm.h>
#endif
#ifdef  HAVE_NETINET_IP_H
#include <netinet/ip.h>
#endif
#include <signal.h>
#include <syslog.h>
#include <errno.h>
#ifdef  HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#ifdef	sun
#include <sys/ioctl.h>
#include <sys/mtio.h>
#endif
#ifdef	HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#if	defined(__hpux) || defined(hpux)
#ifdef	HAVE_SYS_MTIO_H
#include <sys/mtio.h>
#endif
#endif
#ifdef	HAVE_POSIX_THREADS
#ifdef	HAVE_PTHREAD_H
#include <pthread.h>
#endif
#endif

#include <x_types.h>
#include <sysutils.h>
#include <genutils.h>
#include <netutils.h>
#include <fileutil.h>
#include <afpacker.h>

#define	DEBUGFILE	"/tmp/afserver.log"

#include "cryptkey.h"
#include "budefs.h"
#include "afbackup.h"
#include "server.h"

#define	BUFFEREDOP	(streamermode & BUFFERED_OPERATION)
#define	AUTOCHCART	(streamermode & CHANGE_CART_ON_EOT)

#define	MAX_BUFFER_MEMORY	10485760	/* 10 MB */
#define	MAX_UNINTR_CYCLES	5

#define	PREF_CLIENT_DELAY	5

#define	BU_NO_LOCK		0
#define	BU_LOCKED_RD		1
#define	BU_LOCKED_WR		2
#define	BU_LOCKED		(BU_LOCKED_RD | BU_LOCKED_WR)
#define	BU_GOT_LOCK_RD		4
#define	BU_GOT_LOCK_WR		8
#define	BU_GOT_LOCK		(BU_GOT_LOCK_RD | BU_GOT_LOCK_WR)
#define	BU_CANT_LOCK		64

#define	BU_RDLCK		1
#define	BU_WRLCK		2


UChar		*programdir = "nowhere";

UChar		*default_configfilenames[] = {	\
				DEFAULT_SERVER_CONFIGFILES, NULL, NULL };
UChar		*configfilename = NULL;

UChar		*backuphome = NULL;

UChar		*cryptfile = NULL;

UChar		*serverid = NULL;

StreamerDevice	*streamers = NULL;
Int32		num_streamers = 0;
ChangerDevice	*changers = NULL;
Int32		num_changers = 0;
Int32		curdevidx = 0;

UChar		*devicesstr = NULL;

UChar		*devicename = NULL;
struct stat	devstatb;
Int32		devprobe_interval = DEFAULT_DEVPROBEITV;

UChar		*changername = NULL;

UChar		*storefile = NULL;
UChar		*vardir = NULL;
UChar		*bindir = NULL;
UChar		*libdir = NULL;
UChar		*confdir = NULL;

ReplSpec	replacements[] = {
	{ "%B",	NULL, &bindir },
	{ "%L",	NULL, &libdir },
	{ "%V",	NULL, &vardir },
	{ "%C", NULL, &confdir },
	};

UChar		streamermode = BUFFERED_OPERATION | CHANGE_CART_ON_EOT;
UChar		*streamerbuffer = NULL;
Uns32		streamerbufferlen = 0;
Uns32		streamerbuf_insert_idx = 0;
Uns32		streamerbuf_processed_idx = 0;
Flag 		streamerbuf_writing = NO;
Flag		streamerbuf_reading = NO;
Uns32		streamerbuf_highmark;
Uns32		streamerbuf_lowmark;
Uns32		streamerbuf_conf_size = 0;
Real		streamerbuf_conf_highmark = DEFAULT_TAPEBUFHIGHMARK;
Real		streamerbuf_conf_lowmark = DEFAULT_TAPEBUFLOWMARK;
int		streamerbuf_memfilefd = -1;
Uns32		streamerbuf_alloced_size = 0;
UChar		*streamerbuf_memfile = NULL;
UChar		*streamerbuf_confstr = NULL;

#define		num_blocks_in_streamerbuf	( \
			streamerbuf_insert_idx >= streamerbuf_processed_idx ? \
			streamerbuf_insert_idx - streamerbuf_processed_idx : \
			streamerbufferlen - streamerbuf_processed_idx \
				+ streamerbuf_insert_idx )

Flag		at_mediaend = NO;
Flag		force_cartchange = NO;
Flag		newfilerequested = NO;
Uns32		*tapefilenumbuf = NULL;
Uns32		*numbytesread = NULL;
Flag		*reqnewfilebuf = NULL;

UChar		*init_cmd = NULL;
UChar		*exit_cmd = NULL;

UChar		*setcartcmd = NULL;
UChar		*nextcartcmd = NULL;
UChar		*setfilecmd = NULL;
UChar		*skipfilescmd = NULL;
UChar		*erasetapecmd = NULL;
UChar		*initmediacmd = NULL;
UChar		*weofcmd = NULL;
UChar		*tapefullcmd = NULL;

UChar		*tapeposfile = NULL;
UChar		*bytesontape_file = NULL;

UChar		*loggingfile = NULL;
UChar		*syslog_ident = NULL;
UChar		*statusfile = NULL;
Flag		statusf_append = NO;

UChar		*precious_tapes_file = NULL;
UChar		*ro_tapes_file = NULL;
UChar		*cartorder_file = NULL;
UChar		*tape_usage_file = NULL;
UChar		*used_blocksizes_file = NULL;
UChar		*client_message_file = NULL;

UChar		*user_to_inform = NULL;
UChar		*mail_program = NULL;

Flag		cartridge_handler = NO;
Int32		num_cartridges = 1;
Flag		full_append = NO;
Flag		var_append = NO;

UChar		*cartsetstr = NULL;
Uns32Range	**cartridge_sets = NULL;
Int32		*endcarts = NULL;
Uns32		num_cartsets = 0;
UChar		**cartset_clients = NULL;
Int32		active_cartset = 0;	/* index i.e. cart set - 1 */

Flag		reject_unlabeled = NO;
Flag		prefer_cart_in_changer = NO;

Int32		conf_tapeblocksize = DEFAULT_TAPEBLOCKSIZE;
Int32		tapeblocksize = DEFAULT_TAPEBLOCKSIZE;
Int32		lbl_tapeblocksize = -1;
Uns32		maxbytes_per_file = DEFAULT_MAXBYTESPERFILE;

Real64		*maxbytes_per_tape = NULL;
UChar		*maxbytes_per_tape_str = NULL;
Real64		bytes_on_tape = 0.0;

Uns32Range	*written_tapes = NULL;

UChar		*pref_client_file = NULL;

int		commrfd = -1;
int		commwfd = -1;
char		*remotehost = "<unknown>";
UChar		remoteuser[256] = "";
UChar		*clientuname = &(remoteuser[128]);
UChar		client_id[128] = "";

int		lfd = -1;

int		tapefd = -1;

Flag		nobuffering = NO;

UChar		*tapebuffer = NULL;
Int32		outptr = 0;
UChar		*inputbuffer = NULL;
Int32		tapeptr = 0;
Flag		endofrec = NO;
Int32		buff_tape_ptr = 0;
Int32		num_transf_bytes_valid = 0;
Int32		commbufsiz = DEFCOMMBUFSIZ;
Int32		bytes_in_buffer = 0;

UChar		*iauxtapebuffer = NULL;
Int32		iauxtapebufferlen = 0;

Uns32		write_rate = 0;
time_t		write_meas_time = 0;
Uns32		read_rate = 0;
time_t		read_meas_time = 0;

time_t		actime;

UChar		*infobuffer = NULL;
Int32		infobuffersize;
Flag		have_infobuffer = NO;
Flag		tried_infobuffer = NO;
Int32		tape_label_cartno;
Int32		scnd_tape_label_cartno;
Int32		tried_tapeblocksize = 0;
UChar		*tried_tapeblock = NULL;

Int32		tapeaccessmode = NOT_OPEN;	/* reflects the success of a
						* previous open tape command */
Int32		actfilenum = 0;
Int32		actcart = 1;
Int32		*insfilenum = NULL;
Int32		*inscart = NULL;
Int32		rdfilenum = 0;
Int32		rdcart = 1;

UChar		*tlbl_comment = NULL;		/* tape label comment */
Int32		tlbl_cartno = 0;		/* for consistency check */

Uns32		cartins_gracetime = DEFAULT_CARTGRACETIME;
Uns32		devunavail_send_mail = DEFAULT_DEVUNAVSENDMAIL;
Uns32		devunavail_give_up = DEFAULT_DEVUNAVGIVEUP;

Uns32		bytes_in_tapefile = 0;
Uns32		bytes_read = 0;

struct utsname	unam;

Flag		interrupted = NO;
int		pgid_to_kill_on_interrupt = -1;

Flag		tape_moved = NO;
Flag		tape_rewound = NO;
Flag		media_initialized = NO;

Flag		filenum_incr_by_eof = NO;

UChar		*cartctlcmd = NULL;

char		**gargv;
int		gargc;
UChar		*programpath = NULL;

struct stat	gstatb;			/* global multipurpose stat buffer */
struct timeval	null_timeval;

typedef	struct __lockData	{
  UChar		*lockfile;
  int		lockfd;
  UChar		locked;
}	LockData;

LockData	g_lock;
LockData	cart_lock;

Flag		slavemode = NO;

Flag		exiting = NO;
Flag		exiting_on_cerr = NO;

UChar		edebug = 0;

#ifdef	HAVE_POSIX_THREADS

pthread_cond_t	can_write_to_buffer, buffer_contains_data;

pthread_mutex_t	may_reconfig_buffer_mutex, may_modify_buffer_ptrs;
pthread_mutex_t	tapepos_mutex;

pthread_t	buffer_thread;
Flag		buffer_thread_running = NO;
Flag		buffer_thread_initialized = NO;
Flag		buffer_must_end = NO;

#endif

#define	EM__(nmem)	{ nomemfatal(nmem); }
#define	EEM__(st)	{ if(st) nomemfatal(NULL); }
#define ER__(func, i)   { if( (i = (func)) ) return(i); }

ParamFileEntry	entries[] = {
	{ &serverid, NULL,
	(UChar *) "^[ \t]*[Ss]erver[-_ \t]*[Ii]denti\\(ty\\|fier\\):?[ \t]*",
		TypeUCharPTR	},
	{ &nextcartcmd, NULL,
	(UChar *) "^[ \t]*[Cc]hange[-_ \t]*[Cc]artr?i?d?g?e?[-_ \t]*[Cc]o?mm?a?n?d:?[ \t]*",
		TypeUCharPTR	},
	{ &setcartcmd, NULL,
	(UChar *) "^[ \t]*[Ss]et[-_ \t]*[Cc]artr?i?d?g?e?[-_ \t]*[Cc]o?mm?a?n?d:?[ \t]*",
		TypeUCharPTR	},
	{ &init_cmd, NULL,
	(UChar *) "^[ \t]*[iI]niti?a?l?i?z?a?t?i?o?n?[-_ \t]*[Cc]o?mm?a?n?d:?[ \t]*",
		TypeUCharPTR	},
	{ &exit_cmd, NULL,
	(UChar *) "^[ \t]*[eE]xit[-_ \t]*[Cc]o?mm?a?n?d:?[ \t]*",
		TypeUCharPTR	},
	{ &setfilecmd, NULL,
	(UChar *) "^[ \t]*[Ss]et[-_ \t]*[Ff]ile[-_ \t]*[Cc]o?mm?a?n?d:?[ \t]*",
		TypeUCharPTR	},
	{ &skipfilescmd, NULL,
	(UChar *) "^[ \t]*[Ss]kip[-_ \t]*[Ff]iles?[-_ \t]*[Cc]o?mm?a?n?d:?[ \t]*",
		TypeUCharPTR	},
	{ &erasetapecmd, NULL,
	(UChar *) "^[ \t]*[Ee]rase[-_ \t]*[Tt]ape[-_ \t]*[Cc]o?mm?a?n?d:?[ \t]*",
		TypeUCharPTR	},
	{ &initmediacmd, NULL,
	(UChar *) "^[ \t]*[Ii]niti?a?l?i?z?e?[-_ \t]*[Mm]edia[-_ \t]*[Cc]o?mm?a?n?d:?[ \t]*",
		TypeUCharPTR	},
	{ &weofcmd, NULL,
	(UChar *) "^[ \t]*[Ww]r?i?t?e?[-_ \t]*[Ee][Oo][Ff][-_ \t]*[Cc]o?mm?a?n?d:?[ \t]*",
		TypeUCharPTR	},
	{ &tapefullcmd, NULL,
	(UChar *) "^[ \t]*[Tt]ape[-_ \t]*[Ff]ull[-_ \t]*[Cc]o?mm?a?n?d:?[ \t]*",
		TypeUCharPTR	},
	{ &tapeposfile, NULL,
	(UChar *) "^[ \t]*[Tt]ape[-_ \t]*[Pp]osi?t?i?o?n?[-_ \t]*[Ff]ile:?[ \t]*",
		TypeUCharPTR	},
	{ &devicesstr, NULL,
	(UChar *) "^[ \t]*[Bb]ackup[-_ \t]*[Dd]evi?c?e?s?:?[ \t]*",
		TypeUCharPTR	},
	{ &streamerbuf_confstr, NULL,
	(UChar *) "^[ \t]*\\([Ss]treamer\\|[Tt]ape\\)[-_ \t]*[Bb]uffer:?",
		TypeUCharPTR	},
	{ &devprobe_interval, NULL,
	(UChar *) "^[ \t]*[Dd]evi?c?e?[-_ \t]*[Pp]rob\\(e\\|ing\\)[-_ \t]*[Ii]nterval:?",
		TypeInt32	},
	{ &full_append, NULL,
	(UChar *) "^[ \t]*\\([Ff]ull[-_ \t]*\\)?[Aa]ppend\\([-_ \t]*[Mm]ode\\)?:?",
		TypeFlag	},
	{ &var_append, NULL,
	(UChar *) "^[ \t]*[Vv]ar\\(iable\\)?[-_ \t]*[Aa]ppend\\([-_ \t]*[Mm]ode\\)?:?",
		TypeFlag	},
	{ &prefer_cart_in_changer, NULL,
	(UChar *) "^[ \t]*[Pp]refer[-_ \t]*[Cc]artr?i?d?g?e?s?[-_ \t]*[Ii]n[-_ \t]*[Cc]hanger:?",
		TypeFlag	},
	{ &loggingfile, NULL,
	(UChar *) "^[ \t]*[Ll]ogg?i?n?g?[-_ \t]*[Ff]ile:?[ \t]*",
		TypeUCharPTR	},
	{ &statusfile, NULL,
	(UChar *) "^[ \t]*[Ss]tatus[-_ \t]*[Ff]ile:?[ \t]*",
		TypeUCharPTR	},
	{ &g_lock.lockfile, NULL,
	(UChar *) "^[ \t]*[Ll]ocki?n?g?[-_ \t]*[Ff]ile:?[ \t]*",
		TypeUCharPTR	},
	{ &user_to_inform, NULL,
	(UChar *) "^[ \t]*[Uu]ser[-_ \t]*[Tt]o[-_ \t]*[Ii]nfor?m?:?[ \t]*",
		TypeUCharPTR	},
	{ &mail_program, NULL,
	(UChar *) "^[ \t]*[Mm]ail[-_ \t]*[Pp]rogram:?[ \t]*",
		TypeUCharPTR	},
	{ &cartridge_handler, NULL,
	(UChar *) "^[ \t]*[Cc]artr?i?d?g?e?[-_ \t]*[Hh]andler:?[ \t]*",
		TypeFlag	},
	{ &reject_unlabeled, NULL,
	(UChar *) "^[ \t]*[Rr]eject[-_ \t]*[Uu]nlabeled\\([-_ \t]*[Tt]apes?\\)?:?[ \t]*",
		TypeFlag	},
	{ &num_cartridges, NULL,
	(UChar *) "^[ \t]*[Nn]umb?e?r?[-_ \t]*[Oo]f[-_ \t]*[Cc]artr?i?d?g?e?s[-_ \t]*:?",
		TypeInt32	},
	{ &endcarts, &num_cartsets,
	(UChar *) "^[ \t]*[Ll]ast[-_ \t]*[Cc]artr?i?d?g?e?s[-_ \t]*:?",
		TypeInt32PTR	},
	{ &cartsetstr, NULL,
	(UChar *) "^[ \t]*[Cc]artr?i?d?g?e?[-_ \t]*[Ss]ets:?",
		TypeUCharPTR	},
	{ &conf_tapeblocksize, NULL,
	(UChar *) "^[ \t]*[Tt]ape[-_ \t]*[Bb]lock[-_ \t]*[Ss]ize:?",
		TypeInt32	},
	{ &maxbytes_per_file, NULL,
	(UChar *) "^[ \t]*[Mm]axi?m?u?m?[-_ \t]*[Bb]ytes[-_ \t]*[Pp]er[-_ \t]*[Ff]ile:?",
		TypeUns32	},
	{ &maxbytes_per_tape_str, NULL,
	(UChar *) "^[ \t]*[Mm]axi?m?u?m?[-_ \t]*[Bb]ytes[-_ \t]*[Pp]er[-_ \t]*[Tt]apes?:?",
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
	{ &programdir, NULL,
	(UChar *) "^[ \t]*[Pp]rogram[-_ \t]*[Dd]ire?c?t?o?r?y?:?[ \t]*",
		TypeUCharPTR	},
	{ &cryptfile, NULL,
	(UChar *) "^[ \t]*\\([Ee]n\\)?[Cc]rypti?o?n?[-_ \t]*[Kk]ey[-_ \t]*[Ff]iles?:?[ \t]*",
		TypeUCharPTR	},
	{ &vardir, NULL,
	(UChar *) "^[ \t]*[Vv][Aa][Rr][-_ \t]*[Dd]ire?c?t?o?r?y?:?[ \t]*",
		TypeUCharPTR	},
	};

UChar	*lockdirs[] = DEFAULT_LOCKDIRS;

#define	repl_dirs(string)	repl_substrings((string), replacements,	\
		sizeof(replacements) / sizeof(replacements[0]))

#define	TAPE_NOT_CLOSED	\
		(tapeaccessmode != NOT_OPEN && tapeaccessmode != CLOSED_NOTREW)
#define	TAPE_CLOSED	\
		(tapeaccessmode == NOT_OPEN || tapeaccessmode == CLOSED_NOTREW)


void	do_exit(int);
Int32	set_cartridge_cmd(Int32);
Int32	set_cartridge(Int32, Flag);
Int32	set_filenum_cmd(Int32);
Int32	set_filenum(Int32, Flag, Flag);
Int32	update_devicesettings(Int32, Flag);
Int32	skip_files_cmd(Int32);
Int32	skip_files(Int32);
Int32	set_cartset_cmd(Int32);
Int32	poll_device_cmd(UChar *, Flag, Int32);
Int32	closetape(Flag);
Int32	closetape_cmd();
Int32	closetapen_cmd();
Int32	opentapewr_cmd(Uns32);
Int32	opentapewr(Uns32);
Int32	opentaperd_cmd(Uns32);
Int32	opentaperd(Uns32);
Int32	read_from_tape();
Int32	erasetape();
Int32	erasetape_cmd();
Int32	requestnewcart_cmd();
Int32	rewindtape(Flag, Flag);
Int32	initmedia(Int32);
Int32	sendtapepos();
Int32	sendtapewrpos();
Int32	sendtaperdpos();
Int32	sendwrtapes(Uns32Range *);
Int32	sendclientstapes();
Int32	sendvardata(Uns32, Int32, UChar *, Int32, UChar *);
Int32	osendtapepos();
Int32	osendtapewrpos();
Int32	setnumwritevalid();
Int32	set_commbufsiz(Int32);
Int32	sendint(Int32, Int32);
Int32	sendifbusy();
Int32	flush_buffer_to_tape(UChar *, Int32 *, Int32 *);
Int32	write_tape_buffer(Int32 *, Int32 *);
Int32	get_tape_buffer(UChar *, Int32 *, Int32 *);
Int32	write_infobuffer(Int32);
Int32	goodbye(Int32);
Int32	goodbye_cmd();
Int32	read_tapepos_file();
Int32	write_tapepos_file();
Int32	write_tapepos_file_msg();
Int32	get_bytes_per_tape_msg(UChar *, Int32, Real64 *, Int32 *, Flag *, time_t *);
Int32	save_bytes_per_tape_msg(UChar *, Int32, Real64, Int32, Flag, time_t);
Int32	save_cart_order(Int32, Int32, Int32);
Int32	run_tape_full_cmd(Int32, Int32);
Int32	run_tape_full_cmd_msg(Int32, Int32);
Int32	tape_full_actions(Int32);
Int32	find_next_free_pos(Int32 *, Int32 *, Int32, Int32);
Int32	next_read_pos(Int32 *, Int32 *, Int32, Int32);
Int32	can_overwrite_cart(Flag *, Int32);
Int32	write_to_tape();
Int32	client_backup_cmd(Int8);
Int32	send_clientmsg();
Int32	proof_authentication();
Int32	wait_for_service(Int8, Flag);
void	sig_handler();
Int32	create_streamer_buffer();
void	dismiss_streamer_buffer();
Flag	check_interrupted();
Int32	set_lock(LockData *, Int8);
Int32	release_lock(LockData *);
void	register_pref_serv();
Int32	get_input(UChar *);
Int32	process_tape_ops(Flag, Flag);
Int32	check_access_cmd();
Int32	change_tapeblocksize(Int32, UChar **, Int32 *, Flag *);
Int32	find_tape_blocksize(UChar *, UChar **, Flag);
#ifdef	HAVE_POSIX_THREADS
Int32	start_buffer_thread();
Int32	end_buffer_thread();
#endif

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
fatal(UChar * msg)
{
  logmsg(LOG_CRIT, "%s", msg);

  do_exit(1);
}

void
nomemfatal(void * ptr)
{
  if(!ptr)
    fatal("Error: Cannot allocate memory.\n");
}

void
do_exit(int exitst)
{
  if(exiting){
    DB("Internal error, do_exit loops.\n", 0, 0, 0);

    return;
  }

  exiting = YES;	/* just in case, if we have produced a loop */

  closetape(YES);

  if(streamerbuf_memfile)
    unlink(streamerbuf_memfile);

  register_pref_serv();

  release_lock(&g_lock);

  if(interrupted){
    logmsg(LOG_WARNING, T_("Interrupted, going to exit.\n"));
  }
  else{
    if(exitst)
	logmsg(LOG_ERR, T_("Exiting with bad status %d, most recent system error: %s\n"), exitst,
				strerror(errno));
  }

  exit(exitst);
}

static void
set_devices(Int32 idx)
{
  devicename = streamers[idx].devicename;

  if(streamers[idx].changer)
    changername = streamers[idx].changer->changername;
  else
    changername = NULL;
}

Int32
put_in_sh(UChar ** str)
{
  char	*cptr;

  return(0);

/* NOTREACHED */

  if(!str)
    return(0);
  if(!(*str))
    return(0);

  cptr = strchain("sh -c \"", *str, " 1>",
		(loggingfile ? loggingfile : (UChar *) "/dev/null"),
		"0>&1 2>&1\"", NULL);
  if(!cptr)
    return(-1);
  free(*str);
  *str = cptr;

  return(0);
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
	memmove(cptr, cptr2, strlen(cptr2) + 1);
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
	memmove(cptr, cptr2, strlen(cptr2) + 1);
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

void
set_defaults(Flag reconfig)
{
  ZFREE(cryptfile);
  ZFREE(serverid);
  ZFREE(devicesstr);
  devicename = NULL;
  changername = NULL;
  free_devices_and_changers(streamers, num_streamers, changers, num_changers);
  streamers = NULL;
  changers = NULL;
  num_changers = num_streamers = 0;
  devprobe_interval = DEFAULT_DEVPROBEITV;
  ZFREE(vardir);
  ZFREE(init_cmd);
  ZFREE(exit_cmd);
  ZFREE(setcartcmd);
  ZFREE(nextcartcmd);
  ZFREE(setfilecmd);
  ZFREE(skipfilescmd);
  ZFREE(erasetapecmd);
  ZFREE(initmediacmd);
  ZFREE(weofcmd);
  ZFREE(tapefullcmd);
  ZFREE(tapeposfile);
  ZFREE(bytesontape_file);
  ZFREE(loggingfile);
  ZFREE(syslog_ident);
  ZFREE(statusfile);
  ZFREE(precious_tapes_file);
  ZFREE(ro_tapes_file);
  ZFREE(cartorder_file);
  ZFREE(tape_usage_file);
  ZFREE(user_to_inform);
  ZFREE(mail_program);
  ZFREE(used_blocksizes_file);
  ZFREE(client_message_file);
  cartridge_handler = 0;
  num_cartridges = 1;
  reject_unlabeled = NO;
  prefer_cart_in_changer = NO;
  full_append = NO;
  var_append = NO;
  ZFREE(endcarts);
  free_array((UChar **) cartridge_sets, 0);
  cartridge_sets = NULL;
  ZFREE(cartsetstr);
  free_array((UChar **) cartset_clients, 0);
  cartset_clients = NULL;
  num_cartsets = 0;
  if(!reconfig)		/* current tapeblocksize must not change */
    tapeblocksize = DEFAULT_TAPEBLOCKSIZE;
  conf_tapeblocksize = DEFAULT_TAPEBLOCKSIZE;
  maxbytes_per_file = DEFAULT_MAXBYTESPERFILE;
  ZFREE(maxbytes_per_tape);
  ZFREE(maxbytes_per_tape_str);
  ZFREE(pref_client_file);
  ZFREE(infobuffer);
  ZFREE(insfilenum);
  ZFREE(inscart);
  cartins_gracetime = DEFAULT_CARTGRACETIME;
  devunavail_send_mail = DEFAULT_DEVUNAVSENDMAIL;
  devunavail_give_up = DEFAULT_DEVUNAVGIVEUP; 
  ZFREE(g_lock.lockfile);
  ZFREE(cartctlcmd);
  ZFREE(streamerbuf_confstr);
  ZFREE(streamerbuf_memfile);
  streamerbuf_conf_highmark = DEFAULT_TAPEBUFHIGHMARK;
  streamerbuf_conf_lowmark = DEFAULT_TAPEBUFLOWMARK;
}

Int32
serverconf(Flag reconfig)
{
  Int32		i, j, r = NO_ERROR;
  UChar		*cptr, *cptr2, **cpptr;
  int		i1;
  double	d1;
  struct stat	statb;

  set_defaults(reconfig);

  ER__(read_param_file(configfilename, entries,	/* read config file */
			sizeof(entries) / sizeof(entries[0]), NULL, NULL), i);

				  /* set defaults for not given parameters */
  if(cartsetstr)
    if(empty_string(cartsetstr))
      ZFREE(cartsetstr);
  if(num_cartsets < 1 && !cartsetstr){
    ZFREE(endcarts);

    EM__(cartridge_sets = NEWP(Uns32Range *, 2));
    cartridge_sets[1] = NULL;

    EM__(cartridge_sets[0] = add_to_Uns32Ranges(NULL, 1, num_cartridges));
    num_cartsets = 1;
  }

  if(!serverid){
    EM__(cptr = get_my_off_hn());
    EM__(serverid = strchain(cptr, ":", configfilename, NULL));
    free(cptr);
  }

  for(cptr = serverid; *cptr; cptr++)
    if(isspace(*cptr) || !isprint(*cptr))
	*cptr = '*';

  if(!nextcartcmd)
    EM__(nextcartcmd = strdup(DEFAULT_NEXTCARTCMD));
  if(!setfilecmd)
    EM__(setfilecmd = strdup(DEFAULT_SETFILECMD));
  if(!skipfilescmd)
    EM__(skipfilescmd = strdup(DEFAULT_SKIPFILESCMD));
  if(!weofcmd)
    EM__(weofcmd = strdup(DEFAULT_WEOFCMD));
  if(!tapeposfile)
    EM__(tapeposfile = strdup(DEFAULT_TAPEPOSFILE));
  if(!user_to_inform)
    EM__(user_to_inform = strdup(DEFAULT_USERTOINFORM));
  if(!mail_program)
    EM__(mail_program = strdup(DEFAULT_MAILPROGRAM));

  if(init_cmd)
    if(empty_string(init_cmd))
      ZFREE(init_cmd);
  if(exit_cmd)
   if(empty_string(exit_cmd))
      ZFREE(exit_cmd);
  if(setcartcmd)
    if(empty_string(setcartcmd))
      ZFREE(setcartcmd);
  if(erasetapecmd)
    if(empty_string(erasetapecmd))
      ZFREE(erasetapecmd);
  if(initmediacmd)
    if(empty_string(initmediacmd))
      ZFREE(initmediacmd);
  if(tapefullcmd)
    if(empty_string(tapefullcmd))
      ZFREE(tapefullcmd);
  if(loggingfile)
    if(empty_string(loggingfile))
      ZFREE(loggingfile);
  if(g_lock.lockfile)
    if(empty_string(g_lock.lockfile))
      ZFREE(g_lock.lockfile);
  if(cryptfile)
    if(empty_string(cryptfile))
      ZFREE(cryptfile);
  if(vardir)
    if(empty_string(vardir))
      ZFREE(vardir);
  if(statusfile)
    if(empty_string(statusfile))
      ZFREE(statusfile);
  if(vardir)
    massage_string(vardir);
  if(cryptfile)
    massage_string(cryptfile);
  if(devicesstr){
    if( (i = devices_from_confstr(devicesstr, &streamers, &num_streamers,
			&changers, &num_changers)) ){
	logmsg(LOG_ERR, "%s: %s\n", T_("Error"),
			T_(devices_from_confstr_strerr(i)));
	exit(27);
    }
  }
  if(num_streamers < 1){
    if( (i = devices_from_confstr(DEFAULT_TAPE_DEVICE,
			&streamers, &num_streamers,
			&changers, &num_changers)) ){
	logmsg(LOG_ERR, "%s: %s\n",
		T_("Internal error evaluating default tape device string"),
			T_(devices_from_confstr_strerr(i)));
	exit(28);
    }
  }
  set_devices(curdevidx);

#ifndef	ORIG_DEFAULTS
  if(!vardir)
    vardir = strdup(DEFSERVVARDIR);
  if(!libdir)
    libdir = strdup(DEFSERVLIBDIR);
  if(!confdir)
    confdir = strdup(DEFSERVCONFDIR);
  if(!bindir)
    bindir = strdup(DEFSERVBINDIR);
#else
  if(!vardir)
    vardir = strapp(backuphome, FN_DIRSEPSTR "var");
  if(!confdir)
    confdir = strapp(backuphome, FN_DIRSEPSTR "etc");
  if(!libdir)
    libdir = strapp(backuphome, FN_DIRSEPSTR "lib");
  if(!bindir)
    bindir = strapp(backuphome, FN_DIRSEPSTR "bin");
#endif

  if(!vardir || !confdir || !libdir || !bindir)
    EM__(NULL);

  if(!g_lock.lockfile){
    for(cpptr = lockdirs; *cpptr; cpptr++){
      i = stat(*cpptr, &statb);
      if(!i){
	if(! eaccess(*cpptr, W_OK)){
	  EM__(g_lock.lockfile = strchain(*cpptr, FN_DIRSEPSTR,
					DEFAULT_SERVERLOCKFILE, NULL));
	  break;
	}
      }
    }
  }

  EEM__(repl_dirs(&tapeposfile) || repl_dirs(&(g_lock.lockfile))
		|| repl_dirs(&nextcartcmd)
		|| repl_dirs(&setfilecmd) || repl_dirs(&skipfilescmd)
		|| repl_dirs(&weofcmd) || repl_dirs(&mail_program));

  if(setcartcmd)
    EEM__(repl_dirs(&setcartcmd));
  if(init_cmd)
    EEM__(repl_dirs(&init_cmd));
  if(exit_cmd)
    EEM__(repl_dirs(&exit_cmd));
  if(erasetapecmd)
    EEM__(repl_dirs(&erasetapecmd));
  if(initmediacmd)
    EEM__(repl_dirs(&initmediacmd));
  if(tapefullcmd)
    EEM__(repl_dirs(&tapefullcmd));
  if(loggingfile)
    EEM__(repl_dirs(&loggingfile));
  if(cryptfile)
    EEM__(repl_dirs(&cryptfile));
  if(programdir)
    EEM__(repl_dirs(&programdir));

  if(statusfile){
    EEM__(repl_dirs(&statusfile));
  }
  else{
    EM__(statusfile = strapp(vardir, FN_DIRSEPSTR STATUSFILE));
  }

  EM__(cartctlcmd = strapp(bindir, FN_DIRSEPSTR "cart_ctl"));
  if(eaccess(cartctlcmd, X_OK)){
    ZFREE(cartctlcmd);
    logmsg(LOG_WARNING, T_("Warning: program `%s' not found.\n"),
					"cart_ctl");
  }

  /* check and set encryption key */
  cptr = cryptfile;
  if(cryptfile){
    i = get_entry_by_host(cryptfile, slavemode ? "" : remotehost, &cptr);
    if(i < 0){
	logmsg(LOG_ERR,
		T_("Error: No authentication key file configured for %s.\n"),
				remotehost);
	do_exit(14);
    }

   if( (i = check_cryptfile(cptr)) ){
	logmsg((i > 0 ? LOG_WARNING : LOG_ERR),
			T_("%s on encryption key file `%s': %s.\n"),
			(i > 0 ? T_("Warning") : T_("Error")),
			cptr, check_cryptfile_msg(i));

	if(i < 0)
	  do_exit(11);

	logmsg(LOG_WARNING, T_("Warning: Ignoring file `%s', using compiled-in key.\n"),
					cptr);
	ZFREE(cptr);
    }
  }

  set_cryptkey(cptr, ACCESSKEYSTRING,
#ifdef	HAVE_DES_ENCRYPTION
		YES
#else
		NO
#endif
		);
  ZFREE(cptr);

  if(cartsetstr && num_cartsets){
    logmsg(LOG_INFO, T_("Warning: Both old parameter LastCartridges and new CartridgeSets supplied. Ignoring LastCartridges entry.\n"));

    num_cartsets = 0;
    ZFREE(endcarts);
  }

  if(cartsetstr)
    num_cartsets = word_countq(cartsetstr);

  insfilenum = NEWP(Int32, num_cartsets);	/* alloc memory */
  inscart = NEWP(Int32, num_cartsets);
  if(!insfilenum || !inscart)
    EM__(NULL);
  memset(inscart, 0, num_cartsets * sizeof(Int32));
  memset(insfilenum, 0, num_cartsets * sizeof(Int32));

  if(cartsetstr){
    EM__(cartridge_sets = NEWP(Uns32Range *, num_cartsets + 1));
    EM__(cartset_clients = NEWP(UChar *, num_cartsets + 1));
    memset(cartset_clients, 0, sizeof(UChar *) * (num_cartsets + 1));

    for(i = 0; i < num_cartsets; i++){
	EM__(cptr = strwordq(cartsetstr, i));

	EM__(cartridge_sets[i] = sscan_Uns32Ranges(cptr, 1, num_cartridges,
				NULL, &cptr2));
	EM__(cartset_clients[i] = strdup(*cptr2 == ':' ?
					cptr2 + 1 : (UChar *) ""));
	  
	free(cptr);
    }
  }
  else if(endcarts){
    free_array((UChar **) cartridge_sets, 0);

    EM__(cartridge_sets = NEWP(Uns32Range *, num_cartsets + 1));

    for(i = 0; i < num_cartsets; i++){	/* and set up some arrays */
      insfilenum[i] = 1;

      EM__(cartridge_sets[i] = add_to_Uns32Ranges(NULL, 1, endcarts[i]));

      for(j = 0; j < num_cartsets; j++){
	if(endcarts[j] < endcarts[i] &&
			cartridge_sets[i][0].first < endcarts[j] + 1)
	  cartridge_sets[i][0].first = endcarts[j] + 1;
      }

      inscart[i] = cartridge_sets[i][0].first;
    }
  }
  cartridge_sets[num_cartsets] = NULL;
  for(i = 0; i < num_cartsets - 1; i++){
    for(j = i + 1; j < num_cartsets; j++){
      if(overlap_Uns32Ranges(cartridge_sets[i], cartridge_sets[j])){
	logmsg(LOG_ERR, T_("Error: Cartridge sets %d and %d have common cartridges.\n"),
					i + 1, j + 1);
	r = CONFIG_ERROR;
      }
    }
  }

  bytesontape_file = strapp(vardir, FN_DIRSEPSTR BYTESONTAPE_FILE);
  pref_client_file = strapp(vardir, FN_DIRSEPSTR PREFCLIENT_FILE);
  precious_tapes_file = strapp(vardir, FN_DIRSEPSTR PRECIOUS_TAPES_FILE);
  ro_tapes_file = strapp(vardir, FN_DIRSEPSTR READONLY_TAPES_FILE);
  cartorder_file = strapp(vardir, FN_DIRSEPSTR CARTRIDGE_ORDER_FILE);
  tape_usage_file = strapp(vardir, FN_DIRSEPSTR TAPE_USAGE_FILE);
  used_blocksizes_file = strapp(vardir, FN_DIRSEPSTR USED_BLOCKSIZES_FILE);
  client_message_file = strapp(vardir, FN_DIRSEPSTR CLIENT_MESSAGE_FILE);

  EEM__(put_in_sh(&setfilecmd));
  EEM__(put_in_sh(&skipfilescmd));
  if(erasetapecmd){
    EEM__(put_in_sh(&erasetapecmd));
  }
  if(initmediacmd){
    EEM__(put_in_sh(&initmediacmd));
  }
  EEM__(put_in_sh(&nextcartcmd));

  EEM__(put_in_sh(&weofcmd));

  if(setcartcmd){
    EEM__(put_in_sh(&setcartcmd));
  }

  if(tapefullcmd){
    cptr = tapefullcmd;
    EM__(tapefullcmd = repl_substring(tapefullcmd, "%C", configfilename));
    free(cptr);
    EEM__(put_in_sh(&tapefullcmd));
  }

  cptr = mail_program;
  mail_program = repl_substring(mail_program, "%u", user_to_inform);
  free(cptr);
  EEM__(put_in_sh(&mail_program));

  if(!setfilecmd || !nextcartcmd || !mail_program || !weofcmd || !statusfile
	|| !bytesontape_file || !precious_tapes_file || !ro_tapes_file
	|| !pref_client_file || !cartorder_file || ! tape_usage_file
	|| !used_blocksizes_file || !client_message_file || !skipfilescmd){
    EM__(NULL);
  }

  if(!strncmp(statusfile, ">>", 2)){
    memmove(statusfile, statusfile + 2, strlen(statusfile) - 1);
    massage_string(statusfile);
    statusf_append = YES;
  }

  if(streamerbuf_confstr){
    EM__(cptr = strdup(streamerbuf_confstr));
    cptr2 = cptr;
    i = 0;
    while( (cptr2 = sscanwordq(cptr2, cptr)) ){
	i1 = -1;
	sscanf(cptr, "%lf%n", &d1, &i1);
	if(i1 == strlen(cptr)){
	  switch(i){
	   case 0:
		streamerbuf_conf_size = (Uns32) d1;
		break;
	   case 1:
		streamerbuf_conf_highmark = d1;
		break;
	   case 2:
		streamerbuf_conf_lowmark = d1;
		break;
	   default:
		logmsg(LOG_WARNING,
			T_("Warning: Unrecognized parameter in streamer buffer configuration."));
	  }
	  i++;
	}
	else{
	  if(streamerbuf_memfile){
	    logmsg(LOG_WARNING,
			T_("Warning: Unrecognized parameter in streamer buffer configuration."));
	  }
	  else{
	    EM__(streamerbuf_memfile = strdup(cptr));
	    EEM__(repl_dirs(&streamerbuf_memfile));
	  }
	}
    }
    free(cptr);
  }

  if(nobuffering)
    streamermode &= ~BUFFERED_OPERATION;

  if(!reconfig){
    tapeblocksize = conf_tapeblocksize;
    EM__(tapebuffer = ZRENEWP(tapebuffer, UChar, tapeblocksize + 2));
  }

  set_commbufsiz(commbufsiz);

  infobuffersize = align_n(INFOBLOCKSIZE, tapeblocksize);
  EM__(infobuffer = NEWP(UChar, infobuffersize));
  memset(infobuffer, 0, sizeof(UChar) * infobuffersize);

  if(lfd >= 0 && lfd != 2)
    close(lfd);

  if(loggingfile){
    if(loggingfile[0] == '@'){
	if(isspace(loggingfile[1]) || !loggingfile[1])
	  syslog_ident = strdup(PACKAGE);
	else
	  syslog_ident = strword(loggingfile + 1, 0);

	EM__(syslog_ident);

	cptr = first_nospace(first_space(loggingfile));
	memmove(loggingfile, cptr, strlen(cptr) + 1);
	if(empty_string(loggingfile))
	  ZFREE(loggingfile);
    }
  }

  cptr = loggingfile;
  if(!cptr)
    cptr = NULLFILE;
  if(eaccess(cptr, W_OK))
    cptr = NULLFILE;
  lfd = open(cptr, O_APPEND | O_CREAT | O_WRONLY, 0644);
  if(lfd < 0)
    lfd = 2;
  else{
    if(!slavemode)
	dup2(lfd, 1);
    dup2(lfd, 2);
  }

  if(maxbytes_per_file < tapeblocksize){
    logmsg(LOG_INFO, T_("Warning: Maximum filesize must be at least one tape block. "
			"Reset to %d.\n"), tapeblocksize);
    maxbytes_per_file = tapeblocksize;
  }

  if(maxbytes_per_tape_str){
    Real64	default_bytes_per_tape = -1.0;
    double	d1;
    int		i1;

    EM__(maxbytes_per_tape = NEWP(Real64, num_cartridges));

    for(i = 0; i < num_cartridges; i++)
	maxbytes_per_tape[i] = -1.0;

    cptr = maxbytes_per_tape_str;
    while(cptr){
	if(!*cptr)
	  break;

	i = sscanf(cptr, "%lf%n", &d1, &i1);
	if(i < 1){
	  logmsg(LOG_NOTICE, T_("Warning: (MaxBytesPerTape): Entry not recognized, ignored.\n"));
	  break;
	}

	cptr2 = first_nospace(cptr + i1);

	if(*cptr2 && !isdigit(*cptr2)){
	  Uns32Range	*ranges;

	  ranges = sscan_Uns32Ranges(cptr, 1, num_cartridges, &i, &cptr2);
	  if(! ranges || i < 1 || *cptr2 != ':'
				|| sscanf(cptr2 + 1, "%lf%n", &d1, &i1) < 1){
		logmsg(LOG_NOTICE, T_("Warning: (MaxBytesPerTape): Entry not recognized, ignored.\n"));
		free(ranges);
		cptr = cptr2;
		continue;
	  }

	  cptr = cptr2 + 1 + i1;
	  for(i--; i >= 0; i--){
		for(j = ranges[i].first; j <= ranges[i].last; j++){
		  if(j < 1){
		    logmsg(LOG_NOTICE, T_("Warning: (MaxBytesPerTape): First cartridge number too low, starting with 1.\n"));
		    j = 0;
		    continue;
		  }
		  if(j > num_cartridges){
		    logmsg(LOG_NOTICE, T_("Warning: (MaxBytesPerTape): First cartridge number too high, stopping at %d.\n"),
					num_cartridges);
		    break;
		  }

		  maxbytes_per_tape[j - 1] = (Real64) d1;
		}
	  }

	  free(ranges);
	}
	else{
	  default_bytes_per_tape = d1;
	  cptr = cptr2;
	}
    }

    i1 = 0;
    for(i = 0; i < num_cartridges; i++){
	if(maxbytes_per_tape[i] < 0.0)
	  maxbytes_per_tape[i] = default_bytes_per_tape;
	else
	  i1 = 1;
    }

    if(!i1 && default_bytes_per_tape < 0.0){
	ZFREE(maxbytes_per_tape);
	ZFREE(maxbytes_per_tape_str);
    }
  }

  /* read the file containing the current position: # of cartridge and file */
  if(read_tapepos_file()){
    logmsg(LOG_NOTICE, T_("Warning: No tape-state file found. Assuming initial startup with first cartridge in set.\n"));

    write_tapepos_file_msg();
  }


  return(r);
}

void
statusmsg(UChar * fmt, ...)
{
  static UChar	logged_err = 0;
  FILE		*fp;
  Int32		i;
  va_list	args;

  if(!statusfile)
    return;

  if(!statusf_append){
    i = unlink(statusfile);
    if(i && errno && errno != ENOENT){
      if(!logged_err){
	logmsg(LOG_ERR, T_("Error: Status file `%s' cannot be removed. Status report disabled.\n"),
				statusfile);
	logged_err = 1;
      }

      return;
    }
  }

  fp = fopen(statusfile, statusf_append ? "a" : "w");
  if(!fp){
    if(!logged_err){
	logmsg(LOG_ERR, T_("Error: Status file `%s' cannot be written. Status report disabled.\n"),
				statusfile);
	logged_err = 1;
    }

    return;
  }

  va_start(args, fmt);
  fprintf(fp, "%s [%d], ", actimestr(), (int) getpid());
  vfprintf(fp, fmt, args);
  va_end(args);

  fclose(fp);
}

static void
send_status(UChar statusc)	/* utility */
{
  write_forced(commwfd, &statusc, 1);
}

void
command_failed(UChar cmd, UChar * str)
{
  logmsg(LOG_ERR, T_("Error: Command %d ('%c') failed (%s)\n"), cmd, cmd, str);
}

void
prot_error(UChar * str, Int32 num)
{
  UChar	buf[100];

  sprintf(buf, str, num);
  logmsg(LOG_ERR, T_("%s, Protocol Error: %s\n"), actimestr(), buf);

  send_status(PROTOCOL_ERROR);
}

void
command_error(UChar * str)
{
  logmsg(LOG_ERR, T_("Command Error: %s.\n"), str);
}

Int32
cartset_of_cart(Int32 cart)
{
  Int32	i;

  for(i = 0; i < num_cartsets; i++)
    if(in_Uns32Ranges(cartridge_sets[i], cart))
	return(i);

  logmsg(LOG_NOTICE, T_("Warning: Cartridge %d is not in any cartridge set. Assuming current set %d\n"),
			(int) cart, (int) active_cartset + 1);

  return(active_cartset);
}

Int32
host_in_list_msg(UChar * host, UChar * list)
{
  Int32	r;

  r = host_in_list(host, list);
  if(r < 0)
    logmsg(LOG_ERR, T_("Error: Unable to determine, whether client has access. Most recent system error: %s\n"),
			strerror(errno));

  return(r);
}

Int32
host_permitted(UChar * remotehost, Int32 cartridge)
{
  Int32	i;

  if(slavemode)		/* can't check in slavemode, cause we don't */
    return(0);		/* have direct connection to the client(s) */

  if(!cartset_clients)	/* default, if not configured: access granted */
    return(0);

  i = cartset_of_cart(cartridge);
  if(host_in_list_msg(remotehost, cartset_clients[i]) <= 0){
    logmsg(LOG_CRIT, T_("Warning: Client %s tried to access cartridge set %d, denied.\n"),
					remotehost, (int) i + 1);
    return((Int32) CARTSET_EACCESS);
  }

  return(0);
}

void
add_to_written_tapes(Int32 cart)
{
  EM__(written_tapes = add_to_Uns32Ranges(written_tapes, cart, cart));
}

Int32
wait_for_flag_file()
{
  int		fd;
  struct stat	statb;

  fd = open(CARTREADY_FILE, O_CREAT | O_WRONLY, 0600);
  if(fd < 0){
    logmsg(LOG_ERR, T_("Error: Cannot create flag file `%s' to wait for cartridge ready"),
			CARTREADY_FILE);
    do_exit(13);
  }
  close(fd);

  while(!stat(CARTREADY_FILE, &statb)){
    ms_sleep(1000);

    if(check_interrupted())
	  do_exit(1);
  }

  return(0);
}

Int32
set_storefile()
{
  UChar		buf[20];
  Int32		file_to_set;

  if(actfilenum <= 0)	/* can only happen, if uninitialized */
    file_to_set = 0;	/* in this case we point to the label */
  else
    file_to_set = actfilenum - 1 + (have_infobuffer ? 1 : 0);
  sprintf(buf, "%d", (int) file_to_set);

  ZFREE(storefile);
  storefile = strchain(devicename, "/data.", buf, NULL);

  if(!storefile)
    return(FATAL_ERROR);

  return(0);
}

Int32				/* take tape offline, unload */
change_cartridge(Flag wait_success, Int32 next_cart, Flag for_writing)
{
  Int32		i;
  UChar		*cmd = NULL, *cptr = NULL;
  UChar		nbuf[20];

  sprintf(nbuf, "%d", (int) actcart);
  if( (cptr = repl_substring(nextcartcmd, "%c", nbuf)) ){
    sprintf(nbuf, "%d", (int) actcart - 1);
    cmd = repl_substring(cptr, "%b", nbuf);
    ZFREE(cptr);

    if(cmd){
      cptr = cmd;
      cmd = repl_dev_pats(cmd, devicename, changername);
      ZFREE(cptr);

      if(cmd){
	sprintf(nbuf, "%d", (int) num_cartridges);
	cptr = cmd;
	cmd = repl_substring(cmd, "%N", nbuf);
	ZFREE(cptr);

	if(cmd && next_cart > 0){
	  sprintf(nbuf, "%d", (int) next_cart - 1);
	  cptr = cmd;
	  cmd = repl_substring(cmd, "%m", nbuf);
	  ZFREE(cptr);

	  if(cmd){
	    sprintf(nbuf, "%d", (int) next_cart);
	    cptr = cmd;
	    cmd = repl_substring(cmd, "%n", nbuf);
	    ZFREE(cptr);
	  }
	}
      }
    }
  }

  if(!cmd){
    logmsg(LOG_WARNING, T_("Warning: Could not replace %-mark in change cartridge command, leaving it unmodified.\n"));
  }

  i = wait_for_service(for_writing ? BU_WRLCK : BU_RDLCK, NO);
  if(i)
    return(i);

  i = poll_device_cmd(cmd ? cmd : nextcartcmd, wait_success, 0);

  have_infobuffer = NO;
  tried_infobuffer = NO;
  tried_tapeblocksize = 0;

  tape_rewound = NO;
  tape_moved = YES;

  media_initialized = NO;

  filenum_incr_by_eof = NO;

  ZFREE(cmd);

  return(i);
}

Int32
wait_for_device(Int8 lock_mode, Flag quiet)
{
  int		fd, i, st;
  time_t	t, t_mail, t_giveup;
  FILE		*fp;

  i = wait_for_service(lock_mode, quiet);
  if(i)
    return(i);

  initmedia(actcart);

  fd = -1;
  st = stat(devicename, &devstatb);
  if(!st && !IS_DIRECTORY(devstatb)){
    fd = open(devicename, O_RDONLY | NONBLOCKING_FLAGS);
    if(fd < 0)
	st = -1;
  }

  if(st){
    t = time(NULL);
    t_mail = t + devunavail_send_mail * 60;
    t_giveup = t + devunavail_give_up * 60;

    do{
      if(check_interrupted())
	do_exit(1);

      if(t_mail > 0 && t > t_mail && devunavail_send_mail > 0){
	fp = tmp_fp(NULL);
	if(fp){
	  fprintf(fp, T_("The device %s on host %s is not ready for use.\n"),
			devicename, unam.nodename);
	  fprintf(fp, T_("You are requested to check the device for possible\n"));
	  fprintf(fp, T_("errors and to correct them.\n\n"));
	  fprintf(fp, T_("Best regards from your backup service.\n"));

	  i = message_to_operator(fp, YES);
	}

	if(i || !fp){
	  logmsg(LOG_ERR, T_("Error: Unable to ask user %s to check device availability.\n"),
			user_to_inform);
	}

	t_mail = 0;
      }

      if(t_giveup > 0 && t > t_giveup && devunavail_give_up > 0){
	logmsg(LOG_ERR, T_("Warning: Device access timed out.\n"));
	return((Int32) DEVNOTREADY);
      }
      ms_sleep(1000 * 10);
      t += 10;

      st = stat(devicename, &devstatb);
      if(!st && !IS_DIRECTORY(devstatb)){
	fd = open(devicename, O_RDONLY | NONBLOCKING_FLAGS);
	if(fd < 0)
	  st = -1;
      }
    } while(st);
  }

  if(fd >= 0)
    close(fd);

  return(0);
}

static int			/* here we force stdin of the subprocess to */
notty0system(char * cmd)	/* be no tty, thus it never gets interactive */
{
  int	pid, pst, i;
  int	inpipe[2];
  char	*shell;

  i = pipe(inpipe);
  if(i)
   return(-1);

  pid = fork_forced();
  if(pid < 0)
    return(-1);
  if(pid == 0){
    setpgid(0, 0);
    close(inpipe[1]);
    dup2(inpipe[0], 0);

    shell = getenv("SHELL");
    if(!shell || ! FN_ISABSPATH(shell)){
	if(!eaccess("/bin/sh", X_OK))
	  shell = "/bin/sh";
	else if(!eaccess("/usr/bin/sh", X_OK))
	  shell = "/usr/bin/sh";
	else
	  shell = find_program("sh");
    }
    if(!shell)
	exit(99);

    execl(shell, shell, "-c", cmd, NULL);
    exit(98);
  }

  pgid_to_kill_on_interrupt = pid;
  close(inpipe[0]);

  pid = waitpid_forced(pid, &pst, 0);
  close(inpipe[1]);
  pgid_to_kill_on_interrupt = -1;

  return(WEXITSTATUS(pst));
}

Int32
poll_device_cmd(UChar * cmd, Flag wait_success, Int32 maxtries)
{
  int		res;
  Int32		i;	/* uninitialized ok */
  Int32		tries;
  time_t	t, t_mail, t_giveup;
  FILE		*fp;

  if(strlen(remoteuser) > 0)
    set_env(REMOTEUSERENVVAR, remoteuser);
  if(strlen(clientuname) > 0)
    set_env(REMOTEHOSTENVVAR, clientuname);

  res = notty0system(cmd);	
  tries = 1;

  if(res && (wait_success || maxtries > 0)){
    t = time(NULL);
    t_mail = t + devunavail_send_mail * 60;
    t_giveup = t + devunavail_give_up * 60;

    do{
      if(check_interrupted())
	do_exit(1);

      if(maxtries > 0){
	if(tries >= maxtries)
	  break;

	tries++;
      }

      if(t_mail > 0 && t > t_mail && devunavail_send_mail > 0){
	fp = tmp_fp(NULL);
	if(fp){
	  fprintf(fp, T_("The device %s on host %s is not ready for use.\n"),
			devicename, unam.nodename);
	  fprintf(fp, T_("You are requested to check the device for possible\n"));
	  fprintf(fp, T_("errors and to correct them.\n\n"));
	  fprintf(fp, T_("Best regards from your backup service.\n"));

	  i = message_to_operator(fp, YES);
	}
	if(i || !fp){
	  logmsg(LOG_ERR, T_("Error: Unable to ask user %s to check device availability.\n"),
			user_to_inform);
	}
	t_mail = 0;
      }

      if(t_giveup > 0 && t > t_giveup && devunavail_give_up > 0){
	logmsg(LOG_ERR, T_("Warning: Device command timed out.\n"));
	return((Int32) res);
      }
      ms_sleep(1000 * 30);
      t += 30;
    } while( (res = notty0system(cmd)) );
  }

  return(res);
}

Int32
set_lock(LockData * the_lock, Int8 lock_mode)
{
  struct stat	statb;
  int		i, fd, lock_state = BU_NO_LOCK;
  char		buf[30], *cptr;
  struct flock	lockb;

  if(the_lock->locked & BU_GOT_LOCK){
    if(lock_mode == BU_WRLCK && the_lock->locked != BU_GOT_LOCK_WR){
	lseek(the_lock->lockfd, 0, SEEK_SET);
	sprintf(buf, "%d %c\n", (int) getpid(), 'w');
	write_forced(the_lock->lockfd, buf, strlen(buf) + 1);

	the_lock->locked = BU_GOT_LOCK_WR;
    }
    return((Int32) the_lock->locked);
  }

  fd = open(the_lock->lockfile, O_RDONLY);
  if(fd >= 0){
    i = read(fd, buf, 29);
    close(fd);
    lock_state = BU_LOCKED_WR;
    if(i > 0){
	buf[i] = '\0';
	for(cptr = buf + strlen(buf) - 1; cptr >= buf; cptr--){
	  if(*cptr == 'r')
	    lock_state = BU_LOCKED_RD;
	  if(isdigit(*cptr))
	    break;
	}
    }
  }

  i = lstat(the_lock->lockfile, &statb);
  if(!i && !IS_REGFILE(statb)){
    if(unlink(the_lock->lockfile)){
	logmsg(LOG_CRIT, T_("Error: Cannot remove lock file entry `%s', that is not a file.\n"), the_lock->lockfile);
	return( (Int32) (the_lock->locked = BU_CANT_LOCK) );
    }

    i = 1;
  }

  if(! i){
    the_lock->lockfd = open(the_lock->lockfile, O_WRONLY | O_SYNC);
    if(the_lock->lockfd < 0){
      logmsg(LOG_ERR, T_("Warning: Lock file `%s' exists, but can't open it.\n"),
			the_lock->lockfile);
      return( (Int32) (the_lock->locked = lock_state) );
    }
  }
  else{
    the_lock->lockfd = open(the_lock->lockfile, O_WRONLY | O_CREAT | O_SYNC, 0644);
    if(the_lock->lockfd < 0){
	logmsg(LOG_ERR, T_("Error: Cannot create lock file `%s'.\n"), the_lock->lockfile);
	return( (Int32) (the_lock->locked = BU_CANT_LOCK) );
    }
  }

  SETZERO(lockb);
  lockb.l_type = F_WRLCK;
  if(fcntl(the_lock->lockfd, F_SETLK, &lockb)){
    close(the_lock->lockfd);
    return( (Int32) (the_lock->locked = lock_state) );
  }

  sprintf(buf, "%d %c\n", (int) getpid(), lock_mode == BU_WRLCK ? 'w' : 'r');
  write_forced(the_lock->lockfd, buf, strlen(buf) + 1);

  the_lock->locked = (lock_mode == BU_WRLCK ? BU_GOT_LOCK_WR : BU_GOT_LOCK_RD);

  return((Int32) the_lock->locked);
}

void
register_pref_serv()
{
#ifdef	SAME_CLIENT_PREFERENCE_NEEDED
  struct flock	plockb;
  time_t	locked_until, curtime;
  UChar		locking_remhost[300], lbuf[300], *buf = NULL;
  UChar		*cptr, *clientid_in_file = NULL;
  Int32		lock_failed, i, buf_nall = 0;
  Flag		buf_all = NO, update_timeout_in_file;
  int		fd;
  struct stat	statb;

  fd = 0;
  i = lstat(pref_client_file, &statb);
  if(!i && !IS_REGFILE(statb)){
    if(unlink(pref_client_file)){
      if(errno != ENOENT){
	logmsg(LOG_CRIT, T_("Warning: Cannot remove filesystem entry to check preferred service for client, that is not a regular file !!!\n"));

	fd = -1;
      }
    }
  }
		/* check, if the same client did already connect recently */
  if(!fd)
    fd = open(pref_client_file, O_RDWR | O_CREAT, 0644);

  if(fd < 0){
    logmsg(LOG_WARNING, T_("Warning: Cannot open file to check preferred service for client.\n"));
  }
  else{
    update_timeout_in_file = YES;

    do{
      SETZERO(plockb);
      plockb.l_type = F_WRLCK;

      lock_failed = fcntl(fd, F_SETLK, &plockb);
			/* if we cannot get the lock, we will surely be able */
      if(lock_failed)		/* to read the file after a short period */
	ms_sleep(100 + (Int32) (drandom() * 500.0));

      lseek(fd, 0, SEEK_SET);

      i = read(fd, lbuf, 299);

      if(i > 0){
	lbuf[i] = '\0';
	massage_string(lbuf);

	if(word_count(lbuf) >= 2){
	  cptr = sscanwordq(lbuf, locking_remhost);

	  locked_until = strint2time(cptr);

	  cptr = sscanword(cptr, NULL);		/* cptr now used as flag */
	  if(*cptr){				/* client id on file -> */
	    EM__(clientid_in_file = strdup(cptr));
	    if(strcmp(client_id, "")){		/* if client did identify */
		if(!strcmp(cptr, client_id))	/* and is identical */
		  cptr = NULL;			/* no delay */
	    }
	    else{				/* in favour of the client */
		cptr = NULL;			/* we assume, that it's the */
		update_timeout_in_file = NO;
	    }					/* same one and delay later */
	  }					/* when receiving the ID */
	  else{		/* no client id on file: no delay by client id */
	    cptr = NULL;
	  }

	  if(same_host(locking_remhost, remotehost) <= 0 || cptr){
	    curtime = time(NULL);
	    if(locked_until >= curtime){
		ms_sleep(1000 * (locked_until + 1 - curtime));
	    }
	  }
	}
      }
    } while(lock_failed);

    lseek(fd, 0, SEEK_SET);

    EM__(cptr = time_t_to_intstr(	/* keep old value, if no update */
	update_timeout_in_file ? time(NULL) + PREF_CLIENT_DELAY : locked_until,
		NULL));

    EM__(buf = get_mem(buf, &buf_nall, strlen(remotehost) + strlen(cptr) + 8,
				lbuf, sizeof(lbuf), &buf_all));

    sprintwordq(buf, remotehost);
    strcat(buf, " ");
    strcat(buf, cptr);
    free(cptr);

    if(strcmp(client_id, "")){
	EM__(buf = get_mem(buf, &buf_nall, strlen(buf) + strlen(client_id) + 8,
				lbuf, sizeof(lbuf), &buf_all));
	strcat(buf, " ");
	strcat(buf, client_id);
    }
    else if(clientid_in_file){		/* keep the old one in place */
	EM__(buf = get_mem(buf, &buf_nall,
			strlen(buf) + strlen(clientid_in_file) + 8,
			lbuf, sizeof(lbuf), &buf_all));
	strcat(buf, " ");
	strcat(buf, clientid_in_file);
    }
    strcat(buf, "\n");
    ZFREE(clientid_in_file);

    i = strlen(buf);
    write(fd, buf, i);
    ftruncate(fd, i);

    close(fd);
  }

  if(buf_all)
    free(buf);

#else
  unlink(pref_client_file);

#endif	/* defined(SAME_CLIENT_PREFERENCE_NEEDED) */
}

Int32
release_lock(LockData * the_lock)
{
  if(the_lock->locked & BU_GOT_LOCK){
#if	0	/* unnecessary: close removes any lock */
    struct flock	lockb;

    SETZERO(lockb);
    lockb.l_type = F_UNLCK;
    fcntl(the_lock->lockfd, F_SETLK, &lockb);
#endif

    close(the_lock->lockfd);

    unlink(the_lock->lockfile);

    the_lock->lockfd = -1;
    the_lock->locked = BU_NO_LOCK;
  }

  return(the_lock->locked);
}

Int32
wait_for_service(Int8 lock_mode, Flag quiet)
{
  time_t	t, t_mail, t_giveup;
  FILE		*fp;
  Int32		lck;
  Int32		i;	/* uninitialized ok */
  int		sleeptime;

  if(!quiet)
    statusmsg(T_("Waiting for device %s to become available.\n"),
				(char *) devicename);

  lck = set_lock(&g_lock, lock_mode);
  if(lck == BU_CANT_LOCK)
    return((Int32) FATAL_ERROR);

  if(lck & BU_LOCKED){
    t = time(NULL);
    t_mail = t + devunavail_send_mail * 60;
    t_giveup = t + devunavail_give_up * 60;
    sleeptime = 1;

    do{
      if(check_interrupted())
	do_exit(1);

      if(t_mail > 0 && t > t_mail && devunavail_send_mail > 0){
	fp = tmp_fp(NULL);
	if(fp){
	  fprintf(fp, T_("The backup service on host %s is in use.\n"), unam.nodename);
	  fprintf(fp, T_("You are requested to check the service for possible\n"));
	  fprintf(fp, T_("errors and to correct them.\n\n"));
	  fprintf(fp, T_("Best regards from your backup service.\n"));
	  i = message_to_operator(fp, NO);
	}
	if(i || !fp){
	  logmsg(LOG_ERR, T_("Error: Unable to ask user %s to check service availability.\n"),
			user_to_inform);
	}
	t_mail = 0;
      }

      if(t_giveup > 0 && t > t_giveup && devunavail_give_up > 0){
	logmsg(LOG_WARNING, T_("Warning: Service access timed out.\n"));
	return((Int32) SERVICEINUSE);
      }
      ms_sleep(1000 * sleeptime);
      t += sleeptime;

      sleeptime = (sleeptime < 30 ? sleeptime + 1 : 30);

      lck = set_lock(&g_lock, lock_mode);
      if(lck == BU_CANT_LOCK)
	return((Int32) FATAL_ERROR);

    } while(! (lck & BU_GOT_LOCK));

    ER__(read_tapepos_file(), i);	/* if we had to wait, this */
  }				/* information is probably outdated */

  if(!quiet)
    statusmsg(T_("Device %s available, idle.\n"), (char *) devicename);

  return(0);
}

Int32		/* reopen the stream for writing, this is not a cmd */
reopentapewr()
{
  Int32		i, ret = NO_ERROR;
  int		filemode;
  UChar		*filename;

  statusmsg(T_("Device %s reopening for write.\n"), (char *) devicename);

  if(TAPE_NOT_CLOSED && (tapeaccessmode & MODE_MASK) != OPENED_FOR_WRITE){
    logmsg(LOG_CRIT, T_("Internal Error: tape not open for writing in reopen.\n"));
    ret = FATAL_ERROR;
    GETOUT;
  }

  if(TAPE_NOT_CLOSED){
    if(tapefd >= 0){
      i = close(tapefd);
      tapefd = -1;

      if(i){
	ret = i;
	GETOUT;
      }
    }
  }

  if(interrupted)
    do_exit(1);

  bytes_in_tapefile = 0;

  endofrec = NO;

  if( (i = save_bytes_per_tape_msg(bytesontape_file, actcart,
				bytes_on_tape, actfilenum, NO, time(NULL))) ){
    ret = i;
    GETOUT;
  }

  if(TAPE_NOT_CLOSED){
    actfilenum++;
    insfilenum[active_cartset]++;
    rdfilenum = actfilenum;
  }

  if( (i = write_tapepos_file_msg()) ){
    ret = i;
    GETOUT;
  }

  if(stat(devicename, &devstatb)){
    ret = -errno;
    GETOUT;
  }

  if(IS_REGFILE(devstatb) || IS_DIRECTORY(devstatb)){
    if( (i = update_devicesettings(insfilenum[active_cartset], YES)) ){
      logmsg(LOG_ERR, T_("Error: Cannot change store file.\n"));
      ret = i;
      GETOUT;
    }
  }

  tape_moved = YES;
  tape_rewound = NO;

  filename = devicename;
  filemode = O_WRONLY | O_BINARY;

  if(IS_DIRECTORY(devstatb)){
    filename = storefile;
    filemode = O_WRONLY | O_CREAT | O_TRUNC | O_BINARY;
  }

  tapefd = open(filename, filemode, 0600);
  if(tapefd < 0){
    logmsg(LOG_ERR, T_("Error: Cannot open tape once again for writing.\n"));
    ret = -errno;
    GETOUT;
  }

  tapeaccessmode = OPENED_FOR_WRITE;

  if( (i = save_bytes_per_tape_msg(bytesontape_file, actcart,
				bytes_on_tape, actfilenum, NO, time(NULL))) ){
    ret = i;
    GETOUT;
  }

  filenum_incr_by_eof = NO;

  statusmsg(T_("Device %s opened for writing to cartridge %d, file %d.\n"),
		(char *) devicename, (int) actcart, (int) actfilenum);

  return(0);

 getout:
  if(tapefd >= 0)
    close(tapefd);

  tapefd = -1;
  tapeaccessmode = NOT_OPEN;

  return(ret);
}

Int32
write_tape_buffer(Int32 * bufferpos, Int32 * bytes_flushed)
{
  Int32		nextidx, r;

  if(streamerbuffer){
#ifdef	HAVE_POSIX_THREADS	/* prevent other thread from reconfiguring */
    pthread_mutex_lock(&may_reconfig_buffer_mutex);	/* the buffer */

    pthread_mutex_lock(&may_modify_buffer_ptrs);
#endif

    nextidx = streamerbuf_insert_idx + 1;
    if(nextidx >= streamerbufferlen)
	nextidx = 0;

#ifdef	HAVE_POSIX_THREADS
    if(nextidx == streamerbuf_processed_idx){	/* let other thread process */
	streamerbuf_writing = YES; 	/* not really necessary, if highmark */
					/* is set reasonably */
	pthread_mutex_unlock(&may_reconfig_buffer_mutex);

	if(buffer_thread_running)
	  pthread_cond_wait(&can_write_to_buffer,	/* wait for space */
				&may_modify_buffer_ptrs);

	pthread_mutex_lock(&may_reconfig_buffer_mutex);

	nextidx = streamerbuf_insert_idx + 1;	/* buffer might have been */
	if(nextidx >= streamerbufferlen)	/* reconfigured */
	  nextidx = 0;
    }

#else
    while(nextidx == streamerbuf_processed_idx){
	process_tape_ops(NO, YES);

	if(tapefd < 0 && (tapeaccessmode & OPENED_RAW_ACCESS))
	  return(WRITE_FAILED);
    }
#endif

    memcpy(streamerbuffer + streamerbuf_insert_idx * tapeblocksize,
			tapebuffer, sizeof(UChar) * tapeblocksize);
    streamerbuf_insert_idx = nextidx;
    *bytes_flushed = tapeblocksize;

    *bufferpos = 0;

#ifdef	HAVE_POSIX_THREADS
    if(num_blocks_in_streamerbuf >= streamerbuf_highmark){
      streamerbuf_writing = YES;

      pthread_cond_signal(&buffer_contains_data);
    }

    pthread_mutex_unlock(&may_modify_buffer_ptrs);

    pthread_mutex_unlock(&may_reconfig_buffer_mutex);

    if(tapefd < 0 && (tapeaccessmode & OPENED_RAW_ACCESS))
	return(-errno);
#endif

    return(0);
  }
  else{
    if(iauxtapebuffer){
	Int32	i, iauxbufpos, iauxbytes_in_tapefile;

	for(i = r = 0; i < iauxtapebufferlen; i += tapeblocksize){
	  r = flush_buffer_to_tape(iauxtapebuffer + i,
					&iauxbufpos, &iauxbytes_in_tapefile);
	  if(r || iauxbufpos != 0 || iauxbytes_in_tapefile != tapeblocksize){
	    r = (r ? r : FATAL_ERROR);
	    break;
	  }
	}

	ZFREE(iauxtapebuffer);
	iauxtapebufferlen = 0;
	if(r)
	  return(r);
    }

    return(flush_buffer_to_tape(tapebuffer, bufferpos, bytes_flushed));
  }
}

/* write tapebuffer to tape, evtl. start a new file or change cartridge */
Int32
flush_buffer_to_tape(
  UChar		*tapebuf,
  Int32		*bufferpos,		/* only to return the new value */
  Int32		*flushed_bytes)
{
  static Flag	write_error = NO;

  Int32		i, newcart, newfile, written, w, ret = NO_ERROR;
  Int32		cart_bef_change, files_on_tape;
  UChar		*filename, rbuf[64];
  Flag		eom, do_reopen = NO, dont_write = NO;
  int		filemode;

  filenum_incr_by_eof = NO;
  files_on_tape = actfilenum;

  if(!(tapeaccessmode & OPENED_RAW_ACCESS)){
    if(maxbytes_per_tape && bytes_on_tape + (Real64) tapeblocksize
					> maxbytes_per_tape[actcart - 1]){
	eom = YES;

	statusmsg(T_("Current cartridge no. %d is already full\n"), (int) actcart);

	goto change_tape;	/* sorry, otherwise huge complication */
    }

    if(maxbytes_per_file && bytes_in_tapefile + tapeblocksize > maxbytes_per_file){
	do_reopen = YES;
    }

    if(streamerbuffer){
      if(reqnewfilebuf[streamerbuf_processed_idx]){
	if(bytes_in_tapefile > 0)	/* only reopen, if nothing has been */
	  do_reopen = YES;	/* written to the current tape file yet */

	reqnewfilebuf[streamerbuf_processed_idx] = NO;
      }
    }
    else{
      if(newfilerequested){
	if(bytes_in_tapefile > 0)	/* same as above */
	  do_reopen = YES;

	newfilerequested = NO;
      }
    }

    if(do_reopen){
      if(reopentapewr()){
	logmsg(LOG_ERR, T_("Error: Cannot reopen device for writing.\n"));
	ret = REOPEN_FAILED;
	GETOUT;
      }

      files_on_tape = actfilenum;
    }
  }

  errno = 0;	/* set errno unconditionally here, cause return values */
		/* on FreeBSD below are different compared to other OSes */
		/* so errno must be evaluated to and thus be reset here */

  written = write_forced(tapefd, tapebuf, tapeblocksize);
  w = (written > 0 ? written : 0);

  bytes_in_tapefile += w;
  bytes_on_tape += (Real64) w;

	/* status report */
  write_rate += w;

  if(!write_meas_time){
    write_meas_time = time(NULL);
    write_rate = 0;
  }
  else{
    actime = time(NULL);
    if(actime >= write_meas_time + 10){
	si_symvalstr(rbuf, (double) (write_rate / (actime - write_meas_time)),
			3, 0.5, NO, YES);
	statusmsg(T_("Device %s is writing with %sB/s to cartridge %d, file %d.\n"),
		(char *) devicename, rbuf, (int) actcart, (int) actfilenum);
	write_meas_time = actime;
	write_rate = 0;
    }
  }

  *flushed_bytes = w;

  if(written == tapeblocksize){		/* the normal case */
    write_error = NO;
    *bufferpos = 0;

    tape_rewound = NO;
    tape_moved = YES;
    return(0);
  }

  if(written > 0 && written < tapeblocksize){	/* block not fully written */
    memmove(tapebuf, tapebuf + written,
				(tapeblocksize - written) * sizeof(UChar));
    *bufferpos = written;			/* we try not to be afraid */

    write_error = NO;
    return(0);
  }

  if(tapeaccessmode & OPENED_RAW_ACCESS){	/* In raw access mode */
    if(tapefd >= 0)				/* it is always an error, */
	close(tapefd);			/* if nothing could be written */

    tapefd = -1;
    *bufferpos = 0;

    return(ENDOFTAPEREACHED);
  }

#ifdef	__FreeBSD_version
#if	__FreeBSD_version >= 470000
#define	FREE_BSD_470000_OR_HIGHER	1
#endif
#else
#ifdef	__FreeBSD_kernel_version
#if	__FreeBSD_kernel_version >= 470000
#define	FREE_BSD_470000_OR_HIGHER	1
#endif
#endif
#endif
#ifdef	FREE_BSD_470000_OR_HIGHER
	/* hooray, the FreeBSD 'SA' driver behaviour has changed with */
	/* FreeBSD 4.7 towards one closer to System-V style. Sigh. */
	/* Modification suggested and tested by Rudolf Cejka at */
	/* Brno University of Technology */
  eom = (written == 0) ? YES : NO;

  if(written < 0){				/* tape operation failed */

#else

  eom = NO;

  if(written <= 0){				/* tape operation failed */

#endif

    i = errno;		/* this assignment is for debugging purposes */

#ifdef	sun

    if(!END_OF_TAPE(errno)){	/* on full filesystem END_OF_TAPE is true */
	struct mtget	mtstat;		/* so here we get only with tapes */

	i = ioctl(tapefd, MTIOCGET, &mtstat);
	if(i){
	  logmsg(LOG_ERR, T_("Error: Cannot get tape status after write failure.\n"));
	  errno = ENOSPC;
	}
	else{
	  if(mtstat.mt_erreg)	/* mt_erreg is UNDOCUMENTED, even on Sun- */
	    errno = ENOSPC;	/* solve. So we assume end of tape on any */
	}						/* error != 0 */
    }		/* suspicions are: 0x13==EOT, 0x12==EOF, 0x05==illegal-req */
			/* 0x08==blank-check(EOR) */
#endif

#if	defined(__hpux) || defined(hpux)
#ifdef	MTIOCGET

    if(!END_OF_TAPE(errno)){	/* on full filesystem END_OF_TAPE is true */
	struct mtget	mtgetst;	/* so here we get only with tapes */

	i = ioctl(tapefd, MTIOCGET, &mtgetst);
	if(i){
	  logmsg(LOG_ERR, T_("Error: Cannot get tape status after write failure.\n"));
	  errno = ENOSPC;
	}
	else{
	  if(GMT_EOT(mtgetst.mt_gstat))	/* see man 7 mt on HPUX */
	    errno = ENOSPC;
	}						/* error != 0 */
    }

#endif
#endif

    if(END_OF_TAPE(errno)){
	eom = YES;
	errno = 0;
    }
    else if(errno ==
#ifdef	_AIX
			EMEDIA || errno == 
#endif
				EIO){
      if(write_error){
	logmsg(LOG_ERR, T_("Error: Repeated faults writing to tape, now: %s.\n"),
				strerror(errno));
	ret = FATAL_ERROR;
	GETOUT;
      }

      logmsg(LOG_WARNING, T_("Warning: A media error occured (%d), trying to reposition and reopen.\n"),
			errno);

      files_on_tape = actfilenum;

#if 0	/* this may not be done here, cause by definition tapeaccessmode */
      tapeaccessmode = NOT_OPEN;	/* reflects the status seen by the */
#endif			/* client regardless of server internal problems. */
	/* tapeaccessmode = NOT_OPEN will be set if recovery is impossible */

      if(tapefd >= 0)		/* streamers usually rewind on an EIO, e.g. */
	close(tapefd);		/* due to a SCSI bus reset. It would not be */
      				/* a cool idea to simply */
      tapefd = -1;				/* reopen and write on, */
      i = (bytes_in_tapefile > 0 ? 1 : 0);	/* thus overwriting stuff */
      if(set_filenum(actfilenum + i, NO, NO))	/* written before. So we */
	eom = YES;		/* try to re-position to the next file. If */
      else			/* that fails, we load the next cartridge. */
	files_on_tape = actfilenum;	/* If the current tapefile is still */
					/* empty, reposition to the same */
      write_error = YES;
    }
    else{
	logmsg(LOG_ERR, T_("Error: Writing to tape: %s.\n"), strerror(errno));
	ret = FATAL_ERROR;
	GETOUT;
    }
  }

  if(!eom){
    i = reopentapewr();		/* try again into the next tape file */
    if(!i){
      files_on_tape = actfilenum;

      errno = 0;		/* see above for comment */

      written = write_forced(tapefd, tapebuf, tapeblocksize);

      w = (written > 0 ? written : 0);

      bytes_in_tapefile += w;
      bytes_on_tape += (Real64) w;

      write_rate += w;		/* for status report */

      *flushed_bytes = w;

      if(written == tapeblocksize){		/* that worked */
	write_error = NO;
	*bufferpos = 0;

	tape_moved = YES;
	tape_rewound = NO;

	return(0);
      }

      if(written > 0 && written < tapeblocksize){
	memmove(tapebuf, tapebuf + written,
				(tapeblocksize - written) * sizeof(UChar));
	*bufferpos = written;		/* keep fingers crossed */

	write_error = NO;
	return(0);
      }

      eom = YES;	/* some other problems with the tape, assume EOM */

      logmsg(LOG_WARNING, T_("Warning: An unexpected error (%d) occured, changing cartridge. Proceeding with fingers crossed.\n"),
				errno);
      write_error = YES;
    }
    else{			/* reopen failed */
	logmsg(LOG_WARNING, T_("Warning: Reopen failed (%d), changing cartridge. Proceeding with fingers crossed.\n"),
				errno);
	eom = YES;
    }
  }

 change_tape:
  if(eom){			/* TAPE FULL */
	if(tapefd >= 0)
	  close(tapefd);	/* we change the cartridge and try again */
	tapefd = -1;

	tape_full_actions(actcart);

	if( (i = save_bytes_per_tape_msg(bytesontape_file, actcart,
			bytes_on_tape, files_on_tape, YES, time(NULL))) ){
	  ret = REOPEN_FAILED;
	  GETOUT;
	}

	if(interrupted)
	  do_exit(1);

	bytes_in_tapefile = 0;

	endofrec = NO;

	if(!AUTOCHCART)
	  return(ENDOFTAPEREACHED);

	cart_bef_change = actcart;

	i = find_next_free_pos(&newcart, &newfile,
				inscart[active_cartset], active_cartset);
	if(i){
	  logmsg(LOG_ERR, T_("Error: Cannot find a new cartridge for writing.\n"));
	  ret = REOPEN_FAILED;
	  GETOUT;
	}

	i = set_cartridge(- newcart, YES);	/* a clone tape is accepted here */
	if(i){					/* only after repeated tries */
	  logmsg(LOG_ERR, T_("Error: Cannot change to cartridge %d for writing.\n"),
			newcart);
	  ret = REOPEN_FAILED;
	  GETOUT;
	}		/* now actcart is newcart is tape in drive */ 

	if(actcart != newcart && var_append)	/* other cartridge inserted */
	  newfile = insfilenum[active_cartset];

	i = save_cart_order(cart_bef_change, actcart, newfile);
	if(i){		/* this is no longer done in find_next_free_pos */
	  ret = REOPEN_FAILED;
	  GETOUT;
	}

	if(newfile == 1)
	  bytes_on_tape = 0.0;
	else
	  i = get_bytes_per_tape_msg(bytesontape_file, actcart,
					&bytes_on_tape, NULL, NULL, NULL);

	if(newfile == 1){
	  i = rewindtape(YES, NO);
	  actfilenum = 1;
	}
	else
	  i = set_filenum(newfile, YES, NO);
	if(i){
	  logmsg(LOG_ERR, T_("Error: Cannot move to file %d for writing.\n"), (int) newfile);

	  ret = i;
	  GETOUT;
	}

#ifdef	HAVE_POSIX_THREADS
	if(streamerbuffer)
	  pthread_mutex_lock(&tapepos_mutex);
#endif

	inscart[active_cartset] = rdcart = actcart;
	insfilenum[active_cartset] = rdfilenum = actfilenum;

#ifdef	HAVE_POSIX_THREADS
	if(streamerbuffer)
	  pthread_mutex_unlock(&tapepos_mutex);
#endif

	if(actcart != newcart && !var_append)
	  logmsg(LOG_CRIT, T_("Internal Error: Wrong tape %d accepted in drive for %d, please report.\n"),
			actcart, newcart);
	if(actfilenum != newfile)
	  logmsg(LOG_CRIT, T_("Internal Error: Wrong file number %d set instead of %d, please report.\n"),
			actfilenum, newfile);

	if(interrupted)
	  do_exit(1);

	add_to_written_tapes(actcart /* == newcart */);

	if(newfile == 1){
	  i = erasetape();
	  if(i)
	    logmsg(LOG_WARNING, T_("Warning: Cannot erase tape.\n"));

	  if(interrupted)
	    do_exit(1);

	  if(newfile == 1){
	    i = rewindtape(YES, NO);
	    actfilenum = 1;
	  }
	  else
	    i = set_filenum(newfile, YES, NO);
	  if(i){
	    logmsg(LOG_ERR, T_("Error: Cannot move to file %d for writing.\n"), (int) newfile);

	    ret = i;
	    GETOUT;
	  }
	}

	if(interrupted)
	  do_exit(1);

	if(inscart[active_cartset] != actcart /* == newcart */)
	  logmsg(LOG_CRIT, T_("Internal Error: Wrong tape for writing in drive, please report.\n"));

	if(actfilenum == 1){
	  i = write_infobuffer(inscart[active_cartset] /* == newcart */);
	  if(i){
	    logmsg(LOG_ERR, T_("Error: Cannot write label to tape.\n"));

	    ret = i;
	    GETOUT;
	  }

	  insfilenum[active_cartset] = actfilenum = 1;
	}

	if(write_tapepos_file_msg()){
	  ret = REOPEN_FAILED;
	  GETOUT;
	}

	if( (i = save_bytes_per_tape_msg(bytesontape_file, actcart,
				bytes_on_tape, actfilenum, NO, time(NULL))) ){
	  ret = REOPEN_FAILED;
	  GETOUT;
	}

	if(IS_REGFILE(devstatb) || IS_DIRECTORY(devstatb)){
	  if( (i = update_devicesettings(insfilenum[active_cartset], YES)) ){
	    logmsg(LOG_ERR, T_("Error: Cannot change to file %d for writing.\n"),
				insfilenum[active_cartset]);

	    ret = i;
	    GETOUT;
	  }
	}

	filename = devicename;
	filemode = O_WRONLY | O_BINARY;

	if(IS_DIRECTORY(devstatb)){
	  filename = storefile;
	  filemode = O_WRONLY | O_CREAT | O_TRUNC | O_BINARY;
	}

	tapefd = open(filename, filemode, 0600);
	if(tapefd < 0){
	  ret = REOPEN_FAILED;
	  GETOUT;
	}

	tapeaccessmode = OPENED_FOR_WRITE;

	if(have_infobuffer && lbl_tapeblocksize > 0){
	    i = change_tapeblocksize(lbl_tapeblocksize, &tapebuf,
						flushed_bytes, &dont_write);
	    if(i){
		ret = i;
		GETOUT;
	    }
	}
  }		/* eom */

  if(dont_write)
    return(0);

  written = write_forced(tapefd, tapebuf, tapeblocksize);

  w = (written > 0 ? written : 0);

  bytes_in_tapefile += w;
  bytes_on_tape += (Real64) w;

  write_rate += w;		/* for status report */

  *flushed_bytes = w;

  if(written == tapeblocksize){		/* now it worked. Poooh ! */
    *bufferpos = 0;
    write_error = NO;

    tape_moved = YES;
    tape_rewound = NO;

    return(0);
  }

  if(written > 0 && written < tapeblocksize){	/* block not fully written */
    memmove(tapebuf, tapebuf + written,
				(tapeblocksize - written) * sizeof(UChar));
    *bufferpos = written;			/* we try not to be afraid */

    tape_moved = YES;
    tape_rewound = NO;
    write_error = NO;

    return(0);
  }

  ret = FATAL_ERROR;

 getout:
  if(tapefd >= 0)
    close(tapefd);

  tapeaccessmode = NOT_OPEN;
  tapefd = -1;

  write_error = YES;
  return(ret);		/* finally we failed writing to tape */
}

/* get a bufferfull from tape of from the streamerbuffer */
Int32
read_tape_buffer(Int32 * bufferpos, Int32 * num_bytes)
{
  if(streamerbuffer){
#ifdef	HAVE_POSIX_THREADS
    pthread_mutex_lock(&may_reconfig_buffer_mutex);

    pthread_mutex_lock(&may_modify_buffer_ptrs);
#endif

    while(streamerbuf_insert_idx == streamerbuf_processed_idx){
	if(at_mediaend){	/* we are at end of media */
	  if((tapeaccessmode & OPENED_RAW_ACCESS) || TAPE_CLOSED){
#ifdef	HAVE_POSIX_THREADS
	    pthread_mutex_unlock(&may_modify_buffer_ptrs);

	    pthread_mutex_unlock(&may_reconfig_buffer_mutex);
#endif
	    return(ENDOFFILEREACHED);
	  }

	  force_cartchange = YES;	/* need to read on, set flag */
	}				/* to force cartridge change */

#ifdef	HAVE_POSIX_THREADS
	streamerbuf_reading = YES;

	pthread_cond_signal(&can_write_to_buffer);

	pthread_mutex_unlock(&may_reconfig_buffer_mutex);

	pthread_cond_wait(&buffer_contains_data, &may_modify_buffer_ptrs);

	pthread_mutex_lock(&may_reconfig_buffer_mutex);
#else
	process_tape_ops(YES, NO);
#endif
    }

    memcpy(tapebuffer,
	streamerbuffer + tapeblocksize * streamerbuf_processed_idx,
	numbytesread[streamerbuf_processed_idx] * sizeof(UChar));

    *num_bytes = numbytesread[streamerbuf_processed_idx];

    streamerbuf_processed_idx++;
    if(streamerbuf_processed_idx >= streamerbufferlen)
	streamerbuf_processed_idx = 0;

    *bufferpos = 0;

#ifdef	HAVE_POSIX_THREADS
    pthread_mutex_unlock(&may_reconfig_buffer_mutex);

    pthread_cond_signal(&can_write_to_buffer);

    pthread_mutex_unlock(&may_modify_buffer_ptrs);
#endif

    return(0);
  }
  else{
    if(iauxtapebuffer){
	memcpy(tapebuffer, iauxtapebuffer, tapeblocksize * sizeof(UChar));
	iauxtapebufferlen -= tapeblocksize;
	if(iauxtapebufferlen <= 0){
	  ZFREE(iauxtapebuffer);
	  iauxtapebufferlen = 0;
	}
	else{
	  memmove(iauxtapebuffer, iauxtapebuffer + tapeblocksize,
				iauxtapebufferlen * sizeof(UChar));
	}

	return(0);
    }

    return(get_tape_buffer(tapebuffer, bufferpos, num_bytes));
  }
}

/* really read a buffer from tape, evtl. change the cartridge */
Int32
get_tape_buffer(
  UChar		*tapebuf,
  Int32		*bufferpos,
  Int32		*num_bytes)
{
  Int32		newcart, newfile, i, rawaccess, ret = NO_ERROR;
  int		filemode, e;
  UChar		*filename, rbuf[64];
  Flag		dont_read = NO;

  errno = 0;

  filenum_incr_by_eof = NO;

  i = read_forced(tapefd, tapebuf, tapeblocksize);
  e = errno;

  DB("get_tape_buffer: read tape input, # bytes: %d\n", i, 0, 0);

  *num_bytes = i;

  if(i > 0){
    bytes_read += i;

	/* status report */
    read_rate += i;
  }

  if(!read_meas_time){
    read_meas_time = time(NULL);
    read_rate = 0;
  }
  else{
    actime = time(NULL);
    if(actime >= read_meas_time + 10){
	si_symvalstr(rbuf, (double) (read_rate / (actime - read_meas_time)),
			3, 0.5, NO, YES);
	statusmsg(T_("Device %s is reading with %sB/s from cartridge %d, file %d.\n"),
		(char *) devicename, rbuf, (int) actcart, (int) actfilenum);
	read_meas_time = actime;
	read_rate = 0;
    }
  }

  if(i < tapeblocksize){
    DB("get_tape_buffer: too few bytes, errno = %d\n", e, 0, 0);

    if(e){
	logmsg(LOG_WARNING, T_("Error: %s, only %d bytes read, trying to continue, tapefd = %d.\n"),
					strerror(e), (int) i, tapefd);
	errno = 0;
    }

    if(i <= 0){		/* End Of File */
	rawaccess = tapeaccessmode & OPENED_RAW_ACCESS;

	*bufferpos = 0;
	endofrec = NO;

	if(bytes_read){
	  actfilenum++;

	  if(write_tapepos_file_msg()){
	    ret = REOPEN_FAILED;
	    GETOUT;
	  }
	}

	if(rawaccess){
	  DB("get_tape_buffer: setting at media end\n", 0, 0, 0);

	  at_mediaend = YES;

	  filenum_incr_by_eof = YES;

	  return(bytes_read ? ENDOFFILEREACHED : ENDOFTAPEREACHED);
	}

	if(tapefd >= 0)
	  close(tapefd);

	tapefd = -1;

#if 0		/* suggested by Piotr, breaks filesystem pseudotapes */
	tapeaccessmode = NOT_OPEN;
#endif

	bytes_read = 0;

	if(IS_REGFILE(devstatb) || IS_DIRECTORY(devstatb)){
	  if( (i = update_devicesettings(actfilenum, YES)) ){
	    logmsg(LOG_ERR, T_("Error: Cannot change store file.\n"));

	    ret = i;
	    GETOUT;
	  }
	}

	filename = devicename;
	filemode = O_RDONLY | O_BINARY;

	if(IS_DIRECTORY(devstatb)){
	  filename = storefile;
	}

	filenum_incr_by_eof = YES;

	errno = 0;
	tapefd = i = open(filename, filemode);
	if(tapefd < 0 && !IS_REGFILE(devstatb) && !IS_DIRECTORY(devstatb)){
	  ret = REOPEN_FAILED;
	  GETOUT;
	}

	if(tapefd >= 0){
	  tapeaccessmode = OPENED_FOR_READ;

	  i = read_forced(tapefd, tapebuf, tapeblocksize);

	  if(i > 0){
	    bytes_read += i;
	    rdfilenum = actfilenum;
	  }
	}

	if(i <= 0){			/* at end of medium ? */
	  if(errno ==
#ifdef	_AIX
			EMEDIA || errno ==
#endif
				EIO){
	    logmsg(LOG_WARNING, T_("Warning: A media error occurred (%d), trying to move to next file.\n"),
					errno);

	    if(tapefd >= 0)
		close(tapefd);

	    tapefd = -1;

#if 0		/* suggested by Piotr, breaks filesystem pseudotapes */
	    tapeaccessmode = NOT_OPEN;
#endif

	    bytes_read = 0;

	    i = set_filenum(actfilenum, NO, NO);
	    if(!i){
		tapefd = open(filename, filemode);

		if(tapefd >= 0){
		  tapeaccessmode = OPENED_FOR_READ;
		  errno = 0;

		  i = read_forced(tapefd, tapebuf, tapeblocksize);
		  *num_bytes = i;
		  if(i > 0){
		    rdfilenum = actfilenum;

		    bytes_read += i;

		    read_rate += i;

		    goto done_read;
		  }
		}
	    }
	  }

	  if(streamerbuffer){
	    if(!force_cartchange){	/* in read-ahead mode do */
#ifdef	HAVE_POSIX_THREADS
		pthread_mutex_lock(&may_modify_buffer_ptrs);
#endif
		at_mediaend = YES;	/* not change tape until */

#ifdef	HAVE_POSIX_THREADS

		while(!force_cartchange){
		  pthread_cond_signal(&buffer_contains_data);
		  pthread_cond_wait(&can_write_to_buffer,
						&may_modify_buffer_ptrs);
		}
		pthread_mutex_unlock(&may_modify_buffer_ptrs);

#else
		return(NO_ERROR);	/* forced explicitly */
#endif
	    }

	    force_cartchange = at_mediaend = NO;	/* reset flags */
	  }

	  if(tapefd >= 0)
	    close(tapefd);

	  tapefd = -1;

	  bytes_read = 0;

	  *bufferpos = 0;
	  endofrec = NO;

	  if(!AUTOCHCART)
	    return(ENDOFTAPEREACHED);

	  next_read_pos(&newcart, &newfile, actcart, active_cartset);

	  i = set_cartridge(newcart, NO);
	  if(i){
	    logmsg(LOG_ERR, T_("Error: Cannot change to cartridge %d for reading.\n"),
				(int) newcart);
	    ret = REOPEN_FAILED;
	    GETOUT;
	  }

	  i = set_filenum(newfile, YES, NO);
	  if(i){
	    logmsg(LOG_ERR, T_("Error: Cannot change to file %d for reading.\n"),
				(int) newfile);

	    ret = REOPEN_FAILED;
	    GETOUT;
	  }

	  if(write_tapepos_file_msg()){
	    ret = REOPEN_FAILED;
	    GETOUT;
	  }

	  filename = devicename;
	  filemode = O_RDONLY | O_BINARY;

	  if(IS_DIRECTORY(devstatb)){
	    filename = storefile;
	  }

	  tapefd = open(filename, filemode);
	  if(tapefd < 0){
	    ret = REOPEN_FAILED;
	    GETOUT;
	  }

#ifdef	HAVE_POSIX_THREADS
	if(streamerbuffer)
	  pthread_mutex_lock(&tapepos_mutex);
#endif

	  rdfilenum = actfilenum;
	  rdcart = actcart;

#ifdef	HAVE_POSIX_THREADS
	if(streamerbuffer)
	  pthread_mutex_unlock(&tapepos_mutex);
#endif

	  tapeaccessmode = OPENED_FOR_READ;

	  if(have_infobuffer && lbl_tapeblocksize > 0){
	    i = change_tapeblocksize(lbl_tapeblocksize, &tapebuf,
				num_bytes, &dont_read);
	    if(i){
		ret = i;
		GETOUT;
	    }

	    if(dont_read){
		goto done_read;
	    }
	  }

	  /*if(*num_bytes == 0)*/{	/* always read the block again */
 	  i = read_forced(tapefd, tapebuf, tapeblocksize);

	  if(i > 0)
	    bytes_read += i;
	  }
	}

	*num_bytes = i;

	if(i > 0)
	  read_rate += i;
    }
  }

 done_read:

  tape_moved = YES;
  tape_rewound = NO;

  return(NO_ERROR);

 getout:
  if(tapefd >= 0)
    close(tapefd);

  tapeaccessmode = NOT_OPEN;
  tapefd = -1;

  return(ret);
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

#if 0	/* currently not needed */
static Int32
rewrite_tapeposfile_msg()
{
  Int32		r;

  if( (r = rewrite_tapeposfile()) )
    logmsg(LOG_ERR, T_("Error: Rewriting the tapepos-file failed.\n"));

  return(r);
}
#endif

Int32
write_tapepos_file()	/* write the file to store cartridge and file */
{
  Int32		i, r;
  UChar		numstr[64], keystr[32];

  ER__(rewrite_tapeposfile(), r);

  i = (actfilenum >= 1 ? actfilenum : 1);	/* CHECK: why sometimes 0 ? */
  sprintf(numstr, "%ld %ld", (long int) actcart, (long int) i);
  r = kfile_insert(tapeposfile, devicename, numstr, KFILE_SORTN | KFILE_LOCKED);
  if(r)
    return(r);

  for(i = 0; i < num_cartsets; i++){
    sprintf(numstr, "%ld %ld",
		(long int) inscart[i], (long int) insfilenum[i]);
    sprintf(keystr, "%ld:", (long int) i + 1);

    r = kfile_insert(tapeposfile, keystr, numstr, KFILE_SORTN | KFILE_LOCKED);
    if(r)
	return(r);
  }

  statusmsg(T_("Saved tapepos: %ld.%ld (cartset %ld) writepos at %ld.%ld\n"),
		(long int) actcart, (long int) actfilenum,
		(long int) active_cartset + 1,
		(long int) inscart[active_cartset],
		(long int) insfilenum[active_cartset]);

  return(0);
}

Int32
write_tapepos_file_msg()
{
  Int32		i;

  if( (i = write_tapepos_file()) )
	logmsg(LOG_ERR, T_("%s, Error: Cannot write file %s to save the tape state.\n"),
			actimestr(), tapeposfile);

  return(i);
}

Int32
read_tapepos_file()	/* read the file to get cartridge and filenum */
{
  long int	i, j, n;
  long int	idx;	/* uninitialized OK */
  long int	acart, afile, icart, ifile;
  Int32		num_leftover, num_leftsets, r = 0;
  Int32		*leftovercart = NULL, *leftoverfilenum = NULL;
  Int32		*leftoverset = NULL;
  UChar		keystr[32], *cptr;

  if(!strcmp(devicename, "-"))
    return(0);

  if(eaccess(tapeposfile, R_OK))
    return(-EACCES);

  ER__(rewrite_tapeposfile(), r);

  acart = afile = 1;

  cptr = kfile_get(tapeposfile, devicename, KFILE_LOCKED);
  if(!cptr){
    logmsg(LOG_WARNING, T_("Warning: tape-state file found, but no entry for device `%s'. Assuming current position as cartridge %d, file %d.\n"),
				devicename, (int) acart, (int) afile);
  }
  else{
    n = sscanf(cptr, "%ld%ld", &acart, &afile);
    free(cptr);
    if(n < 2)
	logmsg(LOG_WARNING, T_("Warning: Current position entry for device `%s' incomplete in tape-state file. Assuming current position as cartridge %d, file %d.\n"),
				devicename, (int) acart, (int) afile);
  }

  actcart = rdcart = acart;
  actfilenum = rdfilenum = afile;

  memset(inscart, 0, num_cartsets * sizeof(Int32));
  memset(insfilenum, 0, num_cartsets * sizeof(Int32));

  for(i = 0; i < num_cartsets; i++){
    sprintf(keystr, "%d:", (int) i + 1);
    cptr = kfile_get(tapeposfile, keystr, KFILE_LOCKED);
    if(!cptr)
	continue;

    n = sscanf(cptr, "%ld%ld", &icart, &ifile);
    free(cptr);
    if(n < 2)
	continue;

    inscart[i] = icart;
    insfilenum[i] = ifile;
  }

  leftovercart = NEWP(Int32, num_cartsets);
  leftoverfilenum = NEWP(Int32, num_cartsets);
  leftoverset = NEWP(Int32, num_cartsets);
  if(!leftovercart || !leftoverfilenum || !leftoverset){
    r = FATAL_ERROR;
    GETOUT;
  }
  num_leftover = 0;

  for(i = 0, num_leftover = 0; i < num_cartsets; i++){
    if(!in_Uns32Ranges(cartridge_sets[i], inscart[i])){
	if(inscart[i])
	  logmsg(LOG_INFO, T_("Warning: Insert cartridge %d for set %d not in this set. Trying to reorganize.\n"),
						inscart[i], i + 1);
	leftoverset[num_leftover] = i;
	leftovercart[num_leftover] = inscart[i];
	leftoverfilenum[num_leftover] = insfilenum[i];
	num_leftover++;
    }
  }

  if(num_leftover > 0){
    for(i = 0; i < num_leftover; i++){
	for(j = 0, n = 0; j < num_leftover; j++){
	  if(in_Uns32Ranges(cartridge_sets[leftoverset[j]], leftovercart[i])){
	    n++;
	    idx = j;
	  }
	}

	if(n == 1){	/* if there is exactly 1, it's in the wrong position */
	  j = leftoverset[idx];
	  inscart[j] = leftovercart[i];
	  insfilenum[j] = leftoverfilenum[i];

	  num_leftover--;
	  memmove(leftovercart + i, leftovercart + i + 1,
				(num_leftover - i) * sizeof(Int32));
	  memmove(leftoverfilenum + i, leftoverfilenum + i + 1,
				(num_leftover - i) * sizeof(Int32));
	  memmove(leftoverset + idx, leftoverset + idx + 1,
				(num_leftover - idx) * sizeof(Int32));
	  i--;
	}

	if(n == 0)	/* if no set found, this is a wrong entry */
	  leftovercart[i] = leftoverfilenum[i] = 0;
    }
  }

  num_leftsets = num_leftover;
  if(num_leftsets > 0){
    for(i = 0; i < num_leftsets; i++){
	for(j = 0; j < num_leftover; j++){
	  if(in_Uns32Ranges(cartridge_sets[leftoverset[i]], leftovercart[j]))
	    break;
	}

	if(j >= num_leftover){
	  n = leftoverset[i];
	  inscart[n] = cartridge_sets[n][0].first;
	  insfilenum[n] = 1;

	  logmsg(LOG_NOTICE, T_("Warning: Could not find start cartridge for set %d. Starting with cartridge %d, file 1.\n"),
					n + 1, inscart[n]);

	  num_leftsets--;
	  memmove(leftoverset + i, leftoverset + i + 1,
				(num_leftsets - i) * sizeof(Int32));
	  i--;

	  for(j = 0; j < num_leftover; j++){
	    if(!leftovercart[j]){
		num_leftover--;
		memmove(leftovercart + j, leftovercart + j + 1,
				(num_leftover - j) * sizeof(Int32));
		memmove(leftoverfilenum + j, leftoverfilenum + j + 1,
				(num_leftover - j) * sizeof(Int32));
		break;
	    }
	  }
	}
    }
  }

#if	0	/* not now FIXME */
  if(num_leftsets > 0){	/* now it gets heavy - don't need this */
  }
#endif

 getout:
  ZFREE(leftovercart);
  ZFREE(leftoverfilenum);
  ZFREE(leftoverset);

  return(r);
}

Int32
get_bytes_per_tape_msg(
  UChar		*filen,
  Int32		cart,
  Real64	*numbytes,
  Int32		*numfiles,
  Flag		*tapefull,
  time_t	*thetime)
{
  Int32		i;

  if( (i = get_bytes_per_tape(filen, cart,
				numbytes, numfiles, tapefull, thetime)) ){
    logmsg(LOG_ERR, T_("%s, Error: Cannot read file %s to get the number of bytes on tape.\n"),
			actimestr(), filen);
  }

  return(i);
}

Int32
save_bytes_per_tape_msg(
  UChar		*filen,
  Int32		cart,
  Real64	numbytes,
  Int32		numfiles,
  Flag		tapefull,
  time_t	thetime)
{
  Int32		i;

  if( (i = save_bytes_per_tape(filen, cart, numbytes, numfiles, tapefull,
			thetime)) )
	logmsg(LOG_ERR, T_("%s, Error: Cannot write file %s to save the number of bytes on tape.\n"),
			actimestr(), filen);

  return(i);
}

Int32
run_tape_full_cmd_msg(Int32 cart, Int32 nuses)
{
  Int32		r;

  r = run_tape_full_cmd(cart, nuses);
  if(r > 0)
	logmsg(LOG_WARNING, T_("Warning: Command to run when tape full returned status %d.\n"),
		(int) r);
  if(r < 0)
	logmsg(LOG_ERR, T_("Error: Failed to run command to run when tape full.\n"));

  return(r);
}

Int32
run_tape_full_cmd(Int32 cart, Int32 nuses)
{
  UChar		*cmd = NULL, *cptr, nbuf[20];
  Int32		r = 0;

  if(!tapefullcmd)
    return(0);

  sprintf(nbuf, "%d", (int) cart);
  cmd = repl_substring(tapefullcmd, "%c", nbuf);
  if(!cmd)
    return(-1);

  sprintf(nbuf, "%d", (int) nuses);
  cptr = repl_substring(cmd, "%n", nbuf);
  free(cmd);
  if(!cptr)
    return(-1);

  cmd = repl_dev_pats(cptr, devicename, changername);
  free(cptr);
  if(!cmd)
    return(-1);

  r = system(cmd);
  if(WEXITSTATUS(r) == 0 && r >= 0)
    r = 0;

  return(r);
}

Int32
tape_full_actions(Int32 cart)
{
  Int32		i;

  if((i = incr_tape_use_count(tape_usage_file, cart)) < 0){
    logmsg(LOG_WARNING, T_("Warning: Could not register tape usage increment.\n"));
  }
  else{
    run_tape_full_cmd_msg(cart, i);
  }

  return(i);
}

Int32
reuse_tapes(UChar * str)
{
  UChar		*cptr, nbuf[32];
  Uns32Range	*tapes_to_reuse = NULL, *acttapes = NULL, *rgptr;
  Int32		r = 0, i, w, num_entries = 0;
  int		i1;
  KeyValuePair	*allentries = NULL;

  massage_string(str);

  tapes_to_reuse = sscan_Uns32Ranges__(str, 1, num_cartridges, NULL, NULL);
  if(!tapes_to_reuse)
    return(-1);

  allentries = kfile_getall(precious_tapes_file, &num_entries, KFILE_LOCKED);
  if(allentries){
    for(i = 0; i < num_entries; i++){
      w = word_count(allentries[i].value);
      if(w < 1){
	logmsg(LOG_WARNING, T_("Warning: Unrecognized line in file `%s': %s ... , deleting\n"),
				precious_tapes_file, allentries[i].key);
	kfile_delete(precious_tapes_file, allentries[i].key, KFILE_LOCKED);
	continue;
      }

      ZFREE(acttapes);
      acttapes = sscan_Uns32Ranges__(allentries[i].value, 1, num_cartridges, NULL, NULL);
      if(!acttapes)
	CLEANUPR(-3);

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
	kfile_delete(bytesontape_file, nbuf, KFILE_LOCKED);
    }
  }

  /* set write position on the tape to file 1, if writing position is on it */
  for(i = 0, i1 = 0; i < num_cartsets; i++){
    if(in_Uns32Ranges(tapes_to_reuse, inscart[i])){
	insfilenum[i] = 1;
	i1 = 1;
    }
  }
  if(i1)
    ER__(write_tapepos_file_msg(), i);

 cleanup:
  ZFREE(tapes_to_reuse);
  ZFREE(acttapes);
  kfile_freeall(allentries, num_entries);

  return(r);
}

#if 0	/* seems we don't need this */
Int32
delete_tape(Int32 cartno)
{
  char	buf[32];

  sprintf(buf, "%d", (int) cartno);

  return(reuse_tapes(buf));
}
#endif

Int32
set_precious_tapes(UChar * str)
{
  UChar		*cptr, *new_value = NULL;
  Int32		r = 0;

  cptr = sscanwordq(str, str);
  if(!cptr)
    return(-2);

  if(!empty_string(cptr))
    new_value = cptr;

  if(new_value){
    cptr = sscanword(new_value, new_value);
    if(cptr)
      if(!empty_string(cptr))
	logmsg(LOG_WARNING, T_("Warning: Trailing garbage ignored in message `%s'.\n"), str);
  }

  if(new_value)
    r = kfile_insert(precious_tapes_file, str, new_value,
				KFILE_SORT | KFILE_LOCKED);
  else
    r = kfile_delete(precious_tapes_file, str, KFILE_LOCKED);
  if(r)
    logmsg(LOG_ERR, T_("Error: Cannot write file `%s' to save the precious tapes"),
			precious_tapes_file);

  return(r);
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

  fd = set_wlock_forced(ro_tapes_file);
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

Uns32Range *
get_all_nowrite_tapes()
{
  Uns32Range	*alltapes, *newtapes = NULL;
  KeyValuePair	*allentries = NULL;
  Int32		num_entries = 0, i;

  alltapes = get_readonly_tapes();
  if(!alltapes)
    return(NULL);

  allentries = kfile_getall(precious_tapes_file, &num_entries, KFILE_LOCKED);
  if(!allentries)
    CLEANUP;

  for(i = 0; i < num_entries; i++){
    newtapes = sscan_Uns32Ranges(allentries[i].value,
				1, num_cartridges, NULL, NULL);
    if(!newtapes)
	GETOUT;

    merge_Uns32Ranges(&alltapes, newtapes);
    ZFREE(newtapes);
  }

 cleanup:
  kfile_freeall(allentries, num_entries);
  ZFREE(newtapes);

  return(alltapes);

 getout:
  ZFREE(alltapes);

  CLEANUP;
}

Int32
in_readonly_tapes(Flag * contained, Int32 cart)
{
  Uns32Range	*rotapes;

  rotapes = get_readonly_tapes();
  if(!rotapes)
    return(-1);

  *contained = in_Uns32Ranges(rotapes, cart);
  free(rotapes);

  return(0);
}

Int32
in_nowrite_tapes(Flag * contained, Int32 cart)
{
  Uns32Range	*nowrtapes;

  nowrtapes = get_all_nowrite_tapes();
  if(!nowrtapes)
    return(-1);

  *contained = in_Uns32Ranges(nowrtapes, cart);
  free(nowrtapes);

  return(0);
}

Int32
set_readonly_tapes(UChar * rangestr, Flag ro)
{
  Uns32Range	*readonly_tapes = NULL, *tapes = NULL;
  Int32		r = 0;
  UChar		*newstr = NULL;
  int		fd;

  fd = -1;

  tapes = sscan_Uns32Ranges__(rangestr, 1, num_cartridges, NULL, NULL);
  readonly_tapes = get_readonly_tapes();
  if(!tapes || !readonly_tapes){
    r = -1;
    CLEANUP;
  }

  if(ro){
    r = merge_Uns32Ranges(&readonly_tapes, tapes);
    if(r)
	CLEANUP;
  }
  else{
    readonly_tapes = del_range_from_Uns32Ranges(readonly_tapes, tapes);
    if(!readonly_tapes){
	r = -2;
	CLEANUP;
    }
  }

  newstr = str_Uns32Ranges(readonly_tapes, 0);
  if(!newstr){
    r = -3;
    CLEANUP;
  }

  fd = set_wlock_forced(ro_tapes_file);
  if(fd < 0)
    CLEANUP;
  ftruncate(fd, 0);
  write(fd, newstr, strlen(newstr));
  write(fd, "\n", 1);

 cleanup:
  if(fd >= 0)
    close(fd);
  ZFREE(tapes);
  ZFREE(readonly_tapes);
  ZFREE(newstr);

  return(r);
}

Int32
eval_message(UChar * message, Int32 msgsize)
{
  UChar		*cptr, *cptr2;
  Int32		i;

  cptr = message;

  forever{
    cptr2 = strchr(cptr, '\n');
    if(cptr2)
	*cptr2 = '\0';

    if(!strncasecmp(cptr, PRECIOUS_TAPES_MESSAGE,
				i = strlen(PRECIOUS_TAPES_MESSAGE))){
	i = set_precious_tapes(cptr + i);
	if(i)
	  return(i);
    }
    else if(!strncasecmp(cptr, READONLY_TAPES_MESSAGE,
				i = strlen(READONLY_TAPES_MESSAGE))){
	i = set_readonly_tapes(cptr + i, YES);
	if(i)
	  return(i);
    }
    else if(!strncasecmp(cptr, RDWR_TAPES_MESSAGE,
				i = strlen(RDWR_TAPES_MESSAGE))){
	i = set_readonly_tapes(cptr + i, NO);
	if(i)
	  return(i);
    }
    else if(!strncasecmp(cptr, REUSE_TAPES_MESSAGE,
				i = strlen(REUSE_TAPES_MESSAGE))){
	i = reuse_tapes(cptr + i);
	if(i)
	  return(i);
    }
    else if(!strcasecmp(cptr, CARTREADY_MESSAGE)){
	i = unlink(CARTREADY_FILE);
	if(i && errno && errno != ENOENT){
	  logmsg(LOG_ERR, T_("Error unlinking file `%s': %s\n"), CARTREADY_FILE,
					strerror(errno));
	  return(i);
	}
    }
    else if(!strncasecmp(cptr, DELETE_CLIENT_MESSAGE,
				i = strlen(DELETE_CLIENT_MESSAGE))){
	cptr += i;
	massage_string(cptr);
	i = kfile_delete(precious_tapes_file, cptr, KFILE_LOCKED);
	if(i){
	  logmsg(LOG_WARNING, T_("Warning: Removing client `%s' from `%s' failed.\n"),
				cptr, precious_tapes_file);
	  return(i);
	} 
    }
    else{
	logmsg(LOG_WARNING, T_("Got Message (unrecognized): %s\n"), cptr);
	return(-9);
    }

    if(!cptr2)
	break;
    cptr = cptr2 + 1;
  }

  return(0);
}

Int32
get_eval_message(int fd)
{
  Int32		msgsize, r = 0;
  UChar		msgbuf[BUFFERSIZ + 1], *msg;

  if(read_forced(fd, msgbuf, 4) != 4)
    return(- errno);

  xref_to_Uns32(&msgsize, msgbuf);
  msg = msgsize > BUFFERSIZ ? NEWP(UChar, msgsize + 1) : msgbuf;
  if(!msg){
    r = -1;
    CLEANUP;
  }

  if(read_forced(fd, msg, msgsize) != msgsize){
    r = -2;
    CLEANUP;
  }

  msg[msgsize] = '\0';

  r = eval_message(msg, msgsize);

 cleanup:
  if(msg && msg != msgbuf)
    free(msg);

  return(r);
}
  
void
usage(UChar * prnam)
{
  logmsg(LOG_ERR, T_("Usage: %s [ -Sb ] [ <configuration-file> ] [ -L <locale> ]\n"), prnam);
  do_exit(1);
}

#define	cfr	cmd

main(int argc, char ** argv)
{
  Int32		i, j, cmdres;
  UChar		*cptr, buf[256];
  UChar		*localestr = NULL;
  Uns32		cmd, u;
  UChar		**cpptr;
  Uns8		in_trouble;
  Flag		unsecure = NO;
  struct stat	statb;
  struct linger	linger;

  if(isatty(0)){
    fprintf(stderr, T_("Error: This program must be started by the inetd. See the documentation for details.\n"));
    exit(1);
  }

  gargv = argv;
  gargc = argc;

  SETZERO(remoteuser);
  SETZERO(g_lock);
  g_lock.locked = BU_NO_LOCK;

  umask(0022);

  if(sizeof(Int32) > 4){
    fprintf(stderr, T_("Error: Long int is wider than 4 Bytes. Inform author about machine type.\n"));
    do_exit(1);
  }

  if(goptions(argc, (UChar **) argv, "s-1:;s:l;s:x;b:b;b:D;b:s;b:S;s:L",
		&i, &configfilename, &loggingfile, &programdir,
		&nobuffering, &edebug, &unsecure, &slavemode, &localestr))
    usage(argv[0]);

  signal(SIGUSR1, sig_handler);
  while(edebug);	/* For debugging the caught running daemon */

#ifdef ENABLE_NLS
  setlocale(LC_ALL, localestr ? localestr : (UChar *)"");
  if(localestr){	/* some implementations of gettext don't honour */
    set_env("LC_ALL", localestr);	/* the settings done by setlocale, */
  }					/* but always look into the env */
  bindtextdomain(PACKAGE, LOCALEDIR);
  textdomain(PACKAGE);
#endif

  if(!slavemode){
    commrfd = commwfd = dup(0);
    close(0);
  }
  else{
    commrfd = 0;
    commwfd = 1;
  }

  fcntl(commrfd, F_SETFD, fcntl(commrfd, F_GETFD) | 1);
  fcntl(commwfd, F_SETFD, fcntl(commwfd, F_GETFD) | 1);

  signal(SIGPIPE, sig_handler);
  signal(SIGTERM, sig_handler);
  signal(SIGHUP, sig_handler);
  signal(SIGINT, sig_handler);
  signal(SIGSEGV, sig_handler);
  signal(SIGBUS, sig_handler);

  i = uname(&unam);
  if(i < 0)
    strcpy(unam.nodename, "<unknown>");

  if(!configfilename){
    programpath = find_program(argv[0]);	/* programpath is only */
    if(!programpath){		/* needed, if no config file is given */
#ifdef	ORIG_DEFAULTS
	fprintf(stderr, T_("Warning: Could not find program file of `%s'\n"),
				argv[0]);
#endif
    }
    else{
	EM__(backuphome = strdup(programpath));
	EM__(cptr = mkabspath(backuphome, NULL));
	free(backuphome);
	EM__(backuphome = resolve_path__(cptr, NULL));
	free(cptr);
	if(backuphome){
	  cptr = FN_LASTDIRDELIM(backuphome);
	  if(cptr){
	    *cptr = '\0';
	    cptr = FN_LASTDIRDELIM(backuphome);
	    if(cptr)
		*cptr = '\0';
	  }
	}
    }

    for(cpptr = default_configfilenames; *cpptr; cpptr++);

#ifdef	ORIG_DEFAULTS
    EM__(*cpptr = strapp(backuphome, FN_DIRSEPSTR "etc" FN_DIRSEPSTR DEFSERVERCONF));
#else
    EM__(*cpptr = strdup(DEFSERVCONFDIR FN_DIRSEPSTR DEFSERVERCONF));
#endif

    for(cpptr = default_configfilenames; *cpptr; cpptr++){
      configfilename = *cpptr;
      if(!stat(*cpptr, &statb) && eaccess(*cpptr, R_OK)){
	cfr = 1;
	while(*cpptr) cpptr++;
	break;
      }
      if(!stat(*cpptr, &statb))
	break;
    }

    configfilename = *cpptr;
  }

#ifdef	ORIG_DEFAULTS
  if(!backuphome){
    EM__(backuphome = strdup(DEFSERVBINDIR));
    cleanpath(backuphome);
    cptr = FN_LASTDIRDELIM(backuphome);
    if(cptr)
	*cptr = '\0';		/* use default like with no ORIG_DEFAULTS */
  }
#endif

  if(!slavemode){	/* get name (or IP-address) of the connected host */
    EM__(remotehost = get_connected_peername(commrfd));
  }
  else{
    remotehost = "slave-server on localhost";
  }

  cfr = serverconf(NO);

  if(cfr)
    logmsg(LOG_ERR, T_("Warning: Error reading configuration file `%s'.\n"),
		configfilename ? configfilename : (UChar *) T_("<none found>"));

  if(!slavemode){
    i = set_ip_throughput(commwfd);
    if(i)
	logmsg(LOG_WARNING, T_("Warning: Setting TCP/IP throughput failed with %d.\n"),
			(int) i);

    if(set_socket_keepalive(commwfd))
	logmsg(LOG_WARNING, T_("Warning: Setting Keepalive failed with %d\n"), errno);

    linger.l_onoff = 1;
    linger.l_linger = 60;
    if ( (i = setsockopt(commrfd, SOL_SOCKET, SO_LINGER, (char *) &linger,
						sizeof (linger))) < 0)
      logmsg(LOG_WARNING, T_("Warning: setsockopt (SO_LINGER): %d"), errno);
  }

  register_pref_serv();	/* serve same client immediately, others with delay */

  if(init_cmd){	/* run the init command, if supplied */
    cptr = repl_substring(init_cmd, "%p", remotehost);
    if(!cptr){
	logmsg(LOG_WARNING, T_("Warning: Could not replace %%p in init command, leaving it unmodified.\n"));
    }
    else{
	free(init_cmd);
	init_cmd = cptr;
    }

    i = system(init_cmd);
    if(i < 0 || WEXITSTATUS(i)){
	logmsg(LOG_WARNING, T_("Warning: Executing init command %s returned exit status %d.\n"),
				init_cmd, (int) i);
    }
  }

  i = authenticate_client(commwfd, commrfd, unsecure,
			VERSION_MESSAGE GREETING_MESSAGE, cfr, AUTHENTICATION,
#ifdef	HAVE_DES_ENCRYPTION
			AUTH_USE_DES
#else
			0
#endif
			, 0);
  if(i){
    logmsg(LOG_CRIT, T_("Error: Authentication failed for host %s. Exiting.\n"),
						remotehost);
    do_exit(1);
  }

  in_trouble = 0;

  while(get_input(buf)){	/* command processing */
    cmd = buf[0];

    if(interrupted){
	goodbye(1);
	do_exit(1);
    }

    cmdres = 0;

    if(in_trouble){
      switch(cmd){
	case OPENFORWRITE:
	case OPENFORRAWWRITE:
	case ERASETAPE:
	case WRITETOTAPE:
	case CLIENTBACKUP:
	case OCLIENTBACKUP:
	  command_failed(cmd,
		T_("skipping possibly destructive commands while in trouble"));
	  cmd = NOOPERATION;
	  cmdres = PROTOCOL_ERROR;
	  break;
      }
    }

    switch(cmd){
      case WRITETOTAPE:
	cmdres = write_to_tape();
	if(cmdres)
	  command_failed(cmd, T_("writing to tape failed."));
	break;

      case READFROMTAPE:
	cmdres = read_from_tape();
	if(cmdres == ENDOFFILEREACHED || cmdres == ENDOFTAPEREACHED)
	  cmdres = 0;

	if(cmdres)
	  command_failed(cmd, T_("reading from tape failed."));
	break;

      case GETNUMREADVALID:
	cmdres = sendint(num_transf_bytes_valid, 4);
	if(cmdres)
	  command_failed(cmd, T_("sending number of valid transferred bytes failed."));
	break;

      case SETNUMWRITEVALID:
	cmdres = setnumwritevalid();
	if(cmdres)
	  command_failed(cmd, T_("setting number of valid bytes to transfer failed."));
	break;

      case QUERYPOSITION:
	cmdres = sendtapepos();
	if(cmdres)
	  command_failed(cmd, T_("getting the tape position failed."));
	break;

      case QUERYWRPOSITION:
	cmdres = sendtapewrpos();
	if(cmdres)
	  command_failed(cmd, T_("getting the tape write position failed."));
	break;

      case QUERYRDPOSITION:
	cmdres = sendtaperdpos();
	if(cmdres)
	  command_failed(cmd, T_("getting the tape read position failed."));
	break;

      case REQUESTNEWFILE:
	newfilerequested = YES;
	send_status(COMMAND_OK);
	break;

      case REQUESTNEWCART:
	cmdres = requestnewcart_cmd();
	if(cmdres)
	  command_failed(cmd, T_("requesting new cartridge for writing"));
	break;

      case QUERYNUMCARTS:
	cmdres = sendint(num_cartridges, 3);
	if(cmdres)
	  command_failed(cmd, T_("determining the number of cartridges failed."));
	break;

      case QUERYRDYFORSERV:
	cmdres = sendifbusy();
	if(cmdres)
	  command_failed(cmd, T_("checking for service ability failed."));
	break;

      case QUERYCARTSET:
	cmdres = sendint(active_cartset + 1, 3);
	if(cmdres)
	  command_failed(cmd, T_("getting the current cartridge set failed."));
	break;

      case QUERYTAPEBLOCKSIZE:
	cmdres = sendint(tapeblocksize, 4);
	if(cmdres)
	  command_failed(cmd, T_("getting the tape blocksize set failed."));
	break;

      case QUERYWRITTENTAPES:
	cmdres = sendwrtapes(written_tapes);
	if(cmdres)
	  command_failed(cmd, T_("getting the written tapes failed."));
	break;

      case QUERYNEEDEDTAPES:
	cmdres = sendclientstapes();
	if(cmdres)
	  command_failed(cmd, T_("getting the client's tapes failed."));
	break;

      case SETCARTRIDGE:
      case SETRAWCARTRIDGE:
	cmdres = (read_forced(commrfd, buf, 3) != 3);
	if(cmdres){
	  prot_error(T_("no argument to command %d"), cmd);
	  break;
	}
	xref_to_UnsN(&j, buf, 24);
	cmdres = set_cartridge_cmd(cmd == SETRAWCARTRIDGE ? - j : j);
	if(cmdres)
	  command_failed(cmd, T_("cannot set cartridge"));
	break;

      case SETFILE:
      case SETRAWFILE:
	cmdres = (read_forced(commrfd, buf, 4) != 4);
	if(cmdres){
	  prot_error(T_("no argument to command %d"), cmd);
	  break;
	}
	xref_to_Uns32(&j, buf);
	cmdres = set_filenum_cmd(cmd == SETRAWFILE ? - j : j);
	if(cmdres)
	  command_failed(cmd, T_("cannot set file"));
	break;

      case SKIPFILES:
	cmdres = (read_forced(commrfd, buf, 4) != 4);
	if(cmdres){
	  prot_error(T_("no argument to command %d"), cmd);
	  break;
	}
	xref_to_Uns32(&u, buf);
	cmdres = skip_files_cmd(u);
	if(cmdres)
	  command_failed(cmd, T_("cannot skip over files"));
	break;

      case SETCARTSET:
	cmdres = (read_forced(commrfd, buf, 3) != 3);
	if(cmdres){
	  prot_error(T_("no argument to command %d"), cmd);
	  break;
	}
	cmdres = set_cartset_cmd(((Uns32) buf[0] << 16) +
				((Uns32) buf[1] << 8) + buf[2]);
	if(cmdres)
	  command_failed(cmd, T_("cannot set cartridge set"));
	break;

      case SETBUFFEREDOP:
	if(TAPE_NOT_CLOSED){
	  cmdres = DEVINUSE;
	  command_failed(cmd, T_("not allowed during device access"));
	}
	else
	  streamermode |= BUFFERED_OPERATION;

	send_status(cmdres);

	break;

      case SETSERIALOP:
	if(TAPE_NOT_CLOSED){
	  cmdres = DEVINUSE;
	  command_failed(cmd, T_("not allowed during device access"));
	}
	else
	  streamermode &= ~BUFFERED_OPERATION;

	send_status(cmdres);

	break;

      case SETCHCARTONEOT:
	if(TAPE_NOT_CLOSED){
	  cmdres = DEVINUSE;
	  command_failed(cmd, T_("not allowed during device access"));
	}
	else
	  streamermode |= CHANGE_CART_ON_EOT;

	send_status(cmdres);

	break;

      case SETERRORONEOT:
	if(TAPE_NOT_CLOSED){
	  cmdres = DEVINUSE;
	  command_failed(cmd, T_("not allowed during device access"));
	}
	else
	  streamermode &= ~CHANGE_CART_ON_EOT;

	send_status(cmdres);

	break;

      case OPENFORWRITE:
	cmdres = opentapewr_cmd(0);
	if(cmdres)
	  command_failed(cmd, T_("cannot open device for writing"));
	break;

      case OPENFORREAD:
	cmdres = opentaperd_cmd(0);
	if(cmdres)
	  command_failed(cmd, T_("cannot open device for reading"));
	break;

      case OPENFORRAWWRITE:
	cmdres = opentapewr_cmd(OPENED_RAW_ACCESS);
	if(cmdres)
	  command_failed(cmd, T_("cannot open device for writing"));
	break;

      case OPENFORRAWREAD:
	cmdres = opentaperd_cmd(OPENED_RAW_ACCESS);
	if(cmdres)
	  command_failed(cmd, T_("cannot open device for reading"));
	break;

      case CLOSETAPE:
	cmdres = closetape_cmd();
	if(cmdres)
	  command_failed(cmd, T_("cannot close device"));
	break;

      case CLOSETAPEN:
	cmdres = closetapen_cmd();
	if(cmdres)
	  command_failed(cmd, T_("cannot close device"));
	break;

      case ERASETAPE:
	cmdres = erasetape_cmd();
	if(cmdres)
	  command_failed(cmd, T_("cannot erase tape"));
	break;

      case GOODBYE:
	cmdres = goodbye_cmd();
	break;

      case CLIENTBACKUP:
	cmdres = client_backup_cmd(NO);
	if(cmdres)
	  command_failed(cmd, T_("running a backup as client failed."));
	break;

      case CLIENTIDENT:
	cmdres = (read_forced(commrfd, client_id, 128) != 128);
	if(cmdres)
	  command_failed(cmd, T_("getting client identity failed."));
	
	register_pref_serv();	/* delay, if different client */
	send_status(cmdres);
	break;

      case USERIDENT:
	cmdres = (read_forced(commrfd, remoteuser, 256) != 256);
	if(cmdres)
	  command_failed(cmd, T_("getting user identity failed."));
	send_status(cmdres);
	break;				/* ignored */

      case SERVERIDENT:
	strncpy(buf, serverid, 255);
	buf[255] = '\0';
	cmdres = (write_forced(commwfd, buf, 256) != 256);
	if(cmdres)
	  command_failed(cmd, T_("sending server identity failed."));
	send_status(cmdres);
	break;				/* ignored */

      case MESSAGETEXT:
	cmdres = get_eval_message(commrfd);
	if(cmdres)
	  command_failed(cmd, T_("evaluating client message failed."));
	send_status(cmdres);
	break;

      case SETCOMMBUFSIZ:
	cmdres = (read_forced(commrfd, buf, 4) != 4);
	if(cmdres){
	  prot_error(T_("no argument to command %d"), cmd);
	  break;
	}
	xref_to_Uns32(&i, buf);
	cmdres = (UChar) set_commbufsiz(i);
	if(cmdres)
	  command_failed(cmd, T_("cannot change communication buffer size"));
	send_status(cmdres);
	break;

      case NOOPERATION:
	break;

      case CHECKACCESS:
	cmdres = check_access_cmd();
	if(cmdres)
	  command_failed(cmd, T_("cannot check access restrictions"));
	break;

	/* older versions for backward compatibility */
      case OSETCARTRIDGE:
	cmdres = (read_forced(commrfd, buf, 1) != 1);
	if(cmdres){
	  prot_error(T_("no argument to command %d"), cmd);
	  break;
	}
	cmdres = set_cartridge_cmd(buf[0]);
	if(cmdres)
	  command_failed(cmd, T_("cannot set cartridge"));
	break;

      case OSETFILE:
	cmdres = (read_forced(commrfd, buf, 3) != 3);
	if(cmdres){
	  prot_error(T_("no argument to command %d"), cmd);
	  break;
	}
	cmdres = set_filenum_cmd(((Uns32) buf[0] << 16) +
				((Uns32) buf[1] << 8) + buf[2]);
	if(cmdres)
	  command_failed(cmd, T_("cannot set file"));
	break;

      case OQUERYPOSITION:
	cmdres = osendtapepos();
	if(cmdres)
	  command_failed(cmd, T_("getting the tape position failed."));
	break;

      case OQUERYWRPOSITION:
	cmdres = osendtapewrpos();
	if(cmdres)
	  command_failed(cmd, T_("getting the tape write position failed."));
	break;

      case OCLIENTBACKUP:
	cmdres = client_backup_cmd(YES);
	if(cmdres)
	  command_failed(cmd, T_("running a backup as client failed."));
	break;

      case AUTHENTICATE:
	cmdres = proof_authentication();
	if(cmdres)
	  command_failed(cmd, T_("proofing authenticaton failed."));
	break;

      case GETCURMSG:
	cmdres = send_clientmsg();
	break;

      default:
	if(isspace(cmd))
	  break;

	prot_error(T_("unknown command: %d"), cmd);
	cmdres = 1;
	in_trouble = 10;
    }

    if(!cmdres && in_trouble > 0)
	in_trouble--;
  }

  goodbye(0);
  do_exit(0);

  /* NOTREACHED */
}

Int32
write_infobuffer(Int32 cartno)
{
  UChar		*filename, *comment;
  int		i, filemode;
  Int32		actfn;

  i = wait_for_service(BU_WRLCK, NO);
  if(i)
    return(i);

  statusmsg(T_("Device %s is labeling cartridge %d.\n"), (char *) devicename,
			(int) actcart);

  initmedia(cartno);

  have_infobuffer = NO;

  if(tlbl_cartno > 0 && tlbl_cartno == cartno && tlbl_comment){
    comment = tlbl_comment;
    chop(comment);
  }
  else
    comment = T_("<labeled automatically>");

  sprintf(infobuffer,
		"\n%s\n\n%s%d\n\n%s%d\n\n%s%s\n\n%s%d\n\n%s\n\n%s%s\n",
		INFOHEADER, CARTNOTEXT, (int) cartno,
		CART2NOTEXT, (int) cartno,
		DATETEXT, actimestr(), BLOCKSIZETEXT, (int) conf_tapeblocksize,
		VERSIONTEXT, COMMENTTEXT, comment);

  filename = devicename;
  filemode = O_WRONLY | O_BINARY;

  if(IS_DIRECTORY(devstatb)){
    actfn = actfilenum;	/* here actfilenum might be 0, what is deadly. */
    actfilenum = 1;	/* It must be 1 to set the file correctly */

    if( (i = set_storefile()) )
	return(i);

    actfilenum = actfn;

    filename = storefile;
    filemode = O_WRONLY | O_CREAT | O_TRUNC | O_BINARY;
  }

  tapefd = open(filename, filemode, 0600);
  if(tapefd < 0){
    tapefd = -1;
    return(-errno);
  }

  add_to_written_tapes(cartno);

  if(write_forced(tapefd, infobuffer, infobuffersize) < infobuffersize){
    close(tapefd);
    tapefd = -1;
    return(-errno);
  }

  close(tapefd);

  bytes_on_tape += (Real64) infobuffersize;

  tapefd = -1;

  have_infobuffer = YES;
  tape_label_cartno = scnd_tape_label_cartno = cartno;

  lbl_tapeblocksize = conf_tapeblocksize;

  add_to_int_list_file(used_blocksizes_file, lbl_tapeblocksize);

  tape_moved = YES;
  tape_rewound = NO;

  filenum_incr_by_eof = NO;

  tried_tapeblocksize = infobuffersize;
  EM__(tried_tapeblock = ZRENEWP(tried_tapeblock, UChar, infobuffersize));

#ifdef	HAVE_POSIX_THREADS
	if(streamerbuffer)
	  pthread_mutex_lock(&tapepos_mutex);
#endif

  actfilenum = 1;
  actcart = cartno;

#ifdef	HAVE_POSIX_THREADS
	if(streamerbuffer)
	  pthread_mutex_unlock(&tapepos_mutex);
#endif

  statusmsg(T_("Device %s is idle.\n"), (char *) devicename);

  return(0);
}

Int32
read_infobuffer(Int32 * cartno, Int32 * scnd_no, Flag wait_for_succ)
{
  Int32		i;
  UChar		*cptr, *filename, *token;
  int		i1, n, filemode;

  if(tried_infobuffer){
    if(have_infobuffer){
      *cartno = tape_label_cartno;
      if(scnd_no)
	*scnd_no = scnd_tape_label_cartno;
    }
    else{
      *cartno = actcart;
      if(scnd_no)
	*scnd_no = actcart;
    }

    return(0);
  }

  statusmsg(T_("Device %s is reading the cartridge label.\n"),
				(char *) devicename);

  have_infobuffer = NO;
  memset(infobuffer, 0, sizeof(UChar) * infobuffersize);
  lbl_tapeblocksize = -1;
  ZFREE(tlbl_comment);
  tlbl_cartno = 0;

  ER__(rewindtape(wait_for_succ, NO), i);

  ER__(wait_for_device(BU_RDLCK, NO), i);	/* rewindtape might do nothing */

  statusmsg(T_("Device %s is reading the cartridge label.\n"),
				(char *) devicename);

  tape_moved = YES;
  tape_rewound = NO;
  tried_infobuffer = YES;

  filenum_incr_by_eof = NO;

  filename = devicename;
  filemode = O_RDONLY | O_BINARY;

  if(IS_DIRECTORY(devstatb)){
    filename = storefile;
  }

  i = find_tape_blocksize(filename, &cptr, NO);
  if(i < 1)
    return(0);

  memcpy(infobuffer, cptr, MIN(infobuffersize, i) * sizeof(UChar));
  free(cptr);

  if(!strstr(infobuffer, INFOHEADER))
    return(0);

  token = CARTNOTEXT;
  cptr = strstr(infobuffer, token);
  if(!cptr)
    return(0);

  cptr += strlen(token);
  i = sscanf(cptr, "%d%n", &i1, &n);
  if(i < 1)
    return(0);

  tape_label_cartno = scnd_tape_label_cartno = (Int32) i1;

  token = CART2NOTEXT;
  cptr = strstr(infobuffer, token);
  if(cptr){
    cptr += strlen(token);
    i = sscanf(cptr, "%d%n", &i1, &n);
    if(i > 0)
	scnd_tape_label_cartno = (Int32) i1;
  }

  *cartno = tape_label_cartno;
  if(scnd_no)
    *scnd_no = scnd_tape_label_cartno;

  have_infobuffer = YES;

  token = BLOCKSIZETEXT;
  cptr = strstr(infobuffer, token);
  if(cptr){
    cptr += strlen(token);
    i = sscanf(cptr, "%d%n", &i1, &n);
    if(i > 0){
	lbl_tapeblocksize = (Int32) i1;

	add_to_int_list_file(used_blocksizes_file, lbl_tapeblocksize);
    }
  }

  token = COMMENTTEXT;
  cptr = strstr(infobuffer, token);
  if(cptr){
    if(!(tlbl_comment = strdup(cptr + strlen(token))))
	return(-errno);

    tlbl_cartno = tape_label_cartno;
  }

  statusmsg(T_("Found cartridge number %d (secondary %d)\n"),
			tape_label_cartno, scnd_tape_label_cartno);

  statusmsg(T_("Device %s is idle.\n"), (char *) devicename);

  return(0);
}

Int32
find_tape_blocksize(UChar * devicen, UChar ** read_block, Flag quiet)
{
  static Int32	usual_blocksizes[] = { USUAL_BLOCKSIZES };

  int		fd;
  UChar		*buf = NULL;
  Int32		*tryable_blocksizes = NULL;
  Int32		found_blocksize, i, j, n, n_usual;

  if(tried_tapeblocksize > 0){
    if(have_infobuffer)
	memcpy(tried_tapeblock, infobuffer, MIN(infobuffersize, tried_tapeblocksize));

    if(read_block)
	EM__(*read_block = memdup(tried_tapeblock, tried_tapeblocksize));

    return(tried_tapeblocksize);
  }

  n_usual = sizeof(usual_blocksizes) / sizeof(usual_blocksizes[0]);
  found_blocksize = tried_tapeblocksize = -1;
  fd = -1;

  tryable_blocksizes = get_list_from_int_list_file(used_blocksizes_file, &n);
  if(tryable_blocksizes){
    if( (tryable_blocksizes = RENEWP(tryable_blocksizes, Int32, 1 + n + n_usual)) )
	memmove(tryable_blocksizes + 1, tryable_blocksizes, sizeof(Int32) * n);
  }
  else{
    tryable_blocksizes = NEWP(Int32, 1 + n_usual);
    n = 0;
  }
  if(!tryable_blocksizes)
    CLEANUP;

  tryable_blocksizes[0] = conf_tapeblocksize;
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
    tried_tapeblock = ZRENEWP(tried_tapeblock, UChar, tryable_blocksizes[i]);
    if(!tried_tapeblock)
	CLEANUP;

    if(!IS_REGFILE(devstatb) && !IS_DIRECTORY(devstatb)){
	j = rewindtape(NO, quiet);
	if(j)
	  return(-ABS(j));
    }

    j = wait_for_service(BU_RDLCK, NO);
    if(j)
	return(j);

    fd = open(devicen, O_RDONLY);
    if(fd < 0)
	CLEANUP;

    j = read(fd, tried_tapeblock, tryable_blocksizes[i]);

    tape_rewound = NO;
    tape_moved = YES;

    if(j > 0){
	found_blocksize = j;
	if(read_block)
	  EM__(*read_block = memdup(tried_tapeblock, j));

	break;
    }

    close(fd);
    fd = -1;
  }

 cleanup:
  if(fd >= 0)
    close(fd);
  ZFREE(tryable_blocksizes);

  return(tried_tapeblocksize = found_blocksize);
}

Int32
try_read_from_tape(Int32 expected_cart)
{
  int		i, filemode;
  UChar		*filename;

  i = wait_for_service(BU_RDLCK, NO);
  if(i)
    return(i);

  initmedia(expected_cart);

  if( (i = stat(devicename, &devstatb)) )
     return(i);

  filename = devicename;
  filemode = O_RDONLY | O_BINARY;

  if(IS_DIRECTORY(devstatb)){
    if( (i = set_storefile()) ){
	return(i);
    }

    filename = storefile;
  }

  i = find_tape_blocksize(filename, NULL, YES);

  return(i);
}

Int32
closetape_cmd()
{
  Int32	i;

  i = closetape(YES);

  if(i){
    send_status(i);
    return(i);
  }

  send_status(0);

  return(0);
}

Int32
closetapen_cmd()
{
  Int32	i;

  i = closetape(NO);

  if(i){
    send_status(i);
    return(i);
  }

  send_status(0);

  return(0);
}

Int32
closetape(Flag rewind)		/* command: close the stream */
{
  Int32		i, ret;
  Int32		files_on_tape;	/* uninitialized OK */
  Flag		must_do_sth;

  ret = 0;
  write_meas_time = read_meas_time = 0;

  DB("closetape(): starting\n",0,0,0);

  if( (must_do_sth = TAPE_NOT_CLOSED) )
    statusmsg(T_("Device %s is closing.\n"), (char *) devicename);

  if((tapeaccessmode & MODE_MASK) == OPENED_FOR_WRITE){
    DB("closetape: flushing bytes to write to tape\n",0,0,0);

    if(tapeptr > 0){
      if(tapeptr < tapeblocksize)
	memset(tapebuffer + tapeptr,
			0, sizeof(UChar) * (tapeblocksize - tapeptr));

      write_tape_buffer(&tapeptr, &i);
    }

    if(!(tapeaccessmode & OPENED_RAW_ACCESS) && bytes_in_tapefile == 0){
	memset(tapebuffer, 0, tapeblocksize * sizeof(UChar));
	write_tape_buffer(&tapeptr, &i);
    }	/* to avoid empty tape files - somewhat undefined, if EOF is written */
  }

  if(must_do_sth)
    statusmsg(T_("Device %s is closing.\n"), (char *) devicename);

  if(TAPE_NOT_CLOSED){
    if(streamerbuffer){
	DB("closetape: stopping async queue\n",0,0,0);

	dismiss_streamer_buffer();
    }

    if((tapeaccessmode & MODE_MASK) == OPENED_FOR_WRITE){
	files_on_tape = actfilenum;

	actfilenum++;
	if(!(tapeaccessmode & OPENED_RAW_ACCESS)){
	  insfilenum[active_cartset]++;
	  if(actfilenum != insfilenum[active_cartset]){
	    logmsg(LOG_CRIT, T_("Error: File numbers for writing inconsistent.\n"));
	  }
	}
    }

    bytes_in_tapefile = bytes_read = 0;

    DB("closetape: flushed bytes to write to tape\n",0,0,0);
    if(tapefd >= 0){
      DB("closetape: closing tape file descriptor\n",0,0,0);

      i = close(tapefd);
      if(i)
	ret = CLOSE_FAILED;
    }
    else{
      if(!streamerbuffer && !at_mediaend)
	logmsg(LOG_CRIT, "Internal error: Device should be open, fd = %d.\n",
			tapefd);
    }

    write_tapepos_file_msg();
  }

  if((tapeaccessmode & MODE_MASK) == OPENED_FOR_WRITE)
    save_bytes_per_tape_msg(bytesontape_file, actcart,
				bytes_on_tape, files_on_tape, NO, time(NULL));

  tapefd = -1;
  tapeaccessmode = (rewind ? NOT_OPEN : CLOSED_NOTREW);

  if((IS_REGFILE(devstatb) || IS_DIRECTORY(devstatb)) && !rewind){
    if( (i = update_devicesettings(actfilenum, YES)) ){
      logmsg(LOG_ERR, T_("Error: Cannot change store file.\n"));
      return(i);
    }
  }

  if(tape_moved && rewind){
    if( (i = rewindtape(NO, NO)) ){
      logmsg(LOG_ERR, T_("Warning: Cannot rewind tape.\n"));

/*      ret = CLOSE_FAILED;*/	/* warning is enough */
    }
  }

  tapeptr = outptr = num_transf_bytes_valid = 0;
  endofrec = at_mediaend = NO;

  DB("closetape: exiting\n",0,0,0);

  if(must_do_sth)
    statusmsg(T_("Device %s is closed, idle.\n"), (char *) devicename);

  return(ret);
}

Int32
next_read_pos(Int32 *cartp, Int32 *filep, Int32 cart, Int32 cartset)
{
  UChar		numbuf[32], *cptr, *value;
  int		i1, i2, n;
  Flag		found = NO;

  sprintf(numbuf, "%d", (int) cart);
  value = kfile_get(cartorder_file, numbuf, KFILE_LOCKED);

  if(value){
    cptr = strstr(value, "->");
    if(cptr && cptr == value){
	cptr = first_nospace(cptr + 2 /* strlen("->") */);
	n = sscanf(cptr, "%d%d", &i1, &i2);
	if(n > 0){
	  *cartp = (Int32) i1;
	  *filep = (Int32) 1;

	  found = YES;
	}
	if(n > 1)
	  *filep = (Int32) (i2 > 0 ? i2 : 1);
    }

    free(value);
  }

  if(!found){
    *cartp = next_in_Uns32Ranges(cartridge_sets[cartset], cart, YES);
    *filep = (Int32) 1;
  }

  return(0);
}

Int32
save_cart_order(Int32 curcart, Int32 nextcart, Int32 nextfile)
{
  UChar		kbuf[32], vbuf[64], *cptr;
  Int32		i, n, r = 0;
  int		i1, i2;
  KeyValuePair	*allentries = NULL;
  Int32		num_entries = 0;

  sprintf(vbuf, "-> %d %d", (int) nextcart, (int) nextfile);
  sprintf(kbuf, "%d", (int) curcart);

  allentries = kfile_getall(cartorder_file, &num_entries, KFILE_LOCKED);
  for(i = 0; i < num_entries; i++){
    cptr = strstr(allentries[i].value, "->");

    if(cptr && cptr == allentries[i].value){
	cptr = first_nospace(cptr + 2 /* strlen("->") */);
	n = sscanf(cptr, "%d%d", &i1, &i2);
	if(n > 0){
	  if(n < 2)
	    i2 = 1;

	  if((Int32) i1 == nextcart && (Int32) i2 >= nextfile){
	    strcpy(cptr, "0");
	    if(n > 1)
	      strcat(cptr, " 0");
	    r = kfile_insert(cartorder_file, allentries[i].key,
			allentries[i].value, KFILE_SORTN | KFILE_LOCKED);
	    if(r)
		CLEANUP;
	  }
	}
    }
  }

  r = kfile_insert(cartorder_file, kbuf, vbuf, KFILE_SORTN | KFILE_LOCKED);

 cleanup:
  if(r){
    logmsg(LOG_ERR, T_("Error: Cannot write file `%s' to store the cartridge order.\n"),
		cartorder_file);
  }

  kfile_freeall(allentries, num_entries);

  return(r);
}

Int32
request_for_free_cart()
{
  FILE		*fp;
  Uns32Range	*ro_tapes;
  struct stat	statb;
  int		fd;
  KeyValuePair	*allentries = NULL;
  Int32		num_entries = 0, i;

  insfilenum[active_cartset] = 1;

  release_lock(&g_lock);

  ro_tapes = get_readonly_tapes();
  if(ro_tapes && len_Uns32Ranges(ro_tapes) < 1)
    ZFREE(ro_tapes);

  allentries = kfile_getall(precious_tapes_file, &num_entries, KFILE_LOCKED);

  fp = tmp_fp(NULL);
  if(!fp){
    logmsg(LOG_CRIT, T_("Error: Cannot start program to notify administrator: No more writable tapes"));
    do_exit(12);
  }

  fd = open(CARTREADY_FILE, O_CREAT | O_WRONLY, 0600);
  if(fd < 0){
    logmsg(LOG_ERR, T_("Error: Cannot create flag file `%s' to wait for cartridge ready"),
			CARTREADY_FILE);
    do_exit(13);
  }
  close(fd);

  fprintf(fp, T_("Hello,\n\nThere are no more free tapes available.\n"));
  fprintf(fp, T_("All tapes are either crucial for a client to restore\n"));
  fprintf(fp, T_("all needed data, or is set read-only.\n"));
  if(written_tapes){
    fprintf(fp,
	T_("\nThese are the tapes written during the current session:\n\n"));
    fprint_Uns32Ranges(fp, written_tapes, 0);
    fprintf(fp, "\n");
  }
  if(ro_tapes){
    fprintf(fp, T_("\nThese are the read-only tapes:\n\n"));
    fprint_Uns32Ranges(fp, ro_tapes, 0);
    fprintf(fp, "\n");
  }
  if(num_entries > 0){
    fprintf(fp, T_("\nThese are the clients and the tapes they need:\n\n"));
    fprintf(fp, "%30s  %20s\n\n", T_("Client identifier"), T_("Needed cartridges"));
    for(i = 0; i < num_entries; i++){
	if(word_count(allentries[i].value) > 0)
	  fprintf(fp, "%30s %20s\n", allentries[i].key, allentries[i].value);
    }

    kfile_freeall(allentries, num_entries);
  }

  fprintf(fp, T_("\nPlease do one of the following:\n\n"));
  fprintf(fp, T_("- Increase the number of cartridges, label the new\n"));
  fprintf(fp, T_("  ones using the command label_tape and modify the\n"));
  fprintf(fp, T_("  server configuration file appropriately\n"));
  fprintf(fp, T_("- Set one of the tapes from read-only state to writable\n"));
  fprintf(fp, T_("  (use the afclient command with option -M like this:\n"));
  fprintf(fp, T_("  afclient -h backuphost [ -p service ] -M \"TapesReadWrite: XXX\"\n"));
  fprintf(fp, T_("  or edit the file `%s')\n"), ro_tapes_file);
  fprintf(fp, T_("- Select one of the tapes crucial for the client and\n"));
  fprintf(fp, T_("  allow to reuse it (use the afclient command like this\n"));
  fprintf(fp, T_("  afclient -h backuphost [ -p service ] -M \"ReuseTapes: XXX\"\n"));
  fprintf(fp, T_("  or edit the file `%s'\n"), precious_tapes_file);
  fprintf(fp, T_("\nPlease use the single stream server (port) for\n"));
  fprintf(fp, T_("sending messages to the server with option -M\n"));
  fprintf(fp, T_("\nAfter all, log on to the server %s\n"), unam.nodename);
  fprintf(fp, T_("and enter the command:\n\n cartready\n"));
  fprintf(fp, T_("\nor on any client use the afbackup command like this:\n"));
  fprintf(fp, T_("afbackup -h %s [ -p <port-number> ] -M CartridgeReady\n"),
			unam.nodename);
	
  fprintf(fp, T_("\nBest regards from your backup service.\n"));

  if(message_to_operator(fp, YES)){
    logmsg(LOG_CRIT, T_("Error: Cannot start program to notify administrator: No more writable tapes"));
    do_exit(12);
  }

  while(!stat(CARTREADY_FILE, &statb)){
    ms_sleep(1000);

    if(check_interrupted())
	do_exit(1);
  }

  ZFREE(ro_tapes);

  return(serverconf(YES));
}

Uns32Range *
get_tapes_in_changer(UChar * changername)
{
  Uns32Range	*tapes_in_changer = NULL;
  UChar		*cmdstr, *line = NULL;
  int		pid, pst, fd, i1;
  FILE		*fp;

  if(!changername || !cartctlcmd)
    return(NULL);

  EM__(cmdstr = strchain(cartctlcmd, " -l -d ", changername, NULL));
  fd = open_from_pipe(cmdstr, NULL, 1, &pid);
  if(fd < 0){
    logmsg(LOG_ERR, T_("Error: Cannot determine cartridges in changer.\n"));
    CLEANUP;
  }

  fp = fdopen(fd, "r");
  while(!feof(fp)){
    ZFREE(line);
    line = fget_alloc_str(fp);
    if(!line)
	continue;

    if(sscanf(line, "%d", &i1) > 0)
	EM__(tapes_in_changer = add_to_Uns32Ranges(tapes_in_changer, i1, i1));
  }

  fclose(fp);
  waitpid_forced(pid, &pst, 0);

  if(len_Uns32Ranges(tapes_in_changer) < 1)
    ZFREE(tapes_in_changer);

 cleanup:
  ZFREE(cmdstr);
  ZFREE(line);

  return(tapes_in_changer);
}

Int32
find_next_free_pos(Int32 * cartp, Int32 * filep, Int32 cart, Int32 cartset)
{
  Int32		actc, r = 0, num_usages, file, latest_idx, actc_idx;
  Int32		i, curcartidx, search_set_len;
  Uns32Range	*nowrcarts = NULL, *readonly_tapes = NULL, *tmpcarts = NULL;
  Uns32Range	*tapes_in_changer = NULL, *cur_search_cartset = NULL;
  TapeUsage	*usages = NULL;
  Flag		found_one = NO;
  time_t	latest;

  if(prefer_cart_in_changer){
    tapes_in_changer = get_tapes_in_changer(changername);
    EM__(tmpcarts = common_Uns32Ranges(tapes_in_changer, cartridge_sets[cartset]));

    ZFREE(tapes_in_changer);
    tapes_in_changer = tmpcarts;
    if(num_Uns32Ranges(tapes_in_changer) <= 0)
	ZFREE(tapes_in_changer);
  }

  actc = cart;
  file = 1;
  curcartidx = 0;	/* this var is used, if cart is not */
			/* in the current search set */
  forever{	/* first search the tapes in the changer, if available */
    cur_search_cartset = (tapes_in_changer ? tapes_in_changer
					: cartridge_sets[cartset]);
    search_set_len = num_Uns32Ranges(cur_search_cartset);

    if(!nowrcarts){
	nowrcarts = get_all_nowrite_tapes();
	if(!nowrcarts)
	  CLEANUPR(-1);
    }

    cart = next_in_Uns32Ranges(cur_search_cartset, cart, YES);
    curcartidx++;
					/* if no free tape has been found */
    if(curcartidx == search_set_len || cart == actc){
	if(tapes_in_changer){
	  ZFREE(tapes_in_changer);
	  curcartidx = 0;
	  cart = actc;
	  continue;	/* start over searching through all the tapes */
	}

	if(full_append){
		/* We look for a tape with space left and the latest
		 * writing timestamp. Thus it should be achieved, that
		 * tapes with older data are freed to be used for backup
		 * again. Otherwise we might write small amounts of new
		 * data to tapes with lots of old stuff. These old data
		 * might become obsolete soon, but the few new data on
		 * tape prevents the tape to be used again, what is
		 * ineffective and waste of tape space
		 */
	  if(! get_tape_usages(bytesontape_file, &usages, &num_usages)
			&& (readonly_tapes = get_readonly_tapes())){
	    latest_idx = actc_idx = -1;
	    latest = (time_t) 0;

	    cart = next_in_Uns32Ranges(cur_search_cartset, actc, YES);
	    curcartidx++;
	    forever{
		for(i = 0; i < num_usages; i++)		/* find usage entry */
		  if(cart == usages[i].tape_num)
		    break;

		if(i < num_usages && !in_Uns32Ranges(readonly_tapes, cart)
				&& !usages[i].tape_full
				&& usages[i].files_on_tape > 0){
		  if(cart == actc){
		    actc_idx = i;
		  }
		  else{
		    if(latest_idx < 0 || usages[i].last_wrtime > latest)
			latest = usages[latest_idx = i].last_wrtime;
		  }
		}

		if(curcartidx == search_set_len || cart == actc)
		  break;
		cart = next_in_Uns32Ranges(cur_search_cartset, cart, YES);
		curcartidx++;
	    }

	    if(latest_idx < 0 && actc_idx >= 0){
		logmsg(LOG_NOTICE, T_("Warning: There is only remaining space on current cartridge %d. Continuing with it\n"),
				(int) actc);
		cart = usages[actc_idx].tape_num;
		file = usages[actc_idx].files_on_tape + 1;
		found_one = YES;
	    }
	    else if(latest_idx >= 0){
		cart = usages[latest_idx].tape_num;
		file = usages[latest_idx].files_on_tape + 1;
		found_one = YES;
	    }
	  }
	}

	if(!found_one){
	  r = request_for_free_cart();
	  if(r)
	    CLEANUP;

	  ZFREE(nowrcarts);
	  ZFREE(usages);
	  ZFREE(readonly_tapes);

	  curcartidx = 0;
	  cart = actc;

	  continue;
	}
    }

    if((! in_Uns32Ranges(nowrcarts, cart)
			&& ! in_Uns32Ranges(written_tapes, cart))
		|| file > 1){				/* full append mode */
#if 0	/* in the wrong place here */
	if( (i = save_cart_order(actc, cart, file)) ){
	  CLEANUPR(-2);
	}
#endif

	if(cartp)
	  *cartp = cart;
	if(filep)
	  *filep = file;
	break;
    }
  }

 cleanup:
  ZFREE(readonly_tapes);
  ZFREE(nowrcarts);
  ZFREE(usages);
  ZFREE(tapes_in_changer);

  return(r);
}

Int32
get_all_tapes(
  Uns32Range	**rfound_tapes,
  UChar		*filen,
  Uns32Range	*cartridge_set,
  Flag		tape_full,
  Flag		totally)
{
  TapeUsage	*usages;
  Int32		num_tape_uses, i, j, mode;
  Uns32Range	*found_tapes = NULL, *rptr;
  Flag		is_full, partially_full;
  Flag		is_found;	/* uninitialized OK */

  ER__(get_tape_usages(filen, &usages, &num_tape_uses), i);

  mode = (tape_full ? 1 : 0) | (totally ? 2 : 0);

  for(rptr = cartridge_set; rptr->last >= rptr->first; rptr++){
    for(i = rptr->first; i <= rptr->last; i++){
	is_full = partially_full = NO;

	for(j = 0; j < num_tape_uses; j++){
	  if(usages[j].tape_num == i){
	    if(usages[j].tape_full != NO)
		is_full = partially_full = YES;
	    if(usages[j].bytes_on_tape > 0)
		partially_full = YES;

	    break;
	  }
	}

	switch(mode){
	  case 0:	/* free, but not necessarily totally */
		is_found = !is_full;
		break;
	  case 2:	/* totally free */
		is_found = !partially_full;
		break;
	  case 1:	/* used, but not necessarily full */
		is_found = partially_full;
		break;
	  case 3:	/* totally full */
		is_found = is_full;
		break;
	}

	if(is_found)
	  found_tapes = add_to_Uns32Ranges(found_tapes, i, i);
    }
  }

  free(usages);

  if(rfound_tapes)
    *rfound_tapes = found_tapes;

  return(0);
}

Int32
can_overwrite_cart(Flag * ovw, Int32 cart)
{
  Flag		ovwr, ro, nowr;
  Int32		i;

  ovwr = YES;

  ER__(in_readonly_tapes(&ro, cart), i);
  if(ro){
    ovwr = NO;
  }
  else{
    ER__(in_nowrite_tapes(&nowr, cart), i);
    if(written_tapes)
	nowr |= in_Uns32Ranges(written_tapes, cart);

    if(nowr)
	ovwr = NO;
  }

  if(ovw)
    *ovw = ovwr;

  return(0);
}

Int32
try_current_cart_for_write(Int32 * cart_use_cur, Int32 requested_cart)
{
  Flag		ro, ovwr, tapefull = DONT_KNOW;
  Int32		i, real_cartno = -1, nfiles = -1;

  if(!var_append)
    return(0);

  if(!requested_cart)
    requested_cart = inscart[active_cartset];
  requested_cart = ABS(requested_cart);

  read_infobuffer(&real_cartno, NULL, NO);

  if(have_infobuffer
	&& in_Uns32Ranges(cartridge_sets[active_cartset], real_cartno)){
    ER__(in_readonly_tapes(&ro, real_cartno), i);

    if(!ro){
	i = get_bytes_per_tape_msg(bytesontape_file, real_cartno,
				&bytes_on_tape, &nfiles, &tapefull, NULL);
	if(!i){
	  ER__(can_overwrite_cart(&ovwr, real_cartno), i);
	  if(ovwr){
	    nfiles = 0;		/* tape can be overwritten */
	    tapefull = NO;
	    bytes_on_tape = 0.0;

	    ER__(save_bytes_per_tape_msg(bytesontape_file, real_cartno,
				bytes_on_tape, nfiles, NO, time(NULL)), i);
	  }
	  if(tapefull == NO && nfiles >= 0){
	    if(real_cartno != requested_cart)
	      logmsg(LOG_NOTICE, T_("Info: Using cartridge %d instead of the desired %d.\n"),
			(int) real_cartno, (int) requested_cart);

#ifdef	HAVE_POSIX_THREADS
	if(streamerbuffer)
	  pthread_mutex_lock(&tapepos_mutex);
#endif

	    inscart[active_cartset] = real_cartno;
	    insfilenum[active_cartset] = nfiles + 1;

#ifdef	HAVE_POSIX_THREADS
	if(streamerbuffer)
	  pthread_mutex_unlock(&tapepos_mutex);
#endif

	    if(cart_use_cur)
		*cart_use_cur = real_cartno;

	    ER__(write_tapepos_file_msg(), i);
	  }
	}
    }
  }

  return(0);
}

/* this function must not be called when several threads are active
 * furthermore the server lock must be set, when this function is called
 */
Int32
fix_write_pos(Int32 poss_wr_cart, Int32 poss_wr_file, Flag raw_access)
{
  Flag		notthiscart, ro, nowr, ovwr, writeoutpos;
  Int32		i, old_inst_cart, old_inst_file, newcart, newfile;

  writeoutpos = NO;

  old_inst_cart = inscart[active_cartset];
  old_inst_file = insfilenum[active_cartset];

  if(!raw_access)
    ER__(try_current_cart_for_write(&poss_wr_cart, old_inst_cart), i);

  ER__(in_readonly_tapes(&ro, poss_wr_cart), i);
  ER__(in_nowrite_tapes(&nowr, poss_wr_cart), i);
  if(written_tapes)
    nowr |= in_Uns32Ranges(written_tapes, poss_wr_cart);

  notthiscart = NO;
  if(ro){
    notthiscart = YES;
  }
  else{
    if(!raw_access){
	if(poss_wr_file == 1 && nowr)	/* poss_wr_file ignored if raw_access */
	  notthiscart = YES;
    }
    else{
	notthiscart = nowr;
    }
  }

  if(notthiscart){
    ER__(find_next_free_pos(&newcart, &newfile, poss_wr_cart, active_cartset), i);

    inscart[active_cartset] = newcart;
    insfilenum[active_cartset] = newfile;

    writeoutpos = YES;
  }

  if(var_append && !raw_access){
    ER__(can_overwrite_cart(&ovwr, inscart[active_cartset]), i);

    if(ovwr){
	insfilenum[active_cartset] = 1;
	writeoutpos = YES;
    }
  }

  if(writeoutpos){
    if(old_inst_cart != inscart[active_cartset]
		|| old_inst_file != insfilenum[active_cartset]){
	i = set_lock(&g_lock, BU_WRLCK);
	if(!(i & BU_GOT_LOCK_WR))
	  return(SERVICEINUSE);
    }

    ER__(write_tapepos_file_msg(), i);
  }

  return(0);
}

Int32
opentapewr_cmd(Uns32 flags)	/* command: open tape for writing */
{
  Int32		i;

  i = opentapewr(flags);

  send_status(i ? (UChar) i : COMMAND_OK);

  return(i);
}

Int32
opentapewr(Uns32 flags)
{
  Int32		i, rawaccess, cart_to_test;
  int		filemode;
  UChar		*filename;

  statusmsg(T_("Opening device %s for writing.\n"), (char *) devicename);

  rawaccess = flags & OPENED_RAW_ACCESS;

  write_meas_time = read_meas_time = 0;

  i = wait_for_service(BU_WRLCK, NO);
  if(i)
    return(i);

  if(stat(devicename, &devstatb)){
    return(OPENWR_FAILED);
  }

  if(TAPE_NOT_CLOSED)
    closetape(YES);

  tapefd = -1;
  tapeaccessmode = NOT_OPEN;

  filenum_incr_by_eof = NO;

	/* this shouldn't be necessary, cause wait_for_service does it */
  read_tapepos_file();		/* but just in case */

  cart_to_test = (rawaccess ? actcart : inscart[active_cartset]);

  ER__(host_permitted(remotehost, cart_to_test), i);

  ER__(fix_write_pos(cart_to_test, insfilenum[active_cartset], rawaccess), i);

  initmedia(inscart[active_cartset]);

  if(!rawaccess){
    i = set_cartridge(- inscart[active_cartset], YES);	/* a clone tape is */
    if(i)			/* accepted here only after repeated tries */
	return(i);

    if(insfilenum[active_cartset] == 1){
	bytes_on_tape = 0.0;

	ER__(erasetape(), i);

	ER__(rewindtape(YES, NO), i);

	ER__(write_infobuffer(inscart[active_cartset]), i);

	if(IS_REGFILE(devstatb)){
	  if( (i = update_devicesettings(insfilenum[active_cartset], YES)) )
	    return(i);
	}
    }
    else{
	ER__(set_filenum(insfilenum[active_cartset], YES, NO), i);

	if(have_infobuffer && lbl_tapeblocksize > 0)
	  ER__(change_tapeblocksize(lbl_tapeblocksize, NULL, NULL, NULL), i);
    }

    tape_rewound = NO;
    tape_moved = YES;
  }

  statusmsg(T_("Opening device %s for writing.\n"), (char *) devicename);

  newfilerequested = NO;

  if(rawaccess)
    if(tape_rewound)
	bytes_on_tape = 0.0;

  tapeptr = num_transf_bytes_valid = 0;

  filename = devicename;
  filemode = O_WRONLY | O_BINARY;

  if(IS_DIRECTORY(devstatb)){
    if( (i = set_storefile()) )
	return(i);

    filename = storefile;
    filemode = O_WRONLY | O_CREAT | O_TRUNC | O_BINARY;
  }

  tapefd = open(filename, filemode, 0600);
  if(tapefd < 0)
    return(OPENWR_FAILED);

  tapeaccessmode = OPENED_FOR_WRITE | (rawaccess ? OPENED_RAW_ACCESS : 0);

  if(BUFFEREDOP){
    if( (i = create_streamer_buffer()) )
	return(i);
  }

  statusmsg(T_("Device %s opened for writing to cartridge %d, file %d.\n"),
		(char *) devicename, (int) actcart, (int) actfilenum);

  ZFREE(written_tapes);
  add_to_written_tapes(actcart);

  return(0);
}

Int32
opentaperd_cmd(Uns32 flags)		/* command: open tape for reading */
{
  Int32		i;

  i = opentaperd(flags);

  send_status(i ? (UChar) i : COMMAND_OK);

  return(i);
}

Int32
opentaperd(Uns32 flags)
{
  Int32		i, real_cartno, scnd_cartno;
  UChar		have_infobuf, *filename, rawaccess;
  int		filemode;

  statusmsg(T_("Opening device %s for reading.\n"), (char *) devicename);

  i = wait_for_service(BU_RDLCK, NO);
  if(i)
    return(i);

  if(TAPE_NOT_CLOSED)
    closetape(YES);

  initmedia(actcart);

  rawaccess = (flags & OPENED_RAW_ACCESS) ? 1 : 0;

  if(!rawaccess){
    read_infobuffer(&real_cartno, &scnd_cartno, YES);
    have_infobuf = have_infobuffer;

    if(have_infobuf && real_cartno != actcart){
	if(scnd_cartno == actcart){
	  logmsg(LOG_NOTICE, T_("Warning: Found expected cartridge number %d as secondary label.\n"),
					actcart);
	  real_cartno = scnd_cartno;
	}
	else{
	  logmsg(LOG_NOTICE, T_("Warning: Expected cartridge %d in drive, but have %d.\n"),
					actcart, real_cartno);
	  i = actcart;
	  actcart = real_cartno;
	  ER__(set_cartridge(i, NO), i);
	}
    }

    if(have_infobuf && lbl_tapeblocksize > 0)
	ER__(change_tapeblocksize(lbl_tapeblocksize, NULL, NULL, NULL), i);
  }

  ER__(host_permitted(remotehost, actcart), i);

  statusmsg(T_("Opening device %s for reading.\n"), (char *) devicename);

  tape_moved = YES;
  tape_rewound = NO;

  filenum_incr_by_eof = NO;

  if(stat(devicename, &devstatb))
    return(OPENRD_FAILED);

  filename = devicename;
  filemode = O_RDONLY | O_BINARY;

  if(IS_DIRECTORY(devstatb)){
    if( (i = set_storefile()) )
	return(i);

    filename = storefile;
  }

  tapefd = open(filename, filemode);
  if(tapefd < 0)
    return(OPENRD_FAILED);

  tapeaccessmode = OPENED_FOR_READ |
		((flags & OPENED_RAW_ACCESS) ? OPENED_RAW_ACCESS : 0);

#ifdef	HAVE_POSIX_THREADS
	if(streamerbuffer)
	  pthread_mutex_lock(&tapepos_mutex);
#endif

  rdcart = actcart;
  rdfilenum = actfilenum;

#ifdef	HAVE_POSIX_THREADS
	if(streamerbuffer)
	  pthread_mutex_unlock(&tapepos_mutex);
#endif

  if(BUFFEREDOP){
    if( (i = create_streamer_buffer()) )
	return(i);
  }

  active_cartset = cartset_of_cart(actcart);

  statusmsg(T_("Device %s opened for reading from cartridge %d, file %d.\n"),
		(char *) devicename, (int) actcart, (int) actfilenum);

  return(0);
}

Int32
set_cartridge_cmd(Int32 cartnum)	/* command: set cartridge */
{
  Int32		r;

  r = set_cartridge(cartnum, NO);

  send_status(r ? (UChar) r : COMMAND_OK);

  return(r);
}

/* negative cartnum means: Do not accept
 * the matching secondary cartridge number */
Int32
set_cartridge(Int32 cartnum, Flag for_writing)	/* set cartridge */
{
  FILE		*fp;
  Int32		i, real_cartno, scnd_cartno, *scnd_ptr, nextcart, num_tries;
  Int32		max_seq_tries = 0, num_seq_slots = 0, load_fault = 0;
  UChar		buf[200], *cptr, *cptr2;
  Flag		reject_tape, could_read, could_unload, have_infobuf;
  Flag		tapeindrive, scnd_denied = NO;
  int		trylockfd;
  Uns32Range	*found_carts = NULL;
  ChangerDevice	*ch = NULL;

  scnd_ptr = &scnd_cartno;
  if(cartnum < 0){
    scnd_ptr = NULL;
    cartnum = - cartnum;
    scnd_denied = YES;
  }

  if(cartnum == PREVIOUS_CARTRIDGE){
    cartnum = actcart - 1;
    if(cartnum < 1){
	cartnum = num_cartridges;
    }
  }

  ER__(host_permitted(remotehost, cartnum), i);

 tryagain:	/* this saves us heavy complications, really */

  scnd_cartno = 0;
  could_read = NO;

  have_infobuf = NO;
  tape_rewound = NO;			/* force a rewind to test, */
  tapeindrive = ! rewindtape(NO, YES);	/* whether cartridge is in drive */
  if(tapeindrive){			/* If yes, try to read label */
    read_infobuffer(&real_cartno, scnd_ptr, YES);
    have_infobuf = have_infobuffer;

    if(have_infobuf && real_cartno != actcart){
	sprintf(buf, T_("Warning: Expected cartridge %d in drive, have %d instead\n"),
					(int) actcart, (int) real_cartno);
	if(scnd_cartno && scnd_cartno != real_cartno)
	  sprintf(buf + strlen(buf), T_(" (secondary: %d)"), (int) scnd_cartno);
	strcat(buf, ".\n");
	logmsg(LOG_NOTICE, buf);

	actcart = real_cartno;
    }
  }
  else{
    actcart = -2;
    tried_infobuffer = NO;
  }

  reject_tape = reject_unlabeled && for_writing && tapeindrive
		&& (!have_infobuf)
		&& (!IS_REGFILE(devstatb)) && (!IS_DIRECTORY(devstatb));
  if(reject_tape)
    actcart = -3;	/* breaking the default assumtion correct tape */

  if(actcart == cartnum){		/* assumed: have correct tape */
    tape_rewound = NO;		/* minimum action: force a rewind try */
  }
  else if(scnd_cartno && scnd_cartno == cartnum){
    logmsg(LOG_NOTICE, T_("Warning: Found expected cartridge number %d as secondary label.\n"),
					actcart);
    tape_rewound = NO;		/* minimum action: force a rewind try */
  }
  else{
    if(cartnum > num_cartridges || cartnum < 1){
      command_error(T_("no valid cartridge number"));
      return((Int32) NO_VALID_CARTRIDGE);
    }

    if(tapefd >= 0){
      command_error(T_("device is in use"));
      return((Int32) DEVINUSE);
    }

    i = wait_for_service(for_writing ? BU_WRLCK : BU_RDLCK, NO);
    if(i)
	return(i);

    statusmsg(T_("Device %s is loading cartridge %d.\n"),
				(char *) devicename, (int) cartnum);

    if(cartridge_handler && !setcartcmd){	/* in sequential mode */
      if( (i = wait_for_device(BU_RDLCK, NO)) )/* tape must be there */
	return(i);

      ch = changer_by_device(devicename, streamers, num_streamers);
      if(ch)
	if(ch->num_slots > 0)
	  num_seq_slots = max_seq_tries = ch->num_slots;

      if(num_seq_slots == 0){
	max_seq_tries = num_cartridges;

	logmsg(LOG_WARNING, T_("Warning: Number of slots not specified for device `%s' in sequential mode: Unsupported operation mode. If tape labels cannot be read, interaction of an operator (who might wonder, why the device is juggling so much) is required.\n"), devicename);
      }

      tried_infobuffer = NO;
      if( (i = rewindtape(YES, NO)) ){
	logmsg(LOG_ERR, T_("Error: Cannot wind tape back to file 1.\n"));
	return((Int32) CHANGECART_FAILED);
      }
    }

    if(cartridge_handler){
      if(setcartcmd){
	sprintf(buf, "%d", (int) cartnum);

	cptr = repl_substring(setcartcmd, "%n", buf);
	if(!cptr){
	  command_error("cannot allocate memory");
	  return((Int32) FATAL_ERROR);
	}

	sprintf(buf, "%d", (int) (cartnum - 1));
	cptr2 = repl_substring(cptr, "%m", buf);
	free(cptr);
	if(!cptr2){
	  command_error("cannot allocate memory");
	  return((Int32) FATAL_ERROR);
	}

	cptr = repl_dev_pats(cptr2, devicename, changername);
	free(cptr2);
	if(!cptr){
	  command_error("cannot allocate memory");
	  return((Int32) FATAL_ERROR);
	}

	tried_tapeblocksize = 0;
	tried_infobuffer = NO;
	have_infobuffer = NO;

	i = poll_device_cmd(cptr, YES, 0);
	free(cptr);
	if(i){
	  logmsg(LOG_ERR, T_("Warning: Setting the cartridge number %d failed.\n"),
						(int) cartnum);
	  return((Int32) CHANGECART_FAILED);
	}

	for(i = 0; i < cartins_gracetime; i++){
	  if(check_interrupted())
	    do_exit(1);

	  ms_sleep(1000 * 1);
	}
      }
      else{
	num_tries = 0;
	while(actcart != cartnum && num_tries < max_seq_tries){
	  nextcart = ((actcart) + 1 > num_cartridges ? 1 : (actcart) + 1);

	  i = change_cartridge(YES, nextcart, for_writing);
	  if(i){
	    logmsg(LOG_ERR, T_("Error: Cannot run command to change to the next cartridge.\n"));
	    return((Int32) CONFIG_ERROR);
	  }

	  actcart = nextcart;

	  num_tries++;

	  if(write_tapepos_file_msg())
	    return((Int32) CHANGECART_FAILED);

	  for(i = 0; i < cartins_gracetime; i++){
	    if(check_interrupted())
	      do_exit(1);

	    ms_sleep(1000 * 1);
	  }

	  read_infobuffer(&real_cartno, NULL, YES);
	  if(have_infobuffer){
	    if(in_Uns32Ranges(found_carts, real_cartno))
		break; 	/* we've seen this already -> all carts checked */

	    if(actcart != real_cartno)
		logmsg(LOG_WARNING,
			T_("Warning: Expected cartridge %d in drive, have %d instead\n"),
			actcart, real_cartno);
	    actcart = real_cartno;

	    EM__(found_carts = add_to_Uns32Ranges(found_carts, actcart, actcart));
	  }
	}
      }
    }
    else{
	i = open(CARTREADY_FILE, O_CREAT | O_WRONLY, 0600);
	if(i >= 0)
	  close(i);
	else
	  logmsg(LOG_ERR, T_("Error: Cannot create file `%s' to indicate"
			" waiting for tape change.\n"), CARTREADY_FILE);

	if(tapeindrive)
	  could_unload = ! change_cartridge(NO, cartnum, for_writing);
	else
	  could_unload = YES;

	tried_tapeblocksize = 0;

	if(! could_unload)
	  logmsg(LOG_ERR, T_("Warning: Cannot run command to unload the cartridge.\n"));

	fp = tmp_fp(NULL);
	if(!fp){
	  logmsg(LOG_ERR, T_("Error: Unable to ask user %s to change the cartridge.\n"),
			user_to_inform);
	  return((Int32) CHANGECART_FAILED);
	}
	else{
	  fprintf(fp, T_("Hello,\n\nYou are requested to change the cartridge "));
	  fprintf(fp, T_("in your\n backup tape streamer.\n\n"));
	  if(load_fault){
	    if(load_fault > 0){
	      fprintf(fp, T_("Your previous try failed. According to the data\n"));
	      fprintf(fp, T_("on tape you inserted cartridge number %d.\n"),
					(int) load_fault);
	    }
	    else{
	      fprintf(fp, T_("You inserted a tape without a label and the server\n"));
	      fprintf(fp, T_("Is configured to reject unlabeled tapes.\n"));
	    }
	  }
 	  fprintf(fp, T_("Please insert cartridge number %d\n"), (int) cartnum);
	  if(for_writing){
	    UChar	*empty_cart_str, *space_cart_str, *ovwr_cart_str, *ro_str;
	    Uns32Range	*empty_cart_ranges, *space_cart_ranges, *ovwr_cart_ranges;
	    Uns32Range	*readonly_tapes, *nowrite_tapes;

	    readonly_tapes = get_readonly_tapes();
	    ro_str = str_Uns32Ranges(readonly_tapes, 0);

	    nowrite_tapes = get_all_nowrite_tapes();
	    ER__(merge_Uns32Ranges(&nowrite_tapes, written_tapes), i);
	    ER__(merge_Uns32Ranges(&nowrite_tapes, readonly_tapes), i);

	    ER__(get_all_tapes(&empty_cart_ranges, bytesontape_file,
				cartridge_sets[active_cartset], NO, YES), i);
	    ER__(get_all_tapes(&space_cart_ranges, bytesontape_file,
				cartridge_sets[active_cartset], NO, NO), i);
	    ER__(get_all_tapes(&ovwr_cart_ranges, bytesontape_file,
				cartridge_sets[active_cartset], YES, NO), i);

	    ovwr_cart_ranges = del_range_from_Uns32Ranges(ovwr_cart_ranges,
							nowrite_tapes);

	    empty_cart_ranges = del_range_from_Uns32Ranges(empty_cart_ranges,
							nowrite_tapes);
	    empty_cart_str = str_Uns32Ranges(empty_cart_ranges, 0);

	    space_cart_ranges = del_range_from_Uns32Ranges(space_cart_ranges,
							empty_cart_ranges);
	    space_cart_ranges = del_range_from_Uns32Ranges(space_cart_ranges,
							ovwr_cart_ranges);
	    space_cart_ranges = del_range_from_Uns32Ranges(space_cart_ranges,
							readonly_tapes);
	    space_cart_str = str_Uns32Ranges(space_cart_ranges, 0);

	    ovwr_cart_str = str_Uns32Ranges(ovwr_cart_ranges, 0);

	    if(in_Uns32Ranges(ovwr_cart_ranges, cartnum)){
		fprintf(fp, T_("for overwriting/recycling\n"));
	    }
	    else if(in_Uns32Ranges(space_cart_ranges, cartnum)){
		fprintf(fp, T_("for writing to append to\n"));
	    }
	    else{
		fprintf(fp, T_("for writing as new unused media\n"));
	    }

	    ZFREE(ovwr_cart_ranges);
	    ZFREE(readonly_tapes);
	    ZFREE(nowrite_tapes);
	    ZFREE(empty_cart_ranges);
	    ZFREE(space_cart_ranges);

	    if(var_append){
		fprintf(fp, T_("\nAlternate cartridges of cartridge set %d are:\n"),
			(int) active_cartset + 1);
		fprintf(fp, T_("  Completely unused tapes:         %s\n"),
				empty_cart_str);
		fprintf(fp, T_("  Tapes with remaining space:      %s\n"),
				space_cart_str);
		fprintf(fp, T_("  Tapes, that may be overwritten:  %s\n\n"),
				ovwr_cart_str);
		fprintf(fp, T_("  Hint: Readonly Tapes:            %s\n\n"),
				ro_str);
	    }

	    free(ro_str);
	    free(ovwr_cart_str);
	    free(space_cart_str);
	    free(empty_cart_str);
	  }
	  else{
	    fprintf(fp, T_("for reading\n"));
	  }
 	  fprintf(fp, T_("Then enter the command:\n\n cartready\n"));
	  fprintf(fp, T_("\non host %s,\n"), unam.nodename);
	  fprintf(fp, T_("\nor on any client use the afbackup command like this:\n"));
	  fprintf(fp, T_("afbackup -h %s [ -p <port-number> ] -M CartridgeReady\n"),
			unam.nodename);
	  fprintf(fp, T_("\nBest regards from your backup service.\n"));

	  if(message_to_operator(fp, YES)){
	    logmsg(LOG_ERR, T_("Error: Unable to ask user %s to change the cartridge.\n"),
			user_to_inform);
	    return((Int32) CHANGECART_FAILED);
	  }

	  i = 0;
	  could_read = NO;
	  while(!eaccess(CARTREADY_FILE, F_OK)){
	    ms_sleep(1000 * (devprobe_interval > 0 ? devprobe_interval : 2));

	    if(check_interrupted())
		do_exit(1);

	    if(could_unload && devprobe_interval > 0){
	      trylockfd = set_wlock(CARTREADY_FILE);
	      if(trylockfd >= 0){	/* label_tape may set a lock */
		close(trylockfd);	/* -> no probing meanwhile */

		could_read = (try_read_from_tape(cartnum) > 0 ? YES : NO);

		if(IS_REGFILE(devstatb))
		  could_read = !eaccess(devicename, R_OK);
		else if(IS_DIRECTORY(devstatb))
		  could_read = !eaccess(devicename, X_OK);

		if(could_read)
		  break;
	      }
	    }
	  }

	  if(could_read){
	    i = unlink(CARTREADY_FILE);
	    if(i && errno && errno == ENOENT)
		i = 0;

	    fp = tmp_fp(NULL);
	    if(fp){
		fprintf(fp, T_("Hello, your backup system again,\n\n"));
		fprintf(fp, T_("It has been automatically detected, that\n"));
		fprintf(fp, T_("another tape is in the drive. No further\n"));
		fprintf(fp, T_("action is required.\n"));
		fprintf(fp, T_("\nBest regards from your backup service.\n"));
		message_to_operator(fp, YES);
	    }
	  }
	  if(i){
	    logmsg(LOG_ERR, T_("Error: Cannot remove file %s.\n"), CARTREADY_FILE);

	    fp = tmp_fp(NULL);
	    if(fp){
	      fprintf(fp, T_("I cannot remove the file %s.\n"), CARTREADY_FILE);
 	      fprintf(fp, T_("Please do it by hand and check owner and\n"));
 	      fprintf(fp, T_("permissions of the program cartready. It MUST\n"));
	      fprintf(fp, T_("be setuid and owned by the user, under whose\n"));
	      fprintf(fp, T_("ID the backup-daemon is running.\n"));
	      fprintf(fp, T_("(look it up in: /etc/inetd.conf)\n"));
	      fprintf(fp, T_("\nBest regards from your backup service.\n"));
	      message_to_operator(fp, YES);
	    }
	  }
	}

	if(! could_read){
	  for(i = 0; i < cartins_gracetime; i++){
	    if(check_interrupted())
	      do_exit(1);

	    ms_sleep(1000 * 1);
	  }
	}
    }

    tape_moved = YES;
  }

  if(for_writing && !(tapeaccessmode & OPENED_RAW_ACCESS))
    try_current_cart_for_write(&cartnum, cartnum);

  read_infobuffer(&real_cartno, scnd_ptr, YES);
  have_infobuf = have_infobuffer;

  if( (i = rewindtape(YES, NO)) ){
    logmsg(LOG_ERR, T_("Error: Cannot wind tape back to file 1.\n"));
    return((Int32) CHANGECART_FAILED);
  }

  reject_tape = reject_unlabeled && for_writing && (!have_infobuf)
		&& (!IS_REGFILE(devstatb)) && (!IS_DIRECTORY(devstatb));
  if(reject_tape){
    real_cartno = scnd_cartno = cartnum + 1;		/* fake wrong cart */
    have_infobuf = YES;			/* will evaluate reject_tape below */
  }

  if(have_infobuf && real_cartno != cartnum){
    if(scnd_cartno && scnd_cartno == cartnum){
	logmsg(LOG_NOTICE, T_("Warning: Found expected cartridge number %d as secondary label.\n"),
					cartnum);
    }
    else{
	if(!reject_tape){
	  logmsg(LOG_NOTICE, T_("Warning: Expected cartridge %d in drive, but have %d.\n"),
					cartnum, real_cartno);

	  actcart = real_cartno;
	}

	if(cartridge_handler && setcartcmd){
	  logmsg(LOG_ERR, T_("Error: Could not set cartridge no %d, requesting operator interaction.\n"),
				cartnum);

	  fp = tmp_fp(NULL);
	  if(!fp){
		logmsg(LOG_ERR, T_("Error: Unable to ask user %s to correct set cartridge error.\n"),
			user_to_inform);
		return((Int32) FATAL_ERROR);
	  }

	  fprintf(fp,
		T_("Hello,\n\nThe command to put cartridge number %d into the\n"),
				(int) cartnum);
	  fprintf(fp, T_("drive failed%s.\n"),
			(load_fault ? T_(" once again") : ""));
	  if(reject_tape){
		fprintf(fp, T_("The inserted tape is not labeled and the server\n"));
		fprintf(fp, T_("is configured to reject unlabeled tapes.\n"));
	  }
	  fprintf(fp, T_("Please try to correct the error\n"));
	  fprintf(fp, T_("and enter the command:\n\n cartready\n"));
	  fprintf(fp, T_("\non host %s,\n"), unam.nodename);
	  fprintf(fp, T_("\nor on any client use the afbackup command like this:\n"));
	  fprintf(fp, T_("afbackup -h %s [ -p <port-number> ] -M CartridgeReady\n"),
			unam.nodename);
	  fprintf(fp, T_("\nTo abort further retries, kill the server processes\n"));
	  fprintf(fp, T_("with -9.\n"));
	  fprintf(fp, T_("\nBest regards from your backup service.\n"));
	  if(message_to_operator(fp, YES)){
		logmsg(LOG_ERR, T_("Error: Unable to ask user %s to correct set cartridge error.\n"),
			user_to_inform);
		return((Int32) FATAL_ERROR);
	  }

	  load_fault = 1;

	  wait_for_flag_file();

	  if(reject_tape)
	    ER__(serverconf(YES), i);		/* maybe reject flag is gone */

	  goto tryagain;
	}

	if(cartridge_handler && !setcartcmd){
	  if(!in_Uns32Ranges(found_carts, real_cartno))	/* if already seen: */
	    scnd_ptr = NULL;	/* can immediately check for secondary */

	  ZFREE(found_carts);	/* old list not needed anymore */

	  for(num_tries = 0; num_tries < max_seq_tries * 2; num_tries++){
	    i = change_cartridge(YES, cartnum, for_writing);
	    if(i){
	      logmsg(LOG_ERR, T_("Error: Cannot run command to change to the next cartridge.\n"));
	      return((Int32) CONFIG_ERROR);
	    }

	    scnd_cartno = real_cartno = -1;
	    read_infobuffer(&real_cartno, scnd_ptr, YES);

	    if( (i = rewindtape(YES, NO)) ){
	      logmsg(LOG_ERR, T_("Error: Cannot wind tape back to file 1.\n"));
	      return((Int32) CHANGECART_FAILED);
	    }

	    if(real_cartno == cartnum)
		break;

	    if(scnd_cartno == cartnum && !scnd_denied && !for_writing){
		logmsg(LOG_NOTICE, T_("Warning: Found expected cartridge number %d as secondary label.\n"),
					cartnum);
		real_cartno = scnd_cartno;	/* if setting for writing, */
		break;		/* the secondary number is only accepted */
	    }			/* after having first tried all tapes */

	    if(have_infobuffer){
	      if(in_Uns32Ranges(found_carts, real_cartno)){
		load_fault = 1;
		break; 	/* we've seen this already -> all carts checked */
	      }
	    }

	    EM__(found_carts = add_to_Uns32Ranges(found_carts, real_cartno, real_cartno));

	    if(num_tries >= max_seq_tries){
		if(for_writing){
		  load_fault = 1;
		  break;
		}

		scnd_ptr = &scnd_cartno;
	    }
	  }

	  ZFREE(found_carts);

	  if(num_tries >= 2 * max_seq_tries || load_fault){
	    logmsg(LOG_ERR, T_("Error: Did not find cartridge no %d\n"), cartnum);

	    return((Int32) FATAL_ERROR);
	  }
	  else{
	    logmsg(LOG_WARNING, T_("Warning: Found correct cartridge, but administrator should do a check.\n"));
	  }
	}

	if(!cartridge_handler){
	  load_fault = (reject_tape ? -1 : real_cartno);

	  if(reject_tape)
	    ER__(serverconf(YES), i);	/* maybe the reject flag is reset */

	  goto tryagain;
	}

	tape_moved = YES;
    }
  }

  actcart = cartnum;

  if(write_tapepos_file_msg())
    return((Int32) CHANGECART_FAILED);

  if( (i = get_bytes_per_tape_msg(bytesontape_file, actcart,
					&bytes_on_tape, NULL, NULL, NULL)) )
    return(i);

  statusmsg(T_("Device %s is idle.\n"), (char *) devicename);

  return(0);
}

Int32
set_filenum_cmd(Int32 filenum)		/* command: set file */
{
  Int32	r;

  r = set_filenum(filenum, YES, NO);

  send_status(r ? (UChar) r : COMMAND_OK);

  return(r);
}

Int32
requestnewcart_cmd()
{
  Int32		i = COMMAND_OK, ncart;

  if(TAPE_NOT_CLOSED){
    logmsg(LOG_ERR, T_("Error: New cartridge requested while tape busy.\n"));
    i = DEVINUSE;
    GETOUT;
  }

  ER__(wait_for_service(BU_WRLCK, YES), i);

  if(insfilenum[active_cartset] < 2)	/* already have a fresh tape */
    GETOUT;

  i = find_next_free_pos(&ncart, NULL, inscart[active_cartset], active_cartset);
  if(i){
    logmsg(LOG_ERR, T_("Error: Cannot find a new cartridge for writing.\n"));
    i = REQNEWCART_FAILED;
    GETOUT;
  }

  tape_full_actions(inscart[active_cartset]);	/* tape use counter ... */

#ifdef	HAVE_POSIX_THREADS
	if(streamerbuffer)
	  pthread_mutex_lock(&tapepos_mutex);
#endif

  inscart[active_cartset] = ncart;
  insfilenum[active_cartset] = 1;

#ifdef	HAVE_POSIX_THREADS
	if(streamerbuffer)
	  pthread_mutex_unlock(&tapepos_mutex);
#endif

  i = write_tapepos_file_msg();

 getout:
  send_status(i);
  return(i);
}

Int32
update_devicesettings(Int32 filenum, Flag wait_success)
{
  Flag		mem1;
  Int32		r;

  mem1 = filenum_incr_by_eof;

  r = set_filenum(filenum, wait_success, NO);

  filenum_incr_by_eof = mem1;

  return(r);
}

Int32		/* set file, negative: ignore leading tape label file */
set_filenum(Int32 filenum, Flag wait_success, Flag quiet)
{
  UChar		*cptr2, *cptr;
  UChar		buf[20], have_infobuf, absolute;
  Int32		i;
  Int32		tapefilenum, real_cartno, scnd_cartno;

  absolute = (filenum >= 0 ? 0 : 1);
  filenum = (filenum >= 0 ? filenum : - filenum);

  if(filenum > 0x7fffffff || filenum < 1){
    command_error(T_("no valid file number"));
    return(NO_VALID_FILENUM);
  }

  if(tapefd >= 0){
    command_error(T_("device is in use"));
    return(DEVINUSE);
  }

  ER__(wait_for_device(BU_RDLCK, quiet), i);

  if(! absolute){
    read_infobuffer(&real_cartno, &scnd_cartno, YES);
    have_infobuf = have_infobuffer;

    if(have_infobuf && real_cartno != actcart){
      if(scnd_cartno == actcart){
	if(!quiet)
	  logmsg(LOG_NOTICE, T_("Warning: Found expected cartridge number %d as secondary label.\n"),
					actcart);
	real_cartno = scnd_cartno;
      }
      else{
	if(!quiet)
	  logmsg(LOG_NOTICE, T_("Warning: Expected cartridge %d in drive, but have %d.\n"),
					actcart, real_cartno);

#if 0
	i = actcart;
#endif
	actcart = real_cartno;
#if 0
	if(i = set_cartridge(i, NO)){
	  return(i);
	}
#endif
      }
    }
  }

  if(!quiet)
    statusmsg(T_("Device %s is moving to %s file number %d.\n"),
			(char *) devicename,
			absolute ? T_("absolute") : T_("logical"),
			(int) filenum);

  tapefilenum = filenum + ((have_infobuffer && !absolute) ? 1 : 0);

  sprintf(buf, "%d", (int)(tapefilenum - 1));

  cptr = repl_substring(setfilecmd, "%m", buf);
  if(!cptr){
    command_error("cannot allocate memory");
    return(FATAL_ERROR);
  }

  sprintf(buf, "%d", (int) tapefilenum);

  cptr2 = repl_substring(cptr, "%n", buf);
  free(cptr);
  if(!cptr2){
    command_error("cannot allocate memory");
    return(FATAL_ERROR);
  }

  cptr = repl_substring(cptr2, "%d", devicename);
  free(cptr2);
  if(!cptr){
    command_error("cannot allocate memory");
    return(FATAL_ERROR);
  }

  tape_moved = YES;
  tape_rewound = (tapefilenum <= 1 ? YES : NO);

  filenum_incr_by_eof = NO;

  i = poll_device_cmd(cptr, wait_success, MAX_TAPE_POSITIONING_TRIES);
  free(cptr);
  if(i){
    if(!quiet)
	logmsg(LOG_ERR, T_("Warning: Setting the file number %d failed.\n"), (int) tapefilenum);

    return((Int32) SETFILE_FAILED);
  }

  actfilenum = filenum;
  if(absolute && have_infobuffer)
    actfilenum = filenum - 1;

  if(IS_DIRECTORY(devstatb)){
    if( (i = set_storefile()) )
	return(i);
  }

  if(write_tapepos_file_msg()){
    return((Int32) SETFILE_FAILED);
  }

  if(!quiet)
    statusmsg(T_("Device %s is idle.\n"), (char *) devicename);

  return(0);
}

Int32
skip_files_cmd(Int32 numfiles)
{
  Int32		r;

  r = skip_files(numfiles);

  send_status(r ? (UChar) r : COMMAND_OK);

  return(r);
}

Int32		/* set file, negative: ignore leading tape label file */
skip_files(Int32 numfiles)
{
  UChar		*cptr, *cptr2;
  UChar		buf[20];
  Int32		i;

  if(numfiles > 0x7fffffff || numfiles < 1){
    command_error(T_("no valid file number"));
    return(NO_VALID_FILENUM);
  }

  if(tapefd >= 0){
    command_error(T_("device is in use"));
    return(DEVINUSE);
  }

  if( (i = wait_for_device(BU_RDLCK, NO)) )
    return(i);

  statusmsg(T_("Device %s is skipping over %d files.\n"),
			(char *) devicename, (int) numfiles);

  if(filenum_incr_by_eof)
    numfiles--;
  filenum_incr_by_eof = NO;

  sprintf(buf, "%d", (int) numfiles);

  cptr = repl_substring(skipfilescmd, "%n", buf);
  if(!cptr){
    command_error("cannot allocate memory");
    return(FATAL_ERROR);
  }

  sprintf(buf, "%d", (int) numfiles - 1);

  cptr2 = repl_substring(cptr, "%m", buf);
  free(cptr);

  if(!cptr2){
    command_error("cannot allocate memory");
    return(FATAL_ERROR);
  }

  cptr = repl_substring(cptr2, "%d", devicename);
  free(cptr2);
  if(!cptr){
    command_error("cannot allocate memory");
    return(FATAL_ERROR);
  }

  tape_moved = YES;
  tape_rewound = NO;

  i = system(cptr);
  free(cptr);
  if(i < 0 || WEXITSTATUS(i)){
    command_error(T_("command to skip over files failed"));

    return(CONFIG_ERROR);
  }

  actfilenum += numfiles;

  if(IS_DIRECTORY(devstatb)){
    if( (i = set_storefile()) )
	return(i);
  }

  if(write_tapepos_file_msg())
    return((Int32) CHANGECART_FAILED);

  statusmsg(T_("Device %s is idle.\n"), (char *) devicename);

  return(0);
}

Int32
set_cartset_cmd(Int32 cartsetno)
{
  Int32	r = COMMAND_OK;

  if(cartsetno < 1 || cartsetno > num_cartsets){
    r = NO_VALID_CARTSET;
    GETOUT;
  }

  r = host_permitted(remotehost, cartridge_sets[cartsetno - 1][0].first);
  if(r)
    GETOUT;

  active_cartset = cartsetno - 1;

 getout:
  send_status(r);

  return(r);
}

Int32
rewindtape(Flag wait_success, Flag quiet)
{
  Int32		i;

  if(tape_rewound)
    return(0);

  tape_rewound = NO;

  i = set_filenum(-1, wait_success, quiet);
  if(! i)
    tape_rewound = YES;

  return(i);
}

Int32
initmedia(Int32 expected_cart)
{
  Int32		i;
  UChar		*cptr, *cptr2, nbuf[32];

  if(media_initialized || !initmediacmd)
    return(0);

  i = wait_for_service(BU_RDLCK, NO);
  if(i)
    return(i);

  if(!(cptr = repl_dev_pats(initmediacmd, devicename, changername)))
    return(- errno);

  sprintf(nbuf, "%d", (int) expected_cart);
  cptr2 = repl_substring(cptr, "%n", nbuf);
  free(cptr);
  if(!cptr2)
    return(- errno);
  sprintf(nbuf, "%d", (int) expected_cart - 1);
  cptr = repl_substring(cptr2, "%m", nbuf);
  free(cptr2);
  if(!cptr)
    return(- errno);

  i = poll_device_cmd(cptr, NO, 0);
  free(cptr);

  media_initialized = YES;

  return(i);
}

void			/* send the buffer, that is filled from tape */
send_output_buffer(UChar * buffer, UChar statusb)
{
  if(outptr < commbufsiz)
    memset(buffer + outptr, 0, sizeof(UChar) * (commbufsiz - outptr));

  buffer[commbufsiz] = statusb;		/* last CMD is the result */

  write_forced(commwfd, buffer, commbufsiz + 1);

  num_transf_bytes_valid = outptr;

  if(!statusb || statusb == COMMAND_OK || statusb == (UChar) NO_ERROR ||
	statusb == ENDOFFILEREACHED || statusb == ENDOFTAPEREACHED)
    outptr = 0;
}

Int32	/* command: read from tape until the send buffer is full, send it */
read_from_tape()
{
  Int32		i, j;
  UChar		*outputbuffer = inputbuffer;

  if((tapeaccessmode & MODE_MASK) != OPENED_FOR_READ){
    command_error(T_("device is not open for reading"));

    send_output_buffer(outputbuffer, DEVNOTOPENRD);

    return(DEVNOTOPENRD);
  }

  forever{
    if(interrupted){
	goodbye(1);
	do_exit(1);
    }

    if(endofrec && tapeptr == 0){
	send_output_buffer(outputbuffer, ENDOFFILEREACHED);

	return(0);
    }

    if(tapeptr == 0){
	j = read_tape_buffer(&tapeptr, &i);

	if(j){
	  send_output_buffer(outputbuffer, j);

	  return(j);
	}

	if(i == 0){
	  endofrec = YES;

	  send_output_buffer(outputbuffer, ENDOFFILEREACHED);

	  return(0);
	}

	if(i < tapeblocksize){		/* This shouldn't happen, but may */
	  memmove(tapebuffer + tapeblocksize - i, tapebuffer,
						i * sizeof(UChar));
	  tapeptr = tapeblocksize - i;

	  /*memset(tapebuffer + i, 0,
			sizeof(UChar) * (tapeblocksize - i));
	  endofrec = YES;*/

	  logmsg(LOG_WARNING,
		T_("Warning: Incomplete block read from tape (%d bytes).\n"),
			(int) i);
	}
	else
	  endofrec = NO;
    }

    if(tapeblocksize - tapeptr <= commbufsiz - outptr){
	if(tapeptr < tapeblocksize)
	  memcpy(outputbuffer + outptr, tapebuffer + tapeptr,
				sizeof(UChar) * (tapeblocksize - tapeptr));
	outptr += tapeblocksize - tapeptr;
	tapeptr = 0;

	if(endofrec){
	  send_output_buffer(outputbuffer, ENDOFFILEREACHED);

	  return(0);
	}
	else if(outptr == commbufsiz){
	  send_output_buffer(outputbuffer, COMMAND_OK);

	  return(0);
	}
    }
    else{
	if(outptr > commbufsiz){
	  send_output_buffer(outputbuffer, COMMAND_OK);

	  memmove(outputbuffer, outputbuffer + commbufsiz,
			sizeof(UChar) * (outptr - commbufsiz));
	  outptr -= commbufsiz;

	  return(0);
	}

	if(outptr < commbufsiz)
	  memcpy(outputbuffer + outptr, tapebuffer + tapeptr,
				sizeof(UChar) * (commbufsiz - outptr));
	tapeptr += (commbufsiz - outptr);
	outptr = commbufsiz;

	send_output_buffer(outputbuffer, COMMAND_OK);

	return(0);
    }
  }

  return(0);	/* NOTREACHED */
}

Int32	/* command: fill the buffer to tape from sent bytes, evtl. write */
write_to_tape()
{
  Int32		i, j, num, num_written;
  UChar		*cptr;

  if((tapeaccessmode & MODE_MASK) != OPENED_FOR_WRITE){
    command_error(T_("device is not open for writing"));
    read_forced(commrfd, inputbuffer, commbufsiz);	/* satisfy protocol */
    send_status(DEVNOTOPENWR);
    return(DEVNOTOPENWR);
  }

  if(read_forced(commrfd, inputbuffer, commbufsiz) != commbufsiz)
    return(FATAL_ERROR);

  if(interrupted){
    goodbye(1);
    do_exit(1);
  }

  if(newfilerequested && streamerbuffer){
    reqnewfilebuf[streamerbuf_insert_idx] = YES;
    newfilerequested = NO;
  }

  i = num_transf_bytes_valid ? num_transf_bytes_valid : commbufsiz;
  num_transf_bytes_valid = 0;

  cptr = inputbuffer;

  forever{
    if(interrupted){
	goodbye(1);
	do_exit(1);
    }

    if(i + tapeptr <= tapeblocksize){
      if(i > 0){
	memcpy(tapebuffer + tapeptr, cptr, i * sizeof(UChar));
      }

      tapeptr += i;

      if(tapeptr < tapeblocksize){
	send_status(COMMAND_OK);
	return(0);
      }

      num = i;
    }
    else{
      num = tapeblocksize - tapeptr;

      if(num > 0){
	memcpy(tapebuffer + tapeptr, cptr, num * sizeof(UChar));
      }

      tapeptr = tapeblocksize;
    }

    j = write_tape_buffer(&tapeptr, &num_written);	/* both args are set */
    if(j){
	send_status((UChar) j);

	return(j);
    }

    cptr += num;
    i -= num;
  }

  send_status(COMMAND_OK);

  return(0);
}

Int32				/* command: erase tape */
erasetape_cmd()
{
  Int32	i;

  i = erasetape();

  send_status(i ? (UChar) i : COMMAND_OK);

  return(0);
}

Int32
erasetape()
{
  Int32		i;
  UChar		*cptr;

  statusmsg(T_("Device %s is erasing cartridge %d\n"), (char *) devicename,
			(int) actcart);

  i = wait_for_service(BU_WRLCK, NO);
  if(i)
    return(i);

  if(!erasetapecmd)
    return(0);

  initmedia(actcart);

  cptr = repl_substring(erasetapecmd, "%d", devicename);
  if(!cptr){
    logmsg(LOG_ERR, T_("Error: Cannot allocate memory.\n"));
    return(ERASETAPE_FAILED);
  }

  i = poll_device_cmd(cptr, YES, 0);

  free(cptr);

  tape_moved = YES;
  tape_rewound = NO;

  have_infobuffer = NO;
  tried_infobuffer = YES;
  tried_tapeblocksize = 0;

  filenum_incr_by_eof = NO;

  if(i){
    logmsg(LOG_ERR, T_("Error: Command to erase tape returned a bad status.\n"));
    return(ERASETAPE_FAILED);
  }

  statusmsg(T_("Device %s is idle.\n"), (char *) devicename);

  return(0);
}

Int32
sendvardata(
  Uns32		result,
  Int32		fixlen,
  UChar		*fixdata,
  Int32		varlen,
  UChar		*vardata)
{
  Int32		i, n;
  UChar		*sendbuf = NULL, buf[4];

  if(varlen < 0)
    varlen = 0;
  if(fixlen < 0)
    fixlen = 0;

  n = fixlen + 4 + varlen + 1;

  sendbuf = NEWP(UChar, n);
  if(sendbuf){
    if(fixdata && fixlen)
	memcpy(sendbuf, fixdata, fixlen);

    Uns32_to_xref(sendbuf + fixlen, varlen);

    if(vardata && varlen)
	memcpy(sendbuf + fixlen + 4, vardata, varlen);

    sendbuf[fixlen + 4 + varlen] = (UChar) result;

    i = write_forced(commwfd, sendbuf, n);

    free(sendbuf);

    return(i != n);
  }

  if(fixdata && fixlen)
    if(write_forced(commwfd, fixdata, fixlen) != fixlen)
	return(FATAL_ERROR);

  if(write_forced(commwfd, buf, 4) != 4)
    return(FATAL_ERROR);

  if(vardata && varlen)
    if(write_forced(commwfd, vardata, varlen) != varlen)
	return(FATAL_ERROR);

  buf[0] = (UChar) result;

  return(write_forced(commwfd, buf, 1) != 1);
}

Int32
sendpos(Int32 cart, Int32 file, UChar result)
{
  UChar		buf[10];

  UnsN_to_xref(buf, cart, 24);
  Uns32_to_xref(buf + 3, file);
  buf[7] = result;

  return(write_forced(commwfd, buf, 8) != 8);
}

/* For backward compatibility we fake the read position (i.e. the
 * contents of tapefilenumbuf) as current tape access position.
 * This is better than the other way round.
 * note: also in buffering mode, actcart is always == rdcart */

Int32
sendtapepos()		/* command: query tape position */
{
  Int32		c, f;

#ifdef	HAVE_POSIX_THREADS
	if(streamerbuffer)
	  pthread_mutex_lock(&tapepos_mutex);
#endif

  c = actcart;

  if((tapeaccessmode & MODE_MASK) == OPENED_FOR_READ && tapefilenumbuf &&
			streamerbuf_insert_idx != streamerbuf_processed_idx)
    f = tapefilenumbuf[streamerbuf_processed_idx];
  else
    f = (actfilenum < 1 ? 1 : actfilenum);

#ifdef	HAVE_POSIX_THREADS
	if(streamerbuffer)
	  pthread_mutex_unlock(&tapepos_mutex);
#endif

  return(sendpos(c, f, COMMAND_OK));
}

Int32
setnumwritevalid()		/* command: query number of valid bytes sent */
{
  UChar		buf[5];
  Int32		i;

  i = (read_forced(commrfd, buf, 4) != 4);

  if(!i)
    xref_to_Uns32(&num_transf_bytes_valid, buf);

  send_status(i ? (UChar) (- errno) : COMMAND_OK);

  return(i);
}

Int32
sendtapewrpos()		/* command: query tape writing position */
{
  UChar		result;
  Int32		lck, i, j;

  result = COMMAND_OK;

  i = (tapeaccessmode & MODE_MASK);
  if(i == NOT_OPEN || i == CLOSED_NOTREW){
    lck = set_lock(&g_lock, BU_WRLCK);
    result = ((lck & BU_LOCKED_WR) ? DEVINUSE : COMMAND_OK);

    if(!result)
	ER__(fix_write_pos(inscart[active_cartset], insfilenum[active_cartset], NO), i);
  }

#ifdef	HAVE_POSIX_THREADS
	if(streamerbuffer)
	  pthread_mutex_lock(&tapepos_mutex);
#endif

  i = inscart[active_cartset];
  j = insfilenum[active_cartset];

#ifdef	HAVE_POSIX_THREADS
	if(streamerbuffer)
	  pthread_mutex_unlock(&tapepos_mutex);
#endif

  if(i < 1)
    i = 1;
  return(sendpos(i, j, result));
}

Int32
sendtaperdpos()		/* command: query tape reading position */
{
  Int32		c, f;

#ifdef	HAVE_POSIX_THREADS
	if(streamerbuffer)
	  pthread_mutex_lock(&tapepos_mutex);
#endif

  c = rdcart;
  if((tapeaccessmode & MODE_MASK) == OPENED_FOR_READ && tapefilenumbuf &&
			streamerbuf_insert_idx != streamerbuf_processed_idx)
    f = tapefilenumbuf[streamerbuf_processed_idx];
  else
    f = (rdfilenum < 1 ? 1 : rdfilenum);

#ifdef	HAVE_POSIX_THREADS
	if(streamerbuffer)
	  pthread_mutex_unlock(&tapepos_mutex);
#endif

  return(sendpos(c, f, COMMAND_OK));
}

Int32
sendint(Int32 i, Int32 size)
{
  UChar		buf[9];

  UnsN_to_xref(buf, i, size << 3);
  buf[size] = COMMAND_OK;

  size++;
  return(write_forced(commwfd, buf, size) != size);
}

Int32
sendifbusy()
{
  UChar		busy[512];
  Int32		r = 0;
  int		fd;

  memset(busy, 0, sizeof(UChar) * 512);

  if(tapeaccessmode != NOT_OPEN){
    busy[0] = STREAMER_BUSY;
  }
  else{
    if(set_lock(&g_lock, BU_WRLCK) & BU_GOT_LOCK){
      fd = open(devicename, O_RDONLY | NONBLOCKING_FLAGS);
      if(fd < 0){
	if(errno == EBUSY)
	  busy[0] = STREAMER_DEVINUSE;
	else
	  busy[0] = STREAMER_UNAVAIL;
      }
      else{
	close(fd);
	if(rewindtape(NO, YES)){
	  busy[0] = (cartridge_handler && setcartcmd) ?
				(STREAMER_CHANGEABLE | STREAMER_UNLOADED)
				: STREAMER_UNLOADED;
	}
	else{
	  busy[0] = STREAMER_READY;

	  if(cartridge_handler)
	    busy[0] |= STREAMER_CHANGEABLE;
	}
      }
    }
    else
	busy[0] = STREAMER_BUSY;

    release_lock(&g_lock);
  }

  busy[13] |= (var_append ? SERVERMODE_VARAPPEND : 0);

  r = write_forced(commwfd, busy, 512);

  send_status(r != 512 ? - errno : COMMAND_OK);

  return(COMMAND_OK);
}

Int32
sendwrtapes(Uns32Range * wtapes)
{
  UChar		buf[5], *wtapestr = NULL;
  Int32		r, l;

  if(wtapes)
    wtapestr = str_Uns32Ranges(wtapes, 0);

  if(!wtapestr){
    memset(buf, 0, 5 * sizeof(UChar));
    return(write_forced(commwfd, buf, 5) != 5);
  }

  l = strlen(wtapestr);
  wtapestr[l] = COMMAND_OK;

  Uns32_to_xref(buf, l);

  l++;
  ER__(write_forced(commwfd, buf, 4) != 4, r);
  ER__(write_forced(commwfd, wtapestr, l) != l, r);

  free(wtapestr);

  return(0);
}

Int32
sendclientstapes()
{
  Int32		l, r = 0;
  UChar		buf[256];
  UChar		*cptr, *res;

  ER__(read_forced(commrfd, buf, 1) != 1, r);
  l = (Int32) buf[0];
  if(l > 128)
    return(EINVAL);

  ER__(read_forced(commrfd, buf, l) != l, r);
  buf[l] = 0;

  cptr = kfile_get(precious_tapes_file, buf, KFILE_LOCKED);

  strcpy(buf, "-");

  if(cptr)
    res = (strlen(cptr) > 0 ? cptr : buf);
  else
    res = buf;

  r = sendvardata(COMMAND_OK, 0, NULL, strlen(res), res);

  ZFREE(cptr);

  return(r);
}

Int32
set_commbufsiz(Int32 newsize)
{
  if(newsize > MAXCOMMBUFSIZ || newsize < 0)
    return(NO_VALID_COMMBUFSIZ);

  commbufsiz = newsize;

  newsize = MAX(newsize, outptr) + 8;
  EM__(inputbuffer = ZRENEWP(inputbuffer, UChar, newsize));

  return(0);
}

/* The older versions for backward compatibility */
Int32
osendpos(Int32 cart, Int32 file, UChar result)
{
  UChar		buf[8];

  buf[0] = cart;
  UnsN_to_xref(buf + 1, file, 24);
  buf[4] = result;

  write_forced(commwfd, buf, 5);

  return(0);
}

Int32
osendtapepos()		/* command: query tape position */
{
  Int32		c, f;

#ifdef	HAVE_POSIX_THREADS
	if(streamerbuffer)
	  pthread_mutex_lock(&tapepos_mutex);
#endif

  c = actcart;
  if((tapeaccessmode & MODE_MASK) == OPENED_FOR_READ && tapefilenumbuf &&
			streamerbuf_insert_idx != streamerbuf_processed_idx)
    f = tapefilenumbuf[streamerbuf_processed_idx];
  else
    f = (actfilenum < 1 ? 1 : actfilenum);

#ifdef	HAVE_POSIX_THREADS
	if(streamerbuffer)
	  pthread_mutex_unlock(&tapepos_mutex);
#endif

  return(osendpos(c, f, COMMAND_OK));
}

Int32
osendtapewrpos()		/* command: query tape writing position */
{
  UChar		result;
  Int32		lck, i, j;

  result = COMMAND_OK;

  i = (tapeaccessmode & MODE_MASK);
  if(i == NOT_OPEN || i == CLOSED_NOTREW){
    lck = set_lock(&g_lock, BU_WRLCK);
    result = ((lck & BU_LOCKED_WR) ? DEVINUSE : COMMAND_OK);

    if(!result)
	ER__(fix_write_pos(inscart[active_cartset], insfilenum[active_cartset], NO), i);
  }

#ifdef	HAVE_POSIX_THREADS
	if(streamerbuffer)
	  pthread_mutex_lock(&tapepos_mutex);
#endif

  i = inscart[active_cartset];
  j = insfilenum[active_cartset];

#ifdef	HAVE_POSIX_THREADS
	if(streamerbuffer)
	  pthread_mutex_unlock(&tapepos_mutex);
#endif

  if(i < 1)
    i = 1;
  return(osendpos(i, j, result));
}

Int32
goodbye(Int32 exitst)		/* command: goodbye */
{
  Int32		i;
  UChar		*cptr;

  tried_infobuffer = YES;
  have_infobuffer = NO;
  tried_tapeblocksize = 0;

  closetape(YES);

  if(exit_cmd && ! exitst){
    cptr = repl_substring(exit_cmd, "%p", remotehost);
    if(!cptr){
	logmsg(LOG_WARNING, T_("Warning: Could not replace %p in exit command, leaving it unmodified.\n"));
    }
    else{
	free(exit_cmd);
	exit_cmd = cptr;
    }

    i = system(exit_cmd);
    if(i < 0 || WEXITSTATUS(i)){
      logmsg(LOG_WARNING, T_("Warning: Executing exit command returned %d.\n"), i);
    }
  }

  return(0);
}

Int32
goodbye_cmd()
{
  Int32		i;

  i = goodbye(0);

  send_status(i ? (UChar) i : COMMAND_OK);

  do_exit(0);

  /* NOTREACHED */
}

Int32
client_backup_cmd(Int8 old_version)
{
  UChar		r, c, *buf = NULL, *cptr, *eocmd, echr, suppressoutput;
  Int32		i, n;
  int		fd, pid, pst;
  UChar		*output = NULL, lenstr[5];
  Uns32		output_len, new_output_len;
  Flag		subproc_terminated;

  memset(lenstr, 0, 5 * sizeof(UChar));

  r = COMMAND_OK;

  i = (read_forced(commrfd, &c, 1) != 1);
  if(i){
    r = PROTOCOL_ERROR;
    goto retlabel;
  }

  n = (Int32) c;
  if(!old_version){
    i = (read_forced(commrfd, &suppressoutput, 1) != 1);
    if(i){
	r = PROTOCOL_ERROR;
	goto retlabel;
    }
  }

  buf = NEWP(UChar, n + 1);
  if(!buf)
    return(FATAL_ERROR);

  i = (read_forced(commrfd, buf, n) != n);
  if(i){
    r = PROTOCOL_ERROR;
    goto retlabel;
  }
  buf[n] = '\0';

  for(eocmd = buf; !isspace(*eocmd) && *eocmd; eocmd++);
  echr = *eocmd;
  *eocmd = '\0';

  if(! FN_ISPATH(buf)){
    *eocmd = echr;
    EM__(cptr = strchain(programdir, FN_DIRSEPSTR, buf, NULL));
    free(buf);
    buf = cptr;
    for(eocmd = buf; !isspace(*eocmd) && *eocmd; eocmd++);
    echr = *eocmd;
    *eocmd = '\0';
  }
  cptr = FN_LASTDIRDELIM(buf);
  *cptr = '\0';

  if(strncmp(programdir, buf, strlen(programdir))){
    r = AUTHENTICATION;
    goto retlabel;
  }
  *cptr = FN_DIRSEPCHR;

  if(eaccess(buf, X_OK)){
    r = SUBPROCESS_FAILED;
    goto retlabel;
  }

  *eocmd = echr;

  if(old_version){
    i = system(buf);
    if(i < 0 || WEXITSTATUS(i))
	r = SUBPROCESS_FAILED;
  }
  else{
    output_len = 0;
    fd = open_from_pipe(buf, NULL, 1 + 2, &pid);
    if(fd >= 0){
	pst = -1;
	new_output_len = -1;
	subproc_terminated = NO;

	forever{
	  if(output_len != new_output_len){
	    new_output_len = output_len;
	    output = ZRENEWP(output, UChar, new_output_len + 1000);
	    if(!output){
		r = FATAL_ERROR;
		output_len = 0;
		break;
	    }
	  }

	  i = wait_for_input(fd, 1);
	  if(i < 1){
	    if(subproc_terminated)
		break;

	    if(waitpid_forced(pid, &pst, WNOHANG) == pid)
		subproc_terminated = YES;

	    continue;
	  }

	  n = read(fd, output + output_len, 1000);
	  if(n < 1)
	    break;

	  output_len += n;
	}

	if(!subproc_terminated)
	  waitpid_forced(pid, &pst, 0);
	close(fd);

	lenstr[0] = WEXITSTATUS(pst);
    }
    else{
	if( (output = strapp(T_("Error: Could not start subprocess "), buf)) )
	  output_len = strlen(output);

	lenstr[0] = 127;
    }

    if(suppressoutput)
	output_len = 0;

    Uns32_to_xref(lenstr + 1, output_len);

    write_forced(commwfd, lenstr, 5);

    if(output){
	write_forced(commwfd, output, output_len);

	free(output);
    }
  }

 retlabel:
  ZFREE(buf);

  if(!old_version && r != COMMAND_OK)
    write_forced(commwfd, lenstr, 5);	/* to fulfil protocol in error case */

  send_status(r);

  return((Int32) r);
}

Int32
send_clientmsg()
{
  time_t	end_lock_try_time;
  Flag		expired = NO, need_to_send;
  int		msgfd, i;
  Int32		r = 0, tlen;
  UChar		*msgbuf, tmpbuf[1024], *mptr;
  struct stat	statb;

  msgbuf = tmpbuf;
  mptr = msgbuf + 4;
  time_t_to_intstr(time(NULL), mptr);
  strcat(mptr, "\n");
  tlen = strlen(mptr);
  memcpy(mptr + tlen, mptr, tlen * sizeof(UChar));
  tlen += tlen;
  mptr[tlen] = '\0';  
  need_to_send = YES;

  if(!eaccess(client_message_file, F_OK)){
    end_lock_try_time = time(NULL) + 60;
    forever{
      if(time(NULL) > end_lock_try_time){
	expired = YES;
	break;
      }

      msgfd = set_rlock_forced(client_message_file);
      if(msgfd >= 0)
	break;
    }

    if(expired){
	strcat(mptr,
		T_("Server side error: Cannot get lock on message file.\n"));
	r = CANNOT_LOCK;
    }
    else{
	msgbuf = NULL;
	i = fstat(msgfd, &statb);
	if(!i)
	  msgbuf = NEWP(UChar, 4 + (tlen + 10) + statb.st_size + 1);
	if(i || !msgbuf || eaccess(client_message_file, R_OK)){
	  ZFREE(msgbuf);
	  msgbuf = tmpbuf;
	  strcat(mptr, T_("Server side error: Cannot read message file.\n"));
	  r = - errno;
	}
	else{
	  mptr = msgbuf + 4;
	  time_t_to_intstr(statb.st_mtime, mptr);
	  strcat(mptr, "\n");
	  time_t_to_intstr(time(NULL), mptr + strlen(mptr));
	  strcat(mptr, "\n");
	  tlen = strlen(mptr);

	  i = read_forced(msgfd, mptr + tlen, statb.st_size);
	  if(i < 0)
	    i = 0;

	  Uns32_to_xref(msgbuf, tlen + i);
	  mptr[tlen + i] = COMMAND_OK;

	  write_forced(commwfd, msgbuf, 4 + tlen + i + 1);

	  free(msgbuf);

	  need_to_send = NO;
	}

	close(msgfd);
    }
  }

  if(need_to_send){
    i = strlen(msgbuf + 4);
    Uns32_to_xref(msgbuf, i);
    write_forced(commwfd, msgbuf, 4 + i);
  }

  return(r);
}

void
sig_handler(int sig)
{
  signal(sig, sig_handler);

  switch(sig){
    case SIGPIPE:
	logmsg(LOG_ERR, T_("Error: Connection to client lost. Exiting.\n"));
	if(pgid_to_kill_on_interrupt > 0)
	  kill(- pgid_to_kill_on_interrupt, SIGKILL);
	do_exit(1);
    case SIGTERM:
	logmsg(LOG_ERR, T_("Got signal TERM. Exiting.\n"));
	if(pgid_to_kill_on_interrupt > 0)
	  kill(- pgid_to_kill_on_interrupt, SIGKILL);
	interrupted = YES;
	return;
    case SIGINT:
	logmsg(LOG_ERR, T_("Got signal INTR. Exiting.\n"));
	if(pgid_to_kill_on_interrupt > 0)
	  kill(- pgid_to_kill_on_interrupt, SIGKILL);
	interrupted = YES;
	return;
    case SIGHUP:
	logmsg(LOG_ERR, T_("Got signal HUP. Exiting.\n"));
	if(pgid_to_kill_on_interrupt > 0)
	  kill(- pgid_to_kill_on_interrupt, SIGKILL);
	interrupted = YES;
	return;
    case SIGUSR1:
	edebug = 0;
	break;
  }

  if(exiting_on_cerr){
    logmsg(LOG_ERR, T_("Exiting on fatal error, no further cleanup will occur.\n"));
    exit(21);
  }

  switch(sig){
    case SIGSEGV:
	logmsg(LOG_CRIT, T_("Error: Segmentation fault. Exiting.\n"));
	exiting_on_cerr = YES;
	do_exit(1);
    case SIGBUS:
	logmsg(LOG_CRIT, T_("Error: Bus error. Exiting.\n"));
	exiting_on_cerr = YES;
	do_exit(1);
  }
}

Flag
check_interrupted()
{
  int		i;
  nodeaddr	peeraddr;
  Int32		len[2];

  if(interrupted){
    return(YES);
  }

  if(!slavemode){
    len[0] = len[1] = 0;
		/* I use 32 Bit int for len. See below for a *comment* */
    *((int *) &(len[0])) = sizeof(peeraddr);
    i = getpeername(commwfd, (struct sockaddr *)(&peeraddr), (int *) &(len[0]));
    if(i && errno == ENOTCONN){
	logmsg(LOG_ERR, T_("Error: Client disconnected. Exiting.\n"));
	kill(getpid(), SIGPIPE);
    }
  }

  return(interrupted);
}

#ifdef	PERFDEBUG
void
showbuffilltty9()
{
int	buffill, i;
char	showbuf[100];
FILE	*fp;

fp = fopen("/dev/tty9", "w");
if(!fp) return;
if(streamerbuf_insert_idx >= streamerbuf_processed_idx)
  buffill = streamerbuf_insert_idx - streamerbuf_processed_idx;
else
  buffill = streamerbuf_insert_idx + streamerbufferlen - streamerbuf_processed_idx;
buffill = 70 * buffill / streamerbufferlen;
for(i = 0; i < buffill; i++) showbuf[i] = '#';
for(; i < 70; i++) showbuf[i] = '_';
strcpy(showbuf + i, "\r");
fprintf(fp, showbuf);
fclose(fp);
}
#endif

Int32
process_tape_ops(Flag have_to_read, Flag have_to_write)
{
    Int32	i, j, total_bytes;

    if((tapeaccessmode & MODE_MASK) == NOT_OPEN){
	logmsg(LOG_ERR, T_("Internal Error: Tape not open, but unflushed data exists in ring buffer.\n"));
	if(exiting)
	  return(FATAL_ERROR);

	logmsg(LOG_CRIT, "Internal Error: Inconsistency. Inform author. Must exit, sorry.\n");

	exit(99);
    }

#ifdef	PERFDEBUG
showbuffilltty9();
#endif

    total_bytes = 0;

    if(have_to_write && (tapeaccessmode & MODE_MASK) == OPENED_FOR_WRITE){
	i = flush_buffer_to_tape(
		streamerbuffer + tapeblocksize * streamerbuf_processed_idx,
			&buff_tape_ptr, &j);

	if(i){
	  logmsg(LOG_ERR, T_("Error: Writing buffer to tape failed in select routine.\n"));
	  logmsg(LOG_ERR, T_("       Return code is %d, written: %d.\n"), (int) i, (int) j);
	}

#ifdef	HAVE_POSIX_THREADS
	pthread_mutex_lock(&may_modify_buffer_ptrs);
#endif

	streamerbuf_processed_idx++;
	if(streamerbuf_processed_idx >= streamerbufferlen)
	  streamerbuf_processed_idx = 0;

#ifdef	HAVE_POSIX_THREADS
	pthread_cond_signal(&can_write_to_buffer);
	pthread_mutex_unlock(&may_modify_buffer_ptrs);
#endif

	total_bytes += j;
    }

    if(have_to_read && (tapeaccessmode & MODE_MASK) == OPENED_FOR_READ){
	DB("process_tape_ops: reading !\n", 0,0,0);

	i = get_tape_buffer(
		streamerbuffer + tapeblocksize * streamerbuf_insert_idx,
			&buff_tape_ptr, &j);

	total_bytes += j;

	tapefilenumbuf[streamerbuf_insert_idx] = rdfilenum;
	numbytesread[streamerbuf_insert_idx] = j;

	if((i == ENDOFFILEREACHED || i == ENDOFTAPEREACHED)
				&& (tapeaccessmode & OPENED_RAW_ACCESS)){
	  DB("process_tape_ops: reading raw, eof encountered\n", 0,0,0);

	  return(total_bytes);		/* now also at_mediaend is set */
	}

	if(i){
	  logmsg(LOG_ERR, T_("Error: Reading buffer from tape failed in interrupt routine.\n"));
	  logmsg(LOG_ERR, T_("       Return code is %d, read: %d.\n"), (int) i, (int) j);
	}

	if(at_mediaend && !force_cartchange){	/* now at end of media */
	  DB("process_tape_ops: reading, at_mediaend && !force_cartchange\n", 0,0,0);

	  return(total_bytes);		/* do nothing until forced-flag set */
	}

#ifdef	HAVE_POSIX_THREADS
	pthread_mutex_lock(&may_modify_buffer_ptrs);
#endif

	streamerbuf_insert_idx++;	/* at last incr. index */
	if(streamerbuf_insert_idx >= streamerbufferlen)
	  streamerbuf_insert_idx = 0;

#ifdef	HAVE_POSIX_THREADS
	pthread_cond_signal(&buffer_contains_data);
	pthread_mutex_unlock(&may_modify_buffer_ptrs);
#endif

    }

    return(total_bytes);
}

Int32
process_pending_tape_ops()
{
  Int32		i;

  while(streamerbuf_insert_idx != streamerbuf_processed_idx){
    i = process_tape_ops((tapeaccessmode & MODE_MASK) == OPENED_FOR_READ,
		(tapeaccessmode & MODE_MASK) == OPENED_FOR_WRITE);
  }

  return(0);
}

Int32
get_input(UChar * retbuf)
{
#ifndef	HAVE_POSIX_THREADS	/* only read command and return, if threaded */
  struct pollfd	fds[2];
  Int32		next_idx, i, e, total_bytes, buffill;
  Flag		have_to_read, have_to_write;
  Flag		did_need_read;	/* uninitialized OK */
  Flag		got_command;
  Real32	rate;

  if(!streamerbuffer)
#endif	/* !defined	HAVE_POSIX_THREADS */

    return(read_forced(commrfd, retbuf, 1) != 1 ? 0 : 1);

#ifndef	HAVE_POSIX_THREADS
#ifdef	PERFDEBUG
showbuffilltty9();
#endif

  got_command = NO;
  total_bytes = 0;


  fds[0].fd = commrfd;
  fds[0].events = POLLIN;

  forever{
    if(got_command && !did_need_read)
	return((Int32) got_command);

    next_idx = streamerbuf_insert_idx + 1;
    if(next_idx >= streamerbufferlen)
	next_idx = 0;

    fds[1].fd = -1;
    fds[1].events = 0;

    have_to_write = NO;
    if((tapeaccessmode & MODE_MASK) == OPENED_FOR_WRITE){
	if(num_blocks_in_streamerbuf >= streamerbuf_highmark){
	  streamerbuf_writing = YES;
	}

	have_to_write = YES;

	if(next_idx == streamerbuf_processed_idx){
	  fds[0].events &= (~ POLLIN);	/* don't accept commands, if writebuf full */
	}
	else if(!streamerbuf_writing){
	  have_to_write = NO;		/* nothing to be written */
	}

	if(have_to_write){
	  fds[1].events |= POLLOUT;
	  fds[1].fd = tapefd;
	}
    }

    have_to_read = NO;
    if((tapeaccessmode & MODE_MASK) == OPENED_FOR_READ){
	have_to_read = YES;

	if((at_mediaend && !force_cartchange)	/* at end of media */
			|| total_bytes > (commbufsiz << 1)){
	  have_to_read = NO;	/* don't read tape, if buffer full */
	}

	i = num_blocks_in_streamerbuf;

	if(i <= streamerbufferlen - streamerbuf_highmark)
	  streamerbuf_reading = YES;
		/* need streamerbufferlen - 1 here, cause otherwise we cannot */
		/* distinguish between the cases buffer full and buffer empty */
	if(i >= streamerbufferlen - 1 - streamerbuf_lowmark){
	  streamerbuf_reading = NO;

#ifdef	AFB_SERVER_SHOW_BUFFER_STATUS
	  statusmsg(T_("Device %s is waiting for space in the read buffer.\n"), (char *) devicename);
#endif
	}

	if(!streamerbuf_reading)
	  have_to_read = NO;

	if(have_to_read){
	  fds[1].events |= POLLIN;
	  fds[1].fd = tapefd;
	}
    }

    if(got_command && !have_to_read && !have_to_write)
	return((Int32) got_command);

    DB("get_input: going to poll ...\n",0,0,0);

    forever{
	i = poll(fds, 2, -1);
	e = errno;

	if(i < 0){
	  if(e == EINTR)
	    continue;

	  if(POLLINEVENT(fds[1].revents) || POLLOUTEVENT(fds[1].revents))
	    break;

	  return(0);
	}

	if(i)
	  break;
    }

    if(!got_command && POLLINEVENT(fds[0].revents)){	/* input from client ? */
	i = (read_forced(commrfd, retbuf, 1) != 1);	/* get command */
	if(i)
	  return(0);

	got_command = YES;			/* set flag for return */

	DB("get_input: got command ...\n",0,0,0);
    }

#if 0	/* use watermarks now */
    if(got_command && have_to_write){	/* prefer command input ? */
	rate = (Real32) num_blocks_in_streamerbuf / (Real32) streamerbufferlen;
	rate = rate * rate * 21 / 2 - rate / 2;

	if(rate < 1.0 || (Real32) commbufsiz / ((Real32) total_bytes + 0.1) < rate){
	  have_to_write = NO;		/* nothing to be written */

	  DB("get_input: preferring command, wr: %d ...\n",POLLOUTEVENT(fds[1].revents),0,0);
	}
    }
#endif

    did_need_read = NO;

    if(have_to_write && ! POLLOUTEVENT(fds[1].revents))
	have_to_write = NO;
    if(have_to_read && ! POLLINEVENT(fds[0].revents))
	have_to_read = NO;

    if(have_to_read || have_to_write){
	i = process_tape_ops(have_to_read, have_to_write);
	if(i > 0){
	  total_bytes += i;
	  did_need_read = have_to_read;
	}

	if(have_to_write && num_blocks_in_streamerbuf <= streamerbuf_lowmark){
	  streamerbuf_writing = NO;
#ifdef	AFB_SERVER_SHOW_BUFFER_STATUS
	  statusmsg(T_("Device %s is waiting for the write buffer to be filled.\n"), (char *) devicename);
#endif
	}
    }
  }
#endif	/* !defined	HAVE_POSIX_THREADS */
}

Int32
create_streamer_buffer()
{
  Int32		i;
  Uns32		bufsize;
  struct stat	statb;
  Flag		open_file;

  SETZERO(null_timeval);

  dismiss_streamer_buffer();

  if(streamerbuf_conf_size > 0){
    streamerbufferlen = streamerbuf_conf_size / tapeblocksize;
  }
  else{
#ifndef	_WIN32
 {
  struct rlimit		rlim;

  i = getrlimit(RLIMIT_STACK, &rlim);
  if(i){
    logmsg(LOG_WARNING, T_("Error: cannot get maximum available memory\n"));
    streamerbufferlen = 1000000 / tapeblocksize / 3;
  }
  else{
    streamerbufferlen = rlim.rlim_cur / tapeblocksize / 3;
  }
 }
#else
  streamerbufferlen = 1000000 / tapeblocksize / 3;
#endif

    if(streamerbufferlen < 2){
	streamerbufferlen = 0;

	logmsg(LOG_NOTICE, T_("Warning: Low on memory. No tape buffer created.\n"));
	return(0);
    }

    if(streamerbufferlen * tapeblocksize > MAX_BUFFER_MEMORY)
	streamerbufferlen = MAX_BUFFER_MEMORY / tapeblocksize;
    if(streamerbufferlen * tapeblocksize > maxbytes_per_file)
	streamerbufferlen = maxbytes_per_file / tapeblocksize;

  }

  if(streamerbufferlen < 2){	/* buffering makes no sense */
    streamerbufferlen = 0;

    return(0);
  }

  bufsize = streamerbufferlen * tapeblocksize;

  streamerbuf_memfilefd = -1;

#if	defined(HAVE_MMAP) && defined(PROT_READ) && defined(PROT_WRITE) && defined(MAP_PRIVATE)
#define	SERVER_USING_MMAP_FILE_BUFFER	1

  if(streamerbuf_memfile){
    open_file = YES;

    if(!stat(streamerbuf_memfile, &statb)){
	if(IS_REGFILE(statb)){
	  if(chmod(streamerbuf_memfile, 0600) || eaccess(streamerbuf_memfile, W_OK)){
	    unlink(streamerbuf_memfile);
	    if(!eaccess(streamerbuf_memfile, F_OK)){
		logmsg(LOG_WARNING, T_("Warning: Buffer file `%s' exists, has wrong permissions or is not writable and cannot be removed. Not using buffer file.\n"),
				streamerbuf_memfile);

		open_file = NO;
	    }
	  }
	}
	else{
	  unlink(streamerbuf_memfile);
	  rmdir(streamerbuf_memfile);
	  if(!eaccess(streamerbuf_memfile, F_OK)){
	    logmsg(LOG_WARNING, T_("Warning: Filesystem entry '%s' exists, is not a file and cannot be unlinked. Not using buffer file.\n"),
				streamerbuf_memfile);
	    open_file = NO;
	  }
	}
    }

    if(open_file){
	mkbasedir(streamerbuf_memfile, 0700, -1, -1);

	streamerbuf_memfilefd = open(streamerbuf_memfile, O_RDWR | O_CREAT, 0600);
	if(streamerbuf_memfilefd >= 0){
	  if(ftruncate(streamerbuf_memfilefd, bufsize)){
	    logmsg(LOG_WARNING, T_("Warning: Cannot set file '%s' to size %lf. Not using buffer file.\n"),
			streamerbuf_memfile, (long int) bufsize);
	    close(streamerbuf_memfilefd);
	    streamerbuf_memfilefd = -1;
	  }
	  else{
	    streamerbuffer = mmap(NULL, bufsize,
			PROT_READ | PROT_WRITE, MAP_SHARED,
			streamerbuf_memfilefd, 0);
	    if(!streamerbuffer){
		logmsg(LOG_WARNING, T_("Warning: Cannot map file '%s'. Not using buffer file.\n"),
				streamerbuf_memfile);
		close(streamerbuf_memfilefd);
		streamerbuf_memfilefd = -1;
		unlink(streamerbuf_memfile);
	    }
#if	defined(HAVE_MADVISE) && defined(MADV_SEQUENTIAL)
	    else {
		if (madvise(streamerbuffer, bufsize, MADV_SEQUENTIAL) == -1)
		    logmsg(LOG_WARNING, T_("Warning: Cannot set sequential advice for map file '%s'.\n"), streamerbuf_memfile);
	    }
#endif
	  }
	}
    }
  }

#endif

  if(!streamerbuffer){
    streamerbuffer = NEWP(UChar, bufsize);
  }

  tapefilenumbuf = NEWP(Uns32, streamerbufferlen);
  numbytesread = NEWP(Uns32, streamerbufferlen);
  reqnewfilebuf = NEWP(Flag, streamerbufferlen);

  if(!streamerbuffer || !tapefilenumbuf || !reqnewfilebuf || !numbytesread){
    streamerbufferlen = 0;

    ZFREE(streamerbuffer);
    ZFREE(tapefilenumbuf);
    ZFREE(numbytesread);
    ZFREE(reqnewfilebuf);

    logmsg(LOG_NOTICE, T_("Warning: Low on memory. No tape buffer created.\n"));
    return(0);
  }

  streamerbuf_alloced_size = bufsize;

  streamerbuf_insert_idx = streamerbuf_processed_idx = 0;
  buff_tape_ptr = 0;

  streamerbuf_writing = NO;
  streamerbuf_highmark = (Uns32)(streamerbuf_conf_highmark / 100.0
				* (Real) streamerbufferlen + 0.5);
  if(streamerbuf_highmark >= streamerbufferlen)
    streamerbuf_highmark = streamerbufferlen - 1;
  if(streamerbuf_highmark < 0)	/* cannot be, but if the type is ever changed */
    streamerbuf_highmark = 0;
  streamerbuf_lowmark = (Uns32)(streamerbuf_conf_lowmark / 100.0
				* (Real) streamerbufferlen + 0.5);
  if(streamerbuf_lowmark > streamerbuf_highmark)
    streamerbuf_lowmark = streamerbuf_highmark;
  if(streamerbuf_lowmark < 0)	/* cannot be, but if the type is ever changed */
    streamerbuf_lowmark = 0;

  memset(reqnewfilebuf, 0, sizeof(Flag) * streamerbufferlen);

#ifdef	HAVE_POSIX_THREADS

  ER__(start_buffer_thread(), i); 

#endif

  return(0);
}

void
dismiss_streamer_buffer()
{
#ifdef	HAVE_POSIX_THREADS
  end_buffer_thread();
#endif

  if(streamerbuf_insert_idx != streamerbuf_processed_idx
			&& (tapeaccessmode & MODE_MASK) == OPENED_FOR_WRITE){
#ifdef	HAVE_POSIX_THREADS
    logmsg(LOG_ERR, T_("Error: Internal inconsistency. Thread did not process all tape operations. Please report, if there were no previous errors causing this.\n"));
#endif

    process_pending_tape_ops();
  }

#ifdef	SERVER_USING_MMAP_FILE_BUFFER

  if(streamerbuf_memfilefd >= 0){
    munmap(streamerbuffer, streamerbuf_alloced_size);
    close(streamerbuf_memfilefd);
    unlink(streamerbuf_memfile);
    streamerbuf_memfilefd = -1;
    streamerbuffer = NULL;
  }

#endif

  ZFREE(streamerbuffer);
  ZFREE(tapefilenumbuf);
  ZFREE(numbytesread);
  ZFREE(reqnewfilebuf);

  streamerbufferlen = streamerbuf_insert_idx
		= streamerbuf_processed_idx = streamerbuf_alloced_size = 0;
}

#ifdef	HAVE_POSIX_THREADS

void	*
buffer_thread_proc(void * arg)
{
  Flag		end_loop = NO;

  forever{
    if((tapeaccessmode & MODE_MASK) == OPENED_FOR_WRITE){
	pthread_mutex_lock(&may_modify_buffer_ptrs);

      	while(!streamerbuf_writing){
	  if(buffer_must_end){
	    end_loop = YES;
	    break;
	  }
	  else{
	    pthread_cond_wait(&buffer_contains_data, &may_modify_buffer_ptrs);

	    if(buffer_must_end && streamerbuf_processed_idx == streamerbuf_insert_idx){
	      end_loop = YES;
	      break;
	    }
	  }
	}

	pthread_mutex_unlock(&may_modify_buffer_ptrs);

	if(end_loop)
	  break;

	process_tape_ops(NO, YES);

	if(num_blocks_in_streamerbuf <= streamerbuf_lowmark){
	  streamerbuf_writing = NO;
#ifdef	AFB_SERVER_SHOW_BUFFER_STATUS
	  statusmsg(T_("Device %s is waiting for the write buffer to be filled.\n"), (char *) devicename);
#endif
	}

	if(tapefd < 0 && (tapeaccessmode & OPENED_RAW_ACCESS))
	  break;	/* end of tape, error already reported, just stop */
    }
    else if((tapeaccessmode & MODE_MASK) == OPENED_FOR_READ){
	pthread_mutex_lock(&may_modify_buffer_ptrs);

      	while(!streamerbuf_reading || (at_mediaend && !force_cartchange)){
	  if(at_mediaend)
	    pthread_cond_signal(&buffer_contains_data);	/* possible deadlock */

	  pthread_cond_wait(&can_write_to_buffer, &may_modify_buffer_ptrs);

	  if(num_blocks_in_streamerbuf <=
				streamerbufferlen - streamerbuf_highmark)
	    streamerbuf_reading = YES;

	  if(buffer_must_end)
	    break;
	}
		/* need streamerbufferlen - 1 here, cause otherwise we cannot */
		/* distinguish between the cases buffer full and buffer empty */
	if(num_blocks_in_streamerbuf >=
				streamerbufferlen - 1 - streamerbuf_lowmark){
	  streamerbuf_reading = NO;

#ifdef	AFB_SERVER_SHOW_BUFFER_STATUS
	  statusmsg(T_("Device %s is waiting for space in the read buffer.\n"), (char *) devicename);
#endif
	}

	pthread_mutex_unlock(&may_modify_buffer_ptrs);

	if(buffer_must_end)
	  break;

	if(streamerbuf_reading) 
	  process_tape_ops(YES, NO);
    }
    else{
	logmsg(LOG_ERR,
		T_("Error: Tape closed, tape operating thread terminates.\n"));
	break;
    }
  }

  pthread_mutex_lock(&may_modify_buffer_ptrs);
  buffer_thread_running = NO;
  pthread_mutex_unlock(&may_modify_buffer_ptrs);

  pthread_exit(0);
}

Int32
start_buffer_thread()
{
  Int32		i;

  pthread_cond_init(& buffer_contains_data, NULL);
  pthread_mutex_init(& may_modify_buffer_ptrs, NULL);
  pthread_cond_init(& can_write_to_buffer, NULL);
  pthread_mutex_init(& may_reconfig_buffer_mutex, NULL);
  pthread_mutex_init(& tapepos_mutex, NULL);

  buffer_must_end = NO;

  buffer_thread_initialized = YES;

  ER__(pthread_create(&buffer_thread, NULL, buffer_thread_proc, NULL), i);

  buffer_thread_running = YES;

  return(0);
}

Int32
end_buffer_thread()
{
  void		*retval;

  if(!buffer_thread_initialized)
    return(0);

  pthread_mutex_lock(&may_modify_buffer_ptrs);
  buffer_must_end = YES;
  streamerbuf_writing = YES;
  streamerbuf_reading = YES;
  streamerbuf_highmark = streamerbuf_lowmark = 0;
  pthread_mutex_unlock(&may_modify_buffer_ptrs);

  if((tapeaccessmode & MODE_MASK) == OPENED_FOR_READ)
    pthread_cancel(buffer_thread);

  pthread_cond_signal(&buffer_contains_data);

  pthread_join(buffer_thread, &retval);

  pthread_cond_destroy(& buffer_contains_data);
  pthread_mutex_destroy(& may_modify_buffer_ptrs);
  pthread_cond_destroy(& can_write_to_buffer);
  pthread_mutex_destroy(& may_reconfig_buffer_mutex);

  buffer_thread_initialized = NO;

  return(0);
}

#endif

Int32
change_tapeblocksize(
  Int32		new_blocksize,
  UChar		**bufptr,
  Int32		*num_processed,
  Flag		*cancel_op)
{
  UChar		*tbufdataptr;	/* uninitialized OK */
  UChar		*tapebufpendptr, *sbufdataptr;	/* uninitialized OK */
  UChar		*tmpmem, *old_tapebuffer, *cptr, *cptr2;
  Int32		num_used_bytes, num_used_blocks, new_buffersize;
  Int32		new_bufferlen, old_buffersize, tapebufpend;
  Int32		n_pend;	/* uninitialized OK */
  Int32		i, j, u, l, n, d1, d2, p_old, p_new, r = 0;
  Uns32		*new_tapefilenumbuf;
  Int32		tapeopenmode;
  Flag		buffer_consistent, *new_reqnewfilebuf;

  if(new_blocksize == tapeblocksize)
    return(0);

#ifdef	HAVE_POSIX_THREADS
  pthread_mutex_lock(&may_reconfig_buffer_mutex);
#endif
			/* shit. Now we have to adjust the whole fuckin' */
  tapeopenmode = (tapeaccessmode & MODE_MASK);		/* data structure */

  old_tapebuffer = tapebuffer;

  if(bufptr){			/* consistency check to be safe */
    buffer_consistent = YES;
    if(streamerbuffer){
      if(tapeopenmode == OPENED_FOR_WRITE){
	if(*bufptr != streamerbuffer + streamerbuf_processed_idx * tapeblocksize)
	  buffer_consistent = NO;
      }
      if(tapeopenmode == OPENED_FOR_READ){
	if(*bufptr != streamerbuffer + streamerbuf_insert_idx * tapeblocksize)
	  buffer_consistent = NO;
      }
    }
    else{
      if(*bufptr != tapebuffer)
	buffer_consistent = NO;
    }

    if(!buffer_consistent){
	logmsg(LOG_ERR, T_("Error: Internal inconsistency. Buffer pointer incorrect for changing tape blocksize. Data might be harmed. Please report.\n"));
    }
  }

  if(streamerbuffer){
    num_used_blocks = num_blocks_in_streamerbuf;
    num_used_bytes = num_used_blocks * tapeblocksize;
    old_buffersize = streamerbufferlen * tapeblocksize;

    if(tapeopenmode == OPENED_FOR_WRITE){
	tapebufpend = tapeptr;
	tapebufpendptr = tapebuffer;
	sbufdataptr = streamerbuffer;
	tbufdataptr = streamerbuffer + num_used_bytes;
    }
    else if(tapeopenmode == OPENED_FOR_READ){
	tapebufpend = tapeptr;
	tapebufpendptr = tapebuffer;
	sbufdataptr = streamerbuffer + tapebufpend;
	tbufdataptr = streamerbuffer;
    }
    else{
	tapebufpend = 0;
	logmsg(LOG_ERR,
		T_("Error: Internal error: streamer buffer exists, but tape not open for read or write. Please report.\n"));
    }

    new_bufferlen = streamerbufferlen * tapeblocksize / new_blocksize;
    new_buffersize = new_bufferlen * new_blocksize;
    if((n = align_n(num_used_bytes + tapebufpend, new_blocksize)) > new_buffersize)
	new_buffersize = n;
    if(tapeopenmode == OPENED_FOR_READ){
	if((n = align_n(num_used_bytes + tapebufpend + new_blocksize, new_blocksize))
							> new_buffersize)
	  new_buffersize = n;	/* we must take care, that we can always do a block read */
    }				/* after return, cause *num_processed == 0 is EOF !!! */

    if(new_buffersize < (n = (new_blocksize * 2)))	/* minimum is 2 tape blocks */
	new_buffersize = n;
    new_bufferlen = new_buffersize / new_blocksize;

    if(new_buffersize > old_buffersize){
	streamerbuffer = RENEWP(streamerbuffer, UChar, new_buffersize);
	if(!streamerbuffer)
	  CLEANUPR(FATAL_ERROR);
    }

    new_tapefilenumbuf = NEWP(Uns32, new_bufferlen);
    new_reqnewfilebuf = NEWP(Flag, new_bufferlen);
    if(!new_tapefilenumbuf || !new_reqnewfilebuf)
	CLEANUPR(FATAL_ERROR);

    i = p_old = p_new = 0;
    n = streamerbuf_processed_idx;
    forever{
	while(p_old <= p_new + new_blocksize){
	  if(n == streamerbuf_insert_idx)
	    break;
	  new_tapefilenumbuf[i] = tapefilenumbuf[n];
	  p_old += tapeblocksize;
	  n = (n + 1) % streamerbufferlen;
	}
	if(n == streamerbuf_insert_idx){
	  for(j = i + 1; j < new_bufferlen; j++)
	    new_tapefilenumbuf[j] = tapefilenumbuf[i];
	  break;
	}

	i++;
	if(i >= new_bufferlen)
	  break;
	p_new += new_blocksize;
	while(i < new_bufferlen && p_new < p_old){
	  new_tapefilenumbuf[i] = tapefilenumbuf[n];
	  i++;
	  p_new += new_blocksize;
	}
	if(i >= new_bufferlen)
	  break;
    }
    free(tapefilenumbuf);
    tapefilenumbuf = new_tapefilenumbuf;

    memset(new_reqnewfilebuf, 0, new_bufferlen * sizeof(Flag));
    for(i = 0, n = streamerbuf_processed_idx;
			n != streamerbuf_insert_idx;
				n = (n + 1) % streamerbufferlen, i++)
	if(reqnewfilebuf[n])
	  new_reqnewfilebuf[new_bufferlen * i / streamerbufferlen] = YES;
    free(reqnewfilebuf);
    reqnewfilebuf = new_reqnewfilebuf;


    cptr = streamerbuffer + streamerbuf_processed_idx * tapeblocksize;

    if(streamerbuf_insert_idx != streamerbuf_processed_idx){
      if(streamerbuf_insert_idx > streamerbuf_processed_idx){
	memmove(sbufdataptr, cptr, num_used_bytes * sizeof(UChar));
      }
      else{
	u = (streamerbufferlen - streamerbuf_processed_idx) * tapeblocksize;
	l = streamerbuf_insert_idx * tapeblocksize;

	if(l > 0 && l == u){
	  memswap(streamerbuffer, cptr, u * sizeof(UChar));
	}

	if(u > 0 && l > u){
	  for(i = 0; i < l; i += u){
	    n = l - i;
	    if(n > u)
		n = u;

	    memswap(streamerbuffer + i, cptr, n * sizeof(UChar));
	  }
	  if(n != u){
	    d1 = n;
	    for(i = 0, j = d1; j < u; i += d1, j += d1){
		n = u - j;
		if(n > d1)
		  n = d1;

		memswap(cptr + i, cptr + j, n * sizeof(UChar));
	    }
	    if(n != d1){
		d2 = d1 - n;
		tmpmem = NEWP(UChar, d2);
		if(!tmpmem)
		  CLEANUPR(FATAL_ERROR);

		memcpy(tmpmem, cptr + i - d2, d2 * sizeof(UChar));
		memmove(cptr + i - d2, cptr + i, n * sizeof(UChar));
		memcpy(cptr + i + n - d2, tmpmem, d2 * sizeof(UChar));

		free(tmpmem);
	    }
	  }
	}

	if(l > 0 && u > l){
	  cptr += u;

	  for(i = 0; i < u; i += l){
	    n = u - i;
	    if(n > l)
		n = l;

	    memswap(streamerbuffer + l - n, cptr - i - n, n * sizeof(UChar));
	  }
	  if(n != l){
	    cptr = streamerbuffer + l;
	    d1 = n;
	    for(i = 0, j = d1; j < l; i += d1, j += d1){
		n = l - j;
		if(n > d1)
		  n = d1;

		memswap(cptr - i - n, cptr - j - n, n * sizeof(UChar));
	    }
	    if(n != d1){
		d2 = d1 - n;
		tmpmem = NEWP(UChar, d2);
		if(!tmpmem)
		  CLEANUPR(FATAL_ERROR);

		memcpy(tmpmem, cptr - i, d2 * sizeof(UChar));
		memmove(streamerbuffer + d2, streamerbuffer, n * sizeof(UChar));
		memcpy(streamerbuffer, tmpmem, d2 * sizeof(UChar));

		free(tmpmem);
	    }
	  }
	}

	if(u > 0)
	  memmove(streamerbuffer + l, streamerbuffer
				+ streamerbuf_processed_idx * tapeblocksize,
					u * sizeof(UChar));

	memmove(sbufdataptr, streamerbuffer, num_used_bytes * sizeof(UChar));
      }
		/* close the holes, if numbytesread[x] < tapeblocksize */
      if(tapeopenmode == OPENED_FOR_READ){
	for(n = 0, i = streamerbuf_processed_idx, cptr = cptr2 = sbufdataptr;
			n < num_used_blocks;
			n++, i = (i + 1) % streamerbufferlen){
	  cptr2 = cptr + tapeblocksize;
	  cptr += numbytesread[i];
	  if(cptr != cptr2 && sbufdataptr + num_used_bytes > cptr2){
	    memmove(cptr, cptr2, sizeof(UChar) *
				(sbufdataptr + num_used_bytes - cptr2));
	    num_used_bytes -= (cptr2 - cptr);
	  }
	}
      }		/* now finally everything is packed at start of buffer */
    }

    if(tapebufpend){
	memcpy(tbufdataptr, tapebufpendptr, tapebufpend * sizeof(UChar));

	tapeptr = 0;
    }

    num_used_bytes += tapebufpend;

    tapebuffer = RENEWP(tapebuffer, UChar, new_blocksize + 2);
    if(!tapebuffer)
	CLEANUPR(FATAL_ERROR);

    if(tapeopenmode == OPENED_FOR_WRITE){
	n = num_used_bytes / new_blocksize;
	n_pend = num_used_bytes - n * new_blocksize;

	memcpy(tapebuffer, streamerbuffer + num_used_bytes - n_pend,
				n_pend * sizeof(UChar));
	tapeptr = n_pend;
    }

			/* rearrange the numbytesread elements */
    numbytesread = RENEWP(numbytesread, Uns32, new_bufferlen);
    if(!numbytesread)
	CLEANUPR(FATAL_ERROR);
    if(tapeopenmode == OPENED_FOR_READ){
	memset(numbytesread, 0, new_bufferlen * sizeof(numbytesread[0]));
	for(i = n = 0; i < num_used_bytes - new_blocksize;
					i += new_blocksize, n++)
	  numbytesread[n] = new_blocksize;
	numbytesread[n] = num_used_bytes - i;

	n_pend = 0;	/* now there's no more pending stuff for tapebuffer */
    }

    streamerbuf_processed_idx = 0;
    streamerbuf_insert_idx = (num_used_bytes - n_pend) / new_blocksize;
    streamerbufferlen = new_bufferlen;

    if(bufptr){				/* adjust the calling environment */
      if(tapeopenmode == OPENED_FOR_WRITE){
	if(num_used_bytes - n_pend < new_blocksize){
	  if(cancel_op)			/* not enough in buffer to be written */
	    *cancel_op = YES;
	  if(num_processed){	/* ATTENTION: This is ok, but if it´s ever */
	    *num_processed = 0;	/* checked in calling functions, this case */
	    if(streamerbuf_processed_idx == 0)	/* must then be handled */
		streamerbuf_processed_idx = streamerbufferlen - 1;
	    else
		streamerbuf_processed_idx--;
	  }
	}
				
	*bufptr = streamerbuffer;	/* This is what should happen */
      }
      if(tapeopenmode == OPENED_FOR_READ){
	if(new_buffersize - (num_used_bytes - n_pend) < new_blocksize){
	  logmsg(LOG_ERR, T_("Error: Internal inconsistency. Cannot read block after return from change_tapeblocksize. Please report.\n"));
#if 0				/* This MUST NOT happen, space is allocated ! */
	  if(cancel_op){	/* not enough space in buffer for reading */
	    *cancel_op = YES;
	    if(streamerbuf_insert_idx == 0)
		streamerbuf_insert_idx = streamerbufferlen - 1;
	    else
		streamerbuf_insert_idx--;
	  }
#endif
	}
	else{					/* THIS is what we want */
	  *bufptr = streamerbuffer + num_used_bytes - n_pend;
	}
      }
    }
  }
  else{
    if(tapeopenmode == OPENED_FOR_WRITE){
      if(new_blocksize < tapeblocksize){
	if(tapeptr > new_blocksize){	/* and one more buffer ... */
	  i = tapeptr - new_blocksize;
	  iauxtapebufferlen = align_n(i + 1, new_blocksize);

	  iauxtapebuffer = NEWP(UChar, iauxtapebufferlen);
	  if(!iauxtapebuffer)
	    CLEANUPR(FATAL_ERROR);

	  memcpy(iauxtapebuffer, tapebuffer,
				iauxtapebufferlen * sizeof(UChar));
	  tapeptr -= iauxtapebufferlen;
	  if(tapeptr > 0)
	    memmove(tapebuffer, tapebuffer + iauxtapebufferlen,
					tapeptr * sizeof(UChar));
	}
      }

	if(bufptr){			/* don't write anything now, that */
	  if(cancel_op)		/* we have created the wonderful buffer */
	    *cancel_op = YES;
	  if(num_processed)
	    *num_processed = 0;	/* ATTENTION: see ATTENTION above */
	}
    }
    if(tapeopenmode == OPENED_FOR_READ){
      if(new_blocksize < tapeblocksize && tapeptr > new_blocksize){
	  i = tapeptr - new_blocksize;
	  iauxtapebufferlen = align_n(i, new_blocksize);

	  iauxtapebuffer = NEWP(UChar, iauxtapebufferlen);
	  if(!iauxtapebuffer)
	    CLEANUPR(FATAL_ERROR);

	  memcpy(iauxtapebuffer, tapebuffer + tapeptr - iauxtapebufferlen,
				iauxtapebufferlen * sizeof(UChar));
	  tapeptr -= iauxtapebufferlen;
      }

      if(bufptr && tapeptr > 0){
	if(cancel_op)
	  *cancel_op = YES;
	if(num_processed)
	  *num_processed = tapeptr;		/* we fake a read */
      }
    }

    tapebuffer = RENEWP(tapebuffer, UChar, new_blocksize);
    if(!tapebuffer)
	CLEANUPR(FATAL_ERROR);

    if(bufptr)
	*bufptr = tapebuffer;
  }

  tapeblocksize = new_blocksize;

 cleanup:
#ifdef	HAVE_POSIX_THREADS
      pthread_mutex_unlock(&may_reconfig_buffer_mutex);
#endif

  return(r);
}

Int32
check_access_cmd()
{
  UChar		buf[264];
  UChar		cmd;
  Int32		n, r;

  if(read_forced(commrfd, buf, 1 + 4 + 256) != 1 + 4 + 256)
    return(PROTOCOL_ERROR);

  buf[1 + 4 + 256] = '\0';

  cmd = buf[0];
  xref_to_Uns32(&n, buf + 1);
  if(cmd == SETCARTSET)
    n--;

  r = (Int32) PROTOCOL_ERROR;

  switch(cmd){
    case SETCARTRIDGE:
	n = cartset_of_cart(n);

    case SETCARTSET:
	if(cartset_clients)
	  cmd = (host_in_list_msg(buf + 1 + 4, cartset_clients[n]) > 0 ?
				COMMAND_OK : CARTSET_EACCESS);
	else
	  cmd = COMMAND_OK;

	send_status(cmd);
	r = COMMAND_OK;
	break;
  }

  return(r);
}

Int32
proof_authentication()
{
  Int32		i;

  i = logon_to_server(commrfd, commwfd, NULL, NULL, NULL,
#ifdef	HAVE_DES_ENCRYPTION
				AUTH_USE_DES
#else
				0
#endif
				| AUTH_NOCONFIRM);

  return(i);
}
