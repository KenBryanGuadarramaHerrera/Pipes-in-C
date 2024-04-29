#include <stdio.h>    
#include <stdlib.h>    
#include <unistd.h>   
#include <sys/types.h>        
#include <sys/wait.h>    
#include <dirent.h>   
#include <string.h>    
#include <sys/stat.h>    
#include <fcntl.h>           

#define BUFFER_SIZE 1024        

int fd[2];                      

void eliminar_directorio(const char *ruta) {
    DIR *dir = opendir(ruta);
    struct dirent *entrada;  
    char ruta_completa[BUFFER_SIZE];   
    if (dir) {     
        while ((entrada = readdir(dir)) != NULL) {  
            if (strcmp(entrada->d_name, ".") != 0 && strcmp(entrada->d_name, "..") != 0) { 
                sprintf(ruta_completa, "%s/%s", ruta, entrada->d_name);    
                if (entrada->d_type == DT_DIR) {    
                    eliminar_directorio(ruta_completa);  
                } else {
                    unlink(ruta_completa);  
                }
            }
        }
        closedir(dir);  
        printf("Directorio '%s' eliminado\n\n", ruta);   
        rmdir(ruta);   
    }
}

void crear_directorio_respaldo(const char *directorio_respaldo) {
    struct stat st = {0};   
    if (stat(directorio_respaldo, &st) == -1) { 
        char comando[512];  
        sprintf(comando, "mkdir -p %s", directorio_respaldo); 
        system(comando);    
    }
}

void respaldar_archivos(const char *directorio_fuente, const char *directorio_respaldo, int num_archivos) {
    DIR *dir;     
    struct dirent *entrada;         
    char ruta_fuente[BUFFER_SIZE], ruta_destino[BUFFER_SIZE]; 
    int archivos_restantes = num_archivos;  

    printf("Hijo(pid=%d) esperando mensaje de mi padre...\n", getpid());   
    close(fd[1]);   

    char buffer[BUFFER_SIZE];
    if (read(fd[0], buffer, BUFFER_SIZE) > 0) { 
        printf("Hijo(pid=%d), Instrucción de mi padre: %s\n", getpid(), buffer);   
    }

    dir = opendir(directorio_fuente);  
    if (dir == NULL) {  
        perror("Error al abrir el directorio fuente");    
        exit(EXIT_FAILURE); 
    }

    while ((entrada = readdir(dir)) != NULL) {    
        if (entrada->d_type == DT_REG) {    
            snprintf(ruta_fuente, BUFFER_SIZE, "%s/%s", directorio_fuente, entrada->d_name); 
            snprintf(ruta_destino, BUFFER_SIZE, "%s/%s", directorio_respaldo, entrada->d_name); 

            FILE *fuente = fopen(ruta_fuente, "rb"); 
            FILE *destino = fopen(ruta_destino, "wb");   

            if (fuente && destino) {    
                char ch;
                while (fread(&ch, sizeof(char), 1, fuente) == 1) { 
                    fwrite(&ch, sizeof(char), 1, destino);
                }
                fclose(fuente); 
                fclose(destino);
                archivos_restantes--;   
                printf("(Padre --> %s)\n", entrada->d_name);  
                printf("Hijo(pid=%d), respaldando el archivo: %s, pendientes %d/%d\n", getpid(), entrada->d_name, archivos_restantes, num_archivos); 
            } else {
                if (fuente) fclose(fuente); 
                if (destino) fclose(destino);   
                perror("Error al abrir archivos para respaldo");   
            }
        }
    }
    closedir(dir);  

    char mensaje_final[] = "Adiós padre, terminé el respaldo...";
    write(fd[0], mensaje_final, strlen(mensaje_final) + 1);  
    close(fd[0]);
}

int contar_archivos(const char *directorio_fuente) {
    int conteo = 0; 
    DIR *dir = opendir(directorio_fuente);  
    struct dirent *entrada; 
    if (dir) {  
        while ((entrada = readdir(dir)) != NULL) {  
            if (entrada->d_type == DT_REG) {   
                conteo++;   
            }
        }
        closedir(dir);  
    }
    return conteo;  
}

int main() {
    char directorio_fuente[BUFFER_SIZE], directorio_respaldo[BUFFER_SIZE];   
    printf("---------------------PROCESOS RESPALDO-------------------\n");
    printf("Ingrese el directorio fuente: ");
    scanf("%s", directorio_fuente);    
    printf("Ingrese el directorio destino (respaldo): ");
    scanf("%s", directorio_respaldo);  

    printf("\n\nPadre(pid=%d): borrando respaldo antiguo...\n", getpid());  
    eliminar_directorio(directorio_respaldo);   
    printf("Padre(pid=%d): creando directorio de respaldo...\n", getpid());
    crear_directorio_respaldo(directorio_respaldo); 
    int num_archivos = contar_archivos(directorio_fuente); 
    printf("Padre(pid=%d): Hola hijo, realiza el respaldo de %d archivos \n", getpid(), num_archivos);   
    printf("======= RESPALDANDO %d ARCHIVOS ======\n", num_archivos);  

    int pipe_fd[2]; 
    if (pipe(pipe_fd) == -1) { 
        perror("Error al crear el pipe");  
        exit(EXIT_FAILURE); 
    }
    pid_t pid = fork(); 
    if (pid < 0) {  
        perror("Fallo en fork");   
        exit(EXIT_FAILURE); 
    }

    if (pid > 0) {
        close(pipe_fd[1]);  
        char msg_del_hijo[BUFFER_SIZE];   
        ssize_t bytes_leidos = read(pipe_fd[0], msg_del_hijo, BUFFER_SIZE); 
        if (bytes_leidos < 0) { 
            perror("Error al leer del pipe");   
            exit(EXIT_FAILURE); 
        }
        printf("Padre(pid=%d), mensaje del hijo: %s\n", getpid(), msg_del_hijo);   
        wait(NULL); 
        
        printf("Padre(pid=%d) comprobando respaldo:\n", getpid());  
        printf("===================================================\n");
        char comando[2048]; 
        sprintf(comando, "ls -l %s", directorio_respaldo);    
        system(comando);    
        printf("%d\nARCHIVOS RESPALDADOS\n", num_archivos); 
        printf("===================================================\nProceso padre terminado...\n"); 
    } else {    
        close(pipe_fd[0]);  

        respaldar_archivos(directorio_fuente, directorio_respaldo, num_archivos);

        char mensaje_final[] = "Adiós padre, terminé el respaldo...";
        ssize_t bytes_escritos = write(pipe_fd[1], mensaje_final, strlen(mensaje_final) + 1); 
        if (bytes_escritos < 0) { 
            perror("Error al escribir en el pipe"); 
            exit(EXIT_FAILURE); 
        }
        close(pipe_fd[1]);  

        exit(EXIT_SUCCESS); 
    }

    char *arch = directorio_respaldo;  
    char comando[1024]; 
    sprintf(comando, "ls %s > archivosRespaldados.txt", arch); 
    system(comando);    

    sprintf(comando, "echo 'Total de archivos: $(ls %s | wc -l)' >> archivosRespaldados.txt", arch); 
    char *bash = malloc(strlen(comando) + 20); 
    if (bash == NULL) { 
        fprintf(stderr, "Error al alocar memoria.\n"); 
        return 1;  
    }
    sprintf(bash, "bash -c \"%s\"", comando);   
    system(bash);   
    free(bash); 
    return 0;   
}
