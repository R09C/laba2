import threading
import time

# docker build -t python-monitor .
# docker run --name monitor-container python-monitor
# docker rm monitor-container


# Разделяемые данные и мьютекс
lock = threading.Lock()
condition = threading.Condition(lock)  # для синхронизации потоков потоков
ready = False  # флаг готовности данных для чтения


# Мьютекс гарантирует что доступ к данным будет только у одного потока.
# Lock(мьютекс)
# Когда поток хочет получить доступ к данным, он "захватывает" (acquires) Lock.
# Пока Lock захвачен, остальные потоки не могут получить доступ к этим данным.
# Когда поток завершает работу с данными, он "освобождает" (releases) Lock.


# Condition — это более сложный механизм синхронизации, который работает на основе Lock. Он позволяет:

# Потокам "ждать" определённых условий.
# Уведомлять другие потоки, когда условия выполнены.


# Функция-поставщик
def provider():
    global ready
    while True:
        with condition:
            if not ready:
                ready = True
                print("Поставщик: событие предоставлено.")
                condition.notify_all()  # Уведомляем потребителя
            time.sleep(2)


# Функция-потребитель
def consumer():
    global ready
    while True:
        with condition:
            while not ready:  # Ждем, пока поставщик уведомит
                print("Потребитель: жду события...")
                condition.wait()
            ready = False
            print("Потребитель: событие обработано.")
        time.sleep(1)


# Запуск потоков
thread_provider = threading.Thread(target=provider, daemon=True)
thread_consumer = threading.Thread(target=consumer, daemon=True)

thread_provider.start()
thread_consumer.start()

try:
    while True:
        time.sleep(1)  # Основной поток ждет завершения работы потоков
except KeyboardInterrupt:
    print("\nЗавершение работы...")
