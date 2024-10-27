/*
 * Characters output from user programs are checked for \b, \r, and \n.
 * These are translated into SUPDUP cursor movement.  Thus this disallows
 * the use of these 3 SAIL characters.
 * SUPDUP display codes are passed through, but looked at to determine
 * the current cursor position.  This is necessary for simulating \b, \n, \r
 * and the TDBS and TDUP fake SUPDUP codes supplied in the termcap entries.
 * Input from the network is checked for ITP escapes, mainly for bucky
 * bits and terminal location string.
 */

/*  Losing unix doesn't know about the -real- control bit

 * there should be some way to conditionalize this on the basis
 * of %TOFCI -- except that the existing supdup server loses this information!
 * It isn't clear-cut what to do in the server, as %tofci means that the user
 * can generate full 9-bit MIT characters, which isn't what the `km' termcap
 * flag means.  On the other hand, being able to generate 8-bit characters
 * (which is sort of what `km' is) isn't the same as %tofci.
 * I think the problem is fundamental and cultural and irresolvable.

 * unix supdup server uses 0237 as a control escape.
 * c-a		001
 * m-a		341
 * c-m-a	201
 * c-1		237 061
 * m-1		261
 * c-m-1	237 261
 * c-m-_	237 237
 */

/*
 * Since various parts of UNIX just can't hack the 0200 bit, we define
 * a fake lead-in escape - 0177, to avoid a sail graphic that is used
 * sometimes...
 */
#define KLUDGE_ESCAPE 0177

/* ITP bits (from the 12-bit character set */
#define ITP_ASCII	00177	/* ascii part of character */
#define ITP_CTL		00200	/* CONTROL key depressed */
#define ITP_MTA		00400	/* META key depressed */
#define ITP_TOP		04000	/* TOP key depressed */
#define CTL_PREFIX	0237	/* c-m-_ is control prefix */
#define ITP_CHAR(char1,char2) ((((char1) & 037) << 7) + (char2))
#define ITP_ESCAPE	034	/* ITS ITP codes follow */

#define ITP_CURSORPOS 020	/* user sends vpos/hpos */
#define ITP_FLOW_CONTROL_START 032 /* Set buffer to zero -- ignored */
#define ITP_FLOW_CONTROL_INCREASE 001 /* increase buffer size -- ignored */
#define ITP_FLOW_CONTROL_END 011 /* Set buffer to infinity -- ignored */
#define ITP_PIATY 003		/* user says her screen is messed-up
				 we don't (can't) hack this */
#define ITP_STOP_OUTPUT 023	/* Ignore it */
#define ITP_RESTART_OUTPUT 022  /* Ignore it */

#define ASCII_CTL_MASK	~(0177-037)
#define ASCII_ESCAPE	033
#define ASCII_PART(char) (char & ITP_ASCII)

#define SUPDUP_ESCAPE	0300
#define SUPDUP_LOGOUT	0301
#define SUPDUP_LOCATION	0302
#define SUPDUP_ESCAPE_KEY	04101
#define SUPDUP_SUSPEND_KEY	04102
#define SUPDUP_CLEAR_KEY	04103
#define SUPDUP_HELP_KEY		04110

#define TDMOV	0200
#define TDMV1	0201	/* not defined in supdup spec AIM 644 */
#define TDEOF	0202
#define TDEOL	0203
#define TDDLF	0204
#define TDCRL	0207
#define TDNOP	0210
#define TDBS	0211	/* not defined in supdup spec AIM 644 */
#define TDLF	0212	/* not defined in supdup spec AIM 644 */
#define TDCR	0213	/* not defined in supdup spec AIM 644 */
#define TDORS	0214
#define TDQOT	0215
#define TDFS	0216
#define TDMV0	0217
#define TDCLR	0220
#define TDBEL	0221
#define TDILP	0223
#define TDDLP	0224
#define TDICP	0225
#define TDDCP	0226
#define TDBOW	0227
#define TDRST	0230
#define TDGRF	0231
#define TDSCU	0232	/* Scroll region up */
#define TDSCD	0233	/* Scroll region down */

#define TDUP	0237	/* Interpreted locally, not in supdup spec at all */

#ifndef USE_CHAOS_STREAM_SOCKET
#define USE_CHAOS_STREAM_SOCKET 1
#endif

extern int chaos_connect(const char *host, const char *contact);
extern int tcp_connect(const char *host, const char *port);
