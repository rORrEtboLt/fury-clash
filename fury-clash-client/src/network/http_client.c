/*
 * http_client.c — blocking HTTP POST using POSIX sockets.
 * Used once at lobby time to create a relay room via POST /rooms.
 */

#include "http_client.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

/* ── URL helpers ─────────────────────────────────────────────────────────── */

int http_parse_ws_url(const char *ws_url,
                      char *host_out, int host_out_len,
                      int  *port_out)
{
    const char *p = ws_url;

    /* Strip ws:// prefix */
    if (strncmp(p, "ws://", 5) == 0)       p += 5;
    else if (strncmp(p, "wss://", 6) == 0) p += 6;
    /* else: assume bare host:port */

    const char *slash = strchr(p, '/');
    const char *colon = strchr(p, ':');

    if (colon && (!slash || colon < slash)) {
        int hlen = (int)(colon - p);
        if (hlen <= 0 || hlen >= host_out_len) return 0;
        memcpy(host_out, p, hlen);
        host_out[hlen] = '\0';
        *port_out = atoi(colon + 1);
    } else {
        int end = slash ? (int)(slash - p) : (int)strlen(p);
        if (end <= 0 || end >= host_out_len) return 0;
        memcpy(host_out, p, end);
        host_out[end] = '\0';
        *port_out = 80;
    }
    return 1;
}

/* ── Blocking HTTP POST ───────────────────────────────────────────────────── */

int http_post(const char *host, int port, const char *path,
              const char *json_body,
              char *out_buf, int out_size)
{
    if (out_buf && out_size > 0) out_buf[0] = '\0';

    /* DNS + connect (blocking) */
    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%d", port);

    if (getaddrinfo(host, port_str, &hints, &res) != 0 || !res) return -1;

    int fd = (int)socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) { freeaddrinfo(res); return -1; }

    if (connect(fd, res->ai_addr, res->ai_addrlen) != 0) {
        freeaddrinfo(res);
        close(fd);
        return -1;
    }
    freeaddrinfo(res);

    /* Build request */
    int body_len = (int)strlen(json_body);
    char req[2048];
    int req_len = snprintf(req, sizeof(req),
        "POST %s HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        path, host, port, body_len, json_body);

    if (req_len <= 0 || req_len >= (int)sizeof(req)) { close(fd); return -1; }

    /* Send */
    int sent = 0;
    while (sent < req_len) {
        ssize_t n = send(fd, req + sent, (size_t)(req_len - sent), 0);
        if (n <= 0) { close(fd); return -1; }
        sent += (int)n;
    }

    /* Read response */
    char resp[4096];
    int  resp_len = 0;
    while (resp_len < (int)sizeof(resp) - 1) {
        ssize_t n = recv(fd, resp + resp_len,
                         (size_t)(sizeof(resp) - 1 - resp_len), 0);
        if (n < 0) {
            if (errno == EAGAIN || errno == EINTR) continue;
            break;
        }
        if (n == 0) break;
        resp_len += (int)n;
    }
    resp[resp_len] = '\0';
    close(fd);

    /* Parse status code from "HTTP/1.x NNN " */
    int status = -1;
    const char *sp = strstr(resp, "HTTP/");
    if (sp) {
        sp = strchr(sp, ' ');
        if (sp) status = atoi(sp + 1);
    }

    /* Copy body (after \r\n\r\n) to caller */
    if (out_buf && out_size > 0) {
        const char *body = strstr(resp, "\r\n\r\n");
        if (body) {
            body += 4;
            int blen = resp_len - (int)(body - resp);
            if (blen < 0) blen = 0;
            if (blen >= out_size) blen = out_size - 1;
            memcpy(out_buf, body, blen);
            out_buf[blen] = '\0';
        }
    }

    return status;
}
