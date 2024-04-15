#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <semaphore.h>
#include <string.h>

#define BUFFER_SIZE 1024 // Размер буфера для чтения и записи данных

int bytes_to_read; // Количество байт для чтения из файла
sem_t *semaphore; // Семафор для синхронизации доступа к файлу

void sig_child_handler(int signum) {
    bytes_to_read = BUFFER_SIZE;
}

// Функция для процессов-потомков
void childProcess(int index, int N, int M, FILE *fp) {
    int bytes_written;
    for (int i = 0; i < M; i++) {
        char buffer[BUFFER_SIZE];
        memset(buffer, 0, BUFFER_SIZE); // Инициализация буфера нулями
        sem_wait(semaphore); // Ожидание семафора
        fprintf(stdout, "Child %d writes data %d\n", index, i);
        // Определение данных для записи на основе индекса потомка
        buffer[0] = (index % 2) + '0'; // '0' для потомка 1, '1' для потомка 2
        bytes_written = fwrite(buffer, sizeof(char), N, fp); // Запись данных в файл
        if (bytes_written < N) { // Проверка успешности записи
            perror("write");
            exit(EXIT_FAILURE);
        }
        sem_post(semaphore); // Освобождение семафора
        kill(getppid(), SIGUSR1); // Отправка сигнала родителю
    }
    fclose(fp); // Закрытие файла в процессе-потомке
    exit(EXIT_SUCCESS); // Выход из процесса-потомка
}

// Функция для процесса-родителя
void parentProcess(int N, int M, const char *output_file) {
    int status;
    pid_t child_pid1, child_pid2;
    FILE *fp = fopen(output_file, "w"); // Открытие файла для записи
    if (fp == NULL) { // Проверка успешности открытия файла
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    semaphore = sem_open("/file_semaphore", O_CREAT, 0644, 1); // Создание семафора
    if (semaphore == SEM_FAILED) { // Проверка успешности создания семафора
        perror("sem_open");
        exit(EXIT_FAILURE);
    }

    signal(SIGUSR1, sig_child_handler); // Установка обработчика сигнала

    child_pid1 = fork(); // Создание первого потомка
    if (child_pid1 == -1) { // Проверка успешности создания первого потомка
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (child_pid1 == 0) {
        childProcess(1, N, M, fp); // Вызов функции для процесса-потомка 1
    }

    child_pid2 = fork(); // Создание второго потомка
    if (child_pid2 == -1) { // Проверка успешности создания второго потомка
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (child_pid2 == 0) {
        childProcess(2, N, M, fp); // Вызов функции для процесса-потомка 2
    }

    // Ожидание завершения работы обоих потомков
    waitpid(child_pid1, &status, 0);
    waitpid(child_pid2, &status, 0);

    fclose(fp); // Закрытие файла после завершения работы потомков

    printf("File content:\n");
    FILE *read = fopen(output_file, "r"); // Открытие файла для чтения
    if (read == NULL) { // Проверка успешности открытия файла для чтения
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    while ((bytes_read = fread(buffer, sizeof(char), BUFFER_SIZE, read)) > 0) {
        fwrite(buffer, sizeof(char), bytes_read, stdout); // Вывод данных файла на экран
    }

    if (ferror(read)) { // Проверка ошибок чтения файла
        perror("fread");
        fclose(read);
        exit(EXIT_FAILURE);
    }

    printf("\n");

    fclose(read); // Закрытие файла после чтения

    sem_close(semaphore); // Закрытие семафора
    sem_unlink("/file_semaphore"); // Удаление семафора
}

int main(int argc, char *argv[]) {
    if (argc != 4) { // Проверка правильности переданных аргументов
        fprintf(stderr, "Usage: %s <N> <M> <output_file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int N = atoi(argv[1]); // Чтение значения N
    int M = atoi(argv[2]); // Чтение значения M
    const char *output_file = argv[3]; // Чтение имени выходного файла
    bytes_to_read = N; // Инициализация переменной для чтения из файла

    parentProcess(N, M, output_file); // Вызов функции для процесса-родителя

    return EXIT_SUCCESS;
}
