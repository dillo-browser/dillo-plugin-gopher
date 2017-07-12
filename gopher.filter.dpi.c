#include <err.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>

#include "dpi.h"
#include "io.h"

static void respond_errstr(const char *errstr, const char *fmt, ...) {
	va_list ap;
	int err = errno;
	va_start(ap, fmt);
	dpi_send_header(NULL, "text/plain");
	vprintf(fmt, ap);
	printf(": %s", errstr ? errstr : strerror(err));
	va_end(ap);
}

static void respond_err(const char *fmt, ...) {
	va_list ap;
	int err = errno;
	va_start(ap, fmt);
	dpi_send_header(NULL, "text/plain");
	vprintf(fmt, ap);
	printf(": %s", strerror(err));
	va_end(ap);
}

static void respond_errx(const char *fmt, ...) {
	va_list ap;
	int err = errno;
	va_start(ap, fmt);
	dpi_send_header(NULL, "text/plain");
	vprintf(fmt, ap);
	va_end(ap);
}

static int parse_type_path(const char *url, char *type, char *path, size_t path_len) {
	if (!*url) {
		*path = '\0';
		*type = '\0';
		return 0;
	}
	*type = *url++;
	while (path_len > 0 && *url != '\0') {
		*path++ = *url++;
		path_len--;
	}
	if (path_len <= 0) {
		errno = EMSGSIZE;
		return -1;
	}
	*path = '\0';
	return 0;
}

static int parse_port_type_path(const char *url, char *port, size_t port_len, char *type, char *path, size_t path_len) {
	while (port_len > 0 && *url != '\0' && *url != '/') {
		*port++ = *url++;
		port_len--;
	}
	if (port_len <= 0) {
		errno = EMSGSIZE;
		return -1;
	}
	*port = '\0';
	if (*url == '/') return parse_type_path(url + 1, type, path, path_len);
	*path = '\0';
	*type = '\0';
	return 0;
}

static int parse_url(const char *url, char *host, size_t host_len, char *port, size_t port_len, char *type, char *path, size_t path_len) {
	if (!strncmp(url, "gopher:", 7)) url += 7;
	if (!strncmp(url, "//", 2)) url += 2;

	while (host_len > 0 && *url != '\0' && *url != '/' && *url != ':') {
		*host++ = *url++;
		host_len--;
	}
	if (host_len <= 0) {
		errno = EMSGSIZE;
		return -1;
	}
	*host = '\0';
	if (*url == ':') return parse_port_type_path(url + 1, port, port_len, type, path, path_len);
	*port = '\0';
	if (*url == '/') return parse_type_path(url + 1, type, path, path_len);
	*path = '\0';
	return 0;
}

static void parse_line(char *line, char *type, char **title, char **selector, char **host, char **port) {
	*type = *line;
	if (!*line) {
		*title = *selector = *host = *port = NULL;
		return;
	}

	*title = ++line;
	while (*line && *line != '\t') line++;
	if (!*line) {
		*selector = *host = *port = NULL;
		return;
	}
	*line++ = '\0';

	*selector = line;
	while (*line && *line != '\t') line++;
	if (!*line) {
		*host = *port = NULL;
		return;
	}
	*line++ = '\0';

	*host = line;
	while (*line && *line != '\t') line++;
	if (!*line) {
		*port = NULL;
		return;
	}
	*line++ = '\0';

	*port = line;
	while (*line && *line >= '0' && *line <= '9') line++;
	*line = '\0';
}

static int putchar_htmlenc(char c) {
	switch (c) {
		case '<': return printf("&lt;");
		case '>': return printf("&gt;");
		case '"': return printf("&quot;");
		default: return putchar(c);
	}
}

static void print_htmlenc(const char *str) {
	char c;
	while ((c = *str++)) putchar_htmlenc(c);
}

static void render_line_info(const char *title) {
	printf("<tr><td></td><td><pre>");
	print_htmlenc(title);
	printf("</pre></td></tr>\n");
}

static void render_line_unknown(char type, const char *line) {
	printf("<tr><td>");
	putchar_htmlenc(type);
	printf("</td><td><pre>");
	print_htmlenc(line + 1);
	printf("</pre></td></tr>\n");
}

static void render_line_empty() {
	printf("<tr><td colspan=2></td></tr>\n");
}

static void render_line_line() {
	printf("<tr><td colspan=2><hr></td></tr>\n");
}

static const char *icon(char type) {
	switch (type) {
		case '0': // text
			return "<img src=\"data:image/gif;base64,R0lGODdhFAAXAPAAMf///wAAACwAAAAAFAAXAAACOYSPqcvtD02YtFYV4Ez55S5J1gZ0INicoWixVBmNpry80WG7JLfftozC9WK9UW2oIQGPwSTw+YI+CwA7\">";
		case '1': // menu
			return "<img src=\"data:image/gif;base64,R0lGODdhFAAXAPAAMf///wAAACwAAAAAFAAXAAACOYSPqcvtD1OYNAZ1XcZtL1oZHpSNJGBqoYVO0QG+aJt263mTuUbL9AWcBYctoZEIdP1AzBisCc0UAAA7\">";
		case '7': // search
			return "<img src=\"data:image/gif;base64,R0lGODdhFAAXAPAAMf///wAAACwAAAAAFAAXAAACQoSPqcvtD02YtNoKQkRUWT8l2hFypSSNZ7ZqoWqOJJzFjCqzOZmKK8oC7Wq/1FC36AiTRdxShPQdldLN7ILF6rLZAgA7\">";
		case '5':
		case '9': // binary
			return "<img src=\"data:image/gif;base64,R0lGODdhFAAXAPAAMf///wAAACwAAAAAFAAXAAACRoSPqcvtD02YtFYV4Ez55S4dlNSVIglsW3qy6QeTVxmvr4WqIDLKKGaL3H49IO2FNOZiSqRu0XsGRavqlLqLWLbcWfcbKAAAOw==\">";
		case '8': // telnet
			return "<img src=\"data:image/gif;base64,R0lGODlhFAAXAPAAAP///wAAACH5BAAAAAAALAAAAAAUABcAAAJBhI+py+0PT5i0WmpClLrjr3GAN41lKYboGmad63rICZtyWrPza8a2ymORgKMN7IK8xJJI46L1aDGZv6nS2sxsQgUAOw==\">";
		case 'g':
		case 'p':
		case 'I': // image
			return "<img src=\"data:image/gif;base64,R0lGODdhFAAXAPAAMf///wAAACwAAAAAFAAXAAACQISPqcvtD02YtFYV4EzZZtSBgMdJR2eV5jimJolebXzBsrRhJP6BXk6J+HqnW5E4M844uyPT5XzmhsLj71rDYgsAOw==\">";
		case 's': // audio
			return "<img src=\"data:image/gif;base64,R0lGODlhFAAXAPAAAP///wAAACH5BAAAAAAALAAAAAAUABcAAAJIhI+py+0PTZi02gpCRDRqOS1fJoHjEaJlx5GuBoeYS8KZnNj6bbd73FP9eLzPEFgM/lKolLHGbLagJ1XpFW2MWJvk5YsBiycFADs=\">";
		case 'h': // web
			return "<img src=\"data:image/gif;base64,R0lGODdhFAAXAPAAMf///wAAACwAAAAAFAAXAAACQISPqcvtD02YtFYV4Ey5g8x9iOeFovRd5gmWnHWO22HNo0Nh4JKvvE0DynY/Yo/RChJ1RqFpdywOI8OatZa6agsAOw==\">";
		default:
			return "";
	}
}

static void render_line_link(char type, const char *title, const char *selector, const char *host, const char *port) {
	size_t indent = 0;
	while (*title == ' ') {
		title++;
		indent++;
	}
	printf("<tr><td>%s</td><td><pre>%*s<a href=\"gopher://", icon(type), indent, "");
	if (host) {
		print_htmlenc(host);
	}
	if (port && strcmp(port, "70")) {
		putchar(':');
		print_htmlenc(port);
	}
	if (type) {
		putchar('/');
		putchar_htmlenc(type);
	}
	if (selector) {
		print_htmlenc(selector);
	}
	printf("\">");
	if (title) {
		print_htmlenc(title);
	}
	printf("</a></pre></td></tr>\n");
}

static void render_line_search(char type, const char *title, const char *selector, const char *host, const char *port) {
	size_t indent = 0;
	while (*title == ' ') {
		title++;
		indent++;
	}
	printf("<tr><td>%s</td><td><form method=\"get\" action=\"gopher://", icon(type));
	if (host) {
		print_htmlenc(host);
	}
	if (port && strcmp(port, "70")) {
		putchar(':');
		print_htmlenc(port);
	}
	if (type) {
		putchar('/');
		putchar_htmlenc(type);
	}
	if (selector) {
		print_htmlenc(selector);
	}
	printf("\"><pre>%*s", indent, "");
	printf("\n%*s<input name=__gopher__query__ size=72 placeholder=\"", indent, "");
	if (title) {
		print_htmlenc(title);
	}
	printf("\"></pre></form></td></tr>\n");
}

static void render_line_telnet(char type, const char *title, const char *host, const char *port) {
	size_t indent = 0;
	while (*title == ' ') {
		title++;
		indent++;
	}
	printf("<tr><td>%s</td><td><pre>%*s<a href=\"telnet://", icon(type), indent, "");
	if (host) {
		print_htmlenc(host);
	}
	if (port && strcmp(port, "23")) {
		putchar(':');
		print_htmlenc(port);
	}
	printf("\">");
	if (title) {
		print_htmlenc(title);
	}
	printf("</a></pre></td></tr>\n");
}

static void render_line_href(char type, const char *title, const char *selector) {
	size_t indent = 0;
	while (*title == ' ') {
		title++;
		indent++;
	}
	printf("<tr><td>%s</td><td><pre>%*s<a href=\"", icon(type), indent, "");
	print_htmlenc(selector);
	printf("\">");
	print_htmlenc(title);
	printf("</a></pre></td></tr>\n");
}

static void render_line(char *line, size_t len) {
	char type, *title, *selector, *host, *port;
	if (len < 1) return;
	line[len] = '\0';
	printf("<!--");
	print_htmlenc(line);
	printf("-->\n");
	parse_line(line, &type, &title, &selector, &host, &port);
	switch (type) {
		case '0':
		case '1':
		case '5':
		case '9':
		case 'p':
		case 'I':
		case 'g':
		case 's':
			return render_line_link(type, title, selector, host, port);
		case '7':
			return render_line_search(type, title, selector, host, port);
		case '8':
			return render_line_telnet(type, title, host, port);
		case 'h':
			if (*selector == '/') selector++;
			if (!strncmp(selector, "URL:", 4)) selector += 4;
			return render_line_href(type, title, selector);
		case 'i':
		case '3':
			return render_line_info(title);
		case 'E':
			return render_line_info(line);
		case '.':
			return render_line_empty();
		case '_':
			return render_line_line();
		default:
			return render_line_unknown(type, line);
	}
}

static void compact_buf(char *buf, size_t start, size_t end) {
	size_t i;
	for (i = start; i < end; i++) {
		buf[i - start] = buf[i];
	}
}

static size_t read_line(char *buf, size_t len) {
	size_t i;
	for (i = 0; i < len && buf[i] != '\r' && buf[i] != '\n'; i++);
	if (buf[i] == '\r' || buf[i] == '\n') {
		if (i+1 < len && buf[i+1] == '\n') i++;
		render_line(buf, i);
		return i+1;
	}
	return 0;
}

static void read_response(int s) {
	char buf[4096];
	size_t len;
	size_t start = 0;
	size_t end = 0;
	int rc;

	while (1) {
		/* fill up the buffer */
		len = sizeof(buf) - end;
		rc = read_some(s, buf + end, &len);
		if (rc < 0 && errno == EPIPE) break;
		if (rc < 0) return warn("read_some");
		end += len;
		/* read full lines */
		while ((len = read_line(buf + start, end - start)))	start += len;
		compact_buf(buf, start, end);
		end -= start;
		start = 0;
	}
	/* read the partial line */
	if (start < end) render_line(buf + start, end - start);

}

static void read_data(int s) {
	char buf[4096];
	size_t len = sizeof(buf);
	int rc;
	while (1) {
		rc = read_some(s, buf, &len);
		if (rc < 0 && errno == EPIPE) return;
		if (rc < 0) return warn("read response");
		rc = write_all(0, buf, len);
		if (rc < 0) return warn("write response");
	}
}

static void render_dir(int s, const char *url) {
	dpi_send_header(url, "text/html");
	printf("<doctype html>");
	printf("<html><head><title>");
	print_htmlenc(url);
	printf("</title></head><body><table>");
	read_response(s);
	printf("</table></body></html>");
}

static void render_text(int s, const char *url) {
	dpi_send_header(url, "text/plain");
	read_data(s);
}

static void render_binary(int s, const char *url) {
	// dpi_send_header(url, "application/octet-stream");
	dpi_send_header(url, "text/plain");
	read_data(s);
}

static const char *detect_image_content_type(const char buf[4]) {
	if (!strncmp(buf, "GIF8", 4)) return "image/gif";
	if (!strncmp(buf, "\x89PNG", 4)) return "image/png";
	if (!strncmp(buf, "\xff\xd8", 2)) return "image/jpeg";
	return "text/plain";
}

static void render_image(int s, const char *url) {
	int rc;
	char buf[4];
	rc = read_all(s, buf, sizeof(buf));
	if (rc < 0) return respond_err("read image");
	dpi_send_header(url, detect_image_content_type(buf));
	rc = write_all(0, buf, 4);
	if (rc < 0) return warn("write image");
	read_data(s);
}

static void fix_path(char *path) {
	char *start, *end;
	/* change form input name to query */
	start = strstr(path, "__gopher__query__=");
	if (!start) return;
	end = start + 18;
	do *start++ = *end++;
	while (*end);
	if (start[-1] == '?') start--;
	*start = '\0';
}

static void respond(const char *url) {
	int rc;
	const char *errstr = NULL;
	char host[1024];
	char port[1024];
	char type;
	char path[4096];
	int s;
	rc = parse_url(url, host, sizeof(host), port, sizeof(port), &type, path, sizeof(path));
	if (rc < 0) return respond_err("parse_url");
	if (!*host) strcpy(host, "127.0.0.1");
	if (!*port) strcpy(port, "70");
	if (!type) type = '1';
	if (*path) fix_path(path);
	s = tcp_connect(host, port, &errstr);
	if (s < 0) return respond_errstr(errstr, "tcp_connect");
	rc = write_all(s, path, strlen(path));
	if (rc < 0) return respond_err("write request");
	rc = write_all(s, "\r\n", 2);
	if (rc < 0) return respond_err("write request end");
	switch (type) {
		case '1': return render_dir(s, url);
		case '7': return render_dir(s, url);
		case 'g':
		case 'p':
		case 'I': return render_image(s, url);
		case '9': return render_binary(s, url);
		default: return render_text(s, url);
	}
}

int main() {
	char url[1024];
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);
	dpi_read_request(url, sizeof(url));
	respond(url);
}
