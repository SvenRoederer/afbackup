/****************** Start of $RCSfile: __z.c,v $  ****************
*
* $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/__z.c,v $
* $Id: __z.c,v 1.3 2011/12/14 20:27:26 alb Exp alb $
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

  static char * fileversion = "$RCSfile: __z.c,v $ $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/__z.c,v $ $Id: __z.c,v 1.3 2011/12/14 20:27:26 alb Exp alb $ " PACKAGE " " VERSION_STRING;

#include <stdio.h>
#include <unistd.h>
#include <x_types.h>
#include <genutils.h>
#include <fileutil.h>
#include <afbackup.h>
#include <zutils.h>


void
usage(UChar * prognam)
{
  fprintf(stderr, "Usage: %s -{123456789|d}\n", FN_BASENAME(prognam));

  exit(2);
}


main(int argc, char ** argv)
{
  ZFile		zfile = ZFILE_INIT;
  UChar		buf[8192];
  Int32		i, n, level = 0;
  Flag		level1 = NO;
  Flag		level2 = NO;
  Flag		level3 = NO;
  Flag		level4 = NO;
  Flag		level5 = NO;
  Flag		level6 = NO;
  Flag		level7 = NO;
  Flag		level8 = NO;
  Flag		level9 = NO;
  Flag		uncompress = NO;

  i = goptions(-argc, (UChar **) argv,
		"b:1;b:2;b:3;b:4;b:5;b:6;b:7;b:8;b:9;b:d",
		&level1, &level2, &level3, &level4, &level5, &level6,
		&level7, &level8, &level9, &uncompress);
  if(i)
    usage(argv[0]);

  if(level1)
    level = 1;
  if(level2)
    level = 2;
  if(level3)
    level = 3;
  if(level4)
    level = 4;
  if(level5)
    level = 5;
  if(level6)
    level = 6;
  if(level7)
    level = 7;
  if(level8)
    level = 8;
  if(level9)
    level = 9;
  if(level != 0 && uncompress)
    fprintf(stderr,
	"Warning: Level specification %d ignored for uncompression.\n",
				(int) level);
  if(level == 0)
    level = 5;		/* set default */

  reset_zfile(&zfile);

  if(uncompress)
    i = open_file_unzip(&zfile, 1);
  else
    i = open_file_zip(&zfile, 0, level, 0);

  if(i){
    fprintf(stderr, "Error: Initializing %scompression failed, exiting.\n",
		(uncompress ? "un" : ""));
    exit(3);
  }

  forever{
    if(uncompress){
	i = read_forced(0, buf, 8192);
	if(i < 0){
	  fprintf(stderr, "Error: Reading stdin failed: %s\n",
			strerror(errno));
	  exit(5);
	}
	if(i == 0){
	  i = write_file_unzip(&zfile, buf, 0, YES, write_forced);
	  if(i < 0){
	    fprintf(stderr, "Error: Uncompressing data failed: %s\n",
				zfile.z_stream.msg);
	    exit(6);
	  }

	  break;
	}

	n = write_file_unzip(&zfile, buf, i, NO, write_forced);
	if(n < i){
	  fprintf(stderr, "Error: Uncompressing data failed: %s\n",
				zfile.z_stream.msg);
	  exit(8);
	}
    }
    else{
	i = read_file_zip(&zfile, buf, 8192, read_forced);
	if(i < 0){
	  fprintf(stderr, "Error: Compressing stdin failed: %s\n",
				strerror(errno));
	  exit(7);
	}
	if(i == 0)
	  break;

	n = write_forced(1, buf, i);
	if(n < i){
	  fprintf(stderr, "Error: Writing stdout failed: %s\n",
			strerror(errno));
	  exit(9);
	}
    }
  }

  exit(0);
}
