#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <iostream>

#define PORT_REPEATER 9034
#define PACKAGE_SIZE 256

int main()
{
    std::cout << "Client start\n";

    //создание сокета, AF_INET - семейство IPv4, SOCK_STREAM - тип сокета
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == sockfd)
    {
        std::cout << "Error in socket creation";
        close(sockfd);
        return 1;
    }
    std::cout << "A socket was created";

    //создание структуры клиента
    struct sockaddr_in servaddr = {};         //зануление
    servaddr.sin_family = AF_INET;            // AF_INET - семейство IPv4
    servaddr.sin_addr.s_addr = INADDR_ANY;    //локальный ip
    servaddr.sin_port = htons(PORT_REPEATER); //порт

    //соединение с сервером
    if (-1 == connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)))
    {
        std::cout << "Error connecting to the server\n";
        close(sockfd);
        return 2;
    }
    std::cout << "Connecting to the server...\n";

    char buf[PACKAGE_SIZE];

    //требуем пакеты и зависаем
    if (-1 == recv(sockfd, (char *)&buf, sizeof(buf), 0))
    {
        std::cout << "Error in receiving data\n";

        return 3;
    }
}