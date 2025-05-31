#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#define BASE_PATH "/sdcard/Download/"
#define RET_FILE_NAME "result"

#define RET_FILE BASE_PATH RET_FILE_NAME
#define BUFFER_SIZE 4096
#define ANDROID_SOCKET_NAMESPACE_ABSTRACT 0
#define NO_ERR 0
#define CREATE_ERR -1
#define CONNECT_ERR -2
#define LINUX_MAKE_ADDRUN_ERROR -3
#define NO_LINUX_MAKE_ADDRUN_ERROR -4

int socket_local_client(const char *name, int namespaceId, int type)
{
    int socketID;
    int ret;
    socketID = socket(AF_LOCAL, type, 0);
    if (socketID < 0)
    {
        return CREATE_ERR;
    }
    ret = socket_local_client_connect(socketID, name, namespaceId, type);
    if (ret < 0)
    {
        close(socketID);
        return ret;
    }
    return socketID;
}

int socket_local_client_connect(int fd, const char *name, int namespaceId, int type)
{
    struct sockaddr_un addr;
    socklen_t socklen;
    size_t namelen;
    int ret;
    ret = socket_make_sockaddr_un(name, namespaceId, &addr, &socklen);
    if (ret < 0)
    {
        return ret;
    }
    if (connect(fd, (struct sockaddr *)&addr, socklen) < 0)
    {
        return CONNECT_ERR;
    }
    return fd;
}

int socket_make_sockaddr_un(const char *name, int namespaceId, struct sockaddr_un *p_addr, socklen_t *socklen)
{
    size_t namelen;
    memset(p_addr, 0, sizeof(*p_addr));
    namelen = strlen(name);
    if ((namelen + 1) > sizeof(p_addr->sun_path))
    {
        return LINUX_MAKE_ADDRUN_ERROR;
    }
    p_addr->sun_path[0] = 0;
    memcpy(p_addr->sun_path + 1, name, namelen);
    p_addr->sun_family = AF_LOCAL;
    *socklen = namelen + offsetof(struct sockaddr_un, sun_path) + 1;
    return NO_ERR;
}

int main()
{
    int client_fd;
    char buffer[BUFFER_SIZE];

    client_fd = socket_local_client("cmd_skt", ANDROID_SOCKET_NAMESPACE_ABSTRACT, SOCK_STREAM);
    if (client_fd < 0)
    {
        perror("socket creation failed");
        return EXIT_FAILURE;
    }

    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    if (setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
    {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    printf("Connected to the server.\n");
    fflush(stdout);

    FILE *ret_f = fopen(RET_FILE, "w");
    if (ret_f == NULL)
    {
        perror("create RET_FILE failed");
        exit(EXIT_FAILURE);
    }
    fclose(ret_f);

    while (1)
    {
        printf("Enter command (type 'exit' to quit):\n");
        fflush(stdout);
        if (fgets(buffer, 256, stdin) == NULL)
        {
            perror("Error reading input.\n");
            break;
        }

        buffer[strcspn(buffer, "\n")] = 0;

        if (strcmp(buffer, "exit") == 0)
        {
            printf("Exiting client.\n");
            fflush(stdout);
            break;
        }

        if (write(client_fd, buffer, strlen(buffer)) < 0)
        {
            perror("write failed");
            break;
        }

        int ret_fd = open(RET_FILE, O_WRONLY | O_TRUNC);
        if (ret_fd < 0)
        {
            exit(EXIT_FAILURE);
        }

        printf("Server response:\n");
        fflush(stdout);
        ssize_t recv_bytes;
        while ((recv_bytes = read(client_fd, buffer, BUFFER_SIZE)) > 0)
        {
            fwrite(buffer, 1, recv_bytes, stdout);
            printf("\n");
            fflush(stdout);
            if (write(ret_fd, buffer, recv_bytes) != recv_bytes)
            {
                close(ret_fd);
                exit(EXIT_FAILURE);
            }
        }
        close(ret_fd);
    }

    shutdown(client_fd, SHUT_WR);
    close(client_fd);
    client_fd = -1;
    return EXIT_SUCCESS;
}
