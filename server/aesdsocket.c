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
#include <pthread.h>
#include <sys/queue.h>
#include <time.h>

#define PORT 9000
#define DATA_FILE "/var/tmp/aesdsocketdata"
#define BUFFER_SIZE 1024

static int server_fd = -1;
static volatile sig_atomic_t exit_requested = 0;
static pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;

struct thread_node {
    pthread_t thread_id;
    int client_fd;
    int thread_complete;
    SLIST_ENTRY(thread_node) entries;
};

SLIST_HEAD(thread_list_head, thread_node);
static struct thread_list_head thread_list;

void signal_handler(int signo)
{
    exit_requested = 1;
    if (server_fd != -1)
        close(server_fd);
}

void cleanup()
{
    if (server_fd != -1)
        close(server_fd);

    remove(DATA_FILE);
    closelog();
}

void* timestamp_thread(void *arg)
{
    while (!exit_requested)
    {
    	for (int i = 0; i < 10 && !exit_requested; i++)
        	sleep(1);
        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);

        char buffer[128];
        strftime(buffer, sizeof(buffer),
                 "timestamp:%a, %d %b %Y %H:%M:%S %z\n",
                 tm_info);

        pthread_mutex_lock(&file_mutex);

        int fd = open(DATA_FILE,
                      O_CREAT | O_WRONLY | O_APPEND,
                      0644);
        if (fd >= 0)
        {
            write(fd, buffer, strlen(buffer));
            close(fd);
        }

        pthread_mutex_unlock(&file_mutex);
        
  

    }
    return NULL;
}

void* client_handler(void *arg)
{
    struct thread_node *node = (struct thread_node*)arg;
    int client_fd = node->client_fd;

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
            if (!temp) break;

            packet = temp;
            memcpy(packet + packet_size, buffer, chunk_len);
            packet_size += chunk_len;

            // LOCK FILE WRITE
            pthread_mutex_lock(&file_mutex);

            int fd = open(DATA_FILE, O_CREAT | O_WRONLY | O_APPEND, 0644);
            if (fd >= 0)
            {
                write(fd, packet, packet_size);
                close(fd);
            }

            pthread_mutex_unlock(&file_mutex);

            free(packet);
            packet = NULL;
            packet_size = 0;

            // LOCK FILE READ
            pthread_mutex_lock(&file_mutex);

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
                }
                close(fd);
            }

            pthread_mutex_unlock(&file_mutex);

            break;
        }
        else
        {
            char *temp = realloc(packet, packet_size + bytes_read);
            if (!temp) break;

            packet = temp;
            memcpy(packet + packet_size, buffer, bytes_read);
            packet_size += bytes_read;
        }
    }

    free(packet);
    close(client_fd);
    node->thread_complete = 1;
    return NULL;
}

int main(int argc, char *argv[])
{
    int daemon_mode = 0;

    if (argc == 2 && strcmp(argv[1], "-d") == 0)
        daemon_mode = 1;

    openlog("aesdsocket", LOG_PID, LOG_USER);

    remove(DATA_FILE);
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

    pthread_t time_thread;
    pthread_create(&time_thread,
                NULL,
                timestamp_thread,
                NULL);

    SLIST_INIT(&thread_list);

    while (!exit_requested)
    {
        struct sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);

        int client_fd = accept(server_fd,
                            (struct sockaddr*)&client_addr,
                            &addrlen);

        if (client_fd < 0)
        {
            if (exit_requested)
                break;
            continue;
        }

        struct thread_node *node = malloc(sizeof(struct thread_node));
	
	
        node->client_fd = client_fd;
        node->thread_complete = 0;

        pthread_create(&node->thread_id,
                    NULL,
                    client_handler,
                    node);

        SLIST_INSERT_HEAD(&thread_list,
                        node,
                        entries);

        // CLEANUP FINISHED THREADS
	struct thread_node *iter = SLIST_FIRST(&thread_list);
	struct thread_node *tmp;

	while (iter != NULL)
	{
	    tmp = SLIST_NEXT(iter, entries);

	    if (iter->thread_complete)
	    {
		pthread_join(iter->thread_id, NULL);
		SLIST_REMOVE(&thread_list, iter, thread_node, entries);
		free(iter);
	    }

	    iter = tmp;
	}
    }

    // Join remaining client threads
    while (!SLIST_EMPTY(&thread_list))
    {
        struct thread_node *node =
            SLIST_FIRST(&thread_list);

        pthread_join(node->thread_id, NULL);
        SLIST_REMOVE_HEAD(&thread_list, entries);
        free(node);
    }

    // Stop timestamp thread
    pthread_join(time_thread, NULL);

    pthread_mutex_destroy(&file_mutex);

    cleanup();
    return 0;
}

