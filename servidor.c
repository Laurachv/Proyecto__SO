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
int puerto = 9050;

//************************************************************************************************
//Funciones del servidor

//-------------------------------------------------------------------------------------------------
//Funciones principales para ejecutar el codigo
//LogIn
void Loguearse(char nombre[20],char contrasenya[20],MYSQL *conn, int *sock_conn, ListaConectados *milistaconectados,char respuesta[500]){
	//Loguea a el jugador, envia "1/2" al cliente si todo ha salido bien, envia "1/1" si no esta registrado 
	int registrado = BuscarID(nombre,contrasenya,conn);
	
	if (registrado==-1)
		strcpy (respuesta,"1/1/\n");
	else{
		strcpy(respuesta,"1/2/\n");
		AnadirConectado(nombre, sock_conn,milistaconectados);
	}
	printf("Correcto %s\n",respuesta);
	write (sock_conn,respuesta, strlen(respuesta));
}


void ConsultarID(char *usuario, MYSQL *conn, char *respuesta) { //mira que no edvuelva la consulta sino solo la id
	char query[100];
	sprintf(query, "SELECT idj FROM jugadores WHERE usuario = '%s'", usuario);
	memset(respuesta, 0, 100);
	
	if (mysql_query(conn, query)) {
		fprintf(stderr, "%s\n", mysql_error(conn));
		strcpy(respuesta, "Error en la consulta");
		return;
	}
	MYSQL_RES *result = mysql_store_result(conn);
	if (result == NULL) {
		fprintf(stderr, "%s\n", mysql_error(conn));
		strcpy(respuesta, "Error al obtener resultados");
		return;
	}
	MYSQL_ROW row = mysql_fetch_row(result);
	if (row) {
		sprintf(respuesta, "3/%s/", row[0]); 
		
	} else {
		strcpy(respuesta, "Usuario no encontrado");
	}
	
	mysql_free_result(result);
}


int BuscarID(char nombre[20],char contrasenya[20],MYSQL *conn){
	//Busca la ID de un jugador por su nombre y retorna 0 al encontrarlo, retorna -1 si no lo encuentra
	int err=0;
	char str_query[512];
	MYSQL_RES *resultado;
	MYSQL_ROW row;
	
	sprintf(str_query, "SELECT jugadores.idj,jugadores.usuario FROM jugadores WHERE usuario= '%s' AND contrasenya='%s';", nombre,contrasenya);
	err=mysql_query (conn, str_query);
	printf("1\n");
	
	if (err!=0) {
		printf ("Error al consultar datos de la base %u %s\n",
				mysql_errno(conn), mysql_error(conn));
		exit (1);
	}
	
	resultado = mysql_store_result (conn);
	row =  mysql_fetch_row (resultado);
	if (row == NULL)
		return -1;
	else{
		return 0;
	}
}


void ObtenerPartidasJugadas(char *usuario, MYSQL *conn, char *respuesta) {
	char query[512];
	snprintf(query, sizeof(query),
			 "SELECT COUNT(*) FROM participacion p "
			 "JOIN jugadores j ON p.idJugadores = j.idj "
			 "WHERE j.usuario = '%s'", usuario);
	
	memset(respuesta, 0, 100);
	
	if (mysql_query(conn, query)) {
		snprintf(respuesta, 100, "0/ErrorDB/%s", mysql_error(conn));
	return;
}
	
	MYSQL_RES *result = mysql_store_result(conn);
	if (!result) {
		snprintf(respuesta, 100, "0/ErrorResult/");
		return;
	}
	MYSQL_ROW row = mysql_fetch_row(result);
	snprintf(respuesta, 100, "4/%s/", row ? row[0] : "0");
	mysql_free_result(result);
}

void obtener_partida_mas_corta(const char *usuario, MYSQL *conn, char *respuesta) {
	char query[512];
	// Consulta optimizada que convierte duraciÃ³n a nÃºmero para comparaciÃ³n
	snprintf(query, sizeof(query),
			 "SELECT p.idp, p.duracion, p.fecha, p.ganador "
			 "FROM partidas p "
			 "JOIN participacion part ON p.idp = part.idPart "
			 "JOIN jugadores j ON part.idJugadores = j.idj "
			 "WHERE j.usuario = '%s' "
			 "ORDER BY CAST(p.duracion AS UNSIGNED) ASC "
			 "LIMIT 1", usuario);
	
	memset(respuesta, 0, 200);
	
	if (mysql_query(conn, query)) {
		snprintf(respuesta, 200, "0/ErrorDB/%s", mysql_error(conn));
		return;
	}
	MYSQL_RES *result = mysql_store_result(conn);
	if (!result) {
		snprintf(respuesta, 200, "0/ErrorResult/");
		return;
	}
	MYSQL_ROW row = mysql_fetch_row(result);
	if (row) {
		// Formato: 5/[id_partida]/[duracion]/[fecha]/[ganador]/
		snprintf(respuesta, 200, "5/%s/%s/%s/%s/", 
				 row[0], row[1], row[2], row[3], row[4]);
		
	} else {
		snprintf(respuesta, 200, "5/0/Sin partidas//");
	}
	mysql_free_result(result);
}
		
		
	
	int AnadirConectado(char nom[20], int socket, ListaConectados *l){
	//Retorna -1 en caso de que la lista de conectados este llena, retorna 0 si todo está correcto
	if(l->num == 100){
		return -1;
	}
	else{
		printf("Entro\n");
		//Buscar el nombre por el socket 
		int posConectado = DamePosicion(socket,l);
		strcpy(l->conectados[posConectado].nombre, nom);
		printf("Salgo\n");
		return 0;
	}
}

void EliminarConectado(int *socket, ListaConectados *l){
	int p = DamePosicion(socket, l);
	for(int i = p; i< l->num; i++){
		l->conectados[i] = l->conectados[i+1];
	}
	l->num = (l->num)-1;
}

//Registrarse
void Registrarse(char nombre[20],char contrasenya[20],MYSQL *conn, int *sock_conn,char respuesta[500]){
	char str_query[500];
	int err;
	MYSQL_RES *resultado;
	MYSQL_ROW row;
	
	int reg = BuscaRegistrados(nombre,conn);
	
	if(reg==-1){
		printf("El usuario se puede conectar ");
		strcpy(respuesta,"2/1/\n");
		write (sock_conn,respuesta, strlen(respuesta));
	}			
	else{
		strcpy(respuesta,"2/2/\n");
		int id=0;
		strcpy(str_query,"SELECT MAX(idj) FROM jugadores;");
		err=mysql_query (conn, str_query);
		if (err!=0)
		{
			printf ("Error al consultar datos de la base para la id: %u %s \n", mysql_errno(conn), mysql_error(conn));
		}
		
		resultado = mysql_store_result (conn);
		row = mysql_fetch_row (resultado);
		if(row!=NULL){
			id = atoi(row[0]);
			id = id+1;
		}
		
		sprintf(str_query, "INSERT INTO jugadores VALUES ('%d','%s', '%s',%d);",id, nombre,contrasenya,0);
		err=mysql_query (conn, str_query);
		if (err!=0)
		{
			printf ("Error al consultar datos de la base %u %s \n",
					mysql_errno(conn), mysql_error(conn));
			
		}
		write (sock_conn,respuesta, strlen(respuesta));
		printf("salgo del register\n");
	}
}

int BuscaRegistrados(char nombre[20],MYSQL *conn){
	//Busca si alguien ya está registrado en la base de datos(evita dobles registros), 
	//retorna 0 si no hay nadie registrado con ese nombre, retorna -1 en caso contratrio
	char str_query[500];
	int err;
	MYSQL_RES *resultado;
	MYSQL_ROW row;
	sprintf(str_query,"SELECT jugadores.usuario FROM jugadores WHERE jugadores.usuario = '%s'",nombre);
	err=mysql_query(conn, str_query);
	if (err!=0)
	{
		printf ("Error al consultar datos de la base para la id: %u %s \n", mysql_errno(conn), mysql_error(conn));
	}
	
	resultado = mysql_store_result (conn);
	row =mysql_fetch_row(resultado);
	
	if(row !=NULL){
		return -1;
	}			
	else{
		return 0;
	}
}

		

//Lista conectados
void DameConectados(char VectorConectados[100], ListaConectados *l){
	//retorna la lista de conectados en formato J1/J2/J3/......
	//sprintf(respuesta,"%d/",l->num);
	VectorConectados[0]='\0';
	strcpy(VectorConectados,"6/");
	if(l->num!=-1){
		for(int i =0; i!=l->num; i++){
			sprintf(VectorConectados,"%s%s/",VectorConectados,l->conectados[i].nombre);
		}
		printf("%s\n", VectorConectados);
	}
}

void EnviarListaConectados(char LConectados[500],ListaConectados *l){//Envia la lista de conectados a todos los sockets conectados
	for(int i = 0; i!=l->num;i++){
		write(l->conectados[i].socket,LConectados,sizeof(LConectados));
		printf("Envio a %s, con socket, %d y le mando %s\n",l->conectados[i].nombre,l->conectados[i].socket,LConectados);
	}
}

//Metodo de Invitacion
void Invitacion(char nombre[20],char anfitrion[20],ListaConectados *l, int *socket, int *EnPartida,Partida partidas[500]){ 
	//Envia una invitacion de partida al usuario recibido por parametro, retorna como un entero
	//la posicion de la partida EnPartida, si inicialmente era -1 entonces retorna la posicion libre
	char respuesta[500],respuesta2[500];
	
	if(*EnPartida==-1){
		int libre = PartidaLibre(partidas);
		if(libre !=-1){
			int n = AnadirAPartida(partidas,anfitrion,libre,l);
			sprintf(respuesta,"7/%d/%s/",libre,anfitrion);
			sprintf(respuesta2,"8/4/%d/",libre);
			numPartidas+=1;
			*EnPartida = libre;
			write(socket,respuesta2,sizeof(respuesta2));
		}else{
			sprintf(respuesta2,"8/6/",libre);
			write(socket,respuesta2,sizeof(respuesta2));
		}
		
	}else{
		sprintf(respuesta,"7/%d/%s/",*EnPartida,anfitrion);
	}
	int socketInv= DameSocket(l,nombre);
	write(socketInv,respuesta,strlen(respuesta));
}


//Metodo de AceptarInvitacion
void AceptarInvitacion(Partida partidas[500],char invitado[20],ListaConectados *l,int numPartida){
	//Metodo para aceptar invitaciones, en caso de aceptar envia la lista del grupo a todos los miembros "8/1/Miembro1/Miembro2/Miembro3/...."
	//En caso que el jugador esté dentro de algun grupo envia "8/3"y envia un "8/4" si el grupo está lleno.
	int accept = AnadirAPartida(partidas,invitado,numPartida,l);
	char respuesta[500];
	if(accept==-1){
		strcpy(respuesta,"8/3/");
		for(int i=0;i<partidas[numPartida].numparticipantes;i++){
			write(partidas[numPartida].conectados[i].socket,respuesta,sizeof(respuesta));
		}
	}else if(accept==0){
		sprintf(respuesta,"8/1/%s/",invitado);
		for(int i=0;i<partidas[numPartida].numparticipantes;i++){
			write(partidas[numPartida].conectados[i].socket,respuesta,sizeof(respuesta));
		}
	}
}

int AnadirAPartida(Partida partidas[500],char nombre[20],int numPartida,ListaConectados *lc){ 
	//Añade a la partida en posicion pasada por parametro, a la persona pasada por parametro
	//si todo sale bien retorna 0, en caso que la partida esté llena retorna -1.
	if(partidas[numPartida].numparticipantes<=4){//Ha petado aquí en la segunda aceptacion <-----------------------------------------------------------------
		partidas[numPartida].conectados[partidas[numPartida].numparticipantes].socket = DameSocket(lc,nombre);
		strcpy(partidas[numPartida].conectados[partidas[numPartida].numparticipantes].nombre,nombre);
		partidas[numPartida].numparticipantes+=1;
		return 0;
	}else{
		return -1;
	}
	
}

int PartidaLibre(Partida partidas[500]){
	//Busca en un vector de partidas la primera posicion libre, retorna la posicion o -1 si está llena
	int libre=-1,encontrado=0;
	int i=0;
	while((i<500) && (encontrado==0)){
		if(partidas[i].numparticipantes==0){
			libre = i;
			encontrado=1;
		}
		if(!encontrado)
		   i++;
	}
	return libre;
}

int BuscarEnPartidas(Partida partidas[500],char nombre[20],int *libre, int Posiciones[500]){
	//Busca a la persona con nombre X y busca si está dentro de alguna partida, retorna un vector de partidas en las que participa 
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

void EliminarDePartida(Partida partidas[500],char nombre[20],int numPartida){
	//Quita a la persona del grupo al que pertenece, retorna -1 si la persona no esta en algun grupo, 
	//retorna 0 si todo ha salido bien, en caso que el grupo esté vacio lo elimina, si no solo elimina a la persona
	int i=0,encontrado=0;
	if(numPartida!=-1){
		while( (i<partidas[numPartida].numparticipantes) && (encontrado ==0)){
			if(strcmp(partidas[numPartida].conectados[i].nombre,nombre)==0)
				encontrado = 1;
			if(encontrado!=1)
				i++;
		}
		if(encontrado == 1){
			while((i<partidas[numPartida].numparticipantes)){
				if(partidas[numPartida].numparticipantes!=1){
					if(partidas[numPartida].numparticipantes!=4)
						partidas[numPartida].conectados[i]=partidas[numPartida].conectados[i+1];
					partidas[numPartida].numparticipantes = partidas[numPartida].numparticipantes-1;
					i++;
				}else if(partidas[numPartida].numparticipantes==1){
					partidas[numPartida].numparticipantes = partidas[numPartida].numparticipantes-1;
					numPartidas = numPartidas-1;
					break;
				}
			}
		}
	}
}

//Chat global
void EnviarMensajeGlobal(ListaConectados *l, char nombre[20],char mensaje[500]){
	char respuesta[800];
	sprintf(respuesta,"9/1/%s/%s/",nombre,mensaje);
	printf("%s",respuesta);
	for(int i=0;i<l->num;i++){
		write(l->conectados[i].socket,respuesta,sizeof(respuesta));
	}
}

//Chat local(chat de partidas)
/*void EnviarMensajePartida(Partida partidas[500],int partida, char nombre[20],char mensaje[500]){*/
/*	char respuesta[800];*/
/*	sprintf(respuesta,"9/2/%s/%s/",nombre,mensaje);*/
/*	printf("%s",respuesta);*/
/*	for(int i=0;i<partidas[partida].numparticipantes;i++){*/
/*		write(partidas[partida].conectados[i].socket,respuesta,sizeof(respuesta));*/
/*	}*/
/*}*/

//----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------


int DameSocket(ListaConectados *l, char nom[20]){
	//retorna -1 si no se le encuentra, retorna el socket en caso que todo salga correctamente
	int i =0;
	int encontrado =0;
	while((!encontrado) && (i != l->num)){
		if(strcmp(l->conectados[i].nombre,nom)==0)
			encontrado = 1;
		else
			i++;
	}
	if(!encontrado){
		return -1;
	}
	else if(encontrado){
		int n = l->conectados[i].socket;
		return n;
	}
}	

int DamePosicion(int *socket, ListaConectados *l){//retorna la posicion de un usuario mediante su socket, retorna -1 en caso de no encontrarse
	int i =0;
	int encontrado =0;
	while((!encontrado) && (i != l-> num)){
		if(l->conectados[i].socket==socket)
			encontrado = 1;
		else
			i++;
	}
	if(!encontrado)
		return -1;
	if(encontrado)
		return i;
}

int Conected(char nom[20], ListaConectados *l){//Mira si un usuario está conectado y retorna 0, en caso contratio retorna -1
	int i=0, encontrado = 0;
	while((i<l->num) && (encontrado!=1)){
		if(strcmp(nom,l->conectados[i].nombre)==0)
			encontrado=1;
	}
	if(encontrado==1)
		return 0; //Esta conectado
	if(encontrado ==0)
		return -1; //no esta conectado
}
//Juego
int DameTurno(char color[20]){//Obtiene el turno a partir del color, tambien sirve como conversor de String a Numero
	if(strcmp(color,"Blue")==0)
		return 0;
	else if(strcmp(color,"Green")==0)
		return 1;
	else if(strcmp(color,"Red")==0)
		return 2;
	else if(strcmp(color,"Yellow")==0)
		return 3;
}

void DameColor(char color[20],int turno){//Obtiene el color a partir del turno, tambien sirve como conversor de Numero a String
	if(turno==0)
		strcpy(color,"Blue");
	else if(turno==1)
		strcpy(color,"Green");
	else if(turno==2)
		strcpy(color,"Red");
	else if(turno==3)
		strcpy(color,"Yellow");
}

//**************************************************************************************************
//Atender Clientes
void *AtenderCliente(void * socket)
{
	printf("te doy el socket %d",socket);
	int sock_conn = *((int *) socket);
	char nombre[20];
	char Usuario[20];
	char peticion[500];
	char respuesta[500];
	char LConectados[500];
	int ret, numinvitados=0;
	char logged[20];
	int terminar =0;
	int mipartida = -1;
	MYSQL *conn;
	int err;
	
	MYSQL_RES *resultado;
	MYSQL_ROW row;
	
	char consulta [80];
	conn = mysql_init(NULL);
	char str_query[512];
	//char str_query1[512];
	int partidas_ganadas;
	
	conn=mysql_init(NULL);
	if (conn==NULL)
	{
		printf (" crear la conexion: %u %s\n", mysql_errno(conn), mysql_error(conn));
		exit (1);
	}
	//inicializar la conexion
	conn = mysql_real_connect (conn, "localhost","root", "mysql", "BBDDJuego",0, NULL, 0);
	if (conn==NULL)
	{
		printf ("Error al inicializar la conexion: %u %s\n",
				mysql_errno(conn), mysql_error(conn));
		exit (1);
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
			
			char respt[100];
			
			// Ya tenemos el codigo de la peticion
			printf ("Codigo: %d\n", codigo);
			printf("Mensaje enviado por socket: %d\n",sock_conn);
			
			if (codigo ==0) {
				terminar=1;
				char *p = strtok( peticion, "/");
				int partida =  atoi (p);
				
			
				if(Usuario[0]!='\0'){
					pthread_mutex_lock(&mutex);
					EliminarDePartida(partidas,nombre,partida);
					EliminarConectado(sock_conn, &milistaconectados);
					pthread_mutex_unlock(&mutex);
					
					if(milistaconectados.num!=0){
						char LConectados[100];
						DameConectados(LConectados, &milistaconectados);
						
						for(int i = 0; i<milistaconectados.num;i++){
							write(milistaconectados.conectados[i].socket,LConectados,sizeof(LConectados));
						}
					}
					
				}
				
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
				
				
				char LConectados[100];				
				DameConectados(LConectados, &milistaconectados);
				printf("%s\n",LConectados);
				
				
				for(int i = 0; i<milistaconectados.num;i++){
					write(milistaconectados.conectados[i].socket,LConectados,sizeof(LConectados));
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
			else if (codigo ==3){ 
			
				p = strtok(NULL, "/");
				char usuario[20];
				strcpy(usuario, p);
				
				printf("Consultando ID para: %s\n", usuario);
				
				char respuesta[100];
				pthread_mutex_lock(&mutex);
				ConsultarID(usuario, conn, respuesta);
				pthread_mutex_unlock(&mutex);
				
				printf("Enviando respuesta: %s\n", respuesta);
				
				// Asegurar envÃ­o completo con terminador nulo
				write(sock_conn, respuesta, strlen(respuesta) + 1);
			}
				
			
				
			else if (codigo ==4){ // consulta 2: dame la duracion de la partida mas larga que jugo x'
				p = strtok(NULL, "/");
				if (!p) {
					snprintf(respuesta, 100, "0/FormatoInvalido/");
					write(sock_conn, respuesta, strlen(respuesta)+1);
					return;
				}
				char usuario[50];
				strncpy(usuario, p, sizeof(usuario)-1);
				usuario[sizeof(usuario)-1] = '\0';
				
				printf("Buscando partidas para: %s\n", usuario); // Debug
				pthread_mutex_lock(&mutex);
				ObtenerPartidasJugadas(usuario, conn, respuesta);
				pthread_mutex_unlock(&mutex);
				
				printf("Enviando respuesta: %s\n", respuesta); // Debug
				write(sock_conn, respuesta, strlen(respuesta)+1);
				
			}
			
			else if (codigo ==5){ // consulta 3: dime si existe un jugador con el nombre de usuario x
				p = strtok(NULL, "/");
				if (!p) {
					snprintf(respuesta, 200, "0/FormatoInvalido/");
					write(sock_conn, respuesta, strlen(respuesta)+1);
					return;
				}	
				char usuario[50];
				strncpy(usuario, p, sizeof(usuario)-1);
				usuario[sizeof(usuario)-1] = '\0';
				printf("Buscando partida mÃ¡s corta para: %s\n", usuario);
				pthread_mutex_lock(&mutex);
				obtener_partida_mas_corta(usuario, conn, respuesta);
				pthread_mutex_unlock(&mutex);
				
				printf("Enviando respuesta: %s\n", respuesta);
				write(sock_conn, respuesta, strlen(respuesta)+1);
			}
			else if(codigo == 6){//Protocolo de invitacion
				p = strtok(NULL, "/");
				int Partida = atoi(p);
				p = strtok(NULL, "/");
				int ninvitados = atoi(p);
				
				for(int i=0;i<ninvitados;i++){
					p = strtok(NULL, "/");
					char invitar[20];
					strcpy(invitar,p);
					pthread_mutex_lock(&mutex);
					Invitacion(invitar,nombre,&milistaconectados,sock_conn,&Partida,partidas);
					pthread_mutex_unlock(&mutex);
					mipartida= Partida;
				}				
				partidas[Partida].numinvitados = ninvitados;
			}
			
			else if(codigo == 7){//Protocolo de respuesta del invitado 
				p = strtok(NULL,"/");
				int resposta = atoi(p);
				if(resposta==1){//respuesta afirmativa
					p = strtok(NULL, "/");
					int partida = atoi(p);
					mipartida = partida;
					pthread_mutex_lock(&mutex);
					AceptarInvitacion(partidas,nombre,&milistaconectados,partida);
					pthread_mutex_unlock(&mutex);
					
				}
				else if(resposta ==2){//respuesta negativa
					
					p = strtok(NULL, "/");
					int partida = atoi(p);
					
					p = strtok(NULL,"/");
					char invitado[20];
					strcpy(invitado,p);
					
					p = strtok(NULL,"/");
					char anfitrion[20];
					strcpy(anfitrion,p);
					
					sprintf(respuesta,"8/2/%s/",invitado);
					for(int i=0; i<partidas[partida].numparticipantes;i++){
						write(partidas[partida].conectados[i].socket,respuesta,sizeof(respuesta));
					}
					partidas[partida].numinvitados = partidas[partida].numinvitados-1;	
					
					if(partidas[partida].numinvitados==0){
						char respuesta[500];
						strcpy(respuesta,"8/5/");
						write(partidas[partida].conectados[0].socket,respuesta,sizeof(respuesta));
						pthread_mutex_lock(&mutex);
						EliminarDePartida(partidas,anfitrion,partida);
						pthread_mutex_unlock(&mutex);
					}
				}
			}
			else if(codigo ==8){//Chat global
				p = strtok(NULL,"/");
				char nombre[20];
				strcpy(nombre,p);
				
				p = strtok(NULL,"/");
				char mensaje[500];
				strcpy(mensaje,p);
				
				pthread_mutex_lock(&mutex);
				EnviarMensajeGlobal(&milistaconectados,nombre,mensaje);
				pthread_mutex_unlock(&mutex);
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
			else if(codigo ==10){//Empezar juego
				p = strtok(NULL, "/");
				int partida = atoi(p);
				
				if(partidas[partida].numparticipantes==4){
					partidas[partida].CasLibres = 36;
					partidas[partida].turno = 0;
					char mensaje[500];
					char colores[500];
					strcpy(colores,"Blue/Green/Red/Yellow");
					char *p1;
					p1 = strtok(colores,"/");
					
					
					
					
					for(int i = 0;i<partidas[partida].numparticipantes;i++){
						sprintf(mensaje,"10/%s/",p1);
						write(partidas[partida].conectados[i].socket,mensaje,sizeof(mensaje));
						p1 = strtok(NULL,"/");
					}
					
					char turno[20];
					strcpy(turno,"11/1/");
					write(partidas[partida].conectados[partidas[partida].turno].socket,turno,sizeof(turno));
					printf("%s\n",partidas[partida].conectados[partidas[partida].turno].nombre);
					printf("%d\n",&partidas[partida].conectados[partidas[partida].turno].socket);
				}
				else{
					char inicio[20];
					strcpy(inicio,"11/-1/");
					write(sock_conn,inicio,sizeof(inicio));
				}
			}
			else if(codigo ==11){//Codigo para poner una ficha
				p = strtok(NULL,"/");
				int partida =atoi(p);
				
				p = strtok(NULL,"/");
				char ubicacion[20];
				strcpy(ubicacion,p);
				
				p = strtok(NULL,"/");
				char color[20];
				strcpy(color,p);
				if(partidas[partida].CasLibres > 1){
					//Siempre que hayan más de dos casillas libres entrara aqui, esto es asi porque si
					//al hacer click al ultimo picturebox(CasLibres==1) no has mandado 12/Partida significa que no ha ganado nadie y por tanto empate
						char mensaje[500];
						
						sprintf(mensaje,"11/%s/%s/",ubicacion,color);
						for(int i=0;i<partidas[partida].numparticipantes;i++)
							write(partidas[partida].conectados[i].socket,mensaje,strlen(mensaje));
						
						partidas[partida].turno+=1;
						if(partidas[partida].turno >3){
							partidas[partida].turno=0;
						}
						
						partidas[partida].CasLibres = partidas[partida].CasLibres -1;
						char turno[20];
						strcpy(turno,"11/1/");
						printf("turno de %d\n",partidas[partida].turno); 
						write(partidas[partida].conectados[partidas[partida].turno].socket,turno,sizeof(turno));
						printf("exit\n");
						//partidas[partida].turno++;
				}else{//Codigo en caso de empatar
					char mensaje[500];
					strcpy(mensaje,"12/2/");
					for(int i=0;i<partidas[partida].numparticipantes;i++)
						write(partidas[partida].conectados[i].socket,mensaje,sizeof(mensaje));
				}
			}
			else if(codigo==12){//Codigo para indicar gane
				p = strtok(NULL,"/");
				int partida =atoi(p);
				
				p = strtok(NULL,"/");
				char ganador[20];
				strcpy(ganador,p);
				
				
				char mensaje[500];
				sprintf(mensaje,"12/1/%s/",ganador);
				for(int i=0;i<partidas[partida].numparticipantes;i++)
					write(partidas[partida].conectados[i].socket,mensaje,sizeof(mensaje));
			}
			printf("--------------------------------------------------------------------\n");
	}
	
	close(sock_conn);
}	// AtenderCliente




int main(int argc, char *argv[])
{
	int sock_conn, sock_listen, ret;
	struct sockaddr_in serv_adr;
	char peticion[512];
	char respuesta[512];
	// INICIALITZACIONS
	// Obrim el socket
	if ((sock_listen = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		printf("Error creant socket");
	// Fem el bind al port
	
	memset(&serv_adr, 0, sizeof(serv_adr));// inicialitza a zero serv_addr
	serv_adr.sin_family = AF_INET;
		
	// asocia el socket a cualquiera de las IP de la m?quina.
	//htonl formatea el numero que recibe al formato necesario
	serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
	// establecemos el puerto de escucha
	serv_adr.sin_port = htons(puerto);
	if (bind(sock_listen, (struct sockaddr *) &serv_adr, sizeof(serv_adr)) < 0)
		printf ("Error al bind");
	if (listen(sock_listen, 4) < 0)
		printf("Error en el Listen");

	pthread_t thread;
	// Bucle infinito
	for (;;){
			printf ("Escuchando\n");
			
			sock_conn = accept(sock_listen, NULL, NULL);
			printf ("He recibido conexion\n");
			//sock_conn es el socket que usaremos para este cliente
			
			milistaconectados.conectados[milistaconectados.num].socket = sock_conn;
			int socket1= milistaconectados.conectados[milistaconectados.num].socket;
			pthread_create(&thread, NULL, AtenderCliente, &socket1);
			milistaconectados.num+=1;
			printf("%d\n",milistaconectados.num);
	}
}  //Main
	

