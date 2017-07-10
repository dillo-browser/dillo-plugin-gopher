#pragma once

#include <stddef.h>

void dpi_read_request(char *url_buf, size_t url_len);
void dpi_send_status(const char *status);
void dpi_send_header(const char *url, const char *content_type);
