/* Chaosnet specific code, pulled out from supdup.c. */

#if USE_CHAOS_STREAM_SOCKET

#include <stdio.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <sys/un.h>
#include <sys/errno.h>

// Where are the chaos sockets? Cf. https://github.com/bictorv/chaosnet-bridge
#ifndef CHAOS_SOCKET_DIRECTORY
#define CHAOS_SOCKET_DIRECTORY "/tmp"
#endif
// What DNS domain should be used to translate Chaos addresses to names?
#ifndef CHAOS_ADDRESS_DOMAIN
#define CHAOS_ADDRESS_DOMAIN "ch-addr.net"
#endif
// What DNS server should be used to fetch Chaos class data?
#ifndef CHAOS_DNS_SERVER
// #define CHAOS_DNS_SERVER "130.238.19.25"
#define CHAOS_DNS_SERVER "dns.chaosnet.net"
#endif

static int
connect_to_named_socket(int socktype, char *path)
{
  int sock, slen;
  struct sockaddr_un server;
  
  if ((sock = socket(AF_UNIX, socktype, 0)) < 0) {
    perror("socket(AF_UNIX)");
    return -1;
  }

  server.sun_family = AF_UNIX;
  sprintf(server.sun_path, "%s/%s", CHAOS_SOCKET_DIRECTORY, path);
  slen = strlen(server.sun_path)+ 1 + sizeof(server.sun_family);

  if (connect(sock, (struct sockaddr *)&server, slen) < 0) {
    if (errno != ECONNREFUSED)
      // Fail silently if Chaosnet bridge is not there
      perror("connect(server)");
    return -1;
  }
  return sock;
}

static ssize_t write_all(int fd, void *buf, size_t n)
{
  char *x = buf;
  ssize_t m;
  while (n > 0) {
    m = write(fd, x, n);
    if (m == 0)
      break;
    if (m < 0)
      return m;
    x += m;
    n -= m;
  }    
  return x - (char *)buf;
}

static ssize_t read_all(int fd, void *buf, size_t n)
{
  char *x = buf;
  ssize_t m;
  while (n > 0) {
    m = read(fd, x, n);
    if (m == 0)
      break;
    if (m < 0)
      return m;
    x += m;
    n -= m;
  }    
  return x - (char *)buf;
}

static int
connection(int net, const char *host, const char *contact) 
{
  char buf[1000]; /*Bill Gates says this ought to be enough.*/
  char *bp, cbuf[2];
  size_t n;

  n = snprintf(buf, sizeof buf, "RFC %s %s\r\n", host, contact);
  if (write_all(net, buf, n) < n)
    return -1;

  bp = buf;
  while (read_all(net, cbuf, 1) == 1) {
    if ((cbuf[0] != '\r') && (cbuf[0] != '\n'))
      *bp++ = cbuf[0];
    else {
      *bp = '\0';
      break;
    }
  }
  if (strncmp(buf,"OPN ", 4) == 0)
    return 0;
  else
    return -1;
}

int
chaos_connect(const char *hostname, const char *contact)
{
  int fd = connect_to_named_socket(SOCK_STREAM, "chaos_stream");
  if (contact == NULL)
    contact = "SUPDUP";
  if (connection(fd, hostname, contact) < 0) {
    close(fd);
    return -1;
  }
  return fd;
}

#else /* !USE_CHAOS_STREAM_SOCKET */

int
chaos_connect(const char *hostname, const char *contact)
{
  (void)hostname;
  (void)contact;
  return -1;
}

#endif /* !USE_CHAOS_STREAM_SOCKET */
