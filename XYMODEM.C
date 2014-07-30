/*{ $Workfile:   xymodem.c  $
 *  $Revision:   1.12  $
 * 
 *  protocol drivers for file and message xfers
}*/
/* 
**    Copyright (c) 1989, 1991 by Robert P. Lee 
**        Sebastopol, CA, U.S.A. 95473-0846 
**               All Rights Reserved 
** 
**    Permission is granted to use these files for 
**  non-commercial purposes only.  You may not include 
**  them in whole or in part in any commercial software. 
** 
**    Use of this source code or resulting executables 
**  by military organizations, or by companies holding 
**  contracts with military organizations, is expressly 
**  forbidden. 
*/ 

#include <io.h>
#include <string.h>
#include "ctdl.h"
#include "externs.h"
#include "dirlist.h"
#include "audit.h"
#include "protocol.h"

/* #define XMDEBUG */

/*  GLOBAL Contents:
**
**  sendCchar()     send a character into the Hold buffer
**  sendCinit()     set up for putting a file in the Hold buffer
**  sendARinit()    init an ARchive xfer
**  sendYhdr()      send batch header block
**  beginWC()       initializes protocol transfers
**  endWC()         shuts down after a protocol transfer
**  enterfile()     accept a file using some protocol
*/

/*** prototypes for static functions ***/

static int can_can (void);
static int sendXchar (int c);
static int sendYchar (int c);
static int sendVchar (int c);
static int sendXinit (void);
static int sendXend (void);
static int eot_ack  (void);
static int sendYend (void);
static int parseYheader (void);
static int recXfile(int (*pc)(int));
static int gWCpacket (char *packet, char *sector, int size);
static int recVfile(int (*pc)(int));
static int calcrc (register char *p, register int count);
static unsigned int calck (register char *p, register int size);
static void outWCpacket (int pknum, register char *pk, register int pksize);
static int sWCpacket (char pknum, char *pk, int pksize);
static int flingYpacket (register int size);


/*** GLOBAL datas ***/

label  rYfile;          /* file coming in from remote */

char WCError;           /* needed by other modules... */
char batchWC;           /* is this gonna be a batch transfer? */

int (*sendPFchar)(int);

/*** static datas ***/

static char current;
static int  blocknum;
static int  CRCmode;
static int  xmp;
static char lasterror;
static char packet[YMSECTSIZE];

static int  dirty;      /* dirty line?  (more than 10 errors total?) */
static int  rYbatch;    /* receiving Ymodem batch? */
static long rYsize;     /* size of incoming file for ymodem batch */

#define MAX_RETRY       10
#define CRC_START       'C'


/*
** can_can() - dump a pair of cancels out the modem
*/
static int can_can (void)
{
    if (gotcarrier()) 
    {
        modout(CAN);
        modout(CAN);
    }
    return 1;
}


/*
 ************************************************************************
 *      send?Char(c) - send a character out the modem port via the      *
 *                     appropriate protocol.                            *
 ************************************************************************
 */

/*
** send Xmodem char
*/
static int sendXchar (int c)
{
    packet[xmp++] = (char)c;

    if (xmp == SECTSIZE)    /* packet full -- send it... */
    {
        xmp = 0;

        return sWCpacket(current, packet, SECTSIZE);
    }
    return YES;
}


/*
** send Ymodem char
*/
static int sendYchar (int c)
{
    packet[xmp++] = (char)c;

    if (xmp == YMSECTSIZE)  /* packet full -- send it... */
    {                       
        xmp = 0;

        return flingYpacket(YMSECTSIZE);
    }
    return YES;
}


/*
** send Vanilla char
*/
static int sendVchar (int c)
{
    modout((c == CAN) ? '@' : c);
    return gotcarrier();
}


/*
** sendCchar -- put a char in the held msg buffer
*/
int sendCchar (int c)
{
    if (xmp < MAXTEXT-2) 
    {
        if (c)
        {
            tempMess.mbtext[xmp++] = (char)c;
        }
        return 1;
    }
    return 0;
}


/*
** send?init() - prepare the universe for sending a file via some protocol.
*/

/*
** send Xmodem init
*/
static int sendXinit (void)
{
    register count = 10;

    sendPFchar = sendXchar;     
    WCError = CRCmode = NO;
    blocknum = current = 1;
    xmp = dirty = 0;

    while (count && gotcarrier()) 
    {
        register int c;

        switch (receive(10))
        {
        case ERROR:
            --count;
            break;

        case CRC_START:
            CRCmode = YES;
            return YES;

        case NAK:
            CRCmode = NO;
            if (!inNet)
            {
                xprintf("checksum\n");
            }
            return YES;

        case CAN:
            if (receive(1) == CAN)
            {
                count = 0;  /* breakout of while loop */
                break;
            }
        
        default:
            --count;
        }
    }
    ++WCError;
    return NO;
}


/*
** sendCinit - init an xfer to the Held buffer
*/
int sendCinit (void)
{
    if (heldMessage)
    {
        xmp = strlen(tempMess.mbtext);
    }
    else
    {
        xmp = 0;
        zero_struct(tempMess);
    }
    sendPFchar = sendCchar;
    return 1;
}


/*
** sendARinit -- init an ARchive xfer
*/
int sendARinit (void)
{
    static char journal[80] = "";
    char temp[120], file[80];

    sprintf(temp, "journal (");

    if (strlen(journal))
    {
        sprintf(EOS(temp), "C/R = `%s', ", journal);
    }
    strcat(temp, "ESC aborts)");

    usingWCprotocol = ASCII;

    getString(temp, file, 80, ESC, YES);

    if (file[0] != ESC)
    {
        if (file[0])
        {
            strcpy(journal, file);
        }
        else if (journal[0])
        {
            strcpy(file, journal);
        }
        else
        {
            return 0;
        }

        return ARsetup(file);
    }
    return 0;
}


/*
** send?end() - Terminate the download
*/

/*
** send Xmodem end
*/
static int sendXend (void)
{
    register retry;

    if (gotcarrier() && !WCError) 
    {
        while (xmp && sendXchar(0))     /* pad last block with nuls */
            ;

        return eot_ack();
    }
    return NO;
}

/*
** routine common to sendXend() and sendYend()
*/
static int eot_ack (void)
{
    register int retry = MAX_RETRY;

    while (retry-- && gotcarrier())
    {
        modout(EOT);

        if (receive(5) == ACK)
        {
            return YES;
        }
    }
    return NO;
}

/*
** send Ymodem end
*/
static int sendYend (void)
{
    register retry;

    if (!WCError && flingYpacket(xmp))
    {
        return eot_ack();
    }
    return NO;
}


/*
** send closing nul to the Held buffer
*/
int sendCend (void)
{
    tempMess.mbtext[xmp] = 0;
    return heldMessage = YES;
}


/*
** sendYhdr() - send batch header block.
**              if the filename is null, shutdown the batch transfer.
*/
int sendYhdr (char *name, long size)
{
    char *p;

    if (sendXinit()) 
    {
        /* set up to send block 0 */

        blocknum = current = 0;

        if (name) 
        {
            /* send the filename in lowercase to make ymodem
            ** implementors happy, then flip the size out
            */
            for (p = name; *p; ++p)
            {
                sendXchar(tolower(*p));
            }
            sendXchar(0);
            wcprintf("%ld",size);
        }
        else
        {
            sendXchar(0);
        }
        while (xmp > 0 && !WCError)
        {
            sendXchar(0);
        }
    }
    return !WCError;
}


/*
** parseYheader() - got a ymodem header block, so cut it up
*/
#define YOK     0                       /* Header checked out */
#define YDUP    1                       /* got two headers */
#define YNULL   2                       /* got null header */
#define YOOPS   3                       /* file open error */

static int parseYheader (void)
{
    register char *p;

    if (rYbatch)
    {
        return YDUP;
    }
    else if (packet[0]) 
    {
        rYbatch = YES;                  /* reset start-o-file flgs*/
        p = 1 + EOS(packet);            /* skip over the filename */
        if (*p)                         /* and grab the filesize  */
        {
            sscanf(p, "%ld", &rYsize);
        }
        copystring(rYfile,
            ((p = strrchr(packet,'/')) != NULL) ? p : packet, NAMESIZE);

        splitF(netLog, "Receiving `%s' (%s)\n",rYfile, plural("byte",rYsize));

        if (batchWC && (upfd = safeopen(rYfile, "wb")) == NULL)
        {
            return YOOPS;
        }
        return YOK;
    }
    return YNULL;
}


/*
** rec?file() - accept a file from the modem via some protocol.
**              returns 0 if everything worked, -1 if a batch
**              error happened or a null headerblock came in,
**              and 1 if something blew up 
*/

/*
** receive X/Ymodem packet
*/
int recXfile(int (*pc)(int))
{
    register retry, count;
    register char *p;
    register size;
    int foo;
    char pn;
    unsigned char c;
    char lastSector;
    char startingup;

    rYbatch = NO;
    rYsize  = -1L;

xgo:
    lastSector = 0;
    startingup = YES;
    lasterror = ' ';
    blocknum = current = 1;

    for (retry = 0; gotcarrier() && retry < MAX_RETRY; )
    {
        /*
         * start off by acking the last packet or by requesting a
         * transfer mode.
         */
        if (startingup) 
        {
            CRCmode = (retry < 5);

            modout(CRCmode ? CRC_START : NAK);

            #ifdef XMDEBUG
                puts(CRCmode ? "CRC?" : "chksum?");
                #endif
        }
        else
        {
            modout(retry ? NAK : ACK);

            #ifdef XMDEBUG
                puts(retry ? "NAK" : "ACK");
                #endif
        }

        foo = receive(10);

        if (foo == ERROR)   /* was "c = receive(5);" oops! */
        {
            ++retry;
            continue;
        }
        else
        {
            c = (unsigned char)foo;
        }

        if (c == SOH || c == STX) 
        {
            startingup = NO;
            size = (c == SOH) ? SECTSIZE : YMSECTSIZE;
            if (gWCpacket(packet, &pn, size)) 
            {
                if (blocknum == 1) 
                {
                    if (pn == 0) 
                    {
                        if ((c = parseYheader()) == YOK) 
                        {
                            modout(ACK);
                            goto xgo;
                        }
                        else if (c == YNULL) 
                        {
                            modout(ACK);
                            return -1;
                        }
                        else
                        {
                            break;
                        }
                    }
                    else if (batchWC && !rYbatch)
                    {
                        break;
                    }
                }
                if (pn == current) 
                {
                    current++;
                    lastSector++;
                    blocknum++;
                    for (p = packet, count = 0; count < size; count++) 
                    {
                        if (rYsize == 0)
                        {
                            break;
                        }
                        if ((*pc)(*p++) == ERROR )
                        {
                            goto xcan;
                        }
                        if (rYsize > 0)
                        {
                            rYsize--;
                        }
                    }
                    retry = 0;
                    continue;
                }
                else if (pn == lastSector) 
                {
                    retry=0;
                    break;
                }
            }
        }
        else if (c == EOT) 
        {
            #ifdef XMDEBUG 
                puts("[EOT]{ACK}");
                #endif

            modout(ACK);
            if (batchWC)                /* batch receive, close 'er down */
            {
                fclose(upfd);
            }
            else if (rYbatch)           /* got batch during single */
            {
                can_can();              /* so shut down the other end */
            }
            return 0;
        }
        else if (c == CAN) 
        {
            int recval = receive(5);

            c = (unsigned char)recval;

            if (recval == CAN || recval == ERROR) 
            {
                #ifdef XMDEBUG
                    puts("[CAN]");
                    #endif

                goto xout;
            }
        }

        while (receive(1) != ERROR)     /* garbage -- synchronise */
            ;

        ++retry;
    }
xcan:
    #ifdef XMDEBUG  
        puts("[CAN-retry]");
        #endif

    can_can();

xout:
    if (batchWC && rYbatch) 
    {
        fclose(upfd);
        dunlink(rYfile);
    }
    return 1;
}



/*
** gWCpacket -- get a WC packet
*/
static int gWCpacket (char *packet, char *sector, int size)
{
    register unsigned char *p;
    register int recval;
    register int i;
    char cks, packet_number, c_packet_number;
    unsigned crc;

    /*
     * here comes a packet of one flavour or another...
     */

    xprintf("\r<%c%4d%c  ", lasterror, blocknum, (size!=SECTSIZE)?'y':'x');

    #ifdef XMDEBUG
        xprintf("\n");
        #endif

    lasterror = '-';

    if ((recval = receive(2)) == ERROR)
    {
        return 0;
    }
    packet_number = (char)recval;

    if ((recval = receive(2)) == ERROR)
    {
        return 0;
    }
    c_packet_number = (char)recval;

    #ifdef XMDEBUG
        printf("[");
        #endif

    for (p = (unsigned char *)packet, i = 0; i < size; ++i)
    {
        recval = receive(2);

        if (recval == ERROR) 
        {
            #ifdef XMDEBUG
                puts("<TIMEOUT>]");
                #endif
            return 0;
        }
        else
        {
            /*
            ** #ifdef XMDEBUG
            **
            **  char lc=0;
            **
            **  if (isprint(c))
            **      putchar(c);
            **  else if (c)
            **      printf(" %02x", 0xff & c);
            **  else if (lc != c)
            **      putchar(249);
            **  lc = c;
            **
            ** #endif
            */

            *p++ = (char)recval;
        }
    }

    #ifdef XMDEBUG
        puts("]");
        #endif

    lasterror = 'c';

    /*** get checksum or first byte of crc ***/

    if ((recval = receive(2)) == ERROR)
    {
        return 0;
    }

    if (CRCmode) 
    {
        crc  = recval << 8;     /* hi byte of crc */

        if ((recval = receive(2)) == ERROR)
        {
            return 0;
        }
        crc |= recval & 0xFF;   /* lo byte of crc */

        if (crc != calcrc(packet, size))
        {
            return 0;
        }
    }
    else if (recval != calck(packet, size))     /* good checksum? */
    {
        return 0;
    }

    /*
     * make sure that the packet numbers match..
     */
    if (c_packet_number == ~packet_number) 
    {
        lasterror = ' ';
        *sector = packet_number;

        return 1;       /* success! */
    }
    lasterror = '?';
    return 0;
}



/*
** receive Vanilla file
*/
static int recVfile (int (*pc)(int))
{
    register int c;

    while ((c = receive(10)) != ERROR) 
    {
        if (c == CAN) 
        {
            if ((c = receive(2)) == CAN)
            {
                return NO;
            }
            else
            {
                (*pc)(CAN);
            }
        }

        (*pc)(c);
    }
    return YES;
}


/*
** calcrc() - calculate crc for a packet
*/
static int calcrc (char *p, int count)
{
    static const int crctb[256] =     /* sacrifice memory for speed */
    {
        0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
        0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
        0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
        0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
        0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
        0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
        0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
        0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
        0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
        0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
        0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
        0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
        0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
        0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
        0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
        0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
        0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
        0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
        0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
        0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
        0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
        0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
        0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
        0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
        0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
        0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
        0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
        0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
        0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
        0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
        0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
        0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0
    };
    int c, crc;

    for (crc = 0; count > 0; --count) 
    {
        c = 0xff & (*p++);      /* grab the character, kill sign extend */

        crc = crctb[(0xff & (crc >> 8)) ^ c] ^ ((0xff & crc) << 8);
    }
    return crc;
}


/*
** calck() - calculate checksum for a packet
*/
static unsigned int calck (register char *p, register int size)
{
    register unsigned cks;

    for (cks = 0; size > 0; --size)
    {
        cks = (cks + *p++) & 0xFF;
    }
    return cks;
}



/*
** outWCpacket() - dump a WC packet to modem.
*/
static void outWCpacket (int pknum, register char *pk, register int pksize)
{
    int cksum;

    cksum = CRCmode ? calcrc(pk, pksize) : calck(pk, pksize);

    modout((pksize == YMSECTSIZE) ? STX : SOH);

    modout(pknum);
    modout(~pknum);

    while (pksize-- > 0) 
    {
        modout(*pk++);
    }
    if (CRCmode)
    {
        modout(cksum >> 8);
    }

    modout(cksum);
}


/*
** sWCpacket - send & verify a WC packet
*/
static int sWCpacket (char pknum, char *pk, int pksize)
{
    register int reply;
    register int retry = 0;

    while (gotcarrier() && retry < MAX_RETRY) 
    {
        xprintf("\r>%5d%c\r", blocknum, (pksize == YMSECTSIZE) ? 'y' : 'x');

        outWCpacket(pknum, pk, pksize);

        do
        {
            reply = receive(15);

            if (reply == CAN)      /* ^X^X cancels */
            {
                if ((reply = receive(5)) == CAN)
                {
                    goto sWCerr;
                }
            }
        }
        while (reply != ERROR && reply != ACK && reply != NAK);

        if (reply == ACK) 
        {
            current++;
            blocknum++;

            /*
             * adjust the dirtiness factor of the line.
             */
            if (pksize == SECTSIZE)     /* xmodem sectors */
            {   
                if (retry)
                {
                    dirty += 2;
                }
                else if (dirty)
                {
                    --dirty;            /* line's looking cleaner */
                }
            }
            else                        /* 1k sectors */
            {
                dirty = retry * 8;
            }

            return YES;
        }

        ++retry;
    }
sWCerr:
    can_can();
    WCError++;
    return NO;
}


/*
** flingYpacket - send a Ymodem packet.
*/
static int flingYpacket (register int size)
{
    register char *pp;

    while (size % SECTSIZE)
    {
        packet[size++] = 0;
    }

    if (dirty || size < YMSECTSIZE) 
    {
        for (pp = packet; size > 0; pp += SECTSIZE, size -= SECTSIZE)
        {
            if (!sWCpacket(current, pp, SECTSIZE))
            {
                return NO;
            }
        }
        return YES;
    }
    return sWCpacket(current, packet, YMSECTSIZE);
}


/*
** beginWC() - set up the system for a WC download
*/
int beginWC()
{
    switch (usingWCprotocol) 
    {
    case VANILLA:
        sendPFchar = sendVchar;
        return YES;

    case YMODEM:
        if (sendXinit()) 
        {
            sendPFchar = sendYchar;
            return YES;
        }
        return NO;

    case CAPTURE:
        return sendCinit();

    case TODISK:
        return sendARinit();

    default:
        return sendXinit();
    }
}


/*
** endWC() - fix up the system after a WC download
*/
int endWC (void)
{
    switch (usingWCprotocol) 
    {
    case VANILLA:
        return can_can();

    case YMODEM:
        return sendYend();

    case CAPTURE:
        return sendCend();

    case TODISK:
        return sendARend();

    default:
        return sendXend();
    }
}


/*
 ************************************************************************
 *      enterfile() - accepts a file from the modem using some protocol *
 ************************************************************************
 */
int enterfile (int (*pc)(int), char mode)
{
    batchWC = NO;

    switch (mode) 
    {
    case VANILLA:
        WCError = recVfile(pc);
        break;

    default:
        WCError = recXfile(pc);
        break;
    }

    if (WCError)
    {
        while (gotcarrier() && (receive(1) != ERROR))
            ;
    }
    return !WCError;
}
