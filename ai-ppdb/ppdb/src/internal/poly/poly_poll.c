#include "internal/poly/poly_poll.h"
#include "internal/infra/infra_log.h"
#include "internal/infra/infra_thread.h"

#include <poll.h>
#include <errno.h>

infra_error_t poly_poll_create(poly_poll_t** poll) {
    if (!poll) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    *poll = (poly_poll_t*)infra_malloc(sizeof(poly_poll_t));
    if (!*poll) {
        return INFRA_ERROR_NO_MEMORY;
    }

    (*poll)->capacity = 16;  // Initial capacity
    (*poll)->count = 0;
    (*poll)->pfds = (struct pollfd*)infra_malloc((*poll)->capacity * sizeof(struct pollfd));
    (*poll)->sockets = (infra_socket_t*)infra_malloc((*poll)->capacity * sizeof(infra_socket_t));

    if (!(*poll)->pfds || !(*poll)->sockets) {
        if ((*poll)->pfds) infra_free((*poll)->pfds);
        if ((*poll)->sockets) infra_free((*poll)->sockets);
        infra_free(*poll);
        *poll = NULL;
        return INFRA_ERROR_NO_MEMORY;
    }

    infra_error_t err = infra_mutex_create(&(*poll)->mutex);
    if (err != INFRA_OK) {
        infra_free((*poll)->pfds);
        infra_free((*poll)->sockets);
        infra_free(*poll);
        *poll = NULL;
        return err;
    }

    return INFRA_OK;
}

void poly_poll_destroy(poly_poll_t* poll) {
    if (!poll) {
        return;
    }

    infra_mutex_destroy(poll->mutex);
    infra_free(poll->pfds);
    infra_free(poll->sockets);
    infra_free(poll);
}

infra_error_t poly_poll_add(poly_poll_t* poll, infra_socket_t sock, int events) {
    if (!poll || !sock) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    infra_mutex_lock(poll->mutex);

    // Check if we need to resize
    if (poll->count >= poll->capacity) {
        size_t new_capacity = poll->capacity * 2;
        struct pollfd* new_pfds = (struct pollfd*)infra_malloc(new_capacity * sizeof(struct pollfd));
        infra_socket_t* new_sockets = (infra_socket_t*)infra_malloc(new_capacity * sizeof(infra_socket_t));

        if (!new_pfds || !new_sockets) {
            if (new_pfds) infra_free(new_pfds);
            if (new_sockets) infra_free(new_sockets);
            infra_mutex_unlock(poll->mutex);
            return INFRA_ERROR_NO_MEMORY;
        }

        memcpy(new_pfds, poll->pfds, poll->count * sizeof(struct pollfd));
        memcpy(new_sockets, poll->sockets, poll->count * sizeof(infra_socket_t));
        infra_free(poll->pfds);
        infra_free(poll->sockets);
        poll->pfds = new_pfds;
        poll->sockets = new_sockets;
        poll->capacity = new_capacity;
    }

    // Add the new socket
    poll->pfds[poll->count].fd = (int)(intptr_t)sock;
    poll->pfds[poll->count].events = events;
    poll->pfds[poll->count].revents = 0;
    poll->sockets[poll->count] = sock;
    poll->count++;

    infra_mutex_unlock(poll->mutex);
    return INFRA_OK;
}

infra_error_t poly_poll_remove(poly_poll_t* poll, infra_socket_t sock) {
    if (!poll || !sock) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    infra_mutex_lock(poll->mutex);

    // Find the socket
    size_t i;
    for (i = 0; i < poll->count; i++) {
        if (poll->sockets[i] == sock) {
            break;
        }
    }

    if (i == poll->count) {
        infra_mutex_unlock(poll->mutex);
        return INFRA_ERROR_NOT_FOUND;
    }

    // Remove by shifting remaining elements
    if (i < poll->count - 1) {
        memmove(&poll->pfds[i], &poll->pfds[i + 1], (poll->count - i - 1) * sizeof(struct pollfd));
        memmove(&poll->sockets[i], &poll->sockets[i + 1], (poll->count - i - 1) * sizeof(infra_socket_t));
    }
    poll->count--;

    infra_mutex_unlock(poll->mutex);
    return INFRA_OK;
}

infra_error_t poly_poll_wait(poly_poll_t* poll_ctx, int timeout_ms) {
    if (!poll_ctx || !poll_ctx->pfds) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    int ret = poll(poll_ctx->pfds, (nfds_t)poll_ctx->count, timeout_ms);
    if (ret < 0) {
        if (errno == EINTR) {
            return INFRA_ERROR_IO;  // Use IO error instead of INTERRUPTED
        }
        return INFRA_ERROR_IO;
    }

    return INFRA_OK;
}

infra_error_t poly_poll_get_events(poly_poll_t* poll, size_t index, int* events) {
    if (!poll || !events || index >= poll->count) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    *events = poll->pfds[index].revents;
    return INFRA_OK;
}

infra_error_t poly_poll_get_socket(poly_poll_t* poll, size_t index, infra_socket_t* sock) {
    if (!poll || !sock || index >= poll->count) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    *sock = poll->sockets[index];
    return INFRA_OK;
}

size_t poly_poll_get_count(poly_poll_t* poll) {
    return poll ? poll->count : 0;
}

infra_error_t poly_poll_init(poly_poll_context_t* ctx, const poly_poll_config_t* config) {
    if (!ctx || !config) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // Initialize thread pool
    infra_thread_pool_config_t pool_config = {
        .min_threads = config->min_threads,
        .max_threads = config->max_threads,
        .queue_size = config->queue_size
    };

    infra_error_t err = infra_thread_pool_create(&pool_config, &ctx->pool);
    if (err != INFRA_OK) {
        return err;
    }

    // Allocate arrays
    ctx->max_listeners = config->max_listeners;
    ctx->listeners = (infra_socket_t*)infra_malloc(config->max_listeners * sizeof(infra_socket_t));
    ctx->polls = (struct pollfd*)infra_malloc(config->max_listeners * sizeof(struct pollfd));
    ctx->listener_configs = (poly_poll_listener_t*)infra_malloc(config->max_listeners * sizeof(poly_poll_listener_t));

    if (!ctx->listeners || !ctx->polls || !ctx->listener_configs) {
        if (ctx->listeners) infra_free(ctx->listeners);
        if (ctx->polls) infra_free(ctx->polls);
        if (ctx->listener_configs) infra_free(ctx->listener_configs);
        infra_thread_pool_destroy(ctx->pool);
        return INFRA_ERROR_NO_MEMORY;
    }

    ctx->listener_count = 0;
    ctx->running = false;
    ctx->handler = NULL;

    return INFRA_OK;
}

void poly_poll_set_handler(poly_poll_context_t* ctx, poly_poll_connection_handler handler) {
    if (ctx) {
        ctx->handler = handler;
    }
}

infra_error_t poly_poll_add_listener(poly_poll_context_t* ctx, const poly_poll_listener_t* listener) {
    if (!ctx || !listener) {
        INFRA_LOG_ERROR("Invalid parameters: ctx=%p, listener=%p", ctx, listener);
        return INFRA_ERROR_INVALID_PARAM;
    }

    infra_socket_t sock = -1;
    infra_error_t err = infra_net_create(&sock, false);
    if (err != INFRA_OK) {
        return err;
    }

    // Set socket options
    int optval = 1;
    err = infra_net_set_option(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    if (err != INFRA_OK) {
        infra_net_close(sock);
        return err;
    }

    // Set non-blocking mode
    err = infra_net_set_nonblock(sock, true);
    if (err != INFRA_OK) {
        infra_net_close(sock);
        return err;
    }

    // Bind socket
    infra_net_addr_t addr;
    err = infra_net_addr_from_string(listener->bind_addr, listener->bind_port, &addr);
    if (err != INFRA_OK) {
        infra_net_close(sock);
        return err;
    }

    err = infra_net_bind(sock, &addr);
    if (err != INFRA_OK) {
        infra_net_close(sock);
        return err;
    }

    // Start listening
    err = infra_net_listen(sock, SOMAXCONN);
    if (err != INFRA_OK) {
        infra_net_close(sock);
        return err;
    }

    // Add to listeners array
    if (ctx->listener_count >= ctx->max_listeners) {
        infra_net_close(sock);
        return INFRA_ERROR_NO_MEMORY;
    }

    ctx->listeners[ctx->listener_count] = sock;
    ctx->listener_configs[ctx->listener_count] = *listener;
    
    // Add to poll set
    struct pollfd pfd = {
        .fd = (int)(intptr_t)sock,
        .events = POLLIN,
        .revents = 0
    };
    ctx->polls[ctx->listener_count] = pfd;
    ctx->listener_count++;

    INFRA_LOG_INFO("Added listener on %s:%u", listener->bind_addr, listener->bind_port);
    return INFRA_OK;
}

infra_error_t poly_poll_start(poly_poll_context_t* ctx) {
    if (!ctx) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    if (!ctx->handler) {
        return INFRA_ERROR_INVALID_OPERATION;
    }

    ctx->running = true;

    while (ctx->running) {
        // Wait for events on all sockets
        int ret = poll(ctx->polls, ctx->listener_count, 1000);  // 1 second timeout
        if (ret < 0) {
            if (errno == EINTR) {
                if (!ctx->running) break;
                continue;
            }
            INFRA_LOG_ERROR("Poll failed: %s", strerror(errno));
            continue;
        }
        if (ret == 0) {  // Timeout
            if (!ctx->running) break;
            continue;
        }

        // Check each listener
        for (int i = 0; i < ctx->listener_count && ctx->running; i++) {
            if (!(ctx->polls[i].revents & POLLIN)) continue;

            // Accept new connection
            infra_socket_t client = -1;
            infra_net_addr_t client_addr;
            infra_error_t err = infra_net_accept(ctx->listeners[i], &client, &client_addr);
            if (err != INFRA_OK) {
                INFRA_LOG_ERROR("Failed to accept connection: %d", err);
                continue;
            }

            // Set client socket to non-blocking mode
            err = infra_net_set_nonblock(client, true);
            if (err != INFRA_OK) {
                infra_net_close(client);
                continue;
            }

            INFRA_LOG_INFO("New connection from %s:%u on socket %d",
                client_addr.ip, client_addr.port, i);

            // Create handler args
            poly_poll_handler_args_t* args = infra_malloc(sizeof(poly_poll_handler_args_t));
            if (!args) {
                INFRA_LOG_ERROR("Failed to allocate handler args");
                infra_net_close(client);
                continue;
            }
            args->client = client;
            args->user_data = ctx->listener_configs[i].user_data;

            // Submit to thread pool
            err = infra_thread_pool_submit(ctx->pool, (infra_thread_func_t)ctx->handler, args);
            if (err != INFRA_OK) {
                INFRA_LOG_ERROR("Failed to submit to thread pool: %d", err);
                infra_free(args);
                infra_net_close(client);
                continue;
            }
        }
    }

    return INFRA_OK;
}

infra_error_t poly_poll_stop(poly_poll_context_t* ctx) {
    if (!ctx) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    ctx->running = false;
    return INFRA_OK;
}

void poly_poll_cleanup(poly_poll_context_t* ctx) {
    if (!ctx) {
        return;
    }

    // Close all listener sockets
    for (int i = 0; i < ctx->listener_count; i++) {
        if (ctx->listeners[i]) {
            infra_net_close(ctx->listeners[i]);
        }
    }

    // Free arrays
    infra_free(ctx->listeners);
    infra_free(ctx->polls);
    infra_free(ctx->listener_configs);

    // Destroy thread pool
    if (ctx->pool) {
        infra_thread_pool_destroy(ctx->pool);
    }

    // Clear context
    memset(ctx, 0, sizeof(poly_poll_context_t));
}
