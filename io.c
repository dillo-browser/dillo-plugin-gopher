#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "io.h"

int read_all(int fd, char *buf, size_t len) {
	ssize_t nbytes;
	while (len > 0) {
		nbytes = read(fd, buf, len);
		if (nbytes == 0) { errno = EPIPE; return -1; }
		if (nbytes < 0 && errno == EINTR) continue;
		if (nbytes < 0) return -1;
		buf += nbytes;
		len -= nbytes;
	}
	return 0;
}

int read_some(int fd, char *buf, size_t *lenp) {
	ssize_t nbytes;
	do nbytes = read(fd, buf, *lenp);
	while (nbytes < 0 && errno == EINTR);
	if (nbytes == 0) { errno = EPIPE; return -1; }
	if (nbytes < 0) return -1;
	*lenp = nbytes;
	return 0;
}

int write_all(int fd, const char *buf, size_t count) {
	ssize_t nbytes;
	while (count > 0) {
		nbytes = write(fd, buf, count);
		if (nbytes < 0 && errno == EINTR) continue;
		if (nbytes < 0) return -1;
		buf += nbytes;
		count -= nbytes;
	}
	return 0;
}

ssize_t read_file(char *buf, size_t len, const char *fmt, ...) {
	va_list ap;
	int rc;
	struct stat st;
	int fd;

	va_start(ap, fmt);
	rc = vsnprintf(buf, len, fmt, ap);
	va_end(ap);
	if (rc < 0) return -1;
	if ((size_t)rc >= len) { errno = ENAMETOOLONG; return -1; }

	rc = stat(buf, &st);
	if (rc < 0) return -1;
	if ((size_t)st.st_size > len-1) { errno = EMSGSIZE; return -1; }

	fd = open(buf, O_RDONLY);
	if (fd < 0) return -1;

	rc = read_all(fd, buf, st.st_size);
	if (rc < 0) return -1;

	buf[(size_t)st.st_size] = '\0';

	close(fd);
	return st.st_size;
}

int tcp_connect(const char *host, const char *port, const char **errp) {
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	int s;
	int fd;
	int err;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_protocol = IPPROTO_TCP;

	if (host && host[0] == '\0') host = NULL;
	s = getaddrinfo(host, port, &hints, &result);
	if (s < 0) {
		*errp = gai_strerror(s);
		return -1;
	}

	for (rp = result; rp; rp = rp->ai_next) {
		fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (fd < 0) continue;
		if (connect(fd, rp->ai_addr, rp->ai_addrlen) == 0) break;
		err = errno;
		close(fd);
		errno = err;
	}
	if (rp == NULL) fd = -1;

	freeaddrinfo(result);
	return fd;
}
