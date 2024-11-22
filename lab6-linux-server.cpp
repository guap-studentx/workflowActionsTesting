
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>

#include <iostream>
#include <cstdlib>
#include <cstring>
#include <list>

#undef NI_MAXHOST
#define NI_MAXHOST 63
#define DEFAULT_BUFSIZE 512

/* ============== Service Logic ============== */

struct ServiceUser
{
    std::string username;
    std::string ip_address;
    int port = 0;
};

std::list<ServiceUser> usersList;

struct CLIENT_CTX
{
    int socket;
    sockaddr addr;
    socklen_t addrlen;
    char *buffer;
    char IPv4[INET_ADDRSTRLEN];
    int port;
    char hostname[NI_MAXHOST];
};

extern int create_listenfd(uint16_t server_port, int backlog);
extern int run_dispatcher(int listenfd);
extern void process_client(CLIENT_CTX *pCtx, ssize_t bRecived);

int main(int argc, char *argv[])
{
    int servicePort = 0;
    /* -- Обработка входных данных -- */
    if (argc <= 1)
    {
        std::cerr << "Usage: server <port>" << std::endl;
        return 1;
    }
    servicePort = atoi(argv[1]);
    if (servicePort <= 0 || servicePort >= UINT16_MAX)
    {
        std::cerr << "the specified port is incorrect" << std::endl;
        return 1;
    }

    /* -- Создание слушающего сокета -- */
    int listen_fd = create_listenfd(servicePort, 1024);
    printf("[ INFO ] Server started listening on port: %d\n", servicePort);

    // Запуск Диспетчера
    // -----------------
    run_dispatcher(listen_fd);

    /* -- Очистка -- */
    close(listen_fd);

    return 0;
}

int create_listenfd(uint16_t server_port, int backlog)
{
    int listen_fd;
    struct sockaddr_in server_address_settings;
    int result;
    int __opt_val = 1;

    /* Определение адреса, для сокета сервера сокета */
    server_address_settings.sin_family = AF_INET;
    server_address_settings.sin_port = htons(server_port);
    server_address_settings.sin_addr.s_addr = INADDR_ANY;

    /* Создание эфимерного** сокета */
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0)
    {
        fprintf(stderr, "%s:%d socket()\n", __func__, __LINE__);
        perror("Details : ");
        exit(1);
    }

    /* Предотвращение ошибки "Адрес уже используется" */
    result = setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &__opt_val, sizeof(int));
    if (result == -1)
    {
        fprintf(stderr, "%s:%d setsockopt()\n", __func__, __LINE__);
        perror("Details : ");
        exit(1);
    }

    /* Привязка сокета к данным адреса сервера */
    result = bind(listen_fd, (struct sockaddr *)&server_address_settings, sizeof(server_address_settings));
    if (result == -1)
    {
        fprintf(stderr, "%s:%d bind()\n", __func__, __LINE__);
        perror("Details : ");
        exit(1);
    }

    /* Превратить сокет в слушающий */
    result = listen(listen_fd, backlog);
    if (result == -1)
    {
        fprintf(stderr, "%s:%d listen()\n", __func__, __LINE__);
        perror("Details : ");
        exit(1);
    }

    return listen_fd;
}

int run_dispatcher(int listenfd)
{
    int result;
    while (true)
    {
        CLIENT_CTX *pCtx = new CLIENT_CTX;
        memset(pCtx, 0, sizeof(CLIENT_CTX));
        pCtx->addrlen = sizeof(sockaddr);
        pCtx->buffer = new char[DEFAULT_BUFSIZE]{0};

        // Принимаем нового клиента
        // ------------------------

        while (true)
        {
            pCtx->socket = accept(listenfd, &(pCtx->addr), &(pCtx->addrlen));
            if (pCtx->socket == -1)
            {
                fprintf(stderr, "%s:%d accept()\n", __func__, __LINE__);
                perror("Details : ");
                continue;
            }
            else
            {
                break;
            }
        }

        // Получаем информацию о новом клиенте
        // -----------------------------------
        sockaddr_in *addr_in = reinterpret_cast<sockaddr_in *>(&(pCtx->addr));
        // 1) Вычисляем IPv4 адресс
        inet_ntop(AF_INET, &(addr_in->sin_addr), pCtx->IPv4, INET_ADDRSTRLEN);
        // 2) Вычисляем порт
        pCtx->port = ntohs(addr_in->sin_port);
        // 3) Вычисляем hostname
        result = getnameinfo(&(pCtx->addr), sizeof(sockaddr_in), pCtx->hostname,
                             NI_MAXHOST, nullptr, 0, 0);
        if (result != 0)
        {
            strcpy(pCtx->hostname, "unkown");
        }

        printf("<NEW> (%s|%s:%d) - Connected\n",
               pCtx->hostname,
               pCtx->IPv4,
               pCtx->port);

        // Обработать клиента
        // ------------------

        ssize_t bRecived = 0;
        while (1)
        {
            memset(pCtx->buffer, 0, DEFAULT_BUFSIZE);
            bRecived = recv(pCtx->socket, pCtx->buffer, DEFAULT_BUFSIZE, 0);
            if (bRecived == -1)
            {
                fprintf(stderr,
                        "[ERROR] Can't recv message from (%s|%s:%d)\n",
                        pCtx->hostname,
                        pCtx->IPv4,
                        pCtx->port);
                perror("Details : ");
                break;
            }
            else if (bRecived == 0)
            {
                printf("<OUT> (%s|%s:%d) - Disconnected\n",
                       pCtx->hostname,
                       pCtx->IPv4,
                       pCtx->port);
                break;
            }

            // Обработать клиента
            process_client(pCtx, bRecived);

            // Отправить ответ клиенту
            bRecived = send(pCtx->socket, pCtx->buffer, strlen(pCtx->buffer), 0);
            if (bRecived == 0 || bRecived == -1)
            {
                fprintf(stderr,
                        "[ERROR] Can't send message to (%s|%s:%d)",
                        pCtx->hostname,
                        pCtx->IPv4,
                        pCtx->port);
                perror("Details : ");
                break;
            }
        }

        // CleanUp
        // -------
        shutdown(pCtx->socket, SHUT_RDWR);
        close(pCtx->socket);
        delete pCtx->buffer;
        delete pCtx;
    }

    return 0;
}

void process_client(CLIENT_CTX *pCtx, ssize_t bRecived)
{
    printf(" ~ Request: (%s|%s:%d) -> {%s}\n",
           pCtx->hostname, pCtx->IPv4,
           pCtx->port, pCtx->buffer);
    fflush(stdout);

    // Parse command
    if (bRecived < 4)
    {
        memset(pCtx->buffer, 0, DEFAULT_BUFSIZE);
        snprintf(pCtx->buffer, DEFAULT_BUFSIZE, "error: unkown command");
        return;
    }

    // Reg command
    // -----------
    if (0 == strncmp(pCtx->buffer, "reg", 3))
    {
        /* получаем имя регистрируемого пользователя */
        size_t len = strlen(pCtx->buffer);
        if (len < (3 + 1 + 3) || len > 64)
        {
            memset(pCtx->buffer, 0, DEFAULT_BUFSIZE);
            snprintf(pCtx->buffer, DEFAULT_BUFSIZE, "error: wrong format");
            return;
        }

        /* Проверяем если ли пользователь с таким именем */
        std::string usrname = &(pCtx->buffer[4]);
        for (auto it : usersList)
        {
            if (it.username == usrname)
            {
                memset(pCtx->buffer, 0, DEFAULT_BUFSIZE);
                snprintf(pCtx->buffer, DEFAULT_BUFSIZE, "1 ");
                return;
            }
        }

        /* Регистрируем нового */
        ServiceUser newUser;
        newUser.username = usrname;
        newUser.ip_address = pCtx->IPv4;
        newUser.port = pCtx->port;

        usersList.push_back(newUser);
        memset(pCtx->buffer, 0, DEFAULT_BUFSIZE);
        snprintf(pCtx->buffer, DEFAULT_BUFSIZE, "0 ");
        return;
    }
    else if (0 == strncmp(pCtx->buffer, "list", 4))
    { // List command
        memset(pCtx->buffer, 0, DEFAULT_BUFSIZE);
        snprintf(pCtx->buffer, DEFAULT_BUFSIZE, "Users: %lu\n",
                 usersList.size());
        for (auto it : usersList)
        {
            size_t currlen = strlen(pCtx->buffer);
            snprintf(pCtx->buffer + currlen,
                     DEFAULT_BUFSIZE,
                     "%s\t%s:%d\n",
                     it.username.c_str(),
                     it.ip_address.c_str(),
                     it.port);
        }

        return;
    }
    else if (0 == strncmp(pCtx->buffer, "quit", 4))
    { // quit command
        memset(pCtx->buffer, 0, DEFAULT_BUFSIZE);
        snprintf(pCtx->buffer, DEFAULT_BUFSIZE, "0 ");
        return;
    }
    else
    {
        memset(pCtx->buffer, 0, DEFAULT_BUFSIZE);
        snprintf(pCtx->buffer, DEFAULT_BUFSIZE, "error: unkown command");
        return;
    }
}
