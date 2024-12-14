#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <errno.h>
#include <string.h>
#include <syslog.h> // Для логирования
volatile sig_atomic_t wasSigHup = 0; // Флаг, который указывает на получение сигнала SIGHUP

// Обработчик сигнала SIGHUP
void sigHupHandler(int signum)
{
    wasSigHup = 1; // Устанавливаем флаг при получении сигнала
}

// Функция для логирования сообщений
void log_message(const char *msg)
{
    // Выводим сообщение на терминал
    printf("%s\n", msg);
    // Также логируем через syslog
    syslog(LOG_INFO, "%s", msg);
}

int main()
{
    int server_fd, new_fd;
    struct sockaddr_in server_addr;    // Структура для хранения информации об адресе сервера
    fd_set master_fds, read_fds;       // Множества для работы с pselect
    int max_fd;                        // Максимальный файловый дескриптор
    struct sigaction sa;               // Структура для установки обработчика сигнала
    sigset_t origSigMask, blockedMask; // Маски сигналов для блокировки SIGHUP

    // Инициализация syslog
    openlog("webserver", LOG_PID | LOG_CONS, LOG_USER);

    // Инициализация структуры sigaction
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigHupHandler; // Устанавливаем функцию-обработчик
    sigemptyset(&sa.sa_mask);      // Не блокируем дополнительные сигналы в обработчике
    sa.sa_flags = SA_RESTART;      // Обработчик не будет прерывать блокирующие вызовы

    // Регистрация обработчика для сигнала SIGHUP
    if (sigaction(SIGHUP, &sa, NULL) == -1)
    {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    // Блокируем сигнал SIGHUP, чтобы обработать его вручную
    sigemptyset(&blockedMask);
    sigaddset(&blockedMask, SIGHUP);
    if (sigprocmask(SIG_BLOCK, &blockedMask, &origSigMask) == -1)
    {
        perror("sigprocmask");
        exit(EXIT_FAILURE);
    }

    // Создание серверного сокета
    server_fd = socket(AF_INET, SOCK_STREAM, 0); // Создаем TCP сокет
    if (server_fd == -1)
    {
        log_message("Error creating socket");
        exit(EXIT_FAILURE);
    }

    // Установка опции сокета для повторного использования адреса
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
    {
        log_message("Error setting socket options");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Настройка адреса сервера
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);       // Порт 8080
    server_addr.sin_addr.s_addr = INADDR_ANY; // Принимаем подключения на всех интерфейсах

    // Привязка сокета к адресу
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        log_message("Error binding socket");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Ожидание входящих подключений (максимум 10 в очереди)
    if (listen(server_fd, 10) == -1)
    {
        log_message("Error listening on socket");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port 8080...\n");
    log_message("Server started and listening on port 8080");

    FD_ZERO(&master_fds);           // Очищаем множество файловых дескрипторов
    FD_SET(server_fd, &master_fds); // Добавляем серверный сокет в список наблюдаемых
    max_fd = server_fd;             // Изначально максимальный дескриптор - это серверный сокет

    // Основной цикл работы сервера
    while (1)
    {
        read_fds = master_fds; // Копируем множество для pselect

        // Вызов pselect с блокировкой SIGHUP
        int ret = pselect(max_fd + 1, &read_fds, NULL, NULL, NULL, &origSigMask); // Используем pselect для мультиплексирования

        if (ret == -1)
        {
            if (errno == EINTR)
            {
                // Если pselect был прерван сигналом, проверяем именно SIGHUP
                if (wasSigHup)
                {
                    log_message("SIGHUP received, handling...");
                    printf("Received SIGHUP signal. Reloading configuration...\n");
                    // Здесь можно выполнить дополнительную логику обработки перезагрузки/конфигурации
                    wasSigHup = 0; // Сбрасываем флаг
                }
                continue; // Если pselect был прерван, продолжаем
            }

            log_message("Error in pselect");
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
                        log_message("Error accepting connection");
                    }
                    else
                    {
                        FD_SET(new_fd, &master_fds); // Добавляем новый сокет в список наблюдаемых
                        if (new_fd > max_fd)
                            max_fd = new_fd; // Обновляем максимальный дескриптор
                        printf("Accepted new connection on socket %d\n", new_fd);
                        log_message("Accepted new connection");
                    }
                }
                else
                {
                    // Чтение данных от клиента
                    char buf[1025]; // Увеличиваем размер на 1 для завершающего нуля
                    int nbytes = recv(fd, buf, 1024, 0);
                    if (nbytes <= 0)
                    {
                        if (nbytes == 0)
                        {
                            printf("Socket %d disconnected\n", fd);
                            log_message("Client disconnected");
                        }
                        else
                        {
                            printf("Error receiving data on socket %d\n", fd);
                            log_message("Error receiving data");
                        }
                        close(fd);               // Закрытие сокета клиента
                        FD_CLR(fd, &master_fds); // Убираем сокет из списка наблюдаемых
                    }
                    else
                    {
                        buf[nbytes] = '\0'; // Завершаем строку нулевым символом
                        printf("Received data from socket %d: %s\n", fd, buf);
                        log_message("Received data from client");

                        // Отправка ответа клиенту
                        const char *response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nHello, world!";
                        if (send(fd, response, strlen(response), 0) == -1)
                        {
                            printf("Error sending data to socket %d\n", fd);
                            log_message("Error sending data");
                            close(fd);
                            FD_CLR(fd, &master_fds); // Убираем сокет из списка
                        }
                        else
                        {
                            printf("Sent response to socket %d\n", fd);
                            log_message("Sent response to client");
                        }
                    }
                }
            }
        }
    }

    // Завершение работы
    close(server_fd); // Закрытие серверного сокета
    closelog();       // Закрываем логирование
    printf("Server shutting down.\n");
    log_message("Server shutting down");
    return 0;
}
