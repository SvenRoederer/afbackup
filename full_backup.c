/****************** Start of $RCSfile: full_backup.c,v $  ****************
*
* $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/full_backup.c,v $
* $Id: full_backup.c,v 1.19 2011/12/19 21:24:10 alb Exp alb $
* $Date: 2011/12/19 21:24:10 $
* $Author: alb $
*
*
******* description ***********************************************
*
*
*
*******************************************************************/

/*
 * Preliminary:
 *
 * i do not care about little pieces of memory, that are
 * allocated once during program execution and not used any
 * longer from some certain time on. E.g. if a string is
 * allocated via strdup:
 *
 *  char * str = strdup("some stuff");
 *
 * i'm not always freeing it the next time this pointer will
 * be assigned a new value. If wasting 100 Bytes is a problem,
 * buy a new machine. Call me what you like.
 */

#include <conf.h>
#include <version.h>

  static char * fileversion = "$RCSfile: full_backup.c,v $ $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/full_backup.c,v $ $Id: full_backup.c,v 1.19 2011/12/19 21:24:10 alb Exp alb $ " PACKAGE " " VERSION_STRING;

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/utsname.h>
#include <pwd.h>
#ifdef	HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#include <unistd.h>
#include <poll.h>
#include <stdarg.h>
#include <mvals.h>
#include <fileutil.h>
#include <sysutils.h>
#include <netutils.h>
#include <genutils.h>
#include <x_regex.h>
#include <afpacker.h>

#include "afbackup.h"
#include "server.h"

#define	ES		((UChar *) "")

#define	WS		"[ \t]*"

/* The following hurts me, but there is no way out */
static char	*BACKUP_RE = TN_("[ \t]*" "[Bb][Aa][Cc][Kk][Uu][Pp]");
static char	*CARTRIDGE_RE = TN_("[ \t]*" "[Cc][Aa][Rr][Tt][Rr][Ii][Dd][Gg][Ee]");
static char	*FILE_RE = TN_("[ \t]*" "[Ff][Ii][Ll][Ee]");
static char	*SERVER_RE = TN_("[ \t]*" "[Ss][Ee][Rr][Vv][Ee][Rr]");
static char	*PORT_RE = TN_("[ \t]*" "[Pp][Oo][Rr][Tt]");
static char	*NCARTS_RE = TN_("[Nn][Uu][Mm]\\([Bb][Ee][Rr]\\)?" "[ \t]*" "[Oo][Ff]" \
			"[ \t]*" "[Cc][Aa][Rr][Tt]\\([Rr][Ii][Dd][Gg][Ee]\\)?[Ss]");

#define	EMREC_PREFIX	"@@@===--->>>"
#define	EMREC_POSTFIX	"<<<---===@@@"
#define	EMREC_PREFIX_RE	"^" WS EMREC_PREFIX

#define	EMREC_TAPE_PREFIX	"!#@$%&^*(_)+[;" ",{-<=]'.}>`/|: "

#define	BU_NO_LOCK		0
#define	BU_LOCKED		1
#define	BU_GOT_LOCK		2
#define	BU_DONT_LOCK		32
#define	BU_CANT_LOCK		99

#define	MODE_FULL_BACKUP	0
#define	MODE_INCR_BACKUP	1
#define	MODE_RESTORE		2
#define	MODE_VERIFYBU		3
#define	MODE_COPY_TAPE		4
#define	MODE_UPDATE_INDEXES	5

#define	CLEAR_LIST		0
#define	ACCESS_POSITION		1
#define	INSERT_POSITION		2
#define	INSERT_POS_NEWCART	3

#define	FIRST_NUM_ARG		1
#define	IGNORE_NUM_ARG_TRAILERS	2

/* eval_index_line returns:
   -1		empty
   -2		comment
   <= -10:	not recognized
   >=0		fileposition
*/

#define	INDEX_FILEENTRY(n)	((n) >= 0)
	/* might be empty i.e. no filename at this location i.e. '\0' */
#define	INDEX_EMPTY(n)		((n) == -1)
#define	INDEX_COMMENT(n)	((n) == -2)
#define	INDEX_NOCONTENTS(n)	(INDEX_EMPTY(n) || INDEX_COMMENT(n))
#define	INDEX_RECOGNIZED(n)	((n) > -10)
#define	INDEX_UNRECOGNIZED(n)	((n) <= -10)

typedef	struct _scandir_args_ {
  int	fd;
  Flag	entry_found;
  Flag	write_failed;
  Flag	have_location;
  int	ctofd;
  int	cfromfd;
  int	cerrfd;
  UChar	*read_data;
  Int32	read_data_nall;
  Int32	num_read_data;
  UChar	*read_err;
  Int32	read_err_nall;
  Int32	num_read_err;
}	ScanDirArgs;

typedef struct utsname	UName;

typedef struct bu_tapeentry {
  UChar		*server;
  Int32		port;
  Int32		cart;
  Int32		file;
  time_t	budate;
  Int32		mode;
  Int32		bunum;
  Int32		inscart;
  Int32		insfile;
}	BuTapeEntry;

typedef struct _datalocator_ {
  Int32		cart;
  Int32		file;
  Int32		efile;
  UChar		*server;
  Int32		port;
} DataLocator;

typedef struct streamerstat {
  UChar		devstatus;
  Int32		serverstatus;
  Int32		numactive;
  Int32		numpending;
  UChar		serverid[256];
  Uns32Range	*needed_tapes;
  Uns32Range	*deleted_tapes;
}	StreamerStat;

typedef struct serveridtab {
  UChar		*servername;
  Int32		serverport;
  UChar		*serverid;
} ServerIdTab;

typedef	struct _dataarea_ {
  ServerIdTab	serveridtab;
  Uns32Range	*carts;
} DataArea;

UChar	*cmd_res[] = {
	"[Ff][Uu][Ll][Ll]",
	"[Ii][Nn][Cc][Rr]",
	"\\([Bb][Aa][Cc][Kk][Oo][Uu][Tt]\\|[Rr][Ee][Ss][Tt][Oo][Rr][Ee]\\)",
	"[Vv][Ee][Rr][Ii][Ff][Yy]",
	"[Cc][Oo][Pp][Yy]",
	"[Uu][Pp][Dd][Aa][Tt].*[Ii][Nn]?[Dd][Ee]?[Xx]"
		};

UChar	*option2prefix[] = {	FILECONTOPTION " " FILECONTPREFIX,
				FILECONTZOPTION " " FILECONTZPREFIX,
				LOCALDEVOPTION " " LOCALDEVPREFIX,
				NULL };

UChar	*default_paramfiles[] = { DEFAULT_CLIENT_CONFIGFILES , NULL };

UChar	*bindir = NULL;
UChar	*vardir = NULL;
UChar	*libdir = NULL;
UChar	*confdir = NULL;
UChar   *logdir = NULL;

ReplSpec	dir_pat_repl[] = {
	{ "%B",	NULL, &bindir },
	{ "%L",	NULL, &libdir },
	{ "%V",	NULL, &vardir },
	{ "%C",	NULL, &confdir },
	{ "%I",	NULL, &logdir },
	};

UChar	*paramfile = NULL;
UChar	*partfile = NULL;
UChar	*numfile = NULL;
UChar	*oldmarkfile = NULL;
UChar	*orgoldmarkfile = NULL;
UChar	*newmarkfile = NULL;
UChar	*progressfile = NULL;
UChar	*indexfile = NULL;
UChar	*cryptfile = NULL;
UChar	*save_entry_file = NULL;
UChar	**save_entries = NULL;
UChar	*level_times_file = NULL;
KeyValuePair	*level_timespecs = NULL;
Int32	num_level_timespecs = 0;
UChar	*needed_tapes_file = NULL;
UChar	**needed_tapes_lines = NULL;
Int32	num_needed_tapes_lines = 0;
UChar	*server_ids_file = NULL;
UChar	*index_ages_file = NULL;
UChar	*start_pos_file = NULL;
Flag	use_ctime = NO;

UChar	*clientprogram = NULL;

UChar	*piperprogram = NULL;
UChar	*zprogram = NULL;

UChar	*backuphostlist = NULL;
UChar	**backuphosts = NULL;
Int32	num_backuphosts = 0;
UChar	*backupportlist = NULL;
UChar	**backupportstrs;
Int32	*backupports = NULL;
Int32	num_backupports = 0;
Int32	*msgports = NULL;
Int32	serveridx = 0;
UChar	server_id[512];		/* only needed for backup */

UChar	*rootdir = NULL;
UChar	*dirstobackupraw = NULL;
UChar	**dirstobackup = NULL;
UChar	*alldirstobackupraw = NULL;
UChar	**alldirstobackup = NULL;
UChar	*filestoskipraw = NULL;
UChar	**filestoskip = NULL;
UChar	*dirstoskipraw = NULL;
UChar	**dirstoskip = NULL;
UChar	**fstypestosave = NULL;
UChar	**fstypestoskip = NULL;
UChar	**fstypestoprune = NULL;
UChar	*fstypesraw = NULL;
UChar	*indexfilepart = NULL;
Int32	numindexestostore = -1;
Real32	daystostoreindexes = -1.0;
Int32	numindexestoscan = -1;
Real32	daystoscanindexes = -1.0;
UChar	*compresscmd = NULL;
UChar	*uncompresscmd = NULL;
UChar	*idxcompresscmd = NULL;
UChar	*idxuncompresscmd = NULL;
Int32	builtin_compr_level = 0;
Flag	compressbu = YES;
UChar	*logfile = NULL;
Int32	numparts = 1;
UChar	*startinfoprog = NULL;
UChar	*initprog = NULL;
UChar	*exitprog = NULL;
UChar	*reportfile = NULL;
UChar	*filelist = NULL;
Flag	compresslogfiles = YES;
UChar	**filecontentstobu = NULL;
UChar	**dont_compress = NULL;
Int32	num_dont_compress = 0;
UChar	*dont_compress_str = NULL;
UChar	*exclude_filename = NULL;
Int32	backup_level = MAXINT;

time_t	start_time;
time_t	cmp_time;

Flag	no_default_backup = NO;

UChar	*minrestoreinfo = NULL;

Int32	mode = -1;
int	part = 1;
int	num = 1;
Flag	keep_counter = NO;
Flag	keep_timestamp = NO;
Flag	newcart = NO;
Flag	remove_lower_levels = NO;
Flag	progress = NO;

/* Comment from Devin Reade:
 * the variable 'interrupted' should be volatile; it can be changed by
 * a signal handler, and therefore optimizations can lose information
 * Comment on Comment (AF):
 * this is a theoretical consideration without real impact. When the
 * signal handler terminates, interrupted will be modified and that's
 * sufficient
 */
int	interrupted = 0;
int	clientpid = -1;
Flag	logf_lost = NO;
int	msgprocpid = -1;
int	msgprocfd = -1;

UChar	*cartsetlist = NULL;
Int32	*cartsets = NULL;
Int32	num_cartsets = 0;
Flag	detach = NO;
Flag	chksum = NO;
Flag	chk_perms = NO;

Flag	verbose = NO;
Flag	quiet = NO;

UChar	*msgs_config_str = NULL;
Flag	output_server_msgs = NO;

Real64	total_volume;
Real64	current_volume;

sigset_t	idxproc_blk_sigs;

/* for restore */
Flag	do_counting = NO;
Flag	do_listing = NO;
Flag	do_listcarts = NO;
Flag	do_raw_listing = NO;
Flag	ign_case = NO;
UChar	*restoreroot = NULL;
Flag	restore_all = NO;
#define	vfy_list_all	restore_all
Int32	restore_em = 0;
Flag	restore_emlist = NO;
Int32	restore_EM = 0;
Flag	preserve_existing = NO;
UChar	*beforestr = NULL;
UChar	*afterstr = NULL;
time_t	time_older = 0;
time_t	time_newer = 0;
time_t	butime_after = 0;
time_t	butime_before = 0;
UChar	*restoretapestr = NULL;
Uns32Range	*restoretapes = NULL;
time_t	current_bu_time = 0;
time_t	prev_bu_time = 0;
Flag	have_timerange = NO;
Flag	have_mtime_idx = NO;
Int32	num_restore_paths = 0;
UChar	**restore_paths = NULL;
Int32	num_prev_backup = 0;
UChar	*num_prev_backup_str = NULL;
Int32	num_prev_run = 0;
UChar	**indexfiles = NULL, **zindexfiles = NULL;
Int32	numidx, num_indexfiles;
UChar	*formatstr = NULL;
ReplSpec	*fmt_repls = NULL;
Int32	num_fmt_repls = 0;
#define	FMT_REPL_CHARS	"nbhpcfoOmMtT"
uid_t	curuid;
gid_t	curgid;
gid_t	*curgids;
int	curngids;
uid_t	ieuid;
gid_t	iegid;
UChar	*systemname = NULL;
UChar	*identity = NULL;
Flag	identity_from_arg = NO;
Flag	vardir_from_arg = NO;
UName	systeminfo;

/* for copy_tape */
UChar	*tmpdir = NULL;

RE_cmp_buffer	backup_re, cartridge_re, file_re, ncarts_re,
		server_re, port_re;
ScanDirArgs	scandir_args;

UChar		*lockfile = NULL;
int		lockfd;
UChar		locked = BU_NO_LOCK;
struct flock	lockb;


#if	MAXPATHLEN < 1000
#define	TMPBUFSIZ	4096
#else
#define	TMPBUFSIZ	(MAXPATHLEN * 4 + 100)
#endif

UChar	tmpbuf[TMPBUFSIZ];

UChar	*lockdirs[] = {
		"/var/locks", "/var/lock", "/var/spool/locks",
		"/var/spool/lock", "/var/tmp", "/tmp", NULL,
};

UChar	*tmpdirs[10] = { "pwd - dummy",
		FN_DIRSEPSTR "tmp" FN_DIRSEPSTR "%N",
		FN_DIRSEPSTR "var" FN_DIRSEPSTR "tmp" FN_DIRSEPSTR "%N",
		FN_DIRSEPSTR "temp" FN_DIRSEPSTR "%N",
		NULL};
UChar	*tmpfiles[10];

ParamFileEntry	entries[] = {
	{ &dirstobackupraw, NULL,
	(UChar *) "^[ \t]*[Dd]ire?c?t?o?r?i?e?s[ \t_-]*[Tt]o[ \t_-]*[Bb]ackup:?[ \t]*",
		TypeUCharPTR	},
	{ &backuphostlist, NULL,
	(UChar *) "^[ \t]*[Bb]ackup[ \t_-]*[Hh]osts?:?[ \t]*",
		TypeUCharPTR	},
	{ &backupportlist, NULL,
	(UChar *) "^[ \t]*[Bb]ackup[ \t_-]*[Pp]orts?:?[ \t]*",
		TypeUCharPTR	},
	{ &rootdir, NULL,
	(UChar *) "^[ \t]*[Rr]oot[ \t_-]*[Dd]ire?c?t?o?r?y?:?[ \t]*",
		TypeUCharPTR	},
	{ &filestoskipraw, NULL,
	(UChar *) "^[ \t]*[Ff]iles[ \t_-]*[Tt]o[ \t_-]*[Ss]kip:?[ \t]*",
		TypeUCharPTR	},
	{ &dirstoskipraw, NULL,
	(UChar *) "^[ \t]*[Dd]ire?c?t?o?r?i?e?s[ \t_-]*[Tt]o[ \t_-]*[Ss]kip:?[ \t]*",
		TypeUCharPTR	},
	{ &fstypesraw, NULL,
	(UChar *) "^[ \t]*[Ff]i?l?e?[ \t_-]*[Ss]y?s?t?e?m?[ \t_-]*[Tt]ypes:?[ \t]*",
		TypeUCharPTR	},
	{ &indexfilepart, NULL,
	(UChar *) "^[ \t]*[Ii]ndex[ \t_-]*[Ff]ile[ \t_-]*[Pp]art:?[ \t]*",
		TypeUCharPTR	},
	{ &numindexestostore, NULL,
	(UChar *) "^[ \t]*[Nn]um[-_ \t]*[Ii]nd\\(ic\\|ex\\)es[-_ \t]*[Tt]o[ \t_-]*[Ss]tore:?",
		TypeInt32	},
	{ &daystostoreindexes, NULL,
	(UChar *) "^[ \t]*[Dd]ays[-_ \t]*[Tt]o[ \t_-]*[Ss]tore[-_ \t]*[Ii]nd\\(ic\\|ex\\)es:?",
		TypeReal32	},
	{ &numindexestoscan, NULL,
	(UChar *) "^[ \t]*[Nn]um[-_ \t]*[Ii]nd\\(ic\\|ex\\)es[-_ \t]*[Tt]o[ \t_-]*[Ss]can:?",
		TypeInt32	},
	{ &daystoscanindexes, NULL,
	(UChar *) "^[ \t]*[Dd]ays[-_ \t]*[Tt]o[ \t_-]*[Ss]can[-_ \t]*[Ii]nd\\(ic\\|ex\\)es:?",
		TypeReal32	},
	{ &compresscmd, NULL,
	(UChar *) "^[ \t]*\\([Pp]rocess\\|[Cc]ompress\\)[-_ \t]*[Cc]o?m?ma?n?d:?[ \t]*",
		TypeUCharPTR	},
	{ &uncompresscmd, NULL,
	(UChar *) "^[ \t]*\\([Uu]nprocess\\|[Uu]ncompress\\)[-_ \t]*[Cc]o?m?ma?n?d:?[ \t]*",
		TypeUCharPTR	},
	{ &idxcompresscmd, NULL,
	(UChar *) "^[ \t]*[Ii]ndex[-_ \t]*\\([Pp]rocess\\|[Cc]ompress\\)[-_ \t]*[Cc]o?m?ma?n?d:?[ \t]*",
		TypeUCharPTR	},
	{ &idxuncompresscmd, NULL,
	(UChar *) "^[ \t]*[Ii]ndex[-_ \t]*\\([Uu]nprocess\\|[Uu]ncompress\\)[-_ \t]*[Cc]o?m?ma?n?d:?[ \t]*",
		TypeUCharPTR	},
	{ &builtin_compr_level, NULL,
	(UChar *) "^[ \t]*[Bb]uilt[-_ \t]*[iI]n[-_ \t]*[Cc]ompre?s?s?i?o?n?[-_ \t]*\\([Ll]evel\\)?:?",
		TypeInt32	},
	{ &dont_compress_str, NULL,
	(UChar *) "^[ \t]*[Dd]o[-_ \t]*[Nn][\'o]?t[-_ \t]*\\([Pp]rocess\\|[Cc]ompress\\):?[ \t]*",
		TypeUCharPTR	},
	{ &exclude_filename, NULL,
	(UChar *) "^[ \t]*[Ee]xclu?d?e?[-_ \t]*[Ll]ist[-_ \t]*[Ff]ile[-_ \t]*[Nn]?a?m?e?:?[ \t]*",
		TypeUCharPTR	},
	{ &logfile, NULL,
	(UChar *) "^[ \t]*[Ll]ogg?i?n?g?[-_ \t]*[Ff]ile:?[ \t]*",
		TypeUCharPTR	},
	{ &numparts, NULL,
	(UChar *) "^[ \t]*[Nn]um[-_ \t]*[Bb]ackup[-_ \t]*[Pp]arts:?",
		TypeInt32	},
	{ &cartsets, &num_cartsets,
	(UChar *) "^[ \t]*[Cc]artr?i?d?g?e?[-_ \t]*[Ss]ets?:?",
		TypeInt32PTR	},
	{ &startinfoprog, NULL,
	(UChar *) "^[ \t]*[Ss]tartu?p?[-_ \t]*[Ii]nfo[-_ \t]*[Pp]rogram:?[ \t]*",
		TypeUCharPTR	},
	{ &progress, NULL,
	(UChar *) "^[ \t]*\\([Rr]e?po?r?t?[-_ \t]*\\)?[Pp]rogress:?[ \t]*",
		TypeFlag	},
	{ &initprog, NULL,
	(UChar *) "^[ \t]*[Ii]nit[-_ \t]*[Pp]rogram:?[ \t]*",
		TypeUCharPTR	},
	{ &exitprog, NULL,
	(UChar *) "^[ \t]*[Ee]xit[-_ \t]*[Pp]rogram:?[ \t]*",
		TypeUCharPTR	},
	{ &compresslogfiles, NULL,
	(UChar *) "^[ \t]*\\([Pp]rocess\\|[Cc]ompress\\)[-_ \t]*\\([Ll]ogg?i?n?g?[-_ \t]*[Ff]iles?\\|[Ii]n?d\\(e?x\\|i?c\\)e?s?\\([Ff]iles?\\)?\\)[-_ \t]*:?",
		TypeFlag	},
	{ &compressbu, NULL,
	(UChar *) "^[ \t]*\\([Pp]rocess\\|[Cc]ompress\\)[-_ \t]*[Bb]ackupe?d?[-_ \t]*\\([Ff]iles\\)?:?",
		TypeFlag	},
	{ &cryptfile, NULL,
	(UChar *) "^[ \t]*\\([Ee]n\\)?[Cc]rypti?o?n?[ \t_-]*[Kk]ey[ \t_-]*[Ff]ile:?[ \t]*",
		TypeUCharPTR	},
	{ &lockfile, NULL,
	(UChar *) "^[ \t]*[Ll]ocki?n?g?[-_ \t]*[Ff]ile:?[ \t]*",
		TypeUCharPTR	},
	{ &vardir, NULL,
	(UChar *) "^[ \t]*[Vv][Aa][Rr][-_ \t]*[Dd]ire?c?t?o?r?y?:?[ \t]*",
		TypeUCharPTR	},
	{ &identity, NULL,
	(UChar *) "^[ \t]*\\([Cc]lient[-_ \t]*\\)?[Ii]denti\\(ty\\|fier\\):?",
		TypeUCharPTR	},
	{ &chksum, NULL,
	(UChar *) "^[ \t]*\\([Ww]rite\\)?[Cc]heck[-_ \t]*[Ss]um\\(s\\|ming\\)?:?",
		TypeFlag	},
	{ &chk_perms, NULL,
	(UChar *) "^[ \t]*[Cc]heck[-_ \t]*[Rr]estore[-_ \t]*[Aa]ccess[-_ \t]*[Pp]ermi?s?s?i?o?n?s?:?",
		TypeFlag	},
	{ &use_ctime, NULL,
	(UChar *) "^[ \t]*[Uu]se[-_ \t]*[Cc][-_ \t]*[Tt]ime:?",
		TypeFlag	},
	{ &msgs_config_str, NULL,
	(UChar *) "^[ \t]*[Pp]rint[-_ \t]*[Ss]e?r?ve?r?[-_ \t]*[Mm]e?ss?a?ge?s?:?",
		TypeUCharPTR	},
};

#define	EM__(nmem)	{ nomemerrexit(nmem); }
#define	EEM__(nmem)	{ if(nmem) nomemerrexit(NULL); }
#define	ENM__		nomemerrexit(NULL)
#define	ER__(cmd, lerrfl)	{ if( (lerrfl = cmd) ) return(lerrfl); }

#define	repl_dirs(string)	repl_substrings((string), dir_pat_repl,	\
		sizeof(dir_pat_repl) / sizeof(dir_pat_repl[0]))
#define	one_more_for_restore	(restore_EM > 1 || restore_em > 1)


static void	verifybu(int, char **);
static void	restore(int, char **);
static void	restoreall(int, char **);
static void	restoreem(int, char **);
static void	copy_tape(int, char **);
static Int32	get_args(int, char **);
static Int32	get_restore_args(int, char **);
static Int32	get_verify_args(int, char **);
static Int32	get_copytape_args(int, char **);
static Int32	get_update_indexes_args(int, char **);
static Int32	get_backup_args(int, char **);
static int	get_arg_num(int, char **, Int32 *, Int8);
static Int32	write_to_restore(UChar **, UChar **, Int32 *);
static UChar	*repl_substring_safe(UChar *, UChar *, UChar *);
static Int32	get_tapepos(Int32 *, Int32 *, Int32 *, Int8, UChar *, Int32, UChar *);
static Int32	get_streamerstate(StreamerStat *, UChar *, Int32, Flag);
static Int32	read_header_line(UChar *, Int32 *, time_t *, Int32 *,
						UChar *, UChar *);
static Int32	eval_index_line(UChar *, UChar **, Int32 *, Int32 *, Int32 *,
				int *, time_t *);
static void	compose_clntcmd(UChar *, UChar *, UChar *, UChar *, Int32,
		Int32, UChar *, Int32, Int32, time_t, time_t, Int32, UChar *);
static Int32	add_server_id_entries(UChar *, ServerIdTab *, Int32);
static ServerIdTab	*get_server_id_entries(UChar *, Int32 *);
static Int32	serverid_from_netaddr(ServerIdTab *, UChar *, Int32, UChar *);
static Uns32Range	*get_needed_tapes(UChar *, UChar *);
static Int32	remove_needed_tapes(UChar *, UChar *, Uns32Range *);
static Int32	remove_tapes_from_idxes(StreamerStat *, Int32, UChar *,
					Int32, Int32);
static UChar	*repl_fmts(UChar *, ReplSpec *, Int32, UChar *, time_t);
static int	open_idxfile_read(Int32, int *, UChar **, Flag);

static void
do_exit(int s)
{
  int	fd, i;

  if(locked == BU_GOT_LOCK){
#if	0	/* unnecessary: close removes any lock */
    lockb.l_type = F_UNLCK;
    fcntl(lockfd, F_SETLK, &lockb);
#endif

    unlink(lockfile);
    close(lockfd);
  }

  if(interrupted)
    s = 128 + interrupted;

  if(exitprog && (mode == MODE_FULL_BACKUP || mode == MODE_INCR_BACKUP)){
    sprintf(tmpbuf, "%d", s);

    if(reportfile)
	exitprog = repl_substring_safe(exitprog, "%r", reportfile);
    exitprog = repl_substring_safe(exitprog, "%e", tmpbuf);
    exitprog = repl_substring_safe(exitprog, "%i",
			minrestoreinfo ? minrestoreinfo : (UChar *) "");
    exitprog = repl_substring_safe(exitprog, "%l",
		filelist ? (!eaccess(filelist, R_OK) ? filelist
			: (UChar *) T_("<filename-logfile_not_yet_created>"))
		: (UChar *) T_("<filename-logfile_not_yet_determined>"));

    if(reportfile)
      if((fd = open(reportfile, O_WRONLY | O_APPEND | O_CREAT, 0644)) >= 0)
	close(fd);

    system(exitprog);

  }

  if(reportfile)
    if(reportfile[0])
	unlink(reportfile);

  for(i = 0; i < 10; i++){
    if(tmpfiles[i]){
	unlink(tmpfiles[i]);
	free(tmpfiles[i]);
    }
  }

  exit(s);
}

static void
nomemerrexit(void * ptr)
{
  if(!ptr){
    fprintf(stderr, "Error: No memory.\n");
    do_exit(3);
  }
}

static void
errmsg(UChar * msg, ...)
{
  FILE		*lfp = NULL;
  va_list	args;

  if(logfile)
    if(strcmp(logfile, "-"))
	lfp = fopen(logfile, "a");

  if(lfp){
    va_start(args, msg);
    fprintf(lfp, "%s, ", actimestr());
    vfprintf(lfp, msg, args);
    fclose(lfp);
    va_end(args);
  }

  va_start(args, msg);
  vfprintf(stderr, msg, args);
  va_end(args);
}

static void
logmsg(UChar * msg, ...)
{
  FILE		*lfp = NULL;
  Flag		si = NO;
  va_list	args;

  if(logfile){
    if(!strcmp(logfile, "-")){
	lfp = stdout;
	si = YES;
    }
    else
	lfp = fopen(logfile, "a");
  }

  if(lfp){
    fprintf(lfp, "%s, ", actimestr());
    va_start(args, msg);
    vfprintf(lfp, msg, args);
    va_end(args);
    if(!si)
	fclose(lfp);
  }

  if(verbose){
    va_start(args, msg);
    vfprintf(stdout, msg, args);
    va_end(args);
  }
}

UChar *
repl_substring_safe(UChar * org, UChar * to_subst, UChar * subst_by)
{
  UChar		*newstr;

  EM__(newstr = repl_substring(org, to_subst, subst_by));

  return(newstr);
}

Int32
replace_if_pipe(UChar ** str, Int32 mode, Int32 level, Int32 part)
{
  FILE		*fp;
  UChar		*newstr = NULL, *line = NULL, *sep, *newcmd = NULL;
  UChar		typestr[30], *cptr;
  Int32		r = 0;

  if(strncmp(*str, "| ", 2))
    return(0);

  typestr[0] = (mode == MODE_FULL_BACKUP ? 'F' :
		(level > 0 ? 'L' : 'I'));
  typestr[1] = '\0';
  if(mode == MODE_INCR_BACKUP && level > 0)
    sprintf(typestr + 1, "%d", (int) level);
  if(mode == MODE_FULL_BACKUP && part > 0)
    sprintf(typestr + 1, "%d", (int) part);

  newcmd = repl_substring_safe(*str + 2, "%T", typestr);

  fp = popen(newcmd, "r");
  if(!fp){
    errmsg(T_("Cannot run command `%s'.\n"), *str + 2);
    CLEANUPR(-1);
  }

  sep = "";
  EM__(newstr = strdup(""));
  while(!feof(fp)){
    ZFREE(line);
    line = fget_alloc_str(fp);
    if(!line)
	continue;
    while(chop(line));

    cptr = newstr;
    EM__(newstr = strchain(newstr, sep, line, NULL));
    free(cptr);

    sep = " ";
  }

  ZFREE(*str);
  *str = newstr;
  newstr = NULL;

 cleanup:
  if(fp)
    pclose(fp);
  ZFREE(newcmd);
  ZFREE(newstr);
  ZFREE(line);
  return(r);
}

static void
usage(UChar * pname)
{
  pname = FN_BASENAME(pname);

  switch(mode){
   case MODE_RESTORE:
    if(curuid)
      fprintf(stderr,
        T_("Usage: %s [ -nltvmi ] [ -<past-backup-no> ] [ -C <root-directory> ] \\\n"
        "               [ -h <backuphosts> ] [ -P <backup-ports> ] \\\n"
        "               [ -A \"<after-date> [ %% <after-backup-date> ]\" ] \\\n"
	"               [ -B \"<before-date> [ %% <before-backup-date> ]\" ] \\\n"
	"               [ -k <encryption-key-file> ] [ -T <tapes> ] \\\n"
	"               [ -z <process-cmd> <unprocess-cmd> ] \\\n"
	"               [ -Z <built-in-compress-level> ] [ -F <format> ] \\\n"
	"               [ { -N <num-indexfiles> | -O <indexfile-age-days> } ] \\\n"
	"               [ -M <server-message-config> ] [ -c <configuration-file> ]\\\n"
        "               [ -p ] <path-pattern> [ [ -p ] <path-patterns> [ ... ] ]\n"),
		pname);
    else
      fprintf(stderr,
        T_("Usage: %s [ -nltvmi ] [ -<past-backup-no> ] [ -C <root-directory> ] \\\n"
        "               [ -h <backuphosts> ] [ -P <backup-ports> ] \\\n"
	"               [ -c <configuration-file> ] [ -W <identity> ] \\\n"
	"               [ -I <indexfile-part> ] [ -V <var-directory> ] \\\n"
        "               [ -A \"<after-date> [ %% <after-backup-date> ]\" ] \\\n"
	"               [ -B \"<before-date> [ %% <before-backup-date> ]\" ] \\\n"
	"               [ -k <encryption-key-file> ] [ -T <tapes> ] \\\n"
	"               [ -z <process-cmd> <unprocess-cmd> ] \\\n"
	"               [ -Z <built-in-compress-level> ] [ -F <format> ] \\\n"
	"               [ -N <num-indexfiles> | -O <indexfile-age-days> ] \\\n"
	"               [ -M <server-message-config> ] \\\n"
        "               [ -p ] <path-pattern> [ [ -p ] <path-patterns> [ ... ] ]\n"
        "       %s -a [ -nlvm ] [ -<past-backup-no> ] [ -C <root-directory> ] \\\n"
        "               [ -h <backuphosts> ] [ -P <backup-ports> ] \\\n"
	"               [ -c <configuration-file> ] [ -W <identity> ] \\\n"
	"               [ -I <indexfile-part> ] [ -V <var-directory> ] \\\n"
	"               [ -k <encryption-key-file> ] \\\n"
	"               [ -z <process-cmd> <unprocess-cmd> ] \\\n"
	"               [ -Z <built-in-compress-level> ] [ -F <format> ] \\\n"
	"               [ -M <server-message-config> ]\n"
	"       %s -{ef} [ -evm ] [ -C <root-directory> ] [ -h <backuphosts> ] \\\n"
       	"               [ -P <backup-ports> ] [ -V <var-directory> ] \\\n"
	"               [ -z <process-cmd> <unprocess-cmd> ] \\\n"
	"               [ -Z <built-in-compress-level> ] \\\n"
	"               [ -k <encryption-key-file> ] [ -W <identity> ] \\\n"
	"               [ -M <server-message-config> ] \\\n"
	"               [ -c <configuration-file> ] < <startup-info-file>\n"
	"       %s -E [ -Enlvm ] [ -C <root-directory> ] [ -h <backuphosts> ] \\\n"
       	"               [ -P <backup-ports> ] [ -V <var-directory> ] \\\n"
	"               [ -z <process-cmd> <unprocess-cmd> ] \\\n"
	"               [ -Z <built-in-compress-level> ] \\\n"
	"               [ -k <encryption-key-file> ] [ -W <identity> ] \\\n"
	"               [ -M <server-message-config> ] \\\n"
	"               [ -c <configuration-file> ] \\\n"
	"               [ <cartridge-number> | <cartridge-range> ] ... ]\n"),
                        pname, pname, pname, pname);
    break;

   case MODE_UPDATE_INDEXES:
    if(curuid)
      fprintf(stderr, "%s: %s\n", T_("Usage"), pname);
    else
      fprintf(stderr,
	T_("Usage: %s [ -v ] [ -c <configuration-file> ] \\\n"
	"               [ -h <backuphosts> ] [ -P <backup-ports> ] \\\n"
	"               [ -I <indexfile-part> ] [ -V <var-directory> ] \\\n"
	"               [ -k <encryption-key-file> ] [ -W <identity> ] \\\n"
	"               [ -z <process-cmd> <unprocess-cmd> ] \\\n"
	"               [ -Z <built-in-compress-level> ] \\\n"),
	pname);
    break;

   case MODE_VERIFYBU:
    fprintf(stderr,
	T_("Usage: %s [ -lav ] [ -c <configuration-file> ] \\\n"
	"              [ -<past-run-no>[.<past-backup-no>] ] \\\n"
	"              [ -h <backuphosts> ] [ -P <backup-ports> ] \\\n"
	"              [ -C <root-directory> ] [ -S <cartridge-set> ] \\\n"
	"              [ -I <indexfile-part> ] [ -V <var-directory> ] \\\n"
	"              [ -k <encryption-key-file> ] [ -W <identity> ] \\\n"
	"              [ -z <process-cmd> <unprocess-cmd> ] \\\n"
	"              [ -Z <built-in-compress-level> ] \\\n"
	"              [ -M <server-message-config> ]\n"),
	pname);
    break;

   case MODE_COPY_TAPE:
    fprintf(stderr,
	T_("Usage: %s [ -v ] [ -c <configuration-file> ] \\\n"
	"                   [ -l <logfile> ] [ -T <tmpdir> ] \\\n"
	"                   [ -M <server-message-config> ] \\\n"
	"                   [ -h <source-server> ] [ -P <source-serverport> ] \\\n"
	"                   [ -C <source-cartridge> ] [ -F <tape-filenumber> ] \\\n"
	"                   [ -k <source-encryption-key-file> ] \\\n"
	"                   [ -D [ -h <target-server> ] [ -P <target-serverport> ] \\\n"
	"                     [ -C <target-cartridge> ] [ -k <target-encryption-key-file> ] ]\n"),
	pname);
    break;

   default:
    fprintf(stderr,
	T_("Usage: %s [ -daGy%s ] [ {+-}LBx ] [ <files> <directories> ... ] \\\n"
	"                   [ -C <root-directory> ] [ -F \"<files-to-skip>\" ] \\\n"
	"                   [ -D \"<directories-to-skip>\" ] \\\n"
	"                   [ -c <configuration-file> ] [ -W <identity> ] \\\n"
	"                   [ -h <backuphosts> ] [ -P <backup-ports> ] \\\n"
	"                   [ -I <indexfile-part> ] [ -Q <backup-level> ] \\\n"
	"                   [ { -N <num-indexes-to-store> ] | \\\n"
	"                     -O <max-age-of-indexes-to-store-in-days> } ] \\\n"
	"                   [ -z <process-cmd> <unprocess-cmd> ] \\\n"
	"                   [ -Z <built-in-compress-level> ] \\\n"
	"                   [ -s \"<dont-process-patterns>\" ] \\\n"
	"                   [ -X <exclude-list-file> ] [ -l <logfile> ] \\\n"
	"                   [ -i <startup-info-program> ] \\\n"
	"                   [ -b <init-program> ] [ -e <exit-program> ] \\\n"
	"                   [ -k <encryption-key-file> ] \\\n"
	"                   [ -f <filesystem-types> ] \\\n"
	"                   [ -V <var-directory> ] [ -S <cartridge-sets> ] \\\n"
	"                   [ -M <server-message-config> ]\n"),
	pname, mode == MODE_INCR_BACKUP ? "EH" : "");
  }

  do_exit(1);
}

static void
sig_handler(int s)
{
/* Ladies and Gentelmen, let me present you the superduper cracy heuristics
 * to find out, what should really be done on what signal. This is a little
 * like reading an operator's mind. Anyway let's have a try.
 */
  static int	num_int = 0;
  static time_t	first_int = (time_t) 0;

  Flag		hard_interrupt = NO, ttyinput;

  ttyinput = isatty(0);

  if(interrupted){
    if(s == SIGINT && (mode == MODE_INCR_BACKUP || mode == MODE_FULL_BACKUP)){
    /* Maybe stdio should be avoided within a signal handler, but the user
     * should be informed about what is going on. If it is not done here,
     * it can probably never be done from now on. This comment bases on
     * a hint from Davin Reade. I (AF) basically disagree on a 'must'
     * concerning avoiding stdio within signal handlers. If stdio must be
     * avoided, to be policital correct, any system call must be avoided
     */
	if(num_int <= 0)
	  first_int = time(NULL);

	num_int++;
	if(time(NULL) - first_int < 2){
	  if(num_int > 2){
	    fprintf(stderr, T_("Forced interruption, program will terminate as soon as possible.\n"));
	    hard_interrupt = YES;
	  }
	}
	else{
	  num_int = 0;
	}

	if(!hard_interrupt){
	  fprintf(stderr, T_(
		"Repeated interruption, please stand by during cleanup.\n"
		"To force termination press Ctrl-C 3 times within 2 seconds "
		"or Ctrl-\\ once.\n"
		));
	}
    }
    else{
	fprintf(stderr, T_("Repeated interruption, please stand by during cleanup.\n"));
    }

    if(!hard_interrupt)
	return;
  }
    
  if(mode != MODE_INCR_BACKUP && mode != MODE_FULL_BACKUP){
    /*
     * At this point we are now using the default action for this particular
     * signal, due to the semantics of signal(2).
     */
    kill(getpid(), s);
    do_exit(2);
  }

  if(s == SIGTERM || s == SIGABRT || s == SIGQUIT
				|| (s == SIGINT && !ttyinput))
    hard_interrupt = YES;

  if(hard_interrupt && clientpid > 0)
    kill(clientpid, SIGKILL);

  if(s != SIGPIPE){
    signal(s, sig_handler);

    if(interrupted)
	return;

    fprintf(stderr, T_("Interrupted. Cleanup in progress, please stand by.\n"));
  }
  else{
    if(!interrupted)
	errmsg(T_("Connection to client process lost, exiting.\n"));

    logf_lost = YES;
  }

  interrupted = s;
}

static void
exitcleanup()
{
  if(!keep_timestamp){
    switch(mode){
     case MODE_FULL_BACKUP:
      if(!eaccess(orgoldmarkfile, R_OK)){
	unlink(oldmarkfile);
	rename(orgoldmarkfile, oldmarkfile);
      }
      break;

     case MODE_INCR_BACKUP:
      unlink(newmarkfile);

     default:
      break;
    }
  }
}

static void
intrpt_exit()
{
  exitcleanup();

  if(logfile){
    switch(mode){
     case MODE_FULL_BACKUP:
      if(numparts > 1)
	logmsg(T_("Full backup part %d interrupted.\n"), part);
      else
	logmsg(T_("Full backup interrupted.\n"));
      break;

     case MODE_INCR_BACKUP:
	logmsg(T_("Incremental backup interrupted.\n"));
      break;

     default:
      break;
    }
  }

  do_exit(2);
}

static void
failed_exit(Int32 s)
{
  exitcleanup();

  if(logfile){
    if(mode == MODE_FULL_BACKUP){
      if(numparts > 1)
	errmsg(T_("Full backup part %d failed.\n"), part);
      else
	errmsg(T_("Full backup failed.\n"));
    }
    if(mode == MODE_INCR_BACKUP){
      errmsg(T_("Incremental backup failed.\n"));
    }
  }

  do_exit(s);
}

/*
 * Obtain a mutex.  Returns a value indicating a sucessful lock or otherwise
 */

Int32
set_lock()
{
  struct stat	statb;
  char		buf[20];
  Int32		i;

  if(locked == BU_GOT_LOCK)
    return((Int32) locked);

  if(!lockfile)
    return((Int32) BU_CANT_LOCK);

  i = lstat(lockfile, &statb);
  if(!i && !IS_REGFILE(statb)){
    if(unlink(lockfile)){
	errmsg(T_("Error: Cannot remove lock file entry `%s', that is not a file.\n"),
		lockfile);
	return( (Int32) (locked = BU_CANT_LOCK) );
    }

    i = 1;
  }
  if(!i){
    lockfd = open(lockfile, O_WRONLY | O_SYNC);
    if(lockfd < 0){
      errmsg(T_("Warning: Lock file `%s' exists, but can't open it.\n"),
		lockfile);
      return( (Int32) (locked = BU_CANT_LOCK) );
    }
    /* Note: could be, the file exists, belongs to a wrong user, is writable
     * and there is yet no lock on it. In this case some kind of attack is
     * possible. Such an attack might lead to concurrent access to var
     * files maintained by this program, possibly scrambling them up or
     * destroying them. The administrator should avoid this configuring a
     * lock file, that is in a safe place i.e. within the mentioned var
     * directory, that must not be writable by normal users.
     */
  }
  else{
    lockfd = open(lockfile, O_WRONLY | O_CREAT | O_SYNC | O_EXCL, 0644);
    if(lockfd < 0){
	errmsg(T_("Error: Cannot create lock file `%s'.\n"), lockfile);
	return( (Int32) (locked = BU_CANT_LOCK) );
    }
  }

  lockb.l_type = F_WRLCK;
  if(fcntl(lockfd, F_SETLK, &lockb)){
    return( (Int32) (locked = BU_LOCKED) );
    /* GDR: this ignores other ways in which fcntl can fail */
    /* No problem here. fcntl should not fail in any way */
  }

  sprintf(buf, "%d\n", (int) getpid());
  write(lockfd, buf, strlen(buf) + 1);

  return( (Int32) (locked = BU_GOT_LOCK) );
}

/*
 * reads the first characters of a file. If they contain a textual
 * representation of a positive integer, place that integer into num
 * and return zero.  Otherwise, initialize num to zero and returns
 * nonzero.
 */

static Int32
read_pint_file(UChar * filename, int * num)
{
  FILE	*fp;

  *num = 0;
  fp = fopen(filename, "r");
  if(fp){
    if(fscanf(fp, "%d", num) <= 0)
	*num = 0;

    fclose(fp);
  }

  return(*num <= 0);
}

static Int32
write_uns_file(UChar * filename, int num)
{
  FILE		*fp;
  Int32	ret = 0;

  fp = fopen(filename, "w");
  if(fp){
    if(fprintf(fp, "%d\n", num) <= 0)
	ret = 1;

    fclose(fp);
  }
  else
    ret = 1;

  return(ret);
}

static Int32
handle_client_output(ScanDirArgs * arg, Flag * got_output)
{
  Int32		i, n;
  UChar		*cptr;

  if(got_output)
    *got_output = NO;

  while((i = read(arg->cfromfd, arg->read_data, arg->read_data_nall)) > 0){
    n = write_forced(arg->fd, arg->read_data, i);
    if(n < i)
	return(-1);

    if(got_output)
	*got_output = YES;
  }

  forever{
    if(arg->num_read_err + 1024 > arg->read_err_nall){
	arg->read_err = RENEWP(arg->read_err, UChar, arg->read_err_nall + 1024);
	EM__(arg->read_err);
	arg->read_err_nall += 1024;
    }

    i = read(arg->cerrfd, arg->read_err + arg->num_read_err, 1023);
    if(i > 0){
	arg->num_read_err += i;
	if(got_output)
	  *got_output = YES;
    }
    else
	break;

    arg->read_err[arg->num_read_err] = '\0';

    while( (cptr = strchr(arg->read_err, '\n')) ){
	*(cptr++) = '\0';
	for(i = 2; cptr - i >= arg->read_err && *(cptr - i) == '.'; i++)
	  *(cptr - i) = '\0';
	errmsg("%s\n", arg->read_err);
	n = (arg->read_err + arg->num_read_err) - cptr;
	memmove(arg->read_err, cptr, n * sizeof(UChar));
	arg->num_read_err = n;
    }
  }

  return(0);
}

static void
write_progress(Real64 curvol, Real64 totalvol)
{
  static Flag	reported_error = NO;
  FILE	*fp;

  if(!progress || !progressfile)
    return;

  fp = fopen(progressfile, "w");
  if(!fp){
    if(!reported_error)
	errmsg(T_("Error: Could not open progress file `%s' for writing.\n"),
			progressfile);
    reported_error = YES;
    return;
  }

  if(curvol > totalvol)	/* don't report > 100 %, growing might happen */
    curvol = totalvol;

  fprintf(fp, "%.0f %.0f %.2f\n", (float) curvol, (float) totalvol,
		(float) (curvol / totalvol * 100.0));
  fclose(fp);
}

static Int32
calc_total_volume(UChar * name, void * data, struct stat * statbp)
{
  Real64	*sumptr;

  sumptr = (Real *) data;

  if(IS_REGFILE(*statbp))
    *sumptr += (Real64)(statbp->st_size);
	/* estimated average packer entry overhead */
  *sumptr += (Real64) strlen(name) + 50.0;

  return(0);
}

static Int32
write_filename(UChar * name, void * data, struct stat * statbp)
{
  static time_t	last_progress_report_time = 0;
  time_t	curtime;
  int		maxfd;
  UChar		*cptr = NULL;
  Int32		ret = 0, needed_size, n, i, j, nfd;
  Flag		allocated = NO, got_output;
  ScanDirArgs	*scandirargs;
  struct pollfd	fds[3];

  scandirargs = (ScanDirArgs *) data;

  if(name){
    needed_size = strlen(name) * 4 + 5;
    if(needed_size > TMPBUFSIZ){
      cptr = NEWP(UChar, needed_size);
      if(!cptr){
	ret = errno;
	CLEANUP;
      }

      allocated = YES;
    }
    else{
      cptr = tmpbuf;
    }

    mk_esc_seq(name, ESCAPE_CHARACTER, cptr);
    strcat(cptr, "\n");
    name = cptr;

    n = strlen(name);
  }
  else{
    n = 1;
  }

  maxfd = MAX(MAX(scandirargs->ctofd, scandirargs->cfromfd), scandirargs->cerrfd);

  for(i = 0; i < n;){
    if(interrupted && name)
	break;

    fds[0].fd = scandirargs->cfromfd;
    fds[0].events = POLLIN;
    fds[1].fd = scandirargs->cerrfd;
    fds[1].events = POLLIN;
    fds[2].fd = scandirargs->ctofd;
    fds[2].events = POLLOUT;
    fds[2].revents = 0;

    got_output = NO;
    nfd = poll(fds, name ? 3 : 2, -1);
    if(nfd > 0){
	if(name){
	  if(POLLOUTEVENT(fds[2].revents)){
	    j = write(scandirargs->ctofd, name + i, n - i);
	    if(j > 0)
		i += j;
	  }
	}

	if(POLLINEVENT(fds[0].revents) || POLLINEVENT(fds[1].revents)){
	  j = handle_client_output(scandirargs, &got_output);
	  if(j){
	    ret = j;
	    CLEANUP;
	  }
	}
    }

    if(!name && (nfd < 1 || !got_output))
	break;
  }

#if 0
  if(i < n){
    errmsg(T_("Error: Could not send filename to afclient process.\n"));

    scandirargs->write_failed = YES;
    ret = errno;
  }
#endif

  if(name)
    scandirargs->entry_found = YES;

  if(progress && statbp && name){
    if(IS_REGFILE(*statbp))
	current_volume += (Real64 )(statbp->st_size);
	/* estimated average packer entry overhead */
    current_volume += (Real64) strlen(name) + 50.0;

    curtime = time(NULL);
    if(curtime - last_progress_report_time >= 2){
	last_progress_report_time = curtime;

	write_progress(current_volume, total_volume);
    }
  }

 cleanup:
  if(cptr && allocated)
    free(cptr);

  if(interrupted)
    return(1000);

  return(ret);
}

static Int32
err_write_filename(UChar * name, void * data)
{
  int		fd;
  ScanDirArgs	*scandirargs;

  scandirargs = (ScanDirArgs *) data;

  fd = scandirargs->fd;

  scandirargs->fd = 1;

  /* don't: write_filename(name, data));*/

  scandirargs->fd = fd;

  return(0);
}

/* returns:
   -1		empty
   -2		comment
   <= -10:	not recognized
   >=0		fileposition
*/

static Int32
eval_index_line(
  UChar		*line,
  UChar		**server,
  Int32		*port,
  Int32		*cart,
  Int32		*fileno,
  int		*uid,
  time_t	*mtime)
{
  UChar		*cptr, *orgline, *newmem, *colon = NULL, a;
  int		p = 0, c = 0, f = 0, u = - MAXINT, servlen = 0, i, n;
  time_t	m = UNSPECIFIED_TIME;

  if(empty_string(line))
    return(-1);

  if(*(first_nospace(line)) == '#')
    return(-2);

  orgline = line;

  if( (colon = strchr(line, ':')) )
    *colon = '\0';

  cptr = strstr(line, PORTSEP);

  if(colon)
    *colon = ':';

  if(cptr){
    servlen = cptr - line;

    a = *cptr;
    *cptr = '\0';
    i = isfqhn(line);
    *cptr = a;

    if(!i || sscanf(cptr + 1, "%d" LOCSEP, &p) <= 0)
	return(-10);

    line = strstr(line, LOCSEP) + 1;
  }

  i = sscanf(line, "%d.%d%n", &c, &f, &n);
  if(i < 2)
    return(-11);

  if(!strncmp(line + n, UIDSEP, strlen(UIDSEP))){
    i = sscanf(line, "%d.%d" UIDSEP "%d%n", &c, &f, &u, &n);
    if(i < 3)
	return(-12);
    if(line[n] != ':' && strncmp(line + n, MTIMESEP, strlen(MTIMESEP)))
	return(-13);
  }
  else if(line[n] != ':')
    return(-14);

  if(!strncmp(line + n, MTIMESEP, i = strlen(MTIMESEP)))
    m = strint2time(line + n + i);

  if(servlen && server){
    do{
	newmem = NEWP(UChar, servlen + 1);
	if(!newmem)
	  ms_sleep(200);
    }while(!newmem);

    strncpy(newmem, orgline, servlen);
    newmem[servlen] = '\0';
    *server = newmem;
  }
  if(p && port)
    *port = p;
  if(u != - MAXINT && uid)
    *uid = u;
  if(c && cart)
    *cart = c;
  if(f && fileno)
    *fileno = f;
  if(m != UNSPECIFIED_TIME && mtime)
    *mtime = m;

  if(colon)
    return(first_nospace(colon + 1) - orgline);

  return(strlen(line));
}

Int32
start_msg_proc(
  UChar		*server,
  Int32		port)
{
  UChar		cmd[1024];
  int		fd, inp[2], pid;
  Int32		r = 0, i;

  if(!output_server_msgs)
    return(0);

  if(msgprocpid > 0 && ! kill(msgprocpid, 0))
    return(0);

  if(!server)
    server = backuphosts[serveridx];
  if(port == 0)
    port = backupports[serveridx];

  if(port < 0){
    for(i = 0; i < num_backuphosts; i++){
	if(backupports[i] == - port && !strcmp(backuphosts[i], server)){
	  port = msgports[i];
	  break;
	}
    }

    if(port < 0){
	port = - port;
	r = 1;
    }
  }

  if(pipe(inp))
    return(-2);

  pid = fork_forced();
  if(pid < 0){
    close(inp[0]);
    close(inp[1]);
    return(-1);
  }
  if(pid == 0){
    close(inp[1]);
    compose_clntcmd(cmd, "m", NULL, server, port, 0, cryptfile,
			0, 0, 0, 0, 0, NULL);
    fd = open_to_pipe(cmd, NULL, 0, &pid, 0);
    if(fd < 0)
	exit(5);

	/* it is better to exec here because of easier handling of
	 * file descriptors to be closed, so let's dare to rely on
	 * the presence of /bin/sh for effectiveness */
    dup2(inp[0], 0);

    sprintf(tmpbuf, "read a ; kill -9 %d ; exit 0", pid);
    execl("/bin/sh", "sh", "-c", tmpbuf, NULL);
    exit(6);

#if 0
    read(inp[0], cmd, 1);	/* wait until parent dies or closes fd */

    close(fd);
    close(inp[0]);

    kill(pid, SIGKILL);
    waitpid(pid, &pst, 0);
    exit(0);
#endif
  }

  close(inp[0]);

  msgprocpid = pid;
  msgprocfd = inp[1];

  return(r);
}

Int32
stop_msg_proc()
{
  int	pst;

  if(msgprocpid < 0)
    return(0);

  close(msgprocfd);

  waitpid(msgprocpid, &pst, 0);

  return(WEXITSTATUS(pst));
}

Int32
start_msg_proc_msg(
  UChar		*server,
  Int32		port)
{
  Int32	i;

  i = start_msg_proc(server, port);
  if(i > 0)
    errmsg(T_("Warning: Obtaining the server's informational messages might not work.\n"));
  if(i < 0)
    errmsg(T_("Warning: Obtaining the server's informational messages fails.\n"));

  return(i);
}

main(int argc, char ** argv)
{
/* uninitialized ok: uncompcmd, j, ifd, infile, outfile, inunzip,
                     outzip unzipfile, unzipfilecmd */
  UChar		*backuphome, *programfile, c;
  UChar		*cptr, *cptr2, *cptr3, **cpptr, **cpptr2, **cpptr3;
  UChar		*zippedidxf, *tmpidxf;
  UChar		*tmperrf, ***cppptr, *uncompcmd;
  UChar		*dirsarr[2], *crucial_tapes_str = NULL, *header;
  UChar		kbuf[64], vbuf[128];
  Flag		must_read_fnamlog = NO, one_server;
  Flag		got_total_volume, dryrun;
  Flag		remove_previous_equal_level, errflp, success = YES;
  UChar		*filename, **dirlists[10], *tmpstartposfile;
  Int32		i, j, n, lck, num_inlines = 0, num_outlines = 0;
  Int32		startcart, startfile, last_idx_num = 0, used_port;
  Int32		rc, rf;
  int		pos, p;
  FILE		*fp;
  int		pid, pst, ppi[3], ppo[3], ppe[3], lfd, fd;
  int		ifd, opid, ipid, i1, fdflags, ofdflags, efdflags, eofdflags;
  FindParams	findparams;
  UChar		*infile, *outfile, inunzip, outzip;
  UChar		*unzipfile, **unzipfilecmd;
  UChar		*rename_from = NULL, *rename_to = NULL, **lines;
  UChar		*new_needed_tapes_file = NULL;
  Int32		effective_backup_level, num_server_ids, num_servers;
  Uns32Range	*new_tape_ranges, *all_tape_ranges;
  Uns32Range	*needed_tapes_before = NULL;
  time_t	saved_newer_time;
  struct stat	statb, statb2;
  ServerIdTab	*server_id_tab = NULL;
  StreamerStat	*stst;

#ifdef ENABLE_NLS
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);
#endif
  
  SETZERO(dirlists);
  SETZERO(tmpfiles);
  strcpy(server_id, "");

  curuid = getuid();
  curgid = getgid();
  curngids = 0;
  curgids = NULL;
  get_groups(&curngids, &curgids);
  ieuid = geteuid();
  iegid = getegid();

  programfile = find_program(argv[0]);
  if(!programfile){
    errmsg(T_("Error: Cannot find program file of `%s'.\n"), argv[0]);
    exit(4);
  }

  /* use the name of how we were invoked to set our operating mode */
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

  i = stat(programfile, &statb);
  if(i){
    errmsg(T_("Error: Cannot stat %s.\n"), programfile);
    exit(41);
  }

  /*
   * If we're not in restore or "update indices" mode, make sure we're not
   * setuid/setgid
   */
  if(((statb.st_mode & (S_ISUID | S_ISGID)) || curuid != ieuid || curgid != iegid)
	&& mode != MODE_RESTORE && mode != MODE_UPDATE_INDEXES){
    errmsg(T_("Error: %s is installed SUID or SGID. Please check installation.\n"),
			programfile);
    exit(42);
  }

  /* determine home directory */
  backuphome = getenv("BACKUP_HOME");
  if(curuid && backuphome){
    errmsg(T_("Warning: When run as normal user, the environment variable BACKUP_HOME is ignored.\n"));
    backuphome = NULL;
  }

#ifdef	ORIG_DEFAULTS

  if(!backuphome){
    backuphome = programfile;
    EM__(cptr = mkabspath(backuphome, NULL));
    free(backuphome);
    backuphome = resolve_path__(cptr, NULL);
    if(!backuphome){
	errmsg(T_("Error: Cannot follow path `%s'.\n"), cptr);
	do_exit(29);
    }
    free(cptr);

    cptr = FN_LASTDIRDELIM(backuphome);
    if(!cptr){
      errmsg(T_("Strange error: cannot separate binary directory.\n"));
      do_exit(5);
    }
    *cptr = '\0';
    cptr = FN_LASTDIRDELIM(backuphome);
    if(!cptr){
      errmsg(T_("Strange error: cannot separate client directory.\n"));
      do_exit(5);
    }
    *cptr = '\0';
  }
  else
    backuphome = strdup(backuphome);

  EM__(backuphome);

  /* construct file- and dirnames */
  bindir = strapp(backuphome, FN_DIRSEPSTR "bin");
  vardir = strapp(backuphome, FN_DIRSEPSTR "var");
  libdir = strapp(backuphome, FN_DIRSEPSTR "lib");
  confdir = strapp(backuphome, FN_DIRSEPSTR "etc");
  logdir = strdup(vardir);

  if(!bindir || !vardir || !libdir || !logdir || !confdir)
    ENM__;

  if(!paramfile)
    EM__(paramfile = strapp(confdir, FN_DIRSEPSTR "backup.conf"));

#else	/* defined(ORIG_DEFAULTS) */

  if(!backuphome){
     /* construct file- and dirnames */
     bindir = strdup(DEFBINDIR);
     vardir = strdup(DEFVARDIR);
     libdir = strdup(DEFLIBDIR);
     logdir = strdup(DEFLOGDIR);
     confdir = strdup(DEFCONFDIR);
  }
  else{
     EM__(backuphome = strdup(backuphome));
     /* construct file- and dirnames */
     /* first bin or sbin ? */
     EM__(bindir = strapp(backuphome, FN_DIRSEPSTR "bin"));
     EM__(cptr = strapp(bindir, FN_DIRSEPSTR "afbackup"));
     if(eaccess(cptr, X_OK)){
	free(cptr);
	EM__(cptr = strapp(backuphome, FN_DIRSEPSTR "sbin" FN_DIRSEPSTR "afbackup"));
	if(!eaccess(cptr, X_OK)){
	  free(bindir);
	  bindir = strapp(backuphome, FN_DIRSEPSTR "sbin");
	}
     }
     ZFREE(cptr);
     vardir = strapp(backuphome, FN_DIRSEPSTR "var");
     libdir = strapp(backuphome, FN_DIRSEPSTR "lib");
     logdir = strapp(backuphome, FN_DIRSEPSTR "var");
     confdir = strapp(backuphome, FN_DIRSEPSTR "etc");
  }

  if(!bindir || !vardir || !libdir || !logdir || !confdir)
     ENM__;

  if(!paramfile)
    EM__(paramfile = strapp(confdir, FN_DIRSEPSTR DEFCLIENTCONF));

#endif	/* if else defined(ORIG_DEFAULTS) */

  cleanpath(confdir);

  /* check usage */
  get_args(argc, argv);

  systemname = get_my_off_hn();
  if(!systemname){
    i = uname(&systeminfo);
    if(i < 0){
	fprintf(stderr, T_("Warning: Cannot determine hostname.\n"));
	systemname = "<unknown>";
    }
    else
	systemname = systeminfo.nodename;
  }
/*  cptr = strchr(systemname, '.');
  if(cptr)
    *cptr = '\0';
*/ /* not needed anymore, have same_host() function now */

  if(detach){
    if(fork()){
	exit(0);
    }
    else{
	detach_from_tty();

	ms_sleep(1000 * 30);
    }
  }

  /* set signal handlers */
  signal(SIGTERM, sig_handler);
  signal(SIGABRT, sig_handler);
  signal(SIGQUIT, sig_handler);
  signal(SIGHUP, sig_handler);
  signal(SIGINT, sig_handler);
  signal(SIGPIPE, sig_handler);

  /* signals blocked during execution of index processing processes */
  sigemptyset(&idxproc_blk_sigs);
  sigaddset(&idxproc_blk_sigs, SIGINT);	/* this is advanced from Ctrl-C */

  /* compile some regular expressions */
  SETZERO(backup_re);
  SETZERO(cartridge_re);
  SETZERO(file_re);
  SETZERO(server_re);
  SETZERO(port_re);
  SETZERO(ncarts_re);
  if(re_compile_pattern(T_(CARTRIDGE_RE), strlen(T_(CARTRIDGE_RE)), &cartridge_re)
	|| re_compile_pattern(T_(FILE_RE), strlen(T_(FILE_RE)), &file_re)
	|| re_compile_pattern(T_(SERVER_RE), strlen(T_(SERVER_RE)), &server_re)
	|| re_compile_pattern(T_(PORT_RE), strlen(T_(PORT_RE)), &port_re)
	|| re_compile_pattern(T_(NCARTS_RE), strlen(T_(NCARTS_RE)), &ncarts_re)
	|| re_compile_pattern(T_(BACKUP_RE), strlen(T_(BACKUP_RE)), &backup_re))
    ENM__;

  if((mode == MODE_UPDATE_INDEXES || mode == MODE_RESTORE) && curuid){
    if(set_eff_ugids(0, -1, 0, NULL)){
	errmsg(T_("Error: Cannot seteuid root, sorry.\n"));
	do_exit(17);
    }
  }

  /*
   * Locate our configuration file.
   */
  if(!stat(paramfile, &statb) && eaccess(paramfile, R_OK)){
    errmsg(T_("Error: Cannot read parameter file `%s'. %s.\n"), paramfile,
				T_("File not readable"));
    do_exit(6);
  }
  if(stat(paramfile, &statb)){
    for(cpptr = default_paramfiles; *cpptr; cpptr++){
      paramfile = *cpptr;
      if(!stat(paramfile, &statb) && eaccess(paramfile, R_OK)){
	errmsg(T_("Error: Cannot read parameter file `%s'.\n"), paramfile);
	do_exit(6);
      }
      if(!stat(paramfile, &statb))
	break;
    }

    if(!*cpptr){
	errmsg(T_("Warning: No configuration file found, using defaults.\n"));
	paramfile = NULL;
	/*do_exit(6);*/

	/* Note: in this case the statb still contains the inode data of
	 * the binary of this program, what can be used in the next if
	 * i.e. then the binary must be owned by the calling user not
	 * to compromise it (? superfluous paranoia)
	 */
    }
  }

  if(statb.st_uid && curuid){
    errmsg(T_("Error: Configuration file must be owned by root when running as normal user.\n"));
    do_exit(7);
  }

  if(paramfile){
    /* read parameter file */
    i = read_param_file(paramfile, entries,
		sizeof(entries) / sizeof(entries[0]), NULL, NULL);
    if(i){
	errmsg(T_("Error: Cannot read parameter file.\n"));
	do_exit(6);
    }

    /* override by commandline-arguments */
    get_args(argc, argv);
  }

#ifndef	USE_ZLIB

  if(builtin_compr_level){
    fprintf(stderr, T_("Warning: Built-in compression requested, but not compiled in. Ignoring request for built-in compression.\n"));
    builtin_compr_level = 0;
  }

#endif

  massage_string(vardir);
  partfile = strapp(vardir, FN_DIRSEPSTR "part");
  numfile = strapp(vardir, FN_DIRSEPSTR "num");
  oldmarkfile = strapp(vardir, FN_DIRSEPSTR "oldmark");
  newmarkfile = strapp(vardir, FN_DIRSEPSTR "newmark");
  progressfile = strapp(vardir, FN_DIRSEPSTR "progress");
  orgoldmarkfile = strapp(vardir, FN_DIRSEPSTR "oldmark.org");
  clientprogram = strapp(bindir, FN_DIRSEPSTR "afbackup");
  if(!dont_compress_str)
    dont_compress_str = "";
  num_dont_compress = str2wordsq(&dont_compress, dont_compress_str);
  piperprogram = strapp(bindir, FN_DIRSEPSTR "__piper");
  zprogram = strapp(bindir, FN_DIRSEPSTR "__z");
  server_ids_file = strapp(vardir, FN_DIRSEPSTR "server_ids");
  index_ages_file = strapp(vardir, FN_DIRSEPSTR "index_ages");
  if(!partfile || !oldmarkfile || !orgoldmarkfile
		|| !clientprogram || !newmarkfile || !piperprogram
		|| !zprogram || num_dont_compress < 0 || !numfile
		|| !index_ages_file || !server_ids_file || !progressfile)
    ENM__;

  if(!cryptfile){
    EM__(cryptfile = strapp(confdir, FN_DIRSEPSTR "cryptkey"));
    if(eaccess(cryptfile, R_OK))
	ZFREE(cryptfile);
  }
  if(cryptfile)
    massage_string(cryptfile);

  /* correct parameter values, if unreasonable or set defaults */
  if(!dirstobackupraw)
    EM__(dirstobackupraw = strdup("*"));
  if(backuphostlist && empty_string(backuphostlist))
    ZFREE(backuphostlist);
  if(!backuphostlist)			/* set default server name */
    EM__(backuphostlist = strdup(DEFAULT_SERVER));
  while( (cptr = strchr(backuphostlist, ',')) )	/* [,;] -> SPC */
    *cptr = ' ';
  while( (cptr = strchr(backuphostlist, ';')) )
    *cptr = ' ';
  if(backupportlist && empty_string(backupportlist))
    ZFREE(backupportlist);
  if(!backupportlist)			/* set default port number */
    EM__(backupportlist = strdup(DEFAULT_SERVICE));
  while( (cptr = strchr(backupportlist, ',')) )	/* [,;] -> SPC */
    *cptr = ' ';
  while( (cptr = strchr(backupportlist, ';')) )
    *cptr = ' ';
  num_backuphosts = str2words(&backuphosts, backuphostlist);
  num_backupports = str2words(&backupportstrs, backupportlist);
  if(num_backuphosts < 0 || num_backupports < 0)
    ENM__;
  EM__(backupports = ZRENEWP(backupports, Int32, num_backuphosts * sizeof(Int32)));
  if(num_backupports > num_backuphosts)	/* ports: no more than hosts */
    num_backupports = num_backuphosts;
  EM__(msgports = ZRENEWP(msgports, Int32, num_backuphosts * sizeof(Int32)));
  for(i = 0; i < num_backupports; i++){
    p = get_tcp_portnum(backupportstrs[i]);
    if(p < 0){
	errmsg(T_("Error: Cannot resolve TCP service name `%s'.\n"),
				backupportstrs[i]);
	exit(1);
    }

    backupports[i] = p;
  }
  while(num_backupports < num_backuphosts)	/* set default values */
    backupports[num_backupports++] = -1;

  one_server = (num_backuphosts == 1 ? YES : NO);

  if(msgs_config_str){
    if((n = word_count(msgs_config_str)) < 1){
	ZFREE(msgs_config_str);
    }
    else{
	EM__(cptr2 = strdup(msgs_config_str));
	cptr3 = sscanword(msgs_config_str, cptr2);
	for(cptr = cptr2; *cptr; cptr++){
	  switch(*cptr){
	   case 'b':
	   case 'B':
	    output_server_msgs |=
		(mode == MODE_FULL_BACKUP || mode == MODE_INCR_BACKUP);
	    break;

	   case 'v':
	   case 'V':
	    output_server_msgs |= (mode == MODE_VERIFYBU);
	    break;

	   case 'r':
	   case 'R':
	    output_server_msgs |= (mode == MODE_RESTORE);
	    break;

	   case 'C':
	   case 'c':
	    output_server_msgs |= (mode == MODE_COPY_TAPE);
	    break;

	   default:
	    errmsg(T_("Warning: unknown character %c in server message configuration.\n"));
	  }
	}

	n--;
	if(n > num_backuphosts)
	  n = num_backuphosts;
	for(i = 0; i < num_backuphosts; i++){
	  if(i >= n){
	    p = backupports[i];
	  }
	  else{
	    cptr3 = sscanword(cptr3, cptr2);
	    cptr = cptr2;

	    p = get_tcp_portnum(cptr);
	    if(p < 0){
		errmsg(T_("Error: Cannot resolve TCP service name `%s'.\n"),
				cptr2);
		exit(1);
	    }
	  }

	  msgports[i] = p;
	}
    }
  }

  EM__(cartsets = ZRENEWP(cartsets, Int32, num_backuphosts * sizeof(Int32)));
  if(num_cartsets > num_backuphosts)	/* sets: no more than hosts */
    num_cartsets = num_backuphosts;
  i = 0;
  if( (cptr = cartsetlist) ){			/* evaluate list */
    while(sscanf(cptr, "%d%n", &p, &pos) > 0 && i < num_backuphosts){
	cartsets[i++] = p;
	cptr += pos;
	while(!isdigit(*cptr) && *cptr)		/* skip to next number */
	  cptr++;
    }
  }
  if(i > num_cartsets)			/* max of given is new # sets */
    num_cartsets = i;
  if(num_cartsets < num_backuphosts){
    while(num_cartsets < num_backuphosts)
	cartsets[num_cartsets++] = 1;		/* set default values */
  }
  if(!rootdir)
    EM__(rootdir = strdup(FN_DIRSEPSTR));
  if(!indexfilepart)
    EM__(indexfilepart = strapp(logdir, FN_DIRSEPSTR "backup_log."));
  if(numparts <= 0)
    numparts = 1;
  if(numindexestostore >= 0 && daystostoreindexes >= 0.0)
    errmsg(T_("Warning: Both DaysToStoreIndexes and NumIndexesToStore configured. Ignoring the shorter duration.\n"));
  if(numindexestostore < 0)
    numindexestostore = 0;
  if(!compresscmd || !uncompresscmd)
    uncompresscmd = compresscmd = "";
  if(!compresscmd[0] || !uncompresscmd[0])
    uncompresscmd = compresscmd = "";
  EEM__(repl_dirs(&compresscmd) || repl_dirs(&uncompresscmd)
	|| repl_dirs(&idxcompresscmd) || repl_dirs(&idxuncompresscmd));

  /* check, if compress/uncompress commands are executable */
  cptr = check_commands_executable(YES, compresscmd, uncompresscmd,
		idxcompresscmd, idxuncompresscmd, NULL);
  if(cptr){
    errmsg(T_("Error: Cannot execute `%s'.\n"), cptr);
    exit(43);
  }

  /* set compression commands, as appropriate */
  if(!builtin_compr_level){
    if(!idxcompresscmd)
	idxcompresscmd = compresscmd;
    if(!idxuncompresscmd)
	idxuncompresscmd = uncompresscmd;
  }
  else{
    /* Instead of hacking all the zlib stuff in here i construct
     * pipes through the __z program, that does the same as the
     * built-in zlib compression. This is much easier to implement,
     * much less error prone and affects performance only little,
     * because the subprocesses are started only few times */

    sprintf(tmpbuf, "%d", (int) builtin_compr_level);
    if(compresscmd[0]){
	if(!idxcompresscmd)
	  EM__(idxcompresscmd = strchain(piperprogram, " ", compresscmd, " | ",
				zprogram, " -", tmpbuf, NULL));
	EM__(compresscmd = strchain(". ", tmpbuf, " ", compresscmd, NULL));
    }
    else{
	if(!idxcompresscmd)
	  EM__(idxcompresscmd = strchain(zprogram, " -", tmpbuf, NULL));
	EM__(compresscmd = strapp(". ", tmpbuf));
    }

    if(uncompresscmd[0]){
	if(!idxuncompresscmd)
	  EM__(idxuncompresscmd = strchain(piperprogram, " ", zprogram, " -d | ",
				uncompresscmd, NULL));
    }
    else{
	if(!idxuncompresscmd)
	  EM__(idxuncompresscmd = strapp(zprogram, " -d"));
	uncompresscmd = ".";				/* a dummy */
    }
  }
  if(!compresscmd[0] || !uncompresscmd[0]){
    compressbu = NO;
  }
  if(!idxcompresscmd || !idxuncompresscmd)
    idxuncompresscmd = idxcompresscmd = "";
  if(!idxcompresscmd[0] || !idxuncompresscmd[0])
    idxuncompresscmd = idxcompresscmd = "";
  if(!idxcompresscmd[0] || !idxuncompresscmd[0]){
    compresslogfiles = NO;
  }
  if(!logfile)
    logfile = strdup("");
  massage_string(logfile);
  if(!logfile[0])
    EM__(logfile = strapp(logdir, FN_DIRSEPSTR "backup.log"));
  if(startinfoprog){
    if(empty_string(startinfoprog)){
	ZFREE(startinfoprog);
    }
  }
  if(initprog){
    if(empty_string(initprog) ||
		(mode != MODE_INCR_BACKUP
			&& mode != MODE_FULL_BACKUP)){
	ZFREE(initprog);
    }
  }
  if(exitprog){
    if(empty_string(exitprog) ||
		(mode != MODE_INCR_BACKUP
			&& mode != MODE_FULL_BACKUP)){
	ZFREE(exitprog);
    }
  }
  if(exclude_filename){
    if(empty_string(exclude_filename)){
	ZFREE(exclude_filename);
    }
    else
	massage_string(exclude_filename);
  }
  massage_string(rootdir);
  massage_string(indexfilepart);
  if(identity){
    if(empty_string(identity)){
	ZFREE(identity);
    }
    else
	massage_string(identity);
  }
  if(!identity)
    identity = systemname;

  EEM__(repl_dirs(&piperprogram) || repl_dirs(&logfile)
	|| repl_dirs(&cryptfile) || repl_dirs(&indexfilepart)
	|| repl_dirs(&initprog) || repl_dirs(&exitprog)
	|| repl_dirs(&startinfoprog) || repl_dirs(&lockfile));

  EM__(reportfile = tmp_name(FN_TMPDIR FN_DIRSEPSTR "afbrep"));

  /* check encryption keyfile */
  if(cryptfile && (i = check_cryptfile(cryptfile)) ){
    if(i > 0)
	logmsg(T_("Warning concerning encryption key file `%s': %s.\n"),
			cryptfile, check_cryptfile_msg(i));
    else
	errmsg(T_("Error concerning encryption key file `%s': %s.\n"),
			cryptfile, check_cryptfile_msg(i));

    if(i < 0)
	exit(11);

    logmsg(T_("Warning: Ignoring file `%s', using compiled-in key.\n"),
			cryptfile);
    ZFREE(cryptfile);
  }

  /* determine the lock file pathname if not explicitly specified */
  if(!lockfile){
    for(cpptr = lockdirs; *cpptr; cpptr++){
      i = stat(*cpptr, &statb);
      EM__(cptr = strchain(*cpptr, FN_DIRSEPSTR, DEFAULT_CLIENTLOCKFILE, NULL));
      if(!i){
	if(! eaccess(*cpptr, W_OK) || ! stat(cptr, &statb2)){
	  lockfile = cptr;
	  break;
	}
	else{
	  i = setgid(statb.st_gid);

	  if(! eaccess(*cpptr, W_OK)){
	    lockfile = cptr;
	    break;
	  }
	}
      }
    }
  }
  massage_string(lockfile);

  if(mode == MODE_COPY_TAPE)
    copy_tape(argc, argv);	/* will exec */

  lck = set_lock();
  if(lck != BU_GOT_LOCK){
    /*
     * If we can't get the lock, use the kill(2) call to verify if the
     * pid listed in the lock file (or any process in the same process
     * group -- ie: pipe chain) is still running.  If we can't read the
     * pid, assume that the process exists.
     */
    j = 0;
    pid = -1;
    i = read_pint_file(lockfile, &pid);
    if(!i){
	j = ! kill(pid, 0);
    }
    else{
	j = 1;
    }

    if(j){
	errmsg(T_("Error: An application seems to hold a lock on this functionality.\n"));
	if(pid >= 0){
	  sprintf(tmpbuf, "%d", pid);
	  errmsg(T_("      The process ID is %s.\n"), tmpbuf);
	}
	else{
	  errmsg(T_("      Cannot determine process-ID.\n"));
	}
	if(!isatty(0)){
	  errmsg("\n");
	  errmsg(T_("Cannot ask user via TTY for confirmation to override. Exiting.\n"));
	  exit(39);
	}

	fprintf(stderr, T_("Please check if this process is an obstacle to continue.\n"));
	fprintf(stderr, T_("Do you want to continue anyway (unwanted effects may occur) ? (y/N) "));
	if(wait_for_input(0, 120) < 1){
	  fprintf(stderr, T_("\nTimeout on input, exiting.\n"));
	  exit(40);
	}

	strcpy(tmpbuf, "");
	fgets(tmpbuf, 10, stdin);
	cptr = first_nospace(tmpbuf);
	if(tolower(*cptr) != 'y')
	  exit(99);

	fprintf(stderr, T_("Do you want to steal the lock (otherwise continue without lock) ? (y/N) "));
	if(wait_for_input(0, 120) < 1){
	  fprintf(stderr, T_("\nTimeout on input, exiting.\n"));
	  exit(40);
	}

	strcpy(tmpbuf, "");
	fgets(tmpbuf, 10, stdin);
	cptr = first_nospace(tmpbuf);
	if(tolower(*cptr) != 'y')
	  lck = BU_DONT_LOCK;
    }

    if(lck != BU_DONT_LOCK){
	if(unlink(lockfile)){
	  errmsg(T_("Warning: Cannot remove lockfile `%s'.\n"), lockfile);
	}
	lck = set_lock();
    }
  }

  if(lck != BU_GOT_LOCK && lck != BU_DONT_LOCK){
	fprintf(stderr, T_("Warning: Cannot set lock. Continue anyway ? (y/N) "));
	if(!isatty(0)){
	  fprintf(stderr, T_("\nCannot read confirmation from TTY. Exiting.\n"));
	  exit(38);
	}
	if(wait_for_input(0, 120) < 1){
	  fprintf(stderr, T_("Timeout on input, exiting.\n"));
	  exit(41);
	}

	strcpy(tmpbuf, "");
	fgets(tmpbuf, 10, stdin);
	cptr = first_nospace(tmpbuf);
	if(tolower(*cptr) != 'y')
	  exit(99);
  }

  if(mode == MODE_INCR_BACKUP
		|| mode == MODE_RESTORE || mode == MODE_VERIFYBU
		|| mode == MODE_UPDATE_INDEXES){
    if( (i = read_pint_file(numfile, &num)) )
	cptr = numfile;

    last_idx_num = num;

    if(mode == MODE_RESTORE || mode == MODE_VERIFYBU
				|| mode == MODE_UPDATE_INDEXES){
      if(! restore_em && ! restore_emlist && ! restore_EM){
	if(i){
	  errmsg(T_("Error: Cannot read file `%s'.\n"), numfile);
	  do_exit(9);
	}
      }
    }
    else{		/* MODE_INCR_BACKUP */
      if(eaccess(oldmarkfile, R_OK)){
	i = 1;
	cptr = oldmarkfile;
      }

      if(i){
	errmsg(T_("Warning: Cannot read file `%s', switching to full backup.\n"),
				cptr);
	mode = MODE_FULL_BACKUP;
      }
    }
  }

  part = 1;
  if(numparts > 1){
    read_pint_file(partfile, &part);

    if(mode == MODE_FULL_BACKUP){
      part++;
      if(part > numparts)
	part = 1;
    }
  }

  /* read the number of the backup, if file present */
  if(mode == MODE_FULL_BACKUP){
    if(read_pint_file(numfile, &num))	/* set num to 0 if file absent */
      part = 1;

    last_idx_num = num;

    if(part == 1 && ! keep_counter)
      num++;
  }

  if(daystostoreindexes >= 0.0){	/* set numindexestostore from it */
    time_t	read_time, cmp_time;

    cmp_time = time(NULL) - (time_t)(daystostoreindexes * 86400.0);

    for(i = num - 1; ; i--){
      read_time = (time_t) 0;

      sprintf(tmpbuf, "%d:", (int) i);
      cptr = kfile_get(index_ages_file, tmpbuf, KFILE_LOCKED);

      if(cptr){
	read_time = strint2time(cptr);
	free(cptr);
      }
      else{	/* timestamp not in index ages file -> read index to find the */
	pid = -1;		/* latest timestamp in there, save and use it */
	fd = open_idxfile_read(i, &pid, NULL, YES);
	if(fd >= 0){
	  cptr = NULL;
	  n = 0;
	  fp = fdopen(fd, "r");
	  while(!feof(fp)){
	    if(fget_realloc_str(fp, &cptr, &n))
		continue;

	    if(INDEX_COMMENT(
		eval_index_line(cptr, NULL, NULL, NULL, NULL, NULL, NULL)) &&
				!strncmp(first_nospace(cptr), "##", 2))
	      read_header_line(cptr, NULL, &read_time, NULL, NULL, NULL);
	  }

	  ZFREE(cptr);
	  fclose(fp);
	  if(pid >= 0)
	    waitpid(pid, &pst, 0);

	  if(read_time != (time_t) 0){
	    sprintf(tmpbuf, "%d:", (int) i);
	    time_t_to_intstr(read_time, tmpbuf + 20);

	    kfile_insert(index_ages_file, tmpbuf, tmpbuf + 20,
						KFILE_SORTN | KFILE_LOCKED);
	  }
	}
      }

      if(read_time < cmp_time){
	i = num - 1 - i;

	if(i > numindexestostore)
	  numindexestostore = i;

	break;
      }
    }
  }

  /* GDR: left off here */

  if(mode == MODE_RESTORE)
    restore(argc, argv);		/* will exit */

  save_entry_file = strapp(vardir, FN_DIRSEPSTR SAVE_ENTRIES_FILE);
  level_times_file = strapp(vardir, FN_DIRSEPSTR LEVEL_TIMESTAMPS_FILE);
  needed_tapes_file = strapp(vardir, FN_DIRSEPSTR NEEDED_TAPES_FILE);
  start_pos_file = strapp(vardir, FN_DIRSEPSTR START_POSITIONS_FILE);
  if(!save_entry_file || !level_times_file || !needed_tapes_file
			|| !start_pos_file)
    ENM__;

  if(mode == MODE_UPDATE_INDEXES)
    goto poll_servers;		/* skip a lot of stuff */

  /* change to the root directory */
  if(chdir(rootdir)){
    errmsg(T_("Error: Cannot change to directory `%s'.\n"), rootdir);
    do_exit(7);
  }

  if(mode == MODE_VERIFYBU)
    verifybu(argc, argv);

  if(mode == MODE_INCR_BACKUP && backup_level == MAXINT)
    backup_level = 0;

  if(numparts > 1){
    ParamFileEntry	*dirsparams;
    UChar		**partdirstobackupraw = NULL, *cptr;

    /* collect all stuff, that must go into backup */
    dirsparams = NEWP(ParamFileEntry, numparts);
    partdirstobackupraw = NEWP(UChar *, numparts);
    if(!dirsparams || !partdirstobackupraw)
	ENM__;

    for(i = 0; i < numparts; i++){
	sprintf(tmpbuf, "%d:", (int) (i + 1));

	dirsparams[i].pattern =
			repl_substring_safe(entries[0].pattern, ":", tmpbuf);
	dirsparams[i].entry_ptr = partdirstobackupraw + i;
	dirsparams[i].num_entries = NULL;
	dirsparams[i].type = TypeUCharPTR;

	EM__(partdirstobackupraw[i] = strdup(""));
    }

    i = read_param_file(paramfile, dirsparams, numparts, NULL, NULL);
    if(i){
	errmsg(T_("Error: Cannot read parameter file.\n"));
	do_exit(6);
    }

    ZFREE(alldirstobackupraw);
    EM__(alldirstobackupraw = strdup(""));

    for(i = 0; i < numparts; i++){
	replace_if_pipe(&(partdirstobackupraw[i]), mode, backup_level, part);

	EM__(cptr = strchain(alldirstobackupraw, " ",
					partdirstobackupraw[i], NULL));
	free(alldirstobackupraw);
	alldirstobackupraw = cptr;
	free(partdirstobackupraw[i]);
	free(dirsparams[i].pattern);
    }
    free(partdirstobackupraw);
    free(dirsparams);
  }
  else{
    replace_if_pipe(&dirstobackupraw, mode, backup_level, 0);

    EM__(alldirstobackupraw = strdup(dirstobackupraw));
  }

  ZFREE(dirstobackupraw);
  EM__(dirstobackupraw = strdup(alldirstobackupraw));

  /* if backup is split in pieces, get again the directories to backup */
  if(numparts > 1 && mode == MODE_FULL_BACKUP){
    ParamFileEntry	dirsparam;
    UChar		*partdirstobackupraw = NULL;

    sprintf(tmpbuf, "%d:", part);

    dirsparam.pattern = repl_substring_safe(entries[0].pattern, ":", tmpbuf);
    dirsparam.entry_ptr = &partdirstobackupraw;
    dirsparam.num_entries = NULL;
    dirsparam.type = TypeUCharPTR;

    i = read_param_file(paramfile, &dirsparam, 1, NULL, NULL);
    if(i){
	errmsg(T_("Error: Cannot read parameter file.\n"));
	do_exit(6);
    }
    free(dirsparam.pattern);

    if(partdirstobackupraw){
	ZFREE(dirstobackupraw);

	dirstobackupraw = partdirstobackupraw;

	replace_if_pipe(&dirstobackupraw, mode, backup_level, part);
    }
  }

  if(!eaccess(save_entry_file, R_OK) && !no_default_backup){
    EM__(save_entries = read_asc_file(save_entry_file, NULL));
  }

  if(!eaccess(level_times_file, R_OK)){
    errno = 0;
    level_timespecs = kfile_getall(level_times_file,
				 &num_level_timespecs, KFILE_LOCKED);
    if(!level_timespecs && errno == ENOMEM)
	ENM__;
  }

  /* convert the strings (containing lists) to string arrays */
  if(str2wordsq(&dirstobackup, dirstobackupraw ? dirstobackupraw : ES) < 0
	|| str2wordsq(&dirstoskip, dirstoskipraw ? dirstoskipraw : ES) < 0
	|| str2wordsq(&filestoskip, filestoskipraw ? filestoskipraw : ES) < 0
	|| str2wordsq(&alldirstobackup, alldirstobackupraw ? alldirstobackupraw : ES) < 0)
    ENM__;

  if(fstypesraw){
    while( (cptr = strchr(fstypesraw, ',')) )
	*cptr = ' ';
    for(i = 0; (cptr = strword(fstypesraw, i)); i++){
	if(strchr("+-/", c = cptr[0]))
	  memmove(cptr, cptr + 1, strlen(cptr) * sizeof(UChar));

	if(c == '-')
	  cppptr = &fstypestoskip;
	else if(c == '/')
	  cppptr = &fstypestoprune;
	else
	  cppptr = &fstypestosave;

	j = 0;
	if(*cppptr)
	  for(j = 0, cpptr = *cppptr; *cpptr; j++, cpptr++);
	EM__(*cppptr = ZRENEWP(*cppptr, UChar *, j + 2));
	(*cppptr)[j] = cptr;
	(*cppptr)[j + 1] = NULL;
    }
  }

  dirlists[0] = dirstobackup;
  dirlists[1] = alldirstobackup;

  for(cpptr = filestoskip; *cpptr; cpptr++)
    cleanpath(*cpptr);

  for(cppptr = dirlists; *cppptr; cppptr++){
    for(cpptr = *cppptr; *cpptr; cpptr++){
      if(!strncmp(*cpptr, FILECONTPREFIX, i = strlen(FILECONTPREFIX))){
	cleanpath(*cpptr + i);
      }
      else if(!strncmp(*cpptr, FILECONTZPREFIX, i = strlen(FILECONTZPREFIX))){
	cleanpath(*cpptr + i);
      }
      else if(!strncmp(*cpptr, LOCALDEVPREFIX, i = strlen(LOCALDEVPREFIX))){
	cleanpath(*cpptr + i);
      }
      else{
	cleanpath(*cpptr);
      }
    }

#if 0	/* don't do this: it's dangerous, when paths are symlinked or cross-filesystem */
/* FIXME: work this through for dirstobackup -> *cppptr */
  /* resolve glob patterns */
  EM__(alldirs = NEWP(UChar *, 1));
  *alldirs = NULL;
  num_alldirs = 0;
  for(cpptr = dirstobackup; *cpptr; cpptr++){
    cpptr2 = fnglob(*cpptr);
    if(cpptr2){
      for(n = 0, cpptr3 = cpptr2; *cpptr3; cpptr3++, n++);

      EM__(alldirs = RENEWP(alldirs, UChar *, n + num_alldirs + 1));
      memcpy(alldirs + num_alldirs, cpptr2, sizeof(UChar *) * n);
      alldirs[n + num_alldirs] = NULL;
      num_alldirs += n;

      free(cpptr2);
    }

    free(*cpptr);
  }
  free(dirstobackup);
  dirstobackup = alldirs;
      
  n = num_alldirs;

  {	/* throw out redundant paths (e.g. /home/alb when /home is present) */
    for(i = 0; i < n - 1; i++){
      for(j = i + 1; j < n; j++){
	if(strlen(dirstobackup[i]) < strlen(dirstobackup[j])
		&& !strncmp(dirstobackup[i], dirstobackup[j],
						strlen(dirstobackup[i]))){
	  for(cpptr = dirstobackup + j; *cpptr; cpptr++){
	    *cpptr = *(cpptr + 1);
	  }
	  j--;
	  n--;
	}
	else if(strlen(dirstobackup[j]) < strlen(dirstobackup[i])
		&& !strncmp(dirstobackup[j], dirstobackup[i],
						strlen(dirstobackup[j]))){
	  for(cpptr = dirstobackup + i; *cpptr; cpptr++){
	    *cpptr = *(cpptr + 1);
	  }
	  i--;
	  n--;
	  break;
	}
      }
    }
  }
#endif

    for(cpptr = *cppptr; *cpptr; cpptr++){
      for(cpptr2 = option2prefix; *cpptr2; cpptr2++){	/* substitute options */
	cptr = sscanword(*cpptr2, tmpbuf);		/* with prefix */
	if(!strcmp(tmpbuf, *cpptr)){
	  sscanword(cptr, tmpbuf);
	  free(*cpptr);

	  if(*(cpptr + 1)){
	    EM__(*cpptr = strapp(tmpbuf, *(cpptr + 1)));

	    for(cpptr3 = cpptr + 1; *cpptr3; cpptr3++)
		*(cpptr3) = *(cpptr3 + 1);
	    cpptr--;
	  }
	  else
	    *cpptr = NULL;

	  break;
	}
      }
    }

    for(cpptr = *cppptr, n = 0; *cpptr; cpptr++, n++);

    for(i = 0; i < n - 1; i++){		/* remove duplicate entries */
	for(j = i + 1; j < n; j++){
        if(! strcmp((*cppptr)[j], (*cppptr)[i])){
	  for(cpptr2 = *cppptr + j; *cpptr2; cpptr2++)
	    *(cpptr2) = *(cpptr2 + 1);
	  j--;
	  n--;
	}
      }
    }
  }

  EM__(filecontentstobu = NEWP(UChar *, 1));
  *filecontentstobu = NULL;
  i = 0;
  for(cpptr = dirstobackup; *cpptr; cpptr++){
    if(!strncmp(*cpptr, FILECONTPREFIX, strlen(FILECONTPREFIX))
		|| !strncmp(*cpptr, FILECONTZPREFIX, strlen(FILECONTZPREFIX))
		|| !strncmp(*cpptr, CMDINOUTPREFIX, strlen(CMDINOUTPREFIX))){
	EM__(filecontentstobu = RENEWP(filecontentstobu, UChar *, i + 2));

	filecontentstobu[i] = *cpptr;
	filecontentstobu[i + 1] = NULL;
	if(filecontentstobu[i][0])
	  i++;

	for(cpptr2 = cpptr; *cpptr2; cpptr2++)
	  *(cpptr2) = *(cpptr2 + 1);
	cpptr--;
    }
  }

  if(logfile){
    switch(mode){
     case MODE_FULL_BACKUP:
      if(numparts > 1)
	logmsg(T_("Starting full backup part %d.\n"), part);
      else
	logmsg(T_("Starting %sfull backup.\n"), no_default_backup ?
				T_("customized ") : "");
      break;

     case MODE_INCR_BACKUP:
      logmsg(T_("Starting %sincremental backup.\n"), no_default_backup ?
				T_("customized ") : "");
      break;

     default:
      break;
    }
  }

  /* the timestamp is kept, if not the standard files and directories
   * from the comfiguration file are saved, but something else expli-
   * citely given at the command line.  */
  if(no_default_backup || (mode == MODE_INCR_BACKUP && keep_counter))
    keep_timestamp = YES;

  /* Furthermore, if not set due to the above conditions and if full
   * backup is split in pieces, when saving one of these pieces the
   * timestamp is also kept, if an old timestamp already exists.
   * Reason: If a file is modified/created between an incremental
   * and a following part of the full backup and the timestamp
   * changes then, the file will not be saved before the full save
   * of the piece containing that file. This might be quite a long
   * time later, too long to keep the file unsaved. Thus keeping
   * the timestamp here makes files probably saved several times
   * unnecessarily and wastes space, but this behaviour is safer.
   * If no old timestamp exists, one is created nonetheless. */
  if(mode == MODE_FULL_BACKUP && numparts > 1 && !keep_timestamp){
    keep_timestamp = YES;

    if(eaccess(oldmarkfile, F_OK))	/* no old timestamp there */
	keep_timestamp = NO;		/* force creation of a new one */
  }

  /* before we're gonna really do i. e. modify anything, run init program */
  if(initprog){
    i = system(initprog);
    if(i < 0 || WEXITSTATUS(i)){
	logmsg(T_("Before program returned exit status %d. Stopping here.\n"), i);
	failed_exit(30);
    }
  }

  start_time = time(NULL);

  /* if it's a new backup, create the timestamp-file and save the old one */
  if(part == 1 && mode == MODE_FULL_BACKUP && !keep_timestamp){
    if(!eaccess(oldmarkfile, R_OK)){
      unlink(orgoldmarkfile);
      if(rename(oldmarkfile, orgoldmarkfile)){
	errmsg(T_("Error: Cannot rename file `%s'.\n"), oldmarkfile);
	failed_exit(9);
      }
    }

    fp = fopen(oldmarkfile, "w");
    if(!fp){
      errmsg(T_("Error: Cannot create file `%s'.\n"), oldmarkfile);
      failed_exit(10);
    }
    fclose(fp);
  }

  if(mode == MODE_INCR_BACKUP && !keep_timestamp){
    fp = fopen(newmarkfile, "w");
    if(!fp){
      errmsg(T_("Error: Cannot create file `%s'.\n"), newmarkfile);
      failed_exit(10);
    }
    fclose(fp);
  }

  if(interrupted)
    intrpt_exit();

  /* construct the filename logfile */
  sprintf(tmpbuf, "%d", num);
  EM__(indexfile = strapp(indexfilepart, tmpbuf));

  EM__(zippedidxf = strapp(indexfile, COMPRESS_SUFFIX));
  if(!eaccess(zippedidxf, R_OK) && !eaccess(indexfile, R_OK)){
    stat(indexfile, &statb);
    stat(zippedidxf, &statb2);

    if(statb.st_size == 0){
	unlink(indexfile);
    }
    if(statb2.st_size == 0){
	unlink(zippedidxf);
    }
  }

  unzipfile = hidden_filename(zippedidxf);

	/* If there are both files, this must be the result of a
	 * crash of this program (maybe, of course, due to a
	 * kill -9 or other mean kill-s). So the newer file MUST
	 * be the incomplete one and we can remove it, agree ?
	 */
  if(!eaccess(zippedidxf, R_OK) && !eaccess(indexfile, R_OK)){
    if(statb2.st_mtime > statb.st_mtime){
      unlink(zippedidxf);
    }
    else{
      unlink(indexfile);
    }
  }

  EM__(tmpidxf = strapp(indexfile, TMP_SUFFIX));
  unlink(tmpidxf);
  EM__(tmperrf = strapp(logfile, TMP_SUFFIX));
  unlink(tmperrf);

  infile = outfile = rename_from = rename_to = NULL;
  unzipfilecmd = NULL;
  inunzip = outzip = 0;
  ifd = lfd = ipid = opid = -1;

  if(!eaccess(indexfile, R_OK)){
    infile = (compresslogfiles ? indexfile : NULL);
    inunzip = 0;
  }

  if(!eaccess(zippedidxf, R_OK)){
    inunzip = 1;

    if(compresslogfiles){
	infile = rename_to = zippedidxf;
    }
    else{
	infile = zippedidxf;
    }
		/* check for hidden file containing the unprocesscmd */
    if(!eaccess(unzipfile, R_OK)){
	unzipfilecmd = read_asc_file(unzipfile, 0);
    }
  }

  outzip = (compresslogfiles && idxcompresscmd[0] ? 1 : 0);
  outfile = (outzip ? (inunzip ? tmpidxf : zippedidxf) : indexfile);
  filelist = outfile;

  uncompcmd = (idxuncompresscmd[0] ? idxuncompresscmd : (UChar *) "gunzip");


 poll_servers:
  EM__(server_id_tab = NEWP(ServerIdTab, num_backuphosts));
  memset(server_id_tab, 0, sizeof(ServerIdTab) * num_backuphosts);

  EM__(stst = NEWP(StreamerStat, num_backuphosts));
  memset(stst, 0, num_backuphosts * sizeof(StreamerStat));

  num_server_ids = num_servers = 0;

  /* now we poll the servers, if there are several */
    /* Cause below we are asking the determined server some more things now, */
    /* (get_tapepos) and the 5 seconds interval is then kept, the following */
    /* arguments are no longer valid. So we can open the old and new indexes */
    /* after polling the server. This was done before in earlier versions, */
    /* cause it could have taken notable time, especially > 5 seconds. */
    /* *** start of obsolete argumentation *** */
    /* We do this finally here, cause the program to find a ready backup */
    /* server should run within 5 seconds before the program to really */
    /* make the backup will run, cause then no other client can interfere */
    /* in the meantime (cause of preferred serving of repeated requests */
    /* from the same client within 5 - 6 seconds) */
  forever{
    for(serveridx = 0; serveridx < num_backuphosts; serveridx++){
	stop_msg_proc();

	if(interrupted)
	  break;

	start_msg_proc_msg(backuphosts[serveridx], backupports[serveridx]);

	n = get_streamerstate(stst + serveridx,
			backuphosts[serveridx], backupports[serveridx],
			num_backuphosts == 1);	/* output error if fatal */
	if(n < 0){		/* fatal error occurred, not retryable */
	  memmove(backuphosts + serveridx, backuphosts + serveridx + 1,
			(num_backuphosts - serveridx - 1) * sizeof(*backuphosts));
	  memmove(backupports + serveridx, backupports + serveridx + 1,
			(num_backupports - serveridx - 1) * sizeof(*backupports));
	  memmove(cartsets + serveridx, cartsets + serveridx + 1,
			(num_cartsets - serveridx - 1) * sizeof(*cartsets));
	  serveridx--;
	  num_backuphosts--;
	  num_servers--;
	  if(num_backuphosts <= 0){
	    errmsg(T_("Error: No server can be contacted, fatal: exiting.\n"));
	    failed_exit(36);
	  }
	}
	if(n)
	  continue;

	num_servers = MAX(num_servers, serveridx + 1);

	if(num_server_ids < num_backuphosts){
	  server_id_tab[num_server_ids].servername = backuphosts[serveridx];
	  server_id_tab[num_server_ids].serverport = backupports[serveridx];
	  server_id_tab[num_server_ids].serverid = stst[serveridx].serverid;
	  num_server_ids++;
	}

	if(one_server)
	  break;
	
	i = stst[serveridx].devstatus & STREAMER_STATE_MASK;

	if(i == STREAMER_READY)
	  break;

	if(i == STREAMER_UNLOADED
		&& (stst[serveridx].devstatus & STREAMER_CHANGEABLE)){
	  n = get_tapepos(&i, NULL, NULL,
		newcart ? INSERT_POS_NEWCART : INSERT_POSITION,
		backuphosts[serveridx], backupports[serveridx], NULL);
	  if(n)
	    continue;

	  compose_clntcmd(tmpbuf, NULL, identity, backuphosts[serveridx],
				backupports[serveridx], cartsets[serveridx],
				cryptfile, i, 1, 0, 0, 0, NULL);
	  fd = open_from_pipe(tmpbuf, NULL, 0, &pid);
	  if(fd >= 0){
	    clientpid = pid;
	    waitpid_forced(pid, &pst, 0);

	    close(fd);
	    clientpid = -1;
	    pst = WEXITSTATUS(pst);
	    if(pst == 0)
		break;
	  }
	}
    }

    if(serveridx < num_backuphosts || interrupted || one_server)
	break;

    /* none found up to here, so we take the one with the lowest load */
    i = -1;
    for(serveridx = 0; serveridx < num_backuphosts; serveridx++){
	if(stst[serveridx].serverstatus == OPENED_FOR_WRITE){
	  n = stst[serveridx].numactive + stst[serveridx].numpending;
	  if(i < 0){
	    i = n;
	    j = serveridx;
	  }
	  else{
	    if(n < i){
		i = n;
		j = serveridx;
	    }
	  }
	}
    }

    if(i >= 0){
	serveridx = j;
	break;
    }

    ms_sleep(1000 * 30);
  }

  stop_msg_proc();

		/* update the file containing the found assignments */
		/* of hostname/port-pairs to server-IDs */
  add_server_id_entries(server_ids_file, server_id_tab, num_server_ids);

  for(i = 0; i < num_servers - 1; i++){		/* pack duplicate entries */
    for(j = i + 1; j < num_servers; j++){
	if(!strcmp(stst[i].serverid, stst[j].serverid)){
	  merge_Uns32Ranges(&(stst[i].needed_tapes), stst[j].needed_tapes);
	  free(stst[j].needed_tapes);
	  num_servers--;
	  memmove(stst + j, stst + j + 1, sizeof(stst[0]) * (num_servers - j));
	  j--;
	}
    }
  }
		/* find out, whether tapes have been erased on the server */
  for(i = 0; i < num_servers; i++){
    Uns32Range	*needed_tapes;

    if(!stst[i].needed_tapes)
	continue;

    needed_tapes = get_needed_tapes(needed_tapes_file, stst[i].serverid);
    if(!needed_tapes)
	continue;
					/* determine the difference set */
    needed_tapes = del_range_from_Uns32Ranges(needed_tapes, stst[i].needed_tapes);
    if(len_Uns32Ranges(needed_tapes) > 0){
			/* remove the erased tapes from the needed-tapes-file */
	if(remove_needed_tapes(needed_tapes_file, stst[i].serverid, needed_tapes)){
	  errmsg(T_("Error: Could not remove deleted cartridges from file %s.\n"),
			needed_tapes_file);
	}

	cptr = str_Uns32Ranges(needed_tapes, 0);	/* print message */
	logmsg(T_("Info: The following tapes have been erased on the server with ID %s: %s. The indexes will be updated.\n"),
			stst[i].serverid, cptr);
	free(cptr);
    }
    else
	ZFREE(needed_tapes);

    stst[i].deleted_tapes = needed_tapes;
  }
	/* and remove the deleted tapes from the indexes */
  i = remove_tapes_from_idxes(stst, num_servers, server_ids_file,
				last_idx_num, 0);
  if(mode == MODE_UPDATE_INDEXES)
    exit(ABS(i));

  ZFREE(server_id_tab);

  /* now open index files to copy old on incr/level-X backup */
  if(infile){
    cptr = inunzip ? uncompcmd : NULL;

    if(unzipfilecmd)
	if(unzipfilecmd[0])
	  cptr = unzipfilecmd[0];

    ifd = open_from_pipe_sigblk(cptr, infile, 1, &ipid, &idxproc_blk_sigs);
    if(ifd < 0){
	errmsg(T_("Warning: Cannot read filename logfile `%s'.\n"), infile);
    }
  }
  lfd = open_to_pipe_sigblk(outzip ? idxcompresscmd : NULL, outfile, 1 + 2,
			&opid, 0600, &idxproc_blk_sigs);
  if(lfd < 0){
    errmsg(T_("Error: Cannot write filename logfile `%s'. Logging to stderr.\n"),
					outfile);
    lfd = 2;
    outfile = NULL;
  }

  if(ifd >= 0)
    set_closeonexec(ifd);
  if(lfd >= 0)
    set_closeonexec(lfd);

  used_port = backupports[serveridx] > 0 ? backupports[serveridx] : DEFAULT_PORT;

  SETZERO(scandir_args);

  /* start client side message posting process */
  start_msg_proc_msg(backuphosts[serveridx], msgports[serveridx]);

  /* now start the backup client subprocess */
  i = pipe(ppi) | pipe(ppo) | pipe(ppe);
  if(!i){
    pid = fork_forced();
    if(pid < 0){
	errmsg(T_("Error: Cannot start backup client side subprocess.\n"));
	failed_exit(12);
    }
    if(!pid){		/* child */
	char	**clargv;
	char	portnostr[10], cartsetnostr[10];
	sigset_t	blocked_signals;

	close(ppo[0]);
	close(ppe[0]);
	close(ppi[1]);
	dup2(ppi[0], 0);
	dup2(ppo[1], 1);
	dup2(ppe[1], 2);

	sigemptyset(&blocked_signals);	/* client should terminated cleanly */
	sigaddset(&blocked_signals, SIGINT);
	sigaddset(&blocked_signals, SIGTERM);
	sigaddset(&blocked_signals, SIGABRT);
	sigaddset(&blocked_signals, SIGQUIT);
	sigaddset(&blocked_signals, SIGHUP);
	sigprocmask(SIG_SETMASK, &blocked_signals, NULL);

	EM__(clargv = NEWP(char *, 23 + num_dont_compress * 2));

	cpptr = (UChar **) clargv;
	*(cpptr++) = FN_BASENAME(clientprogram);
	*(cpptr++) = "-QwjcvnlOUE";
	if(newcart){
	  *(cpptr++) = "-G";
	}
	*(cpptr++) = "-h";
	*(cpptr++) = backuphosts[serveridx];
	*(cpptr++) = "-H-";
	if(backupports[serveridx] >= 0){
	  *(cpptr++) = "-p";
	  sprintf(portnostr, "%d", (int) backupports[serveridx]);
	  *(cpptr++) = portnostr;
	}
	if(compressbu){
	  *(cpptr++) = "-z";
	  *(cpptr++) = compresscmd;
	  *(cpptr++) = uncompresscmd;
	}
	if(cryptfile){
	  *(cpptr++) = "-k";
	  *(cpptr++) = cryptfile;
	}
	if(dont_compress){
	  for(cpptr2 = dont_compress; *cpptr2; cpptr2++){
	    *(cpptr++) = "-s";
	    *(cpptr++) = *cpptr2;
	  }
	}
	if(cartsets[serveridx] > 1){
	  sprintf(cartsetnostr, "%d", (int) cartsets[serveridx]);
	  *(cpptr++) = "-S";
	  *(cpptr++) = cartsetnostr;
	}
	if(identity){
	  *(cpptr++) = "-W";
	  *(cpptr++) = identity;
	}
	if(reportfile){
	  if(reportfile[0]){
	    *(cpptr++) = "-V";
	    *(cpptr++) = reportfile;
	  }
	}
	if(chksum){
	  *(cpptr++) = "-Z";
	}
	if(use_ctime){
	  *(cpptr++) = "-K";
	}

	*cpptr = NULL;
	execv(clientprogram, clargv);

	do_exit(24);
    }

    clientpid = pid;	/* for hard interrupt */
  }
  else{
    errmsg(T_("Error: Cannot create pipe to subprocess.\n"));
    failed_exit(13);
  }

  close(ppi[0]);
  close(ppo[1]);
  close(ppe[1]);

  errflp = NO;
			/* now really copy the old index to the new one */
  num_inlines = 0;
  if(ifd >= 0 && lfd >= 0){
    c = '\0';
    while((i = read(ifd, tmpbuf, TMPBUFSIZ - 1)) > 0){
	write_forced(lfd, tmpbuf, i);

	tmpbuf[i] = '\0';	/* count lines */
	cptr = tmpbuf;
	while( (cptr = strchr(cptr, '\n')) ){
	  num_inlines++;
	  cptr++;
	}

	c = tmpbuf[i - 1];	/* remind the very last character */
    }

    close(ifd);

    if(c != '\n'){
	write_forced(lfd, "\n\n", 2);
	errmsg(T_("Warning: Old index file was not newline terminated. Possible inconsistency.\n"));
    }

    if(ipid >= 0){
      waitpid_forced(ipid, &pst, 0);

      if(WEXITSTATUS(pst)){
	errmsg(T_("Warning: Errors occurred reading `%s'.\n"), infile);
      }
    }
  }

  if(waitpid(pid, &pst, WNOHANG) == pid){
    errmsg(T_("Error: Backup client side subprocess unexpectedly died.\n"));
    pipethrough(- ppo[0], - ppo[0]);
    pipethrough(- ppe[0], 2);

    failed_exit(28);
  }

  if(interrupted)
    intrpt_exit();

  ofdflags = fdflags = fcntl(ppo[0], F_GETFL);
  eofdflags = efdflags = fcntl(ppe[0], F_GETFL);

  fdflags |= NONBLOCKING_FLAGS;
  efdflags |= NONBLOCKING_FLAGS;

  fcntl(ppo[0], F_SETFL, fdflags);
  fcntl(ppe[0], F_SETFL, efdflags);

  scandir_args.fd = lfd;
  scandir_args.ctofd = ppi[1];
  scandir_args.cfromfd = ppo[0];
  scandir_args.cerrfd = ppe[0];
  EM__(scandir_args.read_data = NEWP(UChar, 8192));
  scandir_args.read_data_nall = 8192;
  EM__(scandir_args.read_err = NEWP(UChar, 1024));
  scandir_args.read_err_nall = 1024;

  strcpy(server_id, "");
  n = rc = rf = 0;
  forever{
    i = read(ppo[0], scandir_args.read_data + n, 8191 - n);
    if(i < 1){
      wait_for_input(ppo[0], 1);
      i = read(ppo[0], scandir_args.read_data + n, 8191 - n);
    }
    if(i > 0){
	n += i;
	if(n >= 8191)
	  break;
    }
    else{
	if(waitpid(pid, &pst, WNOHANG) == pid){
	  if(n){
	    errmsg(T_("Error: Backup client side did not receive necessary starting information from server. Output was:\n"));
	    errmsg("%s", scandir_args.read_data);
	  }
	  else
	    errmsg(T_("Error: Backup client side subprocess unexpectedly died.\n"));
	  failed_exit(44);
      }
    }

    scandir_args.read_data[n] = '\0';

    rc = rf = 0;	/* parse output of the client to get data for the */
    cptr = scandir_args.read_data;		/* minimum restore info */
    forever{
	cptr2 = strchr(cptr, '\n');
	if(cptr2)
	  *cptr2 = '\0';

	if(re_match(&cartridge_re, cptr, strlen(cptr), 0, NULL) >= 0)
	  rc = trailingint(cptr);
	if(re_match(&file_re, cptr, strlen(cptr), 0, NULL) >= 0)
	  rf = trailingint(cptr);
	if(!strncmp(cptr, "Server-ID:", 10))
	  strcpy(server_id, first_nospace(cptr + 10));
	if(!strcmp(cptr, "--") && rc && rf)
	  n = -1 - n;		/* endmarker */

	if(cptr2)
	  *cptr2 = '\n';

	if(n < 0 || !cptr2)
	  break;

	cptr = cptr2 + 1;
    }

    if((rc && rf && strcmp(server_id, "")) || n < 0){
	startcart = rc;
	startfile = rf;
	break;
    }
  }

  if(!rc || !rf){
	errmsg(T_("Error: Could not determine writing position from client process output. afclient output %swas:\n"),
			n < 0 ? "" : T_("(key strings not found) "));
	cptr = scandir_args.read_data;
	forever{
	  cptr2 = strchr(cptr, '\n');
	  if(cptr2)
	    *cptr2 = '\0';

	  errmsg("  ---> %s.\n", cptr);

	  if(!cptr2)
	    break;

	  cptr = cptr2 + 1;
	}

	kill(pid, SIGKILL);	/* brutally murder afclient */
	close(ppi[1]);
	close(ppo[0]);
	close(ppe[0]);
	waitpid_forced(pid, &pst, 0);
	failed_exit(48);
  }
			/* name for the temporary copy of start_positions */
  EM__(cptr = strapp(vardir, FN_DIRSEPSTR "afbsp_"));
  EM__(tmpstartposfile = tmp_name(FN_TMPDIR FN_DIRSEPSTR "afbsp_"));
  free(cptr);

  lines = NULL;	/* now save the tape write start position information */
  n = 0;
  if(!eaccess(start_pos_file, R_OK)){	/* read startpos file if existing */
    EM__(lines = read_asc_file(start_pos_file, &n));

    for(i = 0; i < n; i++){
	j = sscanf(lines[i], "%d", &i1);	/* throw out dated lines */

	if(j > 0 && i1 <= (int)(num - (numindexestostore + 1))){
	  free(lines[i]);
	  memmove(lines + i, lines + i + 1, sizeof(UChar *) * (n - i));
	  i--;
	  n--;
	}
    }
  }

  EM__(lines = ZRENEWP(lines, UChar *, n + 2));	/* add a new line */

  sprintf(tmpbuf, "%d.%d/%d: %s %d %d %d",
			(int) num, (int) part, (int) numparts,
			backuphosts[serveridx], (int) used_port,
			(int) startcart, (int) startfile);
  EM__(lines[n] = strdup(tmpbuf));
  lines[n + 1] = NULL;

  if(write_asc_file_safe(start_pos_file, lines, 0)){	/* save the file */
    errmsg(T_("Error: Cannot write file `%s' to save the backup start positions.\n"),
				start_pos_file);
  }
  free_asc_file(lines, 0);
				/* create a copy with unique name */
  if(copy_file(start_pos_file, tmpstartposfile)){
    errmsg(T_("Warning: Cannot make copy of file `%s', saving original.\n"),
			start_pos_file);
    free(tmpstartposfile);

    tmpstartposfile = start_pos_file;
  }
				/* construct the minimum restore info */
  sprintwordq(tmpbuf, identity);
  i = strlen(EMREC_PREFIX) + strlen(tmpbuf) + strlen(backuphosts[serveridx])
					+ 50 + strlen(tmpstartposfile);
  EM__(minrestoreinfo = NEWP(UChar, i));
  sprintf(minrestoreinfo, "%s %s %s %d %d %d %s", EMREC_PREFIX,
			tmpbuf, backuphosts[serveridx], (int) used_port,
			(int) startcart, (int) startfile, tmpstartposfile);

		/* construct the header information, first for the index */
  cptr = tmpbuf;
  if(mode == MODE_INCR_BACKUP || (mode == MODE_FULL_BACKUP && part > 1))
    *(cptr++) = '\n';

  sprintf(cptr, "%s%s%d%s%d.%d:\n",
		backuphosts[serveridx], PORTSEP, (int) used_port, LOCSEP,
		(int) startcart, (int) startfile);
  cptr += strlen(cptr);
  sprintf(cptr, "## %s backup at %s, current number: %d, hostname: %s client id: ",
		mode == MODE_INCR_BACKUP ? "Incremental" : "Full",
		(char *) actimestr(), (int) num, (char *) systemname);
  sprintwordq(cptr + strlen(cptr), identity);
  strcat(cptr, "\n");

  write_forced(lfd, tmpbuf, strlen(tmpbuf));	/* write it to the index */

		/* construct header data including minimum restore info */
  EM__(cptr2 = mk_esc_seq(cptr - 1, ESCAPE_CHARACTER, NULL));
				/* -1 cause a leading newline is required */
  cptr = actimestr();
  EM__(header = NEWP(UChar, 3 * (strlen(EMREC_TAPE_PREFIX) + 50 + strlen(cptr)
		+ strlen(minrestoreinfo) + strlen(EMREC_POSTFIX))
		* (1 + strlen("\\b")) + strlen(cptr2)));
  strcpy(header, "");

  sprintf(tmpbuf, "%s %s %s", cptr, minrestoreinfo, EMREC_POSTFIX);
  for(i = 0; i < 3; i++){
    sprintf(header + strlen(header), "%s %d %d %s\\n",
			EMREC_TAPE_PREFIX, (int) strlen(tmpbuf),
			(int) parity_byte(tmpbuf, strlen(tmpbuf)), tmpbuf);
  }

  EM__(cptr = strdup(header));
  repl_esc_seq(cptr, ESCAPE_CHARACTER);
  n = strlen(cptr);
  free(cptr);

  for(i = 0; i < n; i++)
    strcat(header, "\\b");
  strcat(header, cptr2);
  free(cptr2);

  write_forced(ppi[1], header, strlen(header));	/* write the header to the */
  write_forced(ppi[1], "\n", 1);	/* client program, then write the */
				/* filename to the client program to save it */
  EM__(cptr = strapp(NOVERIFYPREFIX, tmpstartposfile));
  success = (success &&
	(! write_filename(cptr, (void *) (& scandir_args), NULL)));
  free(cptr);

  cmp_time = (time_t) 0;

  if(mode == MODE_INCR_BACKUP){
    stat(oldmarkfile, &statb);
    cmp_time = statb.st_mtime;
  }

  if(backup_level > 0){		/* backup level is given (or full_backup) */
    if(level_timespecs){	/* if we have read a file with timestamps */
	long int	l;
	time_t		rtime, latest_time = 0;

	for(i = 0; i < num_level_timespecs; i++){	/* search the entry */
	  if(sscanf(level_timespecs[i].key, "%ld", &l) < 1)
	    continue;	/* with level >= given and the latest timestamp */

	  if(l >= backup_level){
	    rtime = time_from_datestr(level_timespecs[i].value);
	    if(rtime == UNSPECIFIED_TIME){
		errmsg(T_("Warning: Format error in level timestamps file. Ignoring Level %ld.\n"), l);
		continue;
	    }

	    if(rtime > latest_time)
		latest_time = rtime;
	  }
	}

	if(latest_time > 0)
	  cmp_time = latest_time;

	kfile_freeall(level_timespecs, num_level_timespecs);
    }

    if(backup_level == MAXINT)
	cmp_time = 0;
  }

  total_volume = current_volume = 0.0;
  got_total_volume = NO;

  SETZERO(findparams);
  findparams.excl_dirs = dirstoskip;
  findparams.excl_names = filestoskip;
  findparams.errfunc = err_write_filename;
  findparams.errfunc_param = (void *) (& scandir_args);
  findparams.excl_filename = exclude_filename;
  findparams.newer_than = cmp_time;
  findparams.options = FIND_DEPTH;
  findparams.interrupted = &interrupted;
  if(use_ctime)
    findparams.options |= FIND_CMP_CTIME;
  findparams.fstypes_to_search = fstypestosave;
  findparams.fstypes_to_skip = fstypestoskip;
  findparams.fstypes_to_prune = fstypestoprune;

  forever{	/* pseudo-loop */
    dryrun = (progress && !got_total_volume);

    dirsarr[1] = NULL;

    for(cpptr = dirstobackup; *cpptr; cpptr++){
      dirsarr[0] = *cpptr;

      if(!strncmp(dirsarr[0], LOCALDEVPREFIX, i = strlen(LOCALDEVPREFIX))){
	findparams.options |= FIND_LOCAL_DEV;
	dirsarr[0] += i;
      }
      else{
	findparams.options &= ~FIND_LOCAL_DEV;
      }

      if(!dirsarr[0][0])
	continue;

      saved_newer_time = findparams.newer_than;

      if(save_entries && mode == MODE_INCR_BACKUP){
	i = 0;
	for(cpptr2 = save_entries; *cpptr2; cpptr2++)
	  if(!strcmp(*cpptr, *cpptr2))
	    i = 1;

	if(!i){
	  findparams.newer_than = (time_t) 0;
	  if(!dryrun)
	    logmsg(T_("Info: New entry, making full backup of `%s'.\n"), *cpptr);
	}
      }

      if(!dryrun){
	i = find(dirsarr, &findparams, write_filename, (void *) (& scandir_args));
      }
      else{	/* only sum up the total size */
	i = find(dirsarr, &findparams, calc_total_volume,
					(void *) (&total_volume));
      }

      findparams.newer_than = saved_newer_time;

      cpptr3 = fnglob(dirsarr[0]);
      if(cpptr3){
	if(! *cpptr3)
	  ZFREE(cpptr3);
      }
      if(!cpptr3){
	if(!dryrun)
	  logmsg(T_("Warning: Entry `%s' did not resolve any filename.\n"),
			dirsarr[0]);
      }
      else
	free_array(cpptr3, 0);

      success = (success && (! i));

      if(interrupted)
	break;
    }

    if(interrupted)
    	break;

    if(dryrun){
	got_total_volume = YES;
	if(total_volume == 0.0)
	  total_volume = 1.0;	/* just in case, avoid division by 0 */

	write_progress(current_volume, total_volume);

	continue;			/* now do the real backup */
    }

    i = 0;
    for(cpptr = filecontentstobu; *cpptr; cpptr++){
      if(! cpptr[0][0])
	continue;

      i |= write_filename(*cpptr, (void *) (& scandir_args), NULL);

      if(interrupted)
	break;
    }

    success = (success && (! i));

    break;
  }

  close(ppi[1]);

  fcntl(ppo[0], F_SETFL, ofdflags);
  fcntl(ppe[0], F_SETFL, eofdflags);

  i = write_filename(NULL, (void *) (& scandir_args), NULL);
  success = (success && (!i));

  if(progress){
    if(current_volume == 0.0)
	current_volume = 1.0;	/* just in case, avoid division by 0 */

    write_progress(current_volume, current_volume);
  }

  waitpid_forced(pid, &pst, 0);
  errflp |= WEXITSTATUS(pst);
  close(ppo[0]);
  close(ppe[0]);
  clientpid = -1;

  close(lfd);
  if(opid >= 0){
    waitpid_forced(opid, &pst, 0);
    errflp |= WEXITSTATUS(pst);
  }

  if(logf_lost || errflp)	/* This happens, when the child dies */
    must_read_fnamlog = YES;

  if(must_read_fnamlog){
    num_outlines = 0;
    if(inunzip || outzip){
      ipid = -1;			/* count written lines */
      cptr = outzip ? (idxuncompresscmd[0] ? idxuncompresscmd : (UChar *) "gunzip") : NULL;
      filename = (inunzip && outzip) ? tmpidxf : outfile;
      i = stat(filename, &statb);
      if(i){
	errmsg(T_("Warning: Cannot read filename logfile `%s'.\n"), filename);
      }
      if(!i && statb.st_size > 0){
	ifd = open_from_pipe_sigblk(cptr, filename, 1,
					&ipid, &idxproc_blk_sigs);
	if(ifd < 0){
	  errmsg(T_("Warning: Cannot read filename logfile `%s'.\n"), filename);
	}
	else{
	  fp = fdopen(ifd, "r");

	  while(!feof(fp)){
	    cptr = fget_alloc_str(fp);
	    if(!cptr)
		continue;

	    num_outlines++;

	    if(num_outlines > num_inlines &&
			INDEX_UNRECOGNIZED(eval_index_line(cptr,
				NULL, NULL, NULL, NULL, NULL, NULL))){
		fprintf(stderr, "%s", cptr);
	    }			/* print new error messages in log */

	    free(cptr);
	  }

	  fclose(fp);
	  if(ipid >= 0){
	    waitpid_forced(ipid, &pst, 0);
	    if(WEXITSTATUS(pst)){
		errmsg(T_("Warning: Cannot find out number of lines written to `%s'.\n"),
				filename);
		num_outlines = 0;
	    }
	  }
	}
      }
    }

    if(num_outlines > num_inlines){	/* That's what it's all about: */
	logf_lost = NO;			/* new file is longer, so nothing */
    }					/* is lost. */
  }

  if(logf_lost){
    filelist = infile;

    if(inunzip && outzip){
	unlink(tmpidxf);
    }
    else{
      if(inunzip || outzip)
	unlink(outfile);
    }
  }
  else{
    if(inunzip && outzip){
	unlink(zippedidxf);
	rename(tmpidxf, zippedidxf);
	filelist = zippedidxf;
    }
    else{
      if(inunzip || outzip)
	unlink(infile);
    }

    if(outzip){
	free_asc_file(unzipfilecmd, 0);
	EM__(unzipfilecmd = NEWP(UChar *, 2));
	unzipfilecmd[0] = uncompcmd;
	unzipfilecmd[1] = NULL;

	write_asc_file_safe(unzipfile, unzipfilecmd, 1);
    }
  }

  if(interrupted)
    intrpt_exit();

  if(! success){
    errmsg(T_("Error: An error occurred during backup.\n"));
    errmsg(T_("The logfile `%s' should tell more details.\n"), logfile);
    failed_exit(14);
  }

  if(!success)
    failed_exit(15);
				/* now we know, that everything worked fine */

  if(strcmp(start_pos_file, tmpstartposfile)){
    unlink(tmpstartposfile);

    free(tmpstartposfile);
  }

  if(startinfoprog){
    fp = popen(startinfoprog, "w");
    fprintf(fp, "%s\n", minrestoreinfo);
    pclose(fp);
  }

  sprintf(tmpbuf, "%d:", (int) num);
  time_t_to_intstr(time(NULL), tmpbuf + 20);
  if(kfile_insert(index_ages_file, tmpbuf, tmpbuf + 20,
				KFILE_SORTN | KFILE_LOCKED)){
    errmsg(T_("Error: Could not update index ages file `%s'.\n"),
			index_ages_file);
  }

  if(mode == MODE_FULL_BACKUP){
    /* write the backup part number to file */
    if(numparts > 1){
      if(write_uns_file(partfile, part)){
	errmsg(T_("Error: Cannot write to file `%s'.\n"), partfile);
	failed_exit(16);
      }

      logmsg(T_("Full backup part %d finished.\n"), part);
    }
    else{
      logmsg(T_("Full backup %sfinished.\n"), no_default_backup ?
				T_("(customized) ") : "");
    }

    /* write the backup number to file */
    if(part == 1){
      if(write_uns_file(numfile, num)){
	errmsg(T_("Error: Cannot write to file `%s'.\n"), numfile);
	failed_exit(8);
      }
    }

    if(!keep_timestamp)
	unlink(orgoldmarkfile);

    /* remove dated filename logfiles */
    if(numindexestostore >= 0){
      n = num - (numindexestostore + 1);

      i = strlen(indexfile) + 30;
      if(i < TMPBUFSIZ)
	cptr = tmpbuf;
      else
	cptr = NEWP(UChar, i);
      j = 1;		/* flag: more lines to be removed in index ages */
      if(cptr){
	while(n > 0){
	  sprintf(cptr, "%s%d", indexfilepart, (int) n);
	  unlink(cptr);
	  strcat(cptr, COMPRESS_SUFFIX);
	  unlink(cptr);
	  filename = hidden_filename(cptr);
	  if(filename){
	    unlink(filename);
	    free(filename);
	  }

	  if(j){
	    sprintf(tmpbuf, "%d:", (int) n);
	    cptr2 = kfile_get(index_ages_file, tmpbuf, KFILE_LOCKED);
	    if(cptr2){
		kfile_delete(index_ages_file, tmpbuf, KFILE_LOCKED);
		free(cptr2);
	    }
	    else{
		j = 0;
	    }
	  }

	  n--;
	}
      }
    }
  }

  if(mode == MODE_INCR_BACKUP){
    if(!keep_timestamp){
      unlink(oldmarkfile);
      rename(newmarkfile, oldmarkfile);
    }

    logmsg(T_("Incremental backup %sfinished.\n"), no_default_backup ?
				T_("(customized) ") : "");
  }

  if(!keep_timestamp && backup_level > 0){
    sprintf(kbuf, "%ld:", (long int) backup_level);
    sprintf(vbuf, "%s", ctime(&start_time));
    massage_string(vbuf);

    if(kfile_insert(level_times_file, kbuf, vbuf, KFILE_SORTN | KFILE_LOCKED))
	errmsg(T_("Error: Cannot write backup level timestamp file `%s'.\n"),
						level_times_file);
  }

  effective_backup_level = backup_level;
  remove_previous_equal_level = (remove_lower_levels && keep_timestamp);

  if(no_default_backup)
    effective_backup_level = MAXINT;

  if(no_default_backup || mode != MODE_INCR_BACKUP){
    remove_lower_levels = NO;		/* like full_backup: never remove */
    remove_previous_equal_level = NO;		/* entries in needed_tapes */
  }

  if(!no_default_backup){
    fp = fopen(save_entry_file, "w");
    if(fp){
      for(cpptr = alldirstobackup; *cpptr; cpptr++)
	fprintf(fp, "%s\n", *cpptr);
      fclose(fp);
    }
    else{
	errmsg(T_("Warning: Cannot save entries to file `%s'.\n"),
			save_entry_file);
    }
  }

  if(!eaccess(needed_tapes_file, R_OK)){	/* save crucial tapes  */
    EM__(needed_tapes_lines = read_asc_file(needed_tapes_file,
					&num_needed_tapes_lines));

    if(remove_lower_levels)
	needed_tapes_before = get_needed_tapes(needed_tapes_file, server_id);
  }

  cpptr = lines = NULL;
  if(!eaccess(reportfile, R_OK)){
    EM__(lines = read_asc_file(reportfile, NULL));
    for(cpptr = lines; *cpptr; cpptr++){
      cptr = T_("Used Cartridges:");
      if(!strncasecmp(*cpptr, cptr, strlen(cptr)))
	break;
    }

    if(!*cpptr)
	cpptr = NULL;
  }

  if(cpptr){
    EM__(cptr = strword(*cpptr, -1));
    EM__(new_tape_ranges = sscan_Uns32Ranges(cptr, 0, 0, NULL, NULL));
    free(cptr);

    if(server_id[0]){
	for(i = 0; i < num_servers; i++)
	  if(!strcmp(server_id, stst[i].serverid))
	    break;
    }

    if(needed_tapes_lines){
      for(i = 0; i < num_needed_tapes_lines; i++){
	n = sscanf(needed_tapes_lines[i], "%d", &i1);

	if(n > 0){
	  if(i1 == (int) num)
	    break;

	  if(i1 <= (int)(num - (numindexestostore + 1))){
	    free(needed_tapes_lines[i]);
	    memmove(needed_tapes_lines + i, needed_tapes_lines + i + 1,
			sizeof(UChar *) * (num_needed_tapes_lines - i));
	    num_needed_tapes_lines--;
	    i--;
	  }
	}
      }
    }

    cptr = str_Uns32Ranges(new_tape_ranges, 0);
    free(new_tape_ranges);
    sprintf(tmpbuf, " %d>%s", (int) effective_backup_level, cptr);
    free(cptr);
    if(server_id[0]){
	strcat(tmpbuf, "@");
	strcat(tmpbuf, server_id);
    }
    EM__(cptr = strdup(tmpbuf));
    sprintf(tmpbuf, "%d:", (int) num);

    if(needed_tapes_lines && i < num_needed_tapes_lines){
	n = i;
	EM__(cptr2 = strapp(needed_tapes_lines[n], cptr));
	free(needed_tapes_lines[n]);
	needed_tapes_lines[n] = cptr2;
    }
    else{
	EM__(needed_tapes_lines = ZRENEWP(needed_tapes_lines, UChar *,
					num_needed_tapes_lines + 2));
	n = num_needed_tapes_lines++;
	EM__(needed_tapes_lines[n] = strapp(tmpbuf, cptr));
	needed_tapes_lines[num_needed_tapes_lines] = NULL;
    }
    free(cptr);
    massage_string(needed_tapes_lines[n]);

			/* throw out unneeded levels lower than current */
    if(remove_lower_levels){
      for(cptr = first_nospace(first_space(needed_tapes_lines[n]));
						*cptr; cptr = cptr2){
	i = sscanf(cptr, "%d", &i1);			/* read level */
	if(i < 1)
	  break;

	cptr2 = first_nospace(first_space(cptr));	/* next word pos */

	if(i1 < effective_backup_level){	/* level lower than current */
	  if(*cptr2){				/* if there are more words */
	    memmove(cptr, cptr2, sizeof(UChar) * (strlen(cptr2) + 1));
	    cptr2 = cptr;
	  }				/* copy then over the current word */
	  else{
	    *cptr = '\0';		/* otherwise terminate the string */
	    break;
	  }
	}
      }
    }

    if(remove_previous_equal_level && word_count(needed_tapes_lines[n]) >= 2){
      cptr = needed_tapes_lines[n] + strlen(needed_tapes_lines[n]) - 1;
      cptr = word_start(cptr, 0);	/* points to begin of last field */
      cptr2 = word_end(cptr, -1);
      while(!isspace(*cptr2) && cptr2 > needed_tapes_lines[n])
	cptr2--;
      if(isspace(*cptr2))
	cptr2++;		/* points to begin of field before last */
      if(sscanf(cptr2, "%d", &i1) > 0){
	i = i1;
	if(sscanf(cptr, "%d", &i1) > 0){
	  if(i == (Int32) i1){
	    memmove(cptr2, cptr, (strlen(cptr) + 1) * sizeof(UChar));
	  }
	}
      }
    }

    EM__(new_needed_tapes_file = tmp_name(needed_tapes_file));

    if(write_asc_file_safe(new_needed_tapes_file, needed_tapes_lines, 0))
	errmsg(T_("Error: Could not write file `%s' with the crucial tape numbers.\n"),
				new_needed_tapes_file);

    if(!stat(needed_tapes_file, &statb)){
	chown(new_needed_tapes_file, statb.st_uid, statb.st_gid);
	chmod(new_needed_tapes_file, statb.st_mode);
    }

    all_tape_ranges = get_needed_tapes(new_needed_tapes_file, server_id);

    if(all_tape_ranges){
	if(needed_tapes_before){
	  needed_tapes_before = del_range_from_Uns32Ranges(needed_tapes_before,
					all_tape_ranges);
	  if(needed_tapes_before){
	    if(num_Uns32Ranges(needed_tapes_before) > 0){
		StreamerStat	strstat;

		strstat.deleted_tapes = needed_tapes_before;
		strcpy(strstat.serverid, server_id);
		remove_tapes_from_idxes(&strstat, 1, server_ids_file,
				last_idx_num, last_idx_num);
	    }
	  }
	}

	EM__(crucial_tapes_str = str_Uns32Ranges(all_tape_ranges, 0));
	free(all_tape_ranges);
    }
  }
  else{
    errmsg(T_("Error: Evaluating report output to obtain crucial tapes failed.\n"));
  }

  free(stst);

  free_asc_file(lines, 0);
  free_asc_file(needed_tapes_lines, 0);

  if(crucial_tapes_str){
    /* send the crucial tapes to the server using a server message */
    compose_clntcmd(tmpbuf, "", identity, backuphosts[serveridx],
				backupports[serveridx], cartsets[serveridx],
				cryptfile, 0, 0, 0, 0, 0, NULL);
    strcpy(tmpbuf + strlen(tmpbuf), " -M \"" PRECIOUS_TAPES_MESSAGE);
    if( (i = word_count(identity)) > 1)
	strcat(tmpbuf, "\\\"");
    strcat(tmpbuf, identity);
    if(i > 1)
	strcat(tmpbuf, "\\\"");
    sprintf(tmpbuf + strlen(tmpbuf), " %s\"", crucial_tapes_str);
    
    free(crucial_tapes_str);

    ifd = open_to_pipe(tmpbuf, NULL, 1 + 2, &pid, 0);
    clientpid = pid;
    close(ifd);

    waitpid_forced(pid, &pst, 0);
    clientpid = -1;
    if( (i = WEXITSTATUS(pst)) ){
	errmsg(T_("Error: program informing the server about the crucial tapes returned status %d.\n"),
		(int) i);
    }
    else{
	if(rename(new_needed_tapes_file, needed_tapes_file)){
	  errmsg(T_("Error: Cannot rename `%s' to `%s' to save the crucial tapes.\n"),
			new_needed_tapes_file, needed_tapes_file);
	}
    }

    unlink(new_needed_tapes_file);
    ZFREE(new_needed_tapes_file);
  }

  stop_msg_proc();

  if(errflp){
    errmsg(T_("Warning: Minor errors occurred during backup. See the logfile `%s' for more details.\n"), logfile);
  }

  do_exit(0);
}

static Int32
set_indexfilename(UChar * idxfile)
{
  ZFREE(indexfile);

  if(idxfile)
    EM__(indexfile = strdup(idxfile));

  return(0);
}

static int
open_idxfile_read(Int32 idxnum, int * pid, UChar ** opened_file, Flag quiet)
{
  UChar		*unzipfile = NULL, *unzipcmd, *of = NULL;
  UChar		*idxfile = NULL, *zidxfile = NULL;
  UChar		**unzipfilecont = NULL, numbuf[32];
  int		fd, lpid;

  sprintf(numbuf, "%d", (int) idxnum);
  EM__(idxfile = strapp(indexfilepart, numbuf));

  lpid = fd = -1;
  if(!eaccess(idxfile, R_OK)){
    fd = open(idxfile, O_RDONLY);

    if(fd >= 0)
	of = idxfile;
  }
  else{
    EM__(zidxfile = strapp(idxfile, COMPRESS_SUFFIX));

    if(eaccess(zidxfile, R_OK)){
      if(!quiet)
	errmsg(T_("Error: No filename logfile found.\n"));
    }
    else{
	unzipcmd = idxuncompresscmd[0] ? idxuncompresscmd : (UChar *) "gunzip";
	unzipfile = hidden_filename(zidxfile);

	if(!eaccess(unzipfile, R_OK)){
	  unzipfilecont = read_asc_file(unzipfile, 0);
	  if(unzipfilecont)
	    if(unzipfilecont[0])
		unzipcmd = unzipfilecont[0];
	}

	fd = open_from_pipe_sigblk(unzipcmd, zidxfile, 1,
				&lpid, &idxproc_blk_sigs);
	if(fd >= 0)
	  of = zidxfile;
    }
  }

  if(fd >= 0 && lpid >= 0)
    *pid = lpid;

  set_indexfilename(idxfile);

  if(of && opened_file)
    EM__(*opened_file = strdup(of));

  ZFREE(idxfile);
  ZFREE(zidxfile);

  free_asc_file(unzipfilecont, 0);
  ZFREE(unzipfile);

  return(fd);
}

static void
compose_clntcmd(
  UChar		*cbuf,
  UChar		*flags,
  UChar		*id,
  UChar		*backup_host,
  Int32		backup_port,
  Int32		cart_set,
  UChar		*keyfile,
  Int32		cartn,
  Int32		filen,
  time_t	time_older,
  time_t	time_newer,
  Int32		user_id,
  UChar		*restore_root)
{
  strcpy(cbuf, clientprogram);

  if(flags){
    if(flags[0]){
	strcat(cbuf, " -");
	strcat(cbuf, flags);
    }
  }

  if(!backup_host)
    backup_host = backuphosts[0];	/* use first server as default */

  strcat(cbuf, " -h ");
  strcat(cbuf, backup_host);

  if(backup_port > 0)			/* a port number must be > 0 */
    sprintf(cbuf + strlen(cbuf), " -p %d", (int) backup_port);

  if(keyfile)
    sprintf(cbuf + strlen(cbuf), " -k %s", keyfile);

  if(cart_set > 1)			/* 1 is default */
    sprintf(cbuf + strlen(cbuf), " -S %d", (int) cart_set);

  if(cartn > 0)
    sprintf(cbuf + strlen(cbuf), " -C %d", (int) cartn);

  if(filen > 0)
    sprintf(cbuf + strlen(cbuf), " -F %d", (int) filen);

  if(user_id != 0)
    sprintf(cbuf + strlen(cbuf), " -o %d", (int) user_id);

  if(id)
    sprintf(cbuf + strlen(cbuf), " -W %s", (char *) id);

  if(restore_root)
    strcat(cbuf, " -r");

  if(time_older > 0){
    strcat(cbuf, " -B ");
    time_t_to_intstr(time_older, cbuf + strlen(cbuf));
  }

  if(time_newer > 0){
    strcat(cbuf, " -A ");
    time_t_to_intstr(time_newer, cbuf + strlen(cbuf));
  }
}

Int32
get_streamerstate(
  StreamerStat	*state,
  UChar		*server,
  Int32		port,
  Flag		perrs)
{
  UChar		cbuf[1000], *cptr, *cptr2, *cptr3, *sstr, tmpst;
  Flag		wr = NO, rd = NO, pd = NO, have_statusline = NO;
  Int32		r = 0;
  int		fd, pid, pst, j, k, n;

  if(!server)
    server = backuphosts[serveridx];
  if(port <= 0)
    port = backupports[serveridx];

  SETZERO(*state);
  compose_clntcmd(cbuf, "wv", identity, server, port,
			0, cryptfile, 0, 0, 0, 0, 0, NULL);

  fd = open_from_pipe(cbuf, NULL, 1 + 2, &pid);
  if(fd >= 0){
    clientpid = pid;

    n = read_forced(fd, cbuf, 999);
    n = (n > 0 ? n : 0);
    cbuf[n] = '\0';

    waitpid_forced(pid, &pst, 0);
    clientpid = -1;
    close(fd);
    pst = WEXITSTATUS(pst);
    if(pst){
	if(pst >= 50 && pst < 100)
	  GETOUTR(1);

	if(perrs)
	  errmsg("%s", cbuf);

	GETOUTR(-1);
    }
    else{
	cptr = cbuf;
	forever{
	  if( (cptr2 = strchr(cptr, '\n')) )
	    *cptr2 = '\0';
	  if(strstr(cptr, T_("Streamer state:"))){
	    EM__(cptr = sstr = strword(cptr, -1));

	    forever{
		if( (cptr3 = strchr(cptr, '+')) )
		  *cptr3 = '\0';
		tmpst = (!strcasecmp(cptr, "BUSY") ? STREAMER_BUSY : (
			!strcasecmp(cptr, "READY") ? STREAMER_READY : (
			!strcasecmp(cptr, "UNLOADED") ? STREAMER_UNLOADED : (
			!strcasecmp(cptr, "DEVINUSE") ? STREAMER_DEVINUSE : (
			!strcasecmp(cptr, "UNAVAILABLE") ? STREAMER_UNAVAIL : (
			!strcasecmp(cptr, "CHANGEABLE") ? STREAMER_CHANGEABLE :
				0))))));
		if(tmpst){
		  state->devstatus |= tmpst;
		}
		else{
		  if((wr = !strncasecmp(cptr, "WRITING", 7))
			|| (rd = !strncasecmp(cptr, "READING", 7))
			|| (pd = !strncasecmp(cptr, "PENDING", 7))){
		    j = sscanf(cptr + 7, "=%d", &k);
		    if(j > 0){
			if(wr){
			  state->serverstatus = OPENED_FOR_WRITE;
			  state->numactive += k;
			}
			if(rd){
			  state->serverstatus = OPENED_FOR_READ;
			  state->numactive += k;
			}
			if(pd){
			  state->numpending += k;
			}
		    }
		  }
		  else if(!strcasecmp(cptr, "RAWWRITING")){
		    state->serverstatus = OPENED_FOR_WRITE | OPENED_RAW_ACCESS;
		  }
		  else if(!strcasecmp(cptr, "RAWREADING")){
		    state->serverstatus = OPENED_FOR_READ | OPENED_RAW_ACCESS;
		  }
		}  
		if(!cptr3)
		  break;
		cptr = cptr3 + 1;
	    }
	    have_statusline = YES;
	  }
	  else if(strstr(cptr, "Server-ID:")){
	    strcpy(state->serverid, first_nospace(cptr + strlen("Server-ID:")));
	  }
	  else if(strstr(cptr, "Precious tapes:")){
	    state->needed_tapes = sscan_Uns32Ranges(
			first_nospace(cptr + strlen("Precious tapes:")),
			1, 0xfffffff /* dummy, no open range appears */,
			NULL, NULL);
	  }

	  if(!cptr2)
	    break;
	  cptr = cptr2 + 1;
	}
    }
  }

  if(!state->devstatus)
    state->devstatus = STREAMER_UNKNOWN;

  if(!have_statusline)
    GETOUTR(-1);

  return(0);

 getout:
  ZFREE(state->needed_tapes);

  return(r);
}

Int32
get_tapepos(
  Int32		*n_cart,
  Int32		*n_file,
  Int32		*n_carts,
  Int8		mode,
  UChar		*server,
  Int32		port,
  UChar		*serverid)
{
  UChar		cbuf[1000], *cptr, *cptr2;
  int		fd, pid, pst, i, n;
  long int	rcart = 0, rfile = 0, rncarts = 0;
  Flag		next_cart = NO;

  if(mode == INSERT_POS_NEWCART){
    next_cart = YES;
    mode = INSERT_POSITION;
 }

  if(!server)
    server = backuphosts[serveridx];
  if(port <= 0)
    port = backupports[serveridx];

  if(serverid)
    *serverid = '\0';

  compose_clntcmd(cbuf, NULL, identity, server, port,
			cartsets[serveridx], cryptfile, 0, 0, 0, 0, 0, NULL);

  sprintf(cbuf + strlen(cbuf), " -%s%s%s%s",
		mode == INSERT_POSITION ? "Q" : "q",
		(n_carts || serverid) ? "v" : "", serverid ? "w" : "",
		next_cart ? "G" : "");

  fd = open_from_pipe(cbuf, NULL, 1 + 2, &pid);
  if(fd >= 0){
    clientpid = pid;

    for(n = 0; (i = read(fd, cbuf + n, 999 - n)) > 0; n += (i > 0 ? i : 0));
    cbuf[n] = '\0';

    waitpid_forced(pid, &pst, 0);
    clientpid = -1;
    close(fd);
    pst = WEXITSTATUS(pst);
    if(pst){
	if(pst >= 50 && pst < 100)
      	  return(1);
	return(-1);
    }
    else{
	cptr = cbuf;
	forever{
	  if( (cptr2 = strchr(cptr, '\n')) )
	    *cptr2 = '\0';

	  if(re_match(&cartridge_re, cptr, strlen(cptr), 0, NULL) >= 0)
	    rcart = trailingint(cptr);

	  if(re_match(&file_re, cptr, strlen(cptr), 0, NULL) >= 0)
	    rfile = trailingint(cptr);

	  if(n_carts){
	    if(re_match(&ncarts_re, cptr, strlen(cptr), 0, NULL) >= 0)
	      rncarts = trailingint(cptr);
	  }

	  if(serverid){
	    if(!strncmp(cptr, "Server-ID:", 10)){
		strcpy(serverid, first_nospace(cptr + 10));
	    }
	  }

	  if(!cptr2)
	    break;
	  cptr = cptr2 + 1;
	}

	if(n_cart)
	  *n_cart = (Int32) rcart;
	if(n_file)
	  *n_file = (Int32) rfile;
	if(n_carts)
	  *n_carts = (Int32) rncarts;
	return(rcart ? 0 : -1);
    }
  }

  return(-1);
}

static Int32
serverid_from_netaddr(
  ServerIdTab	*rid,
  UChar		*servername,
  Int32		serverport,
  UChar		*filen)
{
  ServerIdTab	idtab, *all_ids;
  Int32		r, i, nids;

  r = 1;

  SETZERO(idtab);

  all_ids = get_server_id_entries(filen, &nids);
  if(!all_ids)
    CLEANUPR(-1);

  for(i = 0; i < nids; i++){
    if(serverport == all_ids[i].serverport
			&& !strcmp(servername, all_ids[i].servername)){
	idtab.serverid = strdup(all_ids[i].serverid);
	idtab.servername = strdup(servername);
	idtab.serverport = serverport;

	if(!idtab.serverid || !idtab.servername)
	  CLEANUPR(-1);

	r = 0;
	break;
    }
  }

 cleanup:
  for(i = 0; i < nids; i++){
    ZFREE(all_ids[i].servername);
    ZFREE(all_ids[i].serverid);
  }
  ZFREE(all_ids);
  if(!r && rid)
    COPYVAL(*rid, idtab);
  if(!rid){
    ZFREE(idtab.serverid);
    ZFREE(idtab.servername);
  }

  return(r);
}

static ServerIdTab *
get_server_id_entries(UChar * filen, Int32 * nids)
{
  UChar		*cptr;
  Int32		i, num_entries, idn;
  ServerIdTab	*server_id_tab = NULL;
  int		i1, n1;
  KeyValuePair	*allentries = NULL;

  if(nids)
    *nids = 0;

  allentries = kfile_getall(filen, &num_entries, KFILE_LOCKED);
  if(!allentries)
    return(NULL);

  server_id_tab = NEWP(ServerIdTab, num_entries);
  if(!server_id_tab)
    CLEANUP;

  memset(server_id_tab, 0, num_entries * sizeof(ServerIdTab));

  for(i = idn = 0; i < num_entries; i++){
    cptr = allentries[i].value;
    if(sscanf(cptr, "%d%n", &i1, &n1) < 1)
	continue;

    server_id_tab[idn].servername = strdup(allentries[i].key);

    server_id_tab[idn].serverid = strwordq(cptr + n1, 0);

    if(!server_id_tab[idn].serverid || !server_id_tab[idn].servername)
	GETOUT;

    server_id_tab[idn].serverport = (Int32) i1;

    idn++;
  }

  if(nids)
    *nids = idn;

 cleanup:
  kfile_freeall(allentries, num_entries);

  return(server_id_tab);

 getout:
  if(server_id_tab){
    for(i = 0; i < num_entries; i++){
	ZFREE(server_id_tab[i].serverid);
	ZFREE(server_id_tab[i].servername);
    }
    ZFREE(server_id_tab);
  }

  CLEANUP;
}

static Int32
serverid_from_netaddr_tab(
  UChar		**rid,
  UChar		*servername,
  Int32		serverport,
  ServerIdTab	*tab,
  Int32		tablen)
{
  Int32		i;

  for(i = 0; i < tablen; i++){
    if(serverport == tab[i].serverport
			&& !strcmp(servername, tab[i].servername))
	break;
  }

  if(i < tablen){
    EM__(*rid = strdup(tab[i].serverid));
  }
  else
    *rid = NULL;

  return(i < tablen);
}

static Int32
remove_tapes_from_idxes(
  StreamerStat	*ststs,
  Int32		num_ststs,
  UChar		*sid_file,
  Int32		last_idx_num,
  Int32		first_idx_num)
{
  Int32		i, n, r = 0, portnum, cartnum, sidtablen;
  UChar		numbuf[32], *newname = NULL, *orgname = NULL, *line = NULL;
  UChar		*idxfile = NULL, *zidxfile = NULL, *unzipfile = NULL, *cptr;
  int		ifd, ofd, ipid, ipst, opid, opst;
  FILE		*ifp = NULL, *ofp = NULL;
  UChar		*servername = NULL, *prevline = NULL, *serverid = NULL;
  Flag		push_line;
  ServerIdTab	*sidtab = NULL;

  for(i = 0; i < num_ststs; i++)
    if(len_Uns32Ranges(ststs[i].deleted_tapes) > 0)
	break;

  if(i >= num_ststs)
    return(0);

  sidtab = get_server_id_entries(sid_file, &sidtablen);
  if(!sidtab)
    return(0);

  n = last_idx_num;
  forever{
    sprintf(numbuf, "%d", (int) n);
    EM__(idxfile = strapp(indexfilepart, numbuf));
    EM__(zidxfile = strapp(idxfile, COMPRESS_SUFFIX));

    if(eaccess(idxfile, F_OK) && eaccess(zidxfile, F_OK))
	break;

    ipid = opid = ipst = opst = ofd = ifd = -1;
    ifd = open_idxfile_read(n, &ipid, &orgname, NO);
    if(ifd < 0)
	break;

    EM__(newname = strapp(idxfile, TMP_SUFFIX));
    unlink(newname);

    if(compresslogfiles){
      ofd = open_to_pipe_sigblk(idxcompresscmd, newname, 1,
				&opid, 0600, &idxproc_blk_sigs);
    }
    if(ofd < 0){
      ofd = open_to_pipe(NULL, newname, 1, NULL, 0600);
      if(ofd < 0){
	fprintf(stderr, T_("Error: Cannot open filename logfile.\n"));
      }
    }

    ifp = fdopen(ifd, "r");
    ofp = fdopen(ofd, "w");

    while(!feof(ifp)){
	ZFREE(line);
	line = fget_alloc_str(ifp);
	if(!line)
	  continue;
	while(chop(line));

	ZFREE(serverid);
	push_line = YES;

	ZFREE(servername);
	portnum = cartnum = -1;
	i = eval_index_line(line, &servername, &portnum, &cartnum,
				NULL, NULL, NULL);
	if(INDEX_FILEENTRY(i)){
	  serverid_from_netaddr_tab(&serverid,
				servername, portnum, sidtab, sidtablen);
	  if(serverid){
	    for(i = 0; i < num_ststs; i++){
	      if(len_Uns32Ranges(ststs[i].deleted_tapes) <= 0)
		continue;

	      if(!strcmp(serverid, ststs[i].serverid)
			&& in_Uns32Ranges(ststs[i].deleted_tapes, cartnum))
		break;
	    }

	    if(i < num_ststs)
		push_line = NO;
	  }
	}

	if(push_line){
	  if(prevline){
	    fprintf(ofp, "%s\n", prevline);
	    ZFREE(prevline);
	  }

	  if(serverid){
	    prevline = line;
	    line = NULL;
	  }
	  else
	    fprintf(ofp, "%s\n", line);
	}
	else{
	  ZFREE(prevline);
	}
    }

    if(prevline){
	fprintf(ofp, "%s\n", prevline);
	ZFREE(prevline);
    }

    fclose(ofp);
    fclose(ifp);

    if(ipid > 0){
	waitpid_forced(ipid, &ipst, 0);
	if( (ipst = WEXITSTATUS(ipst)) ){
	  logmsg(T_("Warning: Command to unprocess index exited with status %d.\n"),
			ipst);
	}
    }
    if(opid > 0){
	waitpid_forced(opid, &opst, 0);
	if( (opst = WEXITSTATUS(opst)) ){
	  logmsg(T_("Warning: Command to process index exited with status %d.\n"),
			opst);
	}
    }

    cptr = (compresslogfiles ? zidxfile : idxfile);
    rename(newname, cptr);
    if(strcmp(cptr, orgname))
	unlink(orgname);

    EM__(unzipfile = hidden_filename(zidxfile));

    ZFREE(newname);
    ZFREE(orgname);
    ZFREE(idxfile);
    ZFREE(zidxfile);

    if(opid > 0){
      ofp = fopen(unzipfile, "w");
      if(ofp){
	fprintf(ofp, "%s\n",
		(idxuncompresscmd[0] ? idxuncompresscmd : (UChar *) "gunzip"));
	fclose(ofp);
      }
      else{
	fprintf(stderr, "Error: Cannot open file `%s'.\n", unzipfile);
      }
    }
    else
	unlink(unzipfile);

    ZFREE(unzipfile);

    n--;

    if(first_idx_num > 0 && n < first_idx_num)
	break;
  }

 cleanup:
  ZFREE(idxfile);
  ZFREE(zidxfile);
  ZFREE(unzipfile);
  ZFREE(newname);
  ZFREE(orgname);
  ZFREE(line);
  ZFREE(servername);
  ZFREE(serverid);
  for(i = 0; i < sidtablen; i++){
    ZFREE(sidtab[i].servername);
    ZFREE(sidtab[i].serverid);
  }
  ZFREE(sidtab);

  return(r);
}

static Int32
remove_needed_tapes(UChar * filen, UChar * serverids, Uns32Range * to_remove)
{
  Uns32Range	*tape_ranges = NULL;
  UChar		*cptr, *cptr2, *cptr3, **needed_tapes_lines = NULL;
  UChar		*tmpbuf = NULL, *new_line = NULL, *line;
  Int32		i, j, r = 0;

  if(eaccess(filen, F_OK) || len_Uns32Ranges(to_remove) <= 0)
    return(0);

  needed_tapes_lines = read_asc_file(filen, 0);
  if(!needed_tapes_lines)
    CLEANUPR(-1);

  for(j = 0; ( line = needed_tapes_lines[j] ); j++){
    new_line = strword(line, 0);
    if(!new_line)
	CLEANUPR(-2);

    for(i = 1, cptr = NULL; ; i++){
	tmpbuf = ZRENEWP(tmpbuf, UChar, strlen(line) + 1);
	if(!tmpbuf)
	  CLEANUPR(-20);

	ZFREE(cptr);
	cptr = strword(line, i);
	if(!cptr)
	  break;

	cptr2 = strchr(cptr, '>');
	if(!cptr2)
	  continue;
	cptr2++;
	if(! *cptr2)
	  continue;
	cptr3 = strchr(cptr2, '@');
	if(cptr3){
	  sscanword(cptr3 + 1, tmpbuf);

	  if(strcmp(tmpbuf, serverids)){
	    cptr2 = strchain(new_line, " ", cptr, NULL);
	    if(!cptr2)
		CLEANUPR(-3);

	    free(new_line);
	    new_line = cptr2;

	    continue;
	  }
	}
	tape_ranges = sscan_Uns32Ranges(cptr2, 0, 0, NULL, NULL);
	if(!tape_ranges)
	  CLEANUPR(-5);

	tape_ranges = del_range_from_Uns32Ranges(tape_ranges, to_remove);
	if(!tape_ranges)
	  CLEANUPR(-4);

	if(len_Uns32Ranges(tape_ranges) > 0){
	  strncpy(tmpbuf, cptr, cptr2 - cptr);	/* write level and > */
	  tmpbuf[cptr2 - cptr] = '\0';

	  cptr2 = str_Uns32Ranges(tape_ranges, 0);
	  if(!cptr2)
	    CLEANUPR(-6);

	  tmpbuf = RENEWP(tmpbuf, UChar,
			strlen(tmpbuf) + strlen(cptr2) + strlen(cptr3) + 10);
	  if(!tmpbuf)
	    CLEANUPR(-8);

	  strcat(tmpbuf, cptr2);		/* append new range */
	  free(cptr2);

	  strcat(tmpbuf, cptr3);	/* append @ and server-id */

	  cptr2 = strchain(new_line, " ", tmpbuf, NULL);
	  if(!cptr2)
	    CLEANUPR(-7);

	  free(new_line);
	  new_line = cptr2;
	}

	ZFREE(tape_ranges);
    }

    free(needed_tapes_lines[j]);
    needed_tapes_lines[j] = new_line;
    new_line = NULL;
  }

  r = write_asc_file_safe(filen, needed_tapes_lines, 0);

 cleanup:
  ZFREE(tape_ranges);
  free_asc_file(needed_tapes_lines, 0);
  ZFREE(new_line);
  ZFREE(tmpbuf);

  return(r);
}

static Uns32Range *
get_needed_tapes(UChar * filen, UChar * serverids)
{
  Uns32Range	*all_tape_ranges = NULL, *new_tape_ranges = NULL;
  UChar		*cptr, *cptr2, *cptr3, **needed_tapes_lines = NULL;
  UChar		tmpbuf[1024];
  Int32		i, j;

  all_tape_ranges = empty_Uns32Ranges();
  if(!all_tape_ranges)
    return(NULL);

  needed_tapes_lines = read_asc_file(filen, 0);
  if(!needed_tapes_lines)
    CLEANUP;

  for(j = 0; needed_tapes_lines[j]; j++){
    for(i = 1, cptr = NULL; ; i++){
	ZFREE(cptr);
	cptr = strword(needed_tapes_lines[j], i);
	if(!cptr)
	  break;

	cptr2 = strchr(cptr, '>');
	if(!cptr2)
	  continue;
	cptr2++;
	if(! *cptr2)
	  continue;
	cptr3 = strchr(cptr2, '@');
	if(cptr3){
	  sscanword(cptr3 + 1, tmpbuf);

	  if(strcmp(tmpbuf, serverids))
	    continue;
	}
	EM__(new_tape_ranges = sscan_Uns32Ranges(cptr2, 0, 0, NULL, NULL));

	if(merge_Uns32Ranges(&all_tape_ranges, new_tape_ranges))
	  GETOUT;

	ZFREE(new_tape_ranges);
    }
  }

 cleanup:
  ZFREE(new_tape_ranges);
  free_asc_file(needed_tapes_lines, 0);

  return(all_tape_ranges);

 getout:
  ZFREE(all_tape_ranges);
  CLEANUP;
}

static Int32
add_server_id_entries(UChar * filen, ServerIdTab * ids, Int32 num_ids)
{
  int		fd, i1, n1;
  UChar		**lines = NULL, **cpptr, *cptr;
  UChar		*server_in_line = NULL;
  Int32		num_lines, i, idi, n, r = 0;

  fd = -1;
  if(!eaccess(filen, F_OK)){
    fd = set_wlock_forced(filen);
    if(fd < 0)
	return(-1);

    lines = read_asc_file(filen, 0);
    if(!lines)
	CLEANUPR(-2);
    for(num_lines = 0, cpptr = lines; *cpptr; cpptr++, num_lines++);

    for(idi = 0; idi < num_ids; idi++){
      for(i = 0; i < num_lines; i++){
	n = strlen(lines[i]);
	server_in_line = ZRENEWP(server_in_line, UChar, n);
	
	cptr = sscanwordq(lines[i], server_in_line);
	if(!cptr)
	  continue;
	n = sscanf(cptr, "%d%n", &i1, &n1);
	if(n < 1)
	  continue;

	if((Int32) i1 == ids[idi].serverport
			&& !strcmp(server_in_line, ids[idi].servername))
	  break;
      }

      n = strlen(ids[idi].servername) + strlen(ids[idi].serverid) + 20;

      if(i < num_lines){
	cptr = lines[i] = RENEWP(lines[i], UChar, n);
      }
      else{
	lines = RENEWP(lines, UChar *, num_lines + 2);
	if(!lines)
	  CLEANUPR(-6);

	cptr = lines[num_lines] = NEWP(UChar, n);
	num_lines++;
	lines[num_lines] = NULL;
      }
      if(!cptr)
	CLEANUPR(-5);

      sprintwordq(cptr, ids[idi].servername);
      cptr += strlen(cptr);
      sprintf(cptr, " %d ", (int) ids[idi].serverport);
      cptr += strlen(cptr);
      sprintwordq(cptr, ids[idi].serverid);
    }
  }
  else{
    lines = NEWP(UChar *, num_ids + 1);
    if(!lines)
	CLEANUPR(-3);

    for(i = 0, cpptr = lines; i < num_ids; i++, cpptr++){
	n = strlen(ids[i].servername) + strlen(ids[i].serverid) + 20;
	*cpptr = NEWP(UChar, n);
	if(!(*cpptr))
	  CLEANUPR(-4);

	cptr = *cpptr;
	sprintwordq(cptr, ids[i].servername);
	cptr += strlen(cptr);
	sprintf(cptr, " %d ", (int) ids[i].serverport);
	cptr += strlen(cptr);
	sprintwordq(cptr, ids[i].serverid);
    }
    *cpptr = NULL;
  }

  r = write_asc_file_safe(filen, lines, 0);

 cleanup:
  if(fd >= 0)
    close(fd);
  free_asc_file(lines, 0);
  ZFREE(server_in_line);

  return(r);
}

static Int32
get_args(int argc, char ** argv)
{
  Int32		i;
  UChar		*id_before, *vardir_before;

  id_before = identity;
  vardir_before = vardir;

  switch(mode){
   case MODE_RESTORE:
    i = get_restore_args(argc, argv);
    break;

   case MODE_VERIFYBU:
    i = get_verify_args(argc, argv);
    break;

   case MODE_UPDATE_INDEXES:
    i = get_update_indexes_args(argc, argv);
    break;

   case MODE_COPY_TAPE:
    i = get_copytape_args(argc, argv);
    break;

   default:
    i = get_backup_args(argc, argv);
  }

  if(!id_before && identity)
    identity_from_arg = YES;
  if(!vardir_before && vardir)
    vardir_from_arg = YES;

  return(i);
}

static Int32
get_backup_args(int argc, char ** argv)
{
  Int8		clogf = 7, cbu = 7, cks = 7;
  Int32		num_unused = 0;
  UChar		**unused_args = NULL, *cptr;

  if(goptions(-argc, (UChar **) argv, "s:c;s:S;b:d;+b:x;s:h;s:P;s:C;s:F;"
		"s:D;s:I;i:N;f:O;s2:z;i:Z;s:s;s:X;s:l;b:a;s:i;s:b;s:e;"
		"b:H;+b:L;+b:B;b:y;s:k;s:V;b:v;i:Q;s:W;b:G;b:E;s:f;s:M;*",
			&paramfile, &cartsetlist, &detach, &cks,
			&backuphostlist, &backupportlist, &rootdir,
			&filestoskipraw, &dirstoskipraw, &indexfilepart,
			&numindexestostore, &daystostoreindexes,
			&compresscmd, &uncompresscmd,
			&builtin_compr_level, &dont_compress_str,
			&exclude_filename, &logfile, &keep_counter,
			&startinfoprog, &initprog, &exitprog,
			&remove_lower_levels, &clogf, &cbu, &progress,
			&cryptfile, &vardir, &verbose, &backup_level,
			&identity, &newcart, &use_ctime, &fstypesraw,
			&msgs_config_str, &num_unused, &unused_args
			))
    usage(argv[0]);

  if(mode == MODE_FULL_BACKUP && backup_level != MAXINT){
    fprintf(stderr, T_("Error: Flag -Q not allowed with full_backup.\n"));
    usage(argv[0]);
  }

  if(num_unused > 0){
    EM__(dirstobackupraw = strdup(""));
    for(num_unused--; num_unused >= 0; num_unused--){
	EM__(cptr = strchain("\"", unused_args[num_unused], "\" ",
					dirstobackupraw, NULL));
	free(dirstobackupraw);
	dirstobackupraw = cptr;
    }

    no_default_backup = YES;
    numparts = 1;
  }

#if 0		/* why the f... did i do this ? */
  if(keep_counter)
    numparts = 1;
#endif

  if(clogf != 7)
    compresslogfiles = clogf;
  if(cbu != 7)
    compressbu = cbu;
  if(cks != 7)
    chksum = cks;

  return(0);
}

static Int32
get_update_indexes_args(int argc, char ** argv)
{
  static Flag	warned = NO;

  int		i;

  if(curuid){
    if(argc > 1){
      if(!warned){
	fprintf(stderr, "Warning: All supplied arguments are ignored, when not started as the superuser.\n");
	warned = YES;
      }

      return(0);
    }
  }
			/* option -p is handled in the filelist processing */
  i = goptions(-argc, (UChar **) argv,
	"s:c;s:I;s:h;s:P;s2:z;i:Z;b:v;s:V;s:W;s:k",
	&paramfile, &indexfilepart, &backuphostlist, &backupportlist,
	&compresscmd, &uncompresscmd, &builtin_compr_level, &verbose,
	&vardir, &identity, &cryptfile);

  if(i)
    usage(argv[0]);

  return(0);
}

static Int32
get_verify_args(int argc, char ** argv)
{
  int		i, backupno, runno, n;
  Flag		have_prev = NO;

			/* option -p is handled in the filelist processing */
  i = goptions(-argc, (UChar **) argv,
	"b:0;b:1;b:2;b:3;b:4;b:5;b:6;b:7;b:8;b:9;b:.;s:c;s:S;s:h;s:P;"
		"s:C;s:I;s2:z;s:k;s:V;s:W;b:l;b:a;b:v;s:M",
	&have_prev, &have_prev, &have_prev, &have_prev, &have_prev,
	&have_prev, &have_prev, &have_prev, &have_prev, &have_prev,
	&have_prev, &paramfile, &cartsetlist, &backuphostlist, &backupportlist,
	&rootdir, &indexfilepart, &compresscmd, &uncompresscmd,
	&cryptfile, &vardir, &identity, &do_listing, &vfy_list_all,
	&verbose, &msgs_config_str);

  if(i)
    usage(argv[0]);

  if(have_prev){
    for(i = 1; i < argc; i++){
	n = sscanf(argv[i] + 1, "%d.%d", &runno, &backupno);
	if(n > 1)
	  num_prev_backup = backupno;
	if(n > 0){
	  num_prev_run = runno;
	  break;
	}
    }
  }

  return(0);
}

static Int32
get_copytape_args(int argc, char ** argv)
{
  UChar		*host = NULL, *keyfile = NULL, dest = 0, *service = NULL;
  Int32		port = 0, cart = 0, file = 0, i;

  i = goptions(-argc, (UChar **) argv,
		"b:D;s:h;s:P;i:C;i:F;s:k;b:v;s:c;s:l;s:T;s:M",
		&dest, &host, &service, &cart, &file, &keyfile, &verbose,
		&paramfile, &logfile, &tmpdir, &msgs_config_str);
  if(i)
    usage(argv[0]);

  if(service){
    port = get_tcp_portnum(service);
    if(port < 0){
	errmsg(T_("Error: Cannot resolve TCP service name `%s'.\n"), service);
	exit(1);
    }
  }

  return(0);
}

static Int32
list_backups(Int32 first_num, Int32 last_num)
{
  Int32		r = 0, i, j, ncall = 0, n, n_lines = 0, prev_eval;
  UChar		*line = NULL, **lines = NULL;
  int		*fds = NULL, *pids = NULL, fd, pid, pst;
  FILE		*fp = NULL;

  for(i = 0; last_num - i >= first_num; i++){
    pid = -1;

    fd = open_idxfile_read(last_num - i, &pid, NULL, YES);
    if(fd < 0)
	break;

    EM__(fds = ZRENEWP(fds, int, i + 1));
    EM__(pids = ZRENEWP(pids, int, i + 1));

    fds[i] = fd;
    pids[i] = pid;
  }

  for(i--; i >= 0; i--){
    fp = fdopen(fds[i], "r");
    if(!fp){
	close(fds[i]);
	if(pids[i] >= 0){
	  kill(pids[i], SIGKILL);	/* no mercy */
	  waitpid_forced(pids[i], &pst, 0);
	}
	fds[i] = pids[i] = -1;
	continue;
    }

    prev_eval = -1;	/* this may not mean INDEX_FILEENTRY i.e. >= 0 s.a. */

    while(!feof(fp)){
	if(fget_realloc_str(fp, &line, &ncall))
	  continue;
	while(chop(line));

	n = eval_index_line(line, NULL, NULL, NULL, NULL, NULL, NULL);
	if(INDEX_COMMENT(n) && INDEX_FILEENTRY(prev_eval)){
	  if(!read_header_line(line, NULL, NULL, NULL, NULL, NULL)){
	    EM__(lines = ZRENEWP(lines, UChar *, n_lines + 1));
	    EM__(lines[n_lines] = strdup(line));
	    n_lines++;
	  }
	}
	prev_eval = n;	/* there is only backup data assigned to this */
    }			/* comment, if there was a previous list line */

    fclose(fp);
    if(pids[i] >= 0){
	waitpid_forced(pids[i], &pst, 0);
	pst = WEXITSTATUS(pst);
	if(pst)
	  errmsg(T_("Warning: Uncompressing index returned bad exit status.\n"));
    }

    if(lines){
	for(j = 0; j < n_lines; j++)
	  fprintf(stdout, "-%d.%d: %s\n", (int)(n_lines - 1 - j), (int) i, lines[j]);

	free_asc_file(lines, n_lines);
	lines = NULL;
	n_lines = 0;
    }
  }

 cleanup:
  ZFREE(fds);
  ZFREE(pids);
  ZFREE(line);

  return(r);
}

static void
verifybu(int c, char ** v)
{
  UChar		*line = NULL, *server, **servers, **clientids;
  int		fd, pid = -1, pst, i;
  Int32		*cnos = NULL, *fnos = NULL, listidx, ncall = 0;
  Int32		cartn, filen;
  Int32		portn, *ports;
  FILE		*fp;

  if(do_listing)
    do_exit(list_backups(vfy_list_all ? 1 :
			(num_prev_run > 0 ? num - num_prev_run : num), num));

  if(num_prev_backup > 0)
    num -= num_prev_backup;

  cnos = NEWP(Int32, i = num_prev_run + 1);
  fnos = NEWP(Int32, i);
  servers = NEWP(UChar *, i);
  ports = NEWP(Int32, i);
  clientids = NEWP(UChar *, i);
  if(!cnos || !fnos || !servers || !ports || !clientids)
    ENM__;

  memset(servers, 0, sizeof(UChar *) * i);
  memset(ports, 0, sizeof(Int32) * i);
  memset(clientids, 0, sizeof(UChar *) * i);
  for(i = 0; i <= num_prev_run; i++)
    cnos[i] = fnos[i] = -1;

  fd = open_idxfile_read(num, &pid, NULL, NO);
  fp = fdopen(fd, "r");
  if(!fp){
    errmsg(T_("Error: Cannot read file `%s'.\n"), indexfile);
    do_exit(8);
  }

  listidx = 0;
  while(!feof(fp)){
    if(fget_realloc_str(fp, &line, &ncall))
	continue;

    server = NULL;
    portn = -1;
    cartn = filen = -1;
    i = eval_index_line(line, &server, &portn, &cartn, &filen, NULL, NULL);
    if(INDEX_FILEENTRY(i) && !line[i]){
	cnos[listidx] = cartn;
	fnos[listidx] = filen;
	servers[listidx] = server;
	ports[listidx] = portn;
	listidx = (listidx + 1) % (num_prev_run + 1);
    }
    else if(INDEX_COMMENT(i) && !strncmp(first_nospace(line), "##", 2)){
	i = (listidx + num_prev_run) % (num_prev_run + 1);
	if(!clientids[i])
	  EM__(clientids[i] = NEWP(UChar, 132));
	strcpy(clientids[i], identity);
	if(!identity_from_arg)
	  read_header_line(line, NULL, NULL, NULL, NULL, clientids[i]);
    }
  }
  fclose(fp);
  ZFREE(line);

  if(pid >= 0){
    waitpid_forced(pid, &pst, 0);
    if(WEXITSTATUS(pst)){
      errmsg(T_("Warning: Some errors occurred uncompressing `%s'.\n"), indexfile);
    }
  }

  if(cnos[listidx] < 0 && fnos[listidx] < 0){
    errmsg(T_("Error: Did not find start mark of previous backup in `%s'.\n"),
		indexfile);
    do_exit(9);
  }

  compose_clntcmd(tmpbuf, verbose ? "dv" : "d",
		clientids[listidx] ? clientids[listidx] : identity,
		servers[listidx], ports[listidx], 0, cryptfile,
		cnos[listidx], fnos[listidx], 0, 0, 0, NULL);

  start_msg_proc_msg(servers[listidx], - ports[listidx]);

  system(tmpbuf);

  stop_msg_proc();

  do_exit(0);
}

static int
get_arg_num(int argc, char ** argv, Int32 * idx, Int8 flags)
{
  static int	argp = 1;
  int		n, r = -1, i;

  if(flags & FIRST_NUM_ARG)
    argp = 1;

  for(; argp < argc; argp++){
    if(argv[argp][0] == '-'){
	i = sscanf(argv[argp] + 1, "%d%n", &r, &n);
	if(i > 0 && (flags & IGNORE_NUM_ARG_TRAILERS))
	  break;
	if(strlen(argv[argp] + 1) == n)
	  break;
    }
  }

  if(argp < argc && idx){
    *idx = argp;
  }
  argp++;

  return(r);
}

static Int32
get_restore_args(int argc, char ** argv)
{
  static Flag	reported = NO;

  Int32		i;
  Flag		have_prev = NO;
  UChar		*dummy = NULL, *localpfile = NULL;

  restore_EM = restore_em = 0;

			/* option -p is handled in the filelist processing */
  i = goptions(-argc, (UChar **) argv,
		"b:0;b:1;b:2;b:3;b:4;b:5;b:6;b:7;b:8;b:9;b:n;b:l;"
		"b:L;s:h;s:P;s:I;s:R;s:C;s:V;b:a;c:e;b:f;c:E;s:B;"
		"s:A;b:m;s:T;s:c;s:S;*;s:k;s2:z;b:v;b:q;b:i;s:W;s:F;"
		"b:t;i:N;f:O;s:M",
	&have_prev, &have_prev, &have_prev, &have_prev, &have_prev,
	&have_prev, &have_prev, &have_prev, &have_prev, &have_prev,
	&do_counting, &do_listing, &do_raw_listing, &backuphostlist,
	&backupportlist, (curuid ? &dummy : &indexfilepart),
	&restoreroot, &restoreroot, (curuid ? &dummy : &vardir),
	&restore_all, &restore_em, &restore_emlist, &restore_EM,
	&beforestr, &afterstr, &preserve_existing, &restoretapestr,
	(curuid ? &localpfile : &paramfile), &cartsetlist,
	&num_restore_paths, &restore_paths, &cryptfile,
	&compresscmd, &uncompresscmd, &verbose, &quiet, &ign_case,
	(curuid ? &dummy : &identity), &formatstr, &do_listcarts,
	&numindexestoscan, &daystoscanindexes, &msgs_config_str);

  if(i)
    usage(argv[0]);

  if(formatstr && !do_listing)
    fprintf(stderr,
	T_("Warning: The option -F is only allowed in combination with -l, ignored.\n"));

  if(dummy){
    fprintf(stderr,
	T_("Error: The options -I, -W and -V may only be used by the superuser.\n"));
    usage(argv[0]);
  }
  if(curuid && localpfile){
    dummy = resolve_path__(localpfile, NULL);
    if(!dummy){
	fprintf(stderr,
		T_("Error: Cannot resolve path of configuration file `%s'\n"),
		localpfile);
	do_exit(1);
    }
    localpfile = dummy;
    i = strlen(confdir);
    if(!strncmp(localpfile, confdir, i)){
      if(FN_ISDIRSEP(localpfile[i])){
	paramfile = localpfile;
      }
    }
    if(localpfile != paramfile){
	fprintf(stderr,
		T_("Error: When run as normal user, a given configuration file must reside in the same directory like the default one, otherwise such usage is considered insecure.\n"));
	do_exit(1);
    }
  }

  if(num_restore_paths == 0 &&
		!restore_all && !restore_em && !restore_emlist && !restore_EM)
    usage(argv[0]);

  i = 0;
  if(restore_all)
    i++;
  if((do_listing || do_counting || do_listcarts || do_raw_listing)
					&& !(restore_EM || restore_all))
    i++;
  if(restore_em || restore_emlist)
    i++;
  if(restore_EM)
    i++;
  if(i > 1)
    usage(argv[0]);
  if(restore_em && restore_emlist && restore_EM)
    usage(argv[0]);
  if(have_prev && (restore_emlist || restore_em))
    usage(argv[0]);
  if(num_restore_paths > 0 &&
		(restore_emlist || restore_em || restore_all))
    usage(argv[0]);

  if(have_prev){
    num_prev_backup = get_arg_num(argc, argv, &i, FIRST_NUM_ARG);
    EM__(num_prev_backup_str = strdup(argv[i]));
  }

  if(afterstr){
    dummy = strchr(afterstr, '%');
    if(dummy)
	*(dummy++) = '\0';
    if(!empty_string(afterstr)){
      time_newer = time_from_datestr(afterstr);
      if(time_newer == UNSPECIFIED_TIME){
	fprintf(stderr, T_("Error: Cannot read date and/or time from `%s'.\n"),
			afterstr);
	usage(argv[0]);
      }

      if(!reported && !quiet)
	fprintf(stdout, do_listing ?
		T_("Listing files not older than: %s\n") :
		(do_listcarts || do_counting ?
			T_("Looking for files not older than: %s\n") :
			T_("Restoring files not older than: %s\n")),
				ctime(&time_newer));
    }
    if(dummy){
      butime_after = time_from_datestr(dummy);
      if(butime_after == UNSPECIFIED_TIME){
	fprintf(stderr, T_("Error: Cannot read date and/or time from `%s'.\n"),
			dummy);
	usage(argv[0]);
      }
      if(!reported && !quiet)
	fprintf(stdout, do_listing ?
		T_("Listing files written to backup not earlier than: %s\n") :
		(do_listcarts || do_counting ?
			T_("Looking for files written to backup earlier than: %s\n") :
			T_("Restoring files written to backup earlier than: %s\n")),
				ctime(&butime_after));
    }
  }
  if(beforestr){
    dummy = strchr(beforestr, '%');
    if(dummy)
	*(dummy++) = '\0';
    if(!empty_string(beforestr)){
      time_older = time_from_datestr(beforestr);
      if(time_older == UNSPECIFIED_TIME){
	fprintf(stderr, T_("Error: Cannot read date and/or time from `%s'.\n"),
			beforestr);
	usage(argv[0]);
      }
      if(!reported && !quiet)
	fprintf(stdout, do_listing ?
		T_("Listing files not newer than: %s\n") :
		(do_listcarts || do_counting ?
			T_("Looking for files not newer than: %s\n") :
			T_("Restoring files not newer than: %s\n")),
				ctime(&time_older));
    }
    if(dummy){
      butime_before = time_from_datestr(dummy);
      if(butime_before == UNSPECIFIED_TIME){
	fprintf(stderr, T_("Error: Cannot read date and/or time from `%s'.\n"),
			dummy);
	usage(argv[0]);
      }
      if(!reported && !quiet)
	fprintf(stdout, do_listing ?
		T_("Listing files written to backup not later than: %s\n") :
		(do_listcarts || do_counting ?
			T_("Looking for files written to backup not later than: %s\n") :
			T_("Restoring files written to backup not later than: %s\n")),
				ctime(&butime_before));
    }
  }

  if(restoretapestr){
    if(empty_string(restoretapestr)){
	ZFREE(restoretapestr);
    }
    else{
	restoretapes = sscan_Uns32Ranges__(restoretapestr, 1, MAXINT, NULL, NULL);
	EM__(restoretapes);
    }
  }

  reported = YES;

  return(0);
}

#define	startpattern	"^## [a-zA-Z]+ backup at [a-zA-Z0-9: ]+, "	\
		"current number: [1-9][0-9]*\\(,.*\\)?$"

#define	NEW_CLIENT_START_MARK	((UChar *) 1)
#define	EMPTY_PLACEHOLDER	((UChar *) 2)

#define	MAXALLOC_FIRST		2097152
#define	MAXALLOC_PROBESTEP	524288
#define	MAXALLOC_STEP		(MAXALLOC_PROBESTEP * 10)

static void
restore(int c, char ** v)
{
  UChar		**cpptr, **cpptr2, **matches = NULL, *line, *cptr, *dirp;
  UChar		**newmatches = NULL, **names = NULL, **newnames = NULL;
  UChar		**auxfnames;		/* uninitialized OK */
  UChar		*auxline, *unzipcmd, *unzipfile = NULL;
  UChar		**unzipfilecont = NULL, new_identity[132];
  Int32		lineno, auxlineno, prevlineno, allocated, num_matches = 0;
  Int32		nc, nf, total_num_matches, num_rest, resterr, sh;
  Int32		matchflags, i, j, n, ncall, auxncall, maxalloc;
  Flag		added, do_client_run, sp, sc, sf, same_loc, only_list;
  Flag		cp_prev, clr_prev, force_new_run, alloc_limit_reached;
  int		fd, pst, pid, fileuid, auxfd, auxpst, auxpid;
  FILE		*fp, *auxfp;
  struct stat	statb;
  time_t	newtim, filemtime;
  DataLocator	act_dataloc, prev_dataloc;
  DataArea	*dataareas = NULL;
  ServerIdTab	cur_serveridtab;
  Int32		num_dataareas = 0;
  UChar		fmtpat[3];

  if(numindexestoscan >= 0 && daystoscanindexes >= 0.0)
    errmsg(T_("Warning: Both DaysToScanIndexes and NumIndexesToScan configured. Ignoring the shorter duration.\n"));

  if(daystoscanindexes >= 0.0){		/* set numindexestoscan from it */
    time_t	read_time, cmp_time;

    cmp_time = time(NULL) - (time_t)(daystoscanindexes * 86400.0);

    for(i = num - 1; ; i--){
      read_time = (time_t) 0;

      sprintf(tmpbuf, "%d:", (int) i);
      cptr = kfile_get(index_ages_file, tmpbuf, KFILE_LOCKED);

      if(cptr){
	read_time = strint2time(cptr);
	free(cptr);
      }
      else{
	break;		/* nothing there, cannot go back further */
      }

      if(read_time < cmp_time)
	break;
    }

    i = num - i;
    if(i > numindexestoscan)
	numindexestoscan = i;
  }

  if(formatstr){
    for(; (cptr = strstr(formatstr, "%%")); *(cptr + 1) = '\"');
    for(; (cptr = strstr(formatstr, "\\%")); *(cptr + 1) = '\"', *cptr = '%');
    EM__(fmt_repls = NEWP(ReplSpec, 1));
    num_fmt_repls = 1;
    fmt_repls[0].token = "%\"";
    EM__(fmt_repls[0].repl = strdup("%"));

    strcpy(fmtpat, "%%");
    for(cptr = FMT_REPL_CHARS; *cptr; cptr++){
	fmtpat[1] = *cptr;
	if(strstr(formatstr, fmtpat)){
	  EM__(fmt_repls = RENEWP(fmt_repls, ReplSpec, num_fmt_repls + 1));
	  SETZERO(fmt_repls[num_fmt_repls]);
	  EM__(fmt_repls[num_fmt_repls].token = strdup(fmtpat));
	  num_fmt_repls++;
	}
    }
  }

  if(restoreroot){
    i = 0;

    if(curuid){
	EM__(dirp = strdup(restoreroot));
	cleanpath(dirp);
	forever{
	  if(!strcmp(dirp, "")){
	    free(dirp);
	    EM__(dirp = strdup("."));
	  }

	  i = stat(dirp, &statb);
	  if(!i){
	    if(!IS_DIRECTORY(statb)){
		errmsg(T_("Error: `%s' is not a directory.\n"), dirp);
		do_exit(45);
	    }
	    i = eaccess(dirp, X_OK);
	    if(i){
		errmsg(T_("Error: Cannot read directory `%s'. No execute permission.\n"),
			dirp);
		do_exit(20);
	    }
	    if(statb.st_uid != curuid){
		errmsg(T_("Error: The directory `%s' is not owned by you.\n"),
			dirp);

		do_exit(22);
	    }

	    break;
	  }

	  if(FN_ISROOTDIR(dirp) || !strcmp(dirp, "."))
	    break;
	  cptr = FN_LASTDIRDELIM(dirp);
	  if(cptr == dirp)
	    cptr++;
	  if(!cptr)
	    cptr = dirp;

	  *cptr = '\0';
	}

	free(dirp);
    }

    i = make_dirpath(restoreroot, 0700, curuid, curgid);
    if(i < 0){
	errmsg(T_("Error: Cannot make directory `%s'.\n"), restoreroot);
	do_exit(44);
    }
    if(i > 0){
	errmsg(T_("Warning: Cannot change owner/group of `%s'. Restore might fail.\n"),
			restoreroot);
    }

    rootdir = restoreroot;
  }

  /* change to the root directory */
  if(chdir(rootdir)){
    errmsg(T_("Error: Cannot change to directory `%s'.\n"), rootdir);
    do_exit(7);
  }

  if(restore_em || restore_emlist || restore_EM)
    restoreem(c, v);

  only_list = do_listing || do_counting || do_listcarts;

  if(restore_all && only_list){
    EM__(restore_paths = NEWP(UChar *, 2));
    EM__(restore_paths[0] = strdup("*"));
    restore_paths[1] = NULL;
    num_restore_paths = 1;
    restore_all = NO;
  }

  for(i = 0; i < num_restore_paths; i++){
    if(!strcmp(restore_paths[i], "-p")){
	num_restore_paths--;
	free(restore_paths[i]);
	for(n = i; n < num_restore_paths; n++)
	  restore_paths[n] = restore_paths[n + 1];
    }
    else{
      n = strlen(restore_paths[i]);
      if(n > 0)
	cptr = strchain(restore_paths[i][0] == '*' ? "" : "*",
			restore_paths[i],
			restore_paths[i][n - 1] == '*' ? "" : "*",
			NULL);
      else
	cptr = strdup("*");
      free(restore_paths[i]);
      EM__(restore_paths[i] = cptr);
    }

    cleanpath(restore_paths[i]);
  }

  have_timerange = (time_older > 0 || time_newer > 0 || butime_after > 0 || butime_before > 0);

  indexfiles = NEWP(UChar *, 3);
  zindexfiles = NEWP(UChar *, 3);
  if(!indexfiles || !zindexfiles)
    ENM__;
  memset(indexfiles, 0, 3 * sizeof(UChar *));
  memset(zindexfiles, 0, 3 * sizeof(UChar *));

  if(!have_timerange){
    if(num_prev_backup > 0){
      num -= num_prev_backup;
      sprintf(tmpbuf, "%d", num);
      EM__(indexfiles[0] = strapp(indexfilepart, tmpbuf));
    }
    else{
      if(read_pint_file(partfile, &part))
	part = 1;

      if(part < numparts && num > 1){
	sprintf(tmpbuf, "%d", num - 1);
	EM__(indexfiles[0] = strapp(indexfilepart, tmpbuf));
	sprintf(tmpbuf, "%d", num);
	EM__(indexfiles[1] = strapp(indexfilepart, tmpbuf));
	EM__(zindexfiles[1] = strapp(indexfiles[1], COMPRESS_SUFFIX));
      }
      else{
	sprintf(tmpbuf, "%d", num);
	EM__(indexfiles[0] = strapp(indexfilepart, tmpbuf));
      }
    }
    EM__(zindexfiles[0] = strapp(indexfiles[0], COMPRESS_SUFFIX));
  }
  if(have_timerange || restoretapes){
    num_indexfiles = 0;

    sprintf(tmpbuf, "%s%d", indexfilepart, num);
    forever{
      cptr = tmpbuf + strlen(tmpbuf) + 2;
      sprintf(cptr, "%s%s", tmpbuf, COMPRESS_SUFFIX);

      if(numindexestoscan >= 0 && num_indexfiles >= numindexestoscan)
	break;

      num_indexfiles++;

      EM__(indexfiles = RENEWP(indexfiles, UChar *, num_indexfiles + 1));
      EM__(zindexfiles = RENEWP(zindexfiles, UChar *, num_indexfiles + 1));
      EM__(indexfiles[num_indexfiles - 1] = strdup(tmpbuf));
      EM__(zindexfiles[num_indexfiles - 1] = strdup(cptr));
      indexfiles[num_indexfiles] = zindexfiles[num_indexfiles] = NULL;

      num--;
      sprintf(tmpbuf, "%s%d", indexfilepart, num);
      if(!eaccess(tmpbuf, R_OK))
	continue;
      cptr = tmpbuf + strlen(tmpbuf) + 2;
      sprintf(cptr, "%s%s", tmpbuf, COMPRESS_SUFFIX);
      if(!eaccess(cptr, R_OK))
	continue;

      break;
    }

    for(i = 0; i < num_indexfiles / 2; i++){
	memswap(indexfiles + i, indexfiles + num_indexfiles - 1 - i,
				sizeof(UChar *));
	memswap(zindexfiles + i, zindexfiles + num_indexfiles - 1 - i,
				sizeof(UChar *));
    }
  }

  for(cpptr = indexfiles, i = 0; *cpptr; cpptr++, i++){
    if(eaccess(*cpptr, R_OK) && eaccess(zindexfiles[i], R_OK)){
	errmsg(T_("Serious warning: Cannot read filename logfile `%s'.\n"), *cpptr);
	free(*cpptr);
	free(zindexfiles[i]);

	for(cpptr2 = cpptr, n = i; *cpptr2; cpptr2++, n++){
	  *cpptr2 = *(cpptr2 + 1);
	  zindexfiles[n] = zindexfiles[n + 1];
	}

	cpptr--;
	i--;
    }
  }

  if(restore_all)
    restoreall(c, v);		/* will exit */

  matchflags = GFNM_PATHNAME | GFNM_NOESCAPE;
  if(ign_case)
    matchflags |= GFNM_CASEFOLD;

  line = auxline = NULL;
  ncall = auxncall = 0;
  allocated = 0;
  current_bu_time = 0;
  prev_bu_time = 0;

  SETZERO(act_dataloc);
  SETZERO(prev_dataloc);
  nc = nf = total_num_matches = resterr
				= lineno = auxlineno = prevlineno = 0;

  unzipcmd = idxuncompresscmd[0] ? idxuncompresscmd : (UChar*) "gunzip";

  auxfp = NULL;		/* to read ahead to get the next timestamp entry */
  auxpid = -1;
  if(have_timerange){
    auxfnames = indexfiles;
    auxfd = open(*auxfnames, O_RDONLY);
    if(auxfd < 0){
	i = auxfnames - indexfiles;

	cptr = unzipcmd;

	unzipfile = hidden_filename(zindexfiles[i]);
	if(!eaccess(unzipfile, R_OK)){
	  unzipfilecont = read_asc_file(unzipfile, 0);
	  if(unzipfilecont)
	    if(unzipfilecont[0])
		cptr = unzipfilecont[0];
	}

	auxfd = open_from_pipe_sigblk(cptr, zindexfiles[i], 1,
				&auxpid, &idxproc_blk_sigs);
	if(auxfd >= 0)
	  auxfp = fdopen(auxfd, "r");

	ZFREE(unzipfile);
	free_asc_file(unzipfilecont, 0);
	unzipfilecont = NULL;
    }

    if(auxfd >= 0)
	set_closeonexec(auxfd);
  }

  maxalloc = MAXALLOC_FIRST;
  alloc_limit_reached = NO;

  for(cpptr = indexfiles; *cpptr; cpptr++){
    have_mtime_idx = NO;
    force_new_run = NO;
    pid = -1;
    fd = open(*cpptr, O_RDONLY);
    if(fd < 0){
	i = cpptr - indexfiles;

	cptr = unzipcmd;

	unzipfile = hidden_filename(zindexfiles[i]);
	if(!eaccess(unzipfile, R_OK)){
	  unzipfilecont = read_asc_file(unzipfile, 0);
	  if(unzipfilecont)
	    if(unzipfilecont[0])
		cptr = unzipfilecont[0];
	}

	fd = open_from_pipe_sigblk(cptr, zindexfiles[i], 1,
					&pid, &idxproc_blk_sigs);

	ZFREE(unzipfile);
	free_asc_file(unzipfilecont, 0);
	unzipfilecont = NULL;

	if(fd < 0)
	  continue;
    }

    set_closeonexec(fd);

    fp = fdopen(fd, "r");
    if(!fp){
	errmsg(T_("Error: Cannot read file `%s'.\n"), *cpptr);
	continue;
    }
    while(!feof(fp)){
      if(fget_realloc_str(fp, &line, &ncall))
	continue;
      lineno++;

      if(have_timerange){	/* have to read ahead from second stream */
	if(!auxfp && !current_bu_time)
	  current_bu_time = time(NULL);

	if(auxfp && lineno >= auxlineno){
	  current_bu_time = 0;
	  while(!current_bu_time && auxfp){
	    if(fget_realloc_str(auxfp, &auxline, &auxncall)){
		fclose(auxfp);
		if(auxpid > 0){
		  waitpid_forced(auxpid, &auxpst, 0);

		  if(WEXITSTATUS(auxpst)){
		    errmsg(T_("Warning: Uncompressing `%s' returned bad exit status.\n"),
			zindexfiles[auxfnames - indexfiles]);
		  }
		  auxpid = -1;
		}
		auxfp = NULL;

		auxfnames++;
		if(! *auxfnames){
		  current_bu_time = time(NULL);
		  break;
		}

		auxfd = open(*auxfnames, O_RDONLY);
		if(auxfd < 0){
		  auxfd = open_from_pipe_sigblk(unzipcmd,
			zindexfiles[auxfnames - indexfiles], 1,
				&auxpid, &idxproc_blk_sigs);
		  if(auxfd >= 0)
		    auxfp = fdopen(auxfd, "r");
		}

		if(auxfd >= 0)
		  set_closeonexec(auxfd);

	 	continue;
	    }

	    auxlineno++;
	    if(auxlineno <= lineno)
		continue;

	    if(!strncmp(cptr = first_nospace(auxline), "##", 2)){
		if(!read_header_line(cptr, NULL, &newtim, NULL, NULL, NULL)){
		  current_bu_time = newtim;
		}
	    }
	  }	/* while don't have timestamp and stuff to read */
	}	/* if have stuff to read and not yet read enough lines */
      }		/* if timestamps have to be evaluated */

      cptr = line + strlen(line) - 1;
      if(*cptr == '\n')
	*cptr = '\0';

      if(!strncmp(cptr = first_nospace(line), "##", 2)){
		/* if the client identifier is not given on the command */
			/* line, the one in the index is used, if found */
	strcpy(new_identity, "");
	if(!read_header_line(cptr, NULL, &newtim, NULL, NULL, new_identity)){
	  prev_bu_time = newtim;

	  if(matches)
	    force_new_run = YES;		/* force a new client start */
	}

	if(identity_from_arg)
	  strcpy(new_identity, "");

	if(strcmp(new_identity, "")){
	  ZFREE(identity);
	  EM__(identity = strdup(new_identity));
	}

	continue;
      }

      fileuid = - MAXINT;
      filemtime = UNSPECIFIED_TIME;

      ZFREE(act_dataloc.server);
      SETZERO(act_dataloc);
      act_dataloc.port = -1;
      i = eval_index_line(line, &act_dataloc.server, &act_dataloc.port,
		&act_dataloc.cart, &act_dataloc.file, &fileuid, &filemtime);

      if(act_dataloc.port == -1)
	act_dataloc.port = DEFAULT_PORT;
      if(!act_dataloc.server)
	EM__(act_dataloc.server = strdup(backuphosts[0]));

      if(!INDEX_FILEENTRY(i))
	continue;
      if(!line[i])
	continue;

      if(curuid && curuid != fileuid)
	continue;

      if(restoretapes)
	if(!in_Uns32Ranges(restoretapes, act_dataloc.cart))
	  continue;

      if(have_timerange){
	if(filemtime != UNSPECIFIED_TIME){
	  if((time_newer > 0 && filemtime < time_newer)
		|| (time_older > 0 && filemtime > time_older))
	    continue;
	}
	else{
	  if((time_newer > 0 && current_bu_time < time_newer)
		|| (time_older > 0 && prev_bu_time > time_older))
	    continue;
	}
	if(butime_after > 0 && current_bu_time < butime_after)
	  continue;
	if(butime_before > 0 && current_bu_time > butime_before)
	  continue;
      }

      cptr = line + i;

      for(i = 0; i < num_restore_paths; i++){
	if(!fn_match(restore_paths[i], cptr, matchflags)){
	  if(have_timerange && filemtime != UNSPECIFIED_TIME)
	    have_mtime_idx = YES;

	  total_num_matches++;

	  if(!do_raw_listing){
	    added = do_client_run = cp_prev = clr_prev = NO;

	    if(!nf)
		nf = nc = 1;	/* these are only used for printout */

	    if(!prev_dataloc.cart){
		COPYVAL(prev_dataloc, act_dataloc);
		EM__(prev_dataloc.server = strdup(act_dataloc.server));
		force_new_run = NO;

		EM__(dataareas = NEWP(DataArea, 1));
		SETZERO(dataareas[0]);
		if(serverid_from_netaddr(&(dataareas[0].serveridtab),
			act_dataloc.server, act_dataloc.port, server_ids_file)){
		  EM__(dataareas[0].serveridtab.servername
						= strdup(act_dataloc.server));
		  dataareas[0].serveridtab.serverport = act_dataloc.port;

		  logmsg(T_("Warning: For server %s, port %d (found in index) no server ID is known in file `%s'.\n"),
				act_dataloc.server, (int) act_dataloc.port,
				server_ids_file);
		}
		num_dataareas = 1;
		EM__(dataareas[0].carts = add_to_Uns32Ranges(dataareas[0].carts,
					act_dataloc.cart, act_dataloc.cart));
	    }
	    if(!prev_dataloc.efile)
		prev_dataloc.efile = act_dataloc.file;

	    sh = same_host(act_dataloc.server, prev_dataloc.server);
	    sp = (act_dataloc.port == prev_dataloc.port);
	    sc = (act_dataloc.cart == prev_dataloc.cart);
#if 0		/* why the heck did i do this ? anyone any idea ? */
	    sf = (act_dataloc.file == prev_dataloc.file && !current_bu_time)
		|| ((act_dataloc.file == prev_dataloc.file
				|| act_dataloc.file == prev_dataloc.file + 1
					|| prevlineno + 1 == lineno)
				&& current_bu_time);
#else
	    sf = (act_dataloc.file == prev_dataloc.file
				|| act_dataloc.file == prev_dataloc.file + 1
					|| prevlineno + 1 == lineno);
#endif
	    if(sh <= 0 || !sp || !sc){
		nf++;

		SETZERO(cur_serveridtab);
		serverid_from_netaddr(&cur_serveridtab, act_dataloc.server,
				act_dataloc.port, server_ids_file);
		for(j = 0; j < num_dataareas; j++){
		  if(dataareas[j].serveridtab.serverid && cur_serveridtab.serverid){
		    if(!strcmp(dataareas[j].serveridtab.serverid,
							cur_serveridtab.serverid)){
			break;
		    }
		  }
		  else{
		    if(same_host(act_dataloc.server,
					dataareas[j].serveridtab.servername) > 0
			&& act_dataloc.port == dataareas[j].serveridtab.serverport){
		      break;
		    }
		  }
		}
		ZFREE(cur_serveridtab.servername);
		ZFREE(cur_serveridtab.serverid);

		if(j >= num_dataareas){
		  EM__(dataareas = RENEWP(dataareas, DataArea, num_dataareas + 1));
		  SETZERO(dataareas[num_dataareas]);

		  if(serverid_from_netaddr(&(dataareas[num_dataareas].serveridtab),
			act_dataloc.server, act_dataloc.port, server_ids_file)){
		    EM__(dataareas[num_dataareas].serveridtab.servername
						= strdup(act_dataloc.server));
		    dataareas[num_dataareas].serveridtab.serverport = act_dataloc.port;

		    logmsg(T_("Warning: For server %s, port %d (found in index) no server ID is known in file `%s'.\n"),
				act_dataloc.server, (int) act_dataloc.port,
				server_ids_file);
		  }
		  num_dataareas++;
		}

		if(!in_Uns32Ranges(dataareas[j].carts, act_dataloc.cart)){
		  nc++;
		  EM__(dataareas[j].carts = add_to_Uns32Ranges(dataareas[j].carts,
					act_dataloc.cart, act_dataloc.cart));
		}
	    }
	    else if(act_dataloc.file != prev_dataloc.file){
		nf++;
	    }

	    if(only_list){
		cp_prev = YES;

		if(do_listing){
		  if(fmt_repls){
		    dirp = cptr;
		    cptr = repl_fmts(formatstr, fmt_repls, num_fmt_repls,
					line, prev_bu_time);
		    fprintf(stdout, "%s", cptr);
		    if(cptr != dirp)
			free(cptr);
		  }
		  else
		    fprintf(stdout, "%s\n", cptr);
		}
	    }
	    else{
	      same_loc = sc && sp && (sh > 0) && sf;

	      j = num_matches + 1 + ((!same_loc || force_new_run) ? 1 : 0);
	      newmatches = ZRENEWP(matches, UChar *, j);
	      newnames = ZRENEWP(names, UChar *, j);
	      if(newmatches)
		matches = newmatches;
	      if(newnames)
		names = newnames;
	      if(!newmatches || !newnames){
		if(num_matches < 10)
		  ENM__;

		do_client_run = YES;
	      }

	      if(do_client_run){
		num_rest = total_num_matches - 1;

		fprintf(stdout, T_("Found %d matches on %d servers, %d tapes, %d"
			" files up to now.\n This is not the last pack, please"
			" be patient.\n"), (int) num_rest,
			(int) num_dataareas, (int) nc, (int) nf);

		j = num_matches;
		n = write_to_restore(matches, names, &j);
		num_rest -= (num_matches - j);
		if(!n){			/* write_to_restore frees matches */
		  if(num_matches > j){
		    fprintf(stdout,
			T_("Detected %d duplicate entries, only the newest ones are restored.\n"),
						(int) (num_matches - j));
		  }
		  fprintf(stdout, T_("Successfully restored %d filesystem entries"
			", %d totally from %d servers, %d tapes and %d files, "
				"continuing.\n"), (int) num_matches,
			(int) num_rest, (int) num_dataareas, (int) nc, (int) nf);
		}
		else{
		  errmsg(T_("Errors occurred during restore of the previous %d"
			" files. See above for details.\n"), (int) num_matches);
		  resterr++;
		}

		matches = NEWP(UChar *, 1);
		names = NEWP(UChar *, 1);
		if(!matches || !names)
		  ENM__;
		force_new_run = NO;

	 	num_matches = allocated = 0;
		prev_dataloc.efile = act_dataloc.file;

		maxalloc = MAXALLOC_FIRST;
		alloc_limit_reached = NO;
	      }
	      else{
		if(!same_loc || force_new_run)
		  matches[num_matches++] = NEW_CLIENT_START_MARK;
		force_new_run = NO;
	      }
	      cp_prev = YES;

	      EM__(matches[num_matches] = strdup(line));
	      names[num_matches] = matches[num_matches] + (cptr - line);
	      if(matches[num_matches]){
		num_matches++;
		added = YES;
		allocated += strlen(line) + 1;
	      }
	      if(allocated > maxalloc){	/* try to get 5 more M of real mem */
		j = bytes_free_real_mem_pag(MAXALLOC_PROBESTEP, MAXALLOC_STEP);
		if(j < MAXALLOC_STEP)
		  alloc_limit_reached = YES;
		else
		  maxalloc += j / 2;	/* if ok: allow to use the half */
	      }
	      if((allocated > maxalloc && alloc_limit_reached) || !added){
		num_rest = total_num_matches - (added ? 0 : 1);

		fprintf(stdout, T_("Found %d matches on %d servers, %d tapes, %d"
			" files up to now.\n This is not the last pack, please"
			" be patient.\n"), (int) num_rest,
			(int) num_dataareas, (int) nc, (int) nf);

		j = num_matches;
		n = write_to_restore(matches, names, &j);
		num_rest -= (num_matches - j);
		if(!n){			/* write_to_restore frees matches */
		  if(num_matches > j){
		    fprintf(stdout,
			T_("Detected %d duplicate entries, only the newest ones are restored.\n"),
						(int) (num_matches - j));
		  }
		  fprintf(stdout, T_("Successfully restored %d filesystem entries"
			", %d totally from %d servers, %d tapes and %d files, "
				"continuing.\n"), (int) num_matches,
			(int) num_rest, (int) num_dataareas, (int) nc, (int) nf);
		}
		else{
		  errmsg(T_("Errors occurred during restore of the previous %d"
			" files. See above for details.\n"), (int) num_matches);
		  resterr++;
		}

		matches = NEWP(UChar *, 1);
		names = NEWP(UChar *, 1);
		if(!matches || !names)
		  ENM__;
		force_new_run = NO;

		if(!added){
		  EM__(matches[0] = strdup(line));
		  names[0] = matches[0] + (cptr - line);
		}
		num_matches = added ? 0 : 1;
		allocated = added ? 0 : strlen(line) + 1;
		clr_prev = added;
		cp_prev = ! added;
		prev_dataloc.efile = added ? 0 : act_dataloc.file;

		maxalloc = MAXALLOC_FIRST;
		alloc_limit_reached = NO;
	      }
	    }

	    if(cp_prev || clr_prev)
		ZFREE(prev_dataloc.server);
	    if(clr_prev)
		SETZERO(prev_dataloc);
	    if(cp_prev){
		act_dataloc.efile = prev_dataloc.efile;	/* to keep it */
		COPYVAL(prev_dataloc, act_dataloc);
		act_dataloc.server = NULL;
	    }
	    prev_dataloc.file = act_dataloc.file;
	  }
	  else{			/* do_raw_listing */
	    fprintf(stdout, "%s\n", cptr);
	  }
		/* if successive lines match, don't start new collect */
	  prevlineno = lineno;

	  break;
	}
      }
    }

    fclose(fp);
    if(pid >= 0){
      waitpid_forced(pid, &pst, 0);

      if(WEXITSTATUS(pst)){
	errmsg(T_("Warning: Uncompressing `%s' returned bad exit status.\n"),
			zindexfiles[cpptr - indexfiles]);
      }
    }
  }

  ZFREE(line);
  ZFREE(auxline);
  ncall = auxncall = 0;

  if(!do_raw_listing){
    if(do_counting || !do_listing || do_listcarts){
	if(total_num_matches > 0){
	  if(do_counting){
	    fprintf(stdout, T_("Found %d matches on %d servers, %d tapes, %d files.\n"),
		(int) total_num_matches, (int) num_dataareas, (int) nc, (int) nf);
	  }
	  if(do_listcarts){
	    n = strlen(T_("Server"));
	    for(i = 0; i < num_dataareas; i++){
		j = strlen(dataareas[i].serveridtab.servername);
		if(j > n)
		  n = j;
	    }
	    j = strlen(T_("Port"));
	    if(j < 6)
		j = 6;
	    fprintf(stdout, "%-*s %-*s %s\n", (int) n, T_("Server"),
			(int) j, T_("Port"),
			T_("Required Tapes"));
	    for(i = - strlen(T_("Required Tapes")); i < n + j + 2; i++){
		fprintf(stdout, "-");
	    }
	    for(i = 0; i < num_dataareas; i++){
		fprintf(stdout, "\n%-*s %-*d ",
			(int) n, dataareas[i].serveridtab.servername,
			(int) j, dataareas[i].serveridtab.serverport);
		pack_Uns32Ranges(dataareas[i].carts, NULL);
		fprint_Uns32Ranges(stdout, dataareas[i].carts, 0);
	    }
	    fprintf(stdout, "\n");
	  }
	}
	else{
	  fprintf(stdout, T_("No matches found.\n"));
	}
    }

    if(!only_list){
      if(total_num_matches > 0){
	j = num_matches;
	n = write_to_restore(matches, names, &j);
	total_num_matches -= (num_matches - j);
	if(!n){
	  if(num_matches > j){
	    fprintf(stdout,
		T_("Detected %d duplicate entries, only the newest ones are restored.\n"),
					(int) (num_matches - j));
	  }
	  fprintf(stdout, T_("Successfully restored %d filesystem entries"
		" from %d servers, %d tapes and %d files.\n"),
		(int) total_num_matches, (int) num_dataareas, (int) nc, (int) nf);
	}
	else{
	  errmsg(T_("Errors occurred during restore of the previous %d matches."
		" See previous output for details.\n"), (int) num_matches);
	  resterr++;
	}
      }
    }
  }

  ZFREE(act_dataloc.server);
  ZFREE(prev_dataloc.server);
  for(i = 0; i < num_dataareas; i++){
    ZFREE(dataareas[i].serveridtab.servername);
    ZFREE(dataareas[i].serveridtab.serverid);
    ZFREE(dataareas[i].carts);
  }
  ZFREE(dataareas);

  if(resterr)
    errmsg(T_("Errors occurred during restore. See previous output for details.\n"));

  do_exit(resterr ? 3 : 0);
}

static UChar *
repl_fmts(
  UChar		*fmt,
  ReplSpec	*repls,
  Int32		num_repls,
  UChar		*str,
  time_t	butim)
{
  Int32		i, n, port, cart, file;
  int		uid;
  time_t	mtim;
  UChar		*server = NULL, *newstr;
  UChar		portstr[16], cartstr[16], filestr[16], uidstr[16];
  UChar		mtimestr[32], Mtimestr[32], btimestr[32], Btimestr[32];
  UChar		loginstr[32];
  struct passwd	*uent;

  port = cart = file = uid = -1;
  mtim = UNSPECIFIED_TIME;

  i = eval_index_line(str, &server, &port, &cart, &file, &uid, &mtim);
  if(i <= 0)
    return(str);

  newstr = strdup(fmt);
  if(!newstr)
    return(str);
  repl_esc_seq(newstr, '\\');

  if(port != -1)
    sprintf(portstr, "%d", (int) port);
  else
    strcpy(portstr, "<unknown>");
  if(cart != -1)
    sprintf(cartstr, "%d", (int) cart);
  else
    strcpy(cartstr, "<unknown>");
  if(file != -1)
    sprintf(filestr, "%d", (int) file);
  else
    strcpy(filestr, "<unknown>");
  if(uid != -1){
    sprintf(uidstr, "%d", (int) uid);
    uent = getpwuid(uid);
    if(uent)
	sprintf(loginstr, "%s", uent->pw_name);
    else
	sprintf(loginstr, "%d", (int) uid);
  }
  else{
    strcpy(uidstr, "<unknown>");
    strcpy(loginstr, uidstr);
  }
  if(mtim != UNSPECIFIED_TIME){
    time_t_to_intstr(mtim, mtimestr);
    strcpy(Mtimestr, ctime(&mtim));
    massage_string(Mtimestr);
  }
  else{
    strcpy(mtimestr, "<unknown>");
    strcpy(Mtimestr, mtimestr);
  }
  if(butim != UNSPECIFIED_TIME){
    time_t_to_intstr(butim, btimestr);
    strcpy(Btimestr, ctime(&butim));
    massage_string(Btimestr);
  }
  else{
    strcpy(btimestr, "<unknown>");
    strcpy(Btimestr, Btimestr);
  }

  for(n = 0; n < num_repls; n++){
    switch(repls[n].token[1]){
      case 'n':
	repls[n].repl = str + i;
	break;
      case 'b':
	repls[n].repl = FN_BASENAME(str + i);
	break;
      case 'f':
	repls[n].repl = filestr;
	break;
      case 'p':
	repls[n].repl = portstr;
	break;
      case 'h':
	repls[n].repl = server ? server : (UChar *) "<unknown>";
	break;
      case 'c':
	repls[n].repl = cartstr;
	break;
      case 'o':
	repls[n].repl = uidstr;
	break;
      case 'O':
	repls[n].repl = loginstr;
	break;
      case 'm':
	repls[n].repl = mtimestr;
	break;
      case 'M':
	repls[n].repl = Mtimestr;
	break;
      case 't':
	repls[n].repl = btimestr;
	break;
      case 'T':
	repls[n].repl = Btimestr;
	break;
    }
  }

  i = repl_substrings(&newstr, repls, num_repls);
  ZFREE(server);

  return(newstr);
}

static Int32
cmp_str_ptrs(void * p1, void * p2)
{
  UChar		*str1, *str2;

  str1 = **((UChar ***) p1);
  str2 = **((UChar ***) p2);

  if(str1 != NEW_CLIENT_START_MARK && str1 != EMPTY_PLACEHOLDER
	&& str2 != NEW_CLIENT_START_MARK && str2 != EMPTY_PLACEHOLDER)
    return(strcmp(str1, str2));

  if((str1 == NEW_CLIENT_START_MARK || str1 == EMPTY_PLACEHOLDER)
	&& str2 != NEW_CLIENT_START_MARK && str2 != EMPTY_PLACEHOLDER)
    return(1);
  if(str1 != NEW_CLIENT_START_MARK && str1 != EMPTY_PLACEHOLDER
	&& (str2 == NEW_CLIENT_START_MARK || str2 == EMPTY_PLACEHOLDER))
    return(-1);

  return(0);
}

static Int32
write_to_restore(
  UChar		**matches,
  UChar		**names,
  Int32		*n_matches)
{
  int		fd, pid, pst;
  Int32		i, j, ret = 0, lineno, portno, cartno, fileno, efileno;
  Int32		num_matches, new_num_matches, idx;
  UChar		***match_sort_list = NULL, *str1, *str2;
  UChar		**cpptr, *server = NULL, *flagstr;

  new_num_matches = num_matches = *n_matches;

  if(num_matches <= 0)
    return(0);

  if(!have_timerange || have_mtime_idx){	/* eliminate duplicates */
    match_sort_list = NEWP(UChar **, num_matches);
    if(match_sort_list){		/* if mem is available, sort and */
      for(i = 0; i < num_matches; i++)	/* go through the one-dimensional */
	match_sort_list[i] = (matches[i] == NEW_CLIENT_START_MARK ?
					matches : names) + i;
		/* list instead of the two dimensional search for dups */
      q_sort(match_sort_list, num_matches, sizeof(UChar **), cmp_str_ptrs);

      for(i = 0, j = 1; j < num_matches; i++, j++){
	str1 = *(match_sort_list[i]);
	str2 = *(match_sort_list[j]);
	if(str1 != NEW_CLIENT_START_MARK && str1 != EMPTY_PLACEHOLDER
		&& str2 != NEW_CLIENT_START_MARK && str2 != EMPTY_PLACEHOLDER){
	  if(!strcmp(str1, str2)){	/* if the pointed strings are == */
	    if(match_sort_list[i] > match_sort_list[j]){	/* put the */
		cpptr = match_sort_list[i];			/* smaller */
		match_sort_list[i] = match_sort_list[j];	/* pointer */
		match_sort_list[j] = cpptr;		/* earlier in list */
	    }

	    idx = match_sort_list[i] - names;
	    free(matches[idx]);			/* and mark the entry */
	    matches[idx] = EMPTY_PLACEHOLDER;	/* to be skipped s.b. */
	    match_sort_list[i] = matches + idx;
	    new_num_matches--;
	  }
	}
      }

      free(match_sort_list);
    }
    else{
      for(i = num_matches - 1; i > 0; i--){
	for(j = i - 1; j >= 0; j--){
	  if(matches[i] != NEW_CLIENT_START_MARK
			&& matches[j] != NEW_CLIENT_START_MARK){
	    if(!strcmp(names[i], names[j])){
	      num_matches--;
	      free(matches[j]);
	      memmove(matches + j, matches + j + 1,
				(num_matches - j) * sizeof(matches[0]));
	      memmove(names + j, names + j + 1,
				(num_matches - j) * sizeof(names[0]));
	      i--;
	    }
	  }
	}
      }

      new_num_matches = num_matches;
    }
  }

  *n_matches = new_num_matches;

  fd = -1;
  lineno = 0;

  forever{
    while(lineno < num_matches){
      if(matches[lineno] != NEW_CLIENT_START_MARK
				&& matches[lineno] != EMPTY_PLACEHOLDER)
	break;
      lineno++;
    }

    if(lineno >= num_matches)
	break;

    if(fd < 0){
	ZFREE(server);
	i = eval_index_line(matches[lineno],
			&server, &portno, &cartno, &fileno, NULL, NULL);
	if(!INDEX_FILEENTRY(i)){
	  errmsg(T_("Error: no fileentry given to write_to_server:\n> %s\n"),
				matches[lineno]);
	  free(matches[lineno]);
	  continue;
	}

	i = 60 + (curngids > 0 ? curngids * 20 : 0);
	EM__(flagstr = NEWP(UChar, i));

	strcpy(flagstr, "xvg");
	if(!preserve_existing)
	  strcat(flagstr, "u");
	if(chk_perms)
	  strcat(flagstr, "Z");
	if(curuid){
	  sprintf(flagstr + strlen(flagstr), " -o %d:%d",
					(int) curuid, (int) curgid);
	  for(i = 0; i < curngids; i++)
	    sprintf(flagstr + strlen(flagstr), "%c%d",
				(i ? ',' : ':'), (int) curgids[i]);
	}
	compose_clntcmd(tmpbuf, flagstr,
		identity, server, portno, 0, cryptfile, cartno, fileno,
		time_older, time_newer, 0 /* set above */, restoreroot);

	free(flagstr);

	start_msg_proc_msg(server, - portno);

	if( (fd = open_to_pipe(tmpbuf, NULL, 1 + 2, &pid, 0)) < 0){
	  errmsg(T_("Error: Cannot start client subprocess.\n"));
	  do_exit(18);
	}

	set_closeonexec(fd);

	SETZERO(scandir_args);
	scandir_args.fd = fd;

	for(j = lineno + 1; j < num_matches; j++)
	  if(matches[j] == NEW_CLIENT_START_MARK)
	    break;
	j--;
	while(matches[j] == EMPTY_PLACEHOLDER)
	  j--;
	eval_index_line(matches[j], NULL, NULL, NULL, &efileno, NULL, NULL);

	if(fileno == efileno)
	  fprintf(stdout,
		T_("Restoring files from server %s, port %d, cartridge %d, file %d\n"),
				server, (int) portno, (int) cartno, (int) fileno);
	else
	  fprintf(stdout,
		T_("Restoring files from server %s, port %d, cartridge %d, files %d - %d\n"),
			server, (int) portno, (int) cartno,
				(int) fileno, (int) efileno);

	fflush(stdout);
    }

    for(cpptr = names + lineno; fd >= 0; lineno++, cpptr++){
      if(matches[lineno] != EMPTY_PLACEHOLDER){	/* skip eliminated entries */
	if(!(j = (lineno >= num_matches)))
	  j = (matches[lineno] == NEW_CLIENT_START_MARK);
	if(j){
	  close(fd);
	  waitpid_forced(pid, &pst, 0);

	  stop_msg_proc();

	  if( (pst = WEXITSTATUS(pst)) ){
	    fprintf(stderr, T_("Errors occurred during restore. See previous output for details.\n"));
	    ret = pst;
	  }

	  if(fileno == efileno)
	    fprintf(stdout,
		T_("Done with server %s, port %d, cartridge %d, file %d\n"),
				server, (int) portno, (int) cartno, (int) fileno);
	  else
	    fprintf(stdout,
		T_("Done with server %s, port %d, cartridge %d, files %d - %d\n"),
			server, (int) portno, (int) cartno,
				(int) fileno, (int) efileno);

	  fd = -1;
	}
	else{
	  write_forced(fd, *cpptr, strlen(*cpptr));
	  write_forced(fd, "\n", 1);

		/* write_filename(cptr, (void *) (& scandir_args), NULL); */
	  free(matches[lineno]);
	}
      }
    }
  }

  fflush(stdout);

 cleanup:
  free(names);
  free(matches);
  ZFREE(server);

  return(ret);
}

static void
restoreall(int c, char ** v)
{
  UChar		**cpptr, *cptr, *line = NULL, *prevlast = NULL;
  Flag		server_changed, found;
  UChar		*filename, *line2 = NULL, *server = NULL, *oldserver = NULL;
  UChar		*unzipfile = NULL, **unzipfilecont = NULL, *unzipcmd;
  FILE		*fp, *cfp;
  int		fd, pid, pst, n, cpid, cfd;
  Int32		i, cartno, fileno, oldcart, oldfile, portno, oldport;
  Int32		ncall = 0, ncall2 = 0;
  Int32		pllen;	/* uninitialized ok */

  oldcart = oldfile = oldport = -1;

  for(cpptr = indexfiles; *cpptr; cpptr++){
    pid = -1;
    fd = open(*cpptr, O_RDONLY);
    if(fd < 0){
	unzipcmd = idxuncompresscmd[0] ? idxuncompresscmd : (UChar *) "gunzip";
	i = cpptr - indexfiles;

	unzipfile = hidden_filename(zindexfiles[i]);
	if(!eaccess(unzipfile, R_OK)){
	  unzipfilecont = read_asc_file(unzipfile, 0);
	  if(unzipfilecont)
	    if(unzipfilecont[0])
		unzipcmd = unzipfilecont[0];
	}

	fd = open_from_pipe_sigblk(unzipcmd, zindexfiles[i], 1,
					&pid, &idxproc_blk_sigs);

	ZFREE(unzipfile);
	free_asc_file(unzipfilecont, 0);
	unzipfilecont = NULL;

	if(fd < 0)
	  continue;
    }
    set_closeonexec(fd);

    fp = fdopen(fd, "r");
    if(!fp){
	errmsg(T_("Error: Cannot read file `%s'.\n"), *cpptr);
	continue;
    }
    found = NO;
    while(!feof(fp)){	/* search input logfile lines */
      if(fget_realloc_str(fp, &line, &ncall))
	continue;

      ZFREE(server);
      portno = -1;
      n = eval_index_line(line, &server, &portno, &cartno, &fileno,
						NULL, NULL);
      if(!INDEX_FILEENTRY(n))
	continue;
      if(!line[n])
	continue;

      server_changed = (server && !oldserver) || (!server && oldserver)
		|| (server && oldserver && same_host(server, oldserver) <= 0);

	/* in this case error messages appear at end of client output */
      if(found && cartno == oldcart && fileno == oldfile && oldport == portno
					&& !server_changed)
	continue;

      oldport = portno;
      oldcart = cartno;
      oldfile = fileno;
      if(server_changed){
	ZFREE(oldserver);
	oldserver = server;
	server = NULL;
      }

      chop(line);
      filename = line + n;

      if(!prevlast || found){	/* if found or first line at all: restore */
	compose_clntcmd(tmpbuf, preserve_existing ? "xvanl" : "xvanlu",
			identity, oldserver, portno, 0,
			cryptfile, cartno, fileno, time_older, time_newer,
					curuid, restoreroot);

	start_msg_proc_msg(oldserver, - portno);

	if( (cfd = open_from_pipe(tmpbuf, NULL, 1 + 2, &cpid)) < 0){
	  errmsg(T_("Error: Cannot start client subprocess.\n"));
	  continue;
	}
	set_closeonexec(cfd);

	cfp = fdopen(cfd, "r");
	if(!cfp){
	  errmsg(T_("Error: Cannot read from restore subprocess.\n"));
	  do_exit(34);
	}

	fprintf(stdout,
		T_("Going to restore from server %s, port %d, cartridge %d, file %d\n"),
				server ? server : backuphosts[0], (int) portno,
						(int) cartno, (int) fileno);
	while(!feof(cfp)){
	  if(fget_realloc_str(cfp, &line2, &ncall2))
	    continue;

	  fprintf(stdout, "%s", line2);

	  n = eval_index_line(line2, NULL, NULL, NULL, NULL, NULL, NULL);
	  if(!INDEX_FILEENTRY(n))
	    continue;
	  if(!line2[n])
	    continue;

	  cptr = line2 + strlen(line2) - 1;
	  if(*cptr == '\n')
	  *cptr = '\0';

	  cptr = line2 + n;

	  if(!prevlast){
	    EM__(prevlast = strdup(cptr));
	    pllen = strlen(prevlast);
	  }
	  else{
	    if((n = strlen(cptr)) > pllen){
		EM__(prevlast = RENEWP(prevlast, UChar, n + 1));
		pllen = n;
	    }
	    strcpy(prevlast, cptr);
	  }
	}

	fclose(cfp);
	waitpid_forced(cpid, &pst, 0);
	if(WEXITSTATUS(pst))
	  fprintf(stderr, T_("Errors occurred during restore. See previous output for details.\n"));

	stop_msg_proc();

	found = NO;
      }

      if(!prevlast)
	break;

      if(!strcmp(prevlast, filename)){
	found = YES;
      }
      else{
	if(restoreroot){	/* must be more tolerant, afclient might */
	  mkrelpath(filename);	/* output a relative path, what makes sense */

	  if(!strcmp(prevlast, filename))
	    found = YES;
	}
      }
    }

    fclose(fp);
    if(pid >= 0){
      waitpid_forced(pid, &pst, 0);
      if(WEXITSTATUS(pst)){
	fprintf(stderr,
		T_("Warning: Uncompressing `%s' returned bad exit status.\n"),
			*(cpptr + 3));
      }
    }

    ZFREE(prevlast);
  }

  ZFREE(line);
  ZFREE(line2);

  do_exit(0);
}

Int32
read_header_line(
  UChar		*line,
  Int32		*mode,
  time_t	*starttime,
  Int32		*bunum,
  UChar		*hostname,
  UChar		*clientid)
{
  static UChar		re_initialized = 0;
  static RE_cmp_buffer	startre;

  time_t	read_time;
  Int32		read_mode;
  int		read_num, i;
  UChar		*cptr;

  if(!re_initialized){
    SETZERO(startre);
    re_compile_pattern(startpattern, strlen(startpattern), &startre);
    re_initialized = 1;
  }

  if(re_find_match(&startre, line, NULL, NULL) == 0){
    line = first_nospace(first_space(line));

    if(!strncasecmp(line, "inc", 3))
	read_mode = MODE_INCR_BACKUP;
    else if (!strncasecmp(line, "full", 4))
	read_mode = MODE_FULL_BACKUP;
    else
	return(1);

    if(mode)
	*mode = read_mode;

    for(i = 0; i < 3; i++){
	line = first_nospace(first_space(line));
    }
    cptr = strchr(line, ',');
    *cptr = '\0';

    read_time = time_from_datestr(line);
    if(read_time == UNSPECIFIED_TIME)
	return(1);
    if(starttime)
	*starttime = read_time;

    *cptr = ',';
    line = cptr + 1;
    cptr = "current number:";
    line = strstr(line, cptr);
    if(line){
	line += strlen(cptr);
	sscanf(line, "%d", &read_num);
	if(bunum)
	  *bunum = (Int32) read_num;
    }

    cptr = "hostname:";
    if(line)
	line = strstr(line, cptr);
    if(line){
	line = first_nospace(line + strlen(cptr));
	if(hostname)
	  sscanword(line, hostname);
    }
    else{
	if(hostname)
	  strcpy(hostname, "<unknown>");
    }

    cptr = "client id:";
    if(line)
	line = strstr(line, cptr);
    if(line){
	line = first_nospace(line + strlen(cptr));
	if(clientid)
	  line = sscanwordq(line, clientid);
    }
    else{
	if(clientid)
	  strcpy(clientid, hostname ? hostname : (UChar *) "<unknown>");
    }

    return(0);
  }

  return(1);
}

static Int32
cmp_tapeEntry_loc(void * l1, void * l2)
{
  BuTapeEntry	*e1, *e2;
  UChar		*s1, *s2;
  Int32		c, p1, p2;

  e1 = (BuTapeEntry *) l1;
  e2 = (BuTapeEntry *) l2;

  s1 = e1->server ? e1->server : backuphosts[serveridx];
  s2 = e2->server ? e2->server : backuphosts[serveridx];

  c = (same_host(s1, s2) > 0 ? 0 : 1);
  if(c)
    return(c);

  p1 = e1->port > 0 ? e1->port : 0;
  p2 = e2->port > 0 ? e2->port : 0;
  if(p1 != p2)
    return(p1 - p2);

  return(e1->cart - e2->cart);
}

#define	num_prev_carts		num_prev_backup
#define	num_prev_carts_str	num_prev_backup_str
#define	cartranges		restore_paths
#define	num_cartranges		num_restore_paths
#define	dont_restore		do_counting

static Int32
find_tapepos(
  Int32		*n_cart,
  Int32		*n_file,
  Int32		*n_carts,
  Int8		mode,
  UChar		*server,
  Int32		port)
{
  static Int32		num_buf_entries = 0;
  static BuTapeEntry	*buf_entries = NULL;

  Int32		i, nc, nf, ncs;

  if(!n_cart && !n_file && ! n_carts && mode == CLEAR_LIST){
    num_buf_entries = 0;
    return(0);
  }

  if(!server)
    server = backuphosts[serveridx];
  if(port <= 0)
    port = backupports[serveridx];
  if(port <= 0)
    port = 0;

  for(i = 0; i < num_buf_entries; i++){
    if(buf_entries[i].port == port
		&& same_host(buf_entries[i].server, server) > 0){
	if(buf_entries[i].bunum <= 0)
	  continue;
	if(n_carts)
	  *n_carts = buf_entries[i].bunum;

	if(mode == INSERT_POSITION){
	  if(buf_entries[i].inscart <= 0 || buf_entries[i].insfile <= 0)
	    continue;

	  if(n_cart)
	    *n_cart = buf_entries[i].inscart;
	  if(n_file)
	    *n_file = buf_entries[i].insfile;
	}

	if(mode == ACCESS_POSITION){
	  if(buf_entries[i].cart <= 0 || buf_entries[i].file <= 0)
	    continue;

	  if(n_cart)
	    *n_cart = buf_entries[i].cart;
	  if(n_file)
	    *n_file = buf_entries[i].file;
	}

	return(0);
    }
  }

  i = get_tapepos(&nc, &nf, &ncs, mode, server, port, NULL);
  if(i)
    return(i);

  buf_entries = ZRENEWP(buf_entries, BuTapeEntry, num_buf_entries + 1);
  if(!buf_entries)
    return(-1);

  SETZERO(buf_entries[num_buf_entries]);

  EM__(buf_entries[num_buf_entries].server = strdup(server));
  buf_entries[num_buf_entries].port = port;
  buf_entries[num_buf_entries].bunum = ncs;

  if(mode == INSERT_POSITION){
    buf_entries[num_buf_entries].inscart = nc;
    buf_entries[num_buf_entries].insfile = nc;
  }
  if(mode == ACCESS_POSITION){
    buf_entries[num_buf_entries].cart = nc;
    buf_entries[num_buf_entries].file = nf;
  }

  num_buf_entries++;

  if(n_carts)
    *n_carts = ncs;
  if(n_cart)
    *n_cart = nc;
  if(n_file)
    *n_file = nf;

  return(0);
}

/* like get_streamerstate, but with cache in buf_entries */
Int32
find_streamerstate(
  UChar		*state,
  UChar		*server,
  Int32		port,
  Flag		perrs)
{
  static Int32		num_buf_entries = 0;
  static BuTapeEntry	*buf_entries = NULL;

  StreamerStat	actstate;
  Int32		i;

  if(!state){
    num_buf_entries = 0;
    return(0);
  }

  if(!server)
    server = backuphosts[serveridx];
  if(port <= 0)
    port = backupports[serveridx];
  if(port <= 0)
    port = 0;

  for(i = 0; i < num_buf_entries; i++){
    if(buf_entries[i].port == port
		&& same_host(buf_entries[i].server, server) > 0){
	if(buf_entries[i].mode < 0)
	  continue;

	*state = (UChar) buf_entries[i].mode;
	return(0);
    }
  }

  if( (i = get_streamerstate(&actstate, server, port, perrs)) )
    return(i);

  ZFREE(actstate.needed_tapes);

  buf_entries = ZRENEWP(buf_entries, BuTapeEntry, num_buf_entries + 1);
  if(!buf_entries)
    return(-1);

  SETZERO(buf_entries[num_buf_entries]);

  EM__(buf_entries[num_buf_entries].server = strdup(server));
  buf_entries[num_buf_entries].port = port;
  buf_entries[num_buf_entries].mode = (Int32) actstate.devstatus;

  num_buf_entries++;

  return(0);
}

static Int32
sendrcv(
  int		fromfd,
  UChar		*frombuf,
  Int32		numfrom,
  int		tofd,
  UChar		*tobuf,
  Int32		numto)
{
  Int32		i, j;

  if(numto > 0){
    i = (write_forced(tofd, tobuf, numto) != numto);
    if(i)
      return(- errno);
  }
  if(numfrom > 0){
    j = (read_forced(fromfd, frombuf, numfrom) != numfrom);
    if(j)
      return(- errno);
  }

  return(0);
}

static Int32
simple_cmd(
  Int32		cmd,
  int		fromfd,
  int		tofd)
{
  UChar		cmdb, res;
  Int32		i;

  cmdb = (UChar) cmd;
  i = sendrcv(fromfd, &res, 1, tofd, &cmdb, 1);
  return(i ? i : (Int32) res);
}

static void
restoreem(int c, char ** v)
{
  UChar		**lines = NULL, *line = NULL, *zindexfile, **servers = NULL;
  UChar		*tmpidxfile, *tmpzidxfile;
  UChar		*cptr, *cptr2, *mininfoline = NULL, *unzipfile;
  Flag		noservavail, success, havelastfull;
  UChar		*clientname = NULL, *servername, *filename, *rfilename;
  Int32		num_lines = 0, serverport, cartno, fileno;
  int		i, j, k, l, n, act_bunum = -1, nump, isp, bunum;
  FILE		*fp;
  int		fd, pid, opid, pst, ofd, pp[2];
  Int32		num_backups, *carts, *files, *ports, *bunums, stopidx;
  Int32		num_search_entries = 0, idx, ncall = 0;
  Int32		actinscart, inscart, num_to_search;
  Int32		latest_bupart, num_latest_buparts, startidx;	/* uninitialized OK */
  Int32		earliest_bunum, numcarts = 1, actcart, f;
  Int32		latest_bunum_cart, earliest_bunum_cart, latest_bunum;
  UChar		min_restore_info[1000];
  BuTapeEntry	*search_entries = NULL, *tmpentries;
  StreamerStat	actstate;

  if(curuid){
    errmsg(T_("Error: Emergency restore may only be performed by root.\n"));
    do_exit(19);
  }

  min_restore_info[0] = '\0';

  if(restore_EM){		/* emergency restore without min info */
    time_t	latest_timestamp;


    if(num_cartranges){		/* evaluate args starting with a number */
	for(i = 0; i < num_cartranges; i++){
	  if(sscanf(cartranges[i], "%d-%d", &j, &k) < 2){	/* x-y ? */
	    if(sscanf(cartranges[i], "%d", &j) <= 0){		/* no, x */
		errmsg(T_("Warning: Cannot classify option `%s', ignored.\n"),
						cartranges[i]);
		continue;
	    }

	    if(j < 0){		/* erroneously found an additional arg. */
		num_prev_carts++;	/* we simply ignore it and let */
		continue;		/* the next loop process it - */
	    }			/* makes life easier, as it's hard enough */

	    k = j;
	  }
	  if(k < j)			/* wrong order ? */
	    l = k, k = j, j = l;
	  EM__(search_entries = ZRENEWP(search_entries, BuTapeEntry,
					num_search_entries + k - j + 1));

	  l = -1;		/* check, if we have ...@host%port */
	  if( (cptr = strstr(cartranges[i], SEPLOC)) ){
	    cptr++;
	    cptr2 = strstr(cptr, PORTSEP);
	    if(cptr2)
		*cptr2 = '\0';

	    if(!isfqhn(cptr))
		cptr = NULL;
	    else{
		EM__(cptr = strdup(cptr));

		if(cptr2)
		  l = get_tcp_portnum(cptr2 + 1);
	    }
	  }
	  for(; j <= k; num_search_entries++, j++){
	    search_entries[num_search_entries].cart = j;
	    search_entries[num_search_entries].server = cptr;
	    search_entries[num_search_entries].port = l;
	  }
	}
    }
    if(num_prev_carts){	/* evaluate args starting with - and a number */
      f = FIRST_NUM_ARG | IGNORE_NUM_ARG_TRAILERS;
      while((num_prev_carts = get_arg_num(c, v, &idx, f)) >= 0){
	f = IGNORE_NUM_ARG_TRAILERS;
	num_prev_carts_str = v[idx];

	EM__(search_entries = ZRENEWP(search_entries, BuTapeEntry,
				num_search_entries + num_prev_carts + 1));

	l = -1;			/* check, if we have ...@host%port */
	if( (cptr = strstr(num_prev_carts_str, SEPLOC)) ){
	  cptr++;
	  cptr2 = strstr(cptr, PORTSEP);
	  if(cptr2)
	    *cptr2 = '\0';

	  if(!isfqhn(cptr))
	    cptr = NULL;
	  else{
	    EM__(cptr = strdup(cptr));

	    if(cptr2)
		l = get_tcp_portnum(cptr2 + 1);
	  }
	}

	start_msg_proc_msg(cptr, l < 0 ? 0 : - l);

	if(find_tapepos(&actinscart, NULL, &numcarts,
						INSERT_POSITION, cptr, l)){
	  errmsg(T_("Error: Cannot determine current tape writing position.\n"));
	  do_exit(26);
	}

	stop_msg_proc();

	if(num_prev_carts + 1 > numcarts){
	  errmsg(T_("Warning: Too many previous cartridges specified, reset to %d.\n"),
				(int) numcarts - 1);
	  num_prev_carts = numcarts - 1;
	}

	for(i = actinscart - num_prev_carts; i <= actinscart; i++){
	  search_entries[num_search_entries].cart = (i >= 1 ? i : (numcarts + i));
	  search_entries[num_search_entries].server = cptr;
	  search_entries[num_search_entries].cart = l;
	  num_search_entries++;
	}
      }
    }
		/* set all names, so we don't have to deal with NULL later */
    for(i = 0; i < num_search_entries; i++){
	if(!search_entries[num_search_entries].server){
	  EM__(cptr = search_entries[num_search_entries].server =
					strdup(backuphosts[serveridx]));
	}
    }

		/* There is a special case to handle: no cartridge numbers,
		 * servers etc. are supplied (num_search_entries == 0),
		 * then we step through the known backup servers going back
		 * through the cartridges beginning with the current insert
		 * cartridge on that server. We make a list, that only
		 * contains entries for server and port. In the loop below
		 * that special case is caught  and the loop variable is
		 * always forced to 0, what leads to infinite repetition.
		 * The cartridge entry is repeatedly updated before use.
		 * The stopidx is misused as counter for the cartridges
		 * incremented each run of the loop. The loop ends, if
		 * either all cartridges are through or it seems, enough
		 * cartridges have been read.
		 */

    if(num_search_entries > 0){
	q_sort(search_entries, num_search_entries,
				sizeof(BuTapeEntry), cmp_tapeEntry_loc);

	for(i = 0; i < num_search_entries - 1; i++){
	  if(!cmp_tapeEntry_loc(search_entries + i, search_entries + i + 1)){
	    ZFREE(search_entries[i].server);
	    memmove(search_entries + i, search_entries + i + 1,
			sizeof(BuTapeEntry) * (num_search_entries - i - 1));
	    num_search_entries--;
	    i--;
	  }
	}

	num_to_search = num_search_entries;
    }
    else{
	EM__(search_entries = NEWP(BuTapeEntry, num_backuphosts));
	num_to_search = num_backuphosts;

	for(i = 0; i < num_backuphosts; i++){
	  search_entries[i].server = backuphosts[i];
	  search_entries[i].port = backupports[i];

	  start_msg_proc_msg(search_entries[i].server,
				- search_entries[i].port);

	  j = find_tapepos(&search_entries[i].inscart, NULL,
				&search_entries[i].bunum, INSERT_POSITION,
				search_entries[i].server,
				search_entries[i].port);

	  stop_msg_proc();

	  if(j){
	    fprintf(stderr, T_("Warning: Cannot get cartridge position from "
			"server %s, port %d\n"), search_entries[0].server,
					(int) search_entries[0].port);
	    break;
	  }
	}
    }

    latest_bunum_cart = earliest_bunum_cart = earliest_bunum = 0;
    latest_timestamp = (time_t) 0;

    while(num_to_search > 0){		/* search through the storage units */
	stopidx = -1;
	if(num_search_entries) forever{			/* pseudo loop */
	  noservavail = NO;		/* search given triples with same */
	  startidx = ++stopidx;			/* server and port number */
	  if(startidx >= num_to_search){	/* if no server available */
	    startidx = stopidx = 0;	/* take the first one and wait ... */
	    noservavail = YES;
	  }

	  while(!cmp_tapeEntry_loc(search_entries + startidx,
						search_entries + stopidx)){
	    if(stopidx + 1 >= num_to_search)
		break;

	    stopidx++;
	  }
	  if(noservavail)
	    break;

	  start_msg_proc_msg(search_entries[startidx].server,
				- search_entries[startidx].port);

	  i = get_streamerstate(&actstate, search_entries[startidx].server,
				search_entries[startidx].port, YES);

	  stop_msg_proc();

	  if(i)
	    continue;

	  ZFREE(actstate.needed_tapes);

	  i = actstate.devstatus & STREAMER_STATE_MASK;
	  if(i != STREAMER_READY && i != STREAMER_UNLOADED)
	    continue;

	  if(i == STREAMER_UNLOADED){	/* no cartridge inside streamer */
	    if(!(actstate.devstatus & STREAMER_CHANGEABLE))	/* can we load ? */
		continue;
							/* try to load ! */
	    compose_clntcmd(tmpbuf, NULL, identity,
				search_entries[startidx].server,
				search_entries[startidx].port, 0, cryptfile,
				search_entries[startidx].cart, 1, 0, 0, 0, NULL);
	    i = system(tmpbuf);
	    if(i < 0 || WEXITSTATUS(i))
		continue;		/* loading right cartridge failed */
	  }

	  start_msg_proc_msg(search_entries[startidx].server,
				- search_entries[startidx].port);

	  j = get_tapepos(&actcart, NULL, NULL, ACCESS_POSITION,
				search_entries[startidx].server,
				search_entries[startidx].port, NULL);

	  stop_msg_proc();

	  if(j)
	    continue;

	  start_msg_proc_msg(search_entries[startidx].server,
				- search_entries[startidx].port);

	  j = find_tapepos(&inscart, NULL, NULL, INSERT_POSITION,
				search_entries[startidx].server,
				search_entries[startidx].port);
	  stop_msg_proc();

	  if(j)
	    continue;

		/* ok, now we know the server, port number, state,
		 * cartridge in drive and which cartridges we need
		 * to scan. Now we're going to resort the sublist
		 * we have specified by startidx and stopidx, so
		 * we can probably start with the cartridge, that
		 * is currently in the drive.
		 */

	  for(i = startidx; i <= stopidx; i++){
	    if(actcart <= search_entries[i].cart)
		break;			/* we start with the i-th */
	  }
	  if(i <= stopidx && i > startidx){	/* i is valid */
	    EM__(tmpentries = NEWP(BuTapeEntry, num_to_search));

	    memcpy(tmpentries + startidx, search_entries + i,
				(stopidx + 1 - i) * sizeof(BuTapeEntry));
	    memcpy(tmpentries + startidx + stopidx + 1 - i,
					search_entries + startidx,
					(i - startidx) * sizeof(BuTapeEntry));
	    free(search_entries);
	    search_entries = tmpentries;
	  }
				/* this ends our semi-pseudo-loop, cause */
	  break;		/* we have a result here and can go on */
	}			/* and now really scan the tapes */

	if(!num_search_entries)		/* if no cartridges, hosts, ports */
	  startidx = stopidx = 0;	/* etc are given, step back */
	    
	for(idx = startidx; idx <= stopidx; idx++){	/* for sublist */
	  if(!num_search_entries){	/* no cartridges ... given */
					/* stopidx is misused as counter */
	    search_entries[0].cart = search_entries[0].inscart - stopidx;

	    if(search_entries[0].cart < 1){		/* rotate back */
	      search_entries[0].cart += search_entries[0].bunum;

	      if(search_entries[0].cart == search_entries[0].inscart)
		break;		/* we are through all cartridges */
	    }

	    idx = 0;		/* set index, also forces endless loop  */
	    stopidx++;		/* increment cartridge counter */
	  }

	  compose_clntcmd(tmpbuf, "J", identity,
			search_entries[idx].server,
			search_entries[idx].port, 0, cryptfile,
			search_entries[idx].cart, 1, 0, 0, 0, NULL);

	  start_msg_proc_msg(search_entries[idx].server,
				- search_entries[idx].port);

	  pid = -1;				/* start scan program */
	  i = open_from_to_pipe(tmpbuf, pp, 1, &pid);
	  if(i){
	    fprintf(stderr, T_("Error: Cannot scan cartridge %d.\n"),
						(int) search_entries[idx].cart);
	    if(pid >= 0)
		waitpid_forced(pid, &pst, 0);
	  }
	  else{
	    UChar	frombuf[DEFCOMMBUFSIZ + 4], tobuf[DEFCOMMBUFSIZ + 4];
	    UChar	actclientid[256];
	    UChar	*tapebuf, *prefix, *postfix, *datestr, *tptr, c;
	    Flag	eot;
	    Int32	tapeblocksize, num_bytes_read, tapefilenum;
	    time_t	timestamp;

	    set_closeonexec(pp[0]);
	    set_closeonexec(pp[1]);

	    i = simple_cmd(SETSERIALOP, pp[0], pp[1]);
	    i = simple_cmd(SETERRORONEOT, pp[0], pp[1]);

	    tobuf[0] = QUERYTAPEBLOCKSIZE;
	    i = sendrcv(pp[0], frombuf, 5, pp[1], tobuf, 1);
	    xref_to_Uns32(&tapeblocksize, frombuf);

	    EM__(tapebuf = NEWP(UChar, tapeblocksize + DEFCOMMBUFSIZ));

	    eot = NO;
	    tapefilenum = 0;

	    while(!eot){
	      i = simple_cmd(OPENFORRAWREAD, pp[0], pp[1]);
	      if(i){
		eot = YES;
	      }
	      else{
		tapefilenum++;
		num_bytes_read = 0;
		memset(tapebuf, 0, tapeblocksize * sizeof(UChar));

		while(num_bytes_read < tapeblocksize){
		  tobuf[0] = READFROMTAPE;
		  i = sendrcv(pp[0], frombuf, DEFCOMMBUFSIZ + 1, pp[1], tobuf, 1);
		  if(i || (j = frombuf[DEFCOMMBUFSIZ])){
		    if(j && j != ENDOFFILEREACHED && j != ENDOFTAPEREACHED){
			errmsg(T_("Error: Cannot read enough bytes from tapefile.\n"));
		    }

		    if(j == ENDOFTAPEREACHED)
			eot = YES;

		    break;
		  }

		  memcpy(tapebuf + num_bytes_read, frombuf,
						DEFCOMMBUFSIZ * sizeof(UChar));
		  num_bytes_read += DEFCOMMBUFSIZ;
		}

		tptr = tapebuf;
		while( (cptr = memfind(tptr, tapeblocksize - (tptr - tapebuf),
				EMREC_TAPE_PREFIX, l = strlen(EMREC_TAPE_PREFIX))) ){
		  tptr = (cptr += l);
		  prefix = strstr(cptr, EMREC_PREFIX);
		  postfix = strstr(cptr, EMREC_POSTFIX);
		  i = sscanf(cptr, "%d%d%n", &j, &k, &n);
		  if(prefix && postfix && postfix > prefix && i >= 2){
		    datestr = first_nospace(cptr + n);
		    c = *prefix;
		    *prefix = '\0';
		    timestamp = time_from_datestr(datestr);
		    *prefix = c;
		    postfix[strlen(EMREC_POSTFIX)] = '\0';

		    if(timestamp != UNSPECIFIED_TIME && strlen(datestr) == j
				&& parity_byte(datestr, strlen(datestr)) == k){
		      cptr = prefix + strlen(EMREC_PREFIX);
		      if(sscanwordq(cptr, actclientid)){
			if( (j = !strcmp(actclientid, identity)) ){
			  if(timestamp > latest_timestamp){
			    *postfix = '\0';
			    strcpy(min_restore_info, prefix);
			    latest_timestamp = timestamp;
			  }

			  fprintf(stdout, "Info: ");
			}

			fprintf(stdout, T_("Found minimum restore info for client %s, "
					"backup date: %s"), actclientid,
					ctime(&timestamp));

			if(j){
			  fprintf(stdout, "%s\n", min_restore_info);
			  break;
			}
		      }	/* if we got a client identifier */
		    }	/* if there is a valid date specifier and the parity is ok */
		  }	/* if there is prefix, postfix and two numbers: len and par */
		}	/* while there are more tape prefixes in the tape buffer */

		i = simple_cmd(CLOSETAPEN, pp[0], pp[1]);

		tobuf[0] = SKIPFILES;
		Uns32_to_xref(tobuf + 1, 1);
		i = sendrcv(pp[0], frombuf, 1, pp[1], tobuf, 5);
		if(i){
		  eot = 1;
		}
	      }		/* if we could open the tape for rawread */
	    }		/* while no end of tape */

	    i = simple_cmd(GOODBYE, pp[0], pp[1]);
	    close(pp[1]);
	    i = waitpid_forced(pid, &pst, 0);
	    close(pp[0]);

	    stop_msg_proc();

	    if(tapefilenum < 2){
		errmsg(T_("Warning: There seems to be no data on tape %d.\n"),
					(int) search_entries[idx].cart);
	    }
	    else{
		fprintf(stdout, T_("Scanned %d files on tape %d.\n"),
			(int) tapefilenum, (int) search_entries[idx].cart);
	    }
	  }		/* if we could start the client program */


	  if(!num_search_entries){	/* no cartridges ... given */
	    if(min_restore_info[0])
		break;
	  }
	}				/* foreach cartridge sublist */

	if(!num_search_entries)
	  startidx = stopidx = 0;
					/* remove sublist from list */
	memmove(search_entries + startidx, search_entries + stopidx + 1,
		(num_to_search - (stopidx + 1)) * sizeof(BuTapeEntry));
	num_to_search -= stopidx - startidx + 1;
    }			/* while we have storage units to search */

    if(!min_restore_info[0]){
	errmsg(T_("Error: No minimum restore info found for client with ID `%s', exiting.\n"),
				identity);
	do_exit(35);
    }

    fprintf(stdout, T_("\n%s minimum restore info of %s%s\n"),
			do_listing ? T_("Recommend") : T_("Using"),
			ctime(&latest_timestamp), min_restore_info);

    if(dont_restore || do_listing)	/* that's it with -n or -l option */
	do_exit(0);

    EM__(mininfoline = strdup(min_restore_info + strlen(EMREC_PREFIX)));

    ZFREE(search_entries);
  }
  else{			/* read backup storing positions from stdin */
    fp = stdin;

    while(!feof(fp)){				/* read until eof */
      if(fget_realloc_str(fp, &line, &ncall))
	continue;

      if(! strncmp(line, EMREC_PREFIX, strlen(EMREC_PREFIX))){
	cptr = line + strlen(EMREC_PREFIX);

	ZFREE(clientname);
	EM__(clientname = strdup(cptr));
	sscanwordq(cptr, clientname);
	if(strcmp(clientname, identity)){
	  if(verbose)
	    fprintf(stdout,
		T_("Skipping minimum restore info for client `%s': %s\n"),
			identity, cptr);
	  continue;
	}

	ZFREE(mininfoline);
	EM__(mininfoline = strdup(cptr));

	if(verbose)
	  fprintf(stdout,
		T_("Found minimum restore info for client `%s': %s\n"),
			identity, mininfoline);
      }
    }

    ZFREE(line);
    ncall = 0;

    if(!mininfoline){
	errmsg(T_("Error: No minimum restore info found for client with ID `%s', exiting.\n"),
				identity);
	do_exit(35);
    }

    fprintf(stdout, T_("Done reading input.\n"));
  }

  cptr = sscanwordq(mininfoline, NULL);
  EM__(servername = strword(cptr, 0));
  sscanf(word_start(cptr, 1), "%d%d%d", &i, &j, &k);
  serverport = i;
  cartno = j;
  fileno = k;
  EM__(filename = rfilename = strword(cptr, -1));

  if(restoreroot){
    while(FN_ISDIRSEP(*rfilename))
	rfilename++;
    if(!vardir_from_arg){
      while(FN_ISDIRSEP(*numfile))
	numfile++;
      while(FN_ISDIRSEP(*indexfilepart))
	indexfilepart++;
    }
  }

  if(empty_string(servername) || empty_string(filename)
		|| serverport <= 0 || cartno <= 0 || fileno <= 0){
    errmsg(T_("Format error in the following input line:\n%s\n"), mininfoline);
    do_exit(33);
  }

  free(mininfoline);

  if(verbose)
    fprintf(stdout, T_("Going to recover backup list file `%s' from "
		"server %s, port %d, cartridge %d, file %d\n"), rfilename,
		servername, (int) serverport, (int) cartno, (int) fileno);

  if(!eaccess(rfilename, F_OK)){
	EM__(cptr = tmp_name(rfilename));
	if(rename(rfilename, cptr))
	  errmsg(T_("Error: Could not rename file '%s`, will try to overwrite."
		" error: %s.\n"), rfilename, strerror(errno));

	fprintf(stderr,
		T_("Warning: Existing file `%s' is renamed to `%s'.\n"),
			rfilename, cptr);
	free(cptr);
  }

  EM__(cptr = strapp("x ", filename));
  compose_clntcmd(tmpbuf, cptr, identity, servername, serverport, 0,
		cryptfile, cartno, fileno, 0, 0, 0, restoreroot);
  free(cptr);

  start_msg_proc_msg(servername, - serverport);

  i = system(tmpbuf);

  stop_msg_proc();

  if(i < 0 || WEXITSTATUS(i)){
    errmsg(T_("Error: Extracting the backup list returned bad status %d.\n"),
				(int) i);
    do_exit(27);
  }
  if(eaccess(rfilename, R_OK)){
    errmsg(T_("Error: Recovering backup list file `%s' failed.\n"), rfilename);
    do_exit(32);
  }

  EM__(lines = read_asc_file(rfilename, &num_lines));

  cptr = FN_BASENAME(rfilename);
  if(strcmp(cptr, START_POSITIONS_FILE))
    unlink(rfilename);

  servers = NEWP(UChar *, num_lines + 1);
  ports = NEWP(Int32, num_lines + 1);
  carts = NEWP(Int32, num_lines + 1);	/* make cartridges and files list */
  files = NEWP(Int32, num_lines + 1);
  bunums = NEWP(Int32, num_lines + 1);
  if(!carts || !files || !ports || !servers || !bunums)
    ENM__;

  for(l = num_lines - 1, i = -1; l >= 0; l--){
    if(*(first_nospace(lines[l])) == '#')
	  continue;

    if(sscanf(lines[l], "%d", &i) > 0)
	break;
  }
  if(i < 0){
    errmsg(T_("Error: No backup specifier lines found in `%s'.\n"), rfilename);
    do_exit(23);
  }
  act_bunum = i;

  for(l = num_lines - 1; l >= 0; l--){
    if(*(first_nospace(lines[l])) == '#'){
	free(lines[l]);
	memmove(lines + l, lines + l + 1, (num_lines - l) * sizeof(UChar *));
	num_lines--;
	continue;
    }
  }

  havelastfull = NO;
  latest_bunum = -1;
  for(l = num_lines - 1; l >= 0; l--){
    i = sscanf(lines[l], "%d.%d/%d", &bunum, &isp, &nump);
    if(i > 0){
	if(!havelastfull){
	  if(i >= 3){
	    havelastfull = YES;
	    latest_bupart = isp;
	    num_latest_buparts = nump;
	  }
	}
	if(latest_bunum < 0){
	  latest_bunum = bunum;
	}

	if(havelastfull && latest_bupart == num_latest_buparts
						&& bunum < latest_bunum)
	  break;		/* we have a complete full_backup here */
	if(havelastfull && isp <= latest_bupart && bunum < latest_bunum
						&& ! one_more_for_restore)
	  break;		/* have all parts for a complete backup */
	if(bunum < latest_bunum - 1)
	  break;			/* not it's enough in any case */
    }
  }
  if(l < num_lines - 1)		/* the above loop stopped befor what we need */
    l++;

		/* fill the cartridges and files list from the read lines */
  for(num_backups = 0; l < num_lines; l++){
    success = (sscanf(lines[l], "%d", &bunum) >= 1);
    EM__(servers[num_backups] = strword(lines[l], 1));
    cptr = word_start(lines[l], 2);
    success &= (sscanf(cptr, "%d%d%d", &i, &j, &k) >= 3);

    if(!success){
	free(servers[num_backups]);
    }
    else{
	ports[num_backups] = i;
	carts[num_backups] = j;
	files[num_backups] = k;
	bunums[num_backups] = bunum;
	num_backups++;
    }
  }

  sprintf(tmpbuf, "%d", act_bunum);
  EM__(indexfile = strapp(indexfilepart, tmpbuf));
  EM__(zindexfile = strapp(indexfile, COMPRESS_SUFFIX));
  EM__(tmpidxfile = tmp_name(indexfile));
  EM__(tmpzidxfile = tmp_name(zindexfile));

  sprintf(tmpbuf, T_("Filename logfile for backup number %d"),
						(int) act_bunum);

  if(mkbasedir(indexfile, 0755, getuid(), getgid()) < 0){
    errmsg(T_("Error: cannot create directory for `%s'.\n"), indexfile);
    do_exit(46);
  }
  if(mkbasedir(numfile, 0755, getuid(), getgid()) < 0){
    errmsg(T_("Error: cannot create directory for `%s'.\n"), numfile);
    do_exit(47);
  }

  opid = -1;
  ofd = -1;
  if(compresslogfiles){
    ofd = open_to_pipe_sigblk(idxcompresscmd, tmpzidxfile, 1,
				&opid, 0600, &idxproc_blk_sigs);
  }
  if(ofd < 0){
    ofd = open_to_pipe(NULL, tmpidxfile, 1, NULL, 0600);
    if(ofd < 0){
	fprintf(stderr, T_("Error: Cannot open filename logfile.\n"));
    }
  }
  if(ofd >= 0)
    set_closeonexec(ofd);

  line = NULL;
  ncall = 0;

  for(i = 0; i < num_backups; i++){
    if(!restore_emlist){
      fprintf(stdout,
		T_("Going to restore files from server %s, port %d, cartridge %d, file %d.\n"),
				servers[i] ? servers[i] : backuphosts[0],
				(int) ports[i], (int) carts[i], (int) files[i]);

      compose_clntcmd(tmpbuf, preserve_existing ? "xvnlaOU" : "xvnlaOUu",
		identity, servers[i], ports[i], 0,
		cryptfile, carts[i], files[i], time_older, time_newer,
				0, restoreroot);
    }
    else{
      fprintf(stdout,
		T_("Going to restore filenames from server %s, port %d, cartridge %d, file %d.\n"),
				servers[i] ? servers[i] : backuphosts[0],
				(int) ports[i], (int) carts[i], (int) files[i]);

      compose_clntcmd(tmpbuf, "tnlOU", identity, servers[i], ports[i], 0,
				cryptfile, carts[i], files[i],
				time_older, time_newer, 0, NULL);
    }

    pid = -1;
    fd = open_from_pipe(tmpbuf, NULL, 1 + 2, &pid);
    fp = fdopen(fd, "r");
    if(fd < 0 || !fp){
      fprintf(stderr, T_("Error: Cannot restore files.\n"));
      if(fd >= 0)
	close(fd);
      if(pid >= 0)
	waitpid_forced(pid, &pst, 0);

      continue;
    }

    j = 0;
    while(!feof(fp)){
      if(fget_realloc_str(fp, &line, &ncall))
	continue;

      n = strlen(line);
      if(ofd >= 0){
	if(write(ofd, line, n) < n){
	  j++;
	  if(j < 10)
	    errmsg(T_("Warning: Writing to filename logfile failed.\n"));
	  if(j == 9)
	    errmsg(T_("Further warnings suppressed.\n"));
	}
      }

      n = eval_index_line(line, NULL, NULL, NULL, NULL, NULL, NULL);
      if(n >= 0)
	fprintf(stdout, "%s", line + n);
    }
    fclose(fp);
    waitpid_forced(pid, &pst, 0);

    if(WEXITSTATUS(pst)){
      errmsg(T_("Warning: Something went wrong during restore. See previous output for details.\n"));
    }

    write(ofd, "\n", 1);
  }

  if(ofd >= 0)
    close(ofd);
  if(opid >= 0){
    waitpid_forced(opid, &pst, 0);
    if(WEXITSTATUS(pst)){
	fprintf(stderr, T_("Warning: Processing `%s' returned bad status.\n"),
		indexfile);
    }

    unzipfile = hidden_filename(zindexfile);
    sprintf(tmpbuf,
	T_("File to contain index unprocess command for backup number %d"),
						(int) act_bunum);
    if(rename_if_exists(unzipfile, tmpbuf))
	do_exit(37);

    fp = fopen(unzipfile, "w");
    if(fp){
	fprintf(fp, "%s\n",
		(idxuncompresscmd[0] ? idxuncompresscmd : (UChar *) "gunzip"));
	fclose(fp);
    }
    else{
	fprintf(stderr, "Error: Cannot open file `%s'.\n", unzipfile);
    }
  }

  sprintf(tmpbuf, T_("Index file for backup number %d"), (int) act_bunum);   if(rename_if_exists(indexfile, tmpbuf)
		|| rename_if_exists(zindexfile, tmpbuf))
    do_exit(36);

  if(!eaccess(tmpidxfile, F_OK))
    if(rename(tmpidxfile, indexfile) < 0)
	fprintf(stderr,
		T_("Error: Cannot rename file `%s' to `%s'.\n"),
			tmpidxfile, indexfile);
  if(!eaccess(tmpzidxfile, F_OK))
    if(rename(tmpzidxfile, zindexfile) < 0)
	fprintf(stderr,
		T_("Error: Cannot rename file `%s' to `%s'.\n"),
			tmpzidxfile, zindexfile);

  if(!read_pint_file(numfile, &bunum)){	/* there is a number file !!! */
    k = MIN(bunum, act_bunum);		/* create missing indexes between */
    l = MAX(bunum, act_bunum);		/* currently created and the one */
    for(i = k; i <= l; i++){		/* found in the num file */
	sprintf(tmpbuf, "%d", i);
	EM__(indexfile = strapp(indexfilepart, tmpbuf));
	EM__(zindexfile = strapp(indexfile, COMPRESS_SUFFIX));
	EM__(unzipfile = hidden_filename(zindexfile));

	if(eaccess(indexfile, F_OK) && eaccess(zindexfile, F_OK)){
	  fd = open(indexfile, O_CREAT | O_WRONLY | O_TRUNC, 0600);
	  if(fd >= 0){			/* just "touch" the file */
	    close(fd);
	  }
	  else{
	    fprintf(stderr, T_("Error: Cannot create file `%s'.\n"), indexfile);
	  }
	  unlink(unzipfile);
	}

	free(indexfile);
	free(zindexfile);
	free(unzipfile);
    }
  }
	/* the current restored number is higher than in the file or there */
  if(bunum < act_bunum){	/* is no file (read_pint_file sets to 0) */
    if(write_uns_file(numfile, act_bunum)){
      fprintf(stderr, T_("Warning: Cannot write file `%s'.\n"), numfile);
    }
  }

  if(!paramfile && !vardir_from_arg){
    fprintf(stderr, T_("Warning: No configuration file has been read and no"
	" VarDirectory specified. The necessary files for this directory"
	" are put into the directory `%s'. Relocate them appropriately.\n"),
			vardir);
  }

  do_exit(0);
}

static void
copy_tape(int argc, char ** argv)
{
  Int32		i, filenum = 0;
  UChar		*sourcehost = NULL, *targethost = NULL;
  UChar		*sourcekeyfile = NULL, *targetkeyfile = NULL;
  UChar		*targetportstr = NULL, *sourceportstr = NULL;
  Int32		sourceport = 0, targetport = 0;
  Int32		sourcecart = 0, targetcart = 0;
  char		**cpptr, *cv[25];

  for(i = 1; i < argc; i++)
    if(!strncmp(argv[i], "-D", 2))
	break;

  if(i < argc){
    goptions(argc - i, (UChar **) argv + i,
		"s:h;s:k;s:P;i:C;i:F;s:c;s:l;b:v;s:T;s:M",
		&targethost, &targetkeyfile, &targetportstr, &targetcart,
		&filenum, &paramfile, &logfile, &verbose, &tmpdir,
		&msgs_config_str);
    argc = i;
    EM__(cpptr = NEWP(char *, i + 1));

    memcpy(cpptr, argv, argc * sizeof(char *));
    argv = cpptr;

    argv[i] = NULL;

    if(targetportstr){
	targetport = get_tcp_portnum(targetportstr);
	if(targetport < 0){
	  errmsg(T_("Error: Cannot resolve TCP service name `%s'.\n"),
			targetportstr);
	  do_exit(1);
	}
    }
  }

  goptions(argc, (UChar **) argv,
		"s:h;s:k;s:P;i:C;i:F;s:c;s:l;b:v;s:T",
		&sourcehost, &sourcekeyfile, &sourceportstr, &sourcecart,
		&filenum, &paramfile, &logfile, &verbose, &tmpdir);

  if(!sourcehost)
    sourcehost = backuphosts[0];
  if(!sourceportstr)
    sourceport = backupports[0];
  if(!sourcekeyfile)
    sourcekeyfile = cryptfile;

  if(sourceportstr){
    sourceport = get_tcp_portnum(sourceportstr);
    if(sourceport < 0){
	errmsg(T_("Error: Cannot resolve TCP service name `%s'.\n"),
			sourceportstr);
	do_exit(1);
    }
  }

  i = 0;
  cv[i++] = clientprogram;
  if(verbose){
    cv[i++] = "-v";
  }
  if(sourcecart){
    cv[i++] = "-C";
    sprintf(tmpbuf, "%d", (int) sourcecart);
    EM__(cv[i++] = strdup(tmpbuf));
  }
  if(filenum){
    cv[i++] = "-F";
    sprintf(tmpbuf, "%d", (int) filenum);
    EM__(cv[i++] = strdup(tmpbuf));
  }
  if(sourcehost){
    cv[i++] = "-h";
    cv[i++] = sourcehost;
  }
  if(sourceport > 0){
    cv[i++] = "-p";
    sprintf(tmpbuf, "%d", (int) sourceport);
    EM__(cv[i++] = strdup(tmpbuf));
  }
  if(sourcekeyfile){
    cv[i++] = "-k";
    cv[i++] = sourcekeyfile;
  }
  if(targetcart || targethost || targetport || targetkeyfile){
    strcpy(tmpbuf, "-D ");
    if(targetcart){
	sprintf(tmpbuf + strlen(tmpbuf), "%d", (int) targetcart);
    }
    if(targethost){
	strcat(tmpbuf, SEPLOC);
	strcat(tmpbuf, targethost);
    }
    if(targetport){
	strcat(tmpbuf, PORTSEP);
	sprintf(tmpbuf + strlen(tmpbuf), "%d", (int) targetport);
    }
    if(targetkeyfile){
	strcat(tmpbuf, ":");
	strcat(tmpbuf, targetkeyfile);
    }

    EM__(cv[i++] = strdup(tmpbuf));
  }
  if(tmpdir){
    cv[i++] = "-T";
    cv[i++] = tmpdir;
  }

  cv[i] = NULL;

  start_msg_proc(sourcehost, - sourceport);

  if(!targetport)
    targetport = sourceport;
  if(!targethost)
    targethost = sourcehost;

  if(sourceport != targetport || strcmp(sourcehost, targethost)){
    ServerIdTab		idtab1, idtab2;	/* try to find out serverids */

    i = !serverid_from_netaddr(&idtab1, sourcehost, sourceport,
			server_ids_file)
		&& !serverid_from_netaddr(&idtab2, targethost, targetport,
			server_ids_file);
    if(i)			/* if server-ids found, compare them */
	i = ! strcmp(idtab1.serverid, idtab2.serverid);	/* 1 if equal */

    if(!i){
	msgprocpid = -1;
	start_msg_proc(targethost, - targetport);
    }
  }

  execv(clientprogram, cv);

  errmsg(T_("Error: Cannot execute `%s'.\n"), clientprogram);
  do_exit(31);
}


