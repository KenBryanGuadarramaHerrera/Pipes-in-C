#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>

#define BUFFER_SIZE 1024

int fd[2];  // File descriptor para el pipe

void remove_directory(const char *path) {
    DIR *dir = opendir(path);
    struct dirent *entry;
    char full_path[BUFFER_SIZE];

    if (dir) {
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                sprintf(full_path, "%s/%s", path, entry->d_name);
                if (entry->d_type == DT_DIR) {
                    remove_directory(full_path);
                } else {
                    printf("removed '%s'\n", full_path);
                    unlink(full_path);
                }
            }
        }
        closedir(dir);
        printf("removed directory '%s'\n", path);
        rmdir(path);
    }
}

void create_backup_directory(const char *backup_directory) {
    mkdir(backup_directory, 0777); // No error check for simplicity
}

void backup_files(const char *source_dir, const char *backup_dir, int num_files) {
    DIR *dir;
    struct dirent *entry;
    char src_path[BUFFER_SIZE], dest_path[BUFFER_SIZE];
    int remaining_files = num_files;

    printf("hijo(pid=%d) esperando mensaje de mi padre...\n", getpid());

    close(fd[1]);  // Cierre del lado de escritura del pipe en hijo

    // Esperar mensaje inicial del padre
    char buffer[BUFFER_SIZE];
    if (read(fd[0], buffer, BUFFER_SIZE) > 0) {
        printf("hijo(pid=%d), Instruccion de mi padre: %s\n", getpid(), buffer);
    }

    dir = opendir(source_dir);
    if (dir == NULL) {
        perror("Failed to open source directory");
        exit(EXIT_FAILURE);
    }

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            snprintf(src_path, BUFFER_SIZE, "%s/%s", source_dir, entry->d_name);
            snprintf(dest_path, BUFFER_SIZE, "%s/%s", backup_dir, entry->d_name);

            FILE *src = fopen(src_path, "rb");
            FILE *dest = fopen(dest_path, "wb");

            if (src && dest) {
                char ch;
                while (fread(&ch, sizeof(char), 1, src) == 1) {
                    fwrite(&ch, sizeof(char), 1, dest);
                }
                fclose(src);
                fclose(dest);
                remaining_files--;
                printf("(Padre --> %s)\n", entry->d_name);
                printf("Hijo(pid=%d), respaldando el archivo: %s pendientes %d/%d\n", getpid(), entry->d_name, remaining_files, num_files);
            } else {
                if (src) fclose(src);
                if (dest) fclose(dest);
                perror("Failed to open files for backup");
            }
        }
    }
    closedir(dir);

    // Reportar al padre que la tarea ha concluido
    char final_msg[] = "Adios padre, termine el respaldo...";
    write(fd[0], final_msg, strlen(final_msg) + 1);  // Enviar mensaje de finalización al padre

    close(fd[0]);  // Cierre del lado de lectura del pipe
}

int count_files(const char *source_dir) {
    int count = 0;
    DIR *dir = opendir(source_dir);
    struct dirent *entry;

    if (dir) {
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_type == DT_REG) {
                count++;
            }
        }
        closedir(dir);
    }
    return count;
}

int main() {
    char source_dir[BUFFER_SIZE], backup_dir[BUFFER_SIZE];
    printf("---------------------PROCESOS BACKUP-------------------\n");
    printf("Comunicacion y sincronizacion de procesos → Respaldo de archivos\n\n");
    printf("Ingrese el directorio fuente: ");
    scanf("%s", source_dir);
    printf("Ingrese el directorio destino (respaldo): ");
    scanf("%s", backup_dir);

    printf("\n\nPadre(pid=%d): borrando respaldo viejo...\n", getpid());
    remove_directory(backup_dir);
    printf("Padre(pid=%d): creando directorio de respaldo...\n", getpid());
    create_backup_directory(backup_dir);

    int num_files = count_files(source_dir);
    printf("======= RESPALDANDO %d ARCHIVOS ======\n", num_files);

    if (pipe(fd) < 0) {
        perror("Error al crear el pipe");
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("Fallo en fork");
        exit(EXIT_FAILURE);
    }

    if (pid > 0) {  // Código del padre
        close(fd[0]);  // Cerrar lado de lectura en el padre

        // Enviar mensaje inicial al hijo
        char msg[] = "Hola hijo, realiza el respaldo de archivos";
        write(fd[1], msg, strlen(msg) + 1);
        close(fd[1]);  // Cerrar lado de escritura después de enviar

        // Esperar que el hijo termine
        int status;
        waitpid(pid, &status, 0);

        // Leer mensaje final del hijo
        char final_msg[BUFFER_SIZE];
        read(fd[0], final_msg, BUFFER_SIZE);
        printf("Padre(pid=%d), mensaje del hijo: %s\n", getpid(), final_msg);

        if (WIFEXITED(status)) {
            printf("Padre(pid=%d): Termina el proceso hijo con estado %d\n", getpid(), WEXITSTATUS(status));
        }

        printf("Padre(pid=%d) comprobando respaldo:\n", getpid());
        system("ls -l respaldo");
        printf("%d\nARCHIVOS RESPALDADOS\n", num_files);

        printf("====================\nTermino el proceso padre...\n");
    } else {  // Código del hijo
        backup_files(source_dir, backup_dir, num_files);
        exit(EXIT_SUCCESS);
    }

    return 0;
}