#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <mysql.h>
#include <string.h>
#include <pthread.h>


//Estructuras
typedef struct{
	char nombre[20];
	int socket;
}Conectado;

typedef struct{
	Conectado conectados[100];
	int num;
}ListaConectados;

typedef struct{
	Conectado conectados[4];
	int numparticipantes,numinvitados;
	int turno,CasLibres;
}Partida;

//************************************************************************************************
//Variables globales
ListaConectados milistaconectados;
Partida partidas[500];
int numPartidas;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int contador= 0;
int resultado;
int jugador;
int consulta;
int puerto = 9060;

//************************************************************************************************
//Funciones del servidor

//-------------------------------------------------------------------------------------------------
//Funciones principales para ejecutar el codigo
//LogIn
void Loguearse(char nombre[20], char contrasenya[20], MYSQL *conn, int *sock_conn, ListaConectados *milistaconectados, char respuesta[500]) {
    // Función para loguear a un jugador:
    // Envía "1/2" si el login es exitoso, "1/1" si el usuario no está registrado.

    int registrado = BuscarID(nombre, contrasenya, conn);
    // Llama a BuscarID para comprobar si el usuario existe y la contraseña es correcta.
    // Si no existe devuelve -1, si existe devuelve un ID positivo.

    if (registrado == -1)
        strcpy(respuesta, "1/1/\n");
        // Si no está registrado (ID -1), copia en respuesta el código "1/1/" que indica fallo de login.

    else {
        strcpy(respuesta, "1/2/\n");
        // Si está registrado, copia en respuesta el código "1/2/" que indica login correcto.

        AnadirConectado(nombre, sock_conn, milistaconectados);
        // Añade el usuario a la lista de jugadores conectados, asociando su socket.
    }

    printf("Correcto %s\n", respuesta);
    // Muestra en consola la respuesta que se va a enviar, para depuración.

    write(sock_conn, respuesta, strlen(respuesta));
    // Envía la respuesta al cliente a través del socket.
}

void ConsultarID(char *usuario, MYSQL *conn, char *respuesta) {
    // Construye y ejecuta una consulta para obtener el id del usuario dado
    // No devuelve la consulta completa, solo la ID del usuario.

    char query[100];
    sprintf(query, "SELECT idj FROM jugadores WHERE usuario = '%s'", usuario);
    // Construye la consulta SQL para buscar el id del usuario.

    memset(respuesta, 0, 100);
    // Limpia el buffer respuesta para evitar basura de memoria.

    if (mysql_query(conn, query)) {
        // Ejecuta la consulta. Si hay error, se informa y se copia mensaje de error en respuesta.
        fprintf(stderr, "%s\n", mysql_error(conn));
        strcpy(respuesta, "Error en la consulta");
        return;
    }

    MYSQL_RES *result = mysql_store_result(conn);
    // Guarda los resultados de la consulta.

    if (result == NULL) {
        // Si no se obtienen resultados, se informa del error y se copia mensaje en respuesta.
        fprintf(stderr, "%s\n", mysql_error(conn));
        strcpy(respuesta, "Error al obtener resultados");
        return;
    }

    MYSQL_ROW row = mysql_fetch_row(result);
    // Obtiene la primera fila del resultado.

    if (row) {
        // Si hay resultados, formatea la respuesta con el código 3 y el id encontrado.
        sprintf(respuesta, "3/%s/", row[0]);
    } else {
        // Si no hay fila (usuario no encontrado), se indica en la respuesta.
        strcpy(respuesta, "Usuario no encontrado");
    }

    mysql_free_result(result);
    // Libera la memoria usada por el resultado.
}

void ObtenerJugadoresConocidos(const char *usuario, MYSQL *conn, char *respuesta) {
    char query[1024];

    if (strlen(usuario) == 0) {
        // Comprueba si el nombre de usuario está vacío y responde con error.
        snprintf(respuesta, 200, "0/UsuarioVacio/El nombre de usuario no puede estar vacío");
        return;
    }

    // Construye la consulta SQL para obtener usuarios que han jugado con el usuario dado,
    // excluyendo al propio usuario.
    snprintf(query, sizeof(query),
             "SELECT DISTINCT j2.usuario "
             "FROM partidas p "
             "JOIN participacion part1 ON p.idp = part1.idPart "
             "JOIN jugadores j1 ON part1.idJugadores = j1.idj "
             "JOIN participacion part2 ON p.idp = part2.idPart "
             "JOIN jugadores j2 ON part2.idJugadores = j2.idj "
             "WHERE j1.usuario = '%s' AND j2.usuario <> '%s'",
             usuario, usuario);

    printf("[DEBUG] Query: %s\n", query); // Muestra la consulta para depuración.

    if (mysql_query(conn, query)) {
        // Ejecuta la consulta, y si hay error, se responde con mensaje de error.
        snprintf(respuesta, 200, "0/ErrorDB/%s", mysql_error(conn));
        return;
    }

    MYSQL_RES *result = mysql_store_result(conn);
    // Guarda los resultados de la consulta.

    if (!result) {
        // Si no se obtienen resultados, se responde con mensaje de error.
        snprintf(respuesta, 200, "0/ErrorResult/");
        printf("[ERROR] Resultado vacío\n");
        return;
    }

    MYSQL_ROW row;
    strcpy(respuesta, "3");  // Código 3 para indicar lista de jugadores conocidos.

    int count = 0;
    printf("[DEBUG] Resultados encontrados:\n");

    while ((row = mysql_fetch_row(result))) {
        if (row[0] != NULL) {
            // Por cada fila con un usuario encontrado, concatena el nombre a la respuesta.
            strcat(respuesta, "/");
            strcat(respuesta, row[0]);
            count++;
            printf("[DEBUG] Jugador encontrado: %s\n", row[0]);
        }
    }

    if (count == 0) {
        // Si no se encontró ningún jugador conocido, se informa.
        strcpy(respuesta, "3/0/No se encontraron jugadores");
    }

    mysql_free_result(result);
    // Libera la memoria usada por el resultado.

    printf("[DEBUG] Respuesta final: %s\n", respuesta);
    // Muestra la respuesta final para depuración.
}

int BuscarID(char nombre[20], char contrasenya[20], MYSQL *conn) {
    // Busca la ID de un jugador por su nombre y contraseña.
    // Retorna 0 si lo encuentra, -1 si no.

    int err = 0;
    char str_query[512];
    MYSQL_RES *resultado;
    MYSQL_ROW row;

    // Construye la consulta SQL para buscar el jugador con nombre y contraseña dados.
    sprintf(str_query, "SELECT jugadores.idj, jugadores.usuario FROM jugadores WHERE usuario= '%s' AND contrasenya='%s';", nombre, contrasenya);

    err = mysql_query(conn, str_query);
    printf("1\n");

    if (err != 0) {
        // Si hay error en la consulta, imprime error y termina la ejecución.
        printf("Error al consultar datos de la base %u %s\n", mysql_errno(conn), mysql_error(conn));
        exit(1);
    }

    resultado = mysql_store_result(conn);
    row = mysql_fetch_row(resultado);

    if (row == NULL)
        // No encontró usuario con esa contraseña, devuelve -1.
        return -1;
    else {
        // Usuario encontrado, devuelve 0.
        return 0;
    }
}

void ObtenerResultadosPartidas(const char *usuario, const char *oponente, MYSQL *conn, char *respuesta) {
    char query[1024];

    // 1. Verificar que ambos jugadores existen en la base de datos
    snprintf(query, sizeof(query),
             "SELECT COUNT(*) FROM jugadores WHERE usuario IN ('%s', '%s')",
             usuario, oponente);
    // Construye la consulta para contar cuántos de los dos usuarios existen

    if (mysql_query(conn, query)) {
        // Si falla la consulta, devuelve error con mensaje de la base de datos
        snprintf(respuesta, 2048, "0/ErrorDB/%s", mysql_error(conn));
        return;
    }

    MYSQL_RES *check = mysql_store_result(conn);
    if (!check) {
        // Si no se obtienen resultados, devuelve error en resultado
        snprintf(respuesta, 2048, "0/ErrorResult/");
        return;
    }

    MYSQL_ROW check_row = mysql_fetch_row(check);
    int player_count = atoi(check_row[0]);
    // Convierte el resultado (conteo) a entero para comprobar cuántos jugadores existen

    mysql_free_result(check);
    // Libera memoria de resultados

    if (player_count < 2) {
        // Si no existen ambos jugadores, responde que no se encontraron jugadores
        snprintf(respuesta, 2048, "4/0/Jugador(es) no encontrado(s)");
        return;
    }

    // 2. Consulta para obtener el resultado de la última partida entre usuario y oponente
    snprintf(query, sizeof(query),
             "SELECT IF(p.ganador = '%s', 'Ganó', 'Perdió') as resultado "
             "FROM partidas p "
             "WHERE p.idp IN ("
             "  SELECT part1.idPart "
             "  FROM participacion part1 "
             "  JOIN jugadores j1 ON part1.idJugadores = j1.idj "
             "  WHERE j1.usuario = '%s'"
             ") AND p.idp IN ("
             "  SELECT part2.idPart "
             "  FROM participacion part2 "
             "  JOIN jugadores j2 ON part2.idJugadores = j2.idj "
             "  WHERE j2.usuario = '%s'"
             ") AND p.ganador IS NOT NULL "  // Excluir partidas sin ganador
             "ORDER BY p.fecha DESC LIMIT 1",
             usuario, usuario, oponente);
    // Esta consulta busca la última partida en la que ambos jugadores participaron
    // y cuyo ganador no sea NULL. Devuelve 'Ganó' si el usuario ganó o 'Perdió' si perdió.

    if (mysql_query(conn, query)) {
        // Si falla la consulta, responde con error de base de datos
        snprintf(respuesta, 2048, "0/ErrorDB/%s", mysql_error(conn));
        return;
    }

    MYSQL_RES *result = mysql_store_result(conn);
    if (!result) {
        // Si no hay resultados, responde con error de resultado
        snprintf(respuesta, 2048, "0/ErrorResult/");
        return;
    }

    MYSQL_ROW row = mysql_fetch_row(result);
    if (row && row[0]) {
        // Si hay resultado, devuelve código 4 y el resultado ("Ganó" o "Perdió")
        snprintf(respuesta, 2048, "4/%s", row[0]);
    } else {
        // Si no se encontraron partidas con resultado, informa que no hay datos
        snprintf(respuesta, 2048, "4/0/No se encontraron partidas con resultado entre %s y %s", usuario, oponente);
    }

    mysql_free_result(result);
    // Libera la memoria usada para el resultado
}


#include <mysql/mysql.h>    // Incluye la biblioteca de MySQL para manejar conexiones y consultas a la base de datos
#include <string.h>         // Incluye funciones para manejo de cadenas de texto (ej: strncpy, strtok)
#include <stdio.h>          // Incluye funciones básicas de entrada y salida (ej: printf, snprintf)

void ObtenerPartidasPorFecha(const char* peticion, MYSQL* conn, char* respuesta) {
    char usuario[56] = { 0 };        // Buffer para almacenar el nombre de usuario, inicializado a 0
    char fechaDesde[11] = { 0 };     // Buffer para almacenar la fecha desde en formato DD/MM/YYYY
    char fechaHasta[11] = { 0 };     // Buffer para almacenar la fecha hasta en formato DD/MM/YYYY
    char query[2048] = { 0 };        // Buffer para almacenar la consulta SQL, inicializado a 0, tamaño aumentado para evitar desbordes
    MYSQL_RES* result;               // Puntero para almacenar resultados de consultas MySQL
    MYSQL_ROW row;                   // Variable para almacenar una fila de resultados MySQL

    // 1. Hacer copia modificable de la petición (la original es const y strtok modifica la cadena)
    char peticion_copy[256] = { 0 }; // Buffer para copia de la petición
    strncpy(peticion_copy, peticion, sizeof(peticion_copy) - 1);  // Copia la petición en peticion_copy limitando el tamaño para evitar overflow
    peticion_copy[sizeof(peticion_copy) - 1] = '\0';             // Asegura que la cadena esté terminada en null

    printf("Peticion recibida: %s\n", peticion_copy);            // Imprime la petición copiada para depuración

    // 2. Parsear la petición con strtok usando '/' como separador, esperando formato: "codigo/usuario/fechaDesde/fechaHasta"
    char* token = strtok(peticion_copy, "/");                     // Obtiene el primer token, que es el código (que se va a ignorar)

    // Saltar el código (primer token)
    if (!token) {                                                 // Si no hay token, la petición está mal formada
        snprintf(respuesta, 2048, "0/FormatoInvalido/FaltaCodigo"); // Respuesta de error indicando falta de código
        return;                                                   // Salir de la función
    }

    // Obtener usuario (segundo token)
    token = strtok(NULL, "/");                                    // Obtiene el segundo token que debe ser el usuario
    if (!token) {                                                 // Si no hay usuario, la petición está mal formada
        snprintf(respuesta, 2048, "0/FormatoInvalido/FaltaUsuario"); // Respuesta de error indicando falta de usuario
        return;
    }
    strncpy(usuario, token, sizeof(usuario) - 1);                 // Copia el usuario en el buffer usuario
    usuario[sizeof(usuario) - 1] = '\0';                          // Asegura terminación en null

    // Obtener fechaDesde (tercer token)
    token = strtok(NULL, "/");                                    // Obtiene el tercer token que debe ser fechaDesde
    if (!token) {                                                 // Si falta fechaDesde, error
        snprintf(respuesta, 2048, "0/FormatoInvalido/FaltaFechaDesde");
        return;
    }
    strncpy(fechaDesde, token, sizeof(fechaDesde) - 1);           // Copia fechaDesde en su buffer
    fechaDesde[sizeof(fechaDesde) - 1] = '\0';                    // Termina en null

    // Obtener fechaHasta (cuarto token)
    token = strtok(NULL, "/");                                    // Obtiene el cuarto token que debe ser fechaHasta
    if (!token) {                                                 // Si falta fechaHasta, error
        snprintf(respuesta, 2048, "0/FormatoInvalido/FaltaFechaHasta");
        return;
    }
    strncpy(fechaHasta, token, sizeof(fechaHasta) - 1);           // Copia fechaHasta en su buffer
    fechaHasta[sizeof(fechaHasta) - 1] = '\0';                    // Termina en null

    // Imprime por consola los datos parseados para comprobar que se leyeron bien
    printf("Datos parseados: Usuario=%s, Desde=%s, Hasta=%s\n", usuario, fechaDesde, fechaHasta);
}
	// 3. Verificar que el usuario existe
char checkQuery[256] = { 0 }; 
// Buffer para almacenar la consulta SQL que verifica si el usuario existe en la tabla jugadores

snprintf(checkQuery, sizeof(checkQuery),
	"SELECT COUNT(*) FROM jugadores WHERE usuario = '%s'", usuario);
// Construye la consulta SQL para contar cuántos registros de jugadores tienen el usuario dado

if (mysql_query(conn, checkQuery)) {
	// Ejecuta la consulta en la conexión MySQL, si falla:
	snprintf(respuesta, 2048, "8/ErrorDB/%s", mysql_error(conn));
	// Guarda en respuesta un mensaje de error indicando error en la base de datos con el mensaje de MySQL
	return; 
	// Sale de la función
}

MYSQL_RES* checkResult = mysql_store_result(conn);
// Obtiene el resultado de la consulta anterior (el conteo)

if (!checkResult) {
	// Si no hay resultado válido:
	snprintf(respuesta, 2048, "0/ErrorResult/");
	// Mensaje de error genérico indicando problema al obtener resultados
	return;
}

row = mysql_fetch_row(checkResult);
// Obtiene la primera fila del resultado (debería ser sólo una con el COUNT(*))

int player_count = row ? atoi(row[0]) : 0;
// Convierte el valor de la primera columna (conteo) a entero. Si no hay fila, cuenta como 0

mysql_free_result(checkResult);
// Libera la memoria usada para el resultado

if (player_count < 1) {
	// Si el conteo es menor que 1 (usuario no existe)
	snprintf(respuesta, 2048, "5/0/Jugador no encontrado");
	// Respuesta indicando que el jugador no fue encontrado
	return;
}

// 4. Consulta principal con las fechas correctamente incluidas
snprintf(query, sizeof(query),
	"SELECT p.idp, p.fecha, p.duracion, j.usuario "
	"FROM partidas p "
	"JOIN jugadores j ON p.ganador = j.idj "
	"WHERE p.idp IN ("
	"   SELECT part.idPart FROM participacion part "
	"   JOIN jugadores jug ON part.idJugadores = jug.idj "
	"   WHERE jug.usuario = '%s') "
	"AND STR_TO_DATE(p.fecha, '%%d/%%m/%%Y') BETWEEN "
	"STR_TO_DATE('%s', '%%d/%%m/%%Y') AND STR_TO_DATE('%s', '%%d/%%m/%%Y') "
	"ORDER BY STR_TO_DATE(p.fecha, '%%d/%%m/%%Y') DESC",
	usuario, fechaDesde, fechaHasta);
// Construye una consulta SQL que:
// - Obtiene partidas donde el jugador participó,
// - Que tienen fecha entre fechaDesde y fechaHasta (conversión a fecha real para comparar),
// - Devuelve id, fecha, duración y usuario ganador,
// - Ordena los resultados por fecha descendente.

printf("Consulta partidas:\n%s\n", query);
// Imprime la consulta generada para depuración

printf("Consulta SQL generada:\n%s\n", query); // Para depuración

if (mysql_query(conn, query)) {
	// Ejecuta la consulta principal y comprueba si hay error
	snprintf(respuesta, 2048, "8/ErrorDB/%s", mysql_error(conn));
	// Si hay error, genera respuesta con mensaje de error
	return;
}

result = mysql_store_result(conn);
// Obtiene el resultado de la consulta principal

if (!result) {
	// Si no hay resultados válidos
	snprintf(respuesta, 2048, "0/ErrorResult/");
	// Mensaje genérico de error en resultado
	return;
}

int num_rows = mysql_num_rows(result);
// Obtiene el número de filas resultantes

if (num_rows == 0) {
	// Si no hay partidas en el rango de fechas
	snprintf(respuesta, 2048, "5/0/No se encontraron partidas en ese rango.");
	// Mensaje indicando que no se encontraron partidas
	mysql_free_result(result);
	// Libera la memoria del resultado
	return;
}

	// 5. Construir respuesta
strcpy(respuesta, "8/");
// Inicializa la cadena 'respuesta' con "8/", que podría ser un código de éxito o tipo de mensaje

while ((row = mysql_fetch_row(result))) {
    // Mientras haya filas en el resultado de la consulta

    strcat(respuesta, row[0]); // idp
    // Añade el ID de la partida (idp) a la respuesta

    strcat(respuesta, "/");
    // Añade un separador "/" después del idp

    strcat(respuesta, row[1]); // fecha
    // Añade la fecha de la partida a la respuesta

    strcat(respuesta, "/");
    // Añade otro separador "/"

    strcat(respuesta, row[2]); // duracion
    // Añade la duración de la partida

    strcat(respuesta, "/");
    // Añade separador "/"

    strcat(respuesta, row[3]); // ganador
    // Añade el nombre del ganador

    strcat(respuesta, "/");
    // Añade otro separador al final para separar cada registro
}

mysql_free_result(result);
// Libera la memoria ocupada por el resultado de la consulta
		
		
	
	int AnadirConectado(char nom[20], int socket, ListaConectados *l){
    // Retorna -1 si la lista está llena, 0 si se añade correctamente

    if(l->num == 100){
        // Si ya hay 100 usuarios conectados, no se puede añadir más
        return -1;
    }
    else{
        printf("Entro\n");
        // Busca si el nombre ya existe en la lista

        int posConectado = DamePosicion(socket, l);
        // Obtiene la posición que corresponde a ese socket en la lista

        for (int i = 0; i < l->num; i++) {
            if (strcmp(l->conectados[i].nombre, nom) == 0) {
                // Si el nombre ya está en la lista, retorna error -2 (nombre repetido)
                return -2;
            }
        }

        strcpy(l->conectados[posConectado].nombre, nom);
        // Copia el nombre del usuario en la posición asignada

        printf("Salgo\n");
        return 0;
        // Añadido exitosamente
    }
}

void EliminarConectado(int *socket, ListaConectados *l){
    int p = -1;
    // Variable para guardar la posición del socket a eliminar

    // Buscar la posición del socket en la lista
    for(int i = 0; i < l->num; i++) {
        if(l->conectados[i].socket == *socket) {
            p = i;
            break;
        }
    }

    if (p >= 0 && p < l->num) {
        // Si se encontró el socket en la lista

        memset(&l->conectados[p], 0, sizeof(Conectado));
        // Limpia el contenido en esa posición (pone todo a 0)

        for(int i = p; i < l->num - 1; i++) {
            l->conectados[i] = l->conectados[i+1];
            // Desplaza todos los elementos posteriores una posición hacia adelante para no dejar huecos
        }

        memset(&l->conectados[l->num - 1], 0, sizeof(Conectado));
        // Limpia la última posición (ya duplicada por el desplazamiento)

        l->num--;
        // Reduce el número total de usuarios conectados en la lista
    }
}

// Función que gestiona el registro de un nuevo usuario
void Registrarse(char nombre[20], char contrasenya[20], MYSQL *conn, int *sock_conn, char respuesta[500]) {
	char str_query[500];         // Cadena donde se construirá la consulta SQL
	int err;                     // Variable para guardar el código de error de MySQL
	MYSQL_RES *resultado;        // Resultado de la consulta SQL
	MYSQL_ROW row;               // Fila individual del resultado

	// Llama a la función que busca si el usuario ya está registrado
	int reg = BuscaRegistrados(nombre, conn);

	// Si el usuario YA EXISTE (la lógica está invertida: reg == -1 significa usuario ya registrado)
	if (reg == -1) {
		printf("El usuario ya está registrado\n");
		strcpy(respuesta, "2/1/\n");  // Código para "usuario ya registrado"
		write(sock_conn, respuesta, strlen(respuesta));  // Se envía la respuesta al cliente
	}
	else {
		// El usuario NO está registrado: se procede a registrarlo
		strcpy(respuesta, "2/2/\n");  // Código para "registro exitoso"
		int id = 0;

		// Consulta para obtener el último idj (id de jugador)
		strcpy(str_query, "SELECT MAX(idj) FROM jugadores;");
		err = mysql_query(conn, str_query);  // Ejecuta la consulta
		if (err != 0) {
			printf("Error al consultar la base de datos para el id: %u %s\n",
			       mysql_errno(conn), mysql_error(conn));
		}

		// Almacena el resultado y obtiene la fila
		resultado = mysql_store_result(conn);
		row = mysql_fetch_row(resultado);
		if (row != NULL) {
			id = atoi(row[0]);  // Convierte el valor string a entero
			id = id + 1;        // Se incrementa para generar un nuevo id único
		}

		// Se construye la consulta de inserción del nuevo jugador
		sprintf(str_query, "INSERT INTO jugadores VALUES ('%d','%s', '%s', %d);", id, nombre, contrasenya, 0);
		err = mysql_query(conn, str_query);  // Ejecuta la consulta de inserción
		if (err != 0) {
			printf("Error al insertar nuevo usuario en la base de datos: %u %s\n",
			       mysql_errno(conn), mysql_error(conn));
		}

		// Se envía la respuesta al cliente indicando que el registro se ha realizado
		write(sock_conn, respuesta, strlen(respuesta));
		printf("salgo del register\n");
	}
}

// Función auxiliar para verificar si un usuario ya está registrado
int BuscaRegistrados(char nombre[20], MYSQL *conn) {
	// Retorna -1 si el usuario ya existe, 0 si no existe

	char str_query[500];          // Consulta SQL
	int err;                      // Código de error de MySQL
	MYSQL_RES *resultado;         // Resultado de la consulta
	MYSQL_ROW row;                // Fila de resultado

	// Consulta para buscar si el nombre ya existe en la tabla jugadores
	sprintf(str_query, "SELECT jugadores.usuario FROM jugadores WHERE jugadores.usuario = '%s'", nombre);
	err = mysql_query(conn, str_query);  // Ejecuta la consulta

	if (err != 0) {
		printf("Error al consultar la base de datos: %u %s\n",
		       mysql_errno(conn), mysql_error(conn));
	}

	resultado = mysql_store_result(conn);  // Almacena resultado
	row = mysql_fetch_row(resultado);      // Obtiene una fila

	if (row != NULL) {
		// Si se ha encontrado una fila, el usuario ya existe
		return -1;
	}
	else {
		// Si no hay filas, el usuario no está registrado
		return 0;
	}
}

		

//Lista conectados
void DameConectados(char VectorConectados[100], ListaConectados *l) {
	// Inicializa el vector a ceros
	memset(VectorConectados, 0, 100);

	// Comienza el mensaje con el código "6/" (usado para listar conectados)
	strcpy(VectorConectados, "6/");

	// Si hay conectados
	if(l->num > 0) {
		for(int i = 0; i < l->num; i++) {
			// Añade el nombre si no está vacío
			if(strlen(l->conectados[i].nombre) > 0) {
				strcat(VectorConectados, l->conectados[i].nombre); // Añade el nombre
				strcat(VectorConectados, "/"); // Separador
			}
		}
	}

	// Termina el mensaje con salto de línea
	strcat(VectorConectados, "\n");

	// Imprime por consola para depuración
	printf("Lista conectados: %s\n", VectorConectados);
}


void EnviarListaConectados(char LConectados[500], ListaConectados *l) {
	// Envía la lista de conectados a todos los sockets activos
	for(int i = 0; i != l->num; i++) {
		write(l->conectados[i].socket, LConectados, sizeof(LConectados));
		printf("Envio a %s, con socket %d y le mando %s\n", l->conectados[i].nombre, l->conectados[i].socket, LConectados);
	}
}

//Metodo de Invitacion
oid Invitacion(char nombre[20], char anfitrion[20], ListaConectados *l, int *socket, int *EnPartida, Partida partidas[500]) {
	// Envia una invitación de partida al jugador especificado por 'nombre'
	char respuesta[500], respuesta2[500];

	if (*EnPartida == -1) {
		// Si el jugador anfitrión no está en ninguna partida

		int libre = PartidaLibre(partidas); // Busca una partida libre

		if (libre != -1) {
			// Si se ha encontrado una partida disponible

			int n = AnadirAPartida(partidas, anfitrion, libre, l); // Añade anfitrión a la partida

			// Mensaje que se enviará al invitado
			sprintf(respuesta, "7/%d/%s/", libre, anfitrion);

			// Mensaje que se enviará al anfitrión
			sprintf(respuesta2, "8/4/%d/", libre);

			numPartidas += 1;       // Incrementa el número total de partidas
			*EnPartida = libre;     // Actualiza la posición de la partida en la que está

			write(socket, respuesta2, sizeof(respuesta2)); // Notifica al anfitrión
		} else {
			// No hay partida libre
			sprintf(respuesta2, "8/6/", libre);
			write(socket, respuesta2, sizeof(respuesta2));
		}
	} else {
		// El anfitrión ya tiene partida activa, solo manda invitación
		sprintf(respuesta, "7/%d/%s/", *EnPartida, anfitrion);
	}

	// Obtiene el socket del jugador invitado
	int socketInv = DameSocket(l, nombre);

	// Envía la invitación al invitado
	write(socketInv, respuesta, strlen(respuesta));
}


//Metodo de AceptarInvitacion
void AceptarInvitacion(Partida partidas[500], char invitado[20], ListaConectados *l, int numPartida) {
	char respuesta[500];

	// Intenta añadir al invitado a la partida indicada
	int accept = AnadirAPartida(partidas, invitado, numPartida, l);

	if (accept == -1) {
		// Si la partida está llena, envía "8/3/" (error) a todos los conectados de esa partida
		strcpy(respuesta, "8/3/");
		for (int i = 0; i < partidas[numPartida].numparticipantes; i++) {
			write(partidas[numPartida].conectados[i].socket, respuesta, sizeof(respuesta));
		}
	} else if (accept == 0) {
		// Aceptación exitosa: notifica a todos los miembros con "8/1/invitado/"
		sprintf(respuesta, "8/1/%s/", invitado);
		for (int i = 0; i < partidas[numPartida].numparticipantes; i++) {
			write(partidas[numPartida].conectados[i].socket, respuesta, sizeof(respuesta));
		}
	}
}

int AnadirAPartida(Partida partidas[500], char nombre[20], int numPartida, ListaConectados *lc) {
	// Añade un jugador a la partida si hay hueco. Máximo 5 jugadores.

	if (partidas[numPartida].numparticipantes <= 4) {  // Asegura que no se excede el máximo
		// Añade el socket del jugador
		partidas[numPartida].conectados[partidas[numPartida].numparticipantes].socket = DameSocket(lc, nombre);

		// Copia el nombre del jugador
		strcpy(partidas[numPartida].conectados[partidas[numPartida].numparticipantes].nombre, nombre);

		// Aumenta el contador de participantes
		partidas[numPartida].numparticipantes += 1;

		return 0; // Éxito
	} else {
		return -1; // Partida llena
	}
}


int PartidaLibre(Partida partidas[500]){
	// Busca en un vector de partidas la primera posición libre (sin participantes)

	int libre = -1, encontrado = 0;  // 'libre' guarda la posición libre, '-1' si no se encuentra. 'encontrado' indica si ya se encontró.
	int i = 0; // Iterador

	while((i < 500) && (encontrado == 0)){  // Mientras haya partidas y no se haya encontrado una libre
		if(partidas[i].numparticipantes == 0){  // Si la partida está vacía
			libre = i;        // Se guarda la posición libre
			encontrado = 1;   // Se marca como encontrado
		}
		if(!encontrado)  // Si no se ha encontrado, se avanza al siguiente índice
		   i++;
	}

	return libre; // Devuelve la posición libre o -1 si no hay
}

int BuscarEnPartidas(Partida partidas[500],char nombre[20],int *libre, int Posiciones[500]){
	//Busca a la persona con nombre X y busca si est� dentro de alguna partida, retorna un vector de partidas en las que participa 
	//y un entero con la cantidad de partidas que participa, retorna 0 y en un entero la primera posicion libre si no se le ha encontrado
	int i=0,encontrado=0;
	while((i<500) && (encontrado!=0)){
		for(int j=0;j<partidas[i].numparticipantes;j++){
			if(strcmp(partidas[i].conectados[j].nombre,nombre)==0){
				encontrado+=1;
			}
		}
	}
	
	libre = 0;
	if(!encontrado){
		libre = PartidaLibre(partidas);
		return 0;
	}
	if(encontrado)
		return encontrado;
}

void EliminarDePartida(Partida partidas[500], char nombre[20], int numPartida){
	// Elimina a un jugador de una partida específica
	int i = 0, encontrado = 0;

	if(numPartida != -1){  // Verifica que se ha especificado una partida válida

		// Busca al jugador en la partida
		while((i < partidas[numPartida].numparticipantes) && (encontrado == 0)){
			if(strcmp(partidas[numPartida].conectados[i].nombre, nombre) == 0)
				encontrado = 1;  // Se marca que se encontró
			if(encontrado != 1)
				i++;  // Solo avanza si aún no se ha encontrado
		}

		if(encontrado == 1){
			// Si se encontró, se procede a eliminarlo

			while(i < partidas[numPartida].numparticipantes){
				if(partidas[numPartida].numparticipantes != 1){
					// Si hay más de un participante

					if(partidas[numPartida].numparticipantes != 4)
						partidas[numPartida].conectados[i] = partidas[numPartida].conectados[i + 1];  // Desplaza los demás

					// Reduce el número de participantes
					partidas[numPartida].numparticipantes -= 1;
					i++;
				}else if(partidas[numPartida].numparticipantes == 1){
					// Si es el último participante, borra la partida
					partidas[numPartida].numparticipantes -= 1;
					numPartidas -= 1;  // Decrementa el contador global
					break;
				}
			}
		}
	}
}

//Chat global
void EnviarMensajeGlobal(ListaConectados *l, char nombre[20], char mensaje[500]){
	char respuesta[800];

	// Construye el mensaje con el formato "9/1/Nombre/Mensaje/"
	sprintf(respuesta, "9/1/%s/%s/", nombre, mensaje);

	// Imprime por consola (útil para debug)
	printf("%s", respuesta);

	// Envía el mensaje a todos los sockets conectados
	for(int i = 0; i < l->num; i++){
		write(l->conectados[i].socket, respuesta, sizeof(respuesta));
	}
}


	
	
void EliminarJugadorDeBD(MYSQL *conn, char *nombre) {
	// Elimina de la base de datos al jugador con nombre "nombre"
	
	pthread_mutex_lock(&mutex);  // Protege la zona crítica para evitar conflictos entre hilos con la base de datos

	char consulta[256];           // Para construir las consultas SQL
    MYSQL_RES *res;               // Resultado de una consulta SELECT
    MYSQL_ROW row;                // Fila individual de un resultado

    // 1. Buscar el idj del jugador por su nombre
    sprintf(consulta, "SELECT idj FROM jugadores WHERE usuario='%s'", nombre);  // Prepara consulta SQL
    if (mysql_query(conn, consulta) != 0) {  // Ejecuta la consulta y verifica errores
        printf("Error buscando jugador: %s\n", mysql_error(conn));
        return;  // Si hay error, termina la función
    }

    res = mysql_store_result(conn);  // Recupera el resultado de la consulta
    if (res == NULL || mysql_num_rows(res) == 0) {  // Si no se encontró nada o hubo error
        printf("Jugador '%s' no encontrado en la base de datos.\n", nombre);
        mysql_free_result(res);  // Libera memoria, aunque sea NULL por seguridad
        return;  // Sale de la función
    }

    row = mysql_fetch_row(res);  // Toma la primera fila del resultado
    int idj = atoi(row[0]);      // Convierte el idj (como texto) a entero
    mysql_free_result(res);      // Libera el resultado ya que ya no se necesita

    printf("ID del jugador a eliminar: %d\n", idj);  // Muestra el ID obtenido (debug)

    // 2. Eliminar participaciones del jugador
    sprintf(consulta, "DELETE FROM participacion WHERE idJugadores=%d", idj);  // Prepara eliminación
    if (mysql_query(conn, consulta) != 0)  // Ejecuta la eliminación y muestra error si lo hay
        printf("Error eliminando de participacion: %s\n", mysql_error(conn));

    // 3. Eliminar jugador
    sprintf(consulta, "DELETE FROM jugadores WHERE idj=%d", idj);  // Prepara eliminación del jugador
    if (mysql_query(conn, consulta) != 0)  // Ejecuta y muestra error si ocurre
        printf("Error eliminando jugador: %s\n", mysql_error(conn));
    else
        printf("Jugador '%s' eliminado correctamente de la BD.\n", nombre);  // Confirmación

	// Nota: faltaría desbloquear el mutex al final si se va a usar correctamente (con `pthread_mutex_unlock`)
}

//Chat local(chat de partidas)
void EnviarMensajePartida(Partida partidas[500], int partida, char nombre[20], char mensaje[500]){
	// Envía un mensaje de chat local a todos los jugadores conectados a una partida específica

	char respuesta[800];  // Buffer para el mensaje completo a enviar

	// Construye el mensaje en formato: "9/2/nombre/mensaje/"
	sprintf(respuesta, "9/2/%s/%s/", nombre, mensaje);

	printf("%s", respuesta);  // Imprime el mensaje en consola (debug)

	// Enviar el mensaje a todos los participantes de la partida indicada
	for(int i = 0; i < partidas[partida].numparticipantes; i++){
		write(partidas[partida].conectados[i].socket, respuesta, sizeof(respuesta));  // Envío por socket
	}
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------


int DameSocket(ListaConectados *l, char nom[20]){
    // Retorna el socket de un usuario dado su nombre; -1 si no se encuentra
    int i = 0;
    int encontrado = 0;

    // Bucle para buscar el nombre en la lista de conectados
    while((!encontrado) && (i != l->num)){
        if(strcmp(l->conectados[i].nombre, nom) == 0)  // Compara nombre buscado con el nombre en la posición i
            encontrado = 1;  // Marca como encontrado
        else
            i++;  // Avanza al siguiente conectado
    }

    if(!encontrado){
        return -1;  // No se encontró el usuario
    }
    else {
        int n = l->conectados[i].socket;  // Recupera el socket asociado
        return n;  // Devuelve el socket
    }
}

int DamePosicion(int *socket, ListaConectados *l){
    // Retorna la posición del usuario según su socket. Si no está, devuelve primera posición libre o -1 si está lleno.

    for(int i = 0; i < l->num; i++) {
        if(l->conectados[i].socket == socket) {
            return i;  // Devuelve la posición del usuario si encuentra el socket
        }
    }

    // Si no está, se retorna la primera posición libre (al final de la lista actual)
    if(l->num < 100) {
        return l->num;  // Posición disponible (como inserción)
    }

    return -1;  // No hay espacio disponible
}

int Conected(char nom[20], ListaConectados *l){
    // Retorna 0 si el usuario está conectado; -1 si no lo está
    int i = 0, encontrado = 0;

    // Búsqueda en la lista de conectados
    while((i < l->num) && (encontrado != 1)){
        if(strcmp(nom, l->conectados[i].nombre) == 0)
            encontrado = 1;  // Se encontró
        else
            i++;  // Sigue buscando
    }

    if(encontrado == 1)
        return 0;  // El usuario está conectado

    return -1;  // El usuario NO está conectado
}
//Juego
int DameTurno(char color[20]){
    // Devuelve el número de turno según el color
    if(strcmp(color, "Blue") == 0)
        return 0;
    else if(strcmp(color, "Green") == 0)
        return 1;
    else if(strcmp(color, "Red") == 0)
        return 2;
    else if(strcmp(color, "Yellow") == 0)
        return 3;
}

vvoid DameColor(char color[20], int turno){
    // Devuelve el color en texto a partir de un número de turno
    if(turno == 0)
        strcpy(color, "Blue");
    else if(turno == 1)
        strcpy(color, "Green");
    else if(turno == 2)
        strcpy(color, "Red");
    else if(turno == 3)
        strcpy(color, "Yellow");
}



//**************************************************************************************************
// Función para atender a un cliente, que se ejecuta en un hilo separado
void *AtenderCliente(void * socket)
{
    // Muestra por consola el valor del puntero socket (la dirección)
    printf("te doy el socket %d",socket);
    
    // Convierte el puntero genérico a un puntero a entero y obtiene el valor del socket conectado
    int sock_conn = *((int *) socket);
    
    // Variables para guardar información del cliente
    char nombre[20];        // nombre del usuario
    char Usuario[20];       // usuario (puede usarse para login)
    char desde[20];         // variable posiblemente para rangos de fechas u horas (no usada aquí)
    char hasta[20];         // idem anterior
    
    // Buffers para comunicación
    char peticion[500];     // buffer para recibir peticiones del cliente
    char respuesta[500];    // buffer para preparar respuestas al cliente
    char LConectados[500];  // buffer para lista de usuarios conectados
    
    int ret;                // variable para guardar el resultado de funciones como read()
    int numinvitados = 0;   // contador para invitados (no usado en este fragmento)
    char logged[20];        // variable para controlar estado logueado (no usada aquí)
    int terminar = 0;       // bandera para terminar el ciclo de atención al cliente
    int mipartida = -1;     // índice o id de la partida en la que participa el cliente, inicializado a -1 (sin partida)
    
    MYSQL *conn;            // puntero para la conexión a la base de datos MySQL
    int err;                // variable para guardar códigos de error (no usada aquí)
    
    MYSQL_RES *resultado;   // puntero para resultados de consultas MySQL
    MYSQL_ROW row;          // fila para recorrer resultados MySQL
    
    char consulta[80];      // buffer para construir consultas SQL
    conn = mysql_init(NULL);  // inicializa la estructura de conexión MySQL
    
    char str_query[512];    // buffer para consultas más largas (no usado aquí)
    //char str_query1[512]; // comentario, no usado
    
    int partidas_ganadas;   // variable para contar partidas ganadas (no usada aquí)
    
    // Se inicializa la conexión MySQL, vuelve a llamar mysql_init (innecesario repetir)
    conn = mysql_init(NULL);
    
    // Comprobar si la inicialización fue exitosa
    if (conn == NULL)
    {
        // Imprime error y sale del programa si no se pudo crear la conexión
        printf (" crear la conexion: %u %s\n", mysql_errno(conn), mysql_error(conn));
        exit (1);
    }
    
    // Establece la conexión con la base de datos local "BBDDJuego" con usuario root y password "mysql"
    conn = mysql_real_connect (conn, "localhost", "root", "mysql", "BBDDJuego", 0, NULL, 0);
    
    // Si no se puede conectar, muestra error y termina el programa
    if (conn == NULL)
    {
        printf ("Error al inicializar la conexion: %u %s\n",
                mysql_errno(conn), mysql_error(conn));
        exit (1);
    }
}
	while(!terminar){
			printf("--------------------------------------------------------------------\n");
			peticion[0] = '\0';
			// recibimos la peticion
			ret = read(sock_conn, peticion,sizeof(peticion));
			printf("Recibido\n");
			
			//Tenemos que anadirle la marca de fin de string
			//para que no escriba lo que hay despues en el buffer
			peticion[ret]='\0';
			
			printf ("Peticion: %s\n",peticion);
			
			//Recibir la peticion
			char *p = strtok( peticion, "/");
			int codigo =  atoi (p);
			char usuario[25];
			char usuario2[25];
			
			char respt[100];
			
			// Ya tenemos el codigo de la peticion
			printf ("Codigo: %d\n", codigo);
			printf("Mensaje enviado por socket: %d\n",sock_conn);
			
			if (codigo ==0) {
				printf("Cliente desconectado: %s\n", nombre);
				
				pthread_mutex_lock(&mutex);
				if(mipartida != -1) {
					EliminarDePartida(partidas, nombre, mipartida);
					// Notificar a los demás jugadores de la partida
					char notificacion[100];
					sprintf(notificacion, "8/7/%s/", nombre); // Código para notificar que un jugador abandonó
					
					for(int i = 0; i < partidas[mipartida].numparticipantes; i++) {
						write(partidas[mipartida].conectados[i].socket, notificacion, strlen(notificacion));
					}
				}
				EliminarConectado(&sock_conn, &milistaconectados);
				if(milistaconectados.num > 0) {
					char LConectados[500];
					char nombres[480]; // Espacio suficiente para nombres
					DameConectados(nombres, &milistaconectados); // genera "Juan/Maria/Pedro/"
					sprintf(LConectados, "6/%s", nombres); 
					for (int i = 0; i < milistaconectados.num; i++) {
						write(milistaconectados.conectados[i].socket, LConectados, strlen(LConectados));
					}
				}
				
				
				pthread_mutex_unlock(&mutex);
				close(sock_conn);
				terminar = 1;
				pthread_exit(NULL); // Exit the thread
			
				
			}//peticion de desconexion
				
			else if(codigo==1){ //logearse añadir lista conectados
				p = strtok( NULL, "/");
				strcpy (nombre, p);
				strcpy (Usuario, p);
				p=strtok(NULL,"/");
				char contrasenya[20];
				strcpy(contrasenya,p);
				printf ("Nombre: %s\n", nombre);
				printf("%s %s \n",nombre, contrasenya);
				
				//Funcion para loguearse
				pthread_mutex_lock(&mutex);
				Loguearse(nombre,contrasenya,conn,sock_conn,&milistaconectados,respuesta);
				pthread_mutex_unlock(&mutex);
				printf("Logueando a %s\n",nombre);
				
				if(resultado == -2) {
					// Usuario ya conectado
					strcpy(respuesta, "1/3/Usuario ya conectado/\n");
					write(sock_conn, respuesta, strlen(respuesta));
				} else {
					// Actualizar lista de conectados para todos
					char LConectados[500];                
					DameConectados(LConectados, &milistaconectados);
			
					pthread_mutex_lock(&mutex);
					for(int i = 0; i < milistaconectados.num; i++) {
						write(milistaconectados.conectados[i].socket, LConectados, strlen(LConectados));
					}
					pthread_mutex_unlock(&mutex);
				}
			}
			else if (codigo ==2) //registrarse
			{
				p = strtok( NULL, "/");
				strcpy (nombre, p);
				p=strtok(NULL,"/");
				char contrasenya[20];
				strcpy(contrasenya,p);
				printf ("Codigo: %d, Nombre: %s\n", codigo, nombre);
				printf("Nombre: %s, contrasenya: %s \n ",nombre, contrasenya);
				
				
				pthread_mutex_lock(&mutex);
				Registrarse(nombre,contrasenya,conn,sock_conn,respuesta);
				pthread_mutex_unlock(&mutex);	
					
			}
			else if (codigo == 3) { 
    // Aquí usamos ObtenerJugadoresConocidos para listar jugadores con los que jugó el usuario
    p = strtok(NULL, "/");
    if (!p) {
		snprintf(respuesta, 200, "0/FormatoInvalido/Se esperaba: 3/usuario");
		write(sock_conn, respuesta, strlen(respuesta) + 1);
		return;;
    }
    char usuario[50];
    strncpy(usuario, p, sizeof(usuario) - 1);
    usuario[sizeof(usuario) - 1] = '\0';

	printf("[DEBUG] Usuario recibido: %s\n", usuario);
	memset(respuesta, 0, sizeof(respuesta));
	
	// 2. Llama a la función que consulta la base de datos
	ObtenerJugadoresConocidos(usuario, conn, respuesta);
	write(sock_conn, respuesta, strlen(respuesta) + 1);
	printf("[DEBUG] Respuesta enviada: %s\n", respuesta);
    
}
// Si el código recibido es 4, el cliente solicita resultados de partidas entre dos usuarios
else if (codigo == 4) {
    // Obtiene el primer token: nombre del usuario
    char *usuario = strtok(NULL, "/");
    // Obtiene el segundo token: nombre del adversario
    char *adversario = strtok(NULL, "/");
    
    // Comprueba que ambos nombres hayan sido recibidos correctamente
    if (!usuario || !adversario) {
        // Si falta alguno, envía mensaje de error indicando formato inválido
        snprintf(respuesta, 200, "0/FormatoInvalido/Se esperaba: 4/usuario/adversario");
        write(sock_conn, respuesta, strlen(respuesta) + 1);
        return; // Sale de la función ya que no se puede continuar
    }
    
    // Muestra en consola los nombres entre los que se buscan resultados
    printf("Buscando resultados entre %s y %s\n", usuario, adversario);
    
    // Bloquea el mutex para evitar conflictos al acceder a la base de datos compartida
    pthread_mutex_lock(&mutex);
    
    // Llama a la función que obtiene los resultados de partidas entre los dos usuarios
    ObtenerResultadosPartidas(usuario, adversario, conn, respuesta);
    
    // Desbloquea el mutex después de la consulta
    pthread_mutex_unlock(&mutex);
    
    // Envía la respuesta al cliente a través del socket
    write(sock_conn, respuesta, strlen(respuesta) + 1);
}

else if (codigo == 5) {
    // Obtener el siguiente token de la petición, esperado que sea el usuario
    p = strtok(NULL, "/");
    
    // Comprobar que se haya recibido el usuario
    if (!p) {
        // Si no hay usuario, enviar mensaje de error por formato inválido
        snprintf(respuesta, 200, "0/FormatoInvalido/");
        write(sock_conn, respuesta, strlen(respuesta) + 1);
        return; // Salir de la función
    }
    
    // Copiar el usuario recibido en un buffer seguro, evitando overflow
    char usuario[50];
    strncpy(usuario, p, sizeof(usuario) - 1);
    usuario[sizeof(usuario) - 1] = '\0'; // Asegurar terminación de cadena
    
    // Mostrar en consola el usuario para el que se buscan partidas por fecha
    printf("Buscando por fechas para: %s\n", usuario);
    
    // Bloquear mutex para sincronización al acceder a la base de datos
    pthread_mutex_lock(&mutex);
    
    // Llamar a la función que obtiene las partidas por fecha,
    // usando la cadena completa 'peticion' porque puede contener las fechas
    ObtenerPartidasPorFecha(peticion, conn, respuesta);
    
    // Desbloquear mutex tras finalizar la consulta
    pthread_mutex_unlock(&mutex);
    
    // Mostrar en consola la respuesta que se enviará al cliente
    printf("Enviando respuesta: %s\n", respuesta);
    
    // Enviar la respuesta al cliente
    write(sock_conn, respuesta, strlen(respuesta) + 1);
}

			else if(codigo == 6) { // Código para gestionar invitaciones a partidas
    // Obtener token siguiente: número de partida (ID)
    p = strtok(NULL, "/");
    int Partida = atoi(p); // Convertir a entero
    
    // Obtener siguiente token: número de invitados a la partida
    p = strtok(NULL, "/");
    int ninvitados = atoi(p);
    
    // Para cada invitado recibido en la petición
    for(int i = 0; i < ninvitados; i++) {
        // Obtener nombre del invitado
        p = strtok(NULL, "/");
        char invitar[20];
        strcpy(invitar, p);
        
        // Bloquear mutex para evitar condiciones de carrera al modificar datos compartidos
        pthread_mutex_lock(&mutex);
        
        // Llamar a la función que gestiona la invitación para ese jugador
        Invitacion(invitar, nombre, &milistaconectados, sock_conn, &Partida, partidas);
        
        // Desbloquear mutex
        pthread_mutex_unlock(&mutex);
        
        // Guardar el ID de la partida actual en mipartida (para el cliente)
        mipartida = Partida;
    }
    
    // Actualizar el número de invitados en la estructura de partidas
    partidas[Partida].numinvitados = ninvitados;
}
			else if(codigo == 7){ // Protocolo de respuesta del invitado
	p = strtok(NULL,"/"); // Extrae la siguiente parte del mensaje (la respuesta del jugador)
	int resposta = atoi(p); // Convierte esa respuesta a entero (1 = acepta, 2 = rechaza)

	if(resposta == 1){ // Si la respuesta es afirmativa
		p = strtok(NULL, "/"); // Extrae el número de la partida
		int partida = atoi(p); // Convierte el número de partida a entero
		mipartida = partida; // Se guarda en la variable local del hilo el número de partida al que entra el jugador

		pthread_mutex_lock(&mutex); // Se bloquea el acceso a recursos compartidos (como la lista de partidas)
		AceptarInvitacion(partidas, nombre, &milistaconectados, partida); // Se agrega al jugador a la partida
		pthread_mutex_unlock(&mutex); // Se libera el bloqueo
	}
	else if(resposta == 2){ // Si la respuesta es negativa (rechazo a la invitación)
		p = strtok(NULL, "/"); // Extrae el número de partida
		int partida = atoi(p); // Lo convierte a entero

		p = strtok(NULL,"/"); // Extrae el nombre del invitado que rechaza
		char invitado[20];
		strcpy(invitado, p); // Guarda el nombre del invitado

		p = strtok(NULL,"/"); // Extrae el nombre del anfitrión de la partida
		char anfitrion[20];
		strcpy(anfitrion, p); // Guarda el nombre del anfitrión

		sprintf(respuesta, "8/2/%s/", invitado); // Construye un mensaje para avisar del rechazo a los demás

		for(int i = 0; i < partidas[partida].numparticipantes; i++){
			write(partidas[partida].conectados[i].socket, respuesta, sizeof(respuesta)); // Se envía a todos los participantes
		}

		partidas[partida].numinvitados = partidas[partida].numinvitados - 1; // Se reduce el número de invitados pendientes

		if(partidas[partida].numinvitados == 0){ // Si ya han respondido todos los invitados
			char respuesta[500];
			strcpy(respuesta, "8/5/"); // Código 8/5 indica que la partida debe cancelarse por no aceptación
			write(partidas[partida].conectados[0].socket, respuesta, sizeof(respuesta)); // Se avisa al anfitrión

			pthread_mutex_lock(&mutex); // Se bloquea para modificar estructuras compartidas
			EliminarDePartida(partidas, anfitrion, partida); // Se elimina la partida o al jugador anfitrión de la misma
			pthread_mutex_unlock(&mutex); // Se desbloquea
		}
	}
}
			else if(codigo == 8){ // Si el código recibido es 8, se interpreta como un mensaje del chat global
			p = strtok(NULL,"/"); // Se extrae el siguiente campo del mensaje recibido (el nombre del remitente)
			char nombre[20]; // Se declara un array para guardar el nombre
			strcpy(nombre,p); // Se copia el nombre extraído al array

			p = strtok(NULL,"/"); // Se extrae el siguiente campo del mensaje (el contenido del mensaje del chat)
			char mensaje[500]; // Se declara un array para almacenar el mensaje
			strcpy(mensaje,p); // Se copia el mensaje al array

			pthread_mutex_lock(&mutex); // Se bloquea el acceso a recursos compartidos (protección por concurrencia)
			EnviarMensajeGlobal(&milistaconectados, nombre, mensaje); // Se llama a la función que reenvía el mensaje a todos los conectados
			pthread_mutex_unlock(&mutex); // Se desbloquea el recurso compartido
		}
			//else if(codigo==9){//Chat Local
				//p = strtok(NULL, "/");
/*				int partida = atoi(p);*/
				
/*				p = strtok(NULL,"/");*/
/*				char nombre[20];*/
/*				strcpy(nombre,p);*/
				
/*				p = strtok(NULL,"/");*/
/*				char mensaje[500];*/
/*				strcpy(mensaje,p);*/
				
/*				if(strcmp(mensaje,"T")==0){*/
/*					char resp[20];*/
/*					strcpy(resp,"11/1");*/
/*					write(partidas[partida].conectados[partidas[partida].turno].socket,resp,sizeof(resp));*/
/*				}*/
/*				else{*/
/*					pthread_mutex_lock(&mutex);*/
/*					EnviarMensajePartida(partidas,partida,nombre,mensaje);	*/
/*					pthread_mutex_unlock(&mutex);*/
				//}
				
			//}
			//Codigos para el juego
			else if(codigo == 10){ // Si el código recibido es 10, se inicia el juego
		p = strtok(NULL, "/"); // Se extrae el ID de partida del mensaje recibido
	int partida = atoi(p); // Se convierte a entero

	if(partidas[partida].numparticipantes == 4){ // Solo si hay 4 participantes se puede comenzar
		partidas[partida].CasLibres = 36; // Se inicializan 36 casillas libres
		partidas[partida].turno = 0; // El turno comienza en 0 (primer jugador)

		char mensaje[500];
		char colores[500];
		strcpy(colores, "Blue/Green/Red/Yellow"); // Colores asignados por orden a los jugadores
		char *p1 = strtok(colores, "/"); // Primer color

		for(int i = 0; i < partidas[partida].numparticipantes; i++){
			sprintf(mensaje, "10/%s/", p1); // Mensaje con el color para cada jugador
			write(partidas[partida].conectados[i].socket, mensaje, sizeof(mensaje)); // Envío al socket correspondiente
			p1 = strtok(NULL, "/"); // Siguiente color
		}

		char turno[20];
		strcpy(turno, "11/1/"); // Mensaje para indicar que empieza el turno
		write(partidas[partida].conectados[partidas[partida].turno].socket, turno, sizeof(turno)); // Lo recibe el primer jugador

		printf("%s\n", partidas[partida].conectados[partidas[partida].turno].nombre); // Debug: imprime el nombre del jugador que empieza
		printf("%d\n", &partidas[partida].conectados[partidas[partida].turno].socket); // Debug: imprime dirección del socket
	} else {
		char inicio[20];
		strcpy(inicio, "11/-1/"); // Si no hay 4 jugadores, no se puede comenzar
		write(sock_conn, inicio, sizeof(inicio)); // Se informa al cliente que no es posible iniciar
	}
}
			else if(codigo == 11){ // Código para colocar una ficha
				p = strtok(NULL,"/");
				int partida = atoi(p); // ID de partida

				p = strtok(NULL,"/");
				char ubicacion[20];
				strcpy(ubicacion, p); // Casilla donde se colocará la ficha

				p = strtok(NULL,"/");
				char color[20];
				strcpy(color, p); // Color del jugador que coloca la ficha

				if(partidas[partida].CasLibres > 1){ // Si quedan más de 1 casilla libre
					char mensaje[500];
					sprintf(mensaje, "11/%s/%s/", ubicacion, color); // Construye el mensaje de colocación

					for(int i = 0; i < partidas[partida].numparticipantes; i++)
						write(partidas[partida].conectados[i].socket, mensaje, strlen(mensaje)); // Envío a todos los jugadores

					partidas[partida].turno += 1; // Se avanza el turno
					if(partidas[partida].turno > 3) // Si se pasa de 3 (cuatro jugadores)
						partidas[partida].turno = 0;

					partidas[partida].CasLibres -= 1; // Se reduce el número de casillas libres

					char turno[20];
					strcpy(turno, "11/1/"); // Mensaje para indicar siguiente turno

					printf("turno de %d\n", partidas[partida].turno); 
					write(partidas[partida].conectados[partidas[partida].turno].socket, turno, sizeof(turno)); // Enviar turno al jugador correspondiente
					printf("exit\n");
				} else { // Si solo queda una casilla y nadie gana, hay empate
					char mensaje[500];
					strcpy(mensaje, "12/2/"); // Código de empate

					for(int i = 0; i < partidas[partida].numparticipantes; i++)
						write(partidas[partida].conectados[i].socket, mensaje, sizeof(mensaje)); // Notifica a todos
				}
			}
			else if(codigo==12){ // Si el código recibido es 12, significa que un jugador ha ganado la partida

			p = strtok(NULL,"/");         // Se extrae el ID de la partida del mensaje
			int partida = atoi(p);        // Se convierte a entero

			p = strtok(NULL,"/");         // Se extrae el nombre del jugador ganador
			char ganador[20];
			strcpy(ganador, p);           // Se guarda el nombre en la variable 'ganador'

			char mensaje[500];
			sprintf(mensaje, "12/1/%s/", ganador);  // Se construye el mensaje con el código 12/1 y el nombre del ganador

			// Se envía el mensaje de victoria a todos los jugadores conectados en esa partida
			for(int i = 0; i < partidas[partida].numparticipantes; i++)
				write(partidas[partida].conectados[i].socket, mensaje, sizeof(mensaje));
}
			else if (codigo == 13) { // Si el código es 13, se trata de eliminar un jugador de la base de datos

				p = strtok(NULL, "/");         // Se extrae el nombre del jugador
				char jugador[20];
				strcpy(jugador, p);           // Se copia el nombre en la variable 'jugador'
				
				EliminarJugadorDeBD(conn, jugador); // Se llama a la función que elimina el jugador de la base de datos MySQL

				// Enviar respuesta de confirmación al cliente
				char mensaje[100];
				sprintf(mensaje, "13/1/%s/", jugador); // Mensaje indicando que el jugador fue eliminado
				write(sock_conn, mensaje, strlen(mensaje)); // Se envía al cliente solicitante
			}

			printf("--------------------------------------------------------------------\n");
	} // Se cierra el socket del cliente que ha sido atendido
	
	close(sock_conn);
}	// AtenderCliente




int main(int argc, char *argv[])
{
    int sock_conn, sock_listen, ret;  // Sockets para conexión individual y escucha; ret para posibles retornos
    int puerto = 50051;  // Puerto en el que el servidor escuchará
    struct sockaddr_in serv_adr;  // Estructura para la dirección del servidor
    char respuesta[512];  // Buffer para posibles respuestas (no usado en este fragmento)
    char peticion[512];  // Buffer para posibles peticiones (no usado en este fragmento)
    milista.num = 0;  // Inicializa el número de usuarios conectados (estructura global)

    // INICIALIZACIONES
    // Creamos el socket de escucha
    if ((sock_listen = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("Error creant socket");  // Mensaje de error si falla la creación del socket
    }

    // Configuramos la estructura de dirección del servidor
    memset(&serv_adr, 0, sizeof(serv_adr));  // Inicializa a cero toda la estructura
    serv_adr.sin_family = AF_INET;  // Familia de direcciones: IPv4
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);  // Acepta conexiones desde cualquier IP local
    serv_adr.sin_port = htons(puerto);  // Establece el puerto de escucha, en formato de red

    // Asociamos el socket con la dirección IP y puerto configurados
    if (bind(sock_listen, (struct sockaddr*)&serv_adr, sizeof(serv_adr)) < 0)
    {
        printf("Error al bind");  // Mensaje de error si falla el bind
    }

    // Indicamos al sistema que queremos escuchar conexiones (máximo 3 en cola)
    if (listen(sock_listen, 3) < 0)
    {
        printf("Error en el Listen");  // Error si falla listen
    }

    contador = 0;  // Contador de conexiones (global)
    int i;  // Índice para los arrays de sockets e hilos
    i = 0;
    int sockets[100];  // Array para almacenar hasta 100 sockets de clientes
    pthread_t thread[100];  // Array para almacenar hasta 100 hilos

    // Bucle infinito para aceptar conexiones de clientes
    for (;;)
    {
        printf("Escuchando\n");  // Mensaje indicando que el servidor está esperando conexiones

        sock_conn = accept(sock_listen, NULL, NULL);  // Espera y acepta una nueva conexión entrante
        printf("He recibido conexion\n");  // Se ha aceptado una conexión

        sockets[i] = sock_conn;  // Guarda el socket del cliente en el array

        // Crea un hilo nuevo para atender al cliente, pasándole su socket
        pthread_create(&thread[i], NULL, AtenderCliente, &sockets[i]);

        i = i + 1;  // Avanza al siguiente índice para futuras conexiones
    }
}