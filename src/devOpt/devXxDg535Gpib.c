/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* devXxDg535Gpib.c */
/* share/src/devOpt $Id$ */
/*
 *      Author: John Winans
 *      Date:   11-19-91
 */

#define	DSET_AI		devAiDg535Gpib
#define	DSET_AO		devAoDg535Gpib
#define	DSET_LI		devLiDg535Gpib
#define	DSET_LO		devLoDg535Gpib
#define	DSET_BI		devBiDg535Gpib
#define	DSET_BO		devBoDg535Gpib
#define	DSET_MBBO	devMbboDg535Gpib
#define	DSET_MBBI	devMbbiDg535Gpib
#define	DSET_SI		devSiDg535Gpib
#define	DSET_SO		devSoDg535Gpib

#include	<vxWorks.h>
#include	<stdlib.h>
#include	<stdio.h>
#include	<string.h>
#include	<taskLib.h>
#include	<rngLib.h>

#include	<alarm.h>
#include	<cvtTable.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<devSup.h>
#include	<recSup.h>
#include	<drvSup.h>
#include	<link.h>
#include	<module_types.h>
#include	<dbCommon.h>
#include	<aiRecord.h>
#include	<aoRecord.h>
#include	<biRecord.h>
#include	<boRecord.h>
#include	<mbbiRecord.h>
#include	<mbboRecord.h>
#include	<stringinRecord.h>
#include	<stringoutRecord.h>
#include	<longinRecord.h>
#include	<longoutRecord.h>

#include	<drvGpibInterface.h>
#include	<devCommonGpib.h>

#define STATIC static

STATIC long	init_dev_sup(), report();
STATIC 	struct  devGpibParmBlock devSupParms;

/* forward declarations of some custom convert routines */
STATIC int setDelay();
STATIC int rdDelay();


/******************************************************************************
 *
 * Define all the dset's.
 *
 * Note that the dset names are provided via the #define lines at the top of
 * this file.
 *
 * Other than for the debugging flag(s), these DSETs are the only items that
 * will appear in the global name space within the IOC.
 *
 * The last 3 items in the DSET structure are used to point to the parm 
 * structure, the  work functions used for each record type, and the srq 
 * handler for each record type.
 *
 ******************************************************************************/
gDset DSET_AI   = {6, {report, init_dev_sup, devGpibLib_initAi, NULL, 
	devGpibLib_readAi, NULL, (DRVSUPFUN)&devSupParms,
	(DRVSUPFUN)devGpibLib_aiGpibWork, (DRVSUPFUN)devGpibLib_aiGpibSrq}};

gDset DSET_AO   = {6, {NULL, NULL, devGpibLib_initAo, NULL, 
	devGpibLib_writeAo, NULL, (DRVSUPFUN)&devSupParms,
	(DRVSUPFUN)devGpibLib_aoGpibWork, NULL}};

gDset DSET_BI   = {5, {NULL, NULL, devGpibLib_initBi, NULL, 
	devGpibLib_readBi, (DRVSUPFUN)&devSupParms,
	(DRVSUPFUN)devGpibLib_biGpibWork, (DRVSUPFUN)devGpibLib_biGpibSrq}};

gDset DSET_BO   = {5, {NULL, NULL, devGpibLib_initBo, NULL, 
	devGpibLib_writeBo, (DRVSUPFUN)&devSupParms,
	(DRVSUPFUN)devGpibLib_boGpibWork, NULL}};

gDset DSET_MBBI = {5, {NULL, NULL, devGpibLib_initMbbi, NULL, 
	devGpibLib_readMbbi, (DRVSUPFUN)&devSupParms,
	(DRVSUPFUN)devGpibLib_mbbiGpibWork, (DRVSUPFUN)devGpibLib_mbbiGpibSrq}};

gDset DSET_MBBO = {5, {NULL, NULL, devGpibLib_initMbbo, NULL, 
	devGpibLib_writeMbbo, (DRVSUPFUN)&devSupParms,
	(DRVSUPFUN)devGpibLib_mbboGpibWork, NULL}};

gDset DSET_SI   = {5, {NULL, NULL, devGpibLib_initSi, NULL, 
	devGpibLib_readSi, (DRVSUPFUN)&devSupParms,
	(DRVSUPFUN)&devGpibLib_stringinGpibWork, (DRVSUPFUN)devGpibLib_stringinGpibSrq}};

gDset DSET_SO   = {5, {NULL, NULL, devGpibLib_initSo, NULL, 
	devGpibLib_writeSo, (DRVSUPFUN)&devSupParms, 
	(DRVSUPFUN)devGpibLib_stringoutGpibWork, NULL}};

gDset DSET_LI   = {5, {NULL, NULL, devGpibLib_initLi, NULL, 
	devGpibLib_readLi, (DRVSUPFUN)&devSupParms, 
	(DRVSUPFUN)devGpibLib_liGpibWork, (DRVSUPFUN)devGpibLib_liGpibSrq}};

gDset DSET_LO   = {5, {NULL, NULL, devGpibLib_initLo, NULL, 
	devGpibLib_writeLo, (DRVSUPFUN)&devSupParms, 
	(DRVSUPFUN)devGpibLib_loGpibWork, NULL}};

int Dg535Debug = 0;		/* debugging flags */
extern int ibSrqDebug;

/*
 * Use the TIME_WINDOW defn to indicate how long commands should be ignored
 * for a given device after it times out.  The ignored commands will be
 * returned as errors to device support.
 *
 * Use the DMA_TIME to define how long you wish to wait for an I/O operation
 * to complete once started.
 */
#define TIME_WINDOW	600		/* 10 seconds on a getTick call */
#define	DMA_TIME	20		/* 1/3 second on a watchdog time */

/*
 * Strings used by the init routines to fill in the znam, onam, ...
 * fields in BI, BO, MBBI, and MBBO record types.
 */

static	char		*offOnList[] = { "Off", "On" };
static	struct	devGpibNames	offOn = { 2, offOnList, NULL, 1 };

static	char	*disableEnableList[] = { "Disable", "Enable" };
static	struct	devGpibNames	disableEnable = { 2, disableEnableList, NULL, 1 };

static	char	*resetList[] = { "Reset", "Reset" };
static	struct	devGpibNames	reset = { 2, resetList, NULL, 1 };

static	char	*lozHizList[] = { "50 OHM", "IB_Q_HIGH Z" };
static	struct	devGpibNames	lozHiz = {2, lozHizList, NULL, 1};

static	char	*invertNormList[] = { "INVERT", "NORM" };
static	struct	devGpibNames	invertNorm = { 2, invertNormList, NULL, 1 };

static	char	*fallingRisingList[] = { "FALLING", "RISING" };
static	struct	devGpibNames fallingRising = { 2, fallingRisingList, NULL, 1 };

static	char	*singleShotList[] = { "SINGLE", "SHOT" };
static	struct	devGpibNames	singleShot = { 2, singleShotList, NULL, 1 };

static	char	*clearList[] = { "CLEAR", "CLEAR" };
static	struct	devGpibNames	clear = { 2, clearList, NULL, 1 };

static	char		*tABCDList[] = { "T", "A", "B", "C", "D" };
static	unsigned long	tABCDVal[] = { 1, 2, 3, 5, 6 };
static	struct	devGpibNames	tABCD = { 5, tABCDList, tABCDVal, 3 };

static	char		*ttlNimEclVarList[] = { "TTL", "NIM", "ECL", "VAR" };
static	unsigned long	ttlNimEclVarVal[] = { 0, 1, 2, 3 };
static	struct	devGpibNames	ttlNimEclVar = { 4, ttlNimEclVarList, 
					ttlNimEclVarVal, 2 };

static	char		*intExtSsBmStopList[] = { "INTERNAL", "EXTERNAL", 
					"SINGLE SHOT", "BURST MODE", "STOP" };
static	unsigned long	intExtSsBmStopVal[] = { 0, 1, 2, 3, 2 };
static	struct	devGpibNames	intExtSsBm = { 4, intExtSsBmStopList, 
					intExtSsBmStopVal, 2 };
static	struct	devGpibNames	intExtSsBmStop = { 5, intExtSsBmStopList, 
					intExtSsBmStopVal, 3 };

/* Channel Names, used to derive string representation of programmed delay */

char  *pchanName[8] = {" ", "T + ", "A + ", "B  + ", " ", "C + ", "D + ", " "};

/******************************************************************************
 *
 * String arrays for EFAST operations.  Note that the last entry must be 
 * NULL.
 *
 * On input operations, only as many bytes as are found in the string array
 * elements are compared.  If there are more bytes than that in the input
 * message, they are ignored.  The first matching string found (starting
 * from the 0'th element) will be used as a match.
 *
 * NOTE: For the input operations, the strings are compared literally!  This
 * can cause problems if the instrument is returning things like \r and \n
 * characters.  You must take care when defining input strings so you include
 * them as well.
 *
 ******************************************************************************/

static char	*(userOffOn[]) = {"USER OFF;", "USER ON;", NULL};

/******************************************************************************
 *
 * Array of structures that define all GPIB messages
 * supported for this type of instrument.
 *
 ******************************************************************************/


static struct gpibCmd gpibCmds[] = 
{
  /* Param 0, (model)   */
  FILL,

  /* Channel A Delay and Output */
  /* Param 1, write A delay  */
  {&DSET_AO, GPIBWRITE, IB_Q_HIGH, "DT 2\n", "DT 2,?,%.12lf\n", 0, 32, 
  setDelay, 0, 0, NULL, NULL, -1},

  /* Param 2, currently undefined */
  FILL,

  /* Param 3, read A Delay */
  {&DSET_AI, GPIBREAD, IB_Q_LOW, "DT 2\n", NULL, 0, 32, 
  rdDelay, 0, 0, NULL, NULL, -1 },

  /* Param 4, read A delay reference channel and delay as string */
  {&DSET_SI, GPIBREAD, IB_Q_LOW, "DT 2\n", NULL, 0, 32, 
  rdDelay, 0, 0, NULL, NULL, -1 },

  /* Param 5, set A delay reference channel */
  {&DSET_MBBO, GPIBWRITE, IB_Q_HIGH, "DT 2\n", "DT 2,%u,", 0, 32, 
  setDelay, 0, 0, NULL, &tABCD, -1},

  /* Param 6, read A delay reference */
  {&DSET_MBBI, GPIBREAD, IB_Q_LOW, "DT 2\n", NULL, 0, 32, 
  rdDelay, 0, 0, NULL, &tABCD, -1},

  /* Param 7, set A output mode */
  {&DSET_MBBO, GPIBWRITE, IB_Q_HIGH, NULL, "OM 2,%u\n", 0, 32, 
  NULL, 0, 0, NULL, &ttlNimEclVar, -1}, 
  
  /* Param 8, read A output mode */
  {&DSET_MBBI, GPIBREAD, IB_Q_LOW, "OM 2\n", "%lu", 0, 32, 
  NULL, 0, 0, NULL, &ttlNimEclVar, -1},

  /* Param 9, set A output amplitude */
  {&DSET_AO, GPIBWRITE, IB_Q_HIGH, NULL, "OA 2,%.1f\n", 0, 32, 
  NULL, 0, 0, NULL, NULL, -1},

  /* Param 10, read A output amplitude */
  {&DSET_AI, GPIBREAD, IB_Q_LOW, "OA 2\n", "%lf", 0, 32, 
  NULL, 0, 0, NULL, NULL, -1},

  /* Param 11, set A output offset */
  {&DSET_AO, GPIBWRITE, IB_Q_HIGH, NULL, "OO 2,%.1f\n", 0, 32, 
  NULL, 0, 0, NULL, NULL, -1},

  /* Param 12, read A output offset */
  {&DSET_AI, GPIBREAD, IB_Q_LOW, "OO 2\n", "%lf", 0, 32, 
  NULL, 0, 0, NULL, NULL, -1},

  /* Param 13, set A output Termination */
  {&DSET_BO, GPIBWRITE, IB_Q_HIGH, NULL, "TZ 2,%u\n", 0, 32, 
  NULL, 0, 0, NULL, &lozHiz, -1},

  /* Param 14, read A output Termination */
  {&DSET_BI, GPIBREAD, IB_Q_LOW, "TZ 2\n", "%lu", 0, 32, 
  NULL, 0, 0, NULL, &lozHiz, -1},

  /* Param 15, set A output Polarity */
  {&DSET_BO, GPIBWRITE, IB_Q_HIGH, NULL, "OP 2,%u\n", 0, 32, 
  NULL, 0, 0, NULL, &invertNorm, -1},

  /* Param 16, read A output Polarity */
  {&DSET_BI, GPIBREAD, IB_Q_LOW, "OP 2\n", "%lu", 0, 32, 
  NULL, 0, 0, NULL, NULL, -1},

/* Channel B Delay and Output */
  /* Param 17, write B delay  */
  {&DSET_AO, GPIBWRITE, IB_Q_HIGH, "DT 3\n", "DT 3,?,%.12lf\n", 0, 32, 
  setDelay, 0, 0, NULL, NULL, -1},

  /* Param 18, currently undefined */
  FILL,

  /* Param 19, read B Delay */
  {&DSET_AI, GPIBREAD, IB_Q_LOW, "DT 3\n", NULL, 0, 32, 
  rdDelay, 0, 0, NULL, NULL, -1},

  /* Param 20, read B delay reference channel and delay as string */
  {&DSET_SI, GPIBREAD, IB_Q_LOW, "DT 3\n", NULL, 0, 32, 
  rdDelay, 0, 0, NULL, NULL, -1 },

  /* Param 21, set B delay reference channel */
  {&DSET_MBBO, GPIBWRITE, IB_Q_HIGH, "DT 3\n", "DT 3,%u,", 0, 32, 
  setDelay, 0, 0, NULL, &tABCD, -1},

  /* Param 22, read B delay reference */
  {&DSET_MBBI, GPIBREAD, IB_Q_LOW, "DT 3\n", NULL, 0, 32, 
  rdDelay, 0 ,0, NULL, &tABCD, -1},

  /* Param 23, set B output mode */
  {&DSET_MBBO, GPIBWRITE, IB_Q_HIGH, NULL, "OM 3,%u\n", 0, 32, 
  NULL, 0, 0, NULL, &ttlNimEclVar, -1},

  /* Param 24, read B output mode */
  {&DSET_MBBI, GPIBREAD, IB_Q_LOW, "OM 3\n", "%lu", 0 ,32, 
  NULL, 0, 0, NULL, &ttlNimEclVar, -1},

  /* Param 25, set B output amplitude */
  {&DSET_AO, GPIBWRITE, IB_Q_HIGH, NULL, "OA 3,%.1f\n", 0, 32, 
  NULL, 0, 0, NULL, NULL, -1},

  /* Param 26, read B output amplitude */
  {&DSET_AI, GPIBREAD, IB_Q_LOW, "OA 3\n", "%lf", 0, 32, 
  NULL, 0, 0, NULL, NULL, -1},

  /* Param 27, set B output offset */
  {&DSET_AO, GPIBWRITE, IB_Q_HIGH, NULL, "OO 3,%.1f\n", 0, 32, 
  NULL, 0, 0, NULL, NULL, -1},

  /* Param 28, read B output offset */
  {&DSET_AI, GPIBREAD, IB_Q_LOW, "OO 3\n", "%lf", 0, 32, 
  NULL, 0, 0, NULL, NULL, -1},

  /* Param 29, set B output Termination */
  {&DSET_BO, GPIBWRITE, IB_Q_HIGH, NULL, "TZ 3,%u\n", 0, 32, 
  NULL, 0, 0, NULL, &lozHiz, -1},

  /* Param 30, read B output Termination */
  {&DSET_BI, GPIBREAD, IB_Q_LOW, "TZ 3\n", "%lu", 0, 32,
  NULL, 0, 0, NULL, &lozHiz, -1},

  /* Param 31, set B output Polarity */
  {&DSET_BO, GPIBWRITE, IB_Q_HIGH, NULL, "OP 3,%u\n", 0, 32,
  NULL, 0, 0, NULL, &invertNorm, -1},

  /* Param 32, read B output Polarity */
  {&DSET_BI, GPIBREAD, IB_Q_LOW, "OP 3\n", "%lu", 0, 32,
  NULL, 0, 0, NULL, NULL, -1},

/* Channel AB Outputs */
  /* Param 33, set AB output mode */
  {&DSET_MBBO, GPIBWRITE, IB_Q_HIGH, NULL, "OM 4,%u\n", 0, 32,
  NULL, 0, 0, NULL, &ttlNimEclVar, -1},

  /* Param 34, read AB output mode */
  {&DSET_MBBI, GPIBREAD, IB_Q_LOW, "OM 4\n", "%lu", 0, 32,
  NULL, 0, 0, NULL, &ttlNimEclVar, -1},

  /* Param 35, set AB output amplitude */
  {&DSET_AO, GPIBWRITE, IB_Q_HIGH, NULL, "OA 4,%.1f\n", 0, 32,
  NULL, 0, 0, NULL, NULL, -1},

  /* Param 36, read AB output amplitude */
  {&DSET_AI, GPIBREAD, IB_Q_LOW, "OA 4\n", "%lf", 0, 32,
  NULL, 0, 0, NULL, NULL, -1},

  /* Param 37, set AB output offset */
  {&DSET_AO, GPIBWRITE, IB_Q_HIGH, NULL, "OO 4,%.1f\n", 0, 32,
  NULL, 0, 0, NULL, NULL, -1},

  /* Param 38, read AB output offset */
  {&DSET_AI, GPIBREAD, IB_Q_LOW, "OO 4\n", "%lf", 0, 32,
  NULL, 0, 0, NULL, NULL, -1},

  /* Param 39, set AB output Termination */
  {&DSET_BO, GPIBWRITE, IB_Q_HIGH, NULL, "TZ 4,%u\n", 0, 32,
  NULL, 0, 0, NULL, &lozHiz, -1},

  /* Param 40, read AB output Termination */
  {&DSET_BI, GPIBREAD, IB_Q_LOW, "TZ 4\n", "%lu", 0, 32,
  NULL, 0, 0, NULL, &lozHiz, -1},

  /* Param 41, set AB output Polarity */
  {&DSET_BO, GPIBWRITE, IB_Q_HIGH, NULL, "OP 4,%u\n", 0, 32,
  NULL, 0, 0, NULL, &invertNorm, -1},

  /* Param 42, read AB output Polarity */
  {&DSET_BI, GPIBREAD, IB_Q_LOW, "OP 4\n", "%lu", 0, 32,
  NULL, 0, 0, NULL, NULL, -1},

/* Channel C Delay and Output */
  /* Param 43, write C delay  */
  {&DSET_AO, GPIBWRITE, IB_Q_HIGH, "DT 5\n", "DT 5,?,%.12lf\n", 0, 32,
  setDelay, 0, 0, NULL, NULL, -1},

  /* Param 44, currently undefined */
  FILL,

  /* Param 45, read C Delay */
  {&DSET_AI, GPIBREAD, IB_Q_LOW, "DT 5\n", NULL, 0, 32,
  rdDelay, 0, 0, NULL, NULL, -1},

  /* Param 46, read C delay reference channel and delay as string */
  {&DSET_SI, GPIBREAD, IB_Q_LOW, "DT 5\n", NULL, 0, 32, 
  rdDelay, 0, 0, NULL, NULL, -1 },

  /* Param 47, set C delay reference channel */
  {&DSET_MBBO, GPIBWRITE, IB_Q_HIGH, "DT 5\n", "DT 5,%u,", 0, 32,
  setDelay, 0, 0, NULL, &tABCD, -1},

  /* Param 48, read C delay reference */
  {&DSET_MBBI, GPIBREAD, IB_Q_LOW, "DT 5\n", NULL, 0, 32,
  rdDelay, 0, 0, NULL, &tABCD, -1},

  /* Param 49, set C output mode */
  {&DSET_MBBO, GPIBWRITE, IB_Q_HIGH, NULL, "OM 5,%u\n", 0, 32,
  NULL, 0, 0, NULL, &ttlNimEclVar, -1},

  /* Param 50, read C output mode */
  {&DSET_MBBI, GPIBREAD, IB_Q_LOW, "OM 5\n", "%lu", 0, 32,
  NULL, 0, 0, NULL, &ttlNimEclVar, -1},

  /* Param 51, set C output amplitude */
  {&DSET_AO, GPIBWRITE, IB_Q_HIGH, NULL, "OA 5,%.1f\n", 0, 32,
  NULL, 0, 0, NULL, NULL, -1},

  /* Param 52, read C output amplitude */
  {&DSET_AI, GPIBREAD, IB_Q_LOW, "OA 5\n", "%lf", 0, 32,
  NULL, 0, 0, NULL, NULL, -1},

  /* Param 53, set C output offset */
  {&DSET_AI, GPIBWRITE, IB_Q_HIGH, NULL, "OO 5,%.1f\n", 0, 32,
  NULL, 0, 0, NULL, NULL, -1},

  /* Param 54, read C output offset */
  {&DSET_AI, GPIBREAD, IB_Q_LOW, "OO 5\n", "%lf", 0, 32,
  NULL, 0, 0, NULL, NULL, -1},

  /* Param 55, set C output Termination */
  {&DSET_BO, GPIBWRITE, IB_Q_HIGH, NULL, "TZ 5,%u\n", 0, 32,
  NULL, 0, 0, NULL, &lozHiz, -1},

  /* Param 56, read C utput Termination */
  {&DSET_BI, GPIBREAD, IB_Q_LOW, "TZ 5\n", "%lu", 0, 32,
  NULL, 0, 0, NULL, &lozHiz, -1},

  /* Param 57, set C output Polarity */
  {&DSET_BO, GPIBWRITE, IB_Q_HIGH, NULL, "OP 5,%u\n", 0, 32,
  NULL, 0, 0, NULL, &invertNorm, -1},

  /* Param 58, read C output Polarity */
  {&DSET_BI, GPIBREAD, IB_Q_LOW, "OP 5\n", "%lu", 0, 32,
  NULL, 0, 0, NULL, NULL, -1},

/* Channel D Delay and Output */
  /* Param 59, write D delay  */
  {&DSET_AO, GPIBWRITE, IB_Q_HIGH, "DT 6\n", "DT 6,?,%.12lf\n", 0, 32,
  setDelay, 0, 0, NULL, NULL, -1},

  /* Param 60, currently undefined */
  FILL,

  /* Param 61, read D Delay */
  {&DSET_AI, GPIBREAD, IB_Q_LOW, "DT 6\n", NULL, 0, 32,
  rdDelay, 0, 0, NULL, NULL, -1},

  /* Param 62, read D delay reference channel and delay as string */
  {&DSET_SI, GPIBREAD, IB_Q_LOW, "DT 6\n", NULL, 0, 32, 
  rdDelay, 0, 0, NULL, NULL, -1 },

  /* Param 63, set D delay reference channel */
  {&DSET_MBBO, GPIBWRITE, IB_Q_HIGH, "DT 6\n", "DT 6,%u,", 0, 32,
  setDelay, 0, 0, NULL, &tABCD, -1},

  /* Param 64, read D delay reference */
  {&DSET_MBBI, GPIBREAD, IB_Q_LOW, "DT 6\n", NULL, 0, 32,
  rdDelay, 0 ,0, NULL, &tABCD, -1},

  /* Param 65, set D output mode */
  {&DSET_MBBI, GPIBWRITE, IB_Q_HIGH, NULL, "OM 6,%u\n", 0, 32,
  NULL, 0, 0, NULL, &ttlNimEclVar, -1},

  /* Param 66, read D output mode */
  {&DSET_MBBI, GPIBREAD, IB_Q_LOW, "OM 6\n", "%lu", 0, 32,
  NULL, 0, 0, NULL, &ttlNimEclVar, -1},

  /* Param 67, set D output amplitude */
  {&DSET_AO, GPIBWRITE, IB_Q_HIGH, NULL, "OA 6,%.1f\n", 0, 32,
  NULL, 0, 0, NULL, NULL, -1},

  /* Param 68, read D output amplitude */
  {&DSET_AI, GPIBREAD, IB_Q_LOW, "OA 6\n", "%lf", 0, 32,
  NULL, 0, 0, NULL, NULL, -1},

  /* Param 69, set D output offset */
  {&DSET_AO, GPIBWRITE, IB_Q_HIGH, NULL, "OO 6,%.1f\n", 0, 32,
  NULL, 0, 0, NULL, NULL, -1},

  /* Param 70, read D output offset */
  {&DSET_AI, GPIBREAD, IB_Q_LOW, "OO 6\n", "%lf", 0, 32,
  NULL, 0, 0, NULL, NULL, -1},

  /* Param 71, set D output Termination */
  {&DSET_BO, GPIBWRITE, IB_Q_HIGH, NULL, "TZ 6,%u\n", 0, 32,
  NULL, 0, 0, NULL, &lozHiz, -1},

  /* Param 72, read D output Termination */
  {&DSET_BI, GPIBREAD, IB_Q_LOW, "TZ 6\n", "%lu", 0, 32,
  NULL, 0, 0, NULL, &lozHiz, -1},

  /* Param 73, set D output Polarity */
  {&DSET_BO, GPIBWRITE, IB_Q_HIGH, NULL, "OP 6,%u\n", 0, 32,
  NULL, 0, 0, NULL, &invertNorm, -1},

  /* Param 74, read D output Polarity */
  {&DSET_BI, GPIBREAD, IB_Q_LOW, "OP 6\n", "%lu", 0, 32,
  NULL, 0, 0, NULL, NULL, -1},

/* Channel CD Outputs */
  /* Param 75, set CD output mode */
  {&DSET_MBBO, GPIBWRITE, IB_Q_HIGH, NULL, "OM 7,%u\n", 0, 32,
  NULL, 0, 0, NULL, &ttlNimEclVar, -1},

  /* Param 76, read CD output mode */
  {&DSET_MBBI, GPIBREAD, IB_Q_LOW, "OM 7\n", "%lu", 0, 32,
  NULL, 0, 0, NULL, &ttlNimEclVar, -1},

  /* Param 77, set CD output amplitude */
  {&DSET_AO, GPIBWRITE, IB_Q_HIGH, NULL, "OA 7,%.1f\n", 0, 32,
  NULL, 0, 0, NULL, NULL, -1},

  /* Param 78, read CD output amplitude */
  {&DSET_AI, GPIBREAD, IB_Q_LOW, "OA 7\n", "%lf", 0, 32, 
  NULL, 0, 0, NULL, NULL, -1},

  /* Param 79, set CD output offset */
  {&DSET_AI, GPIBWRITE, IB_Q_HIGH, NULL, "OO 7,%.1f\n", 0, 32,
  NULL, 0, 0, NULL, NULL, -1},

  /* Param 80, read CD output offset */
  {&DSET_AI, GPIBREAD, IB_Q_LOW, "OO 7\n", "%lf", 0, 32,
  NULL, 0, 0, NULL, NULL, -1},

  /* Param 81, set CD output Termination */
  {&DSET_BO, GPIBWRITE, IB_Q_HIGH, NULL, "TZ 7,%u\n", 0, 32,
  NULL, 0, 0, NULL, &lozHiz, -1},

  /* Param 82, read CD output Termination */
  {&DSET_BI, GPIBREAD, IB_Q_LOW, "TZ 7\n", "%lu", 0, 32,
  NULL, 0, 0, NULL, &lozHiz, -1},

  /* Param 83, set CD output Polarity */
  {&DSET_BO, GPIBWRITE, IB_Q_HIGH, NULL, "OP 7,%u\n", 0, 32,
  NULL, 0, 0, NULL, &invertNorm, -1},

  /* Param 84, read CD output Polarity */
  {&DSET_BI, GPIBREAD, IB_Q_LOW, "OP 7\n", "%lu", 0, 32,
  NULL, 0, 0, NULL, NULL, -1},

/* Trigger Settings */
  /* Param 85, set Trig Mode */
  {&DSET_MBBO, GPIBWRITE, IB_Q_HIGH, NULL, "TM %u\n", 0, 32,
  NULL, 0, 0, NULL, &intExtSsBmStop, -1},

  /* Param 86, read Trig Mode */
  {&DSET_MBBI, GPIBREAD, IB_Q_LOW, "TM \n", "%lu", 0, 32,
  NULL, 0, 0, NULL, &intExtSsBm, -1},

  /* Param 87, set Trig Rate */
  {&DSET_AO, GPIBWRITE, IB_Q_HIGH, NULL, "TR 0,%.3lf\n", 0, 32,
  NULL, 0, 0, NULL, NULL, -1},

  /* Param 88, read Trig Rate */
  {&DSET_AI, GPIBREAD, IB_Q_LOW, "TR 0\n", "%lf", 0, 32,
  NULL, 0, 0, NULL, NULL, -1},

  /* Param 89, set Burst Rate */
  {&DSET_AO, GPIBWRITE, IB_Q_HIGH, NULL, "TR 1,%.3lf\n", 0, 32,
  NULL, 0, 0, NULL, NULL, -1},

  /* Param 90, read Burst Rate */
  {&DSET_AI, GPIBREAD, IB_Q_LOW, "TR 1\n", "%lf", 0, 32,
  NULL, 0, 0, NULL, NULL, -1},

  /* Param 91, set Burst Count */
  {&DSET_AO, GPIBWRITE, IB_Q_HIGH, NULL, "BC %01.0lf\n", 0, 32,
  NULL, 0, 0, NULL, NULL, -1},

  /* Param 92, read Burst Count */
  {&DSET_AI, GPIBREAD, IB_Q_LOW, "BC \n", "%lf", 0, 32,
  NULL, 0, 0, NULL, NULL, -1},

  /* Param 93, set Burst Period */
  {&DSET_AO, GPIBWRITE, IB_Q_HIGH, NULL, "BP %01.0lf\n", 0, 32,
  NULL, 0, 0, NULL, NULL, -1},

  /* Param 94, read Burst Period */
  {&DSET_AI, GPIBREAD, IB_Q_LOW, "BP \n", "%lf", 0, 32,
  NULL, 0, 0, NULL, NULL, -1},

  /* Param 95, set Trig Input Z */
  {&DSET_BO, GPIBWRITE, IB_Q_HIGH, NULL, "TZ 0,%u\n", 0, 32,
  NULL, 0, 0, NULL, &lozHiz, -1},

  /* Param 96, read Trig Input Z */
  {&DSET_BI, GPIBREAD, IB_Q_LOW, "TZ 0\n", "%lu", 0, 32,
  NULL, 0, 0, NULL, &lozHiz, -1},

  /* Param 97, set Trig Input slope */
  {&DSET_BO, GPIBWRITE, IB_Q_HIGH, NULL, "TS %u\n", 0, 32,
  NULL, 0, 0, NULL, &fallingRising, -1},

  /* Param 98, read Trig Input slope */
  {&DSET_BI, GPIBREAD, IB_Q_LOW, "TS \n", "%lu", 0, 32,
  NULL, 0, 0, NULL, &fallingRising, -1},

  /* Param 99, set Trig Input level */
  {&DSET_AO, GPIBWRITE, IB_Q_HIGH, NULL, "TL %.2lf\n", 0, 32,
  NULL, 0, 0, NULL, NULL, -1},

  /* Param 100, read Trig Input Level */
  {&DSET_AI, GPIBREAD, IB_Q_LOW, "TL \n", "%lf", 0, 32,
  NULL, 0, 0, NULL, NULL, -1},

  /* Param 101, generate Single Trig */
  {&DSET_BO, GPIBCMD, IB_Q_HIGH, "SS \n", NULL, 0, 32,
  NULL, 0, 0, NULL, &singleShot, -1},

  /* Param 102, Store Setting # */
  {&DSET_AO, GPIBWRITE, IB_Q_HIGH, NULL, "ST %01.0lf\n", 0, 32,
  NULL, 0, 0, NULL, NULL, -1},

  /* Param 103, Recall Setting # */
  {&DSET_AO, GPIBWRITE, IB_Q_HIGH, NULL, "RC %01.0lf\n", 0, 32,
  NULL, 0, 0, NULL, NULL, -1},

  /* Param 104, Recall Setting #  */
  {&DSET_BO, GPIBCMD, IB_Q_HIGH, "CL \n", NULL, 0, 32,
  NULL, 0, 0, NULL, &clear, -1}
};


/* The following is the number of elements in the command array above.  */
#define NUMPARAMS	sizeof(gpibCmds)/sizeof(struct gpibCmd)

/******************************************************************************
 *
 * Initialization for device support
 * This is called one time before any records are initialized with a parm
 * value of 0.  And then again AFTER all record-level init is complete
 * with a param value of 1.
 *
 ******************************************************************************/
STATIC long 
init_dev_sup(int parm)
{
  if(parm==0)  {
    devSupParms.debugFlag = &Dg535Debug;
    devSupParms.respond2Writes = -1;
    devSupParms.timeWindow = TIME_WINDOW;
    devSupParms.hwpvtHead = 0;
    devSupParms.gpibCmds = gpibCmds;
    devSupParms.numparams = NUMPARAMS;
    devSupParms.magicSrq = -1;
    devSupParms.name = "devXxDg535Gpib";
    devSupParms.dmaTimeout = DMA_TIME;
    devSupParms.srqHandler = 0;
    devSupParms.wrConversion = 0;
  }
  return(devGpibLib_initDevSup(parm, &DSET_AI));
}

/******************************************************************************
 *
 * Print a report of operating statistics for all devices supported by this
 * module.
 *
 ******************************************************************************/
STATIC long
report(void)
{
  return(devGpibLib_report(&DSET_AI));
}


/****************************************************************************
 *
 * 
 * Custom convert routines for DG535 
 *
 *
 ***************************************************************************/

/******************************************************************************
 *
 * Unique message interpretaion for reading Channel Delays :
 * The command to read the delay setting returns a string with two arguments,
 * (eg  1, 4.300000000000)
 * This routine extracts both arguments and assigns them appropriately. If the
 * parameter corresponds to a "READ DELAY INTO STRING RECORD",
 * a string is generated with the format CHAN + xxx.xxxxxxxxxxxx and stored
 * in the val field of the SI record. (ex:  T + .000000500000)
 *****************************************************************************/

STATIC int rdDelay(struct gpibDpvt *pdpvt, int p1, int p2, char **p3)
{
  int         status;
  double      delay;
  unsigned long chan;

  union delayRec {
    struct aiRecord ai;
    struct mbbiRecord mbbi;
    struct stringinRecord si;
  } *prec = (union delayRec *) (pdpvt->precord);

  if(Dg535Debug)
    printf("rdDelay : returned msg :%s\n",pdpvt->msg);

  /* Change the "," in returned string to a " " to separate fields */
  pdpvt->msg[1] = 0x20;

  /* scan response string for chan reference & delay value  */
  status = sscanf(pdpvt->msg, "%ld%lf", &chan, &delay);

  if(Dg535Debug)
  {
    printf("Dg535 rdDelay(): pdpvt=%p prec=%p status=%ld msg=%s\n",
	pdpvt, prec, status, pdpvt->msg);
  }
  switch (pdpvt->parm)
  {
  case 4:   /* A Delay monitor, must be an si record */
  case 20:  /* B Delay monitor, must be an si record */
  case 46:  /* C Delay monitor, must be an si record */
  case 62:  /* D Delay monitor, must be an si record */
    if (status == 2)    /* make sure both parameters were assigned */
    {
      /* create a string in the value field*/
      strcpy(prec->si.val, pchanName[chan]);
      strcat(prec->si.val, &((pdpvt->msg)[3]));
    }
    else
    {
      if (prec->si.nsev < INVALID_ALARM)
      {
        prec->si.nsev = INVALID_ALARM;
        prec->si.nsta = READ_ALARM;
      }
    }
    break;

  case 3:   /* A Delay monitor, must be an ai record */
  case 19:  /* B Delay monitor, must be an ai record */
  case 45:  /* C Delay monitor, must be an ai record */
  case 61:  /* D Delay monitor, must be an ai record */
    if (status == 2)    /* make sure both parameters were assigned */
    {
      /* assign new delay to value field*/
      prec->ai.val = delay;
    }
    else
    {
      if (prec->ai.nsev < INVALID_ALARM)
      {
        prec->ai.nsev = INVALID_ALARM;
        prec->ai.nsta = READ_ALARM;
      }
    }
    break;

  case 6:    /* A Delay Reference monitor, must be an mbbi record */
  case 22:   /* B Delay Reference monitor, must be an mbbi record */
  case 48:   /* C Delay Reference monitor, must be an mbbi record */
  case 64:   /* D Delay Reference monitor, must be an mbbi record */
    if (status == 2)    /* make sure both parameters were assigned */
    {
      prec->mbbi.rval = chan;
    }
    else
    {
      if (prec->mbbi.nsev < INVALID_ALARM)
      {
        prec->mbbi.nsev = INVALID_ALARM;
        prec->mbbi.nsta = READ_ALARM;
      }
    }
    break;
  }
  return(OK);
}

/******************************************************************************
 *
 * Unique message generation for writing channel Delays :
 * The command to set the channel delay requires two parameters: The channel
 * # to reference from and the time delay. Since changing either of these
 * parameters requires the entire command to be sent, the current state of
 * other parameter must be determined. This is done by reading the delay (which
 * returns both parameters), changing one of the paramaters, and sending
 * the command back.
 *
 *************************************************************************/

STATIC int setDelay(struct gpibDpvt *pdpvt, int p1, int p2, char **p3)
{
  char        curChan;
  char        tempMsg[32];

  union delayRec {
    struct aoRecord ao;
    struct mbboRecord mbbo;
  } *prec = (union delayRec *) (pdpvt->precord);


  /* go read the current delay & channel reference setting */
  /* this is done by specifying a GPIBREAD even though the gpibCmd is 
     defined as a GPIBWRITE. The cmd string in the gpibCmd is initialized
     to make this work */
  
  if(devGpibLib_xxGpibWork(pdpvt, GPIBREAD, -1) == ERROR) 
  {  /* abort operation if read failed */
     return(ERROR);             /* return, signalling an error */
  }

  /* Due to a fluke in the DG535, read again to insure accurate data */

  if(devGpibLib_xxGpibWork(pdpvt, GPIBREAD, -1) == ERROR) 
  { /* abort operation if read failed */
    return(ERROR);             /* return, signalling an error */
  }

  /* change one of the two parameters ... */

  switch(pdpvt->parm)
  {
    case 1:				/* changing time delay */
    case 17:
    case 43:
    case 59:
      curChan = pdpvt->msg[0];		/* save current chan reference */
      /* generate new delay string (correct rounding error) */
      sprintf(pdpvt->msg,gpibCmds[pdpvt->parm].format, (prec->ao.val + 1.0e-13)); 
      pdpvt->msg[5] = curChan;    /* replace "?" with current chan */
      break;

    case 5:                         /* changing reference channel */
    case 21:
    case 47:
    case 63:
      strcpy(tempMsg, &((pdpvt->msg)[3])); /* save current delay setting */
      /* generate new channel reference */
      sprintf(pdpvt->msg, gpibCmds[pdpvt->parm].format, (unsigned int)prec->mbbo.rval);
      strcat(pdpvt->msg, tempMsg); /* append current delay setting */
      break;
  }

  if(Dg535Debug)
    printf("setDelay : returned msg :%s\n",pdpvt->msg);

  return(OK);         /* aoGpibWork or mbboGpibWork will call xxGpibWork */
}