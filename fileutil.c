/****************** Start of $RCSfile: fileutil.c,v $  ****************
*
* $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/fileutil.c,v $
* $Id: fileutil.c,v 1.14 2013/11/08 17:58:06 alb Exp alb $
* $Date: 2013/11/08 17:58:06 $
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

  static char * fileversion = "$RCSfile: fileutil.c,v $ $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/fileutil.c,v $ $Id: fileutil.c,v 1.14 2013/11/08 17:58:06 alb Exp alb $ " PACKAGE " " VERSION_STRING;

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <dirent.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef  HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#include <genutils.h>
#include <mvals.h>
#include <fileutil.h>
#include <sysutils.h>
#include <x_regex.h>

/* Function for inserting the output of a typical "save"-function
 * into a possibly existing file into the block marked by two
 * delimiters determined by <blockname> and END_<blockname>.
 *
 * save_block_func is the users save-function, that wants to insert
 * into the file named by <filename>. <block> is the data structure,
 * the user's function wants to save.
 */
Int32	save_insert(
  UChar		*filename,
  UChar		*blockname,
  Int32		(*save_block_func)(FILE *,void *),
  void		*block)
{
  FILE		*fpw;
  UChar		end_mark[100];
  Int32	not_in_file;
  Int32	i, j;
  Int32	own_line, line_ended;
  UChar		*cptr = NULL;
  UChar		**file;
  Int32	num_lines;

  /* if(errno)
    return(errno); *//* no longer critical */

  num_lines = 0;

/* read the existing file into a string array
 */
  file = read_asc_file(filename, &num_lines);

/* if the file doesn't exist, this is no error
 */
  if(file == NULL)
    errno = NO_ERROR;

/* open the (possibly same) file for writing
 */
  fpw = fopen(filename, "w");

  if(fpw == NULL){
    free_asc_file(file, num_lines);
    return(errno = NO_ACCESS_TO_FILE);
  }

  i = -1;
  not_in_file = 1;

  line_ended = 0;

  if(file){
    forever{

     /* search for the beginning block delimiter <blockname>
      */
      i++;
      if(i >= num_lines)
	break;
      cptr = strstr(file[i], blockname);

      if(cptr != NULL){

       /* block marker is found
	*/
	UChar	tmp_buf[BUFSIZ];

       /* handle it, if the marker is not at the beginning of the line
	*/
	if(cptr != file[i]){
	    strncpy(tmp_buf, file[i], (Int32) (cptr - file[i]));
	    fprintf(fpw, "%s", tmp_buf);
	}

	not_in_file = 0;

	break;
      }

      own_line = 0;

     /* if there is some information left in the existing line,
      * put it into a new own line
      */
      for(j = 0; file[i][j] != '\0'; j++)
	if(! isspace(file[i][j]))
	  own_line = 1;

      if(!line_ended && !own_line){
	fprintf(fpw, "\n");
	line_ended = 1;
      }

      if(own_line){
	fprintf(fpw, "%s\n", file[i]);
	line_ended = 0;
      }
    }
  }

/* call the users save-function with his data as parameter,
 * handle user's errors (his function has to return 0, if no
 * error occured)
 */
  if((errno = save_block_func(fpw, block)) != NO_ERROR){
    fclose(fpw);
    free_asc_file(file, num_lines);
    return(errno);
  }

/* append the rest of the previously existing file to the
 * newly generated one
 */
  fprintf(fpw, "\n");

  if(file){
    if(! not_in_file){
      sprintf(end_mark, "END_%s", blockname);

     /* handle the case, that the end-mark is in the same line
      * as the begin-mark
      */
      if(strstr(file[i], end_mark) > (char *) cptr && cptr != NULL){
	cptr = strstr(file[i], end_mark) + strlen(end_mark);
        fprintf(fpw, "%s\n", cptr);
      }

     /* search for the end-mark
      */
      forever{
	i++;
	if(i >= num_lines){
	  /* There is no end-mark -> error
	   */
	    fclose(fpw);
	    free_asc_file(file, num_lines);
	    return(errno = FILE_FORMAT_ERROR);
	}
	if(strstr(file[i], end_mark) != NULL){
	  /* end-mark found. If it's not the last expression in the
	   * line, copy the rest
	   */
	    cptr = strstr(file[i], end_mark) + strlen(end_mark);
	    fprintf(fpw, "%s\n", cptr);
	    break;
	}
      }
    }

   /* copy the rest of the lines into the new file, skip empty lines
    */
    j = 0;
    for(i++; i < num_lines; i++){
	if(! empty_string(file[i])){
	    fprintf(fpw, "%s\n", file[i]);
	    j = 0;
	}
	else{
	    if(! j)
		fprintf(fpw, "\n");
	    j = 1;
	}
    }
  }		/* end of if(file) */

  fclose(fpw);

  return(errno);
}

/**** Function to read parameters out of a file ****/

#define	re_start_buffer	(re_buffers + 0)
#define	re_end_buffer	(re_buffers + 1)

static	RE_cmp_buffer	*re_buffers = NULL;
static	Int32		num_re_buffers = 0;

static	Int32		allocate_re_buffers(Int32);
static	UChar		*read_a_line(FILE *);
static	Int32		get_entry(ParamFileEntry *, UChar *);

Int32
read_param_file(
  UChar			*filename,
  ParamFileEntry	*entries,
  Int32			num_entries,
  UChar			*start_pat,
  UChar			*end_pat)
{
  FILE		*fp = NULL;
  Int32		errc, i, j, ret = NO_ERROR;
  UChar		*line;
  UChar		*flags;

  line = NULL;

  if(num_entries < 1)
    return(num_entries < 0 ? (errno = ILLEGAL_VALUE) : NO_ERROR);

  fp = fopen(filename, "r");

  if(!fp)
    return(errno = NO_SUCH_FILE);

  if( (errc = allocate_re_buffers(num_entries + 2)) )
    return(errc);

  flags = NEWP(UChar, num_entries);
  if(!flags)
    return(errno);

  memset(flags, 0, num_entries * sizeof(UChar));

  for(i = 0; i < num_entries; i++){
    if(re_compile_pattern(entries[i].pattern, strlen(entries[i].pattern),
		re_buffers + i + 2))
	GETOUT;

    if(entries[i].num_entries)
	*(entries[i].num_entries) = 0;
  }

  if(start_pat)
    if(re_compile_pattern(start_pat, strlen(start_pat), re_start_buffer))
	GETOUT;

  if(end_pat)
    if(re_compile_pattern(end_pat, strlen(end_pat), re_end_buffer))
	GETOUT;

  if(start_pat)
    forever{
	if(line){
	  free(line);
	  line = NULL;
	}

	if(!(line = read_a_line(fp)))
	  CLEANUP;

	if(re_find_match(re_start_buffer, line, NULL, NULL) >= 0)
	  break;
    }

  if(line)
    free(line);

  while( (line = read_a_line(fp)) ){
    if(end_pat)
      if(re_find_match(re_end_buffer, line, NULL, NULL) >= 0)
	CLEANUP;

    for(i = 0; i < num_entries; i++){
      if(flags[i])
	continue;

      if(re_find_match(re_buffers + i + 2, line, NULL, &j) >= 0){
	get_entry(entries + i, line + j);
	flags[i] = 1;
      }
    }

    free(line);
  }

 cleanup:
  if(line)
    free(line);
  if(flags)
    free(flags);
  if(fp)
    fclose(fp);

  return(ret);

 getout:
  ret = errno;
  CLEANUP;
}

static Int32
get_entry(
  ParamFileEntry	*entry,
  UChar			*line)
{
  UChar		*cptr;
  Uns32		num_values;
  void		*values = NULL;
  double	indouble;
  float		infloat;
  long int	inlongint;
  unsigned long	inlonguns;
  short int	inshortint;
  unsigned short inshortuns;
  unsigned char	inchar;
  int		j;

  num_values = 0;

  cptr = line;

  if(!entry->entry_ptr)
    return(NO_ERROR);

  if(entry->num_entries)
    *entry->num_entries = 0;

  switch(entry->type){
    case TypeReal64:
	for(; *cptr;){
	  if(sscanf(cptr, "%lf", &indouble) < 1)
	    cptr++;

	  else{
	    if(entry->num_entries)
		*entry->num_entries = 1;

	    *((Real64 *) entry->entry_ptr) = (Real64) indouble;

	    break;
	  }
	}
	break;
    case TypeReal32:
	for(; *cptr;){
	  if(sscanf(cptr, "%f", &infloat) < 1)
	    cptr++;

	  else{
	    if(entry->num_entries)
		*entry->num_entries = 1;

	    *((Real32 *) entry->entry_ptr) = (Real32) infloat;

	    break;
	  }
	}
	break;
    case TypeInt32:
	for(; *cptr;){
	  if(sscanf(cptr, "%ld", &inlongint) < 1)
	    cptr++;

	  else{
	    if(entry->num_entries)
		*entry->num_entries = 1;

	    *((Int32 *) entry->entry_ptr) = (Int32) inlongint;

	    break;
	  }
	}
	break;
    case TypeUns32:
	for(; *cptr;){
	  if(sscanf(cptr, "%lu", &inlonguns) < 1)
	    cptr++;

	  else{
	    if(entry->num_entries)
		*entry->num_entries = 1;

	    *((Uns32 *) entry->entry_ptr) = (Uns32) inlonguns;

	    break;
	  }
	}
	break;
    case TypeInt16:
	for(; *cptr;){
	  if(sscanf(cptr, "%hd", &inshortint) < 1)
	    cptr++;

	  else{
	    if(entry->num_entries)
		*entry->num_entries = 1;

	    *((Int16 *) entry->entry_ptr) = (Int16) inshortint;

	    break;
	  }
	}
	break;
    case TypeUns16:
	for(; *cptr;){
	  if(sscanf(cptr, "%hu", &inshortuns) < 1)
	    cptr++;

	  else{
	    if(entry->num_entries)
		*entry->num_entries = 1;

	    *((Uns16 *) entry->entry_ptr) = (Uns16) inshortuns;

	    break;
	  }
	}
	break;
    case TypeUChar:
    case TypeSChar:
	for(; *cptr;){
	  if(sscanf(cptr, "%c", &inchar) < 1)
	    cptr++;

	  else{
	    if(entry->num_entries)
		*entry->num_entries = 1;

	    *((UChar *) entry->entry_ptr) = (UChar) inchar;

	    break;
	  }
	}
	break;
    case TypeFlag:
	*((Flag *) entry->entry_ptr) = NO;

	if(entry->num_entries)
	  *entry->num_entries = 1;

	cptr = first_nospace(cptr);
	j = first_space(cptr) - cptr;
	*((Flag *) entry->entry_ptr) = is_yes(cptr, j);
	break;
    case TypeReal64PTR:
	values = seg_malloc(sizeof(Real64));

	if(!values)
	  GETOUT;

	for(; *cptr;){
	  if(sscanf(cptr, "%lf", &indouble) < 1){
	    cptr++;
	    continue;
	  }

	  values = seg_realloc(values, (num_values + 1) * sizeof(Real64),
					num_values * sizeof(Real64));

	  if(!values)
	    GETOUT;

	  *((Real64 *) values + num_values) = indouble;

	  num_values++;

	  sscanf(cptr, "%*f%n", &j);

	  cptr += j;
	}

	if(entry->num_entries)
	  *entry->num_entries = num_values;

	*((Real64 **) entry->entry_ptr) = values;

	break;

    case TypeReal32PTR:
	values = seg_malloc(sizeof(Real32));

	if(!values)
	  GETOUT;

	for(; *cptr;){
	  if(sscanf(cptr, "%f", &infloat) < 1){
	    cptr++;
	    continue;
	  }

	  values = seg_realloc(values, (num_values + 1) * sizeof(Real32),
					num_values * sizeof(Real32));

	  if(!values)
	    GETOUT;

	  *((Real32 *) values + num_values) = infloat;

	  num_values++;

	  sscanf(cptr, "%*f%n", &j);

	  cptr += j;
	}

	if(entry->num_entries)
	  *entry->num_entries = num_values;

	*((Real32 **) entry->entry_ptr) = values;

	break;

    case TypeInt32PTR:
	values = seg_malloc(sizeof(Int32));

	if(!values)
	  GETOUT;

	for(; *cptr;){
	  if(sscanf(cptr, "%ld", &inlongint) < 1){
	    cptr++;
	    continue;
	  }

	  values = seg_realloc(values, (num_values + 1) * sizeof(Int32),
					num_values * sizeof(Int32));

	  if(!values)
	    GETOUT;

	  *((Int32 *) values + num_values) = inlongint;

	  num_values++;

	  sscanf(cptr, "%*d%n", &j);

	  cptr += j;
	}

	if(entry->num_entries)
	  *entry->num_entries = num_values;

	*((Int32 **) entry->entry_ptr) = values;

	break;

    case TypeUns32PTR:
	values = seg_malloc(sizeof(Uns32));

	if(!values)
	  GETOUT;

	for(; *cptr;){
	  if(sscanf(cptr, "%lu", &inlonguns) < 1){
	    cptr++;
	    continue;
	  }

	  values = seg_realloc(values, (num_values + 1) * sizeof(Uns32),
					num_values * sizeof(Uns32));

	  if(!values)
	    GETOUT;

	  *((Uns32 *) values + num_values) = inlonguns;

	  num_values++;

	  sscanf(cptr, "%*u%n", &j);

	  cptr += j;
	}

	if(entry->num_entries)
	  *entry->num_entries = num_values;

	*((Uns32 **) entry->entry_ptr) = values;

	break;

    case TypeInt16PTR:
	values = seg_malloc(sizeof(Int16));

	if(!values)
	  GETOUT;

	for(; *cptr;){
	  if(sscanf(cptr, "%hd", &inshortint) < 1){
	    cptr++;
	    continue;
	  }

	  values = seg_realloc(values, (num_values + 1) * sizeof(Int16),
					num_values * sizeof(Int16));

	  if(!values)
	    GETOUT;

	  *((Int16 *) values + num_values) = inshortint;

	  num_values++;

	  sscanf(cptr, "%*d%n", &j);

	  cptr += j;
	}

	if(entry->num_entries)
	  *entry->num_entries = num_values;

	*((Int16 **) entry->entry_ptr) = values;

	break;

    case TypeUns16PTR:
	values = seg_malloc(sizeof(Uns16));

	if(!values)
	  GETOUT;

	for(; *cptr;){
	  if(sscanf(cptr, "%hu", &inshortuns) < 1){
	    cptr++;
	    continue;
	  }

	  values = seg_realloc(values, (num_values + 1) * sizeof(Uns16),
					num_values * sizeof(Uns16));

	  if(!values)
	    GETOUT;

	  *((Uns16 *) values + num_values) = inshortuns;

	  num_values++;

	  sscanf(cptr, "%*u%n", &j);

	  cptr += j;
	}

	if(entry->num_entries)
	  *entry->num_entries = num_values;

	*((Uns16 **) entry->entry_ptr) = values;

	break;

    case TypeUCharPTR:
    case TypeSCharPTR:
	*((UChar **) entry->entry_ptr) = strdup(cptr);

	if(! (*((UChar **) entry->entry_ptr)))
	  GETOUT;

	if(entry->num_entries)
	  *entry->num_entries = strlen(cptr);

	break;
  }

  return(NO_ERROR);

 getout:
  if(values)
    free(values);

  return(errno);
}

static UChar *
read_a_line(FILE * fp)
{
  UChar		*line = NULL;
  UChar		*next_line = NULL;
  UChar		*cptr;
  Int32		len, oldlen;

  line = fget_alloc_str(fp);

  if(!line)
    return(NULL);

  oldlen = strlen(line) + 1;

  cptr = first_nospace(line);
  if(*cptr == '#')
    *cptr = '\0';

  len = strlen(line);

  if(line[len - 1] == '\n'){
    len--;
    line[len] = '\0';
  }

  while(line[len - 1] == '\\'){
    line[len - 1] = '\0';

    next_line = fget_alloc_str(fp);	/* returns seg_alloc aligned ptr */

    if(!next_line)
	return(line);

    line = seg_realloc(line, sizeof(UChar) * (len + 1 + strlen(next_line)),
			oldlen);
    oldlen = sizeof(UChar) * (len + 1 + strlen(next_line));

    if(!line)
	GETOUT;

    strcpy(line + len - 1, next_line);

    free(next_line);

    cptr = strchr(line, '#');
    if(cptr)
	*cptr = '\0';

    len = strlen(line);
    if(line[len - 1] == '\n'){
	len--;
	line[len] = '\0';
    }
  }

  return(line);

 getout:

  if(line)
    free(line);

  if(next_line)
    free(next_line);

  return(NULL);
}

static Int32
allocate_re_buffers(Int32 new_num_buffers)
{
  if(new_num_buffers <= num_re_buffers)
    return(0);

  re_buffers = ZRENEWP(re_buffers, RE_cmp_buffer, new_num_buffers);
  if(!re_buffers){
    num_re_buffers = 0;
    return(errno);
  }

  memset(re_buffers + num_re_buffers, 0,
	sizeof(RE_cmp_buffer) * (new_num_buffers - num_re_buffers));

  num_re_buffers = new_num_buffers;

  return(NO_ERROR);
}

/*
 * find_program
 *
 * Given a program name, returns the pathname to that program.
 *
 * If argv0 contains a directory separator character ('/', in UNIX parlance),
 * returns a malloc'd copy of argv0.  In this case, no check for existence
 * is done.
 *
 * If argv0 does not contain a directory separator character, then the
 * directories specified by the "PATH" environment variable are searched.
 * If argv0 is found there and is executable, then the malloc'd pathname
 * is returned.
 *
 * Returns NULL if the program is not found or if an error occurs (such as
 * a malloc error).
 *
 * Note that in the pathological case of passing in the empty string, a
 * string containing the root directory will be returned.
 */

UChar *
find_program(UChar * argv0)
{
  UChar		*largv0 = NULL, *lpath = NULL, *ret = NULL, *cptr, *next;

  if(FN_ISPATH(argv0))
    return((UChar *) strdup(argv0));

  largv0 = (UChar *) strdup(argv0);
  if(!largv0)
    CLEANUP;
  lpath = getenv("PATH");
  if(!lpath)
    CLEANUP;
  lpath = (UChar *) strdup(lpath);
  if(!lpath)
    CLEANUP;
  ret = NEWP(UChar, strlen(argv0) + strlen(lpath) + 2);
  if(!ret)
    CLEANUP;

  cptr = lpath;
  forever{
    next = (UChar *) strchr(cptr, ENV_PATHSEPCHR);
    if(next)
      *next = '\0';
    strcpy(ret, cptr);
    strcat(ret, FN_DIRSEPSTR);
    strcat(ret, argv0);
    if(!eaccess(ret, X_OK))
      CLEANUP;

    if(!next)
	break;

    cptr = next + 1;
  }

  free(ret);
  ret = NULL;

 cleanup:
  if(largv0)
    free(largv0);
  if(lpath)
    free(lpath);

  return(ret);
}

/*
 * Checks the list {cmd1, ...} to ensure that all arguments are executable
 * commands. The command could be either a fully qualified pathname, or
 * just a basename if the command exists in the PATH environment variable.
 *
 * If all commands are executable and are regular files (no symlinks!),
 * then this function returns NULL.  If any of the commands are not
 * executable (or not regular files), then a pointer to a malloc'd copy
 * of the offending command is returned.
 *
 * Note that under some rare circumstances, the returned string may be from
 * the data segment rather than from the heap, so it's best not to free it.
 * This can be determined from the first character. If this is '<', then
 * malloc did not succeed and the string must not be freed
 */

UChar *
check_commands_executable(Flag empty_is_ok, UChar * cmd1, ...)
{
  UChar		*cmd, *argv0, *prog = NULL, *r = NULL;
  struct stat	statb;
  va_list	args;

  va_start(args, cmd1);

  for(cmd = cmd1; cmd; cmd = va_arg(args, UChar *)){
    if(empty_is_ok && word_count(cmd) < 1)
	continue;

    argv0 = strwordq(cmd, 0);
    if(!argv0){
	r = strdup(cmd);
	if(!r)
	  r = "<Error: Cannot allocate memory for filename>";
	else if(r[0] == '<')
	  r[0] = '[';
	CLEANUP;
    }

    prog = find_program(argv0);
    if(!prog)
	CLEANUPR(argv0);
    if(eaccess(prog, X_OK))
	CLEANUPR(argv0);
    if(stat(prog, &statb) < 0)
	CLEANUPR(argv0);
    if(!S_ISREG(statb.st_mode))
	CLEANUPR(argv0);

    free(argv0);
    ZFREE(prog);
  }

 cleanup:
  va_end(args);
  ZFREE(prog);

  return(r);
}

/*
 * eliminate superfluous character sequences from the given
 * path, e.g. a trailing /. , a leading ./ , /./ or other stuff
 *
 * the result might be called somewhat 'normalized' or 'unified'
 */

Int32
cleanpath(UChar * path)
{
  UChar		*cptr1, *cptr2, modified;

  if(!path){
    errno = EINVAL;
    return(errno);
  }
  if(!(*path))
    return(0);

  do{
    modified = 0;
    if(FN_TRAILINGDUMMY(path)){
	*(FN_LASTDIRDELIM(path) + 1) = '\0';
	modified = 1;
    }

    cptr1 = path + strlen(path) - 1;
    while(FN_ISDIRSEP(*cptr1) && ! FN_ISROOTDIR(path)){
	*(cptr1--) = '\0';
	modified = 1;
    }
  } while(modified);

  while( (cptr1 = FN_STRDBLDIRSEP(path)) ){
    cptr1++;
    cptr2 = cptr1 + 1;
    while(FN_ISDIRSEP(*cptr2))
	cptr2++;
    while(*cptr2)
      *(cptr1++) = *(cptr2++);

    *cptr1 = '\0';
  }

  while( (cptr1 = FN_STREMPTYDIRSEP(path)) ){
    cptr1++;
    cptr2 = cptr1 + 2;
    while(*cptr2)
      *(cptr1++) = *(cptr2++);

    *cptr1 = '\0';
  }

  if(FN_LEADINGDUMMY(path)){
    cptr1 = path;
    cptr2 = path + strlen(FN_CURDIR FN_DIRSEPSTR);
    while(*cptr2)
      *(cptr1++) = *(cptr2++);

    *cptr1 = '\0';
  }

  return(0);
}

/*
 * like cleanpath, but also eliminates sequences like
 * /subdir/../
 */

Int32
cleanpath__(UChar * path)
{
  UChar		*cptr1, *cptr2, *cptr3, *orgpath;
  UChar		updir[10], dirup[10], rootdummy[10];
  Int32		updirlen, diruplen, rootdummylen;

  cleanpath(path);

  strcpy(updir, FN_PARENTDIR FN_DIRSEPSTR);
  updirlen = strlen(updir);
  strcpy(dirup, FN_DIRSEPSTR FN_PARENTDIR);
  diruplen = strlen(dirup);
  strcpy(rootdummy, FN_DIRSEPSTR FN_PARENTDIR);
  rootdummylen = strlen(rootdummy);

  orgpath = path;
  forever{
    while(!strncmp(path, updir, updirlen))
      path += updirlen;

    while(!strncmp(path, rootdummy, rootdummylen)
		&& (!path[rootdummylen] || FN_ISDIRSEP(path[rootdummylen]))){
      cptr1 = path;
      cptr2 = cptr1 + rootdummylen;
      if(FN_ISDIRSEP(*cptr2)){
	cptr1++;
	cptr2++;
      }

      while(*cptr2)
	*(cptr1++) = *(cptr2++);
      *cptr1 = '\0';

      if(!*path){
	path[0] = FN_DIRSEPCHR;
	path[1] = '\0';
	GETOUT;
      }
    }

    if(FN_ISDIRSEP(*path))
	path++;

    cptr1 = FN_FIRSTDIRSEP(path);
    if(!cptr1)
	GETOUT;

    cptr2 = cptr1 + 1 - updirlen;
    cptr3 = cptr1 + diruplen;
    if(!strncmp(cptr1, dirup, diruplen)
			&& (!(*cptr3) || FN_ISDIRSEP(*cptr3))
			&& strncmp(cptr2, updir, updirlen)){
	cptr2 = cptr1 + diruplen;
	if(FN_ISDIRSEP(*cptr2))
	  cptr2++;

	cptr1 = path;
	while(*cptr2)
	  *(cptr1++) = *(cptr2++);
	*cptr1 = '\0';

	path = orgpath;
    }
    else{
	path = cptr1 + 1;
    }
  }

 getout:
  cleanpath(orgpath);
  return(0);
}

UChar *
get_cwd(UChar * buf, Int32 size)
{
  UChar	*wd = NULL, *cptr;
  Int32	n;
  Flag	gotit = NO;

  if(buf && size > 0){
    if(getcwd(buf, size))
	return(buf);

    if(errno != ERANGE)
	return(NULL);
  }

  for(n = 512; n < 262144; n += 256){
    cptr = ZRENEWP(wd, UChar, n);
    if(!cptr)
	break;

    wd = cptr;

    if(getcwd(wd, n)){
	gotit = YES;
	break;
    }

    if(errno != ERANGE)
	break;
  }

  if(!gotit)
    ZFREE(wd);

  return(wd);
}

/*
 * makes a complete absolute path from the given path (if relative),
 * and the current working directory. If the given path is already
 * an absolute one, it is left unchanged.
 *
 * If retpath is non-NULL, the result is put there, otherwise memory
 * is malloc'ed for the result and the pointer returned.
 *
 * On error, NULL is returned
 */

UChar *
mkabspath(UChar * path, UChar * retpath)
{
  UChar	*curpath = NULL, lbuf[256];

  if(!path)
    return(NULL);

  if(FN_ISABSPATH(path)){
    if(retpath){
	strcpy(retpath, path);
	return(retpath);
    }
    else{
	return(strdup(path));
    }
  }

  while(FN_LEADINGDUMMY(path))
    path += strlen(FN_CURDIR FN_DIRSEPSTR);

  curpath = get_cwd(lbuf, 256);

  if(retpath){
    sprintf(retpath, "%s" FN_DIRSEPSTR "%s", curpath, path);
  }
  else{
    retpath = strchain(curpath, FN_DIRSEPSTR, path, NULL);
  }

  if(curpath != lbuf)
    free(curpath);

  return(retpath);
}

Int32
mkrelpath(UChar * path)
{
  UChar		*cptr;

  if(!path)
    return(0);

  cleanpath(path);

  if(FN_ISABSPATH(path)){
    for(cptr = FN_FIRSTDIRSEP(path); FN_ISDIRSEP(*cptr); cptr++);

    memmove(path, cptr, (strlen(cptr) + 1) * sizeof(UChar));
  }

  return(0);
}

/* Function to resolve symbolic links from path, producing a new path
 * not containing any symbolic link any more. New path is stored in
 * retpath, if not NULL, otherwise malloced. Pointer to it returned.
 */

UChar *
resolve_path(UChar * path, UChar * retpath)
{
  UChar		*newpath, *cptr, *oldpath, *dirdelim;
  UChar		linkpath[MAXPATHLEN + 1];
  UChar		c;		/* uninitialized OK */
  Int32		i;
  struct stat	statb;

  path = strdup(path);
  if(!path)
    return(NULL);

 restart:
  cleanpath(path);
  oldpath = path;
  if(FN_ISABSPATH(oldpath))
    oldpath = FN_FIRSTDIRSEP(oldpath) + 1;

  forever{
    while(FN_ISDIRSEP(*oldpath))
	oldpath++;

    if(!oldpath[0])
	break;

    dirdelim = FN_FIRSTDIRSEP(oldpath);
    if(dirdelim){
	c = *dirdelim;
	*dirdelim = '\0';
    }

    if(lstat(path, &statb))
	CLEANUP;

    if(S_ISLNK(statb.st_mode)){
	i = readlink(path, linkpath, MAXPATHLEN);
	if(i < 0)
	  CLEANUP;
	linkpath[i] = '\0';
	cptr = dirdelim ? dirdelim + 1 : (UChar *) "";

	if(FN_ISABSPATH(linkpath)){
	  newpath = strchain(linkpath, FN_DIRSEPSTR, cptr, NULL);
	}
	else{
	  *oldpath = '\0';
	  newpath = strchain(path, path[0] ? FN_DIRSEPSTR : "",
				linkpath, FN_DIRSEPSTR, cptr, NULL);
	}
	if(!newpath)
	  CLEANUP;

	free(path);
	path = newpath;
	newpath = NULL;

	goto restart;
    }

    if(!dirdelim)
	break;

    *dirdelim = c;
    oldpath = dirdelim + 1;
  }

  if(retpath){
    strcpy(retpath, path);
    free(path);
    return(retpath);
  }
  else
    return(path);

 cleanup:
  ZFREE(path);
  return(NULL);
}

/* Same as resolve_path, but furthermore removing ../ parts from
 * path, thus giving the true position of the given file in the
 * filesystem
 */

UChar *
resolve_path__(UChar * path, UChar * retpath)
{
  UChar		*newpath;

  newpath = resolve_path(path, retpath);
  if(!newpath)
    return(NULL);

  cleanpath__(newpath);

  return(newpath);
}

typedef struct __find_file_node__ {
  dev_t		dev;
  ino_t		ino;
} FindFileNode;

static Int32
real_find1(
  UChar		*filename,
  FindParams	*params,
  Int32		(*func)(UChar *, void *, struct stat *),
  void		*funcarg,
  dev_t		device,
  Flag		have_device,
  UChar		**excl_dirs_names,
  FindFileNode	*excl_dirs_nodes,
  UChar		**excl_files_names,
  FindFileNode	*excl_files_nodes)
{
  FindParams	lparams;
  DIR		*dir = NULL;
  Int32		ret, i;
  UChar		**files, **excl_files, **excl_dirs;
  UChar		**local_excl_files = NULL;
  Uns32		num_local_excl_files = 0;
  UChar		*excl_filename;
  Uns32		options;
  UChar		*cptr, **names;
  UChar		*buf = NULL;
  struct stat	statb;
  struct dirent	*entry;
  Flag		processit;
  UChar		*bname;
  UChar		*fstype;

  if(!filename || !func)
    return(EINVAL);

  ret = 0;

  filename = strdup(filename);
  if(!filename)
    return(errno);

  cleanpath(filename);

  if(!params){
    memset(&lparams, 0, sizeof(lparams));
    params = &lparams;
  }
  excl_dirs = excl_dirs_names;
  excl_files = excl_files_names;
  files = params->names;
  options = params->options;

  if(options & FIND_FOLLOW_SLINKS){
    if(stat(filename, &statb)){
      if(params->errfunc){
	ret = (*params->errfunc)(filename, params->errfunc_param);
      }
      CLEANUP;
    }
  }
  else{
    if(lstat(filename, &statb)){
      if(params->errfunc){
	ret = (*params->errfunc)(filename, params->errfunc_param);
      }
      CLEANUP;
    }
  }

  if(!have_device && (options & FIND_LOCAL_DEV)){
    have_device = YES;
    device = statb.st_dev;
  }

  if(S_ISLNK(statb.st_mode) && (options & FIND_NO_SLINKS)){
      ret = 0;
      CLEANUP;
  }
  if(S_ISFIFO(statb.st_mode) && (options & FIND_NO_FIFOS)){
      ret = 0;
      CLEANUP;
  }
  if(S_ISREG(statb.st_mode) && (options & FIND_NO_FILES)){
      ret = 0;
      CLEANUP;
  }
  if(S_ISBLK(statb.st_mode) && (options & FIND_NO_BDEVS)){
      ret = 0;
      CLEANUP;
  }
  if(S_ISCHR(statb.st_mode) && (options & FIND_NO_CDEVS)){
      ret = 0;
      CLEANUP;
  }
  if(S_ISSOCK(statb.st_mode) && (options & FIND_NO_SOCKS)){
      ret = 0;
      CLEANUP;
  }
  if((options & FIND_LOCAL_DEV) && have_device && device != statb.st_dev){
      ret = 0;
      CLEANUP;
  }

  processit = YES;

  if(params->newer_than){
    processit = NO;

    if(statb.st_mtime >= params->newer_than)
	processit = YES;

    if((options & FIND_CMP_CTIME) && statb.st_ctime >= params->newer_than)
	processit = YES;
  }

  if(params->older_than){
    processit = NO;

    if(statb.st_mtime < params->older_than)
	processit = YES;

    if((options & FIND_CMP_CTIME) && statb.st_ctime < params->older_than)
	processit = YES;
  }

  bname = FN_BASENAME(filename);

  if(files && processit){
    processit = NO;

    for(names = files; *names; names++){
      if(FN_ISPATH(*names)){
	if(!fn_match(*names, filename, 0))
	  break;
      }
      else{
	if(!fn_match(*names, bname, GFNM_PATHNAME))
          break;
      }
    }

    if(*names)
      processit = YES;
  }

  if(excl_files && processit){
    for(i = 0, names = excl_files; *names; i++, names++){
      if((*names)[0]){			/* use filename to match */
	if(FN_ISPATH(*names)){
	  if(!fn_match(*names, filename, 0))
	    break;
	}
	else{
	  if(!fn_match(*names, bname, GFNM_PATHNAME))
	    break;
	}
      }
      else{				/* use inode and device info */
	if(excl_files_nodes[i].dev == statb.st_dev
				&& excl_files_nodes[i].ino == statb.st_ino)
	  break;
      }
    }

    if(*names)
      processit = NO;
  }

  if(params->fstypes_to_search || params->fstypes_to_skip || params->fstypes_to_prune)
    fstype = get_fstype_by_devno_dir(statb.st_dev, filename);
  else
    fstype = NULL;

  if(fstype && params->fstypes_to_search && processit){
    processit = NO;
    for(names = params->fstypes_to_search; *names; names++){
      if(!strcmp(*names, fstype)){
	processit = YES;
	break;
      }
    }
  }

  if(fstype && params->fstypes_to_skip && processit){
    for(names = params->fstypes_to_skip; *names; names++){
      if(!strcmp(*names, fstype)){
	processit = NO;
	break;
      }
    }
  }

  if(processit && !(S_ISDIR(statb.st_mode) && (params->options & FIND_DEPTH))){
    if( (ret = (*func)(filename, funcarg, &statb)) )
	CLEANUP;
  }

  if(! S_ISDIR(statb.st_mode)){
    ret = 0;
    CLEANUP;
  }

  if(excl_dirs){
    for(i = 0, names = excl_dirs; *names; i++, names++){
      if((*names)[0]){			/* use filename to match */
	if(FN_ISPATH(*names)){
	  if(!fn_match(*names, filename, 0))
	    break;
	}
	else{
	  if(!fn_match(*names, bname, GFNM_PATHNAME))
	    break;
	}
      }
      else{				/* use inode and device info */
	if(excl_dirs_nodes[i].dev == statb.st_dev
				&& excl_dirs_nodes[i].ino == statb.st_ino)
	  break;
      }
    }

    if(*names){
	ret = 0;
	CLEANUP;
    }
  }

  if(fstype && params->fstypes_to_prune){
    for(names = params->fstypes_to_prune; *names; names++){
      if(!strcmp(*names, fstype)){
	ret = 0;
	CLEANUP;
      }
    }
  }

  dir = opendir(filename);
  if(!dir){
    if(params->errfunc){
	ret = (*params->errfunc)(filename, params->errfunc_param);
    }
    CLEANUP;
  }

  buf = NEWP(UChar, 1);
  if(!buf){
    ret = ENOMEM;
    CLEANUP;
  }

  if(params->excl_filename){
    excl_filename = strchain(filename, FN_DIRSEPSTR,
					 params->excl_filename, NULL);
    if(!excl_filename){
	ret = ENOMEM;
	CLEANUP;
    }

    if(!eaccess(excl_filename, R_OK)){
	FILE	*fp;

	fp = fopen(excl_filename, "r");

	while(!feof(fp)){
	  cptr = fget_alloc_str(fp);
	  if(!cptr)
	    break;

	  massage_string(cptr);

	  if(num_local_excl_files)
	    local_excl_files = RENEWP(local_excl_files, UChar *, num_local_excl_files + 1);
	  else
	    local_excl_files = NEWP(UChar *, 1);

	  local_excl_files[num_local_excl_files++] = cptr;
	}

	fclose(fp);
    }

    free(excl_filename);
  }
    
  forever{
    entry = readdir(dir);
    if(! entry)
      break;

    cptr = (UChar *) &(entry->d_name[0]);

    if(!strcmp(cptr, FN_CURDIR) || !strcmp(cptr, FN_PARENTDIR))
      continue;

    if(local_excl_files){
      for(i = 0; i < num_local_excl_files; i++){
	if(!fn_match(local_excl_files[i], cptr, 0))
	  break;
      }

      if(i < num_local_excl_files)
	continue;
    }

    buf = RENEWP(buf, UChar, strlen(filename) + strlen(cptr) + 2);
    if(!buf){
      ret = ENOMEM;
      CLEANUP;
    }

    sprintf(buf, "%s%c%s", filename, FN_DIRSEPCHR, cptr);

    if( (ret = real_find1(buf, params, func, funcarg, device, have_device,
				excl_dirs_names, excl_dirs_nodes,
				excl_files_names, excl_files_nodes)) ){
      CLEANUP;
    }
  }

  if(processit && S_ISDIR(statb.st_mode) && (params->options & FIND_DEPTH)){
    if( (ret = (*func)(filename, funcarg, &statb)) )
	CLEANUP;
  }

 cleanup:
  if(params->interrupted)
    if(*params->interrupted)
	ret = 128;

  ZFREE(buf);
  if(dir)
    closedir(dir);
  ZFREE(filename);
  if(local_excl_files){
    for(i = 0; i < num_local_excl_files; i++)
	ZFREE(local_excl_files[i]);
    free(local_excl_files);
  }

  return(ret);
}

Int32
find1(
  UChar		*filename,
  FindParams	*params,
  Int32		(*func)(UChar *, void *, struct stat *),
  void		*funcarg)
{
  UChar		**excl_dirs_names = NULL;
  FindFileNode	*excl_dirs_nodes = NULL;
  UChar		**excl_files_names = NULL;
  FindFileNode	*excl_files_nodes = NULL;
  UChar		**cpptr;
  Int32		i, n, r;
  Flag		use_ino_inf;
  struct stat	statb;

  if(params->excl_dirs){	/* for the dirs to excl containing slashes ... */
    for(n = 0, cpptr = params->excl_dirs; *cpptr; cpptr++, n++);
    if(n > 0){
	excl_dirs_names = NEWP(UChar *, n + 1);
	if(!excl_dirs_names)
	  CLEANUPR(-2);
	memset(excl_dirs_names, 0, (n + 1) * sizeof(UChar *));

	excl_dirs_nodes = NEWP(FindFileNode, n);
	if(!excl_dirs_nodes)
	  CLEANUPR(-3);
	memset(excl_dirs_nodes, 0, n * sizeof(FindFileNode));

	for(i = 0; i < n; i++){	/* ... use inode and device info for exclusion */
	  use_ino_inf = NO;

	  if(FN_ISPATH(params->excl_dirs[i])){
	    if(!stat(params->excl_dirs[i], &statb)){	/* if we cannot stat, there */
		excl_dirs_nodes[i].dev = statb.st_dev;	/* is no need to really */
		excl_dirs_nodes[i].ino = statb.st_ino;	/* exclude -> leave as is */
		use_ino_inf = YES;
	    }
	  }

	  if(use_ino_inf)
	    excl_dirs_names[i] = "";	/* empty string as marker */
	  else
	    excl_dirs_names[i] = params->excl_dirs[i];
	}
    }
  }

  if(params->excl_names){	/* for the files to excl containing slashes ... */
    for(n = 0, cpptr = params->excl_names; *cpptr; cpptr++, n++);
    if(n > 0){
	excl_files_names = NEWP(UChar *, n + 1);
	if(!excl_files_names)
	  CLEANUPR(-2);
	memset(excl_files_names, 0, (n + 1) * sizeof(UChar *));

	excl_files_nodes = NEWP(FindFileNode, n);
	if(!excl_files_nodes)
	  CLEANUPR(-3);
	memset(excl_files_nodes, 0, n * sizeof(FindFileNode));

	for(i = 0; i < n; i++){	/* ... use inode and device info for exclusion */
	  use_ino_inf = NO;

	  if(FN_ISPATH(params->excl_names[i])){
	    if(!stat(params->excl_names[i], &statb)){	/* if we cannot stat, there */
		excl_files_nodes[i].dev = statb.st_dev;	/* is no need to really */
		excl_files_nodes[i].ino = statb.st_ino;	/* exclude -> leave as is */
		use_ino_inf = YES;
	    }
	  }

	  if(use_ino_inf)
	    excl_files_names[i] = "";	/* empty string as marker */
	  else
	    excl_files_names[i] = params->excl_names[i];
	}
    }
  }

  r = real_find1(filename, params, func, funcarg, (dev_t) 0, NO,
				excl_dirs_names, excl_dirs_nodes,
				excl_files_names, excl_files_nodes);

 cleanup:
  ZFREE(excl_dirs_names);
  ZFREE(excl_dirs_nodes);
  ZFREE(excl_files_names);
  ZFREE(excl_files_nodes);

  return(r);
}

UChar **
fnglob1(UChar * namepat)
{
  struct stat	statb;
  UChar		**matches = NULL, *cptr;
  UChar		*buf = NULL;
  DIR		*dir = NULL;
  UChar		*dir_name = NULL;
  struct dirent	*entry;
  Int32		num_matches;

  if(!namepat){
    errno = EINVAL;
    return(NULL);
  }

  namepat = strdup(namepat);
  if(!namepat)
    return(NULL);

  cleanpath(namepat);

  if(FN_ISROOTDIR(namepat)){
    matches = NEWP(UChar *, 2);
    if(!matches)
	return(NULL);
    matches[0] = strdup(namepat);
    if(!matches[0])
	GETOUT;
    matches[1] = NULL;
    return(matches);
  }

  if(FN_ISPATH(namepat)){
    stat(namepat, &statb);	/* if it's automounted, trigger the mount */
    dir_name = strdup(namepat);
    if(!dir_name)
      return(NULL);
    cptr = FN_LASTDIRDELIM(dir_name);
    if(cptr){
      if(cptr > dir_name)
	*cptr = '\0';
      else
	dir_name[1] = '\0';
    }
    cptr = dir_name;
  }
  else{
    cptr = FN_CURDIR;
  }
  stat(cptr, &statb);		/* if it's automounted, trigger the mount */
  dir = opendir(cptr);

  if( (matches = NEWP(UChar *, 1)) )
    *matches = NULL;

  if(!dir)
    CLEANUP;

  buf = strdup("");
  if(!buf || !matches)
    GETOUT;

  num_matches = 0;

  forever{
    entry = readdir(dir);
    if(! entry)
      break;

    cptr = (UChar *) &(entry->d_name[0]);

    if((!strcmp(cptr, FN_CURDIR) || !strcmp(cptr, FN_PARENTDIR))
				&& strcmp(cptr, FN_BASENAME(namepat)))
      continue;

    buf = RENEWP(buf, UChar,
			(dir_name ? strlen(dir_name) : 0) + strlen(cptr) + 2);
    if(!buf)
	GETOUT;

    if(dir_name){
      sprintf(buf, "%s%c%s", dir_name, FN_DIRSEPCHR, cptr);
    }
    else{
      strcpy(buf, cptr);
    }

    cleanpath(buf);

    if(!fn_match(namepat, buf, GFNM_PATHNAME)){
	matches = RENEWP(matches, UChar *, num_matches + 2);
	if(!matches)
	  GETOUT;
	matches[num_matches] = strdup(buf);
	if(!matches[num_matches])
	  GETOUT;
	num_matches++;
	matches[num_matches] = NULL;
    }
  }

 cleanup:
  ZFREE(buf);
  ZFREE(dir_name);
  ZFREE(namepat);
  if(dir)
    closedir(dir);

  return(matches);

 getout:
  stringlist_free(matches, 0);
  matches = NULL;

  CLEANUP;
}

UChar **
fnglob(UChar * namepat)
{
  UChar		**uppermatches = NULL, **matches = NULL, **m;
  UChar		**newmatches = NULL, **mi;
  UChar		*buf = NULL, *bname = NULL, *cptr;
  Int32		i, j;

  if(FN_ISROOTDIR(namepat) || ! FN_ISPATH(namepat))
    return(fnglob1(namepat));

  buf = strdup("");
  namepat = strdup(namepat);
  if(!namepat || !buf)
    GETOUT;

  cleanpath(namepat);
  bname = strdup(FN_BASENAME(namepat));
  if(!bname)
    GETOUT;

  cptr = FN_LASTDIRDELIM(namepat);
  if(cptr){
    if(cptr > namepat)
	*cptr = '\0';
    else
	namepat[1] = '\0';
  }

  uppermatches = fnglob(namepat);
  if(!uppermatches)
    GETOUT;

  matches = NEWP(UChar *, 1);
  if(!matches)
    GETOUT;
  *matches = NULL;

  for(m = uppermatches; *m; m++){
    buf = RENEWP(buf, UChar, strlen(*m) + strlen(bname) + 2);
    if(!buf)
      GETOUT;

    sprintf(buf, "%s%c%s", *m, FN_DIRSEPCHR, bname);
    newmatches = fnglob1(buf);
    if(!newmatches)
      GETOUT;

    for(mi = matches, i = 0; *mi; mi++, i++);
    for(mi = newmatches, j = 0; *mi; mi++, j++);

    matches = RENEWP(matches, UChar *, i + j + 1);
    if(!matches)
      GETOUT;

    memcpy(matches + i, newmatches, j * sizeof(UChar *));
    matches[i + j] = NULL;
    free(newmatches);
    newmatches = NULL;
  }

 cleanup:
  if(uppermatches){
    for(m = uppermatches; *m; m++)
      free(*m);
    free(uppermatches);
  }
  if(buf)
    free(buf);
  if(bname)
    free(bname);
  if(namepat)
    free(namepat);

  return(matches);

 getout:
  if(matches){
    for(m = matches; *m; m++)
      free(*m);
    free(matches);
  }
  if(newmatches){
    for(m = newmatches; *m; m++)
      free(*m);
    free(newmatches);
  }
  matches = NULL;

  CLEANUP;
}

Int32
find(
  UChar		**filenames,
  FindParams	*params,
  Int32		(*func)(UChar *, void *, struct stat *),
  void		*funcarg)
{
  UChar		**matches = NULL, **m, **fn;
  Int32		ret = 0;

  if(!filenames){
    return(errno = EINVAL);
  }

  for(fn = filenames; *fn; fn++){
    matches = fnglob(*fn);
    if(!matches){
	ret = errno;
	CLEANUP;
    }

    for(m = matches; *m; m++){
      if( (ret = find1(*m, params, func, funcarg)) )
	CLEANUP;
    }

    for(m = matches; *m; m++)
	free(*m);
    free(matches);
    matches = NULL;
  }

 cleanup:
  if(matches){
    for(m = matches; *m; m++)
      free(*m);
    free(matches);
  }

  return(ret);
}

/* copies srcfile to destfile. Permissions are preserved.
 *
 * returns 0 on success, otherwise errno i.e. a value != 0
 */

Int32
copy_file(UChar * srcfile, UChar * destfile)
{
  int		infd, outfd, i;
  UChar		buf[BUFSIZ];
  struct stat	statb;
  Int32		ret = 0;

  if(stat(srcfile, &statb))
    return(errno);

  infd = open(srcfile, O_RDONLY);
  if(infd < 0)
    return(errno);
  unlink(destfile);
  outfd = open(destfile, O_WRONLY | O_BINARY | O_CREAT | O_EXCL | O_TRUNC, 0600);
  if(outfd < 0){
    close(infd);
    return(errno);
  }

  while((i = read(infd, buf, BUFSIZ)) > 0){
    if(write_forced(outfd, buf, i) < i){
	ret = errno;
	break;
    }
  }

  close(outfd);
  close(infd);

  if(chmod(destfile, statb.st_mode)){
    if(!ret)
	ret = errno;
  }

  if(ret)
    unlink(destfile);

  return(ret);
}

/* sets a write lock on the (entire) file with the given name
 * and returns the filedescriptor to the open file. On failure
 * a value < 0 is returned
 */

static int
set_lock(UChar * name, int filemode, int lockmode)
{
  int		fd, i;
  struct flock	lockb;

  fd = open(name, filemode, 0600);
  if(fd < 0)
    return(fd);

  SETZERO(lockb);
  lockb.l_type = lockmode;

  i = fcntl(fd, F_SETLK, &lockb);
  if(i < 0)
    close(fd);

  return(i < 0 ? i : fd);
}

int
set_wlock(UChar * name)
{
  return(set_lock(name, O_WRONLY | O_CREAT, F_WRLCK));
}

int
set_rlock(UChar * name)
{
  return(set_lock(name, O_RDONLY, F_RDLCK));
}

/* given the name of a file, probably a complete path to a file
 * (i.e. not only the 'basename'), a filename with a leading dot
 * . is produced, memory malloc'ed for it and the pointer to it
 * returned. On failure NULL is returned
 *
 * example: /path/to/file results in /path/to/.file
 */

UChar *
hidden_filename(UChar * filename)
{
  UChar	*new_name, *cptr;

  new_name = NEWP(UChar, strlen(filename) + 2);
  if(!new_name)
    return(NULL);

  strcpy(new_name, filename);

  cptr = FN_LASTDIRDELIM(new_name);
  if(cptr)
    cptr++;
  else
    cptr = new_name;

  memmove(cptr + 1, cptr, (strlen(cptr) + 1) * sizeof(UChar));
  *cptr = '.';

  return(new_name);
}


/* like mkdir -p <name> i.e. a complete directory path is created,
 * if not existing. If uid and/or gid is -1, owner/group remains
 * unchanged. mode is applied for permissions
 */

Int32
make_dirpath(UChar * name, Uns32 mode, uid_t uid, gid_t gid)
{
  UChar		*cptr, c;
  struct stat	statb;
  Int32		r, i;

  r = 0;
  name = strdup(name);
  if(!name)
    return(-1);

  cleanpath(name);

  i = lstat(name, &statb);
  if(!i){
    i = stat(name, &statb);
    if(i)
	CLEANUPR(-1);

    if(!S_ISDIR(statb.st_mode)){
	errno = EEXIST;
	CLEANUPR(-1);
    }

    CLEANUPR(0);
  }

  if(i < 0){
    cptr = FN_LASTDIRDELIM(name);
    if(cptr){
	if(cptr > name){
	  c = *cptr;
	  *cptr = '\0';
	  i = make_dirpath(name, mode, uid, gid);

	  if(i < 0)	/* if only chown failed, continue making path */
	    CLEANUPR(-errno);

	  *cptr = c;
	}
    }

    i = mkdir(name, mode);
    if(i && errno != EEXIST)
	CLEANUPR(-errno);

    if(!i && (uid != (uid_t) -1 || gid != (gid_t) -1))
      if(chown(name, uid, gid))
	 CLEANUPR(1);	/* return positive on non-fatal chown */
  }

 cleanup:
  free(name);

  return(r);
}

static Int32
kfile_proc(UChar * filen, UChar * key, UChar * value, Uns32 flags, Flag insert)
{
  FILE		*rfp = NULL, *wfp = NULL;
  UChar		*line = NULL, *tkey = NULL, *nfilen = NULL;
  Int32		i, r = 0;
  int		i1, i2, lockfd;
  Flag		file_existed = NO, sorted_by_n;
  struct stat	statb;

  i = eaccess(filen, F_OK);
  if(!i){
    if(eaccess(filen, R_OK))
	return(-1);
    if(stat(filen, &statb) < 0)
	return(-1);
    file_existed = YES;
  }

  lockfd = -1;
  if(flags & KFILE_LOCKED){
    lockfd = set_wlock_forced(filen);
    if(lockfd < 0 && !(flags & KFILE_OPTLOCK))
	CLEANUPR(-7);
  }

  if(!i){
    rfp = fopen(filen, "r");
    if(!rfp)
	return(-2);
  }

  nfilen = strapp(filen, ".tmp");
  if(!nfilen)
    CLEANUPR(-3);

  unlink(nfilen);
  if(!eaccess(nfilen, F_OK))
    CLEANUPR(-4);

  wfp = fopen(nfilen, "w");
  if(!wfp)
    CLEANUPR(-5);

  if(rfp){
    if(flags & KFILE_SORTN)
      if(sscanf(key, "%d", &i1) < 1)
	i1 = MAXINT;

    while(!feof(rfp)){
	ZFREE(line);
	line = fget_alloc_str(rfp);
	if(!line)
	  continue;
	chop(line);

	tkey = ZRENEWP(tkey, UChar, strlen(line) + 1);
	if(!tkey)
	  GETOUTR(-8);
	sscanwordq(line, tkey);
	if(!strcmp(key, tkey)){
	  ZFREE(line);
	  break;
	}

	sorted_by_n = NO;
	if(flags & KFILE_SORTN){
	  if(sscanf(line, "%d", &i2) < 1)
	    i2 = -2;
	  else
	    sorted_by_n = YES;

	  if(i2 >= i1)
	    break;
	}
	if((flags & KFILE_SORT) && !sorted_by_n){
	  i = strcmp(tkey, key);
	  if(i >= 0)
	    break;
	}

	fprintf(wfp, "%s\n", line);
    }
  }

  if(insert){
    fprintwordq(wfp, key);
    fprintf(wfp, " %s\n", value);
  }

  if(rfp){
    if(line)
	fprintf(wfp, "%s\n", line);

    while(!feof(rfp)){
	ZFREE(line);
	line = fget_alloc_str(rfp);
	if(!line)
	  continue;
	chop(line);
	tkey = ZRENEWP(tkey, UChar, strlen(line) + 1);
	if(!tkey)
	  GETOUTR(-9);
	sscanwordq(line, tkey);
	if(strcmp(tkey, key))
	  fprintf(wfp, "%s\n", line);
    }
  }
  fclose(wfp);
  wfp = NULL;
  if(file_existed){
    chown(nfilen, statb.st_uid, statb.st_gid);
    chmod(nfilen, statb.st_mode);
  }
  if(rename(nfilen, filen))
    GETOUTR(-6);

  if(lockfd >= 0)
    close(lockfd);

 cleanup:
  ZFREE(tkey);
  ZFREE(nfilen);
  ZFREE(line);
  if(rfp)
    fclose(rfp);
  if(wfp)
    fclose(wfp);

  return(r);

 getout:
  if(nfilen)
    unlink(nfilen);

  CLEANUP;
}

/*
 * Opens the file <filen>, searches for a key <key> as the word or (if
 * containing whitespace) string in double quotes in the first column of
 * a line, and replaces the key's value (i.e. the rest of the line) with
 * the string passed in value. If the <key> has not been found, a new
 * line is created for the new <key> <value> pair. If KFILE_SORT is set
 * in <flags>, the lines in the file are sorted in ASCII order of the
 * <key> fields. If KFILE_SORTN is set, it is attempted to interpret the
 * <key> fields as numerical values and to sort the lines accordingly.
 * If both are set, sorting by numerical value has higher priority than
 * sorting by ASCII. If KFILE_LOCKED is set in <flags>, then the file is
 * locked before modifying it. If KFILE_OPTLOCK is set (optional lock,
 * includes KFILE_LOCKED), a lock is attempted, but if that fails, the
 * failure is ignored and processing continues.
 * If <filen> does not exist, it is created.
 *
 * Returns NULL on error.
 */

Int32
kfile_insert(UChar * filen, UChar * key, UChar * value, Uns32 flags)
{
  return(kfile_proc(filen, key, value, flags, YES));
}

/*
 * Opens the file <filen>, searches for a key <key> as the word or (if
 * containing whitespace) string in double quotes in the first column of
 * a line, and removes this line from the file. If KFILE_LOCKED is set in
 * <flags>, then the file is locked before modifying it.
 */

Int32
kfile_delete(UChar * filen, UChar * key, Uns32 flags)
{
  return(kfile_proc(filen, key, "", flags, NO));
}

/*
 * Opens the file <filen>, searches for a key <key> as the word or (if
 * containing whitespace) string in double quotes in the first column of
 * a line, and returns a malloc'd string containing the key's value (i.e.
 * the rest of the line). If KFILE_LOCKED is set in <flags>, then the file
 * is locked before reading it.
 *
 * Returns NULL on error or if no line starting with <key> is found.
 *
 * kfile_get does not make assumptions on the file being sorted
 */

UChar *
kfile_get(UChar * filen, UChar * key, Uns32 flags)
{
  UChar		*fval = NULL, *line = NULL, *tkey = NULL, *cptr;
  FILE		*fp;
  int		lockfd;

  /* make sure the file exists and we can read it */
  if(eaccess(filen, F_OK))
    return(NULL);
  /* we cannot do the eaccess(..., F_OK | R_OK) as documented, but must
   * do it in two steps, because F_OK is usually defined as 0, so in
   * fact this form would be the same like eaccess(..., R_OK) :-( */
  if(eaccess(filen, R_OK)){
    return(NULL);
  }

  lockfd = -1;
  if(flags & KFILE_LOCKED){
    lockfd = set_rlock_forced(filen);
    if(lockfd < 0 && !(flags & KFILE_OPTLOCK))
	return(NULL);
  }

  fp = fopen(filen, "r");
  if(!fp)
    return(NULL);

  while(!feof(fp)){
    ZFREE(line);
    line = fget_alloc_str(fp);
    if(!line)
	continue;
    chop(line);

    tkey = ZRENEWP(tkey, UChar, strlen(line) + 1);
    if(!tkey)
	GETOUT;

    cptr = sscanwordq(line, tkey);
    if(cptr){
      if(!strcmp(key, tkey)){
	if(*(cptr - 1) == ' '){
	  strcpy(tkey, cptr);
	  tkey = RENEWP(tkey, UChar, strlen(tkey) + 1);
	  fval = tkey;
	  tkey = NULL;
	  CLEANUP;
	}
      }
    }
  }

 cleanup:
  if(fp)
    fclose(fp);
  if(lockfd >= 0)
    close(lockfd);
  ZFREE(tkey);
  ZFREE(line);

  return(fval);

 getout:
  ZFREE(fval);
  CLEANUP;
}

KeyValuePair *
kfile_getall(UChar * filename, Int32 * num, Uns32 flags)
{
  KeyValuePair	*all_pairs = NULL;
  UChar		**lines, **cpptr, *line, *cptr;
  Int32		i, n, num_lines;
  int		lockfd;		/* uninitialized ok */

  if(flags & KFILE_LOCKED){
    lockfd = set_rlock_forced(filename);
    if(lockfd < 0 && !(flags & KFILE_OPTLOCK))	/* optional lock */
	return(NULL);
  }

  lines = read_asc_file(filename, &num_lines);

  if((flags & KFILE_LOCKED) && lockfd >= 0)
    close(lockfd);

  if(!lines){
    if(num)
	*num = 0;

    return(NULL);
  }

  n = 0;
  for(cpptr = lines; *cpptr; cpptr++){
    line = *cpptr;
    if(word_count(line) > 0){
	all_pairs = ZRENEWP(all_pairs, KeyValuePair, n + 2);
	if(!all_pairs)
	  GETOUT;

	memset(all_pairs + n, 0, 2 * sizeof(KeyValuePair));

	if(!(cptr = all_pairs[n].key = strwordq(line, 0)))
	  GETOUT;

	cptr = sscanwordq(line, NULL);
	if(*cptr && isspace(*cptr))
	  cptr++;

	if(!(all_pairs[n].value = strdup(cptr)))
	  GETOUT;

	n++;
    }
  }

  if(num)
    *num = n;

  if(flags & KFILE_SORTN)
    q_sort(all_pairs, n, sizeof(all_pairs[0]), cmp_KeyValue_bykeyn);
  else if(flags & KFILE_SORT)
    q_sort(all_pairs, n, sizeof(all_pairs[0]), cmp_KeyValue_bykey);

 cleanup:
  free_asc_file(lines, 0);

  return(all_pairs);

 getout:
  for(i = 0; i < n; i++){
    ZFREE(all_pairs[i].key);
    ZFREE(all_pairs[i].value);
  }
  ZFREE(all_pairs);

  CLEANUP;
}

void
kfile_freeall(KeyValuePair * kvs, Int32 num)
{
  Int32		i;

  if(!kvs)
    return;

  for(i = 0; num < 1 || i < num; i++){
    if(num < 1 && !kvs[i].key && !kvs[i].value)
	break;

    ZFREE(kvs[i].key);
    ZFREE(kvs[i].value);
  }

  free(kvs);
}

Int32
add_to_int_list_file(UChar * filename, Int32 val)
{
  UChar		nbuf[32];

  sprintf(nbuf, "%d", (int) val);

  return(kfile_insert(filename, nbuf, "", KFILE_SORTN | KFILE_LOCKED));
}

Int32 *
get_list_from_int_list_file(UChar * filename, Int32 * rnum)
{
  KeyValuePair	*values;
  Int32		i, num_vals, *intvals;
  int		i1;

  values = kfile_getall(filename, &num_vals, 0);
  if(!values)
    return(NULL);

  intvals = NEWP(Int32, num_vals);
  if(!intvals)
    CLEANUP;

  for(i = 0; i < num_vals; i++){
    intvals[i] = 0;

    if(sscanf(values[i].key, "%d", &i1) > 0)
	intvals[i] = i1;
  }

  if(rnum)
    *rnum = num_vals;

 cleanup:
  kfile_freeall(values, num_vals);

  return(intvals);
}


UChar *
tmp_name(UChar * base)
{
  static Int32	count = 0;

  UChar		*tmpdir = NULL, *cptr;
  Int32		pid, i, n, tc;
  time_t	t1, t2, t3;
  struct timeval	tv1, tv2, *tv;
  struct stat	statb;

  if(!base)
    base = FN_TMPDIR FN_DIRSEPSTR;

  tmpdir = NEWP(UChar, strlen(base) + 100);
  if(!tmpdir)
    CLEANUP;

  do{
    tc = count;		/* to be multithreading-safe */
    count++;
  }while(count != tc + 1);

  do{
    strcpy(tmpdir, base);
    n = strlen(tmpdir);
    if(n > 0){
	if(!strchr("_.-", tmpdir[n - 1]))
	  strcat(tmpdir, "_");
    }

    cptr = tmpdir + strlen(tmpdir);
    do{
	t1 = time(NULL);
	gettimeofday(&tv1, NULL);
	t2 = time(NULL);
	gettimeofday(&tv2, NULL);
	t3 = time(NULL);
    }while(t1 != t2 && t2 != t3);

    tv = &tv1;
    if(t2 == t3){
	t1 = t2;
	tv = &tv2;
    }

    pid = getpid();		/* to be unique in this millisecond */
    while(pid > 0){
	*(cptr++) = char64((Int32) (pid & 63));
	pid >>= 6;
    }
    *(cptr++) = '_';		/* to be more unique and in this process */
    i = (Int32) (drandom() * 65536) | (tc << 16);
    while(i > 0){
	*(cptr++) = char64((Int32) (i & 63));
	i >>= 6;
    }
    *(cptr++) = '_';		/* to be unique in seconds */
    while(t1 > 0){
	*(cptr++) = char64((Int32) (t1 & 63));
	t1 >>= 6;
    }
    *(cptr++) = '_';
    while(tv->tv_usec > 0){	/* to be unique within this second */
	*(cptr++) = char64((Int32) (tv->tv_usec & 63));
	tv->tv_usec >>= 6;
    }
    *cptr = '\0';
  }while(!stat(tmpdir, &statb));

  tmpdir = RENEWP(tmpdir, UChar, strlen(tmpdir) + 1);

 cleanup:
  return(tmpdir);
}

int
tmp_file(UChar * base)
{
  UChar		*tmpn;
  int		fd = -1, e;

  do{
    tmpn = tmp_name(base);
    if(!tmpn)
	return(-1);

    fd = open(tmpn, O_RDWR | O_CREAT | O_EXCL, 0600);
    e = errno;

    unlink(tmpn);
    free(tmpn);

    if(fd < 0 && e != EEXIST)
	break;
  }while(fd < 0);

  return(fd);
}

FILE *
tmp_fp(UChar * base)
{
  int	fd;

  fd = tmp_file(base);
  if(fd < 0)
    return(NULL);

  return(fdopen(fd, "r+"));
}

/* returns 0 on success; 1, if chuid/chgid failed; < 0, if mkdir failed */

Int32
mkbasedir(UChar * name, mode_t mode, uid_t uid, gid_t gid)
{
  UChar		*cptr;
  struct stat	statb;
  Int32		r, i;

  name = strdup(name);
  if(!name)
    return(-1);

  cleanpath(name);
  r = 0;

  cptr = FN_LASTDIRDELIM(name);
  if(cptr){
    if(cptr == name)
	CLEANUPR(0);

    *cptr = '\0';

    i = stat(name, &statb);
    if(!i){
      if(!S_ISDIR(statb.st_mode)){
	errno = EEXIST;
	CLEANUPR(-1);
      }
    }

    if(i < 0){
	if( (r = mkbasedir(name, mode, uid, gid)) < 0){
	  *cptr = FN_DIRSEPCHR;
	  CLEANUPR(r);
	}

	r = mkdir(name, mode);
	if(r){
	  *cptr = FN_DIRSEPCHR;
	  CLEANUPR(-errno);
	}

	if(uid != (uid_t) -1 || gid != (gid_t) -1){
	  if(chown(name, uid, gid)){
	    *cptr = FN_DIRSEPCHR;
	    r = 1;
	  }
	}
    }

    *cptr = FN_DIRSEPCHR;
  }

 cleanup:
  free(name);

  return(r);
}

Int32
mkdirpath(UChar * name, mode_t mode, uid_t uid, gid_t gid)
{
  Int32		i;

  name = strapp(name, FN_DIRSEPSTR "a");
  if(!name)
    return(-1);

  i = mkbasedir(name, mode, uid, gid);

  free(name);

  return(i);
}

Int32
perms_from_string(UChar * string, mode_t * rperms)
{
  UChar		*locstr = NULL, *cptr;
  Int32		r = 0;
  mode_t	perms = 0;
  int		i1, n1, whomap;

  if(!(locstr = strdup(string)))
    CLEANUP;

  for(cptr = locstr; *cptr; cptr++){
    if(isspace(*cptr)){
	memmove(cptr, cptr + 1, strlen(cptr));
	cptr--;
    }
  }

  i1 = n1 = -1;
  sscanf(locstr, "%o%n", &i1, &n1);
  if(n1 == strlen(locstr)){
    perms = i1;
  }
  else{
    perms = 0;

    for(cptr = locstr; *cptr; cptr++){
      if(*cptr == ',')
	continue;

      whomap = 0;

      if(!strchr("ugoa-=", *cptr))
	CLEANUPR(-1);

      while(*cptr && strchr("ugoa-", *cptr)){
	switch(*cptr){
	 case 'u':
	  whomap |= 04700;
	  break;
	 case 'g':
	  whomap |= 02070;
	  break;

	 case 'o':
	  whomap |= 01007;
	  break;

	 case 'a':
	  whomap |= 07777;
	  break;
	}

	cptr++;
      }

      if(*cptr == ',')
	continue;

      if(*cptr != '=')
	CLEANUPR(-2);

      cptr++;
      if(*cptr == ',')
	continue;
      if(!strchr("rwxst-", *cptr))
	CLEANUPR(-3);

      while(*cptr && strchr("rwxst-", *cptr)){
	switch(*cptr){
	 case 'r':
	  perms |= (whomap & 0444);
	  break;
	 case 'w':
	  perms |= (whomap & 0222);
	  break;
	 case 'x':
	  perms |= (whomap & 0111);
	  break;
	 case 't':
	  perms |= 01000;
	  break;
	 case 's':
	  perms |= (whomap & 06000);
	  break;
	}

	cptr++;
      }

      if(*cptr && *cptr != ',')
	CLEANUPR(-1);
    }
  }

  if(rperms)
    *rperms = perms;

 cleanup:
  ZFREE(locstr);

  return(r);
}

static char	*filemodestr = "sstrwxrwxrwx";

void
permstr_from_mode(char * retstr, mode_t mode)
{
  int	i;

  retstr[10] = retstr[11] = '\0';
  strcpy(retstr, "----------");
  for(i = 0; i < 9; i++)
    retstr[9 - i] = ((mode & (1 << i)) ? filemodestr[11 - i] : '-');
  for(i = 0; i < 3; i++)
    if(mode & (1 << (i + 9)))
	retstr[9 - 3 * i] = (retstr[9 - 3 * i] == '-' ? toupper(filemodestr[2 - i]) : filemodestr[2 - i]);
}

static Int32
close_fd(Int32 fd, void * data)
{
  close((int) fd);
  return(0);
}

void
close_fd_ranges(Uns32Range * fds)
{
  foreach_Uns32Ranges(fds, close_fd, NULL);
}

Int32
pipethrough(int infd, int outfd)
{
  int	ifd, ofd, pid, n;
  UChar	buf[8192];

  ifd = ABS(infd);
  ofd = ABS(outfd);

  pid = fork_forced();
  if(pid < 0)
    return(-1);

  if(pid == 0){
    chdir("/");
    setsid();
    detach_from_tty();

    do{
	n = read(ifd, buf, 8192);
	if(n > 0 && infd != outfd)
	  write(ofd, buf, n);
    }while(n > 0);

    exit(0);
  }

  if(infd < 0)
    close(ifd);
  if(outfd < 0)
    close(ofd);

  return(0);  
}

UChar *
parentdir(UChar * dir, Flag resolve)
{
  UChar		*cptr, *realpath;

  cptr = realpath = mkabspath(dir, NULL);
  if(!cptr)
    return(NULL);

  if(resolve){
    realpath = resolve_path(cptr, NULL);
    free(cptr);
    if(!realpath)
	return(NULL);
  }

  cleanpath__(realpath);

  if(FN_ISROOTDIR(realpath))
    return(realpath);

  cptr = FN_LASTDIRDELIM(realpath);
  if(!cptr){
    free(realpath);
    return(NULL);	/* This cannot happen */
  }

  if(cptr == (UChar *) FN_FIRSTDIRSEP(realpath))
    cptr += strlen(FN_DIRSEPSTR);

  *cptr = '\0';

  return(realpath);
}

Flag
fsentry_access(UChar * path, int flag)
{
  UChar		*pdir = NULL;
  Flag		r = DONT_KNOW, resolve;
  struct stat	statb;

  if(!path){
    errno = EINVAL;
    return(NO);
  }
  if(!strcmp(path, "")){
    errno = EINVAL;
    return(DONT_KNOW);
  }

  if(!eaccess(path, flag))	/* access is ok -> end of story */
    return(YES);

  resolve = NO;
  if(!lstat(path, &statb))
    resolve = IS_SYMLINK(statb);

  pdir = parentdir(path, resolve);
  if(!pdir)
    return(DONT_KNOW);

  r = (eaccess(pdir, X_OK) ? DONT_KNOW : NO);

  free(pdir);
  return(r);
}

/* e_access: access(2) with the effective uid/gid.
 * The only really significant test is to try to do it.
 * For the execute flag, this is not appropriate, so the
 * normal procedure is implemented here. But this does
 * not take possible ACLs into account. For now this is
 * accepted. In the long run access should not be used.
 * It's anyway only a means of predicting, what might
 * fail later, so one can as well do the next steps and
 * carefully evaluate the returned status */
int e_access(char * path, int mode)
{
  struct stat	statb;
  DIR		*dir;
  int		fd, openmode, lm;
  UChar		*tmpfile, lbuf[256];

  if(mode & (~(F_OK | R_OK | W_OK | X_OK))){
    errno = EINVAL;
    return(-1);
  }

  if(stat(path, &statb) < 0)
    return(-1);

  if(F_OK == 0 && mode == F_OK)
    return(0);

  if(! IS_DIRECTORY(statb)){
    lm = (mode & (R_OK | W_OK));
    if(lm){
      if(lm == (R_OK | W_OK))
	openmode = O_RDWR;
      else if(lm == R_OK)
	openmode = O_RDONLY;
      else
	openmode = O_WRONLY;

      fd = open(path, openmode);
      if(fd >= 0)
	close(fd);

      if(fd < 0)
	return(-1);
    }
  }
  else{		/* directory */
    if(mode & W_OK){
	tmpfile = strchain(path, FN_DIRSEPSTR, "eaccess", NULL);
	if(!tmpfile)
	  return(-1);

	fd = tmp_file(tmpfile);
	free(tmpfile);
	if(fd >= 0)
	  close(fd);
	if(fd < 0)
	  return(-1);
    }
    if(mode & R_OK){
	dir = opendir(path);
	if(dir)
	  closedir(dir);
	if(!dir)
	  return(-1);
    }
    if(mode & X_OK){
	tmpfile = get_cwd(lbuf, 256);
	if(tmpfile){
	  lm = chdir(path);
	  chdir(tmpfile);

	  if(tmpfile != lbuf)
	    free(tmpfile);

	  return(lm ? -1 : 0);
	}
    }
  }

	/* FIXME: ACLs not supported here. Cannot execute for testing !!! */
  if(mode & X_OK){
    if(geteuid() == statb.st_uid)
	return((statb.st_mode & S_IXUSR) ? 0 : -1);
    if(getegid() == statb.st_gid)
	return((statb.st_mode & S_IXGRP) ? 0 : -1);
    return((statb.st_mode & S_IXOTH) ? 0 : -1);
  }

  return(0);
}

Int32
compare_files(UChar * file1, UChar * file2)
{
  struct stat	statb1, statb2;
  int		fd1, fd2, r, i, n;
  UChar		buf1[4096], buf2[4096];

  r = fd1 = fd2 = -1;

  if(stat(file1, &statb1) < 0 || stat(file2, &statb2) < 0)
    return(-1);

  if(statb1.st_size != statb2.st_size)
    return(1);

  fd1 = open(file1, O_RDONLY);
  fd2 = open(file2, O_RDONLY);
  if(fd1 < 0 || fd2 < 0)
    CLEANUP;

  r = 3;
  for(i = 0; i < statb1.st_size; i += n){
    n = statb1.st_size - i;
    if(n > 4096)
	n = 4096;

    if(read_forced(fd1, buf1, n) < n || read_forced(fd2, buf2, n) < n)
	CLEANUP;
    if(memcmp(buf1, buf2, n))
	CLEANUP
  }

  r = 0;

 cleanup:
  if(fd1 >= 0)
    close(fd1);
  if(fd2 >= 0)
    close(fd2);

  return(r);
}

UChar *
read_file(UChar * filename, Int64 * filesz)
{
  return(read_file_x(filename, 0, 0, NULL, filesz));
}

UChar *
read_file_x(UChar * filename, Int64 offs, Int64 n, Int64 * nread, Int64 * filesz)
{
  struct stat	statb;
  UChar		*content = NULL;
  int		fd;

  if(stat(filename, &statb) < 0)
    return(NULL);
  if(!IS_REGFILE(statb)){
    errno = EINVAL;
    return(NULL);
  }

  if(n == 0 || n > statb.st_size - offs)
    n = statb.st_size - offs;

  if(offs > statb.st_size){
    n = 0;
    offs = 0;
  }

  fd = -1;

  content = NEWP(UChar, n + 1);			/* allocate 1 byte more for */
  if(!content)					/* a often useful tailing 0 */
    GETOUT;

  if(n > 0){
    fd = open(filename, O_RDONLY);
    if(fd < 0)
	GETOUT;
    if(lseek(fd, offs, SEEK_SET) != offs)
	GETOUT;
    if(read_forced(fd, content, n) < n)
	GETOUT;
  }

  content[n] = '\0';
  if(nread)
    *nread = n;
  if(filesz)
    *filesz = statb.st_size;

 cleanup:
  if(fd >= 0)
    close(fd);
  return(content);

 getout:
  ZFREE(content);
  CLEANUP;
}

int
read_Uns64_file(UChar * file, Uns64 * ret)
{
  int	fd, n;
  char	*cptr, buf[64];

  if(!ret){
    errno = EINVAL;
    return(-3);
  }

  fd = open(file, O_RDONLY);
  if(fd < 0)
    return(-1);

  n = read(fd, &buf, 63);
  close(fd);
  if(n < 0)
    n = 0;

  buf[n] = '\0';
  massage_string(buf);
  if(!isdigit(buf[0])){
    errno = EINVAL;
    return(-2);
  }

  *ret = 0;
  for(cptr = buf; *cptr && isdigit(*cptr); cptr++)
    *ret = *ret * 10 + ((int)(*cptr) - (int)('0'));

  return(0);
}

int
write_Uns64_file(UChar * file, Uns64 n, int perms)
{
  char	buf[64];
  int	i, fd;

  memset(buf, ' ', 63);
  buf[63] = '\0';
  buf[62] = '0';
  i = 62;
  while(n > 0){
    buf[i--] = (n % 10) + '0';
    n /= 10;
  }
  massage_string(buf);
  strcat(buf, "\n");

  fd = open(file, O_WRONLY | O_TRUNC | O_CREAT, perms);
  if(fd < 0)
    return(-1);

  i = strlen(buf);
  n = write(fd, buf, i);
  close(fd);
  if(n < i){
    unlink(file);
    return(-2);
  }

  return(0);
}

/************ end of $RCSfile: fileutil.c,v $ ******************/
