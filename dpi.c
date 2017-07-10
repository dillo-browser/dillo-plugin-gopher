#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <err.h>
#include "dpi.h"
#include "io.h"

static void check_auth() {
	char buf[30];
	int rc;
	char key[4], local_key[4];
	char keys[128];
	char *home;
	ssize_t sz;
	rc = read_all(STDIN_FILENO, buf, 29);
	if (rc < 0) err(1, "read auth");
	buf[30] = '\0';
	rc = sscanf(buf, "<cmd='auth' msg='%4x' '>", key);
	if (rc < 0) err(1, "auth: %.*s", 29, buf);
	if (rc < 1) errx(1, "auth: %.*s", 29, buf);
	home = getenv("HOME");
	if (!home) home = ".";
	sz = read_file(keys, sizeof(keys), "%s/.dillo/dpid_comm_keys", home);
	if (sz < 0) err(1, "read dillo comm keys");
	rc = sscanf(keys, "%*d %4x' '>", local_key);
	if (rc < 0) err(1, "comm key: %.*s", sz, keys);
	if (rc < 1) errx(1, "comm key: %.*s", sz, keys);
	if (memcmp(key, local_key, 4)) errx(1, "wrong dillo key");
}

static void get_url(char *url_buf, size_t url_len) {
	char buf[21];
	size_t len;
	int rc;
	int i;

	rc = read_all(STDIN_FILENO, buf, sizeof(buf));
	if (rc < 0) err(1, "read open_url");
	if (strncmp(buf, "<cmd='open_url' url='", 21)) {
		err(1, "bad open_url cmd: %.*s", sizeof(buf), buf);
	}
	len = url_len;
	rc = read_some(STDIN_FILENO, url_buf, &len);
	if (rc < 0) err(1, "read url");

	for (i = 0; i < len-1; i++) {
		if (url_buf[i] == '\'' && url_buf[i+1] == ' ') break;
	}
	if (i > len-3 || strncmp(url_buf + i, "' '>", 4)) {
		err(1, "bad url end: %.*s", len, url_buf);
	}
	url_buf[i] = '\0';
}

void dpi_read_request(char *url_buf, size_t url_len) {
	check_auth();
	get_url(url_buf, url_len);
}

void dpi_send_status(const char *status) {
	int rc = printf("<cmd='send_status_msg' msg='%s' '>", status);
	if (rc < 0) err(1, "send_status_msg");
}

void dpi_send_header(const char *url, const char *content_type) {
	int rc;
	if (!url) url = "";
	rc = printf("<cmd='start_send_page' url='%s' '>"
	              "Content-Type: %s\n\n", url, content_type);
	if (rc < 0) err(1, "start_send_page");
}
