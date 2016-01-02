/****************** Start of $RCSfile: __numset.c,v $  ****************
*
* $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/__numset.c,v $
* $Id: __numset.c,v 1.3 2004/07/08 20:34:42 alb Exp alb $
* $Date: 2004/07/08 20:34:42 $
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

  static char * fileversion = "$RCSfile: __numset.c,v $ $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/__numset.c,v $ $Id: __numset.c,v 1.3 2004/07/08 20:34:42 alb Exp alb $ " PACKAGE " " VERSION_STRING;

#include <stdio.h>
#include <genutils.h>
#include <fileutil.h>

void
usage(UChar * pnam)
{
  fprintf(stderr, "Usage: %s <set> {+|-|in} <set_to_add_or_remove>\n"
			"      %s # <set>\n",
	FN_BASENAME(pnam), FN_BASENAME(pnam));
  exit(2);
}

main(int argc, char ** argv)
{
  Int32		i, j, n;
  Flag		calc_num;
  Uns32Range	*ranges_opnd, *ranges_opor;
  Uns32Range	*result;	/* uninitialized OK */

  calc_num = (argc == 3 && !strcmp(argv[1], "#"));

  if(argc != 4 && !calc_num)
    usage(argv[0]);

  if(strcmp(argv[2], "-") && strcmp(argv[2], "+") && strcasecmp(argv[2], "in")
		&& strcmp(argv[1], "#"))
    usage(argv[0]);

  ranges_opnd = sscan_Uns32Ranges__(argv[calc_num ? 2 : 1], 1, 1, NULL, NULL);
  if(!calc_num)
    ranges_opor = sscan_Uns32Ranges__(argv[3], 1, 1, NULL, NULL);

  if(!ranges_opnd || (!ranges_opor && !calc_num)){
    fprintf(stderr, "Error: Cannot read argument 1 or 3 as set of numbers.\n");
    exit(1);
  }

  pack_Uns32Ranges(ranges_opnd, NULL);
  if(calc_num){
    fprintf(stdout, "%d\n", (int) num_Uns32Ranges(ranges_opnd));
    exit(0);
  }

  pack_Uns32Ranges(ranges_opor, NULL);

  if(argv[2][0] == '+'){
    i = merge_Uns32Ranges(&ranges_opnd, ranges_opor);
    if(i){
	fprintf(stderr, "Error: Merging sets failed: %s.\n",
			strerror(errno));
	exit(1);
    }
    result = ranges_opnd;
  }
  if(argv[2][0] == '-'){
    result = del_range_from_Uns32Ranges(ranges_opnd, ranges_opor);
    if(!result){
	fprintf(stderr, "Error: Removing numbers from sets failed: %s.\n",
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
