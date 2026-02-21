#pragma once

/*
 * Minimal blocking HTTP POST over a plain TCP socket.
 * No external dependencies beyond POSIX networking.
 *
 * Returns the HTTP status code (e.g. 200, 201) or -1 on socket/protocol error.
 * The response body is written into out_buf[0..out_size-1] as a NUL-terminated string.
 */
int http_post(const char *host, int port, const char *path,
              const char *json_body,
              char *out_buf, int out_size);

/*
 * Parse "ws://host:port[/path]" into host and port.
 * Returns 1 on success, 0 on failure.
 */
int http_parse_ws_url(const char *ws_url,
                      char *host_out, int host_out_len,
                      int  *port_out);
