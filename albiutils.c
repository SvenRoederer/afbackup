#include "conf.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <albiapps.h>
#include <fileutil.h>

main(int argc, char ** argv)
{
  UChar	*programname;

  programname = FN_BASENAME((UChar *) argv[0]);

  albi_main(argc, argv);

  fprintf(stderr, T_("Error: Unrecognized command: `%s'\n"), programname);
  exit(2);
}
