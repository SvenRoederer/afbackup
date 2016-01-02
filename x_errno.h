/****************** Start of $RCSfile: x_errno.h,v $  ****************
*
* $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/x_errno.h,v $
* $Id: x_errno.h,v 1.2 2004/07/08 20:34:47 alb Exp alb $
* $Date: 2004/07/08 20:34:47 $
* $Author: alb $
*
*
******* description ***********************************************
*
*
*
*******************************************************************/

#ifndef __X_ERRNO_H
#define	__X_ERRNO_H

#include <errno.h>
#include "x_types.h"

typedef	struct	__x_error_pair	{
  Int32	err_num;
  UChar		*message;
}	ZnErrorPair;


/*
 * Error codes
 */

#define	NO_ERROR		0	/* no error occured		*/

/* general error messges */
#define	NULL_POINTER_PASSED	10000	/* no valid pointer passed	*/
#define	ILLEGAL_ARGUMENT	10001	/* argument to function illegal	*/
#define	ILLEGAL_VALUE		10002	/* no comment			*/
#define	INDEX_OUT_OF_RANGE	10003	/* dito				*/
#define	BAD_ARGUMENT		10004	/* somehow faulty		*/
#define	BAD_STRING_FORMAT	10005	/* string has a bad format	*/
#define	MULTIPLE_USE_ILLEGAL	10006	/* not to be used several times	*/
#define	NOT_IMPLEMENTED		10050	/* sorry			*/

/* file handling errors */
#define	NO_SUCH_FILE		10100	/* file doesn't exist		*/
#define	NO_ACCESS_TO_FILE	10101	/* file can't be accessed	*/
#define	FILE_FORMAT_ERROR	10120	/* file format wrong		*/
#define	UNKNOWN_KEYWORD		10121	/* keyword in file unknown	*/
#define	NO_VALID_DATABLOCK	10150	/* magic number is not correct	*/

/* authentication stuff */
#define	NO_GREETING_MESSAGE	10250	/* server did not send init str	*/

/* for the fuzzy tools */
#define	NO_VALID_FUZZY_POINTER	10300	/* wrong magic number		*/
#define	FUZZY_ADD_INF_TO_OBJ	10301	/* may not add f_system to s.th */
#define	INTERNAL_FUZZY_ERROR	10302	/* something strange happened	*/
#define	FUZZY_UNKNOWN_OPTION	10303	/* unknown key in create or set	*/
#define	FUZZY_UNKNOWN_COND_TYPE	10304	/* unknown condition type	*/
#define	FUZZY_UNKNOWN_CONC_TYPE	10305	/* unknown conclusion type	*/
#define	NO_VALID_RULE_PTR	10306	/* wrong magic number		*/
#define	NO_VALID_VARIABLE_PTR	10307	/* wrong_magic_number		*/
#define	NO_VALID_SYSTEM_PTR	10308
#define	NO_VALID_ATTRIB_PTR	10309
#define	UNKNOWN_CONCLUSION_MODE	10310
#define	UNKNOWN_GRAVITY_MODE	10311
#define	ILLEGAL_MS_FUNCTION	10312
#define	DATA_STRUCT_CORRUPTED	10313	/* chained list of objs corrupt */
#define	NO_VALID_PTR		10314
#define	UNKNOWN_FUNCTION_TYPE	10315
#define	UNKNOWN_TYPE_OF_RULE	10316
#define	RULE_NAME_EXISTS	10317
#define	ATTRIBUTE_NAME_EXISTS	10318
#define	VARIABLE_NAME_EXISTS	10319
#define	FOBJECT_NOT_FOUND	10320

/* for the backprop network */
#define	NO_VALID_BPNET		10400	/* no valid pointer to a bp-net	*/
#define	BPNET_ATTRIBUTE_ERROR_1	10401	/* attribute errors s. b.	*/
#define	BPNET_ATTRIBUTE_ERROR_2	10402
#define	BPNET_ATTRIBUTE_ERROR_3	10403
#define	BPNET_ATTRIBUTE_ERROR_4	10404
#define	BPNET_ATTRIBUTE_ERROR_5	10405
#define	BPNET_ATTRIBUTE_ERROR_6	10406
#define	BPNET_ATTRIBUTE_ERROR_7	10407
#define	BPNET_TOO_MANY_CI_TYPES	10408
#define	BPNET_TOO_MANY_TR_TYPES	10409
#define	BPNET_TOO_MANY_ER_TYPES	10410
#define	BPNET_ATTRIBUTE_ERROR_8	10411
#define	BPNET_RAY_SEARCH_IS_ERR	10412
#define	BPNET_TAP_SEARCH_IS_ERR	10413
#define	BPNET_RUD_SEARCH_IS_ERR	10414
#define	BPNET_RUD_SEARCH_PS_ERR	10415
#define	BPMIN_BAD_COMBINATION	10416
#define	BPMIN_BAD_CALL_SEQUENCE	10417
#define	BPMIN_NO_EVAL_FUNCTION	10418
#define	BPMIN_G_WITHOUT_F	10419
#define	BPMIN_NO_GRAD_EVAL_FUNC	10420
#define	BPMIN_SAME_F_AND_G	10421
#define	BPMIN_FG_AND_F_SUPPLIED	10422
#define	BPMIN_FG_AND_G_SUPPLIED	10423
#define	BPMIN_BAD_GRAD_METHOD	10424
#define	BPMIN_CANNOT_EVALUATE	10425
#define	BPMIN_SERIOUS_GRAD_ERR	10426
#define	BPMIN_SERIOUS_CONS_ERR	10427
#define	BPNET_BAD_ARRAYSIZE	10430
#define	BPNET_CONNECT_UNITS_ERR	10431
#define	BPNET_ADD_UNIT_TO_G_ERR	10432
#define	BPNET_GROUP_FROM_REGEX	10433
#define	BPNET_UNIT_FROM_REGEX	10434
#define	BPNET_NET_FROM_REGEX	10435
#define	BPNET_WRONG_ARRAY_SIZE	10436
#define	BPNET_NO_LINK_FROM_IDX	10437
#define	BPNET_NO_OBJECT_PASSED	10438
#define	BPNET_UNKNOWN_OPTION	10439
#define	BPNET_UNKNOWN_OBJECT	10440
#define	BPNET_PARAMETER_CHANGE	10441
#define	BPNET_ILLEGAL_OPTION	10442
#define	BPNET_CANNOT_PROCESS	10450	/* for some reason impossible	*/
#define	BPMIN_UNKNOWN_MIN_CODE	10451	/* minimize method unknown	*/
#define	BPMIN_UNKNOWN_DIR_CODE	10452	/* direction method unknown	*/
#define	BPMIN_UNKNOWN_STEP_CODE	10453	/* direction method unknown	*/
#define	BPMIN_UNKNOWN_IS_CODE	10454	/* direction method unknown	*/
#define	BPMIN_TOO_MANY_MIN_METH	10455	/* minimize method		*/
#define	BPNET_AT_LEAST_2_LAYERS	10460	/* too few layers specified	*/
#define	BPNET_ADD_NET_TO_OBJ	10461	/* dont't add net to anything	*/
#define	BPNET_AT_LEAST_1_NEURON	10462	/* too few neurons specified	*/
#define	BPMIN_NO_RESULT_CODE	10480	/* unknown minimize error	*/
#define	BPNET_ARG_HANDLING	10499	/* arg or component necessary	*/


/* for the kohonen tools */
#define	NO_VALID_KOHONEN_MAP	10500	/* no valid pointer to a map	*/
#define	INCONSISTENT_DIMENSIONS	10501	/* number of neurons incons.	*/
#define	DIMENSION_OVERFLOW	10502	/* index too high		*/
#define	NO_NEURON_FOUND		10503	/* within the given radius	*/
#define	KOHONEN_FILE_FMT	10504	/* wrong file format		*/

/* for the matrix calculus tools */
#define	NO_VALID_MATRIX		10700	/* no valid pointer to a matrix	*/
#define	NO_SQUARE_MATRIX	10701	/* matrix should be of square form */
#define	DIFFERENT_SIZES		10702	/* matrixes are not operatable	*/
#define	MULT_DIMENSION_ERROR	10703	/* matrix multiplication needs	*/
#define	DIMENSION_ERROR		10704	/* result and source don't fit	*/
#define	TRIA_DIMENSION_ERROR	10705	/* error performing trianglzton	*/
#define	MATRIX_NOT_INVERTABLE	10706	/* no comment			*/

/* for the parameter estimation */
#define	NO_VALID_IDENT_ID	10800	/* no valid pointer to ID-object */

/* for optimization routines */
#define	START_AREA_RESTRICTED	10900	/* procedure can't find out	*/
#define	DIM_ALIGNMENT_ERROR	10901	/* dimensions of aux vectors ...*/

/*	for signal device handling	*/
#define SDEVICE_NOT_VALID	11000	/* strange magic number */ 
#define SDEVICE_NOT_BUSY	11001	/* device busy */
#define SDEVICE_READ_ONLY	11002	/* device can do only data input */
#define SDEVICE_WRITE_ONLY	11003	/* device can do only data output */
#define SDEVICE_PARAMETER_ERROR	11004	/* device can not handle give parameters */
#define SDEVICE_UNSPECIFIED_ERROR	11005	/* unknown error */
 



/* error message list */
#define	X_ERROR_MSGS	{	\
	{ 10000,	"Null pointer passed, where not allowed" },		\
	{ 10001,	"Illegal argument to function" },			\
	{ 10002,	"Illegal value in argument" },				\
	{ 10003,	"Index out of range" },					\
	{ 10004,	"Bad argument" },					\
	{ 10005,	"Bad string format" },					\
	{ 10006,	"Argument may not be used more than one time" },	\
	{ 10050,	"Requested feature is not (yet) implemented, sorry" },	\
									\
	{ 10100,	"No such file" },					\
	{ 10101,	"File cannot be accessed" },				\
	{ 10120,	"File format error" },					\
	{ 10121,	"Unknown keyword, file format error" },			\
	{ 10150,	"Pointer to datablock invalid or not initialized" },	\
									\
	{ 10250,	"Server did not send protocol initialization string" },	\
									\
	{ 10300,	"No valid pointer to a fuzzy object passed" },		\
	{ 10301,	"System is root object and may not be added to s.th." },\
	{ 10302,	"Internal fuzzy error. Report this to developer" },	\
	{ 10303,	"Unknown option passed" },				\
	{ 10304,	"Unknown condition code passed" },			\
	{ 10305,	"Unknown conclusion code passed" },			\
	{ 10306,	"No valid pointer to an f_system rule passed" },	\
	{ 10307,	"No valid pointer to a fuzzy variable passed" },	\
	{ 10308,	"No valid pointer to an f_system passed" },		\
	{ 10309,	"No valid pointer to a fuzzy attribute passed" },	\
	{ 10310,	"Unknown conclusion mode" },				\
	{ 10311,	"Unknown gravity evaluation mode" },			\
	{ 10312,	"Membership function is illegal cause of numerical characteristics" },\
	{ 10313,	"Data structure corrupted. Probably pointer jam on heap" },\
	{ 10314,	"Invalid pointer passed" },				\
	{ 10315,	"Unknown membership function type" },			\
	{ 10316,	"Unknown type of rule" },				\
	{ 10317,	"Rule with that name already exists" },			\
	{ 10318,	"Attribute with that name already exists" },		\
	{ 10319,	"Variable with that name already exists" },		\
	{ 10320,	"Desired object not found" },				\
									\
	{ 10400,	"No valid pointer to a backprop net passed" },		\
	{ 10401,	"Non output group requested to gated" },		\
	{ 10402,	"Output group requested to be softmax, but not with crossentropy error measure" }, \
	{ 10403,	"Group of type RBF is not of type distance" },		\
	{ 10404,	"Group of type RBF is not of type negexp or radial" },	\
	{ 10405,	"No combin function allowed for this group" },		\
	{ 10406,	"No transfer function allowed for this group" },	\
	{ 10407,	"Error function for non-output group requested" },	\
	{ 10408,	"Only one combin type possible per group" },		\
	{ 10409,	"Only one transfer type possible per group" },		\
	{ 10410,	"Only one error type possible per group" },		\
	{ 10411,	"Group is softmax, but has no exp. activation function" }, \
	{ 10412,	"Ray's line search needs an initial step" }, \
	{ 10413,	"Tap's line search needs an initial step" }, \
	{ 10414,	"Rudi's line search needs an initial step" }, \
	{ 10415,	"Rudi's line search failed: previous step has width 0" }, \
	{ 10416,	"Gradient check: bad calling combination" }, \
	{ 10417,	"Gradient check: bad calling sequence, changes number of values" }, \
	{ 10418,	"No evaluation function supplied" },			\
	{ 10419,	"Gradient function supplied, but no error function" },	\
	{ 10420,	"No gradient evaluation function supplied" },		\
	{ 10421,	"Functions to evaluate error and gradient are the same" }, \
	{ 10422,	"FG-evaluation and F-evaluation function supplied" },	\
	{ 10423,	"FG-evaluation and G-evaluation function supplied" },	\
	{ 10424,	"Check Gradient: bad method selected" },		\
	{ 10425,	"Cannot evaluate function(s)" },			\
	{ 10426,	"Gradient error occurred" },				\
	{ 10427,	"Serious consistency error occurred" },			\
	{ 10430,	"Bad size of variable array in net" },			\
	{ 10431,	"Connecting units failed, probably one unit invalid" }, \
	{ 10432,	"Adding unit to group failed" },			\
	{ 10433,	"Getting group from regex failed" },			\
	{ 10434,	"Getting unit from regex failed" },			\
	{ 10435,	"Getting net from regex failed" },			\
	{ 10436,	"Wrong size of link array" },				\
	{ 10437,	"Cannot get link from variable index" },		\
	{ 10438,	"No valid object passed" },				\
	{ 10439,	"Unknown option" },					\
	{ 10440,	"Unknown bp-network related object" },			\
	{ 10441,	"Parameter must not be changed now" },			\
	{ 10442,	"Parameter not allowed in this context" },		\
	{ 10450,	"Cannot calculate net error. Try to debug" },		\
	{ 10451,	"Unknown code for minimize algorithm passed" },		\
	{ 10452,	"Unknown code for minimize direction passed" },		\
	{ 10453,	"Unknown code for step method passed" },		\
	{ 10454,	"Unknown code for initial step passed" },		\
	{ 10455,	"Too many minimize methods specified" },		\
	{ 10460,	"Less than 2 layers specified" },			\
	{ 10461,	"Net may not be added to anything" },			\
	{ 10462,	"Less than 1 neuron specified for layer" },		\
	{ 10480,	"Unknown error occurred while minimizing" },		\
	{ 10499,	"Argument or struct component required, but not passed" }, \
										\
	{ 10500,	"No valid pointer to a kohonen map passed" },		\
	{ 10501,	"Product of dimension widths != number of neurons" },	\
	{ 10502,	"Passed index out of range: too high or < 0" },		\
	{ 10503,	"No neuron found within the maximum radius" },		\
	{ 10504,	"Error reading file containing kohonen feature map" },	\
										\
	{ 10700,	"No valid matrix pointer passed" },			\
	{ 10701,	"Matrix has unequal number of rows and columns" },	\
	{ 10702,	"Arguments have different numbers of rows and/or columns" }, \
	{ 10703,	"Arguments for multiplication have incompatible sizes" },\
	{ 10704,	"Argument and result have incompatible dimensions" },	\
	{ 10705,	"Index is out of range" },				\
	{ 10706,	"Illegal dimensions for triangularization" },		\
	{ 10707, "Matrix is not invertable" },					\
										\
	{ 10800,	"No valid pointer to a identification object passed" },	\
										\
	{ 10900,	"Area around start point restricted" },			\
	{ 10901,	"Illegal dimensions of state or target vectors" },	\
										\
	{ 11000,	"No valid signal device pointer passed" },		\
	{ 11101,	"Signal device is busy" },	\
	{ 11102,	"Signal device is read only" }, \
	{ 11103,	"Signal Device is write only" },\
	{ 11104,	"Signal Device can't handle given parameters" },	\
	{ 11105,	"Unspecified signal device error" }			\
}


#ifdef	__cplusplus
extern "C" {
#endif

extern	UChar	*x_strerror(Int32);
extern	void	x_perror(UChar *);

#ifdef __cplusplus
}
#endif

#endif	/* __X_ERRNO_H */
