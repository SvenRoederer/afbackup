/****************** Start of $RCSfile: strlist.c,v $  ****************
*
* $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/strlist.c,v $
* $Id: strlist.c,v 1.7 2014/07/26 18:05:58 alb Exp alb $
* $Date: 2014/07/26 18:05:58 $
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

  static char * fileversion = "$RCSfile: strlist.c,v $ $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/strlist.c,v $ $Id: strlist.c,v 1.7 2014/07/26 18:05:58 alb Exp alb $ " PACKAGE " " VERSION_STRING;

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <sys/stat.h>

#include <genutils.h>
#include <fileutil.h>

UChar **
read_asc_file(
  UChar		*filename,
  Int32		*rows)
{
  FILE		*fp;
  Int32		i, num_lines = 0, leng, old_size;
  UChar		*line_buf;
  UChar		**file, **newfile;

  if(!filename)
    filename = "";

  if(filename[0])
    fp = fopen(filename, "r");
  else
    fp = stdin;

  if(!fp)
    return(NULL);

  old_size = sizeof(UChar **);
  file = (UChar **) seg_malloc(old_size);
  if(!file)
    GETOUT;

  file[0] = NULL;

  while(!feof(fp)){
    line_buf = fget_alloc_str(fp);
    if(!line_buf)
	continue;

    leng = strlen(line_buf) - 1;
    if(line_buf[leng] == '\n')
	line_buf[leng] = '\0';
    num_lines++;
    newfile = (UChar **) seg_realloc(file, (num_lines + 1) * sizeof(UChar *),
				old_size);
    if(! newfile)
	GETOUT;
    old_size = (num_lines + 1) * sizeof(UChar *);

    file = newfile;
    file[num_lines - 1] = strdup(line_buf);
    file[num_lines] = NULL;
    free(line_buf);

    if(!file[num_lines - 1])
	GETOUT;
  }

  if(rows)
    *rows = num_lines;

 cleanup:
  if(filename[0])
    fclose(fp);

  return(file);

 getout:
  if(file){
    for(i = 0; file[i]; i++)
	free(file[i]);
    ZFREE(file);
  }

  CLEANUP;
}

UChar **
empty_asc_file()
{
  UChar		**lines;

  lines = NEWP(UChar *, 1);
  if(lines)
    lines[0] = NULL;

  return(lines);
}

void
free_asc_file(
  UChar		**file,
  Int32		rows)
{
  Int32		i;

  if(file){
    for(i = 0; file[i] && (rows < 1 || i < rows); i++)
      ZFREE(file[i]);
    free(file);
  }
}

Int32
write_asc_file(
  UChar		*filename,
  UChar		**lines,
  Int32		rows)
{
  FILE		*fp;
  Int32		i, r = 0;

  if(!lines){
    errno = EINVAL;
    return(-1);
  }
  if(filename)
    fp = fopen(filename, "w");
  else
    fp = stdout;

  if(!fp)
    return(-1);

  for(i = 0; *lines && (rows < 1 || i < rows); i++, lines++){
    if(fprintf(fp, "%s\n", *lines) < 0){
	r = -1;
	break;
    }
  }

  if(filename)
    fclose(fp);

  return(r);
}

Int32
write_asc_file_safe(
  UChar		*filename,
  UChar		**lines,
  Int32		rows)
{
  UChar		*tmpfilename = NULL;
  Int32		i, r = 0;
  struct stat	statb;
  Flag		oldfile;

  oldfile = ! eaccess(filename, F_OK);
  if(oldfile)
    if(stat(filename, &statb))
	return(-5);

  tmpfilename = tmp_name(filename);
  if(!tmpfilename)
    return(-2);

  i = unlink(tmpfilename);
  if(i && errno && errno != ENOENT){	/* On Solaris unlink might return */
	r = -3;				/* -1, but errno is 0 (?!?!?) */
	CLEANUP;
  }

  r = write_asc_file(tmpfilename, lines, rows);
  if(r)
    CLEANUP;

  if(oldfile){
    if(chmod(tmpfilename, statb.st_mode)){
	r = -6;
	CLEANUP;
    }

    i = errno;		/* the following chown might fail, but we don't */
    chown(tmpfilename, statb.st_uid, statb.st_gid);	/* consider this */
    errno = i;						/* a real problem */
  }
  

  i = rename(tmpfilename, filename);
  if(i)
    r = -4;

 cleanup:
  unlink(tmpfilename);
  ZFREE(tmpfilename);

  return(r);
}

Int32
stringlist_remove(UChar *** list1, UChar ** list2)
{
  UChar		**cpptr, **fptr;
  Int32		len1, i;

  if(!(*list1) || !list2)
    return(0);

  for(cpptr = *list1, len1 = 0; *cpptr; cpptr++, len1++);

  for(cpptr = list2; *cpptr; cpptr++){
    while( (fptr = l_find(cpptr, *list1, &len1, sizeof(UChar *), cmp_UCharPTR)) ){
	i = fptr - *list1;
	free(*fptr);
	memmove(fptr, fptr + 1, (len1 - i) * sizeof(UChar *));
	len1--;
    }
  }

  return(0);
}

Int32
stringlist_combine(UChar *** list1, UChar ** list2)
{
  UChar		**cpptr, **fptr;
  Int32		len1, r = 0;

  if(!list2)
    return(0);

  if(!(*list1)){
    if(!(*list1 = NEWP(UChar *, 1)))
	CLEANUPR(-1);
    (*list1)[0] = NULL;
  }

  for(cpptr = *list1, len1 = 0; *cpptr; cpptr++, len1++);

  for(cpptr = list2; *cpptr; cpptr++){
    fptr = l_find(cpptr, *list1, &len1, sizeof(UChar *), cmp_UCharPTR);
    if(!fptr){
	fptr = ZRENEWP(*list1, UChar *, len1 + 2);
	if(!fptr)
	  CLEANUPR(-1);

	*list1 = fptr;
	if(!((*list1)[len1] = strdup(*cpptr)))
	  CLEANUPR(-1);
	len1++;
	(*list1)[len1] = NULL;
    }
  }

 cleanup:
  return(r);
}

Int32
stringlist_common(UChar *** list1, UChar ** list2)
{
  UChar		**cpptr, **fptr;
  Int32		len1, len2, i, r = 0;
  Flag		empty = NO;

  if(!list1 || !list2)
    empty = YES;
  else
    if(list1)
      if(! *list1)
	empty = YES;

  if(empty){
    if(list1){
	free_array(*list1, 0);
	*list1 = NULL;
    }
    return(0);
  }

  for(cpptr = *list1, len1 = 0; *cpptr; cpptr++, len1++);
  for(cpptr = list2, len2 = 0; *cpptr; cpptr++, len2++);

  for(cpptr = *list1; *cpptr; cpptr++){
    fptr = l_find(cpptr, list2, &len2, sizeof(UChar *), cmp_UCharPTR);
    if(!fptr){
	free(*cpptr);
	i = cpptr - *list1;
	memmove(cpptr, cpptr + 1, (len1 - i) * sizeof(UChar *));
	cpptr--;
	len1--;
    }
  }

  return(r);
}

Int32
stringlist_num(UChar ** strings)
{
  Int32		n;

  if(!strings)
    return(0);

  for(n = 0; *strings; strings++, n++);

  return(n);
}

Int32
stringlist_totallen(UChar ** strings)
{
  Int32		n;

  if(!strings)
    return(0);

  for(n = 0; *strings; n += strlen(*(strings++)));

  return(n);
}

UChar *
stringlist_cat(UChar ** strings, Int32 n, UChar * sep)
{
  UChar		*catstr, *spc, *cptr;
  Int32		i, seplen, spclen;

  if(!sep)
    sep = "\t";
  seplen = strlen(sep);

  catstr = NEWP(UChar, stringlist_num(strings) * seplen
				+ stringlist_totallen(strings) + 1);
  if(!catstr)
    return(NULL);

  spc = "";
  spclen = 0;
  strcpy(catstr, spc);

  for(cptr = catstr, i = 0; *strings && (n <= 0 || i < n); strings++, i++){
    if(*strings){
	strcpy(cptr, spc);
	cptr += spclen;
	strcpy(cptr, *strings);
	cptr += strlen(*strings);
	spc = sep;
	spclen = seplen;
    }
  }

  return(catstr);
}

Int32
stringlist_contlines(UChar ** strings, UChar contchr)
{
  Int32		nl, i, l;
  UChar		*newstr;

  if(!contchr)
    contchr = '\\';

  nl = stringlist_num(strings);
  for(i = 0; i < nl; i++){
    l = strlen(strings[i]);

    if(strings[i][l - 1] == contchr){
      if(i == nl - 1){
	strings[i][l - 1] = '\0';
      }
      else{
	newstr = RENEWP(strings[i], UChar, l - 1 + strlen(strings[i + 1]) + 1);
	if(!newstr)
	  return(-1);

	strcpy(newstr + l - 1, strings[i + 1]);
	strings[i] = newstr;
	free(strings[i + 1]);
	memmove(strings + i + 1, strings + i + 2, sizeof(UChar *)
				* (nl - i - 1));
	i--;
	nl--;
      }
    }
  }

  return(0);
}

Int32
stringlist_uniq(UChar ** strlist, Flag sorted, Flag do_free)
{
  Int32	num, i, j;

  num = stringlist_num(strlist);

  for(i = 0; i < num - 1; i++){
    for(j = i + 1; j < (sorted ? MIN(num, i + 2) : num); j++){
	if(!strcmp(strlist[i], strlist[j])){
	  if(do_free)
	    free(strlist[j]);
	  memmove(strlist + j, strlist + j + 1, sizeof(UChar *) * (num - j));
	  j--;
	  num--;
	}
    }
  }

  return(0);
}


/* string chain functions */

UChar **
str_array_from_chain(UChar * chain, Int32 len)
{
  UChar		*cptr, *endptr, **r;
  Int32		n;

  if(!chain)
    return(NULL);

  endptr = chain + len;
  for(n = 0, cptr = chain; cptr < endptr; cptr += strlen(cptr) + 1, n++);

  r = NEWP(UChar *, n + 1);
  if(!r)
    return(NULL);

  memset(r, 0, sizeof(UChar *) * (n + 1));

  for(n = 0, cptr = chain; cptr < endptr; cptr += strlen(cptr) + 1, n++)
    if( !(r[n] = strdup(cptr)) )
	GETOUT;

  return(r);

 getout:
  if(r){
    stringlist_free(r, 0);
    free(r);
  }
  return(NULL);
}

Int32
str_chain_num(UChar * str, Int32 len)
{
  Int32		n = 0, l = 0;
  UChar		*cptr;

  if(str)
    for(cptr = str; cptr < str + len; cptr += strlen(cptr) + 1)
	n++;

  return(n);
}

Int32
str_chain_uniq(UChar * list, Int32 len)
{
  UChar		*cptr, *cptr2;
  Int32		l, l2;

  for(cptr = list; cptr < list + len; cptr += l){
    l = strlen(cptr) + 1;

    for(cptr2 = cptr + l; cptr2 < list + len; cptr2 += l2){
      l2 = strlen(cptr2) + 1;
      if(!strcmp(cptr, cptr2)){
	memmove(cptr2, cptr2 + l2, list + len - (cptr2 + l2));
	len -= l2;
	l2 = 0;
      }
    }
  }

  return(len);
}

UChar *
str_chain_from_array(UChar ** strarr, Int32 * nchars)
{
  UChar		*r, *cptr;

  r = NEWP(UChar, stringlist_num(strarr) + stringlist_totallen(strarr) + 1);
  if(!r)
    return(r);
  r[0] = '\0';
  if(!strarr)
    return(r);

  cptr = r;
  while(*strarr){
    strcpy(cptr, *(strarr++));
    cptr += strlen(cptr) + 1;
  }

  if(nchars)
    *nchars = cptr - r;

  return(r);
}
