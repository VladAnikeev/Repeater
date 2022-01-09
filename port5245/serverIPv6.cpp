#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <iostream>

#define PORT 5245
#define HEARING_TIME 2048 // 2048 - время слушания
#define PACKAGE_SIZE 256

int main()
{
    std::cout << "Server start\n";

    struct sockaddr_in6 servaddr6 = {};     //зануление
    servaddr6.sin6_family = AF_INET6;       // AF_INET - семейство IPv4
    servaddr6.sin6_port = htons(PORT);      //порт
    servaddr6.sin6_addr = IN6ADDR_ANY_INIT; //локальный ip,

    int sockfd6 = socket(AF_INET6, SOCK_STREAM, 0);
    //создание сокета, AF_INET6 - семейство IPv6, SOCK_STREAM - тип сокета
    if (-1 == sockfd6)
    {
        std::cout << "Error in socket creation\n";
        close(sockfd6);
        return 1;
    }
    std::cout << "A socket was created\n";

    int yes = 1;
    setsockopt(sockfd6, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

    //биндим
    if (0 != bind(sockfd6, (struct sockaddr *)&servaddr6, sizeof(servaddr6)))
    {
        std::cout << "Error in binding a socket to an IP address/Port pair\n";
        close(sockfd6);
        return 2;
    }
    std::cout << "Connected to the port\n";

    //слушаем
    if (-1 == listen(sockfd6, HEARING_TIME))
    {
        std::cout << "Error in listening\n";
        close(sockfd6);
        return 3;
    }
    std::cout << "Listening to the port...\n";

    //создаем новую структуру для клиента
    sockaddr_in6 clientInfo = {};
    socklen_t sizeClientInfo = sizeof(clientInfo);

    //потверждаем подключение клиента
    int connfd = accept(sockfd6, (sockaddr *)&clientInfo, &sizeClientInfo);
    if (-1 == connfd)
    {
        std::cout << "Error in connection confirmation\n";
        close(sockfd6);
        close(connfd);
        return 4;
    }
    std::cout << "Confirmation of connection\n";

    char buf[PACKAGE_SIZE]; //буфер для данных

    //слушаем первый пакет
    if (-1 == recv(connfd, (char *)&buf, PACKAGE_SIZE - 48, 0))
    {
        std::cout << "Error in receiving data\n";
        close(sockfd6);
        close(connfd);
        return 5;
    }
    std::cout << "client message: " << buf << std::endl;

    while (true)
    {   
        //слушаем
        if (-1 == recv(connfd, (char *)&buf, sizeof(buf), 0))
        {
            std::cout << "Error in receiving data\n";
            close(sockfd6);
            close(connfd);
            return 6;
        }
        std::cout << "client message: " << buf << std::endl;

        //отправка данных
        if (-1 == send(connfd, (char *)&buf, sizeof(buf), 0))
        {
            std::cout << "Error in sending data\n";
            close(sockfd6);
            close(connfd);
            return 7;
        }
    }
}
