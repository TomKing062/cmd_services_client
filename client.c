#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define BUFFER_SIZE 4096
#define ANDROID_SOCKET_NAMESPACE_ABSTRACT 0
#define NO_ERR 0
#define CREATE_ERR -1
#define CONNECT_ERR -2
#define LINUX_MAKE_ADDRUN_ERROR -3
#define NO_LINUX_MAKE_ADDRUN_ERROR -4
#define CLOSE_ERR -5
#define HAVE_LINUX_LOCAL_SOCKET_NAMESPACE "linux_local_socket_namespace"
#define MEM_ZERO(pDest, destSize) memset(pDest, 0, destSize)
int socket_local_client(const char *name, int namespaceId, int type)
{
    int socketID;
    int ret;

    socketID = socket(AF_LOCAL, type, 0);
    if(socketID < 0)
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

/* 连接到相应的fileDescriptor上 */
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

    if(connect(fd, (struct sockaddr *) &addr, socklen) < 0)
    {
        return CONNECT_ERR;
    }

    return fd;
}

/* 构造sockaddr_un */
int socket_make_sockaddr_un(const char *name, int namespaceId, struct sockaddr_un *p_addr, socklen_t *socklen)
{
    size_t namelen;

    MEM_ZERO(p_addr, sizeof(*p_addr));
#ifdef HAVE_LINUX_LOCAL_SOCKET_NAMESPACE

    namelen  = strlen(name);

    // Test with length +1 for the *initial* '\0'.
    if ((namelen + 1) > sizeof(p_addr->sun_path))
    {
        return LINUX_MAKE_ADDRUN_ERROR;
    }
    p_addr->sun_path[0] = 0;
    memcpy(p_addr->sun_path + 1, name, namelen);

#else

    namelen = strlen(name) + strlen(FILESYSTEM_SOCKET_PREFIX);

    /* unix_path_max appears to be missing on linux */
    if (namelen > (sizeof(*p_addr) - offsetof(struct sockaddr_un, sun_path) - 1))
    {
        return NO_LINUX_MAKE_ADDRUN_ERROR;
    }

    strcpy(p_addr->sun_path, FILESYSTEM_SOCKET_PREFIX);
    strcat(p_addr->sun_path, name);

#endif

    p_addr->sun_family = AF_LOCAL;
    *socklen = namelen + offsetof(struct sockaddr_un, sun_path) + 1;

    return NO_ERR;
}


int main() {
    int client_fd;
    char buffer[BUFFER_SIZE];

    // 创建客户端 socket
    client_fd = socket_local_client("cmd_skt", ANDROID_SOCKET_NAMESPACE_ABSTRACT, SOCK_STREAM);
    if (client_fd < 0) {
        perror("socket creation failed");
        return EXIT_FAILURE;
    }

    printf("Connected to the server.\n");
	fflush(stdout);
    while (1) {
        printf("Enter a command to send (type 'exit' to quit): ");
		fflush(stdout);
        memset(buffer, 0, BUFFER_SIZE);
        if (fgets(buffer, BUFFER_SIZE, stdin) == NULL) {
            printf("Error reading input.\n");
			fflush(stdout);
            break;
        }

        // 去掉换行符
        buffer[strcspn(buffer, "\n")] = 0;

        // 检查退出条件
        if (strcmp(buffer, "exit") == 0) {
            printf("Exiting client.\n");
			fflush(stdout);
            break;
        }

        // 发送消息到服务器
        if (write(client_fd, buffer, strlen(buffer)) < 0) {
            perror("write failed");
            break;
        }

        // 接收服务器响应
        memset(buffer, 0, BUFFER_SIZE);
        if (read(client_fd, buffer, BUFFER_SIZE) < 0) {
            perror("read failed");
            break;
        }

        printf("Server response: %s\n", buffer);
		fflush(stdout);
    }

    // 关闭连接
    if(close(client_fd)<0)
	{
		perror("close client failed");
		return EXIT_FAILURE;
	}
    return EXIT_SUCCESS;
}
