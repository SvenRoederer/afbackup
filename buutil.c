/****************** Start of $RCSfile: buutil.c,v $  ****************
*
* $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/buutil.c,v $
* $Id: buutil.c,v 1.9 2011/12/14 20:27:26 alb Exp alb $
* $Date: 2011/12/14 20:27:26 $
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

  static char * fileversion = "$RCSfile: buutil.c,v $ $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/buutil.c,v $ $Id: buutil.c,v 1.9 2011/12/14 20:27:26 alb Exp alb $ " PACKAGE " " VERSION_STRING;

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#ifdef	HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef	HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifdef	HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif
#include <sys/types.h>
#include <signal.h>
#ifdef	HAVE_SYS_TIME_H
#ifdef	TIME_WITH_SYS_TIME
#include <time.h>
#endif
#endif
#include <x_types.h>
#include <fileutil.h>
#include <sysutils.h>
#include <netutils.h>
#include <genutils.h>
#include <x_regex.h>

#define DEBUGFILE	"/tmp/afserverlog"

#include "afbackup.h"

int
connect_afbu_server(
  UChar		*servername,
  UChar		*servname,
  Int32		portnum,
  Int32		timeout)
{
  int		sockfd;
  time_t	starttime;
  Flag		uxsock = NO;

  if(servname)
    if(FN_ISPATH(servname))
	uxsock = YES;

  starttime = time(NULL);

  forever{
    if(uxsock)
	sockfd = open_uxsock_conn(servname);
    else
	sockfd = open_tcpip_conn(servername, servname, portnum);
    if(sockfd >= 0)
	break;

    if(sockfd > -128)		/* no transient error */
	return(sockfd);
    if(sockfd < 0 && time(NULL) - starttime > timeout)
	return(sockfd);

    ms_sleep(drandom() * 400 + 100);
  }

  set_ip_throughput(sockfd);

  set_socket_keepalive(sockfd);

  return(sockfd);
}

Int32
rewrite_tape_label(
  UChar		*label,
  Int32		new_cartnum,
  UChar		*oldserver,
  Int32		oldport)
{
  UChar		new_buffer[1000];	/* label is <= 512 bytes */
  UChar		*cptr, *cptr2;
  Int32		n;
  int		org_cartno;

  if(!strstr(label, INFOHEADER))
    return(1);

  cptr = strstr(label, CARTNOTEXT);
  if(!cptr)
    return(1);

  if(sscanf(cptr + strlen(CARTNOTEXT), "%d", &org_cartno) < 1)
    return(1);
  strncpy(new_buffer, label, n = (cptr - label) + strlen(CARTNOTEXT));
  sprintf(new_buffer + n, "%d\n", (int) new_cartnum);
  cptr2 = strchr(cptr, '\n');
  if(cptr2)
    strcpy(new_buffer + strlen(new_buffer), cptr2 + 1);
  else
    return(1);

  strcpy(label, new_buffer);

  if( !(cptr = strstr(label, CART2NOTEXT)) ){
    cptr = strstr(label, CARTNOTEXT);
    cptr2 = strchr(cptr, '\n');
    if(cptr2){
	strncpy(new_buffer, label, n = cptr2 - label);
	new_buffer[n] = '\0';
    }
    else{
	strcpy(new_buffer, label);
	strcat(new_buffer, "\n\n");
    }
    sprintf(new_buffer + strlen(new_buffer), "%s%d\n\n%s%s%s%d\n",
				CART2NOTEXT, org_cartno, COPYFROMTEXT,
				oldserver, PORTSEP, (int) oldport);
    if(cptr2)
	strcpy(new_buffer + strlen(new_buffer), cptr2 + 1);
  }
#if 0 /* if there is a secondary label, leave it untouched (recursive copies) */
  else{
    n = (cptr - label) + strlen(CART2NOTEXT);
    strncpy(new_buffer, label, n);
    sprintf(new_buffer + n, "%d\n\n%s%s%s%d\n", org_cartno, COPYFROMTEXT,
					oldserver, PORTSEP, (int) oldport);
    n = strlen(new_buffer);
    if( (cptr2 = strchr(cptr, '\n')) )
	strcpy(new_buffer + n, cptr2 + 1);
  }
#endif

  strcpy(label, new_buffer);

  if( (cptr = strstr(label, CDATETEXT)) ){
    n = (cptr - label) + strlen(CDATETEXT);
    strncpy(new_buffer, label, n);
    sprintf(new_buffer + n, "%s\n", actimestr());
    n = strlen(new_buffer);
    if( (cptr2 = strchr(cptr, '\n')) )
	strcpy(new_buffer + n, cptr2 + 1);
  }
  else if( (cptr = strstr(label, DATETEXT)) ){
    cptr2 = strchr(cptr, '\n');
    if(cptr2){
	strncpy(new_buffer, label, n = cptr2 - label);
	new_buffer[n + 1] = '\0';
    }
    else{
	strcpy(new_buffer, label);
	strcat(new_buffer, "\n");
    }
    sprintf(new_buffer + strlen(new_buffer), "\n%s%s\n",
				CDATETEXT, actimestr());
    if(cptr2)
	strcpy(new_buffer + strlen(new_buffer), cptr2 + 1);
  }
  else{
    cptr = strstr(label, INFOHEADER);
    strncpy(new_buffer, label, n = (cptr - label) + strlen(INFOHEADER));
    sprintf(new_buffer + n, "%s%s\n\n%s%s\n\n",
			DATETEXT, actimestr(), CDATETEXT, actimestr());
    strcat(new_buffer, cptr + strlen(INFOHEADER));
  }

  strcpy(label, new_buffer);

  return(0);
}

Int32
save_bytes_per_tape(
  UChar		*filen,
  Int32		cart,
  Real64	numbytes,
  Int32		numfiles,
  Flag		tape_full,
  time_t	the_time)
{
  UChar		dbuf[32], tbuf[32], vbuf[128], kbuf[32];

  Real64_to_intstr(numbytes, dbuf);
  time_t_to_intstr(the_time, tbuf);

  sprintf(vbuf, "%s %d %d %s", dbuf, (int) numfiles,
				tape_full ? 1 : 0, tbuf);
  sprintf(kbuf, "%d:", (int) cart);

  return(kfile_insert(filen, kbuf, vbuf, KFILE_SORTN | KFILE_LOCKED));
}

Int32
get_bytes_per_tape(
  UChar		*filen,
  Int32		cart,
  Real64	*numbytes,
  Int32		*numfiles,
  Flag		*tape_full,
  time_t	*last_wrtime)
{
  UChar		*cptr, kbuf[32];
  int		i1, i2, n;
  double	d1;
  time_t	t;
  Flag		got_entry = NO;

  sprintf(kbuf, "%d:", (int) cart);
  cptr = kfile_get(filen, kbuf, KFILE_LOCKED);
  if(cptr){
    i1 = i2 = -1;
    if(sscanf(cptr, "%lf%d%d%n", &d1, &i1, &i2, &n) >= 1){
	if(numbytes)
	  *numbytes = (Real64) d1;
	if(i1 != -1 && numfiles)
	  *numfiles = (Int32) i1;
	if(i2 != -1){
	  if(tape_full)
	    *tape_full = (i2 ? YES : NO);
	  if(last_wrtime){
	    *last_wrtime = (time_t) 0;
	    if((t = strint2time(cptr + n)) != UNSPECIFIED_TIME)
		*last_wrtime = t;
	  }
	}

	got_entry = YES;
    }
    free(cptr);
  }

  if(!got_entry){
    if(numbytes)
	*numbytes = 0.0;
    if(numfiles)
	*numfiles = 0;
    if(tape_full)
	*tape_full = NO;
    if(last_wrtime)
	*last_wrtime = (time_t) 0;
  }

  return(0);
}

Int32
get_tape_usages(
  UChar		*filen,
  TapeUsage	**usagesp,
  Int32		*num_usages)
{
  UChar		*cptr;
  int		i1, i2, n;
  double	d1;
  Int32		i, r = 0, num_specs, num_entries = 0;
  TapeUsage	*usages;
  time_t	t;
  KeyValuePair	*allentries = NULL;

  allentries = kfile_getall(filen, &num_entries, KFILE_LOCKED);
  if(!allentries && !eaccess(filen, F_OK) && eaccess(filen, R_OK))
    return(-1);

  usages = NEWP(TapeUsage, num_entries + 1);
  if(! usages)
	GETOUTR(FATAL_ERROR);

  memset(usages, 0, sizeof(usages[0]) * (num_entries + 1));
  *usagesp = usages;
  num_specs = 0;

  if(num_entries > 0){
    for(i = 0; i < num_entries; i++){
      if(sscanf(allentries[i].key, "%d%n", &i1, &n) > 0){
	cptr = first_nospace(allentries[i].key + n);
	if(*cptr != ':')
	  continue;

	usages[num_specs].tape_num = i1;

	i1 = i2 = -1;
	if(sscanf(allentries[i].value, "%lf%d%d%n", &d1, &i1, &i2, &n) > 0){
	  usages[num_specs].bytes_on_tape = (Real64) d1;

	  if(i1 != -1)
	    usages[num_specs].files_on_tape = (Int32) i1;

	  usages[num_specs].tape_full = DONT_KNOW;
	  if(i2 != -1){
	    usages[num_specs].tape_full = (i2 ? YES : NO);

	    usages[num_specs].last_wrtime = (time_t) 0;
	    if((t = strint2time(allentries[i].value + n)) != UNSPECIFIED_TIME)
		usages[num_specs].last_wrtime = t;
	  }
	}

	num_specs++;
      }
    }
  }

  if(num_usages)
    *num_usages = num_specs;

  memset(usages + num_specs, 0, sizeof(usages[0]));

 cleanup:
  kfile_freeall(allentries, num_entries);

  return(r);

 getout:
  CLEANUP;
}

Int32
incr_tape_use_count(UChar * filen, Int32 cart)
{
  UChar		vbuf[32], kbuf[32], *cptr;
  Int32		n = 0;

  sprintf(kbuf, "%d:", (int) cart);
  cptr = kfile_get(filen, kbuf, KFILE_LOCKED);
  if(cptr){
    strint(cptr, &n);
    free(cptr);
  }

  sprintf(vbuf, "%d", (int) n + 1);

  return(kfile_insert(filen, kbuf, vbuf, KFILE_SORTN | KFILE_LOCKED));
}

Int32
host_in_list(UChar * host, UChar * hostlist)
{
  Int32	r = 0;
  UChar	*line = NULL;
  FILE	*fp;

  if(!hostlist)
    return(1);
  if(!hostlist[0])
    return(1);

  if(FN_ISABSPATH(hostlist)){
    fp = fopen(hostlist, "r");
    if(!fp)
	return(-2);

    while(!feof(fp)){
	ZFREE(line);
	line = fget_alloc_str(fp);
	if(!line)
	  continue;

	massage_string(line);

	if(same_host(line, host) > 0){
	  r = 1;
	  break;
	}
    }
    ZFREE(line);
    fclose(fp);
  }
  else if(hostlist[0] == '|'){
    int	pid, pst, inp[2];

    line = repl_substring(hostlist + 1, "%H", host);
    if(!line)
	return(-3);
    if(pipe(inp))
	return(-5);

    pid = fork_forced();

    if(pid == 0){
	dup2(inp[0], 0);
	close(inp[0]);
	close(inp[1]);

	pst = open(NULLFILE, O_WRONLY);
	if(pst >= 0){
	  dup2(pst, 1);
	  dup2(pst, 2);
	  close(pst);
	}

	execl("/bin/sh", "/bin/sh", "-c", line, NULL);
	free(line);
	return(-5);
    }

    close(inp[0]);
    if(pid > 0){
	struct sigaction	siga, osiga;

	SETZERO(siga);
	siga.sa_handler = SIG_IGN;
#ifdef	SA_RESTART
	siga.sa_flags = SA_RESTART;
#endif
	sigaction(SIGPIPE, &siga, &osiga);
	
	write_forced(inp[1], host, strlen(host));
	write_forced(inp[1], "\n", 1);

	sigaction(SIGPIPE, &osiga, NULL);
    }
    close(inp[1]);

    free(line);

    if(pid < 0)
	return(-4);

    waitpid_forced(pid, &pst, 0);

    r = (WEXITSTATUS(pst) ? 0 : 1);
  }
  else{
    forever{
	line = strchr(hostlist, ',');
	if(!line)
	  line = strchr(hostlist, ';');
	if(!line)
	  line = strchr(hostlist, ':');

	if(line)
	  *line = '\0';

	if(same_host(host, hostlist) > 0)
	  r = 1;

	if(line){
	  *line = ',';
	  hostlist = line + 1;
	}
	else
	  break;
    }
  }

  return(r);
}

Int32
peer_in_list(int fd, UChar * hostlist)
{
  UChar	*peern;
  Int32	r;

  peern = get_connected_peername(fd);
  if(!peern)
    return(-1);

  r = host_in_list(peern, hostlist);

  free(peern);

  return(r);
}

Int32
get_entry_by_host(UChar * liststr, UChar * hostname, UChar ** rentry)
{
  UChar		*lstr, *cptr, *wptr, *optr, *gentry;
  Int32		r = -1, i, gidx = -1;

  if(!liststr){
    errno = EINVAL;
    return(-1);
  }

  liststr = repl_substring(liststr, "%H", hostname);
  if(!liststr)
    return(-3);

  lstr = NEWP(UChar, MAX(strlen(hostname), strlen(liststr)) + 1);
  if(!lstr)
    CLEANUPR(-2);

  strcpy(lstr, hostname);
  if( (cptr = strchr(lstr, '.')) )
    *cptr = '\0';
  cptr = repl_substring(liststr, "%h", lstr);
  if(!cptr)
    CLEANUPR(-4);
  free(liststr);
  liststr = cptr;

  lstr = RENEWP(lstr, UChar, strlen(liststr) + 1);
  if(!lstr)		/* liststr might have become LONGER */
    CLEANUPR(-5);

  wptr = liststr;
  i = 0;
  forever{
    optr = wptr;
    wptr = sscanwordq(wptr, lstr);
    if(!wptr)
	break;

    for(cptr = strchr(lstr, ':'); cptr; cptr = strchr(cptr + 1, ':'))
      if(!escaped(lstr, cptr, '\\'))
	break;

    if(cptr){
	if(host_in_list(hostname, cptr + 1) > 0)
	  break;
    }
    else{
	gidx = i;
	gentry = optr;
    }

    i++;
  }

  if(!wptr && gidx >= 0)
    r = gidx;
  if(wptr)
    r = i;

  if(r >= 0){
    if(rentry){
      if(!wptr && gidx >= 0)
	sscanwordq(gentry, lstr);
      else
	*cptr = '\0';

      repl_esc_seq(lstr, '\\');

      *rentry = RENEWP(lstr, UChar, strlen(lstr) + 1);
      lstr = NULL;
    }
  }

 cleanup:
  ZFREE(lstr);
  ZFREE(liststr);

  return(r);
}

Int32
rename_if_exists(UChar * filename, UChar * denot)
{
  UChar		*cptr = NULL, *cptr2 = NULL;
  Int32		r = 0;

  if(!eaccess(filename, F_OK)){
    cptr = strapp(filename, ".");
    if(!cptr)
	CLEANUPR(-2);
    cptr2 = tmp_name(cptr);
    if(!cptr2)
	CLEANUPR(-3);
    if(!denot)
	denot = T_("File");
    fprintf(stdout, T_("Warning: %s exists, renaming to `%s'.\n"),
					denot, cptr2);
    if(rename(filename, cptr2)){
	fprintf(stderr, T_("Error: Renaming %s failed.\n"), filename);
	r = -1;
    }
  }

 cleanup:
  ZFREE(cptr);
  ZFREE(cptr2);

  return(r);
}

#define	BUDEVCONF_NOMEM		-1
#define	BUDEVCONF_NUMFMT	-2
#define	BUDEVCONF_NUMENT	-3
#define	BUDEVCONF_INCONS	-4
#define	BUDEVCONF_DUPDEV	-5
#define	BUDEVCONF_EINVAL	-6
#define	BUDEVCONF_DUPPOS	-7

static UChar	*budevconf_errors[] = {
			TN_("No memory"),
			TN_("Numerical format error in device entry %d"),
			TN_("No slots or no drives configured for changer"),
			TN_("Inconsistent numbers of slots or loadports given in device entry %d"),
			TN_("Duplicate device name in entry %d"),
			TN_("Illegal value in entry %d"),
			TN_("Duplicate entry for position in changer"),
		};

Int32
devices_from_confstr(
  UChar			*string,
  StreamerDevice	**streamersp,
  Int32			*num_streamersp,
  ChangerDevice		**changersp,
  Int32			*num_changersp)
{
  UChar			*cptr, *devptr, *endptr, *chgptr;
  Flag			nextdev, have_pos_in_changer;
  ChangerDevice		*changers = NULL, *cur_changer;
  Int32			num_changers = 0;
  StreamerDevice	*streamers = NULL;
  Int32			num_streamers = 0;
  Int32			cur_pos_in_changer;
  Int32			r = 0, i, nstr_1, nchr_1;
  int			i1, n;

  for(cptr = first_nospace(string); *cptr; cptr = devptr){
    for(devptr = cptr; ; devptr++){
      if(*devptr == '=' || *devptr == '@' || isspace(*devptr) || !(*devptr)){
	if(!(*devptr) || !escaped(cptr, devptr, '\\')){
	  nextdev = NO;

	  if(!(*devptr)){
	    nextdev = YES;
	  }
	  else if(isspace(*devptr)){
	    devptr = first_nospace(devptr);
	    if(*devptr != '=' && *devptr != '@')
		nextdev = YES;
	  }

	  streamers = ZRENEWP(streamers, StreamerDevice, num_streamers + 1);
	  if(!streamers)
	    GETOUTR(BUDEVCONF_NOMEM);
	  SETZERO(streamers[num_streamers]);
	  nstr_1 = num_streamers;
	  num_streamers++;

	  streamers[nstr_1].devicename = NEWP(UChar, (n = devptr - cptr) + 1);
	  if(!streamers[nstr_1].devicename)
	    GETOUTR(BUDEVCONF_NOMEM);

	  strncpy(streamers[nstr_1].devicename, cptr, n);
	  streamers[nstr_1].devicename[n] = '\0';

	  endptr = streamers[nstr_1].devicename + n - 1;
	  while(isspace(*endptr)
		&& !escaped(streamers[nstr_1].devicename, endptr, '\\'))
	    *(endptr--) = '\0';
	  repl_esc_seq(streamers[nstr_1].devicename, '\\');

	  for(i = 0; i < nstr_1; i++)
	    if(!strcmp(streamers[i].devicename, streamers[nstr_1].devicename))
		GETOUTR(BUDEVCONF_DUPDEV);

	  cur_changer = NULL;
	  have_pos_in_changer = NO;

	  if(!nextdev && *devptr == '='){
	    i = sscanf(++devptr, "%d%n", &i1, &n);
	    if(i < 1)
		GETOUTR(BUDEVCONF_NUMFMT);
	    if(i1 < 1)
		GETOUTR(BUDEVCONF_EINVAL);

	    cur_pos_in_changer = i1;
	    have_pos_in_changer = YES;

	    devptr = first_nospace(devptr + n);
	  }

	  if(!nextdev && *devptr == '@' && !escaped(cptr, devptr, '\\')){
	    devptr = first_nospace(devptr + 1);

	    for(chgptr = devptr; ; chgptr++){
	      if(*chgptr == '#' || *chgptr == '^' || isspace(*chgptr) || !(*chgptr)){
		if(!(*chgptr) || !escaped(devptr, chgptr, '\\')){
		  nextdev = NO;

		  if(!(*chgptr)){
		    nextdev = YES;
		  }
		  else if(isspace(*chgptr)){
		    chgptr = first_nospace(chgptr);
		    if(*chgptr != '#' && *chgptr != '^')
			nextdev = YES;
		  }

		  changers = ZRENEWP(changers, ChangerDevice, num_changers + 1);
		  if(!changers)
		    GETOUTR(BUDEVCONF_NOMEM);
		  SETZERO(changers[num_changers]);
		  nchr_1 = num_changers;
		  num_changers++;

		  n = chgptr - devptr;
		  changers[nchr_1].changername = NEWP(UChar, n + 1);
		  if(!changers[nchr_1].changername)
		    GETOUTR(BUDEVCONF_NOMEM);

		  strncpy(changers[nchr_1].changername, devptr, n);
		  changers[nchr_1].changername[n] = '\0';

		  endptr = changers[nchr_1].changername + n - 1;
		  while(isspace(*endptr)
			&& !escaped(changers[nchr_1].changername, endptr, '\\'))
		    *(endptr--) = '\0';
		  repl_esc_seq(changers[nchr_1].changername, '\\');

		  cur_changer = changers + nchr_1;

		  for(i = 0; i < nchr_1; i++){
		    if(!strcmp(changers[i].changername, changers[nchr_1].changername)){
		      free(changers[nchr_1].changername);
		      num_changers--;
		      nchr_1--;

		      cur_changer = changers + i;

		      break;
		    }
		  }

		  if(*chgptr == '#'){
		    i = sscanf(++chgptr, "%d%n", &i1, &n);
		    if(i < 1)
			GETOUTR(BUDEVCONF_NUMFMT);
		    if(i1 < 1)
			GETOUTR(BUDEVCONF_EINVAL);

		    if(cur_changer->num_slots > 0
				&& cur_changer->num_slots != i1)
			GETOUTR(BUDEVCONF_INCONS);

		    cur_changer->num_slots = i1;

		    chgptr = first_nospace(chgptr + n);

		    if(!(*chgptr))
			nextdev = YES;
		  }
		  if(*chgptr == '^'){
		    i = sscanf(++chgptr, "%d%n", &i1, &n);
		    if(i < 1)
			GETOUTR(BUDEVCONF_NUMFMT);
		    if(i1 < 0)
			GETOUTR(BUDEVCONF_EINVAL);

		    if(cur_changer->num_loadports > 0
				&& cur_changer->num_loadports != i1)
			GETOUTR(BUDEVCONF_INCONS);

		    cur_changer->num_loadports = i1;

		    chgptr = first_nospace(chgptr + n);
		  }

		  break;
		}
	      }
	    }
	  }
	  else{
	    chgptr = devptr;
	  }

	  if(cur_changer){
	    streamers[nstr_1].changername_ptr = cur_changer->changername;
	    cur_changer->num_drives++;

	    if(have_pos_in_changer)
		streamers[nstr_1].pos_in_changer = cur_pos_in_changer;
	  }

	  devptr = chgptr;

	  break;
	}
      }
    }
  }

  for(i = 0; i < num_streamers; i++){
    if(!streamers[i].pos_in_changer && streamers[i].changername_ptr){
      cur_pos_in_changer = 1;

      for(n = 0; n < num_streamers; n++){	/* complete pos_in_changer */
	if(streamers[n].pos_in_changer == cur_pos_in_changer){
	  if(streamers[n].changername_ptr){
	    if(!strcmp(streamers[n].changername_ptr,
				streamers[i].changername_ptr)){
		cur_pos_in_changer++;
		n = -1;		/* restart search */
	    }
	  }
	}
      }

      streamers[i].pos_in_changer = cur_pos_in_changer;
    }

    if(streamers[i].pos_in_changer && streamers[i].changername_ptr)
      for(n = 0; n < i; n++)
        if(streamers[i].pos_in_changer == streamers[n].pos_in_changer
		&& !strcmp(streamers[i].changername_ptr, streamers[n].changername_ptr))
	  GETOUTR(BUDEVCONF_DUPPOS);

    for(n = 0; n < num_changers; n++){
      if(changers[n].num_slots < 1 || changers[n].num_drives < 1)
	GETOUTR(BUDEVCONF_NUMENT);

      if(streamers[i].changername_ptr){
	if(!strcmp(streamers[i].changername_ptr, changers[n].changername)){
	  streamers[i].changer = changers + n;
	  break;
	}
      }
    }
  }
  for(i = 0; i < num_streamers; i++)
    streamers[i].pos_in_changer--;	/* start with 0 - index */

  if(changersp)
    *changersp = changers;
  if(num_changersp)
    *num_changersp = num_changers;
  if(streamersp)
    *streamersp = streamers;
  if(num_streamersp)
    *num_streamersp = num_streamers;

 cleanup:
  return(r);

 getout:
  for(i = 0; i < num_changers; i++)
    ZFREE(changers[i].changername);
  for(i = 0; i < num_streamers; i++)
    ZFREE(streamers[i].devicename);
  if(num_streamersp)
    *num_streamersp = num_streamers - 1;
  CLEANUPR(r);
}

UChar *
devices_from_confstr_strerr(Int32 errn)
{
  return(T_(budevconf_errors[-1 - errn]));
}

Int32
devices_from_confstr_msg(
  UChar			*string,
  StreamerDevice	**streamersp,
  Int32			*num_streamersp,
  ChangerDevice		**changersp,
  Int32			*num_changersp,
  FILE			*fp)
{
  Int32		r, nsp;
  UChar		*rawmsg, *msg, numbuf[32];

  r = devices_from_confstr(string, streamersp, &nsp, changersp, num_changersp);

  if(r){
    sprintf(numbuf, "%d", (int) nsp + 1);

    rawmsg = devices_from_confstr_strerr(r);

    msg = repl_substring(rawmsg, "%d", numbuf);

    fprintf(fp, "%s: %s\n", T_("Error"), msg ? msg : rawmsg);

    ZFREE(msg);
  }

  if(num_streamersp)
    *num_streamersp = nsp;

  return(r);
}

void
free_devices_and_changers(
  StreamerDevice	*streamers,
  Int32			num_streamers,
  ChangerDevice		*changers,
  Int32			num_changers)
{
  Int32		i;

  if(streamers){
    for(i = 0; i < num_streamers; i++)
      ZFREE(streamers[i].devicename);
    free(streamers);
  }
  if(changers){
    for(i = 0; i < num_changers; i++)
      ZFREE(changers[i].changername);
    free(streamers);
  }
}

ChangerDevice *
changer_by_device(
  UChar			*devicename,
  StreamerDevice	*deviceslist,
  Int32			num_streamers)
{
  Int32		i;
  UChar		*curdev;

  if(!devicename || !deviceslist){
    errno = EINVAL;
    return(NULL);
  }

  for(i = 0; i < num_streamers; i++){
    curdev = deviceslist[i].devicename;
    if(curdev){
	if(!strcmp(curdev, devicename))
	  return(deviceslist[i].changer);
    }
  }

  return(NULL);
}

UChar *
repl_dev_pats(UChar * str, UChar * streamer, UChar * changer)
{
  UChar		*cptr;

  if(streamer){
    str = repl_substring(str, "%d", streamer);
    if(!str)
	return(NULL);
  }
  if(changer){
    cptr = repl_substring(str, "%D", changer);
    if(streamer)
	free(str);
    str = cptr;
  }

  return(str);
}
