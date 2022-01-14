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
#include "hex.h"

using namespace std::chrono;

Hex toHex;

void printIP(sockaddr *client, std::ofstream &log_file)
{
    char clientIP[INET6_ADDRSTRLEN];  // INET6_ADDRSTRLEN = 46
    if (client->sa_family == AF_INET) // AF_INET семейство IPv4
    {                                 //семейство IPv4
        inet_ntop(client->sa_family, &(((sockaddr_in *)client)->sin_addr),
                  clientIP, INET6_ADDRSTRLEN); //записоваем char IP
    }
    else
    { //семейство IPv6/home/user/Рабочий стол/repeater/Repeater/Repeater.cpp
        inet_ntop(client->sa_family, &(((sockaddr_in6 *)client)->sin6_addr),
                  clientIP, INET6_ADDRSTRLEN); //записоваем char IP
    }
    std::cout << "IP: " << clientIP << std::endl;
    log_file << toHex.stringToHEx("IP: " + std::string(clientIP)) << std::endl;
}

//логирование ошибок
void errorLog(const char *nameError, std::ostream &logFile)
{
    std::cout << nameError << errno << std::endl;
    logFile << toHex.stringToHEx(nameError + toHex.intToHEX(errno)) << std::endl;
}

//логирование
void logging(const char *massage, std::ofstream &log_file)
{
    std::cout << massage << std::endl;
    log_file << toHex.stringToHEx(massage) << std::endl;
}
void logging(const char *massage, int id, std::ofstream &log_file)
{
    std::cout << massage << id << std::endl;
    log_file << toHex.stringToHEx(massage + toHex.intToHEX(id)) << std::endl;
}
void logging(const char *massage1, int id,
             const char *massage2, std::ofstream &log_file)
{
    std::cout << massage1 << id << massage2 << std::endl;
    log_file << toHex.stringToHEx(massage1 + toHex.intToHEX(id) + massage2) << std::endl;
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
        log_file << toHex.stringToHEx("Error: " + std::string(gai_strerror(error))) << std::endl;
        return 1;
    }
    logging("Basic setup passed", log_file);

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
        errorLog("Error: failed to bind, errno ", log_file);
        return 2;
    }
    logging("Successful port binding", log_file);

    freeaddrinfo(ready); //закончили с данными с созданием сокета

    //слушаем, 10 клиентов
    if (listen(listener, MAX_CLIENTS) == -1)
    {
        errorLog("Error: listen, errno ", log_file);
        return 3;
    }
    logging("The audition went well", log_file);
    std::cout << std::endl;

    //пакет для клиента подключающего первый раз
    struct IpPortData
    {
        char IPv6[INET6_ADDRSTRLEN]; // INET6_ADDRSTRLEN = 46, хранит адресс
        unsigned short port;         //порт
        // char data[PACKAGE_SIZE - 48]; //доп данные
        //буфер 256, short 2, IPv6 46
        //  256 - 46 - 2 = 208
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
    char buf[PACKAGE_SIZE] = {};  // буфер для данных клиента
    sockaddr_storage client_addr; // адрес клиента, sockaddr_storage - кроссплатформа для IPv4 и IPv6

    int incoming_bytes; //пришедшие байты

    struct clientData
    {
        int fd_server;
        system_clock::time_point start;
    };
    //ассоциативный массивы c сортировкой ключей
    std::map<int, clientData> fdClientServer; //связка клиент сервер
    struct serverData
    {
        int fd_client;
        system_clock::time_point start;
    };
    std::map<int, serverData> fdServerClient; //связка сервер клиент

    struct timeval tv = {}; //бесполезный таймер

    // главный цикл
    while (true)
    {
        read_fds = master; // копируем

        //функция мониторит массивы файловых дескрипторов
        if (select(fd_max + 1, &read_fds, NULL, NULL, &tv) == -1)
        {
            errorLog("Error: select, errno ", log_file);
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
                        errorLog("Error: accept, errno ", log_file);
                    }
                    else
                    {
                        FD_SET(new_fd, &master); //добавляем в главный массив
                        if (new_fd > fd_max)     //меняем макс файловый дескриптор
                        {
                            fd_max = new_fd;
                        }
                        fdClientServer[new_fd].fd_server = 0;               //клиент без сервера
                        fdClientServer[new_fd].start = system_clock::now(); //время создание

                        logging("New clientIPv4 #id", new_fd, log_file);
                        printIP((struct sockaddr *)&client_addr, log_file); //выводим адрес клиента
                    }
                }
                else
                {
                    // получение данных от клиента
                    if ((incoming_bytes = recv(i, (char *)&buf, sizeof buf, 0)) <= 0)
                    { //что не так
                        //соединение закрыто клиентом
                        if (incoming_bytes == 0)
                        {
                            // соединение закрыто
                            logging("Communication is interrupted with id#", i, log_file);
                            std::cout << std::endl;
                        }
                        else // это уже ошибка
                        {
                            errorLog("Error: recv, errno ", log_file);
                        }
                        FD_CLR(i, &master);   //удаляем из главного массива
                        FD_CLR(i, &read_fds); //удаляем из массива для чтения
                        close(i);             //закрываем соединие

                        //если это сервер
                        if ((fdServerClient.find(i) != fdServerClient.end()))
                        {
                            FD_CLR(fdServerClient[i].fd_client, &master);      //удаляем клиента из главного массива
                            FD_CLR(fdServerClient[i].fd_client, &read_fds);    //удаляем клиенты из массива для чтения
                            close(fdServerClient[i].fd_client);                //закрываем сокеты клиента
                            fdClientServer.erase(fdServerClient[i].fd_client); //удаляем клиента из ассоциативного массива
                            fdServerClient.erase(i);                           //удаляем клиента из ассоциативного массива

                            logging("Communication is interrupted clientIPv4 with id#", fdServerClient[i].fd_client, log_file);
                            std::cout << std::endl;
                        }
                        else //если это клиент
                        {
                            if (fdClientServer[i].fd_server) //если есть у клиента сервер
                            {
                                FD_CLR(fdClientServer[i].fd_server, &master);      //удаляем сервер из главного массива
                                FD_CLR(fdClientServer[i].fd_server, &read_fds);    //удаляем сервер из массива для чтения
                                close(fdClientServer[i].fd_server);                //закрываем сокеты сервера
                                fdServerClient.erase(fdClientServer[i].fd_server); //удаляем сервер из ассоциативного массива

                                logging("Communication is interrupted with serverIPv6 id#", fdClientServer[i].fd_server, log_file);
                                std::cout << std::endl;
                            }
                            fdClientServer.erase(i); //удаляем клиент из ассоциативного массива
                        }
                    }
                    else //успешно получены данные, тогда обрабатываем
                    {
                        //проверяем это клиент
                        if (fdClientServer.find(i) != fdClientServer.end())
                        {
                            if (fdClientServer[i].fd_server == 0) //если у клиента нет сервера
                            {
                                std::string string_buf(buf);
                                int indexSpace = string_buf.find_first_of(' ', 0); //ищем разделитель

                                char adressIPv6[INET6_ADDRSTRLEN] = {0};    //адрес сервера
                                string_buf.copy(adressIPv6, indexSpace, 0); //копируем из буфера

                                char portIPv6[INET6_ADDRSTRLEN] = {0};                                 //порт сервера
                                string_buf.copy(portIPv6, string_buf.size() - indexSpace, indexSpace); //копируем
                                int portServera = std::stoi(portIPv6);

                                logging("ClientIPv4 message:", log_file);

                                //в консоль

                                std::cout << "IP - " << adressIPv6 << std::endl;
                                std::cout << "Port - " << portServera << std::endl;

                                //в файл

                                log_file << toHex.stringToHEx("IP - " + std::string(adressIPv6)) << std::endl;
                                log_file << toHex.stringToHEx("Port - " + toHex.intToHEX(portServera)) << std::endl;

                                //создаем данные для подключаемся клиенту
                                sockaddr_in6 servaddr6 = {};                             //зануление
                                servaddr6.sin6_family = AF_INET6;                        // AF_INET6 - семейство IPv6
                                inet_pton(AF_INET6, adressIPv6, &(servaddr6.sin6_addr)); //Заносим IPv6 адрес сервера
                                servaddr6.sin6_port = htons(portServera);                //порт

                                int new_fd = socket(AF_INET6, SOCK_STREAM, 0); //создаем сокет
                                memset(buf, 0, PACKAGE_SIZE);                  //зануляем буфер
                                //подключаемся к серверу
                                if (-1 == connect(new_fd, (struct sockaddr *)&servaddr6, sizeof(servaddr6)))
                                {
                                    //не удачное подключение
                                    errorLog("Error: connecting to the serverIPv6 ", log_file);
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
                                    logging("Connecting to the serverIPv6 id#", new_fd, log_file);
                                    std::cout << std::endl;

                                    //отправляем серверу данные

                                    FD_SET(new_fd, &master); //добавляем в главный массив
                                    if (new_fd > fd_max)     //меняем макс файловый дискриптор
                                    {
                                        fd_max = new_fd;
                                    }
                                    fdServerClient[new_fd].fd_client = i;               //серверу привязываем клиента
                                    fdClientServer[i].fd_server = new_fd;               //клиенту привязываем сервер
                                    fdServerClient[new_fd].start = system_clock::now(); //время старта работы с сервером
                                    fdClientServer[i].start = system_clock::now();      //время реагирование клиента
                                }
                            }
                            else //если уже у клиента есть присоединенный сервер
                            {
                                logging("CliendIPv4 id#", i, " message:", log_file);
                                logging(buf, log_file);

                                //отправляем данные для клиента
                                if (-1 == send(fdClientServer[i].fd_server, (char *)&buf, sizeof(buf), 0))
                                {
                                    //удаление полное
                                    errorLog("Error: sending data, errno ", log_file);

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
                                    logging("Sending data to the serverIPv6 #id", fdClientServer[i].fd_server, log_file);
                                    std::cout << std::endl;
                                    fdClientServer[i].start = system_clock::now(); //время реагирование клиента
                                }
                                memset(buf, 0, PACKAGE_SIZE); //зануляем буфер
                            }
                        }
                        else //тут принимаем сообщения от сервера
                        {
                            logging("ServerIPv6 id#", i, " message:", log_file);
                            logging(buf, log_file);

                            if (-1 == send(fdServerClient[i].fd_client, (char *)&buf, sizeof(buf), 0))
                            {
                                //удаление полное
                                errorLog("Error: sending data, errno ", log_file);

                                FD_CLR(i, &master);                             //удаляем из главного массива
                                FD_CLR(i, &read_fds);                           //удаляем из массива для чтения
                                FD_CLR(fdServerClient[i].fd_client, &master);   //удаляем клиента из массива для чтения
                                FD_CLR(fdServerClient[i].fd_client, &read_fds); //удаляем клиента из главного массива

                                close(fdServerClient[i].fd_client); //клиент
                                close(i);                           //сервер
                                //убираем с ассоциативных массивов
                                fdClientServer.erase(fdServerClient[i].fd_client);
                                fdServerClient.erase(i);
                            }
                            else
                            {
                                fdServerClient[i].start = system_clock::now(); //время последнего реагирование сервера
                                logging("Sending data to the clientIPv4 #id", fdServerClient[i].fd_client, log_file);
                                std::cout << std::endl;
                            }
                            memset(buf, 0, PACKAGE_SIZE); //зануляем буфер
                        }
                    }
                }
            }

            else if ((i > 3) && FD_ISSET(i, &master))
            { //если это не слушатель, если есть в главном массиве файловых дескрипторов, если есть в ассоциативном массиве клиентов

                if (fdClientServer.find(i) != fdClientServer.end())
                {
                    system_clock::time_point now = system_clock::now();

                    if (duration_cast<milliseconds>(now - fdClientServer[i].start).count() > WAITING_TIME)
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

                        logging("Disconnecting for a long wait clientIPv4 #id", i, log_file);
                        std::cout << std::endl;
                    }
                }
                else if (fdServerClient.find(i) != fdServerClient.end())
                {
                    {
                        system_clock::time_point now = system_clock::now();

                        if (duration_cast<milliseconds>(now - fdServerClient[i].start).count() > WAITING_TIME)
                        { //если таймер истек, таймер на  30 секунд

                            FD_CLR(i, &master);   //удаляем из главного массива
                            FD_CLR(i, &read_fds); //удаляем из массива для чтения

                            //закрываем сокеты клиента
                            close(i);

                            FD_CLR(fdServerClient[i].fd_client, &master);      //удаляем сервер из главного массива
                            FD_CLR(fdServerClient[i].fd_client, &read_fds);    //удаляем сервер из массива для чтения
                            close(fdServerClient[i].fd_client);                //закрываем сокеты сервера
                            fdClientServer.erase(fdServerClient[i].fd_client); //удаляем сервер из ассоциативного массива

                            fdServerClient.erase(i); //удаляем клиент из ассоциативного массива

                            logging("Disconnecting for a long wait serverIPv6 #id", i, log_file);
                            std::cout << std::endl;
                        }
                    }
                }
            }
        }
    }
    return 0;
}
