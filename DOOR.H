/*
 * door.h -- headers used for shell escapes
 *
 * shell escape information is kept in the file CTDLDOOR.SYS, which is composed
 * of lines of the following form:
 *
 * <cmd name> <access> <program name> [command tail] [#comment]
 *
 * like so:
 *
 * shell   s    a:\auto\tsh.prg  -N
 * arcp    ur   a:\bin\arc.tos p
 * popular a    a:\bin\tsh.prg a:\bin\popular.tos -m >! a:\library\popular
 * rogue   un   a:\bin\rogue.prg -tty
 *
 * permission modes for shell commands are as follows:
 *
 *  [s|a|u][t][n][r][w][f][x][l<dir>]
 *  s == only sysop access
 *  a == aide & sysop access
 *  u == universal access
 *  t == allow command tails
 *  n == pass username to the command as a command tail
 *  d == only usable in directory rooms
 *  r == only executable in readable directories (requires 'd' flag)
 *  w == only executable in writable directories (requires 'd' flag)
 *  c == run on the console only
 *  m == run on the modem only
 *  l<dir> == link the command to this directory
 *  f == door is FOSSIL aware, so do not redirect output
 *  x == watchdog (reboot is carrier is lost)
 *
 *  The command-line form for a shell escape is:
 *  !<name> <tail>
 *
 *  if you are not a sysop, you cannot use the characters '\' or ':' in
 *  a command tail.
 *
 *  internally, shell escapes will be done via a straight Pexec() to
 *  execute them.
 */

struct doorway {
#define DOORSIZE        10
    char dr_name[DOORSIZE];
    int  dr_mode;

    /* mode flags represented as octal (all bits set = 177777 */

#define DR_UMASK        000003
#define DR_SYSOP        000002          /* sysops only                  */
#define DR_AIDE         000001          /* ... + aides                  */
#define DR_USER         000000          /* ... + users                  */

#define DR_TAIL         000004          /* command tails are allowed    */

#define DR_NAMED        000010          /* pass in username             */

#define DR_DIR          000020          /* directory rooms...           */
#define DR_WRITE        000040          /* only in writable dirs...     */
#define DR_READ         000100          /* only in readable dirs...     */

#define DR_IOMASK       000600
#define DR_CONSOLE      000200          /* run on the console only?     */
#define DR_MODEM        000400          /* or the modem only?           */

#define DR_LINKED       001000          /* link the program to a dir    */

#define DR_WATCHDOG     010000          /* invoke watchdog              */
#define DR_FOSSIL       020000          /* door is FOSSIL aware         */

    char *dr_dir;
    char *dr_cmd;                       /* program...                   */
    char *dr_tail;                      /* tail to pass in              */
    char *dr_remark;                    /* `what I do' comment...       */
    struct doorway *dr_next;
} ;


void rundoor    (struct doorway *door, char *tail);
void initdoor   (void);
void dodoor     (void);
