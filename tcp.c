/* TCP specific code, pulled out from supdup.c. */

#include <stdio.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define STANDARD_PORT 95  /*Per gospel from St. Postel.*/

static int
get_port(struct sockaddr_in *tsin, const char *port)
{
  if (port == NULL) {
    struct servent *sp = getservbyname ("supdup", "tcp");
    if (sp == NULL)
      tsin->sin_port = htons(STANDARD_PORT);
    else
      tsin->sin_port = sp->s_port;
  } else {
    tsin->sin_port = atoi (port);
    if (tsin->sin_port <= 0) {
      fprintf(stderr,"%s: bad port number.\n", port);
      return -1;
    }
    tsin->sin_port = htons (tsin->sin_port);
  }
  return 0;
}

static int
get_host (struct sockaddr_in *tsin, const char *name)
{
  struct hostent *host;
  host = gethostbyname (name);
  if (host)
    {
      tsin->sin_family = host->h_addrtype;
#ifdef notdef
      bcopy (host->h_addr_list[0], (caddr_t) &tsin->sin_addr, host->h_length);
#else
      bcopy (host->h_addr, (caddr_t) &tsin->sin_addr, host->h_length);
#endif /* h_addr */
      return 0;
    }
  else
    {
      tsin->sin_family = AF_INET;
      tsin->sin_addr.s_addr = inet_addr (name);
      if (tsin->sin_addr.s_addr == -1)
        return -1;
      else
        return 0;
    }
}

int
tcp_connect(const char *host, const char *port)
{
  struct sockaddr_in tsin;
  int fd;

  if (get_port(&tsin, port) < 0 || get_host(&tsin, host) < 0)
    return -1;

  fd = socket (AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    perror ("supdup: socket");
    return -1;
  }

  if (connect (fd, (struct sockaddr *) &tsin, sizeof (tsin)) < 0) {
    close(fd);
    perror ("supdup: connect");
    return -1;
  }

  return fd;
}
