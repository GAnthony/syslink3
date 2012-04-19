/*
 *  ======== _lad.h ========
 */

#ifndef _lad_
#define _lad_

#ifdef __cplusplus
extern "C" {
#endif

#include "_MultiProc.h"
#include <ti/ipc/NameServer.h>


extern Bool logFile;
extern FILE *logPtr;

/* macros for writing to log file: */
#define LOG0(a)  \
    if (logFile == TRUE) {  fprintf(logPtr, a); fflush(logPtr); }

#define LOG1(a, b)  \
    if (logFile == TRUE) {  fprintf(logPtr, a, b); fflush(logPtr); }

#define LOG2(a, b, c)  \
    if (logFile == TRUE) {  fprintf(logPtr, a, b, c); fflush(logPtr); }


/* macros for generating verbose output: */
#define PRINTVERBOSE0(a)  \
    if (verbose == TRUE) {  printf(a); }

#define PRINTVERBOSE1(a, b)  \
    if (verbose == TRUE) {  printf(a, b); }

#define PRINTVERBOSE2(a, b, c)  \
    if (verbose == TRUE) {  printf(a, b, c); }


/* LAD commmand FIFO strings: */
#define LAD_COMMANDFIFO		"/tmp/LAD/LADCMDS"
#define LAD_WORKINGDIR		"/tmp/LAD/"
#define LAD_RESPONSEFIFOPATH	LAD_WORKINGDIR
#define LAD_PROTOCOLVERSION	"03000000"    /*  MMSSRRRR */

#define LAD_MAXNUMCLIENTS	32      /* max simultaneous clients */
#define LAD_CONNECTTIMEOUT	5.0	/* LAD connect response timeout (sec) */
#define LAD_DISCONNECTTIMEOUT	5.0	/* LAD disconnect timeout (sec) */
#define LAD_MAXLENGTHFIFONAME	128 	/* max length client FIFO name */
#define LAD_MAXLENGTHCOMMAND	512	/* size limit for LAD command string */
#define LAD_MAXLENGTHRESPONSE	512	/* size limit for LAD response string */
#define LAD_MAXLENGTHPROTOVERS	16	/* size limit for protocol version */
#define LAD_MAXLOGFILEPATH	256	/* size limit for LAD log file path */
#define LAD_COMMANDLENGTH       sizeof(struct LAD_CommandObj)
#define LAD_RESPONSELENGTH      sizeof(union LAD_ResponseObj)


typedef enum {
    LAD_CONNECT = 0,
    LAD_DISCONNECT,
    LAD_NAMESERVER_SETUP,
    LAD_NAMESERVER_DESTROY,
    LAD_NAMESERVER_PARAMS_INIT,
    LAD_NAMESERVER_CREATE,
    LAD_NAMESERVER_DELETE,
    LAD_NAMESERVER_ADDUINT32,
    LAD_NAMESERVER_GETUINT32,
    LAD_NAMESERVER_REMOVE,
    LAD_NAMESERVER_REMOVEENTRY,
    LAD_EXIT
} _LAD_Command;

struct LAD_CommandObj {
    Int cmd;
    Int clientId;
    union {
	struct {
	    Int pid;
	    Char name[LAD_MAXLENGTHFIFONAME];
	    Char protocol[LAD_MAXLENGTHPROTOVERS];
	} connect;
	struct {
	    Char name[NameServer_Params_MAXNAMELEN];
	    NameServer_Params params;
	} create;
	struct {
	    NameServer_Handle handle;
	} delete;
	struct {
	    NameServer_Handle handle;
	    Char name[NameServer_Params_MAXNAMELEN];
	    UInt32 val;
	} addUInt32;
	struct {
	    NameServer_Handle handle;
	    Char name[NameServer_Params_MAXNAMELEN];
	    UInt16 procId[MultiProc_MAXPROCESSORS];
	} getUInt32;
	struct {
	    NameServer_Handle handle;
	    Char name[NameServer_Params_MAXNAMELEN];
	} remove;
	struct {
	    NameServer_Handle handle;
	    Ptr entryPtr;
	} removeEntry;
    } args;
};

union LAD_ResponseObj {
    struct {
	Int status;
	UInt32 val;
    } getUInt32;
    struct {
	Int status;
	Int assignedId;
    } connect;
    struct {
	Int status;
	NameServer_Handle handle;
    } delete;
    NameServer_Params params;
    NameServer_Handle handle;
    Ptr entryPtr;
    Int status;
};


#ifdef __cplusplus
}
#endif

#endif
