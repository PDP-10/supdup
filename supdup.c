/*
 * User supdup program.
 *
 *	Written Jan. 1985 by David Bridgham.  Much of the code dealing
 * with the network was taken from the telnet user program released
 * with 4.2 BSD UNIX.
 */

/* Hacked by Bjorn Victor, November 2010, to add function declarations
   to make it compile/run in some more environments.
   Hacked by Bjorn Victor, 2004-2005, to support termios (Linux) and location setting.
*/

/* Hacked by Wojciech Gac, January 2015
   Added #ifdef __OpenBSD__ conditions to use strlcpy() when possible.
   This rids us of the compiler complaints on OpenBSD.
*/

/* Hacked by Klotz 2/20/89 to remove +%TDORS from init string.
 * Hacked by Klotz 12/19/88 added response to TDORS.
 * Hacked by Mly 9-Jul-87 to improve reading of supdup escape commands
 * Hacked by Mly July 1987 to do bottom-of-screen cursor-positioning hacks
 *  when escape char is typed 
 * Hacked by Mly 29-Aug-87 to nuke stupid auto_right_margin lossage.

 * TODO: Meta, Super, Hyper prefix keys.
 *  Deal with lossage when running on very narrow screen
 *   (Things like "SUPDUP command ->" should truncate)
 *  Defer printing message at bottom of screen if input chars are pending
 *  Try multiple other host addresses if first fails.
 */

#ifndef USE_CHAOS_STREAM_SOCKET
#define USE_CHAOS_STREAM_SOCKET 1
#endif

#ifndef USE_TERMIOS
#define USE_TERMIOS 1		/* e.g. Linux */
#endif
#ifndef USE_BSD_SELECT
#define USE_BSD_SELECT 0	/* not e.g. Linux */
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#if USE_CHAOS_STREAM_SOCKET
#include <sys/un.h>
#include <sys/stat.h>
#include <arpa/nameser.h>
#include <resolv.h>
#endif

#include <unistd.h>
#include <stdlib.h>

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <setjmp.h>
#include <netdb.h>

#include <curses.h>
#include <term.h>
#include <locale.h>
#include <langinfo.h>

#include "supdup.h"
#include "charmap.h"

#define OUTSTRING_BUFSIZ 2048
unsigned char *outstring;

#define TBUFSIZ 1024
unsigned char ttyobuf[TBUFSIZ];
unsigned char *ttyfrontp = ttyobuf;
unsigned char netobuf[TBUFSIZ];
unsigned char *netfrontp = netobuf;

unsigned char hisopts[256];
unsigned char myopts[256];


int connected = 0;

char myloc[128];

/* fd of network connection */
int net;

int showoptions = 0;
int options;

int debug = 0;
FILE *tdebug_file = 0;	/* For debugging terminal output */
FILE *indebug_file = 0;	/* For debugging network input */
FILE *outdebug_file = 0;	/* For debugging network output */
#define TDEBUG_FILENAME "supdup-trmout"
#define INDEBUG_FILENAME "supdup-netin"
#define OUTDEBUG_FILENAME "supdup-netout"

/* 0377 if terminal has a meta-key */
int mask = 0177;
int high_bits = 0;
/* user supdup command-escaper character */
unsigned char escape_char = ('^'&037);
/* As opposed to winningly-wrap */
int do_losingly_scroll = 0;

/* jmp_buf toplevel; */
jmp_buf	peerdied;

int unicode_translation = 0;

/* Impoverished un*x keyboards */
#define Ctl(c) ((c)&037)

int sd(void);
void suspend(void);
void help(void);
void quit(void);
void rlogout(void);
void suprcv(void);
void command(unsigned char c);
void do_setloc(char *c);
void netflush(int d);
void setescape(void);
void top(void);
void status(void);
void setloc(void);

struct cmd
 {
   unsigned char name;          /* command name */
   char *help;                  /* help string */
   void (*handler)(void);       /* routine which executes command */
 };

struct cmd cmdtab[] =
  {
    /* also q */
    { 'l',	"logout connection and quit", rlogout },
    { 'c',	"close connection and exit", quit },
    /* also c-z */
    { 'p',	"suspend supdup", suspend },
    { 'e',	"set escape character",	setescape },
    { 't',	"set \"top\" bit on next character", top },
    { 's',	"print status information", status },
    { 'L',	"set console location", setloc }, /* [BV] */
    /* also c-h */
    { '?',	"print help information", help },
#if 0
    { 'r',	"toggle mapping of received carriage returns", setcrmod },
    { 'd',	"toggle debugging", setdebug },
    { 'v',	"toggle viewing of options processing", setoptions },
#endif /* 0 */
    { 0 }
  };

int currcol, currline;	/* Current cursor position */

struct sockaddr_in tsin;

void sup_term(void);
void supdup (char *loc);
void intr(int), deadpeer(int);
char *key_name(int);
struct cmd *getcmd(void);
struct servent *sp;

#if !USE_TERMIOS
struct	tchars otc;
struct	ltchars oltc;
struct	sgttyb ottyb;
// historically, this is really fast.
unsigned int sup_ispeed = 9600, sup_ospeed = 9600;
#else
struct termios otio;
speed_t sup_ispeed = 9600, sup_ospeed = 9600;
#endif

int mode(int);
void ttyoflush(void);

#if USE_CHAOS_STREAM_SOCKET

// Where are the chaos sockets? Cf. https://github.com/bictorv/chaosnet-bridge
#ifndef CHAOS_SOCKET_DIRECTORY
#define CHAOS_SOCKET_DIRECTORY "/tmp"
#endif
// What DNS domain should be used to translate Chaos addresses to names?
#ifndef CHAOS_ADDRESS_DOMAIN
#define CHAOS_ADDRESS_DOMAIN "ch-addr.net"
#endif
// What is the default DNS domain for Chaosnet names?
#ifndef CHAOS_NAME_DOMAIN
#define CHAOS_NAME_DOMAIN "chaosnet.net"
#endif
// What DNS server should be used to fetch Chaos class data?
#ifndef CHAOS_DNS_SERVER
#define CHAOS_DNS_SERVER "130.238.19.25"
#endif

static int chaosp = 0;
static int chaos_socket = 0;

static int
connect_to_named_socket(int socktype, char *path)
{
  int sock, slen;
  struct sockaddr_un server;
  
  if ((sock = socket(AF_UNIX, socktype, 0)) < 0) {
    perror("socket(AF_UNIX)");
    exit(1);
  }

  server.sun_family = AF_UNIX;
  sprintf(server.sun_path, "%s/%s", CHAOS_SOCKET_DIRECTORY, path);
  slen = strlen(server.sun_path)+ 1 + sizeof(server.sun_family);

  if (connect(sock, (struct sockaddr *)&server, slen) < 0) {
    if (debug)
      perror("connect(server)");
    return 0;
  }
  return sock;
}

struct __res_state chres;
void
init_chaos_dns()
{
  res_state statp = &chres;

  // initialize resolver library
  if (res_ninit(statp) < 0) {
    fprintf(stderr,"Can't init statp\n");
    exit(1);
  }
  // make sure to make recursive requests
  statp->options |= RES_RECURSE;
  // change nameserver
  if (inet_aton(CHAOS_DNS_SERVER, &statp->nsaddr_list[0].sin_addr) < 0) {
    perror("inet_aton (chaos_dns_server does not parse)");
    exit(1);
  } else {
    statp->nsaddr_list[0].sin_family = AF_INET;
    statp->nsaddr_list[0].sin_port = htons(53);
    statp->nscount = 1;
  }
  // what about the timeout? RES_TIMEOUT=5s, statp->retrans (RES_MAXRETRANS=30 s? ms?), ->retry (RES_DFLRETRY=2, _MAXRETRY=5)
}

// given a domain name (including ending period!) and addr of a u_short vector,
// fill in all Chaosnet addresses for it, and return the number of found addresses.
int 
dns_addrs_of_name(u_char *namestr, u_short *addrs, int addrs_len)
{
  res_state statp = &chres;
  char a_dom[NS_MAXDNAME];
  int a_addr;
  char qstring[NS_MAXDNAME];
  u_char answer[NS_PACKETSZ];
  int anslen;
  ns_msg m;
  ns_rr rr;
  int i, ix = 0, offs;

  sprintf(qstring,"%s.", namestr);

  if ((anslen = res_nquery(statp, qstring, ns_c_chaos, ns_t_a, (u_char *)&answer, sizeof(answer))) < 0) {
    // fprintf(stderr,"DNS: addrs of %s failed, errcode %d: %s\n", qstring, statp->res_h_errno, hstrerror(statp->res_h_errno));
    return -1;
  }

  if (ns_initparse((u_char *)&answer, anslen, &m) < 0) {
    fprintf(stderr,"ns_init_parse failure code %d",statp->res_h_errno);
    return -1;
  }

  if (ns_msg_getflag(m, ns_f_rcode) != ns_r_noerror) {
    // if (trace_dns) 
    fprintf(stderr,"DNS: bad response code %d\n", ns_msg_getflag(m, ns_f_rcode));
    return -1;
  }
  if (ns_msg_count(m, ns_s_an) < 1) {
    // if (trace_dns) 
    fprintf(stderr,"DNS: bad answer count %d\n", ns_msg_count(m, ns_s_an));
    return -1;
  }
  for (i = 0; i < ns_msg_count(m, ns_s_an); i++) {
    if (ns_parserr(&m, ns_s_an, i, &rr) < 0) { 
      // if (trace_dns)
      fprintf(stderr,"DNS: failed to parse answer RR %d\n", i);
      return -1;
    }
    if (ns_rr_type(rr) == ns_t_a) {
      if (((offs = dn_expand(ns_msg_base(m), ns_msg_end(m), ns_rr_rdata(rr), (char *)&a_dom, sizeof(a_dom))) < 0)
	  ||
	  ((a_addr = ns_get16(ns_rr_rdata(rr)+offs)) < 0))
	return -1;
      if (strncasecmp(a_dom, CHAOS_ADDRESS_DOMAIN, strlen(CHAOS_ADDRESS_DOMAIN)) == 0) {
	// only use addresses in "our" address domain
	if (ix < addrs_len) {
	  addrs[ix++] = a_addr;
	}
      } 
    } 
  }
  return ix;
}

int
chaos_connect(char *host, char *contact) 
{
  dprintf(net, "RFC %s %s\r\n", host, contact);
  netflush(0);
  {
    char buf[256], *bp, cbuf[2];
    bp = buf;
    while (read(net, cbuf, 1) == 1) {
      if ((cbuf[0] != '\r') && (cbuf[0] != '\n'))
	*bp++ = cbuf[0];
      else {
	*bp = '\0';
	break;
      }
    }
    if (strncmp(buf,"OPN ", 4) != 0) {
      fprintf(stderr,"%s\n", buf);
      return -1;
    } else {
      printf("%s\n", &buf[4]);
      return 0;
    }
  }
}

char *
get_chaos_host(char *name)
{
  int naddrs = 0;
  u_short haddrs[4];

  // this is just to see it's really a Chaos host
  if (chaos_socket == 0)
    // but if we couldn't connect to cbridge, it's a moot point
    return NULL;

  if ((sscanf(name, "%ho", &haddrs[0]) == 1) && 
      (haddrs[0] > 0xff) && (haddrs[0] < 0xfe00) && ((haddrs[0] & 0xff) != 0)) {
    // Use the address for a "name": it is precise, and it works with the cbridge parser
    return name;
  }
  else if ((naddrs = dns_addrs_of_name((u_char *)name, (u_short *)&haddrs, 4)) > 0) {
    return name;
  } else if (index(name, '.') == NULL) {
    char buf[256];
    sprintf(buf, "%s.%s", name, CHAOS_NAME_DOMAIN);
    if (dns_addrs_of_name((u_char *)buf, (u_short *)&haddrs, 4) > 0) {
      return strdup(buf);
    }
  } 
  // not a Chaos host
  return NULL;
}
#endif // USE_CHAOS_STREAM_SOCKET

int putch (int c)
{
  *ttyfrontp++ = c;
  /*>>>>> LOSES if overflows */
  return 1;
}


void put_newline ()
{
  if (newline)
    tputs (newline, 1, putch);
  else
    {
      if (carriage_return)
        tputs (carriage_return, 0, putch);
      else
        putch ('\r');
      if (cursor_down)
        tputs (cursor_down, 0, putch);
      else
        putch ('\n');
    }
  ttyoflush ();
}

#define term_goto(c, l) \
  tputs (tparm (cursor_address, l, c), lines, putch)

char *hostname;
#if USE_CHAOS_STREAM_SOCKET
char *contact = "SUPDUP";
#endif

void
get_host (char *name)
{
#if USE_CHAOS_STREAM_SOCKET
  if ((hostname = get_chaos_host(name)) != NULL) {
    chaosp = 1;
    // done here, hostname is now the host to connect to.
    return;
  } else
    // It wasn't a Chaos name/address, try regular Internet
    chaosp = 0;
#endif

  struct hostent *host;
  host = gethostbyname (name);
  if (host)
    {
      tsin.sin_family = host->h_addrtype;
#ifdef notdef
      bcopy (host->h_addr_list[0], (caddr_t) &tsin.sin_addr, host->h_length);
#else
      bcopy (host->h_addr, (caddr_t) &tsin.sin_addr, host->h_length);
#endif /* h_addr */
      hostname = host->h_name;
    }
  else
    {
      tsin.sin_family = AF_INET;
      tsin.sin_addr.s_addr = inet_addr (name);
      if (tsin.sin_addr.s_addr == -1)
        hostname = 0;
      else
        hostname = name;
    }
}

int
to_bps(int speed)
{
  switch (speed) {
    // Cf. BAUDRT in SYSTEM;TS3TTY >
    //case B75: return 75;
    //case B134: return 134;
    //case B200: return 200;
    //case B19200: return 19200;
    //case B38400: return 38400;
  case B0: return 0;
  case B50: return 50;
  case B110: return 110;
  case B150: return 150;
  case B300: return 300;
  case B600: return 600;
  case B1200: return 1200;
  case B1800: return 1800;
  case B2400: return 2400;
  case B4800: return 4800;
  case B9600: return 9600;
    // reasonable default?
  default: return 9600;
  }
}

int
main (int argc, char **argv)
{
  setlocale(LC_ALL, "");

  myloc[0] = '\0';

  sp = getservbyname ("supdup", "tcp");
  if (sp == 0)
    {
      fprintf (stderr, "supdup: tcp/supdup: unknown service.\n");
      sp = (struct servent *)malloc(sizeof(struct servent));
      sp->s_port = htons(95); // standard
    }

#if !USE_TERMIOS
  ioctl (0, TIOCGETP, (char *) &ottyb);
  ioctl (0, TIOCGETC, (char *) &otc);
  ioctl (0, TIOCGLTC, (char *) &oltc);
  // @@@@ how do we get speed in this case? (doesn't really matter)
  sup_ispeed = 9600;
  sup_ospeed = 9600;
#else
  tcgetattr(0, &otio);
  sup_ispeed = to_bps(cfgetispeed(&otio));
  sup_ospeed = to_bps(cfgetospeed(&otio));
#endif
  setbuf (stdin, 0);
  setbuf (stdout, 0);
  do_losingly_scroll = 0;

  // Before checking arguments, set a default value for unicode_translation.
  // The computed value can be overridden using the -u or -U flags.
  char *encoding = nl_langinfo(CODESET);
  if(strcmp(encoding, "UTF-8") == 0) {
    unicode_translation = 1;
  }
  else {
    unicode_translation = 0;
  }

  if (argc > 1 && (!strcmp (argv[1], "-s") ||
                   !strcmp (argv[1], "-scroll")))
    {
      argc--; argv++;
      do_losingly_scroll = 1;
    }

  if (argc > 1 && (!strcmp (argv[1], "-d") ||
                   !strcmp (argv[1], "-debug")))
    {
      argv++; argc--;
      debug = 1;
    }
  if (argc > 1 && (!strcmp (argv[1], "-tdebug")))
    {
      argv++; argc--;
      tdebug_file = fopen(TDEBUG_FILENAME, "wb"); /* Open for binary write */
      if (tdebug_file == NULL)
        {
          fprintf (stderr, "Couldn't open debug file %s\n",
		   TDEBUG_FILENAME);
          exit (1);
        }
      setbuf(tdebug_file, NULL);	/* Unbuffered so see if we crash */
    }

  if (argc > 1 && !strcmp (argv[1], "-t"))
    {
      argv++, argc--;
      indebug_file = fopen(INDEBUG_FILENAME, "wb"); /* Open for binary write */
      if (indebug_file == NULL)
        {
          fprintf (stderr, "Couldn't open debug file %s\n",
		   INDEBUG_FILENAME);
          exit (1);
        }
      outdebug_file = fopen(OUTDEBUG_FILENAME, "wb"); /* Open for binary write */
      if (outdebug_file == NULL)
        {
          fprintf (stderr, "Couldn't open debug file %s\n",
		   OUTDEBUG_FILENAME);
          exit (1);
        }
    }
  if (argc > 1 && !strcmp(argv[1], "-loc"))
    {
      argv++, argc--;
      if (argc > 1) {
#ifdef __OpenBSD__
	strlcpy(myloc, argv[1], sizeof(myloc));
#else
	strncpy(myloc, argv[1], sizeof(myloc));
#endif /* __OpenBSD__ */
	argv++, argc--;
      } else {
	fprintf(stderr, "-loc requires an argument\n");
	exit(1);
      }
    }

  if (argc > 1 && !strcmp(argv[1], "-u"))
    {
      argv++;
      argc--;
      unicode_translation = 1;
    }

  if (argc > 1 && !strcmp(argv[1], "-U"))
    {
      argv++;
      argc--;
      unicode_translation = 0;
    }

#if USE_CHAOS_STREAM_SOCKET
  init_chaos_dns();
  // Connect to the socket already here, to see whether parsing Chaos host names is relevant
  chaos_socket = connect_to_named_socket(SOCK_STREAM, "chaos_stream");
#endif

  if (argc == 1)
    {
      char *cp;
      char line[200];

    again:
      printf ("Host: ");
      if (fgets(line, sizeof(line), stdin) == 0)
        {
          if (feof (stdin))
            {
              clearerr (stdin);
              putchar ('\n');
            }
          goto again;
        }
      if ((cp = strchr(line, '\n')))
	*cp = '\0';
      get_host (line);
      if (!hostname)
        {
          printf ("%s: unknown host.\n", line);
          goto again;
        }
    }
  else if (argc > 3)
    {
#if USE_CHAOS_STREAM_SOCKET
      fprintf(stderr,"usage: %s host-name [port-or-contact] [-scroll]\n", argv[0]);
#else
      fprintf(stderr,"usage: %s host-name [port] [-scroll]\n", argv[0]);
#endif
      exit(1);
    }
  else
    {
      get_host (argv[1]);
      if (!hostname)
        {
          fprintf(stderr,"%s: unknown host.\n", argv[1]);
          exit (1);
        }
    }

  tsin.sin_port = sp->s_port;
  if (argc == 3)
    {
#if USE_CHAOS_STREAM_SOCKET
      if (chaosp)
	contact = argv[2];
      else {
#endif
      tsin.sin_port = atoi (argv[2]);
      if (tsin.sin_port <= 0)
        {
          fprintf(stderr,"%s: bad port number.\n", argv[2]);
          exit(1);
        }
      tsin.sin_port = htons (tsin.sin_port);
#if USE_CHAOS_STREAM_SOCKET
      }
#endif
    }

#if USE_CHAOS_STREAM_SOCKET
  if (chaosp && chaos_socket)
    net = chaos_socket;
  else
#endif
  net = socket (AF_INET, SOCK_STREAM, 0);
  if (net < 0)
    {
      perror ("supdup: socket");
      exit(1);
    }
  outstring = (unsigned char *) malloc (OUTSTRING_BUFSIZ);
  if (outstring == 0)
    {
      fprintf (stderr, "Memory exhausted.\n");
      exit (1);
    }
  sup_term ();
  if (debug && setsockopt (net, SOL_SOCKET, SO_DEBUG, 0, 0) < 0)
    perror ("setsockopt (SO_DEBUG)");
  signal (SIGINT, intr);
  signal (SIGPIPE, deadpeer);
#if USE_CHAOS_STREAM_SOCKET
  if (chaosp) {
    printf("Trying %s %s...", hostname, contact);
    fflush (stdout);
    if (chaos_connect(hostname, contact) < 0) {
      signal(SIGINT, SIG_DFL);
      exit(1);
    }
  } else {
#endif
    printf("Trying %s port %d ...", inet_ntoa (tsin.sin_addr), ntohs(tsin.sin_port));
  fflush (stdout);
  if (connect (net, (struct sockaddr *) &tsin, sizeof (tsin)) < 0)
/* >> Should try other addresses here (like BSD telnet) #ifdef h_addr */
    {
      perror ("supdup: connect");
      signal (SIGINT, SIG_DFL);
      exit(1);
    }
#if USE_CHAOS_STREAM_SOCKET
  }
#endif
  connected = 1;
  printf ("Connected to %s.\n", hostname);
  printf ("Escape character is \"%s\".", key_name (escape_char));
  fflush (stdout);
  mode (1);
  if (clr_eos)
    tputs (clr_eos, lines - currline, putch);
  put_newline ();
  if (setjmp (peerdied) == 0)
    supdup (myloc);
  ttyoflush ();
  mode (0);
  // cosmetics
  if (clr_eol)
    tputs (clr_eol, columns, putch);
  ttyoflush ();
  fprintf (stderr, "Connection closed by %s.\n", hostname);
  // cosmetics
  if (clr_eol)
    tputs (clr_eol, columns, putch);
  ttyoflush ();
  exit (0);
}

static signed char inits[] =
  {
    /* -wordcount,,0.  should always be -6 unless ispeed, ospeed and uname are there*/
    077,	077,	-6,	0,	0,	0,
    /* TCTYP variable.  Always 7 (supdup) */
    0,	0,	0,	0,	0,	7,
    /* TTYOPT variable.  %TOMVB %TOMOR %TOLWR %TPCBS */
    /* %TOSAI will be set if the user has enabled character translation  */
    1,	2,	020,	0,	0,	040,
    /* Height of screen -- updated later */
    0,	0,	0,	0,	0,	24,
    /* Width of screen minus one -- updated later */
    0,	0,	0,	0,	0,	79,
    /* auto scroll number of lines */
    0,	0,	0,	0,	0,	1,
    /* TTYSMT */
    0,	0,	0,	0,	0,	0,
    // ISPEED
    0,	0,	0,	0,	0,	0,
    // OSPEED
    0,	0,	0,	0,	0,	0,
    // UNAME
    0,	0,	0,	0,	0,	0
  };
#define	INIT_LEN	(sizeof(inits)) // 42	/* Number of bytes to send at initialization */


int sixbit(char c)
{
  if (islower(c)) c = toupper(c);
  if (c >= 040 && c <= 0137)
    return c-040;
  else
    return 0; // space
}
int unsixbit(char c)
{
  return c+040;
}

/*
 * Initialize the terminal description to be sent when the connection is
 * opened.
 */
void
sup_term (void)
{
  int errret, i;

  setupterm (0, 1, &errret);
  if (errret == -1)
    {
      fprintf (stderr, "Bad terminfo database.\n");
      exit (1);
    }
  else if (errret == 0)
    {
      fprintf (stderr, "Unknown terminal type.\n");
      exit (1);
    }

  if (columns <= 1) {
      int badcols = columns;
      fprintf (stderr, "supdup: bogus # columns (%d), using %d\r\n",
	       badcols, columns = 80);
  }

/*
 *if (!cursor_address)
 *  {
 *    fprintf (stderr, "Can't position cursor on this terminal.\n");
 *    exit (1);
 *  }
 */
 
  if (do_losingly_scroll) {
      inits[13] |= 01;
  }

  inits[23] = lines & 077;
  inits[22] = (lines >> 6) & 077;
  {
    int w;

    if (auto_right_margin)
      /* Brain death!  Can't write in last column of last line
       * for fear that stupid terminal will scroll up.  Glag. */
      columns = columns - 1;

    /* Silly SUPDUP spec says that should specify (1- columns) */
    w = columns - 1;
    inits[29] = w & 077;
    inits[28] = (w >> 6) & 077;
  }
  if (clr_eol)		   inits[12] |= 04;
  if (over_strike)	   inits[13] |= 010;
  if (cursor_address)	   inits[13] |= 04;
  if (unicode_translation) inits[13] |= 060; // %TOSAI + %TOSA1
  if (has_meta_key)
    {
      /* %TOFCI */
/* Don't do this -- it implies that we can generate full MIT 12-bit */
/*      inits[14] |= 010; */
      mask = 0377;
    }
  if ((delete_line || parm_delete_line) &&
      (insert_line || parm_insert_line))
    inits[14] |= 02;
  if ((delete_character || parm_dch) &&
      (insert_character || parm_ich))
    inits[14] |= 01;

  if (sup_ispeed != 0) {
    for (i = 0; i < 6; i++)
      inits[(7*6)+i] = (sup_ispeed>>((5-i)*6)) & 077;
  }
  if (sup_ospeed != 0) {
    for (i = 0; i < 6; i++)
      inits[(8*6)+i] = (sup_ospeed>>((5-i)*6)) & 077;
  }
  char *uname = getenv("USER");
  if (*uname != 0) {
    int ulen = strlen(uname);
    for (i = 0; i < ulen && i < 6; i++)
      inits[(9*6)+i] = sixbit(uname[i]);
  }
  // note that we have 9 values being sent
  inits[2] = (signed char)-9;
}

#if !USE_TERMIOS
struct	tchars notc =	{ -1, -1, -1, -1, -1, -1 };
struct	ltchars noltc =	{ -1, -1, -1, -1, -1, -1 };
#endif

int
mode (int f)
{
  static int prevmode = 0;
#if !USE_TERMIOS
  struct tchars *tc;
  struct ltchars *ltc;
  struct sgttyb sb;
#else
  struct termios ntio;
#endif
  int onoff, old;

  if (prevmode == f)
    return (f);
  old = prevmode;
  prevmode = f;
#if !USE_TERMIOS
  sb = ottyb;
#else
  memcpy(&ntio,&otio,sizeof(ntio));
#endif
  switch (f)
    {
    case 0:
      onoff = 0;
#if !USE_TERMIOS
      tc = &otc;
      ltc = &oltc;
#endif
      break;

    default:
#if !USE_TERMIOS
      sb.sg_flags |= RAW;       /* was CBREAK */
      sb.sg_flags &= ~(ECHO|CRMOD);
      sb.sg_flags |= LITOUT;
#ifdef PASS8
      sb.sg_flags |= PASS8;
#endif
      sb.sg_erase = sb.sg_kill = -1;
      tc = &notc;
      ltc = &noltc;
#else
      cfmakeraw(&ntio);
#endif
      onoff = 1;
      break;
    }
#if !USE_TERMIOS
  ioctl (fileno (stdin), TIOCSLTC, (char *) ltc);
  ioctl (fileno (stdin), TIOCSETC, (char *) tc);
  ioctl (fileno (stdin), TIOCSETP, (char *) &sb);
#else
  tcsetattr(fileno(stdin), TCSANOW, &ntio);
#endif
  ioctl (fileno (stdin), FIONBIO, &onoff);
  ioctl (fileno (stdout), FIONBIO, &onoff);
  return (old);
}

unsigned char sibuf[TBUFSIZ], *sbp;
unsigned char tibuf[TBUFSIZ], *tbp;
int scc;
int tcc;

int escape_seen;
int saved_col, saved_row;

void
restore (void)
{
  if (cursor_address)
    {
      if ((escape_seen & 1) != 0)
        {
          term_goto (0, currline);
          if (clr_eos)
            tputs (clr_eos, lines - currline, putch);
        }
      term_goto (currcol = saved_col, currline = saved_row);
    }
  escape_seen = 0;
  ttyoflush ();
}

void
clear_bottom_line (void)
{
  if (cursor_address)
    {
      currcol = 0; currline = lines - 1;
      term_goto (currcol, currline);
      if (clr_eol)
        tputs (clr_eol, columns, putch);
    }
  ttyoflush ();
}

int
read_char (void)
{
#if USE_BSD_SELECT
  int readfds;
#else
  fd_set readfds;
#endif

  while (1)
    {
      tcc = read (fileno (stdin), tibuf, 1);
      if (tcc >= 0 || errno != EWOULDBLOCK)
        {
          int c = (tcc <= 0) ? -1 : tibuf[0];
          tcc = 0; tbp = tibuf;
          return (c);
        }
      else
	{
#if USE_BSD_SELECT
	  readfds = 1 << fileno (stdin);
	  if (select(32, &readfds, 0, 0, 0, 0) < 0)
	    perror("select(read_char)");
#else
	  FD_SET(fileno(stdin),&readfds);
	  if (select(fileno(stdin)+1, &readfds, 0, 0, 0) < 0) 
	    perror("select(read_char)");
#endif
        }
    }
}


/*
 * Select from tty and network...
 */
void
supdup (char *loc)
{
  int c, ilen;
  int tin = fileno (stdin), tout = fileno (stdout);
  int on = 1;
#if !USE_BSD_SELECT
  fd_set ibits, obits;
#endif

  if (ioctl (net, FIONBIO, &on) < 0)
    perror("ioctl(FIONBIO)");

  // inits[0-5] is an AOBJN ptr for how many words follow
  ilen = -inits[2]*6+6;
  for (c = 0; c < ilen; c++) {
    *netfrontp++ = inits[c] & 077;
  }
  // netflush(0);
  /* [BV] AFTER inits! */
  if (*loc != '\0')
    do_setloc(loc);

  scc = 0;
  tcc = 0;
  escape_seen = 0;
  for (;;)
    {
#if USE_BSD_SELECT
      int ibits = 0, obits = 0;
#else
      FD_ZERO(&ibits); FD_ZERO(&obits);
#endif

      if (netfrontp != netobuf)
#if USE_BSD_SELECT
        obits |= (1 << net);
#else
        FD_SET(net, &obits);
#endif
      else
#if USE_BSD_SELECT
        ibits |= (1 << tin);
#else
        FD_SET(tin, &ibits);
#endif
      if (ttyfrontp != ttyobuf)
#if USE_BSD_SELECT
        obits |= (1 << tout);
#else
        FD_SET(tout, &obits);
#endif
      else
#if USE_BSD_SELECT
        ibits |= (1 << net);
#else
        FD_SET(net, &ibits);
#endif
      if (scc < 0 && tcc < 0)
        break;
#if USE_BSD_SELECT
      if (select (16, &ibits, &obits, 0, 0) < 0)
	perror("select");
      if (ibits == 0 && obits == 0)
	{
          sleep (5);
          continue;
        }
#else
      {
	int nfds = 0;
	struct timeval tmo = { 60, 0 };	/* sleep max one minute */
	nfds = select(16, &ibits, &obits, 0, &tmo);
	if (nfds < 0)
	{
	  perror("select");
          sleep (5);		/* nice error handling? */
          continue;
        } else if (nfds == 0) {
	  /* dummy command just to avoid timeout at other end */
	  /* #### not very friendly to the ITS - better fix that end */
	  *netfrontp++ = SUPDUP_ESCAPE;
	  *netfrontp++ = SUPDUP_LOCATION+040;
	  netflush(0);
	  continue;
	}
      }
#endif

      /*
       * Something to read from the network...
       */
#if USE_BSD_SELECT
      if ((escape_seen == 0) && (ibits & (1 << net)))
#else
      if ((escape_seen == 0) && (FD_ISSET(net, &ibits)))
#endif
        {
          scc = read (net, sibuf, sizeof (sibuf));
          if (scc < 0 && errno == EWOULDBLOCK)
            scc = 0;
          else
            {
              if (scc <= 0) {
		if (scc < 0) perror("read from net failed");
                break;
	      }
              sbp = sibuf;
              if (indebug_file)
                fwrite(sibuf, scc, 1, indebug_file);
            }
        }

      /*
       * Something to read from the tty...
       */
#if USE_BSD_SELECT
      if (ibits & (1 << tin))
#else
      if (FD_ISSET(tin, &ibits))
#endif
        {
          tcc = read (tin, tibuf, sizeof (tibuf));
          if (tcc < 0 && errno == EWOULDBLOCK)
            tcc = 0;
          else
            {
              if (tcc <= 0) {
		if (tcc < 0) perror("read from terminal failed");
                break;
	      }
              tbp = tibuf;
            }
        }

      while (tcc > 0)
        {
          int c;

          if ((&netobuf[sizeof(netobuf)] - netfrontp) < 2)
            break;
          c = *tbp++ & mask; tcc--;
          if (escape_seen > 2)
            {
              /* ``restore'' the screen (or at least the cursorpos) */
              restore ();
            }
          else if (escape_seen > 0)
            {
              escape_seen = escape_seen + 2;
              command (c);
              continue;
            }

          if (c == escape_char)
            {
              escape_seen = (tcc == 0) ? 1 : 2;
              saved_col = currcol;
              saved_row = currline;
              if (tcc == 0)
                {
                  clear_bottom_line ();
                  fprintf (stdout, "SUPDUP %s command -> ", hostname);
                  ttyoflush ();
                }
              continue;
            }

          if (c & 0200)
            {
              high_bits = 2;
              c &= 0177;
            }
          if (c == ITP_ESCAPE)
            *netfrontp++ = ITP_ESCAPE;
          else if (high_bits)
            {
              *netfrontp++ = ITP_ESCAPE;
              *netfrontp++ = high_bits + 0100;
              high_bits = 0;
            }
          *netfrontp++ = c;
        }
#if USE_BSD_SELECT
      if ((obits & (1 << net)) && (netfrontp != netobuf))
#else
      if (FD_ISSET(net, &obits) && (netfrontp != netobuf))
#endif
        netflush (0);
      if (scc > 0)
        suprcv ();
#if USE_BSD_SELECT
      if ((obits & (1 << tout)) && (ttyfrontp != ttyobuf))
#else
      if (FD_ISSET(tout, &obits) && (ttyfrontp != ttyobuf))
#endif
        ttyoflush ();
    }
}


void
command (unsigned char chr)
{
  struct cmd *c;

  /* flush typeahead */
  tcc = 0;
  if (chr == escape_char)
    {
      *netfrontp++ = chr;
      restore ();
      return;
    }

  for (c = cmdtab; c->name; c++)
    if (c->name == chr)
      break;

  if (!c->name && (chr >= 'A' && chr <= 'Z'))
    for (c = cmdtab; c->name; c++)
      if (c->name == (chr - 'A' + 'a'))
        break;

  if (c->name)
    (*c->handler) ();
  else if (chr == '\177' || chr == Ctl ('g'))
    restore ();
  else if (chr == Ctl ('z'))
    suspend ();
  else if (chr == Ctl ('h'))
    help ();
  else if (chr == 'q')
    rlogout ();
  else
    {
      clear_bottom_line ();
      printf ("?Invalid SUPDUP command \"%s\"",
              key_name (chr));
      ttyoflush ();
      return;
    }
  ttyoflush ();
  if (!connected)
    exit (1);
  return;
}

void
status (void)
{
  if (cursor_address)
    {
      currcol = 0; currline = lines - 3;
      term_goto (currcol, currline);
      if (clr_eos)
        tputs (clr_eos, lines - currline, putch);
    }
  ttyoflush ();
  if (connected)
    printf ("Connected to %s.", hostname);
  else
    printf ("No connection.");
  /* [BV] show location */
  if (myloc[0] != '\0')
    printf(" (Console location: \"%s\")", myloc);
  ttyoflush ();
  put_newline ();
  printf ("Escape character is \"%s\".", key_name (escape_char));
  ttyoflush ();
  /* eat space, or unread others */
  {
    int c;
    c = read_char ();
    restore ();
    if (c < 0)
      return;
    if (c == ' ')
      return;
    /* unread-char */
    tibuf[0] = c; tcc = 1; tbp = tibuf;
  }
}

void
suspend (void)
{
  int save;

  if (cursor_home)
    tputs (cursor_home, 1, putch);
  else if (cursor_address)
    term_goto (0, 0);
  if (clr_eol)
    tputs (clr_eol, columns, putch);
  ttyoflush ();
  save = mode (0);
  if (!cursor_address)
    putchar ('\n');
  kill (0, SIGTSTP);
  /* reget parameters in case they were changed */
#if !USE_TERMIOS
  ioctl (0, TIOCGETP, (char *) &ottyb);
  ioctl (0, TIOCGETC, (char *) &otc);
  ioctl (0, TIOCGLTC, (char *) &oltc);
#else
  tcsetattr(0, TCSANOW, &otio);
#endif
  (void) mode (save);
  *netfrontp++ = ITP_ESCAPE;      /* Tell other end that it sould refresh */
  *netfrontp++ = ITP_PIATY;       /* the screen */
  restore ();
}

/*
 * Help command.
 */
void
help (void)
{
  struct cmd *c;
  
  if (cursor_address)
    {
      for (c = cmdtab, currline = lines - 1 ;
           c->name;
           c++, currline--)
        ;
      currcol = 0;
      currline--;                   /* For pass-through `command' doc */
      term_goto (currcol, currline);
      if (clr_eos)
        tputs (clr_eos, lines - currline, putch);
    }
      
  ttyoflush ();
  printf ("Type \"%s\" followed by the command character.  Commands are:",
          key_name (escape_char));
  ttyoflush ();
  put_newline ();
  printf (" %-8s%s",
          key_name (escape_char),
          "sends escape character through");
  ttyoflush ();
  for (c = cmdtab; c->name; c++)
    {
      put_newline ();
      printf (" %-8s%s",
              key_name (c->name),
              c->help);
    }
  ttyoflush ();

  {
    int c;
    c = read_char ();
    restore ();
    if (c < 0)
      return;
    if (c == ' ')
      return;
    /* unread-char */
    tibuf[0] = c; tcc = 1; tbp = tibuf;
  }
  return;
}

/* [BV] send "set console location" (see rfc734) */
void
do_setloc(char *loc)
{
  *netfrontp++ = SUPDUP_ESCAPE;
  *netfrontp++ = SUPDUP_LOCATION;
  while (*loc != '\0' && *loc != '\r' && *loc != '\n')
    *netfrontp++ = *loc++;
  *netfrontp++ = '\0';
}

/* [BV] set location command */
/* This turns out to be of limited use, since TELSER in ITS doesn't null-terminate
   the sent string; thus you can only make it longer.  Use :TTLOC in ITS instead. */
void
setloc(void)
{
  char c, loc[128];
  int i;

  clear_bottom_line();
  tcc = 0;
  if (myloc[0] != '\0')
    fprintf(stdout,"Set console location (currently \"%s\"): ",myloc);
  else
    fprintf(stdout,"Set console location: ");
  ttyoflush();
  for (i = 0; i < sizeof(loc); i++) {
    c = read_char();
    if (c == Ctl ('g')) {	/* abort */
      restore ();
      return;
    }
    if (c == '\177' && i > 0) {
      loc[--i] = '\0';
      if (0 && delete_character) { /* I don't know what I'm doing here */
	tputs(delete_character, 1, putch);
	ttyoflush();
      } else {
	fputs("\b \b",stdout);	/* "works" */
      }
      continue;
    }
    if (c == '\n' || c == '\r')	/* done */
      break;
    loc[i] = c;
    fputc(c,stdout);		/* echo */
  }
  loc[i] = '\0';		/* terminate */
  do_setloc(loc);
  netflush(1);
#ifdef __OpenBSD__
  strlcpy(myloc,loc, sizeof(myloc));		/* save */
#else
  strcpy(myloc,loc);		/* save */
#endif /* __OpenBSD__ */
  restore();    
}

void
punt (int logout_p)
{
  int c;

  clear_bottom_line ();
  /* flush typeahead */
  tcc = 0;
  fprintf (stdout, "Quit (and %s from %s)? ",
           logout_p ? "logout" : "disconnect",
           hostname);
  ttyoflush ();
  while (1)
    {
      c = read_char ();
      if (c == 'y' || c == 'Y')
        break;
      else if (c == 'n' || c == 'N' || c == '\177' || c == Ctl ('g'))
        {
          restore ();
          return;
        }
    }
  if (logout_p)
    {
      netflush (1);
      *netfrontp++ = SUPDUP_ESCAPE;
      *netfrontp++ = SUPDUP_LOGOUT;
      netflush (1);
    }
  if (cursor_home)
    tputs (cursor_home, 1, putch);
  else if (cursor_address)
    term_goto (0, 0);
  ttyoflush ();
  (void) mode (0);
  if (!cursor_address)
    putchar ('\n');
  if (connected)
    {
      shutdown (net, 2);
      // cosmetics
      if (clr_eol)
	tputs (clr_eol, columns, putch);
      ttyoflush ();
      printf ("Connection closed.\n");
      // cosmetics
      if (clr_eol)
	tputs (clr_eol, columns, putch);
      ttyoflush ();
      close (net);
    }
  exit (0);
}

void
quit (void)
{
  punt (0);
}

void
rlogout (void)
{
  punt (1);
}


/*
 * Supdup receiver states for fsm
 */
#define	SR_DATA		0
#define	SR_M0		1
#define	SR_M1		2
#define	SR_M2		3
#define	SR_M3		4
#define	SR_QUOTE	5
#define	SR_IL		6
#define	SR_DL		7
#define	SR_IC		8
#define	SR_DC		9

void
suprcv (void)
{
  int c;
  static int state = SR_DATA;
  static int y;
  // The text until the first TDNOP should be shown in ASCII, without translation.
  // See RFC 734, bottom of page 3, or SUPIN2 in SYSNET;SUPDUP > (for ITS).
  static int greeting_done = 0;

  while (scc > 0)
    {
      c = *sbp++ & 0377; scc--;
//      if(c>=0&&c<128)
//        printf("State: %d, Incoming: %d. Translation: %s\n", state, c, charmap[c].name);
      if (debug) {
	if (c < 0200) {
	  if (greeting_done)
	    fprintf(stderr,"State: %d, Incoming: %#o. Translation: %s\r\n", state, c, charmap[c].name);
	  else
	    fprintf(stderr,"State: %d, Incoming: %#o.\r\n", state, c);
	} else
	  fprintf(stderr,"State: %d, Incoming: %#o (TD%s)\r\n", state, c,
		  c == TDCRL ? "CRL" : c == TDNOP ? "NOP" : c == TDEOL ? "EOL" : c == TDCLR ? "CLR" : "xxx");
	if (c == TDCLR) c = TDNOP;
      }
      switch (state)
        {
        case SR_DATA:
          if ((c & 0200) == 0)
            {
              if (currcol < columns)
                {
                  currcol++;
                  if(unicode_translation && greeting_done) {
                    char *s = charmap[c].utf8;
                    while(*s) {
                      *ttyfrontp++ = *s++;
                    }
                  }
                  else {
                    *ttyfrontp++ = c;
                  }
                }
              else
                {
                  /* Supdup (ITP) terminals should `stick' at the end
                     of `long' lines (ie not do TERMCAP `am') */
                }
              continue;
            }
          else switch (c)
            {
            case TDMOV:
              state = SR_M0;
              continue;
            case TDMV1:
            case TDMV0:
              state = SR_M2;
              continue;
            case TDEOF:
              if (clr_eos)
                tputs (clr_eos, lines - currline, putch);
              continue;
            case TDEOL:
              if (clr_eol)
                tputs (clr_eol, columns - currcol, putch);
              continue;
            case TDDLF:
              putch (' ');
              goto foo;
            case TDBS:
              currcol--;
            foo:
              if (currcol < 0)
                currcol = 0;
              else if (cursor_left)
                tputs (cursor_left, 0, putch);
              else if (cursor_address)
                term_goto (currcol, currline);
              continue;
            case TDCR:
              currcol = 0;
              if (carriage_return)
                tputs (carriage_return, 0, putch);
              else if (cursor_address)
                term_goto (currcol, currline);
              else
                putch ('\r');
              continue;
            case TDLF:
              currline++;
              if (currline >= lines)
                currline--;
              else if (cursor_down)
                tputs (cursor_down, 0, putch);
              else if (cursor_address)
                term_goto (currcol, currline);
              else
                putch ('\n');
              continue;
            case TDCRL:
              put_newline ();              currcol = 0;
              currline++;
              if (clr_eol)
                tputs (clr_eol, columns - currcol, putch);
              continue;
            case TDNOP:
	      greeting_done = 1;
              continue;
            case TDORS:         /* ignore interrupts and */
	      if (debug) fprintf(stderr,"TDORS at %d %d\n", currline, currcol);
	      netflush (0);     /* send cursorpos back every time */
	      *netfrontp++ = ITP_ESCAPE;
	      *netfrontp++ = ITP_CURSORPOS;
	      *netfrontp++ = ((unsigned char) currline);
	      *netfrontp++ = ((unsigned char) currcol);
	      netflush (0);
              continue;
            case TDQOT:
              state = SR_QUOTE;
              continue;
            case TDFS:
              if (currcol < columns)
                {
                  currcol++;
                  if (cursor_right)
                    tputs (cursor_right, 1, putch);
                  else if (cursor_address)
                    term_goto (currcol, currline);
                  else
                    currcol--;
                }
              continue;
            case TDCLR:
              currcol = 0;
              currline = 0;
              if (clear_screen)
                tputs (clear_screen, lines, putch);
              else
                {
                  if (cursor_home)
                    tputs (cursor_home, 1, putch);
                  else if (cursor_address)
                    term_goto (0, 0);
                  if (clr_eos)
                    tputs (clr_eos, lines, putch);
                }
              continue;
            case TDBEL:
              if (flash_screen)
                tputs (flash_screen, 0, putch);
              else if (bell)
                tputs (bell, 0, putch);
              else
                /* >>>> ?? */
                putch ('\007');
              continue;
            case TDILP:
              state = SR_IL;
              continue;
            case TDDLP:
              state = SR_DL;
              continue;
            case TDICP:
              state = SR_IC;
              continue;
            case TDDCP:
              state = SR_DC;
              continue;
            case TDBOW:
              if (enter_standout_mode)
                tputs (enter_standout_mode, 0, putch);
              continue;
            case TDRST:
              if (exit_standout_mode)
                tputs (exit_standout_mode, 0, putch);
              continue;
            default:
              ttyoflush ();
              fprintf (stderr, ">>>bad supdup opcode %o ignored<<<", c);
              ttyoflush ();
              if (cursor_address)
                term_goto (currcol, currline);
            }
        case SR_M0:
          state = SR_M1;
          continue;
        case SR_M1:
          state = SR_M2;
          continue;
        case SR_M2:
          y = c;
          state = SR_M3;
          continue;
        case SR_M3:
          if (c < columns && y < lines)
            {
              currcol = c; currline = y;
              term_goto (currcol, currline);
            }
          state = SR_DATA;
          continue;
        case SR_QUOTE:
          if(unicode_translation) {
            char *s = charmap[c].utf8;
            while(*s) {
              putch(*s++);
            }
          }
          else {
            putch (c);
          }
          state = SR_DATA;
          continue;
        case SR_IL:
          if (parm_insert_line)
            {
              tputs (tparm (parm_insert_line, c), c, putch);
            }
          else
            if (insert_line)
              while (c--)
                tputs (insert_line, 1, putch);
          state = SR_DATA;
          continue;
        case SR_DL:
          if (parm_delete_line)
            {
              tputs (tparm (parm_delete_line, c), c, putch);
            }
          else
            if (delete_line)
              while (c--)
                tputs (delete_line, 1, putch);
          state = SR_DATA;
          continue;
        case SR_IC:
          if (parm_ich)
            {
              tputs (tparm (parm_ich, c), c, putch);
            }
          else
            if (insert_character)
              while (c--)
                tputs (insert_character, 1, putch);
          state = SR_DATA;
          continue;
        case SR_DC:
          if (parm_dch)
            {
              tputs (tparm (parm_dch, c), c, putch);
            }
          else
            if (delete_character)
              while (c--)
                tputs (delete_character, 1, putch);
          state = SR_DATA;
          continue;
        }
    }
}

void
ttyoflush (void)
{
  int n;
  unsigned char *back = ttyobuf;

  fflush (stdout);
  while ((n = ttyfrontp - back) > 0)
    {
	if (tdebug_file) {
	    fprintf(tdebug_file, "Outraw %d:[", n);
	    fwrite(back, n, 1, tdebug_file);
	    fprintf(tdebug_file, "]\n");
	}

      n = write (fileno (stdout), back, n);
/*      fflush (stdout); */
      if (n >= 0)
        back += n;
      else
        if (errno  == EWOULDBLOCK)
          continue;
      else
        /* Here I am being a typical un*x programmer and just
           ignoring other error codes.
           I really hate this environment!
         */
        return;
    }
  ttyfrontp = ttyobuf;
}

void
netflush (int dont_die)
{
  int n, m;
  unsigned char *back = netobuf;

  while ((n = netfrontp - back) > 0)
    {
      m = write (net, back, n);
      if (m < 0)
        {
	  if (debug) perror("write");
          if (errno == ENOBUFS || errno == EWOULDBLOCK)
            return;
          if (dont_die)
            return;
          (void) mode (0);
          perror (hostname);
          close (net);
          longjmp (peerdied, -1);
          /*NOTREACHED*/
        }
      else if (n != m)
	fprintf(stderr,"should write %d, wrote %d\n", n, m);
      n = m;
      if (outdebug_file)
	fwrite(back, n, 1, outdebug_file);
      back += n;
    }
  netfrontp = netobuf;
}


char key_name_buffer[20];

char *
key_name (int c)
{
  char *p = key_name_buffer;
  if (c >= 0200)
    {
      *p++ = 'M';
      *p++ = '-';
      c -= 0200;
    }
  if (c < 040)
    {
      if (c == 033)
	{
	  *p++ = 'E';
	  *p++ = 'S';
	  *p++ = 'C';
	}
      else if (c == Ctl ('I'))
	{
	  *p++ = 'T';
	  *p++ = 'A';
	  *p++ = 'B';
	}
      else if (c == Ctl ('J'))
	{
	  *p++ = 'L';
	  *p++ = 'F';
	  *p++ = 'D';
	}
      else if (c == Ctl ('M'))
	{
	  *p++ = 'R';
	  *p++ = 'E';
	  *p++ = 'T';
	}
      else
	{
	  *p++ = 'C';
	  *p++ = '-';
	  if (c > 0 && c <= Ctl ('Z'))
	    *p++ = c + 0140;
	  else
	    *p++ = c + 0100;
	}
    }
  else if (c == 0177)
    {
      *p++ = 'D';
      *p++ = 'E';
      *p++ = 'L';
    }
  else if (c == ' ')
    {
      *p++ = 'S';
      *p++ = 'P';
      *p++ = 'C';
    }
  else
    *p++ = c;
  *p++ = 0;
  return (key_name_buffer);
}

void deadpeer(int sig)
{
  mode (0);
  longjmp (peerdied, -1);
}

void intr (int sig)
{
  mode (0);
  exit (1);
}

void
top (void)
{
  high_bits |= 020;
  restore ();
}

/*
 * Set the escape character.
 */
void
setescape (void)
{
  clear_bottom_line ();
  printf ("Type new escape character: ");
  ttyoflush ();
  escape_char = read_char ();
  clear_bottom_line ();
  printf ("Escape character is \"%s\".", key_name (escape_char));
  ttyoflush ();
}

#if 0
setoptions (void)
{
  showoptions = !showoptions;
  clear_bottom_line ();
  printf ("%s show option processing.", showoptions ? "Will" : "Wont");
  ttyoflush ();
}
#endif /* 0 */

#if 0
/* >>>>>> ???!! >>>>>> */
void
setcrmod (void)
{
  crmod = !crmod;
  clear_bottom_line ();
  printf ("%s map carriage return on output.", crmod ? "Will" : "Wont");
  ttyoflush ();
}
#endif /* 0 */

#if 0
void
setdebug (void)
{
  debug = !debug;
  clear_bottom_line ();
  printf ("%s turn on socket level debugging.", debug ? "Will" : "Wont");
  if (debug && net > 0 && setsockopt (net, SOL_SOCKET, SO_DEBUG, 0, 0) < 0)
    perror ("setsockopt (SO_DEBUG)");
  ttyoflush ();
}
#endif /* 0 */

/*
 * Local Variables:
 * c-basic-offset: 2
 * End:
 */
