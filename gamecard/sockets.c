 /* ABAJO PUSE UNAS FUNCIONES DE MANEJO DE BLOQUES, LO DE MOVER LOS BOQUES POR SI CAMBIA DE 9 A 10 Y ASI..
 IGUAL SEGUN LEI LO TENES BASTANTE MANEJADO...
#include "sockets.h"

void suscripcion_colas_broker() {

	log_info(logger, "ESTABLECIENDO CONEXION CON EL BROKER");
	suscribirse_a_new_pokemon();
	suscribirse_a_catch_pokemon();
	suscribirse_a_get_pokemon();

	if(socket_cliente_np == -1 && socket_cliente_cp == -1 && socket_cliente_gp == -1) {
			log_error(logger, "BROKER NO ESTA DISPONIBLE PARA LA CONEXION");
			pthread_create(&servidor_gamecard, NULL, (void *)iniciar_servidor, NULL);
			pthread_join(servidor_gamecard, NULL);
	} else {
		log_info(logger, "GAMECARD CONECTADO AL BROKER");

		//pthread_create(&thread_new_pokemon, NULL, (void *)recibir_mensajes_new_pokemon, NULL);
		//pthread_detach(thread_new_pokemon);

		//pthread_create(&thread_catch_pokemon, NULL, (void *)recibir_mensajes_catch_pokemon, NULL);
		//pthread_detach(thread_catch_pokemon);

		//pthread_create(&thread_get_pokemon, NULL, (void *)recibir_mensajes_get_pokemon, NULL);
		//pthread_detach(thread_get_pokemon);
	}
}

void suscribirse_a_new_pokemon() {
	socket_cliente_np = crear_conexion(datos_config->ip_broker, string_itoa(datos_config->puerto_broker));
	if(socket_cliente_np != -1) {
		//enviar_mensaje_suscripcion(NEW_POKEMON, socket_cliente_np, pid_gamecard);
		enviar_info_suscripcion(NEW_POKEMON, socket_cliente_np, pid_gamecard);
		recv(socket_cliente_np, &(acks_gamecard.ack_new), sizeof(int), MSG_WAITALL);
		log_info(logger, "ACK RECIVIDO PARA COLA NEW_POKEMON: %d", acks_gamecard.ack_new);
	}
}

void suscribirse_a_catch_pokemon() {
	socket_cliente_cp = crear_conexion(datos_config->ip_broker, string_itoa(datos_config->puerto_broker));
	if(socket_cliente_cp != -1) {
		//enviar_mensaje_suscripcion(CATCH_POKEMON, socket_cliente_cp, pid_gamecard);
		enviar_info_suscripcion(CATCH_POKEMON, socket_cliente_cp, pid_gamecard);
		recv(socket_cliente_cp, &(acks_gamecard.ack_catch), sizeof(int), MSG_WAITALL);
		log_info(logger, "ACK RECIVIDO PARA COLA CATCH_POKEMON: %d", acks_gamecard.ack_catch);
	}
}

void suscribirse_a_get_pokemon() {
	socket_cliente_gp = crear_conexion(datos_config->ip_broker, string_itoa(datos_config->puerto_broker));
	if(socket_cliente_gp != -1) {
		//enviar_mensaje_suscripcion(GET_POKEMON, socket_cliente_gp, pid_gamecard);
		enviar_info_suscripcion(GET_POKEMON, socket_cliente_gp, pid_gamecard);
		recv(socket_cliente_gp, &(acks_gamecard.ack_get), sizeof(int), MSG_WAITALL);
		log_info(logger, "ACK RECIVIDO PARA COLA GET_POKEMON: %d", acks_gamecard.ack_get);
	}
}

void recibir_mensajes_new_pokemon(){
	while(1) {
		int codigo_operacion;
		recv(socket_cliente_np, &(codigo_operacion), sizeof(int), MSG_WAITALL);

		t_list* paquete = recibir_paquete(socket_cliente_np);

		void display(void* valor){

			int desplazamiento = 0;

			NPokemon *new_pokemon = malloc(sizeof(NPokemon));

			memcpy(&(new_pokemon->id_mensaje), valor, sizeof(int));
			desplazamiento += sizeof(int);
			memcpy(&(new_pokemon->size_nombre), valor + desplazamiento, sizeof(int));
			desplazamiento += sizeof(int);
			new_pokemon->nombre = malloc(new_pokemon->size_nombre + 1);
			memcpy(new_pokemon->nombre, valor + desplazamiento, new_pokemon->size_nombre + 1);
			desplazamiento += new_pokemon->size_nombre + 1;
			memcpy(&(new_pokemon->posicion.posX), valor + desplazamiento, sizeof(int));
			desplazamiento += sizeof(int);
			memcpy(&(new_pokemon->posicion.posY), valor + desplazamiento, sizeof(int));
			desplazamiento += sizeof(int);
			memcpy(&(new_pokemon->cantidad), valor + desplazamiento, sizeof(int));
			desplazamiento += sizeof(int);
			log_info(logger,"Llega Un Mensaje Tipo: NEW_POKEMON ID:%d POKEMON:%s POSX:%d POSY:%d CANT:%d\n",new_pokemon->id_mensaje, new_pokemon->nombre, new_pokemon->posicion.posX, new_pokemon->posicion.posY, new_pokemon->cantidad);
			free(valor);
			enviar_ack(NEW_POKEMON, new_pokemon->id_mensaje, pid_gamecard, socket_cliente_np);
			free(new_pokemon->nombre);
			free(new_pokemon);
		}

		list_iterate(paquete,(void*)display);
		list_destroy(paquete);
	}
}

void recibir_mensajes_catch_pokemon(){

	while(1) {

		int codigo_operacion;
		recv(socket_cliente_cp, &(codigo_operacion), sizeof(int), MSG_WAITALL);

		t_list* paquete = recibir_paquete(socket_cliente_cp);

		void display(void* valor){

			int desplazamiento = 0;

			CPokemon *catch_pokemon = malloc(sizeof(CPokemon));

			memcpy(&(catch_pokemon->id_mensaje), valor + desplazamiento, sizeof(int));
			desplazamiento += sizeof(int);

			memcpy(&(catch_pokemon->size_nombre), valor + desplazamiento, sizeof(int));
			desplazamiento += sizeof(int);

			catch_pokemon->nombre = malloc(catch_pokemon->size_nombre + 1);
			memcpy(catch_pokemon->nombre, valor + desplazamiento, catch_pokemon->size_nombre + 1);
			desplazamiento += catch_pokemon->size_nombre + 1;

			memcpy(&(catch_pokemon->posicion.posX), valor + desplazamiento, sizeof(int));
			desplazamiento += sizeof(int);

			memcpy(&(catch_pokemon->posicion.posY), valor + desplazamiento, sizeof(int));
			desplazamiento += sizeof(int);

			log_info(logger,"Llega Un Mensaje Tipo: CATCH_POKEMON ID:%d POKEMON:%s POSX:%d POSY:%d\n", catch_pokemon->id_mensaje, catch_pokemon->nombre, catch_pokemon->posicion.posX, catch_pokemon->posicion.posY);
			enviar_ack(CATCH_POKEMON, catch_pokemon->id_mensaje, pid_gamecard, socket_cliente_cp);
			// pthread_create(&thread_new_pokemon, NULL , NULL, NULL);
			// pthread_detach(thread_new_pokemon);

			free(catch_pokemon->nombre);
			free(catch_pokemon);
			free(valor);
		}
		list_iterate(paquete,(void*)display);
		list_destroy(paquete);
	}
}

void recibir_mensajes_get_pokemon(){

	while(1) {

		int codigo_operacion;
		recv(socket_cliente_gp, &(codigo_operacion), sizeof(int), MSG_WAITALL);

		t_list* paquete = recibir_paquete(socket_cliente_gp);

		void display(void* valor){

			int desplazamiento = 0;

			GPokemon *get_pokemon = malloc(sizeof(GPokemon));

			memcpy(&(get_pokemon->id_mensaje), valor + desplazamiento, sizeof(int));
			desplazamiento += sizeof(int);

			memcpy(&(get_pokemon->size_nombre), valor + desplazamiento, sizeof(int));
			desplazamiento += sizeof(int);

			get_pokemon->nombre = malloc(get_pokemon->size_nombre + 1);
			memcpy(get_pokemon->nombre, valor + desplazamiento, get_pokemon->size_nombre + 1);
			desplazamiento += get_pokemon->size_nombre + 1;

			log_info(logger,"Llega Un Mensaje Tipo: GET_POKEMON ID:%d POKEMON:%s \n", get_pokemon->id_mensaje, get_pokemon->nombre);
			enviar_ack(GET_POKEMON, get_pokemon->id_mensaje, pid_gamecard, socket_cliente_gp);
			// pthread_create(&thread_new_pokemon, NULL , NULL, NULL);
			// pthread_detach(thread_new_pokemon);

			free(valor);
			free(get_pokemon->nombre);
			free(get_pokemon);
		}
		list_iterate(paquete,(void*)display);
		list_destroy(paquete);
	}
}

get_pokemon* deserializar_get(void* buffer) {
    get_pokemon* get = malloc(sizeof(get_pokemon));

    memcpy(&(get->name_size),buffer, sizeof(int));
	get->name = malloc(get->name_size+1); // para que esta este segundo malloc?
	memcpy(get->name,buffer+sizeof(int), get->name_size+1);

    return get;
}

t_list* recibir_paquete(int socket_cliente){
	uint32_t size;
	uint32_t desplazamiento = 0;
	void * buffer;
	t_list* valores = list_create();
	uint32_t tamanio;
	buffer = recibir_buffer(socket_cliente,&size);
	while(desplazamiento < size){
		memcpy(&tamanio, buffer + desplazamiento, sizeof(uint32_t));
		desplazamiento+=sizeof(uint32_t);
		void* valor = malloc(tamanio);
		memset(valor,0,tamanio);
		memcpy(valor, buffer+desplazamiento, tamanio);
		desplazamiento+=tamanio;
		list_add(valores, valor);
	}
	free(buffer);
	return valores;
}

void enviar_ack(int tipo,int id,pid_t pid,int cliente_fd) {
	t_paquete* paquete = malloc(sizeof(t_paquete));
	paquete->codigo_operacion = tipo;
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = sizeof(int)+sizeof(pid_t);
	paquete->buffer->stream = malloc(paquete->buffer->size);
	memcpy(paquete->buffer->stream,&id,sizeof(int));
	memcpy(paquete->buffer->stream+sizeof(int),&pid, sizeof(pid_t));

	int bytes = paquete->buffer->size + 2*sizeof(int);

	void* a_enviar = serializar_paquete(paquete, bytes);

	send(cliente_fd, a_enviar, bytes, 0);
	log_info(logger, "ACK ENVIADO A BROKER");

	free(a_enviar);
	free(paquete->buffer->stream);
	free(paquete->buffer);
	free(paquete);
}

void* recibir_buffer(int socket_cliente, uint32_t* size){
	void * buffer;

	recv(socket_cliente, size, sizeof(int), MSG_WAITALL);
	buffer = malloc(*size);
	recv(socket_cliente, buffer, *size, MSG_WAITALL);

	return buffer;
}

int crear_conexion() {
	struct addrinfo hints;
	struct addrinfo *server_info;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	getaddrinfo("127.0.0.1", "4444", &hints, &server_info); // despues en la entrega cambiar los valores de la ip y el puerto

	int socket_cliente = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);

	if(connect(socket_cliente, server_info->ai_addr, server_info->ai_addrlen) == -1) {
		return -1;
	}

	freeaddrinfo(server_info);

	return socket_cliente;
}

void enviar_mensaje_suscripcion(enum TIPO cola, int socket_cliente, pid_t pid) {
	int desplazamiento = 0;
	t_paquete* paquete = malloc(sizeof(t_paquete));

	paquete->codigo_operacion = SUSCRITO;
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = sizeof(enum TIPO) + sizeof(pid_t);
	paquete->buffer->stream = malloc(paquete->buffer->size);

	memcpy(paquete->buffer->stream + desplazamiento, &(cola), paquete->buffer->size);
	desplazamiento += sizeof(enum TIPO);

	memcpy(paquete->buffer->stream + desplazamiento, &(pid), sizeof(pid_t));
	desplazamiento += sizeof(pid_t);

	int bytes = paquete->buffer->size + 2*sizeof(int);

	void* info_a_enviar = serializar_paquete(paquete, bytes);

	log_info(logger, "ENVIANDO MENSAJE DE SUSCRIPCION AL BROKER");
	if(send(socket_cliente, info_a_enviar, bytes, MSG_WAITALL) == -1) {
		log_error(logger, "ERROR AL ENVIAR MENSAJE DE SUSCRIPCION AL BROKER");
	}

	free(info_a_enviar);
	free(paquete->buffer->stream);
	free(paquete->buffer);
	free(paquete);
}

void* serializar_paquete(t_paquete* paquete, int bytes) {
	void *magic = malloc(bytes);
	int desplazamiento = 0;

	memcpy(magic + desplazamiento, &(paquete->codigo_operacion), sizeof(int));
	desplazamiento+= sizeof(int);
	memcpy(magic + desplazamiento, &(paquete->buffer->size), sizeof(int));
	desplazamiento += sizeof(int);
	memcpy(magic + desplazamiento, paquete->buffer->stream, paquete->buffer->size);
	desplazamiento+= paquete->buffer->size;

	return magic;
}

void iniciar_servidor(void) {

	int socket_servidor;

    struct addrinfo hints, *servinfo, *p;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    getaddrinfo(IP_SERVIDOR, PUERTO_SERVIDOR, &hints, &servinfo);

    for (p=servinfo; p != NULL; p = p->ai_next)
    {
        if ((socket_servidor = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
            continue;

        if (bind(socket_servidor, p->ai_addr, p->ai_addrlen) == -1) {
            close(socket_servidor);
            continue;
        }
        break;
    }

	listen(socket_servidor, SOMAXCONN);
	log_info(logger, "GAMECARD INICIADO COMO SERVIDOR PARA GAMEBOY, ESPERANDO MENSAJES...");
    freeaddrinfo(servinfo);

    while(1)
    	esperar_cliente(socket_servidor);
}

void esperar_cliente(int socket_servidor) {
	struct sockaddr_in dir_cliente;
	int tam_direccion = sizeof(struct sockaddr_in);

	int socket_cliente = accept(socket_servidor, (void*) &dir_cliente, (socklen_t *)&tam_direccion);
	log_info(logger, "GAMEBOY CONECTADO!",socket_cliente);

	int cod_op;
	if(recv(socket_cliente, &cod_op, sizeof(int), MSG_WAITALL) == -1) {
		cod_op = -1;
	}
	log_info(logger, "codigo de operacion: %d", cod_op);

	atender_peticion(socket_cliente, cod_op);
}

void atender_peticion(int socket_cliente, int cod_op) {

	int sizeStream;
	if(recv(socket_cliente, &sizeStream, sizeof(int), MSG_WAITALL) == -1) {
		sizeStream = -1;
	}
	printf("size_stream: %d\n", sizeStream);

	void *stream = malloc(sizeStream);
	if(recv(socket_cliente, stream, sizeStream, MSG_WAITALL) == -1) {
	}

	switch(cod_op) {
		case NEW_POKEMON:
			log_info(logger, "SE RECIBIO UN MENSAJE CON OPERACION NEW_POKEMON");
			new_pokemon *newPokemon = malloc(sizeof(new_pokemon));
			int id_mensaje_new;
			memcpy(&(id_mensaje_new), stream, sizeof(int));
			printf("ID MENSAJE: %d\n", id_mensaje_new);

			//deserealizar_new_pokemon_gameboy(stream, newPokemon);
			newPokemon = deserializar_new(stream + sizeof(int));

			printf("POKEMON: %s\n", newPokemon->name);
			printf("PosX: %d\n", newPokemon->pos.posx);
			printf("PosY: %d\n", newPokemon->pos.posy);
			printf("Cantidad: %d\n", newPokemon->cantidad);

			//pthread_create(&atender_new_pokemon, NULL, (void *) operacion_new_pokemon, (void *) newPokemon);

			free(newPokemon);
			free(stream);
		break;

		case CATCH_POKEMON:
			log_info(logger, "SE RECIBIO UN MENSAJE CON OPERACION CATCH_POKEMON");
			catch_pokemon *catchPokemon = malloc(sizeof(catch_pokemon));
			int id_mensaje_catch;
			memcpy(&(id_mensaje_catch), stream, sizeof(int));
			printf("ID MENSAJE: %d\n", id_mensaje_catch);
			//deserealizar_catch_pokemon_gameboy(stream, catchPokemon);
			catchPokemon = deserializar_catch(stream + sizeof(int));

			printf("POKEMON: %s\n", catchPokemon->name);
			printf("PosX: %d\n", catchPokemon->pos.posx);
			printf("PosY: %d\n", catchPokemon->pos.posy);

			free(catchPokemon);
			free(stream);
		break;

		case GET_POKEMON:
			log_info(logger, "SE RECIBIO UN MENSAJE CON OPERACION GET_POKEMON");
			get_pokemon *getPokemon = malloc(sizeof(get_pokemon));
			int id_mensaje;
			memcpy(&(id_mensaje), stream, sizeof(int));
			printf("ID MENSAJE: %d\n", id_mensaje);
			//deserealizar_get_pokemon_gameboy(stream, getPokemon);
			getPokemon = deserializar_get(stream + sizeof(int));

			//printf("POKEMON: %s\n", getPokemon->name);

			free(getPokemon);
			free(stream);
		break;

		default:
			log_warning(logger, "NO SE RECIBIO NINGUNA DE LAS ANTERIORES");
		break;
	}
}

// Deserealizacion para Gameboy

void deserealizar_new_pokemon_gameboy(void *stream, new_pokemon *newPokemon) {
	int desplazamiento = 0;

	memcpy(&(newPokemon->id_mensaje), stream + desplazamiento, sizeof(int));
	desplazamiento += sizeof(int);

	int size_name;
	memcpy(&size_name, stream + desplazamiento, sizeof(int));
	desplazamiento += sizeof(int);
	size_name ++;

	newPokemon->nombre = malloc(size_name);
	memcpy(newPokemon->nombre, stream + desplazamiento, size_name);
	desplazamiento += size_name;

	memcpy(&(newPokemon->posicion), stream + desplazamiento, sizeof(Position));
	desplazamiento += sizeof(Position);

	memcpy(&(newPokemon->cantidad), stream + desplazamiento, sizeof(int));
	desplazamiento += sizeof(int);
}


void deserealizar_catch_pokemon_gameboy(void *stream, CPokemon *catchPokemon) {
	int desplazamiento = 0;

	memcpy(&(catchPokemon->id_mensaje), stream + desplazamiento, sizeof(int));
	desplazamiento += sizeof(int);

	int size_name;
	memcpy(&size_name, stream + desplazamiento, sizeof(int));
	desplazamiento += sizeof(int);
	size_name ++;

	catchPokemon->nombre = malloc(size_name);
	memcpy(catchPokemon->nombre, stream + desplazamiento, size_name);
	desplazamiento += size_name;

	memcpy(&(catchPokemon->posicion), stream + desplazamiento, sizeof(Position));
	desplazamiento += sizeof(Position);
}

void deserealizar_get_pokemon_gameboy(void *stream, GPokemon *getPokemon) {
	int desplazamiento = 0;

	memcpy(&(getPokemon->id_mensaje), stream + desplazamiento, sizeof(int));
	desplazamiento += sizeof(int);

	int size_name;
	memcpy(&size_name, stream + desplazamiento, sizeof(int));
	desplazamiento += sizeof(int);
	size_name ++;

	getPokemon->nombre = malloc(size_name);
	memcpy(getPokemon->nombre, stream + desplazamiento, size_name);
	desplazamiento += size_name;
}

//---------------------------------------------------------------------------
//QUIZAS SIRVE ES EL MANEJO DE BLOQUES 

void escribirRegistroEnArchivo(char* direccionArchivo, nodo_memtable* registro){
	t_config* archivo = config_create(direccionArchivo);
	char** bloques = config_get_array_value(archivo, "BLOCKS");
	int size = config_get_int_value(archivo, "SIZE");
	int length = cantidadElementosCharAsteriscoAsterisco(bloques);
	char* direccionBloque = direccionDeBloque(bloques[length - 1]);
	FILE* bloque = fopen(direccionBloque, "a");
	printf("REGISTRO: Timestamp = %s, Key = %s, Value = %s\n", string_itoa(registro->timestamp),  string_itoa(registro->key), registro->value);
	char* registroString = pasarRegistroAString(registro);
	printf("DIRECCION DEL BLOQUE: %s\n", direccionArchivo);
	int longitudRegistro = string_length(registroString) + 1;
	int sobrante;
	int indice = 0;
	char* registroAuxiliar;

	while( longitudRegistro > 0){
		sobrante = tamanioMaximoDeArchivo - size%tamanioMaximoDeArchivo;
		printf("sobrante = %i\n",sobrante);

		if( sobrante - longitudRegistro >= 0 ){
			//printf("HOLA123\n");
			strcat(registroString, "\n");
			fwrite(registroString, strlen(registroString), 1,bloque);
			//fprintf(bloque, "%s\n", registroString);
			//printf("HOLA123\n");
			fclose(bloque);
			//printf("HOLA123\n");
			//free(registroString);
			size += longitudRegistro;
			longitudRegistro = 0;
		}
		else{
			char* registroRecortado = string_substring(registroString, indice, sobrante);
			fwrite(registroRecortado, strlen(registroRecortado), 1,  bloque);
			//fprintf(bloque, "%s", registroRecortado);
			//fprintf(bloque, "%s", string_substring(registroString, indice, sobrante));
			indice += sobrante;
			fclose(bloque);
			asignarBloqueAConfig(archivo);
			//char* sizeString =  string_itoa(size);
			//config_set_value(archivo, "SIZE", sizeString);
			config_save(archivo);
			config_destroy(archivo);
			free(direccionBloque);

			archivo = config_create(direccionArchivo);

			bloques = config_get_array_value(archivo, "BLOCKS");
			length = cantidadElementosCharAsteriscoAsterisco(bloques);
			printf("bloque = %s\n", bloques[length - 1]);
			direccionBloque = direccionDeBloque(bloques[length - 1]);
			printf("direccion del bloque = %s\n",direccionBloque);
			bloque = fopen(direccionBloque, "a");

			//registroAuxiliar = malloc(longitudRegistro-sobrante+1);
			registroAuxiliar = string_substring_from(registroString, indice);
			free(registroString);

			registroString = string_duplicate(registroAuxiliar);
			//registroString = malloc(strlen(registroAuxiliar) + 1);
			//strcpy(registroString, registroAuxiliar);
			free(registroAuxiliar);
			size += sobrante;
			longitudRegistro -= sobrante;
			printf("LONGITUD REGISTRO = %i\n", longitudRegistro);
			printf("Registro String = %s\n", registroString);
		}
	}



	char* sizeString =  string_itoa(size);

	//printf("DIRECCION DEL ARCHIVO: %s\n", direccionArchivo);
	//printf("size = %s\n", sizeString);

	//printf("HOLA FEDE\n");

	config_set_value(archivo, "SIZE", sizeString);
	//printf("HOLA FEDE\n");
	//config_save(archivo);
	config_save_in_file(archivo,archivo->path);
	//printf("HOLA FEDE\n");
	config_destroy(archivo);
	//printf("terminaste tu funcion?\n");
}

char* direccionDeBloque(char* numeroDeBloque){

	char* direccion_Bloques = obtenerDireccionDirectorio("Bloques/");

	int length = strlen(direccion_Bloques) + strlen(numeroDeBloque) + 5;
	char* direccion = malloc(length);
	int posicion = 0;
	strcpy(direccion, direccion_Bloques);
	posicion += strlen(direccion_Bloques);
	strcpy(direccion + posicion, numeroDeBloque);
	posicion += strlen(numeroDeBloque);
	strcpy(direccion + posicion, ".bin");

	free(direccion_Bloques);

	return direccion;
}

void escanearBloques(char** listaDeBloques, t_list* listaDeRegistros){
	int lengthListaDeBloques = cantidadElementosCharAsteriscoAsterisco(listaDeBloques);
	char* registroIncompleto = malloc(tamanioMaximoDeRegistro);
	char* registro = NULL;
	bool completo = true;
	size_t len = 0;
	ssize_t read;
	for(int i=0;i<lengthListaDeBloques;i++){
		char* direccionDelBloque = direccionDeBloque(listaDeBloques[i]);
		FILE* archivo = fopen(direccionDelBloque, "r");
		if(archivo){
			while((read = getline(&registro, &len, archivo)) != EOF){

				if(!completo){
					strcpy(registroIncompleto + strlen(registroIncompleto), registro);
					free(registro);
					registro = string_duplicate(registroIncompleto);
				}
				if(registro != NULL && registroCompleto(registro)){ // registro != NULL && registroCompleto(registro)
					printf("registro = %s\n", registro);
					insertarRegistroEnLaLista(listaDeRegistros, registro);
					completo = true;
				}else{
					strcpy(registroIncompleto, registro);
					completo = false;
				}


				//else if(!completo){
			//		strcpy(registroIncompleto + strlen(registroIncompleto), registro);
			//	}else{
			//		strcpy(registroIncompleto, registro);
			//		completo = false;
			//	}

				free(registro);
				registro = NULL;
			}
			fclose(archivo);
		}
		else{
			error_show("No se pudo abrir el archivo");
		}

		free(direccionDelBloque);
	}

	return;
}

ESTRUCTURAS
typedef struct {
	uint32_t timestamp;
	uint16_t key;
	char* value;
} nodo_memtable;






rtaGet* operacion_get_Pokemon(int idMensaje, char* pokemon){
	pthread_mutex_lock(&mutexGET);
	rtaGet* respuesta = malloc(sizeof(rtaGet));
    respuesta->id_mensaje = idMensaje;
    respuesta->name = pokemon;
    DIR *dir;
	char *path_directorio_pokemon = string_new();
		string_append_with_format(&path_directorio_pokemon, "%s%s%s%s",datos_config->pto_de_montaje, FILES_DIR, "/", pokemon);
		log_info(logger, "EN BUSCA DEL DIRECTORIO DEL POKEMON CON PATH <%s>", path_directorio_pokemon);

		if((dir = opendir(path_directorio_pokemon)) == NULL) { // si existe o no el directorio (FAIL)
			log_error(logger, "EL DIRECTORIO <%s> NO EXISTE", path_directorio_pokemon);
		    //si no existe se devuelve el nombre y el id, los cuales ya estan cargados
		} else {
			char *valor = get_valor_campo_metadata(path_directorio_pokemon, "DIRECTORY");
			if(string_equals_ignore_case( valor, "Y")) { // si es un directorio, se envia al broker la respuesta (FAIL)
		    	log_info(logger, "LA RUTA <%s> ES SOLO UN DIRECTORIO ", path_directorio_pokemon);
		    } else {
		    	log_info(logger, "EL DIRECTORIO <%s> EXISTE JUNTO CON EL ARCHIVO POKEMON");
			
		    	//mutex_lock
    		char *valor_open = get_valor_campo_metadata(path_directorio_pokemon, "DIRECTORY");
    		if(string_equals_ignore_case(valor_open, "N")) {
    			cambiar_valor_metadata(path_directorio_pokemon, "OPEN", "Y");
    			// mutex_unlock
		
            
            char* pathFILE = string_new();
            string_append(&pathFILE,path_directorio_pokemon);
            string_append_with_format(&pathFILE,"%s%s%s", "/",pokemon, POKEMON_FILE_EXT);
			//log_info(logger, "Path pokemon %s", pathFILE);
			FILE *fp =fopen(pathFILE, "r");
			int fplen = fileSize(pathFILE);
            char* scaneo = NULL;
			//revisar como entran las cosas al buffer
            read_file_into_buf (scaneo, fp);
			//mutex_lock
			cambiar_valor_metadata(path_directorio_pokemon, "OPEN", "N");
			log_info(logger, "Cerro archivo pokemon");
    		// mutex_unlock
			free(pathFILE);
			free(fp);
            //falta separar uno a uno las lineas
			respuesta->posiYcant = list_create();
			log_info(logger, "Hizo los frees  y Crea lista");
       
			int desplazamiento = 0;
			while(desplazamiento < fplen){
			posYcant* posicionesCant = malloc(sizeof(posYcant));
			log_info(logger, "zona de memcpy");
	//TENGO PROBLEMAS CON LOS MEMCPY REVISAR
			memcpy(&(posicionesCant->posX), scaneo, sizeof(int));
			log_info(logger, "Entrando a memcpy");
			log_info(logger, "%d Posicione en x", posicionesCant->posX);
			desplazamiento = desplazamiento + sizeof(int) + sizeof(char);//el char del -
			memcpy(&(posicionesCant->posY), scaneo, sizeof(int));
			desplazamiento = desplazamiento + sizeof(int) + sizeof(char);//el char del =
			memcpy(&(posicionesCant->cant), scaneo, sizeof(int));
			list_add(respuesta->posiYcant, posicionesCant);
			desplazamiento = desplazamiento + sizeof(int) + 1; //por el /n
			free(posicionesCant);
			}
            
            free(scaneo);
			
		    } else{// en caso de que este abierto
				  //finalizar hilo y reintentar despues del tiempo que dice el config

				log_warning(logger, "HILO EN STANDBY DE OPERACION");
    			pthread_cancel(thread_get_pokemon);
				sleep(datos_config->tiempo_retardo_operacion);
COSAS DEL NUEVO HILO 		//	pthread_create(&atender_get_pokemon, NULL, (void *) operacion_get_pokemon, PARAMETROS);
    		//	pthread_join(atender_get_pokemon, NULL);
    			log_info(logger, "HILO RETOMANDO LA OPERACION");
				

}
				
		}

		    free(path_directorio_pokemon);
			
    }
	closedir(dir);
	pthread_mutex_unlock(&mutexGET);
    return respuesta;
}


*/
