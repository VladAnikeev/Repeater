#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <iostream>
#include <map>
#include <cstring>
#include <errno.h>
#include <chrono>
#include <fstream>
#include "Constants.h"

void printIP(sockaddr *client, std::ofstream &log_file)
{
    char clientIP[INET6_ADDRSTRLEN];  // INET6_ADDRSTRLEN = 46
    if (client->sa_family == AF_INET) // AF_INET семейство IPv4
    {                                 //семейство IPv4
        inet_ntop(client->sa_family, &(((sockaddr_in *)client)->sin_addr),
                  clientIP, INET6_ADDRSTRLEN); //записоваем char IP
    }
    else
    { //семейство IPv6
        inet_ntop(client->sa_family, &(((sockaddr_in6 *)client)->sin6_addr),
                  clientIP, INET6_ADDRSTRLEN); //записоваем char IP
    }
    std::cout << "IP: " << clientIP << std::endl;
    log_file << "IP: " << clientIP << std::endl;
}

int main()
{

    std::cout << "Program start\n";

    std::ofstream log_file("logfile.txt");
    if (!log_file)
    {
        std::cout << "Error: could not open the file" << std::endl;
    }

    //создаем структуру для первичных адресных данных, сразу зануляем
    addrinfo initial = {};
    initial.ai_family = AF_UNSPEC;     //семейство IPv4 и IPv6
    initial.ai_socktype = SOCK_STREAM; //потоковый сокет(tcp)
    initial.ai_flags = AI_PASSIVE;     //флаг на использование локального ip

    addrinfo *ready = nullptr; //готовая структура со всеми данными
    if (int error = getaddrinfo(NULL, PORT, &initial, &ready))
    {
        std::cout << "Error: " << gai_strerror(error) << std::endl;
        log_file << "Error: " << gai_strerror(error) << std::endl;
        return 1;
    }
    std::cout << "Basic setup passed\n";
    log_file << "Basic setup passed\n";

    int listener; // дескриптор слушаемого сокета

    addrinfo *temporary = nullptr; //временная структура
    for (temporary = ready; temporary != NULL; temporary = temporary->ai_next)
    {
        //создаем слущаюший сокет
        listener = socket(temporary->ai_family,
                          temporary->ai_socktype, temporary->ai_protocol);
        if (listener < 0)
        {
            continue;
        }

        //настройка сокета, для повторного использование локального IP и порта внутренний сети
        //для теста
        int yes = 1;
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        //привязоваем порт к сокету
        if (bind(listener, temporary->ai_addr, temporary->ai_addrlen) < 0)
        {
            close(listener);
            continue;
        }
        break;
    }

    if (temporary == NULL)
    {
        std::cout << "Error: failed to bind, errno " << errno << std::endl;
        log_file << "Error: failed to bind, errno " << errno << std::endl;
        return 2;
    }
    std::cout << "Successful port binding\n";
    log_file << "Successful port binding\n";

    freeaddrinfo(ready); //закончили с данными с созданием сокета

    //слушаем, 10 клиентов
    if (listen(listener, MAX_CLIENTS) == -1)
    {
        std::cout << "Error: listen, errno " << errno << std::endl;
        log_file << "Error: listen, errno " << errno << std::endl;
        return 3;
    }
    std::cout << "The audition went well\n\n";
    log_file << "The audition went well\n\n";

    //пакет для клиента подключающего первый раз
    struct IpPortData
    {
        char IPv6[INET6_ADDRSTRLEN];  // INET6_ADDRSTRLEN = 46, хранит адресс
        unsigned short port;          //порт
        char data[PACKAGE_SIZE - 48]; //доп данные
        //буфер 256, short 2, IPv6 46
        // 256 - 46 - 2 = 208
    };
    IpPortData firstPacket = {};

    //для select()
    fd_set master;             // главный список файловых дескрипторов
    FD_ZERO(&master);          // очистка главного массивов
    FD_SET(listener, &master); // добавить слушателя в главный массив
    fd_set read_fds;           // временный список файловых дескрипторов для select()
    FD_ZERO(&read_fds);        // очистка временного массивов

    int fd_max = listener;        // максимальный номер файлового дескриптора
    int new_fd;                   // новопринятый дескриптор сокета
    char buf[PACKAGE_SIZE];       // буфер для данных клиента
    sockaddr_storage client_addr; // адрес клиента, sockaddr_storage - кроссплатформа для IPv4 и IPv6

    int incoming_bytes; //пришедшие байты

    struct clientData
    {
        int fd_server;
        std::chrono::system_clock::time_point start;
    };
    //ассоциативный массивы c сортировкой ключей
    std::map<int, clientData> fdClientServer; //связка клиент сервер
    std::map<int, int> fdServerClient;        //связка сервер клиент

    struct timeval tv = {}; //бесполезный таймер

    // главный цикл
    while (true)
    {
        read_fds = master; // копируем

        //функция мониторит массивы файловых дескрипторов
        if (select(fd_max + 1, &read_fds, NULL, NULL, &tv) == -1)
        {
            std::cout << "Error: select, errno " << errno << std::endl;
            log_file << "Error: select, errno " << errno << std::endl;
            return 4;
        }
        //обрабатываем массив
        for (int i = 0; i <= fd_max; ++i)
        {
            //вовращает true если i(файловый дескриптор) есть в read_fds
            if (FD_ISSET(i, &read_fds))
            {
                if (i == listener) //сокет этого сервера
                {
                    // обрабатываем новые подключения
                    socklen_t addrlen = sizeof client_addr;
                    new_fd = accept(listener, (struct sockaddr *)&client_addr, &addrlen);
                    if (new_fd == -1)
                    {
                        std::cout << "Error: accept, errno " << errno << std::endl;
                        log_file << "Error: accept, errno " << errno << std::endl;
                    }
                    else
                    {
                        FD_SET(new_fd, &master); //добавляем в главный массив
                        if (new_fd > fd_max)     //меняем макс файловый дескриптор
                        {
                            fd_max = new_fd;
                        }
                        fdClientServer[new_fd].fd_server = 0;                            //клиент без сервера
                        fdClientServer[new_fd].start = std::chrono::system_clock::now(); //время создание
                        std::cout << "New client #id" << new_fd << ", ";                 //выводим IP клиента
                        log_file << "New client #id" << new_fd << ", ";
                        printIP((struct sockaddr *)&client_addr, log_file); //выводим адрес клиента
                    }
                }
                else
                {
                    // получение данных от клиента
                    if ((incoming_bytes = recv(i, (char *)&buf, sizeof buf, 0)) <= 0)
                    {
                        //соединение закрыто клиентом
                        if (incoming_bytes == 0)
                        {
                            // соединение закрыто
                            std::cout << "Communication with id#" << i
                                      << " is interrupted\n\n";
                            log_file << "Communication with id#" << i
                                     << " is interrupted\n\n";
                        }
                        else // это уже ошибка
                        {
                            std::cout << "Error: recv, errno " << errno << " client id#" << i << std::endl;
                            log_file << "Error: recv, errno " << errno << " client id#" << i << std::endl;
                        }
                        FD_CLR(i, &master);   //удаляем из главного массива
                        FD_CLR(i, &read_fds); //удаляем из массива для чтения
                        close(i);             //закрываем соединие

                        //если это сервер
                        if ((fdServerClient.find(i) != fdServerClient.end()))
                        {
                            FD_CLR(fdServerClient[i], &master);   //удаляем клиента из главного массива
                            FD_CLR(fdServerClient[i], &read_fds); //удаляем клиенты из массива для чтения
                            close(fdServerClient[i]);             //закрываем сокеты клиента

                            std::cout << "Communication with client id#" << fdServerClient[i]
                                      << " is interrupted\n\n";
                            log_file << "Communication with client id#" << fdServerClient[i]
                                     << " is interrupted\n\n";

                            fdClientServer.erase(fdServerClient[i]); //удаляем клиента из ассоциативного массива
                            fdServerClient.erase(i);                 //удаляем клиента из ассоциативного массива
                        }
                        if ((fdClientServer.find(i) != fdClientServer.end()))
                        {
                            if (fdClientServer[i].fd_server)
                            {

                                FD_CLR(fdClientServer[i].fd_server, &master);   //удаляем сервер из главного массива
                                FD_CLR(fdClientServer[i].fd_server, &read_fds); //удаляем сервер из массива для чтения

                                std::cout << "Communication with server id#" << fdClientServer[i].fd_server
                                          << " is interrupted\n\n";
                                log_file << "Communication with server id#" << fdClientServer[i].fd_server
                                         << " is interrupted\n\n";

                                close(fdClientServer[i].fd_server);                //закрываем сокеты сервера
                                fdServerClient.erase(fdClientServer[i].fd_server); //удаляем сервер из ассоциативного массива
                            }
                            fdClientServer.erase(i); //удаляем клиент из ассоциативного массива
                        }
                        //сервер -клиент
                    }
                    else //успешно получены данные, тогда обрабатываем
                    {
                        //проверяем это клиент
                        if (fdClientServer.find(i) != fdClientServer.end())
                        {
                            if (fdClientServer[i].fd_server == 0) //если у клиента нет сервера
                            {
                                firstPacket = *((IpPortData *)&buf); //разбиваем пакет на структуру

                                //выводим струтуру
                                std::cout << "Client message:\n";
                                std::cout << "IP - " << firstPacket.IPv6 << std::endl;
                                std::cout << "Port - " << firstPacket.port << std::endl;
                                std::cout << "Data - " << firstPacket.data << std::endl;

                                log_file << "Client message:\n";
                                log_file << "IP - " << firstPacket.IPv6 << std::endl;
                                log_file << "Port - " << firstPacket.port << std::endl;
                                log_file << "Data - " << firstPacket.data << std::endl;

                                //создаем данные для подключаемся клиенту
                                sockaddr_in6 servaddr6 = {};                                   //зануление
                                servaddr6.sin6_family = AF_INET6;                              // AF_INET6 - семейство IPv6
                                inet_pton(AF_INET6, firstPacket.IPv6, &(servaddr6.sin6_addr)); //Заносим IPv6 адрес сервера
                                servaddr6.sin6_port = htons(firstPacket.port);                 //порт

                                int new_fd = socket(AF_INET6, SOCK_STREAM, 0); //создаем сокет

                                //подключаемся к серверу
                                if (-1 == connect(new_fd, (struct sockaddr *)&servaddr6, sizeof(servaddr6)))
                                {
                                    std::cout << "Error: connecting to the server, errno " << errno << std::endl;
                                    log_file << "Error: connecting to the server, errno " << errno << std::endl;
                                    FD_CLR(i, &master);   //удаляем из главного массива
                                    FD_CLR(i, &read_fds); //удаляем из массива для чтения
                                    //закрываем все в сокеты
                                    close(new_fd);
                                    close(i);
                                    //убираем с ассоциативного массивов
                                    fdClientServer.erase(i);
                                }
                                else //удачное подключение
                                {
                                    std::cout << "Connecting to the server id#" << new_fd << std::endl
                                              << std::endl;
                                    log_file << "Connecting to the server id#" << new_fd << std::endl
                                             << std::endl;

                                    //отправляем серверу данные
                                    if (-1 == send(new_fd, (char *)&firstPacket.data, sizeof(firstPacket.data), 0))
                                    {
                                        std::cout << "Error: sending data, errno " << errno << std::endl;
                                        log_file << "Error: sending data, errno " << errno << std::endl;
                                        FD_CLR(i, &master);   //удаляем из главного массива
                                        FD_CLR(i, &read_fds); //удаляем из массива для чтения
                                        //закрываем все в сокеты
                                        close(new_fd);
                                        close(i);
                                        //убираем с ассоциативного массивов
                                        fdClientServer.erase(i);
                                    }
                                    else //если присоединились и отправили данные – то добавляем в массивы
                                    {
                                        FD_SET(new_fd, &master); //добавляем в главный массив
                                        if (new_fd > fd_max)     //меняем макс файловый дискриптор
                                        {
                                            fd_max = new_fd;
                                        }
                                        fdServerClient[new_fd] = i;                                 //серверу привязываем клиента
                                        fdClientServer[i].fd_server = new_fd;                       //клиенту привязываем сервер
                                        fdClientServer[i].start = std::chrono::system_clock::now(); //время реагирование клиента
                                    }
                                }
                            }
                            else //если уже у клиента есть присоединенный сервер
                            {
                                std::cout << "Cliend id#" << i << " message:" << std::endl;
                                std::cout << buf << std::endl;

                                log_file << "Cliend id#" << i << " message:" << std::endl;
                                log_file << buf << std::endl;

                                //отправляем данные для клиента
                                if (-1 == send(fdClientServer[i].fd_server, (char *)&buf, sizeof(buf), 0))
                                {
                                    //удаление полное
                                    std::cout << "Error: sending data, errno " << errno << std::endl;
                                    log_file << "Error: sending data, errno " << errno << std::endl;

                                    FD_CLR(i, &master);                             //удаляем из главного массива
                                    FD_CLR(i, &read_fds);                           //удаляем из массива для чтения
                                    FD_CLR(fdClientServer[i].fd_server, &read_fds); //удаляем сервер из массива для чтения
                                    FD_CLR(fdClientServer[i].fd_server, &master);   //удаляем сервер из главного массива
                                    //закрываем сокеты
                                    close(fdClientServer[i].fd_server); //сервер
                                    close(i);                           //клиент
                                    //убираем с ассоциативного массива
                                    fdServerClient.erase(fdClientServer[i].fd_server);
                                    fdClientServer.erase(i);
                                }
                                else
                                {
                                    fdClientServer[i].start = std::chrono::system_clock::now(); //время реагирование клиента
                                    std::cout << "Sending data to the server #id" << fdClientServer[i].fd_server << std::endl
                                              << std::endl;
                                    log_file << "Sending data to the server #id" << fdClientServer[i].fd_server << std::endl
                                             << std::endl;
                                }
                            }
                        }
                        else //тут принимаем сообщения от сервера
                        {
                            std::cout << "Server id#" << i << " message:" << std::endl;
                            std::cout << buf << std::endl;
                            log_file << "Server id#" << i << " message:" << std::endl;
                            log_file << buf << std::endl;

                            if (-1 == send(fdServerClient[i], (char *)&buf, sizeof(buf), 0))
                            {
                                //удаление полное
                                std::cout << "Error: sending data, errno " << errno << std::endl;
                                log_file << "Error: sending data, errno " << errno << std::endl;
                                FD_CLR(i, &master);                   //удаляем из главного массива
                                FD_CLR(i, &read_fds);                 //удаляем из массива для чтения
                                FD_CLR(fdServerClient[i], &master);   //удаляем клиента из массива для чтения
                                FD_CLR(fdServerClient[i], &read_fds); //удаляем клиента из главного массива

                                close(fdServerClient[i]); //клиент
                                close(i);                 //сервер
                                //убираем с ассоциативных массивов
                                fdClientServer.erase(fdServerClient[i]);
                                fdServerClient.erase(i);
                            }
                            else
                            {
                                std::cout << "Sending data to the client #id" << fdServerClient[i] << std::endl
                                          << std::endl;
                                log_file << "Sending data to the client #id" << fdServerClient[i] << std::endl
                                         << std::endl;
                            }
                        }
                    }
                }
            }

            else if ((i > 3) && FD_ISSET(i, &master) && (fdClientServer.find(i) != fdClientServer.end()))
            { //если это не слушатель, если есть в главном массиве файловых дескрипторов, если есть в ассоциативном массиве клиентов

                std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

                if (std::chrono::duration_cast<std::chrono::milliseconds>(now - fdClientServer[i].start).count() > WAITING_TIME)
                { //если таймер истек, таймер на  30 секунд

                    FD_CLR(i, &master);   //удаляем из главного массива
                    FD_CLR(i, &read_fds); //удаляем из массива для чтения

                    //закрываем сокеты клиента
                    close(i);

                    if (fdClientServer[i].fd_server)
                    {
                        FD_CLR(fdClientServer[i].fd_server, &master);      //удаляем сервер из главного массива
                        FD_CLR(fdClientServer[i].fd_server, &read_fds);    //удаляем сервер из массива для чтения
                        close(fdClientServer[i].fd_server);                //закрываем сокеты сервера
                        fdServerClient.erase(fdClientServer[i].fd_server); //удаляем сервер из ассоциативного массива
                    }
                    fdClientServer.erase(i); //удаляем клиент из ассоциативного массива

                    std::cout << "Disconnecting for a long wait client #id" << i << std::endl
                              << std::endl;
                    log_file << "Disconnecting for a long wait client #id" << i << std::endl
                             << std::endl;
                }
            }
        }
    }
    return 0;
}
