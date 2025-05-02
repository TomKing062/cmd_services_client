#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#define BASE_PATH "/sdcard/Download/"
#define PID_FILE_NAME "daemon_pid"
#define COM_FILE_NAME "command"
#define RET_FILE_NAME "result"

#define PID_FILE BASE_PATH PID_FILE_NAME
#define COM_FILE BASE_PATH COM_FILE_NAME
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

void init_file()
{
    FILE *com_f = fopen(COM_FILE, "w");
    if (com_f == NULL)
    {
        perror("create RET_FILE failed");
        exit(EXIT_FAILURE);
    }
    fclose(com_f);
    FILE *ret_f = fopen(RET_FILE, "w");
    if (ret_f == NULL)
    {
        perror("create RET_FILE failed");
        exit(EXIT_FAILURE);
    }
    fclose(ret_f);
}

int wait_for_file(int fd, int timeout_ms)
{
    struct stat st, last_st;
    last_st.st_size = 0;

    int waited_ms = 0;
    while (waited_ms < timeout_ms)
    {
        if (fstat(fd, &st) != 0)
        {
            return -1;
        }
        if (last_st.st_size)
        {
            if (last_st.st_size == st.st_size)
                return 1;
            else
                waited_ms = 0;
        }
        last_st.st_size = st.st_size;
        usleep(100000);
        waited_ms += 100;
    }
    return 0;
}

pid_t check_daemon_exists()
{
    pid_t pid;
    FILE *pid_file = fopen(PID_FILE, "r");
    if (pid_file == NULL)
    {
        return -1;
    }
    if (fscanf(pid_file, "%d", &pid) != 1)
    {
        fclose(pid_file);
        return -1;
    }
    fclose(pid_file);

    if (pid == -1)
    {
        return -1;
    }
    if (kill(pid, 0) == 0)
    {
        return pid;
    }
    else
    {
        return -1;
    }
}

void start_daemon()
{
    pid_t pid, sid;
    pid = fork();
    if (pid < 0)
    {
        perror("fork failed");
        exit(EXIT_FAILURE);
    }
    if (pid > 0)
    {
        // exit(EXIT_SUCCESS);
        return;
    }
    sid = setsid();
    if (sid < 0)
    {
        perror("[CHILD] setsid failed");
        exit(EXIT_FAILURE);
    }
    if (chdir("/") < 0)
    {
        perror("[CHILD] chdir failed");
        exit(EXIT_FAILURE);
    }

    int client_fd, com_fd, ret_fd;
    client_fd = socket_local_client("cmd_skt", ANDROID_SOCKET_NAMESPACE_ABSTRACT, SOCK_STREAM);
    if (client_fd < 0)
    {
        perror("[CHILD] socket creation failed");
        exit(EXIT_FAILURE);
    }

    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    if (setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
    {
        perror("[CHILD] setsockopt failed");
        exit(EXIT_FAILURE);
    }

    FILE *pid_file = fopen(PID_FILE, "w");
    if (pid_file == NULL)
    {
        perror("[CHILD] create PID_FILE failed");
        exit(EXIT_FAILURE);
    }
    fprintf(pid_file, "%d\n", getpid());
    fclose(pid_file);

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    char buffer[BUFFER_SIZE];
    while (1)
    {
        com_fd = open(COM_FILE, O_RDONLY);
        if (com_fd < 0)
        {
            exit(EXIT_FAILURE);
        }

        int ret = wait_for_file(com_fd, 1000);
        if (ret < 0)
        {
            exit(EXIT_FAILURE);
        }
        else if (ret == 0)
        {
            sleep(1);
            continue;
        }

        ssize_t n = read(com_fd, buffer, BUFFER_SIZE);
        close(com_fd);
        com_fd = open(COM_FILE, O_WRONLY | O_TRUNC);
        close(com_fd);

        if (!strcmp(buffer, "kill-server\n"))
        {
            shutdown(client_fd, SHUT_WR);
            close(client_fd);
            client_fd = -1;
            unlink(PID_FILE);
            unlink(COM_FILE);
            unlink(RET_FILE);
            exit(EXIT_SUCCESS);
        }

        if (write(client_fd, buffer, n) < 0)
        {
            exit(EXIT_FAILURE);
        }
        ret_fd = open(RET_FILE, O_WRONLY | O_TRUNC);
        if (ret_fd < 0)
        {
            exit(EXIT_FAILURE);
        }

        ssize_t recv_bytes;
        while ((recv_bytes = read(client_fd, buffer, BUFFER_SIZE)) > 0)
        {
            if (write(ret_fd, buffer, recv_bytes) != recv_bytes)
            {
                close(ret_fd);
                exit(EXIT_FAILURE);
            }
        }
        close(ret_fd);
    }
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Invalid command\n");
        return;
    }
    pid_t g_pid = -1;
    if (strcmp(argv[1], "start-server") == 0)
    {
        if (check_daemon_exists() != -1)
        {
            printf("Daemon already running.\n");
        }
        else
        {
            printf("Starting new daemon...\n");
            init_file();
            start_daemon();
        }
        return 0;
    }
    else
    {
        if ((g_pid = check_daemon_exists()) == -1)
        {
            printf("No daemon running.\n");
            init_file();
            start_daemon();
            printf("New daemon started.\n");
        }
        char buffer[BUFFER_SIZE];
        size_t buffer_len = 0;
        int i, com_fd, ret_fd;
        for (i = 1; i < argc; i++)
        {
            size_t len = strlen(argv[i]);
            if (buffer_len + len + 1 < 256)
            {
                strcpy(buffer + buffer_len, argv[i]);
                buffer_len += len;
                if (i < argc - 1)
                {
                    buffer[buffer_len++] = ' ';
                }
                else
                    buffer[buffer_len++] = '\n';
            }
            else
            {
                fprintf(stderr, "Buffer size exceeded.\n");
                return;
            }
        }
        ret_fd = open(RET_FILE, O_WRONLY | O_TRUNC);
        if (ret_fd < 0)
        {
            exit(EXIT_FAILURE);
        }
        close(ret_fd);
        com_fd = open(COM_FILE, O_WRONLY | O_TRUNC);
        if (com_fd < 0)
        {
            perror("[MAIN] open com_fd failed");
            exit(EXIT_FAILURE);
        }
        if (write(com_fd, buffer, buffer_len) < 0)
        {
            perror("[MAIN] write to child failed");
        }
        close(com_fd);
        if (strcmp(argv[1], "kill-server") == 0)
        {
            while (1)
            {
                if (check_daemon_exists() == -1)
                    break;
            }
            printf("Child daemon exited (PID %d).\n", g_pid);
            return 0;
        }
        ret_fd = open(RET_FILE, O_RDONLY);
        if (ret_fd < 0)
        {
            perror("[MAIN] open RET_FILE failed");
            return 1;
        }

        int ret = wait_for_file(ret_fd, 5000);
        if (ret < 1)
        {
            exit(EXIT_FAILURE);
        }
        ssize_t bytes_read;
        while ((bytes_read = read(ret_fd, buffer, BUFFER_SIZE)) > 0)
        {
            fwrite(buffer, 1, bytes_read, stdout);
        }
        if (bytes_read < 0)
        {
            perror("[MAIN] read RET_FILE failed");
            close(ret_fd);
            exit(EXIT_FAILURE);
        }
        fwrite("\n", 1, 1, stdout);

        close(ret_fd);
    }
    return 0;
}
