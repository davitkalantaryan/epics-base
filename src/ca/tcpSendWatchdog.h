/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/*  
 *  $Id$
 *
 *                              
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *                                  
 *  Copyright, 1986, The Regents of the University of California.
 *                                  
 *           
 *	Author Jeffrey O. Hill
 *	johill@lanl.gov
 *	505 665 1831
 */

#ifndef tcpSendWatchdogh  
#define tcpSendWatchdogh

#include "epicsTimer.h"

class tcpSendWatchdog : private epicsTimerNotify {
public:
    tcpSendWatchdog ( 
        epicsMutex & cbMutex, cacContextNotify & ctxNotify,
        tcpiiu &, double periodIn, epicsTimerQueue & queueIn );
    virtual ~tcpSendWatchdog ();
    void start ( const epicsTime & );
    void cancel ();
private:
    const double period;
    epicsTimer & timer;
    epicsMutex & cbMutex;
    cacContextNotify & ctxNotify;
    tcpiiu & iiu;
    expireStatus expire ( const epicsTime & currentTime );
	tcpSendWatchdog ( const tcpSendWatchdog & );
	tcpSendWatchdog & operator = ( const tcpSendWatchdog & );
};

#endif // #ifndef tcpSendWatchdog