#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <ctype.h>
#include <mysql.h>
#include <pthread.h>

typedef struct{
	char nombre[20];
	int socket;
} Conectado;

typedef struct{
	Conectado conectados[100];
	int num;
} ListaConectados;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int contador;

ListaConectados milista;

void GiveConnected(char conectados[300])
{
	sprintf(conectados,"Online Players - %d",milista.num);
	int i;
	for(i=0;i < milista.num;i++)
	{
		sprintf(conectados,"%s/Player %d - %s",conectados,i+1,milista.conectados[i].nombre);
	}
}
int Delete(char nombre[20])
{
	int pos = GivePos(nombre);
	if(pos == -1)
	{
		return -1;
	}
	else
	{
		int i;
		for(i=pos; i < milista.num; i++)
		{
			milista.conectados[i] = milista.conectados[i+1];
		}
		milista.num--;
		printf("%d\n",milista.num);
		return 0;
	}
}
int GivePos (char nombre[20])
{
	for (int i = 0; i < milista.num; i++)
	{
		if (strcmp(milista.conectados[i].nombre, nombre) == 0)
			return i;
	}
	return -1;
}


int Pon (char nombre[20], int socket)
{
	if(milista.num == 100)
	{
		return -1;
	}
	else
	{
		strcpy(milista.conectados[milista.num].nombre, nombre);
		milista.conectados[milista.num].socket = socket;
		milista.num++;
		return 0;
	}
}
void *AtenderCliente (void *socket)
{
	
	int sock_conn;
	int *s;
	s= (int *) socket;
	sock_conn= *s;
	
	//int socket_conn = * (int *) socket;
	
	char peticion[512];
	char respuesta[512];
	int ret;
	
	int terminar =0;
	// Entramos en un bucle para atender todas las peticiones de este cliente
	//hasta que se desconecte
	while (terminar ==0)
	{
		// Ahora recibimos la peticion
		ret=read(sock_conn,peticion, sizeof(peticion));
		printf ("Recibido\n");
		printf("Bytes recibidos: %d\n", ret);
		
		// Tenemos que añadirle la marca de fin de string 
		// para que no escriba lo que hay despues en el buffer
		peticion[ret]='\0';
		
		printf ("Peticion: %s\n",peticion);
		
		// vamos a ver que quieren
		char *p = strtok( peticion, "/");
		int codigo =  atoi (p);
		// Ya tenemos el codigo de la peticion	
		
		
		char nick[25];
		char pass[10];
		char respt[100];
		char conectados[300];
		// variables temporales para guardar datos
		
		if (codigo ==0) //peticion de desconexion
		{	
			terminar=1;
		}
		else if (codigo ==1)
		{
			//funcion de registro de player
			p = strtok( NULL, "/");
			strcpy (nick, p);
			Consulta(respt,nick, codigo);
			//consulta si el nickname proporcionado esta en uso o no
			if(respt != NULL && respt[0] != '\0')
			{
				//si no esta en uso crea uno de nuevo junto a la password proporcionada
				p = strtok( NULL, "/");
				strcpy (pass, p);
				Add(nick,pass,respt);
				strcpy(respuesta,"1");
			}
			else
			{
				//si esta en uso notifica con un 2 para informarle al usuario
				strcpy(respuesta, "2");
			}
		}
		else if (codigo ==2)
		{
			//funcion para iniciar sesion en el juego
			p = strtok( NULL, "/");
			strcpy (nick, p);
			Consulta(respt, nick, codigo);
			//consulta que el nickname se encuentre en la base de datos y devuelve la contraseï¿±a
			if(respt == NULL || respt[0] == '\0')
			{
				//nickname no encontrado en la base
				strcpy(respuesta, "2");
			}
			else
			{
				//en caso de que este en la base de datos
				p = strtok( NULL, "/");
				strcpy (pass, p);
				if(strcmp(pass,respt)==0)
				{
					//devuelve 1 si la password esta asociada a ese nickname
					strcpy(respuesta, "1");
					Pon(nick,sock_conn);
				}
				else
				{
					//devuelve 3 si la password no es la asociada al nickname
					strcpy(respuesta, "3");
				}
			}
		}
		else if (codigo ==3)
		{
			//funcion de consulta
			p = strtok( NULL, "/");
			strcpy (nick, p);
			Consulta(respt, nick, codigo);
			//recoge todos los datos del usuario
			if(respt != NULL || respt[0] != '\0')
			{
				//envia al cliente los datos asociados a ese nickname
				strcpy(respuesta,respt);
			}
			else
			{
				//envia un 2 si no se ha encontrado al usuario en la base de datos
				strcpy(respuesta, "2");
			}
		}	
		else if (codigo == 4)
		{
			p = strtok( NULL, "/");
			strcpy (nick, p);
			printf("%s:\n",nick);
			Delete(nick);
			strcpy(respuesta, "");
		}
		else if (codigo == 5)
		{
			GiveConnected(conectados);
			printf("%d\n",milista.num);
			for(int o = 0; o < milista.num;o++)
			{
				printf("1 - %d\n",milista.conectados[o].socket);
			}
			printf("Resultado: %s\n",conectados);
			if(milista.num == 0)
			{
				strcpy(respuesta, "1");
			}
			else
			{
				strcpy(respuesta, conectados);
			}
		}
		if (codigo !=0)
		{
			
			printf ("Respuesta: %s\n", respuesta);
			// Enviamos respuesta
			write (sock_conn,respuesta, strlen(respuesta));
		}
		if ((codigo ==1)||(codigo==2)|| (codigo==3)|| (codigo==4)||(codigo==5))
		{
			pthread_mutex_lock( &mutex ); //No me interrumpas ahora
			contador = contador +1;
			pthread_mutex_unlock( &mutex); //ya puedes interrumpirme
		}
		// Se acabo el servicio para este cliente
	}
	close(sock_conn);
}

void Consulta(char* respt, char nick[25], int tipo)
{
	MYSQL *conn;
	int err;
	// Estructura especial para almacenar resultados de consultas
	MYSQL_RES *resultado;
	MYSQL_ROW row;
	
	char ID_jugador[10];
	char Nickname[25];
	char Password[10];
	
	char consulta [80];
	
	
	//Creamos una conexion al servidor MYSQL
	conn = mysql_init(NULL);
	if (conn==NULL) {
		printf ("Error al crear la conexi\ufff3n: %u %s\n", mysql_errno(conn), mysql_error(conn));
		exit (1);
	}
	//inicializar la conexion
	conn = mysql_real_connect (conn, "shiva2.upc.es", "root", "mysql", "T1_DDBBjuego", 0, NULL, 0);
	if (conn==NULL) 
	{
		printf ("Error al inicializar la conexion: %u %s\n",mysql_errno(conn), mysql_error(conn));
		exit (1);
	}
	strcpy(Nickname,nick);
	if(tipo==1)
	{
		//cuando se hace la funcion de consulta llamada desde la funcion de registrarse del cliente
		strcpy (consulta,"SELECT Nickname FROM DB_players WHERE Nickname = '");
		strcat (consulta, Nickname);
		strcat (consulta,"'");
	}
	else if(tipo==2)
	{
		//cuando se hace la funcion de consulta llamada desde la funcion de log in del cliente
		strcpy (consulta,"SELECT Nickname, Password FROM DB_players WHERE Nickname = '");
		strcat (consulta, Nickname);
		strcat (consulta,"'");
	}
	else if(tipo==3)
	{
		//cuando se hace la funcion de consulta llamada desde la funcion de log in del cliente
		strcpy (consulta,"SELECT * FROM DB_players WHERE Nickname = '");
		strcat (consulta, Nickname);
		strcat (consulta,"'");
	}
	
	err=mysql_query (conn, consulta);
	
	if (err!=0) 
	{
		printf ("Error al consultar datos de la base %u %s\n",mysql_errno(conn), mysql_error(conn));
		exit (1);
	}
	//recogemos el resultado de la consulta. El resultado de la
	//consulta se devuelve en una variable del tipo puntero a
	//MYSQL_RES tal y como hemos declarado anteriormente.
	//Se trata de una tabla virtual en memoria que es la copia
	//de la tabla real en disco.
	resultado = mysql_store_result (conn);
	// El resultado es una estructura matricial en memoria
	// en la que cada fila contiene los datos de una persona.
	// Ahora obtenemos la primera fila que se almacena en una
	// variable de tipo MYSQL_ROW
	row = mysql_fetch_row (resultado);
	
	if (row == NULL)
	{
		if(tipo==1)
		{
			//notifica a la funcion de registrarse de que no hay ningun usuario con el nickname dado y se proporciona el numero de jugadores
			//dentro de la base de datis para poder asignar la ID de player
			mysql_query(conn,"SELECT COUNT(*) AS total FROM DB_players");
			resultado = mysql_store_result (conn);
			row = mysql_fetch_row (resultado);
			sprintf(respt,"%d",atoi(row[0]));
		}
		else if(tipo==2 || tipo==3)
		{
			//notifica tanto a la funcion de log in y consultar de que el nickname dado no se encuentra en la base de datos
			respt[0] = NULL;
		}
	}
	else
	{
		while (row !=NULL) 
		{
			if(tipo==1)
			{
				//notifica a la funcion de registrarse de que el nickname dado esta siendo usado por otro player
				respt[0] = NULL;
			}
			else if(tipo==2)
			{
				//notifica a la funcion de log in de que el nickname dado se encuentra en la base de datos y proporciona la Password
				//asociada a ese nickname
				strcpy(respt,row[1]);
			}
			if(tipo==3)
			{
				//notifica a la funcion de consulta de que el nickname dado se encuentra en la base de datos y devuelve todos los datos
				//de ese player
				printf("Valor de row[4]: %s\n", row[4]);  // Para verificar el valor de la cadena
				
				sprintf(respt,"ID PLyaer: %s/Nickname: %s/Password: %s/Total Score: %f/Last Log: %s",row[0],row[1],row[2],row[3],row[4]);
			}
			row = mysql_fetch_row (resultado);
		}
	}
	mysql_close (conn);
	return;
	exit(0);
}

int Add(char nick[25], char pass[10], char* respt)
{
	MYSQL *conn;
	int err;
	
	char ID_jugador[10];
	
	char Nickname [25];
	
	char Password[10];
	
	float Total_Score;
	char Total_Scores[3];
	
	int Last_Log;
	char Last_Logs[3];
	
	char consulta [80];
	
	//Creamos una conexion al servidor MYSQL
	conn = mysql_init(NULL);
	
	if (conn==NULL) 
	{
		printf ("Error al crear la conexion: %u %s\n",	mysql_errno(conn), mysql_error(conn));
		exit (1);
	}
	//inicializar la conexi\uffc3\uffb3n, entrando nuestras claves de acceso y
	//el nombre de la base de datos a la que queremos acceder
	conn = mysql_real_connect (conn, "shiva2.upc.es","root", "mysql", "T1_BBDDJuego",0, NULL, 0);
	if (conn==NULL) 
	{
		printf ("Error al inicializar la conexion: %u %s\n",mysql_errno(conn), mysql_error(conn));
		exit (1);
	}
	
	strcpy(ID_jugador,respt);
	strcpy(Nickname,nick);
	strcpy(Password,pass);
	//copia en las variables temporales los datos proporcionados a la funcion Add()
	
	snprintf(consulta, sizeof(consulta),"INSERT INTO DB_players VALUES ('%s', '%s', '%s', %f, %d);",ID_jugador, Nickname, Password, Total_Score, Last_Log);
	//registra unn nuevo player
	printf("consulta = %s\n", consulta);
	// Ahora ya podemos realizar la insercion
	err = mysql_query(conn, consulta);
	if (err!=0) 
	{
		printf ("Error al introducir datos la base %u %s\n", mysql_errno(conn), mysql_error(conn));
		exit (1);
	}
	// cerrar la conexion con el servidor MYSQL
	mysql_close (conn);
}
int main(int argc, char *argv[])
{
	
	int sock_conn, sock_listen, ret;
	int puerto = 50051;
	struct sockaddr_in serv_adr;
	char respuesta[512];
	char peticion[512];
	milista.num=0;
	// INICIALITZACIONS
	// Obrim el socket
	if ((sock_listen = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("Error creant socket");
	}
	// Fem el bind al port
	
	memset(&serv_adr, 0, sizeof(serv_adr));// inicialitza a zero serv_addr
	serv_adr.sin_family = AF_INET;
	
	// asocia el socket a cualquiera de las IP de la m?quina. 
	//htonl formatea el numero que recibe al formato necesario
	serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
	// establecemos el puerto de escucha
	serv_adr.sin_port = htons(puerto);
	
	if (bind(sock_listen, (struct sockaddr *) &serv_adr, sizeof(serv_adr)) < 0)
	{
		printf ("Error al bind");
	}
	
	if (listen(sock_listen, 3) < 0)
	{
		printf("Error en el Listen");
	}
	
	contador =0;
	int i;
	i=0;
	int sockets[100];
	pthread_t thread[100];
	for (;;)
	{
		printf ("Escuchando\n");
		
		sock_conn = accept(sock_listen, NULL, NULL);
		printf ("He recibido conexion\n");
		//sock_conn es el socket que usaremos para este cliente
		sockets[i] =sock_conn;
		pthread_create (&thread[i], NULL, AtenderCliente,&sockets[i]);
		i=i+1;}
}


