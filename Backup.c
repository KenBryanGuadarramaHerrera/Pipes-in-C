// biblioteca estándar de entrada/salida
#include <stdio.h>    
// biblioteca estándar utilizada para asignacion de memoria dinamica (malloc) y para llamadas al sistema (system), (EXIT_FAILURE,EXIT_SUCESS)         
#include <stdlib.h>    
// biblioteca estándar utilizada para acceder a funciones de sistema operativo (read,write,open, close, fork, wait,mkdir,rmdir,getpid)         
#include <unistd.h>   
// Incluye definiciones de tipos de datos del sistema (pid_t,size_t)         
#include <sys/types.h>        
// Incluye funciones y macros para trabajar con procesos hijos  
#include <sys/wait.h>    
// Incluye definiciones para trabajar con directorios (opendir,readdir,closedir)     
#include <dirent.h>   
// Incluye funciones para manipular cadenas de caracteres (strcpy,strlen,strcmp)        
#include <string.h>    
// Incluye definiciones para manipular información de archivos (stat)      
#include <sys/stat.h>    
// Incluye definiciones para el control de archivos (open,read,write)      
#include <fcntl.h>           
   
// Definimo el tamaño del búfer utilizado 
#define BUFFER_SIZE 1024        

// Descriptor de archivo para el pipe
int fd[2];                      

// Funcion que elimina un directorio y su contenido de forma recursiva
void eliminar_directorio(const char *ruta) {
    // Abre el directorio especificado por 'ruta' y devuelve el puntero DIR
    DIR *dir = opendir(ruta);
    // Estructura que representa una entrada en un directorio   
    struct dirent *entrada;  
    // Almacenamos la ruta completa de un archivo   
    char ruta_completa[BUFFER_SIZE];   
    // Si el directorio se abrió correctamente
    if (dir) {     
        // Leemos cada entrada del directorio             
        while ((entrada = readdir(dir)) != NULL) {  
            // Ignora las entradas '.' y '..'
            if (strcmp(entrada->d_name, ".") != 0 && strcmp(entrada->d_name, "..") != 0) { 
                // Construye la ruta completa del archivo
                sprintf(ruta_completa, "%s/%s", ruta, entrada->d_name);    
                // Si la entrada es un directorio
                if (entrada->d_type == DT_DIR) {    
                    // Llama recursivamente a la función para eliminar el directorio
                    eliminar_directorio(ruta_completa);  
                } else {
                    // Imprimimos el nombre del archivo que se está eliminando
                    printf("removed '%s'\n", ruta_completa); 
                    // Eliminamos el archivo especificado por la ruta completa   
                    unlink(ruta_completa);  
                }
            }
        }
        // Cierra el directorio
        closedir(dir);  // Cierra el directorio
        // Imprimimos que el directorio ha sido eliminado
        printf("Directorio '%s' eliminado\n\n", ruta);   
        // Elimina el directorio especificado por 'ruta'
        rmdir(ruta);   
    }
}

//Funcion para crear el directorio de respaldo
void crear_directorio_respaldo(const char *directorio_respaldo) {
    // Estructura para almacenar información sobre un archivo
    struct stat st = {0};   
    // Obtenemos información sobre el directorio de respaldo
    if (stat(directorio_respaldo, &st) == -1) { 
        // Almacena el comando a ejecutar   
        char comando[512];  
        // Construye el comando para crear el directorio
        sprintf(comando, "mkdir -p %s", directorio_respaldo); 
        // Ejecuta el comando para crear el directorio de respaldo
        system(comando);    
    }
}

//Funcion para hacer el respaldo de archivos
void respaldar_archivos(const char *directorio_fuente, const char *directorio_respaldo, int num_archivos) {
    // Puntero a la estructura que representa un directorio
    DIR *dir;     
    // Puntero a la estructura que representa una entrada en un directorio                  
    struct dirent *entrada;         
     // Almacenan las rutas de origen y destino de los archivos
    char ruta_fuente[BUFFER_SIZE], ruta_destino[BUFFER_SIZE]; 
    // Número de archivos restantes por respaldar, inicialmente es igual al numero de archivos para hacer la cuenta descendiente de restantes
    int archivos_restantes = num_archivos;  

    // Imprimimos un mensaje indicando que el hijo está esperando un mensaje del padre
    printf("Hijo(pid=%d) esperando mensaje de mi padre...\n", getpid());   

    // Cierra el lado de escritura del pipe en el hijo
    close(fd[1]);   

    // Espera un mensaje inicial del padre
    char buffer[BUFFER_SIZE];
    // Lee un mensaje del pipe
    if (read(fd[0], buffer, BUFFER_SIZE) > 0) { 
        // Imprimimos el mensaje recibido del padre
        printf("Hijo(pid=%d), Instrucción de mi padre: %s\n", getpid(), buffer);   
    }

    // Abre el directorio de origen
    dir = opendir(directorio_fuente);  
    // Si ocurrió un error al abrir el directorio
    if (dir == NULL) {  
        // Imprimimos un mensaje de error
        perror("Error al abrir el directorio fuente");    
        // Sale del programa con un código de error
        exit(EXIT_FAILURE); 
    }

    // Lee cada entrada del directorio de origen
    while ((entrada = readdir(dir)) != NULL) {    
        // Si la entrada es un archivo regular
        if (entrada->d_type == DT_REG) {    
            // Construye la ruta de origen del archivo
            snprintf(ruta_fuente, BUFFER_SIZE, "%s/%s", directorio_fuente, entrada->d_name); 
            // Construye la ruta de destino del archivo
            snprintf(ruta_destino, BUFFER_SIZE, "%s/%s", directorio_respaldo, entrada->d_name); 

            // Abre el archivo de origen en modo lectura 
            FILE *fuente = fopen(ruta_fuente, "rb"); 
            // Abre el archivo de destino en modo escritura 
            FILE *destino = fopen(ruta_destino, "wb");   

            // Si se pudieron abrir correctamente ambos archivos
            if (fuente && destino) {    
                char ch;
                // Lee y escribe cada byte del archivo de origen al de destino
                while (fread(&ch, sizeof(char), 1, fuente) == 1) { 
                    fwrite(&ch, sizeof(char), 1, destino);
                }
                // Cierra el archivo de origen
                fclose(fuente); 
                // Cierra el archivo de destino 
                fclose(destino);
                // Decrementa el contador de archivos restantes por respaldar
                archivos_restantes--;   
                // Imprimimos el nombre del archivo que se está respaldando
                printf("(Padre --> %s)\n", entrada->d_name);  
                // Imprimimos información sobre el respaldo 
                printf("Hijo(pid=%d), respaldando el archivo: %s, pendientes %d/%d\n", getpid(), entrada->d_name, archivos_restantes, num_archivos); 
            } else {
                // Cierra el archivo de origen si se abrió correctamente
                if (fuente) fclose(fuente); 
                // Cierra el archivo de destino si se abrió correctamente
                if (destino) fclose(destino);   
                // Imprimimos un mensaje de error
                perror("Error al abrir archivos para respaldo");   // Imprime un mensaje de error
            }
        }
    }
    // Cierra el directorio de origen
    closedir(dir);  

    // Informa al padre que la tarea ha concluido
    char mensaje_final[] = "Adiós padre, terminé el respaldo...";
    // Envia un mensaje de finalización al padre a través del pipe
    write(fd[0], mensaje_final, strlen(mensaje_final) + 1);  
    // Cierra el lado de lectura del pipe
    close(fd[0]);
}

//Funcion para contar los archivos a respaldar
int contar_archivos(const char *directorio_fuente) {
    // Inicializa el contador de archivos
    int conteo = 0; 
    // Abre el directorio especificado por 'directorio_fuente'
    DIR *dir = opendir(directorio_fuente);  
    // Puntero a la estructura que representa una entrada en un directorio
    struct dirent *entrada; 

     // Si el directorio se abrió correctamente
    if (dir) {  
        // Lee cada entrada del directorio
        while ((entrada = readdir(dir)) != NULL) {  
            // Si la entrada es un archivo regular  
            if (entrada->d_type == DT_REG) {   
                // Incrementa el contador de archivos
                conteo++;   
            }
        }
        // Cierra el directorio
        closedir(dir);  
    }
    // Retorna el número de archivos contados
    return conteo;  
}

int main() {
    // Almacenan las rutas de los directorios de origen y destino
    char directorio_fuente[BUFFER_SIZE], directorio_respaldo[BUFFER_SIZE];   
    printf("---------------------PROCESOS RESPALDO-------------------\n");
    // Solicitamos al usuario la ruta del directorio de origen y destino
    printf("Ingrese el directorio fuente: ");
    scanf("%s", directorio_fuente);    
    printf("Ingrese el directorio destino (respaldo): ");
    scanf("%s", directorio_respaldo);  

    // Imprimimos un mensaje indicando que se está borrando el respaldo antiguo
    printf("\n\nPadre(pid=%d): borrando respaldo antiguo...\n", getpid());  
    // Llama a la función para eliminar el directorio de respaldo antiguo 
    eliminar_directorio(directorio_respaldo);   
    // Imprimimos un mensaje indicando que se está creando el directorio de respaldo
    printf("Padre(pid=%d): creando directorio de respaldo...\n", getpid());
    // Llama a la función para crear el directorio de respaldo  
    crear_directorio_respaldo(directorio_respaldo); 
    // Obtiene el número de archivos en el directorio de origen
    int num_archivos = contar_archivos(directorio_fuente); 
    // Imprimimos un mensaje indicando el inicio del proceso de respaldo
    printf("Padre(pid=%d): Hola hijo, realiza el respaldo de %d archivos \n", getpid(), num_archivos);   
    // Imprimimos un mensaje indicando la cantidad de archivos a respaldar
    printf("======= RESPALDANDO %d ARCHIVOS ======\n", num_archivos);  

    // Declaramos un array para el pipe
    int pipe_fd[2]; 
    // Se crea un pipe y verifica si hubo algún error
    if (pipe(pipe_fd) == -1) { 
        // Imprimimos un mensaje de error si falla la creación del pipe
        perror("Error al crear el pipe");  
        // Sale del programa con un código de error
        exit(EXIT_FAILURE); 
    }
    // Crea un nuevo proceso hijo
    pid_t pid = fork(); 
    // Si hubo un error en la creación del proceso hijo
    if (pid < 0) {  
        // Imprimimos un mensaje de error
        perror("Fallo en fork");   
        // Sale del programa con un código de error 
        exit(EXIT_FAILURE); 
    }

    // Tareas del proceso padre
    if (pid > 0) {
        // Cierra el lado de escritura del pipe en el padre  
        close(pipe_fd[1]);  
        // Almacena el mensaje enviado por el hijo
        char msg_del_hijo[BUFFER_SIZE];   
        // Lee el mensaje enviado por el hijo a través del pipe 
        ssize_t bytes_leidos = read(pipe_fd[0], msg_del_hijo, BUFFER_SIZE); 
        // Si hubo un error al leer del pipe 
        if (bytes_leidos < 0) { 
            // Imprimimos un mensaje de error
            perror("Error al leer del pipe");   
            // Sale del programa con un código de error
            exit(EXIT_FAILURE); 
        }
        // Imprimimos el mensaje recibido del hijo
        printf("Padre(pid=%d), mensaje del hijo: %s\n", getpid(), msg_del_hijo);   

        // Espera a que el hijo termine su ejecucion
        wait(NULL); 
        
        // Imprimimos un mensaje indicando que el padre está comprobando el respaldo
        printf("Padre(pid=%d) comprobando respaldo:\n", getpid());  
        printf("===================================================\n");
        // Arreglo que almacenará el comando a ejecutar
        char comando[2048]; 
        // Formatea y almacena el comando mediante sprintf
        sprintf(comando, "ls -l %s", directorio_respaldo);    
        // Ejecuta el comando almacenado en la var 'command'
        system(comando);    
        // Imprimimos la cantidad de archivos respaldados
        printf("%d\nARCHIVOS RESPALDADOS\n", num_archivos); 
        // Imprimimos un mensaje indicando que el proceso padre ha terminado
        printf("===================================================\nProceso padre terminado...\n"); 
    } else {    // Tareas del proceso hijo
        // Cerrar lado de lectura en el hijo
        close(pipe_fd[0]);  

        // Realiza el respaldo de los archivos
        respaldar_archivos(directorio_fuente, directorio_respaldo, num_archivos);

        // Mensaje que recibirá el padre
        char mensaje_final[] = "Adiós padre, terminé el respaldo...";
        // Se escribirá el mensaje anterior del lado de escritura del PIPE
        ssize_t bytes_escritos = write(pipe_fd[1], mensaje_final, strlen(mensaje_final) + 1); 
        // Si hubo un error al escribir en el pipe
        if (bytes_escritos < 0) { 
            // Si al escribir el mensaje falla, manda error
            perror("Error al escribir en el pipe"); 
            // Sale de la secuencia con el error
            exit(EXIT_FAILURE); 
        }
        // Cerrar lado de escritura después de enviar el mensaje al padre
        close(pipe_fd[1]);  

        // Termina la ejecución del proceso hijo con éxito
        exit(EXIT_SUCCESS); 
    }

    // CREANDO EL TXT

    // Referenciando el directorio de respaldo mediante un apuntador
    char *arch = directorio_respaldo;  
    // Comando para listar los archivos y guardarlos en un archivo
    // Buffer encargado de almacenar comandos
    char comando[1024]; 
    // Construye el comando para listar los archivos del directorio de respaldo y guardar el resultado en un archivo
    sprintf(comando, "ls %s > archivosRespaldados.txt", arch); 
    // Ejecuta el comando 
    system(comando);    

    // Reescribe las instrucciones del comando, ahora el comando añadirá una línea de texto indicando el total de archivos a respaldar
    // Reescribe las intrucciones del comando, por asi decirlo.
    sprintf(comando, "echo 'Total de archivos: $(ls %s | wc -l)' >> archivosRespaldados.txt", arch); 
    // Comando bash con seguridad mejorada para evitar desbordamiento de búfer
    // Asigna memoria para almacenar el comando bash
    char *bash = malloc(strlen(comando) + 20); 
    // Si no se pudo asignar memoria
    if (bash == NULL) { 
        // Imprime un mensaje de error
        // Si falla la asignación de memoria, da error
        fprintf(stderr, "Error al alocar memoria.\n"); 
        // Termina el programa si no se puede alocar memoria
        return 1;  
    }
    // "Formatea" el comando y lo ejecuta dentro de 'bash -c' para mayor seguridad
    sprintf(bash, "bash -c \"%s\"", comando);   
    // Ejecuta el comando bash
    system(bash);   
    // Libera la memoria asignada para el comando bash
    free(bash); 
    return 0;   
}
