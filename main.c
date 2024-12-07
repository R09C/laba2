"""
Сигнал SIGHUP
Когда программа запускается, создаётся специальный обработчик сигнала SIGHUP. Он следит за системными изменениями, например, перезагрузкой или изменением конфигурации.

Чтобы всё работало, нужно зарегистрировать обработчик с помощью структуры sigaction. Сигнал блокируется на время обработки.

Создание серверного сокета
Создаём серверный сокет с помощью вызова socket(). Он настроен на использование протокола IPv4 и TCP.
Затем привязываем сокет к определённому адресу и порту с помощью вызова bind(). Порт 8080 — это стандартный HTTP-порт, который мы используем для нашего простого веб-сервера.
С помощью функции listen() сервер начинает прослушивать входящие соединения.

Основной цикл сервера
В основном цикле работы сервера используем функцию select(), чтобы сервер мог обрабатывать несколько соединений одновременно.
Когда сервер получает новое подключение, оно принимается с помощью функции accept(). Новый сокет добавляется в список наблюдаемых файловых дескрипторов.
Когда данные приходят от клиента, они считываются с помощью функции recv(). Если данные получены, сервер отправляет HTTP-ответ с сообщением «Hello, world!». Если соединение закрывается, сокет удаляется из списка и закрывается.

Сигнал SIGHUP
Если получен сигнал SIGHUP, сервер обрабатывает его (устанавливается флаг wasSigHup), выводит сообщение и сбрасывает флаг.
EOF
"""

// docker run -p 8080:8080 myapp  

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <errno.h>
#include <string.h>

volatile sig_atomic_t wasSigHup = 0; // Флаг, который указывает на получение сигнала SIGHUP

// Обработчик сигнала SIGHUP
void sigHupHandler(int signum)
{
    wasSigHup = 1; // Устанавливаем флаг при получении сигнала
    printf("Received SIGHUP signal\n");
}

int main()
{
    int server_fd, new_fd;
    struct sockaddr_in server_addr;    // Структура для хранения информации об адресе сервера
    fd_set master_fds, read_fds;       // Множества для работы с select
    int max_fd;                        // Максимальный файловый дескриптор
    struct sigaction sa;               // Структура для установки обработчика сигнала
    sigset_t origSigMask, blockedMask; // Маски сигналов для блокировки SIGHUP

    // Регистрация обработчика сигнала SIGHUP
    sa.sa_handler = sigHupHandler; // Устанавливаем функцию-обработчик
    sa.sa_flags |= SA_RESTART;     // Обработчик не будет прерывать блокирующие вызовы
    sigaction(SIGHUP, &sa, NULL);  // Регистрируем обработчик для сигнала SIGHUP

    // Блокируем сигнал SIGHUP, чтобы обработать его вручную
    sigemptyset(&blockedMask);
    sigaddset(&blockedMask, SIGHUP);
    sigprocmask(SIG_BLOCK, &blockedMask, &origSigMask); // Блокируем SIGHUP

    // Создание серверного сокета
    server_fd = socket(AF_INET, SOCK_STREAM, 0); // Создаем TCP сокет
    if (server_fd == -1)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Настройка адреса сервера
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);       // Порт 8080
    server_addr.sin_addr.s_addr = INADDR_ANY; // Принимаем подключения на всех интерфейсах

    // Привязка сокета к адресу
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("bind");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Ожидание входящих подключений (максимум 10 в очереди)
    if (listen(server_fd, 10) == -1)
    {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    FD_ZERO(&master_fds);           // Очищаем множество файловых дескрипторов
    FD_SET(server_fd, &master_fds); // Добавляем серверный сокет в список наблюдаемых
    max_fd = server_fd;             // Изначально максимальный дескриптор - это серверный сокет

    // Основной цикл работы сервера
    while (1)
    {
        read_fds = master_fds;                                     // Копируем множество для select
        int ret = select(max_fd + 1, &read_fds, NULL, NULL, NULL); // Используем select для мультиплексирования

        if (ret == -1)
        {
            perror("select"); // Ошибка в select
            break;
        }

        // Перебор всех файловых дескрипторов
        for (int fd = 0; fd <= max_fd; fd++)
        {
            if (FD_ISSET(fd, &read_fds)) // Если дескриптор готов для чтения
            {
                if (fd == server_fd)
                {
                    // Прием нового соединения
                    new_fd = accept(server_fd, NULL, NULL);
                    if (new_fd == -1)
                    {
                        perror("accept");
                    }
                    else
                    {
                        FD_SET(new_fd, &master_fds); // Добавляем новый сокет в список наблюдаемых
                        if (new_fd > max_fd)
                            max_fd = new_fd; // Обновляем максимальный дескриптор
                        printf("New connection accepted\n");
                    }
                }
                else
                {
                    // Чтение данных от клиента
                    char buf[1024];
                    int nbytes = recv(fd, buf, sizeof(buf), 0);
                    if (nbytes <= 0)
                    {
                        if (nbytes == 0)
                        {
                            printf("Client disconnected\n");
                        }
                        else
                        {
                            perror("recv");
                        }
                        close(fd);               // Закрытие сокета клиента
                        FD_CLR(fd, &master_fds); // Убираем сокет из списка наблюдаемых
                    }
                    else
                    {
                        buf[nbytes] = '\0'; // Завершаем строку нулевым символом
                        printf("Received data: %s\n", buf);

                        // Отправка ответа клиенту
                        const char *response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nHello, world!";
                        if (send(fd, response, strlen(response), 0) == -1)
                        {
                            perror("send");
                            close(fd);
                            FD_CLR(fd, &master_fds); // Убираем сокет из списка
                        }
                    }
                }
            }
        }

        // Обработка сигнала SIGHUP, если он был получен
        if (wasSigHup)
        {
            printf("SIGHUP signal handled, resuming operation\n");
            wasSigHup = 0; // Сбрасываем флаг после обработки
        }
    }

    close(server_fd); // Закрытие серверного сокета
    return 0;
}
