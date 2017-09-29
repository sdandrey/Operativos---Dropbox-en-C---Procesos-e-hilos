#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501 /* Windows XP. */
#endif
#include <winsock2.h>
#include <Ws2tcpip.h>
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#endif
#if defined _WIN32
#define close(x) closesocket(x)
#endif
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <time.h>
#define bloque 4000
#define registro 1000

//the thread function

void *connection_handler(void *);
int servidor(char*);

char *directorioActual;
char registros[10][registro];

struct table{
	char nombres[registro][256];
	long fechasServidor[registro];
	long fechasCliente[registro];
	char verificar[registro];
	int cantidadArchivos;
};

int inicializarTabla(struct table *tab){
	tab->cantidadArchivos = 0;
	return 0;
}

int cargarTabla(struct table *tab){
	FILE *f = fopen("Sinc.cfg", "rb+");
	if(f){
		fread(tab, sizeof(struct table), 1, f);
		printf("Se cargo la tabla registro\n");
	}else{
		f = fopen("Sinc.cfg", "wb+");
		fseek(f, 0, SEEK_SET);
		fwrite(tab, sizeof(struct table), 1, f);
		printf("Se creo la tabla de registro\n");
	}
	fclose(f);

	//El arreglo de verificacion se pone en cero por defecto
	for(int i = 0;i<tab->cantidadArchivos;i++){
		if(tab->verificar[i]!=2) //2 indica los documentos que han sido renombrados en caso de conflicto
			tab->verificar[i] = 0;
	}
	return 0;
}

int imprimirTabla(struct table *tab){
	printf("********Tabla de Registros**********\n");
	for(int i = 0;i<tab->cantidadArchivos;i++){
		printf("%s - %d %d %d\n", tab->nombres[i], tab->fechasServidor[i], tab->fechasServidor[i], tab->verificar[i]);
	}
	printf("******************\n");
}

int reacomodarLista(struct table *tab, int indice){
	for(int i = indice;i<tab->cantidadArchivos;i++){
		strcpy(tab->nombres[i-1], tab->nombres[i]);
		tab->fechasServidor[i-1] = tab->fechasServidor[i];
		tab->fechasCliente[i-1] = tab->fechasCliente[i];
		tab->verificar[i-1] = tab->verificar[i];
	}
	return 0;
}

int agregarArchivo(struct table *tab, char *nombreArchivo, int fecha){
	strcpy(tab->nombres[tab->cantidadArchivos], nombreArchivo);
	tab->fechasServidor[tab->cantidadArchivos] = fecha;
	tab->fechasCliente[tab->cantidadArchivos] = getFechaArchivo(nombreArchivo);
	tab->cantidadArchivos++;
	return 0;
}

int modificarArchivo(struct table *tab, char *nombreArchivo, int fecha){
	for(int i = 0;i<tab->cantidadArchivos;i++){
		if(strcmp(tab->nombres[i], nombreArchivo) == 0){
			tab->fechasServidor[i] = fecha;
			tab->fechasCliente[i] = getFechaArchivo(nombreArchivo);
			return 0;
		}
	}
	return -1; //No se encontro archivo
}

int buscarArchivo(struct table *tab, char *nombreArchivo){
	for(int i = 0;i<tab->cantidadArchivos;i++){
		if(strcmp(nombreArchivo, tab->nombres[i]) == 0){
			tab->verificar[i] = 1;
			FILE *f = fopen(nombreArchivo, "rb+");
			if(!f){
				printf("El archivo existe virtualmente pero fisicamente no... Eliminando entrada\n");
				eliminarArchivo(tab, nombreArchivo);
				return -1;
			}else{
				fclose(f);
				return i;
			}
		}
	}
	return -1;
}

int getFechaArchivo(char *ruta){
    struct stat attr;
    stat(ruta, &attr);
    return attr.st_mtime;
}

int eliminarArchivo(struct table *tab, char *nombreArchivo){
	for(int i = 0;i<tab->cantidadArchivos;i++){
		if(strcmp(nombreArchivo, tab->nombres[i]) == 0){
			//Encontro el archivo
			reacomodarLista(tab, i+1);
			tab->cantidadArchivos--;
			return 0;
		}

	}
	return -1; //No se encontro el archivo
}

int guardarTabla(struct table *tab){
	imprimirTabla(tab);

	for(int i = 0;i<tab->cantidadArchivos;i++){
		if(tab->verificar[i] == 0){
			//Se elimina el archivo que se comprueba que no se encuentra en el servidor
			int status = remove(tab->nombres[i]);
			printf("Eliminando: %s\n", tab->nombres[i]);
			eliminarArchivo(tab, tab->nombres[i]);
		}
	}
	FILE *f = fopen("Sinc.cfg", "wb+");
	fseek(f, 0, SEEK_SET);
	fwrite(tab, sizeof(struct table), 1, f);
	fclose(f);
}

unsigned long ToUInt(char* str)
{
    unsigned long mult = 1;
    unsigned long re = 0;
    int len = strlen(str);
    for(int i = len -1 ; i >= 0 ; i--)
    {
        re = re + ((int)str[i] -48)*mult;
        mult = mult*10;
    }
    return re;
}

int cliente(char *directorio , char *ip) {
	struct table tab;
	int sock;
	struct sockaddr_in server;
	char message[1000];
	char *server_reply = malloc(bloque+1);
	#if defined _WIN32
	WSADATA wsa_data;
	WSAStartup(MAKEWORD(1,1), &wsa_data);
	#endif
	//Create socket
	sock = socket(AF_INET , SOCK_STREAM , 0);
	if (sock == -1) {
		printf("Could not create socket");
	}
	puts("Socket created");
	server.sin_addr.s_addr = inet_addr(ip); // "127.0.0.1" para modo local
	server.sin_family = AF_INET;
	server.sin_port = htons( 8889 );
	//Connect to remote server
	if (connect(sock , (struct sockaddr *)&server ,
	sizeof(server)) < 0) {
		perror("Error al conectar");
		return 1;
	}
	puts("Conectado\n");
	//keep communicating with server	
	strcpy(message, "1");
	if (send(sock, directorio, bloque,0) < 0) {
		puts("Send failed");
		return 1;
	}
	printf("Se van a recibir los archivos\n");
	inicializarTabla(&tab);
	cargarTabla(&tab);
	imprimirTabla(&tab);
	while(1) {
		strcpy(server_reply, "");
		if( recv(sock , server_reply , 256 , 0) < 0) {
			puts("Error al recibir");
		break;//
		}
		if(strcmp(server_reply, "__")==0){
			printf("Se termino de sincronizar\n");
			guardarTabla(&tab);
			return 0;
		}
		printf("Recibiendo: %s\n", server_reply);
		//Se recibe el contenido del archivo
		send(sock , "1" , 1,0); //Si lo mando con bloque no funciona correctamente :/
		//Recibir archivo
		char *resp = "s";
		char *nombre = malloc(server_reply);
		nombre = strdup(server_reply);
		if( recv(sock , server_reply , 256 , 0) < 0) {
			puts("Error al recibir");
		break;//
		}
		//Se recibe el contenido del archivo
		long temp = ToUInt(server_reply);
		send(sock , "11" , 1,0);
		int indice = buscarArchivo(&tab, nombre);
		int igual = 0; // 0 = archivo igual, 1 = nuevo archivo, 2 = modificacion o conflicto 
		//Tabla de registros-------
		if(indice == -1){
			printf("Nuevo Archivo: %s\n", nombre);
			igual = 1;
		}else{
			if(temp>tab.fechasServidor[indice]){
				if(getFechaArchivo(nombre)!=tab.fechasCliente[indice]){
					strcat(nombre, "_");
					int indice2 = buscarArchivo(&tab, nombre);
					if(indice2 != -1){ // En caso de que ya se haya agregado entonces simplemente se modifica el archivo de conflicto
						eliminarArchivo(&tab, nombre);
						agregarArchivo(&tab, nombre, temp);
						tab.verificar[tab.cantidadArchivos-1] = 2;
					}else{ //Si no existe el archivo de conflicto entonces se agrega
						agregarArchivo(&tab, nombre, temp);
						tab.verificar[tab.cantidadArchivos-1] = 2;
						printf("Conflicto en: %s\n", nombre);		
					}
				}else{
					eliminarArchivo(&tab, nombre);
					agregarArchivo(&tab, nombre, temp);
					tab.verificar[tab.cantidadArchivos-1] = 1; //Se reafirma que se visito el archivo, ya que al eliminarse se quita esta marca
					printf("Modificando: %s\n", nombre);
				}
				igual = 2;
			}else{//Es igual, no se debe de modificar
				igual = 0;
			}
		}
		FILE *f;
		if(igual == 1 || igual == 2){
			f = fopen(nombre, "wb+");
			fseek(f, 0, SEEK_SET);
		}
		//Se recibira el archivo por partes
		do{
			//Se carga una parte del archivo
			strcpy(server_reply,"");
			server_reply = malloc(bloque);
			if( recv(sock , server_reply , bloque , 0) < 0) {
				puts("Error al recibir");
				break;
			}
			//Recepcion del tamano del archivo
			int taman = 0;
			char tamano_s[bloque];
			if( recv(sock , tamano_s , bloque , 0) < 0) {
				puts("Error al recibir");
				break;
			}

			taman = (int)ToUInt(tamano_s);
			//Se escribe esa parte
			if(igual==1 || igual == 2){
				fwrite(server_reply, taman, 1, f);
			}

			server_reply = malloc(bloque);
			//Se obtiene la respuesta del servidor para ver si hay mas partes o no.
			if( recv(sock , server_reply , bloque , 0) < 0) {
					printf("Error al recibir");
					break;
			}
		}while(strcmp(server_reply, "f")!=0);
		if(igual == 1){ //Flag = 0 significa que se debe de agregar una nuevva entrada
			fclose(f);
			agregarArchivo(&tab, nombre, temp);
			tab.verificar[tab.cantidadArchivos-1] = 1; //Se reafirma que se visito el archivo, ya que al eliminarse se quita esta marca
		}
		if(igual == 2){	//Flag = 1 significa que se debe de modificar la entrada
			fclose(f);
			modificarArchivo(&tab, nombre, temp);
		}
		send(sock , "11" , 1,0); // Se indica que termino la recepcion del archivo
		server_reply = malloc(bloque);
	}
	printf("salio del ciclo\n");
	close(sock);
	#if defined _WIN32
	WSACleanup();
	#endif
	return 0;
}

int listdir(int sock, const char *name, int level){
	DIR *dir;
	struct dirent *entry;
	if((dir = opendir(name))){
		/*Se carga de nuevo el directorio*/
	}
	if(!(entry = readdir(dir))){
		return -1;
	}
	do{
		if(entry->d_type==DT_DIR){
			char path[1024];
			int len = snprintf(path, sizeof(path)-1, "%s/%s", name, entry->d_name);
			path[len] = 0;
			if(strcmp(entry->d_name, ".")==0 || strcmp(entry->d_name,"..")==0)
				continue;
			printf("%*s[%s]\n", level*2, "",entry->d_name);
			listdir(sock, path, level + 1);
		}
		else{
			char *temp = malloc(strlen(entry->d_name) + strlen(name)+2);
			strcpy(temp, name);
			strcat(temp, "/");
			strcat(temp, entry->d_name);
			printf("%s\n", temp);
			enviarArchivo(sock, temp);
		}
	}while(entry = readdir(dir));
	send(sock, "__", 256, 0);
	printf("Se termino de sincronizar con la conexion: %d\n", sock);
	closedir(dir);
	return 0;
}

int servidor(char *directorio) {
	int socket_desc , client_sock , c , *new_sock;
	struct sockaddr_in server , client;
	#if defined _WIN32
	WSADATA wsa_data;
	WSAStartup(MAKEWORD(1,1), &wsa_data);
	#endif
	//Create socket
	socket_desc = socket(AF_INET , SOCK_STREAM , 0);
	if (socket_desc == -1) {
		printf("No se pudo crear el socket");
	}
	puts("Socket creado");
	//Prepare the sockaddr_in structure
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons( 8889 );
	//Bind
	if( bind(socket_desc,(struct sockaddr *)&server ,
		sizeof(server)) < 0) {
		//print the error message
		perror("bind failed. Error");
		return 1;
	}
	puts("Asociacion correcta");
	//Listen
	listen(socket_desc , 3);
	//Accept and incoming connection
	puts("Esperando conexiones...");
	c = sizeof(struct sockaddr_in);
	while( (client_sock = accept(socket_desc,
	(struct sockaddr *)&client, (socklen_t*)&c)) ) {
		puts("Conexion aceptada");
		pthread_t sniffer_thread;
		new_sock = malloc(1);
		*new_sock = client_sock;
		if ( pthread_create( &sniffer_thread , NULL ,
			connection_handler , (void*) new_sock) < 0) {
			perror("No se puede crear el hilo");
			return 1;
		}
		puts("Manejador asignado");
		}
	if (client_sock < 0) {
		perror("Error al aceptar");
		return 1;
	}
	#if defined _WIN32
	WSACleanup();
	#endif
	return 0;
}

int elegirOperacion(int sock, int tipo, char *rutaArchivo){
	if(0==tipo){
		listdir(sock, rutaArchivo, 0);
	}else if(1==tipo){

	}else if(2==tipo){

	}else{
		printf("Tipo de envio no especificado\n");
	}
}

int enviarArchivo(int sock, char *rutaArchivo){
	printf("ruta Archivo:::: %s\n", rutaArchivo);
	char *resp;
	resp = malloc(10);
	printf("Enviando: %s\n", rutaArchivo);
	//Envio del nombre del archivo
	send(sock , rutaArchivo , 256,0);		
	if( recv(sock , resp , bloque , 0) < 0) {
		puts("Error al recibir");
	}
	if(strcmp(resp,"1")==0){
		printf("Se recibio exitosamente el nombre\n");
	}else{
		printf("Ocurrio un error al pasar el nombre\n");
		printf("resp: %s cont\n", resp);
	}
	//Envio de la fecha de modificacion
	int aInt = getFechaArchivo(rutaArchivo);
	char str[15];
	sprintf(str, "%d", aInt);
	send(sock , str , 256,0);
	if( recv(sock , resp , bloque , 0) < 0) {
		puts("Error al recibir");
	}
	if(strcmp(resp,"1")!=0){
		printf("Ocurrio un error al pasar el nombre\n");
	}
	//Envio del contenido del archivo en partes
	FILE *f = fopen(rutaArchivo, "rb+");
	fseek(f, 0, SEEK_END);
	long fsize = ftell(f);
	fseek(f, 0, SEEK_SET);  //same as rewind(f);
	int contador = fsize;
	printf("Tamano archivo: %d\n", fsize);
	int acumulado = 0;

	int cantidad = 0; //cantidad de datos que se pasaran en esa parte
	char *string;
	while(contador > 0){
		if(contador < bloque){
			cantidad = contador;
		}else{
			cantidad = bloque;
		}
		string = malloc(bloque);
		fread(string, cantidad, 1, f);
		send(sock , string , bloque,0);
		acumulado = acumulado + cantidad;
		contador = contador - bloque;
		int aInt = cantidad;
		char str[15];
		sprintf(str, "%d", aInt);
		send(sock , str , bloque,0);
		if(contador>0){
			send(sock, "n", bloque, 0); //Se indica que no se ha terminado el archivo
		}else{
			send(sock, "f", bloque, 0); //Se indica que finalizo de pasar el archivo
			printf("Finalizo de pasar el archivo\n");
		}
	}
	fclose(f);
	if( recv(sock , resp , bloque , 0) < 0) {
		puts("Error al recibir");
		return -1;
	}
	if(strcmp(resp,"11") == 0){
		printf("\n");
	}else{
		printf("\n");
	}
	printf("*****************\n");
}

void *connection_handler(void *socket_desc) {
	//Get the socket descriptor
	int sock = *(int*)socket_desc;
	int read_size;
	char *message , client_message[bloque];
	message = malloc(bloque);
	//Receive a message from client
	while (read_size = recv(sock ,
	message , bloque , 0) > 0 ) {
	//Send the message back to client
		printf("%s %s\n", directorioActual, message);
		if(strcmp(directorioActual, message) !=0 ){
			printf("Error: No se intenta sincronizar el mismo directorio entre el cliente y servidor\n");
			return -1; // No se esta intentando sincronizar el mismo directorio
		}
		elegirOperacion(sock, 0, directorioActual);
	}
	if (read_size == 0) {
		puts("Cliente Desconectado");
		fflush(stdout);
	} else if (read_size == -1) {
		perror("Error al recibir");
	}
	free(socket_desc);
	return 0;
}

int main(int argc , char *argv[]){ 
	if(argc == 2){
		//Es el servidor
		printf("Se ha iniciado el programa en modo servidor\n");
		printf("Directorio: %s\n", argv[1]);
		directorioActual = malloc( strlen(argv[1]) + 1);
		directorioActual = argv[1];
		servidor(argv[1]);
	}
	else if(argc == 3){

		printf("Directorio Actual: %s\n", directorioActual);

		//Es el cliente, el segundo parametro es la IP
		printf("Se ha iniciado el programa en modo cliente\n");
		printf("Directorio: %s\n", argv[1]);
		printf("Direccion IP: %s\n", argv[2]);
		cliente(argv[1], argv[2]);
	}
	else{
		printf("Error: Cantidad incorrecta de parametros\n");
	}
}