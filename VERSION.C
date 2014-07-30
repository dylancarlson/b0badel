/* $Workfile:   version.c  $
** $Revision:   1.19  $
*/
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
 
/* the first three fields refer to the original STadel version */

/*
int  PATCHNUM = 199;
char VERSION[] = "3.3b";
char MACHINE[] = "-PC";
*/

/* this is the current b0badel version number */

char b0bVersion [] = "1.19";


#if 0  /* b0badel version log */
  
 1.00
 * send M)ail option in message reading prompt
  
 1.01
 * +area on command line changes "room" references to "area" for Time Arts
 
 1.02
 * prompt user to read Mail (y/N)? at Login time
 * B)ack option in message reading prompt
 * A)gain option at end of each room during Q-scan

 1.03
 * CRs in messages need not be followed by SPACE to do a CR when reading
 
 1.04
 * Hot blurbs (notice.blb)
 * User edit in sysop menu
 * Realname in user log
 * reserved space for alien network support
 * Added many fields to various global data structures
   This means b0badel is no longer data file compatible with STadel 3.3b
 * Only 48 rooms now, but 75 messages per room
 
 1.05
 * Improved the hot help system

 1.06
 * Uploaded file message includes description
 * "Description not available" string in $dir file
 * Prompt for description after an upload instead of before
 * Subject field ("Title" in edit cmd: menu ) support added
 * Improved formatting of msg header for 40 column users

 1.07
 * tightened up orc's io.asm
 * added fossil status to .RS (onConsole only)
 * added A)rkles cmd for articles.blb
 * .A is still Aide menu
 * moved Aide room edit cmd to Y) key
 * added support for main40.mnu - 40 column menu file

 1.08
 * fixed a few nagging little bugs:
  > .RD after .RE works
  > non-expert entering msg in anon room now gets proper header
  > trash-crash on out of memory when adding nodes is now trapped
 * no new features

 1.09
 * added !n! macro to insert username into blurb files.
 * removed .rh command - better done with a door, IMO.
 * added ^Y for sysop to break into chat abruptly
 * added V)iew as synonym for L)ist in editor menu
 * I)nsert paragraph now inserts a blank line as well
 * auto ANSI detection won't knock you out of terminal mode now

 1.10
 * allow tab character during message entry
 * added !r! macro to insert realname into blurbs
 * added #novice string and #novice_cr to configuration
 * renamed configuration file ctdlcnfg.sys to b0badel.cfg
 * prompt for autonet when setting up a shared room
 * messages deleted/moved/copied by an aide on the console
   no longer are reported in the Aide> room.

 1.11
 * fixed an xmodem bug that caused problems occasionally when uploading
   or networking.
 * fixed a bug that kept mail addressed to "name @ system" from being
   delivered (the trailing space in "name " was an unknown user).
 * no new features

 1.12
 * login in Mail room (from console, for example) now prompts to read mail
 * new Aide menu when you hit '.' at the message prompt or while reading
   a message

 1.13
 * added !d! and !t! macros
 * fixed crash mode bug in the macro processor
 * incorporated orc's domain code from STadel 3.3d

 1.14
 * added !s! (system) and !l! (lobby name) macros
 * fixed bug that prevented macros in banner.blb
 * added K)ill msg command for aides (instead of . then Delete)
 * made a cleaner looking, lowercase expert mode message command line
 * fixed bug that caused the loop zapper to reject anonymous messages
 * changed anonymous poster's name from "****" to "*** ******"
   in honor of Max Watson, who's name was once deemed profane

 1.15
 * recompiled with Zortech C
 * fixed crasher bug in prtBody()

 1.16
 * solved Zortech door problem (obscure compiler "feature")

 1.17
 * removed "FLOOR mode"
 * added non-expert instructions to the "*" System Map command
 * removed the "ready y/n" prompt from download procedures
 * added '!' before each door name for the "!?" command
 * made getSysName() recursive (sysop T cmd, sysop/net E cmd, etc.)
 * added K)ill as an option under sysop/net
 * killing a node now zeros the struct in the net table for safety

 1.18
 * word wrap in message editor now handles TAB properly
 * massive code cleanup (lots of prototypes added, etc.)
 * removed E)cho option from sysop menu (unnecessary complication)
 * removed Z) autodialer as well (waste of code space, IMHO)
 * removed #PARANOID configuration flag - it's now always "paranoid"
 * simplified and cleaned up modem code
 * fixed the awful STadel bug that reared its ugly head
   whenever a user dropped carrier in Chat
 * L)ist command now lists users in Mail> room
 * removed SEELOG.EXE from the distribution package

 1.19
 * changed ESC to ^X for exiting chat, term mode, etc.
 * added '\' command to skip to the next directory room (for file leeches)
 * added crlobby.mnu, crmail.mnu, crmsg.mnu and crdir.mnu - seen when a user
   hits <cr> at the roomname prompt.
 * added roomname.hlp, which is seen by non-experts when they do the '*'
   command to get a floor map
#endif

