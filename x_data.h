/****************** Start of $RCSfile: x_data.h,v $  ****************
*
* $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/x_data.h,v $
* $Id: x_data.h,v 1.2 2004/07/08 20:34:47 alb Exp alb $
* $Date: 2004/07/08 20:34:47 $
* $Author: alb $
*
*
******* description ***********************************************
*
*
*
*******************************************************************/

#ifndef	_X_DATA_H
#define	_X_DATA_H	_X_DATA_H

#include <stdio.h>
#include <x_types.h>

typedef	struct	__x_datablock {
  Real64	**data;
  Uns32	num_rows;
  Uns32	num_cols;
  UChar		**ds_names;
  UChar		*title;
  UChar		*creator;
  UChar		*creadate;
  UChar		*comment;

/* PRIVATE */
  Int32	magic_number;
}	X_DataBlock;


#define	X_DBL_MAGIC_NUMBER	0x14238bf5

#define	DATA_BLOCK_INIT		{ NULL, 0, 0, NULL, NULL, NULL, NULL,	\
				  NULL, X_DBL_MAGIC_NUMBER }

#define	X_MAXLINELENGTH	20000

#define	ASCII		0x0001
#define	BIN		0x0002
#define	NO_HEADER	0x0100
#define	ONLY_DS_NAMES	0x0200

#define	DEF_FMT		"%13.6lg"	/* numeric default format */


#ifdef	__cplusplus
extern	"C"	{
#endif

extern	Int32	alloc_datablock(X_DataBlock *, Int32, Int32);
extern	Int32	realloc_datablock(X_DataBlock *, Int32, Int32);
extern	Int32	init_datablock(X_DataBlock *);
extern	Int32	write_datablock(FILE *, X_DataBlock *, Int32, UChar *);
extern	Int32	read_datablock(FILE *, X_DataBlock *, Int32);
extern	void	free_datablock(X_DataBlock *);
extern	Int32	append_datablock(FILE *, X_DataBlock *, Int32);
extern	Int32	trsp_datablock(X_DataBlock *, X_DataBlock *);
extern	Int32	copy_datablock(X_DataBlock *, X_DataBlock *);
extern	Int32	dbl_set_title(X_DataBlock *, UChar *);
extern	Int32	dbl_set_creadate(X_DataBlock *, UChar *);
extern	Int32	dbl_set_creator(X_DataBlock *, UChar *);
extern	Int32	dbl_set_comment(X_DataBlock *, UChar *);
extern	Int32	dbl_set_ds_name(X_DataBlock *, Int32, UChar *);
extern	Int32	dbl_set_default_ds_names(X_DataBlock *);

#ifdef	__cplusplus
}
#endif


/* file format:

[Title: <titletext>]
[Creator: <creator>]
[CreationDate: <date>]
[Comment: <comment>]
[NumberOfDatasets: <num...>]
[DataFormat: <format>]
[[DatasetNames:] <name1> <name2> ...]
data

*/

#endif	/* ! _X_DATA_H */

/************ end of $RCSfile: x_data.h,v $ ******************/
