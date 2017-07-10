#pragma once

#include <stdio.h>

int read_all(int fd, unsigned char *buf, size_t count);
int read_some(int fd, unsigned char *buf, size_t *lenp);
int write_all(int fd, unsigned const char *buf, size_t count);
ssize_t read_file(char *buf, size_t len, const char *fmt, ...);
int tcp_connect(const char *host, const char *port, char const **err);
