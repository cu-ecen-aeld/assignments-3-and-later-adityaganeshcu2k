/*References :- https://beej.us/guide/bgnet/html/ , http://man7.org/linux/man-pages/man8/start-stop-daemon.8.html , https://chatgpt.com/share/6992b468-ee74-8008-834b-cb796a405b88*/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <fcntl.h>

#define PORT 9000
#define DATA_FILE "/var/tmp/aesdsocketdata"
#define BUFFER_SIZE 1024

static int server_fd = -1;
static volatile sig_atomic_t exit_requested = 0;

void signal_handler(int signo)
{
    syslog(LOG_INFO, "Caught signal, exiting");
    exit_requested = 1;
}

void cleanup()
{
    if (server_fd != -1)
        close(server_fd);

    remove(DATA_FILE);
    closelog();
}

int main(int argc, char *argv[])
{
    int daemon_mode = 0;

    if (argc == 2 && strcmp(argv[1], "-d") == 0)
        daemon_mode = 1;

    openlog("aesdsocket", LOG_PID, LOG_USER);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
        return -1;

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
        return -1;

    if (daemon_mode)
    {
        pid_t pid = fork();
        if (pid < 0)
            return -1;
        if (pid > 0)
            exit(0);

        if (setsid() < 0)
            return -1;

        chdir("/");
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
    }

    if (listen(server_fd, 10) < 0)
        return -1;

    while (!exit_requested)
    {
        struct sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);

        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addrlen);
        if (client_fd < 0)
        {
            if (exit_requested)
                break;
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));

        syslog(LOG_INFO, "Accepted connection from %s", client_ip);

        char buffer[BUFFER_SIZE];
        char *packet = NULL;
        size_t packet_size = 0;

        ssize_t bytes_read;
        while ((bytes_read = recv(client_fd, buffer, BUFFER_SIZE, 0)) > 0)
        {
            char *newline = memchr(buffer, '\n', bytes_read);

            if (newline)
            {
                size_t chunk_len = newline - buffer + 1;

                char *temp = realloc(packet, packet_size + chunk_len);
                if (!temp)
                {
                    free(packet);
                    packet = NULL;
                    break;
                }
                packet = temp;

                memcpy(packet + packet_size, buffer, chunk_len);
                packet_size += chunk_len;

                int fd = open(DATA_FILE, O_CREAT | O_WRONLY | O_APPEND, 0644);
                if (fd >= 0)
                {
                    write(fd, packet, packet_size);
                    close(fd);
                }

                free(packet);
                packet = NULL;
                packet_size = 0;

                fd = open(DATA_FILE, O_RDONLY);
                if (fd >= 0)
                {
                    while ((bytes_read = read(fd, buffer, BUFFER_SIZE)) > 0)
                    {
                            ssize_t total_sent = 0;

			    while (total_sent < bytes_read)
			    {
				ssize_t sent = send(client_fd,
						    buffer + total_sent,
						    bytes_read - total_sent,
						    0);

				if (sent <= 0)
				    break;

				total_sent += sent;
			    }

			    if (total_sent < bytes_read)
				break;   // send error
                    }

                    close(fd);
                }

                break;
            }
            else
            {
                char *temp = realloc(packet, packet_size + bytes_read);
                if (!temp)
                {
                    free(packet);
                    packet = NULL;
                    break;
                }
                packet = temp;

                memcpy(packet + packet_size, buffer, bytes_read);
                packet_size += bytes_read;
            }
        }

        free(packet);
        close(client_fd);

        syslog(LOG_INFO, "Closed connection from %s", client_ip);
    }

    cleanup();
    return 0;
}

