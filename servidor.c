#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mysql/mysql.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define MAX_USERS 100
#define USERNAME_LENGTH 50
#define PASSWORD_LENGTH 50

MYSQL *conn;
MYSQL_RES *res;
MYSQL_ROW row;

// Función para conectar con la base de datos MySQL
int conectar_mysql() {
    conn = mysql_init(NULL);
    if (conn == NULL) {
        fprintf(stderr, "Error al inicializar MySQL: %s\n", mysql_error(conn));
        return 0;
    }
    if (mysql_real_connect(conn, "localhost", "root", "mysql", "mistablas", 0, NULL, 0) == NULL) {
        fprintf(stderr, "Error de conexión a la base de datos: %s\n", mysql_error(conn));
        mysql_close(conn);
        return 0;
    }
    return 1;
}

// Función para desconectar de la base de datos MySQL
void desconectar_mysql() {
    if (res != NULL) {
        mysql_free_result(res);
    }
    mysql_close(conn);
}

// Función para registrar un usuario
int registrar_usuario(char *jugador) {
    char query[512];
    snprintf(query, sizeof(query), "INSERT INTO jugadores (jugador) VALUES ('%s')", jugador);
    
    if (mysql_query(conn, query)) {
        fprintf(stderr, "Error al ejecutar la consulta de registro: %s\n", mysql_error(conn));
        return 0;
    }
    return 1;
}

// Función para verificar si un usuario existe
int verificar_usuario(char *jugador) {
    char query[512];
    snprintf(query, sizeof(query), "SELECT * FROM jugadores WHERE jugador='%s'", jugador);
    
    if (mysql_query(conn, query)) {
        fprintf(stderr, "Error al ejecutar la consulta de verificación: %s\n", mysql_error(conn));
        return 0;
    }
    
    res = mysql_store_result(conn);
    if (res == NULL) {
        fprintf(stderr, "Error al almacenar el resultado: %s\n", mysql_error(conn));
        return 0;
    }
    int exists = (mysql_num_rows(res) > 0);
    mysql_free_result(res); // Liberamos resultado aquí para evitar memory leaks
    return exists;
}

// Función que maneja la comunicación con un cliente
void *AtenderCliente(void *socket) {
    int sock_conn = *(int *)socket;
    free(socket);
    
    char peticion[512];
    char respuesta[512];
    int ret;
    int terminar = 0;
    
    while (!terminar) {
        ret = read(sock_conn, peticion, sizeof(peticion) - 1);
        if (ret <= 0) break;  // Si hay error o desconexión

        peticion[ret] = '\0'; // Añadir fin de cadena
        printf("Petición recibida: %s\n", peticion);

        // Procesar la petición
        char *p = strtok(peticion, "/");
        if (p == NULL) continue; // Evita segfault si strtok devuelve NULL

        int codigo = atoi(p);
        char *jugador;
        
        if (codigo == 0) {
            terminar = 1; // Desconexión
        } else if (codigo == 1) { // Login
            p = strtok(NULL, "/");
            if (p == NULL) continue;
            jugador = p;
            
            snprintf(respuesta, sizeof(respuesta), "1/%s", verificar_usuario(jugador) ? "OK" : "ERROR");
            write(sock_conn, respuesta, strlen(respuesta));
        } else if (codigo == 2) { // SignUp
            p = strtok(NULL, "/");
            if (p == NULL) continue;
            jugador = p;
            
            snprintf(respuesta, sizeof(respuesta), "2/%s", verificar_usuario(jugador) ? "ERROR" : (registrar_usuario(jugador) ? "OK" : "ERROR"));
            write(sock_conn, respuesta, strlen(respuesta));
        }

        printf("Respuesta enviada: %s\n", respuesta);
    }

    // Cerrar la conexión del cliente
    close(sock_conn);
    printf("Cliente desconectado\n");
    return NULL;
}

int main() {
    if (!conectar_mysql()) {
        fprintf(stderr, "No se pudo conectar a la base de datos\n");
        exit(1);
    }

    int sock_listen, *sock_conn;
    struct sockaddr_in serv_adr;
    pthread_t thread;

    // Crear el socket del servidor
    if ((sock_listen = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Error creando el socket");
        exit(1);
    }

    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(9010);

    // Asociar el socket al puerto
    if (bind(sock_listen, (struct sockaddr *)&serv_adr, sizeof(serv_adr)) < 0) {
        perror("Error en el bind");
        exit(1);
    }

    if (listen(sock_listen, 3) < 0) {
        perror("Error en el listen");
        exit(1);
    }

    printf("Servidor listo. Esperando conexiones...\n");

    // Bucle para aceptar clientes
    while (1) {
        sock_conn = malloc(sizeof(int)); // Reservar memoria para el socket
        if (sock_conn == NULL) {
            perror("Error al asignar memoria");
            continue;
        }
        *sock_conn = accept(sock_listen, NULL, NULL);
        if (*sock_conn < 0) {
            perror("Error en accept");
            free(sock_conn);
            continue;
        }
        printf("Cliente conectado\n");

        // Crear un hilo para atender al cliente
        pthread_create(&thread, NULL, AtenderCliente, sock_conn);
        pthread_detach(thread);
    }

    desconectar_mysql();
    return 0;
}
