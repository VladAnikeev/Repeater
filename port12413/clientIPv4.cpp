#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <iostream>
#include <cstring>
#include <chrono>
#include <thread>

#define PORT_REPEATER 9034
#define PORT_SERVER_IPV6 12413
#define IPV6 "::1"
#define PACKAGE_SIZE 256
#define SENDING_TIME 3000

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
    struct sockaddr_in servaddr = {};      //зануление
    servaddr.sin_family = AF_INET;         // AF_INET - семейство IPv4
    servaddr.sin_addr.s_addr = INADDR_ANY; //локальный ip
    servaddr.sin_port = htons(PORT_REPEATER);       //порт

    //соединение с сервером
    if (-1 == connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)))
    {
        std::cout << "Error connecting to the server\n";
        close(sockfd);
        return 2;
    }
    std::cout << "Connecting to the server...\n";

    //данные для отправки
    struct package
    {
        char IPv6[INET6_ADDRSTRLEN]; // INET6_ADDRSTRLEN = 46
        unsigned short port;         //порт
        char str[PACKAGE_SIZE - 48]; //доп данные
    };

    package forIPv4{};

    strcpy(forIPv4.str, IPV6);           //данные
    forIPv4.port = PORT_SERVER_IPV6;     //порт
    strcpy(forIPv4.str, "package zero"); //данные

    //отправка первого пакета данных
    if (-1 == send(sockfd, (char *)&forIPv4, sizeof(forIPv4), 0))
    {
        std::cout << "Error in sending data\n";
        close(sockfd);
        return 3;
    }
    std::cout << "Data sent package zero\n";

    
    char buf[PACKAGE_SIZE];//буфер для данных
    std::string string_buf = "package ";//шаблон для пакета
    int i = 1;

    while (true)//бесконечный цикл
    {
        strcpy(buf, (string_buf + std::to_string(i)).c_str()); //делаем данные

        //отправляем
        if (-1 == send(sockfd, (char *)&buf, sizeof(buf), 0))
        {
            std::cout << "Error in sending data";
            close(sockfd);
            return 4;
        }
        std::cout << "sent data: " << buf << std::endl;
        //принимаем
        if (-1 == recv(sockfd, (char *)&buf, sizeof(buf), 0))
        {
            std::cout << "Error in receiving data\n";

            return 5;
        }
        std::cout << "incoming data: " << buf << std::endl;
        i++;
        std::this_thread::sleep_for(std::chrono::milliseconds(SENDING_TIME));
    }
    close(sockfd); //закрываем соединение с сервером
}