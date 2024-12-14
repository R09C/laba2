# Используем базовый образ с поддержкой C
FROM gcc:latest

# Копируем исходный код в контейнер
COPY main.c /usr/src/myapp.c

# Компилируем приложение
RUN gcc -o /usr/local/bin/myapp /usr/src/myapp.c

# Открываем порт 8080
EXPOSE 8080

# Определяем команду по умолчанию
CMD ["myapp"]