#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <iostream>

#define PORT 6000
#define HEARING_TIME 2048 // 2048 - время слушания
#define PACKAGE_SIZE 1024

int main()
{
    std::cout << "Server start\n";

    struct sockaddr_in6 servaddr = {}; //зануление
    servaddr.sin6_family = AF_INET6;   // AF_INET - семейство IPv4
    servaddr.sin6_port = htons(PORT);  //порт
    inet_pton(AF_INET6, "::1", &(servaddr.sin6_addr));

    int sockfd = socket(AF_INET6, SOCK_STREAM, 0);
    //создание сокета, AF_INET6 - семейство IPv6, SOCK_STREAM - тип сокета
    if (-1 == sockfd)
    {
        std::cout << "Error in socket creation\n";
        close(sockfd);
        return 1;
    }
    std::cout << "A socket was created\n";

    int yes = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

    //биндим
    if (0 != bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)))
    {
        std::cout << "Error in binding a socket to an IP address/Port pair\n";
        close(sockfd);
        return 2;
    }
    std::cout << "Connected to the port\n";

    //слушаем
    if (-1 == listen(sockfd, HEARING_TIME))
    {
        std::cout << "Error in listening\n";
        close(sockfd);
        return 3;
    }
    std::cout << "Listening to the port...\n";

    //создаем новую структуру для клиента
    sockaddr_in clientInfo = {};
    socklen_t sizeClientInfo = sizeof(clientInfo);

    //потверждаем подключение клиента
    int connfd = accept(sockfd, (sockaddr *)&clientInfo, &sizeClientInfo);
    if (-1 == connfd)
    {
        std::cout << "Error in connection confirmation\n";
        close(sockfd);
        close(connfd);
        return 4;
    }
    std::cout << "Confirmation of connection\n";

    char buf[PACKAGE_SIZE] = {0}; //буфер для данных
    std::string string_buf;
    //слушаем первый пакет
    while (true)
    {
        //слушаем
        if (-1 == recv(connfd, (char *)&buf, sizeof(buf), 0))
        {
            std::cout << "Error in receiving data\n";
            close(sockfd);
            close(connfd);
            return 6;
        }
        std::cout << "client message: " << buf << std::endl;
        string_buf += "server response ";
        string_buf += buf;
        memset(buf, 0, PACKAGE_SIZE);
        string_buf.copy(buf, string_buf.size());
        //отправка данных
        if (-1 == send(connfd, (char *)&buf, sizeof(buf), 0))
        {
            std::cout << "Error in sending data\n";
            close(sockfd);
            close(connfd);
            return 7;
        }
        string_buf.clear();
    }
}
