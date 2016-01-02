/****************** Start of $RCSfile: afbackup.h,v $  ****************
*
* $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/afbackup.h,v $
* $Id: afbackup.h,v 1.1 2011/12/14 20:27:22 alb Exp alb $
* $Date: 2011/12/14 20:27:22 $
* $Author: alb $
*
*
******* description ***********************************************
*
*
*
*******************************************************************/

#ifndef	BACKUP_H
#define	BACKUP_H	BACKUP_H

#include <stdio.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#ifdef	HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#ifdef	HAVE_LOCALE_H
#include <locale.h>
#else
#ifdef	HAVE_INTL_LOCALE_H
#include <intl/locale.h>
#endif
#endif
#include <x_types.h>
#include <x_errno.h>
#include <crptauth.h>
#include <budefs.h>
#ifndef	O_SYNC
# define O_SYNC O_FSYNC
#endif
/* This is shit, but HP-UX is one big shit, so it fits */
#if defined(HPUX_9) && !defined(RLIMIT_STACK)
#define	RLIMIT_STACK	3
#endif

struct fault_msgs {
  Int32		code;
  char		*msg;
};

#define FAULT_MESSAGES  { \
	{ CLOSE_FAILED,		"device cannot be closed" }, \
	{ OPENRD_FAILED,	"opening the device for reading failed" },\
	{ OPENWR_FAILED,	"opening the device for writing failed" },\
	{ DEVINUSE,		"device is in use" },\
	{ SERVICEINUSE,		"service is in use" },\
	{ DEVNOTOPENRD,		"device has not been opened for reading" },\
	{ DEVNOTOPENWR,		"device has not been opened for writing"},\
	{ DEVNOTREADY,		"device is not ready for use" },\
	{ ENDOFFILEREACHED,	"end of file encountered" },\
	{ ENDOFTAPEREACHED,	"end of tape encountered" },\
	{ PROTOCOL_ERROR,	"protocol error" },\
	{ CHANGECART_FAILED,	"cartridge cannot be changed" },\
	{ REQNEWCART_FAILED,	"no free or reusable cartridge available" },\
	{ SETFILE_FAILED,	"setting the file failed" },\
	{ WRITE_FAILED,		"writing to storage media failed" },\
	{ ERASETAPE_FAILED,	"erasing the tape failed" },\
	{ FATAL_ERROR,		"a fatal error occured on the server"},\
	{ CONFIG_ERROR,		"server configuration error"},\
	{ REOPEN_FAILED,	"reopening the device for writing failed" },\
	{ NO_VALID_CARTRIDGE,	"illegal cartridge number" },\
	{ NO_VALID_FILENUM,	"illegal file number" },\
	{ NO_VALID_CARTSET,	"illegal cartridge set" },\
	{ AUTHENTICATION,	"authentication failure" },\
	{ SUBPROCESS_FAILED,	"subprocess failed" },\
	{ NO_GREETING_MESSAGE,	"no greeting message from server" },\
	{ CLIENT_NOT_UNIQUE,	"duplicate client identification" },\
	{ TOO_MANY_CLIENTS,	"maximum number of clients exceeded" },\
	{ MESSAGE_UNRECOGNIZED,	"message not recognized" },\
	{ NO_VALID_COMMBUFSIZ,	"illegal communication buffer size" },\
	{ CARTSET_EACCESS,	"access to cartridge set denied for client" },\
	{ EOF,			"connection closed" },\
}

#define	read_tape	read_forced
#define	write_tape	write_forced

typedef struct __tape_usage__ {
  Int32		tape_num;
  Real64	bytes_on_tape;
  Int32		files_on_tape;
  Flag		tape_full;
  time_t	last_wrtime;
} TapeUsage;

typedef struct __changerdev__ {
  UChar		*changername;
  Int32		num_slots;
  Int32		num_loadports;
  Int32		num_drives;
} ChangerDevice;

typedef struct __streamerdev__ {
  UChar		*devicename;
  Int32		blocksize;
  ChangerDevice	*changer;
  Int32		pos_in_changer;		/* starts with 0 */
  UChar		*changername_ptr;	/* private, usage deprecated */
} StreamerDevice;

#ifdef	_WIN32
#define	ntohs	htons
#endif

extern	Int32	rewrite_tape_label(UChar *, Int32, UChar *, Int32);
extern	UChar	*fault_string(Int32);

extern	int	connect_afbu_server(UChar *, UChar *, Int32, Int32);

extern	Int32	save_bytes_per_tape(UChar *, Int32, Real64, Int32, Flag, time_t);
extern	Int32	get_bytes_per_tape(UChar *, Int32, Real64 *, Int32 *, Flag *, time_t *);
extern	Int32	get_tape_usages(UChar *, TapeUsage **, Int32 *);
extern	Int32	incr_tape_use_count(UChar *, Int32);
extern	Int32	host_in_list(UChar *, UChar *);
extern	Int32	peer_in_list(int, UChar *);
extern	Int32	get_entry_by_host(UChar *, UChar *, UChar **);
extern	Int32	rename_if_exists(UChar *, UChar *);

extern	Int32	devices_from_confstr(UChar *, StreamerDevice **, Int32 *,
			ChangerDevice **, Int32 *);
extern	Int32	devices_from_confstr_msg(UChar *, StreamerDevice **, Int32 *,
			ChangerDevice **, Int32 *, FILE *);
extern	UChar	*devices_from_confstr_strerr(Int32);
extern	void	free_devices_and_changers(StreamerDevice *, Int32,
			ChangerDevice *, Int32);
extern	UChar	*repl_dev_pats(UChar *, UChar *, UChar *);
extern	ChangerDevice	*changer_by_device(UChar *, StreamerDevice *, Int32);

#ifdef  DEBUG
#ifdef	DEBUGFILE
#define	DB(fmt, arg1, arg2, arg3)	\
			{	FILE *fp; \
				sigset_t sigs, osigs; \
				sigemptyset(&sigs); \
				sigaddset(&sigs, SIGALRM); \
				sigprocmask(SIG_BLOCK, &sigs, &osigs); \
				fp = fopen(DEBUGFILE, "a"); \
				fprintf(fp, fmt, arg1, arg2, arg3); \
				fflush(fp); \
				fclose(fp); \
				sigprocmask(SIG_SETMASK, &osigs, NULL);}
#else
#define	DB(fmt, arg1, arg2, arg3)	fprintf(stderr, fmt, arg1, arg2, arg3)
#endif	/* ifelse defined(DEBUGFILE) */
#else
#define DB(fmt, arg1, arg2, arg3)   /**/
#endif

#endif	/* !defined(BACKUP_H) */
