/*
 * infra_net.c - Network Infrastructure Layer Implementation
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/time.h>

#include "internal/infra/infra_core.h"
#include "internal/infra/infra_net.h"
#include "internal/infra/infra_log.h"

//-----------------------------------------------------------------------------
// Network Operations Implementation
//-----------------------------------------------------------------------------

infra_error_t infra_net_create(infra_socket_t* sock, bool is_udp) {
    if (!sock) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    int fd = socket(AF_INET, is_udp ? SOCK_DGRAM : SOCK_STREAM, 0);
    if (fd < 0) {
        INFRA_LOG_ERROR("Failed to create socket: %s", strerror(errno));
        return INFRA_ERROR_IO;
    }

    *sock = fd;
    return INFRA_OK;
}

infra_error_t infra_net_bind(infra_socket_t sock, const infra_net_addr_t* addr) {
    if (sock < 0 || !addr) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(addr->port);
    if (inet_pton(AF_INET, addr->ip, &server_addr.sin_addr) <= 0) {
        INFRA_LOG_ERROR("Invalid IP address: %s", addr->ip);
        return INFRA_ERROR_INVALID_PARAM;
    }

    if (bind(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        INFRA_LOG_ERROR("Failed to bind socket to %s:%u: %s", 
                       addr->ip, addr->port, strerror(errno));
        return INFRA_ERROR_IO;
    }

    return INFRA_OK;
}

infra_error_t infra_net_listen(infra_socket_t sock, int backlog) {
    if (sock < 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // Get socket type
    int type;
    socklen_t optlen = sizeof(type);
    if (getsockopt(sock, SOL_SOCKET, SO_TYPE, &type, &optlen) < 0) {
        return INFRA_ERROR_IO;
    }

    // Check if it's UDP socket
    if (type == SOCK_DGRAM) {
        return INFRA_ERROR_INVALID_OPERATION;
    }

    if (listen(sock, backlog) < 0) {
        return INFRA_ERROR_IO;
    }

    return INFRA_OK;
}

infra_error_t infra_net_accept(infra_socket_t sock, infra_socket_t* client_sock, infra_net_addr_t* client_addr) {
    if (sock < 0 || !client_sock) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    int client_fd = accept(sock, (struct sockaddr*)&addr, &addr_len);
    
    if (client_fd < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return INFRA_ERROR_WOULD_BLOCK;
        }
        INFRA_LOG_ERROR("Accept failed: %s", strerror(errno));
        return INFRA_ERROR_IO;
    }

    *client_sock = client_fd;

    if (client_addr) {
        inet_ntop(AF_INET, &addr.sin_addr, client_addr->ip, sizeof(client_addr->ip));
        client_addr->port = ntohs(addr.sin_port);
    }

    return INFRA_OK;
}

infra_error_t infra_net_connect(const infra_net_addr_t* addr, infra_socket_t* sock) {
    if (!addr || !sock) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // Create socket if not already created
    if (*sock < 0) {
        infra_error_t err = infra_net_create(sock, false);
        if (err != INFRA_OK) {
            return err;
        }
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(addr->port);
    if (inet_pton(AF_INET, addr->ip, &server_addr.sin_addr) <= 0) {
        if (*sock >= 0) {
            infra_net_close(*sock);
            *sock = -1;
        }
        return INFRA_ERROR_INVALID_PARAM;
    }

    if (connect(*sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        INFRA_LOG_ERROR("Connect failed: %s", strerror(errno));
        if (*sock >= 0) {
            infra_net_close(*sock);
            *sock = -1;
        }
        return INFRA_ERROR_IO;
    }

    return INFRA_OK;
}

infra_error_t infra_net_send(infra_socket_t sock, const void* data, size_t size, size_t* sent) {
    if (sock < 0 || !data || !sent) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    ssize_t ret = send(sock, data, size, 0);
    if (ret < 0) {
        return INFRA_ERROR_IO;
    }

    *sent = (size_t)ret;
    return INFRA_OK;
}

infra_error_t infra_net_recv(infra_socket_t sock, void* buffer, size_t size, size_t* received) {
    if (sock < 0 || !buffer || !received) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    ssize_t ret = recv(sock, buffer, size, 0);
    if (ret < 0) {
        return INFRA_ERROR_IO;
    }

    *received = (size_t)ret;
    return INFRA_OK;
}

void infra_net_close(infra_socket_t sock) {
    if (sock >= 0) {
        close(sock);
    }
}

infra_error_t infra_net_set_option(infra_socket_t sock, int level, int option, const void* value, size_t len) {
    if (sock < 0 || !value) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // Map our socket level to system socket level
    int sys_level;
    switch (level) {
        case INFRA_SOL_SOCKET:
            sys_level = SOL_SOCKET;
            break;
        case IPPROTO_TCP:
            sys_level = IPPROTO_TCP;
            break;
        default:
            return INFRA_ERROR_INVALID_PARAM;
    }

    // Map our socket options to system socket options
    int sys_option;
    switch (option) {
        case INFRA_SO_REUSEADDR:
            sys_option = SO_REUSEADDR;
            break;
        case INFRA_SO_KEEPALIVE:
            sys_option = SO_KEEPALIVE;
            break;
        case INFRA_SO_RCVTIMEO:
            sys_option = SO_RCVTIMEO;
            break;
        case INFRA_SO_SNDTIMEO:
            sys_option = SO_SNDTIMEO;
            break;
        default:
            return INFRA_ERROR_INVALID_PARAM;
    }

    if (setsockopt(sock, sys_level, sys_option, value, (socklen_t)len) < 0) {
        INFRA_LOG_ERROR("Failed to set socket option: %s", strerror(errno));
        return INFRA_ERROR_IO;
    }

    return INFRA_OK;
}

infra_error_t infra_net_get_option(infra_socket_t sock, int level, int option, void* value, size_t* len) {
    if (sock < 0 || !value || !len) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    socklen_t optlen = (socklen_t)*len;
    if (getsockopt(sock, level, option, value, &optlen) < 0) {
        return INFRA_ERROR_IO;
    }

    *len = (size_t)optlen;
    return INFRA_OK;
}

infra_error_t infra_net_set_nonblock(infra_socket_t sock, bool nonblock) {
    if (sock < 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0) {
        INFRA_LOG_ERROR("Failed to get socket flags: %s", strerror(errno));
        return INFRA_ERROR_IO;
    }

    if (nonblock) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }

    if (fcntl(sock, F_SETFL, flags) < 0) {
        INFRA_LOG_ERROR("Failed to set socket flags: %s", strerror(errno));
        return INFRA_ERROR_IO;
    }

    return INFRA_OK;
}

infra_error_t infra_net_set_timeout(infra_socket_t sock, uint32_t send_timeout_ms, uint32_t recv_timeout_ms) {
    if (sock < 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    if (send_timeout_ms > 0) {
        struct timeval tv;
        tv.tv_sec = send_timeout_ms / 1000;
        tv.tv_usec = (send_timeout_ms % 1000) * 1000;
        if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
            return INFRA_ERROR_IO;
        }
    }

    if (recv_timeout_ms > 0) {
        struct timeval tv;
        tv.tv_sec = recv_timeout_ms / 1000;
        tv.tv_usec = (recv_timeout_ms % 1000) * 1000;
        if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
            return INFRA_ERROR_IO;
        }
    }

    return INFRA_OK;
}

infra_error_t infra_net_get_local_addr(infra_socket_t sock, infra_net_addr_t* addr) {
    if (sock < 0 || !addr) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    struct sockaddr_in local_addr;
    socklen_t addr_len = sizeof(local_addr);
    if (getsockname(sock, (struct sockaddr*)&local_addr, &addr_len) < 0) {
        return INFRA_ERROR_IO;
    }

    inet_ntop(AF_INET, &local_addr.sin_addr, addr->ip, sizeof(addr->ip));
    addr->port = ntohs(local_addr.sin_port);

    return INFRA_OK;
}

infra_error_t infra_net_get_peer_addr(infra_socket_t sock, infra_net_addr_t* addr) {
    if (sock < 0 || !addr) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    struct sockaddr_in peer_addr;
    socklen_t addr_len = sizeof(peer_addr);
    if (getpeername(sock, (struct sockaddr*)&peer_addr, &addr_len) < 0) {
        return INFRA_ERROR_IO;
    }

    inet_ntop(AF_INET, &peer_addr.sin_addr, addr->ip, sizeof(addr->ip));
    addr->port = ntohs(peer_addr.sin_port);

    return INFRA_OK;
}

infra_error_t infra_net_addr_from_string(const char* ip, uint16_t port, infra_net_addr_t* addr) {
    if (!ip || !addr) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    strncpy(addr->ip, ip, sizeof(addr->ip) - 1);
    addr->ip[sizeof(addr->ip) - 1] = '\0';
    addr->port = port;

    return INFRA_OK;
}

infra_error_t infra_net_addr_to_string(const infra_net_addr_t* addr, char* buffer, size_t size) {
    if (!addr || !buffer || size == 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    snprintf(buffer, size, "%s:%u", addr->ip, addr->port);
    return INFRA_OK;
}
