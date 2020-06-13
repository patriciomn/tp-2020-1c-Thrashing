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

		pthread_create(&thread_new_pokemon, NULL, (void *)recibir_mensajes_new_pokemon, NULL);
		pthread_detach(thread_catch_pokemon);
		//pthread_join(thread_new_pokemon, NULL);

		pthread_create(&thread_catch_pokemon, NULL, (void *)recibir_mensajes_catch_pokemon, NULL);
		pthread_detach(thread_catch_pokemon);
		//pthread_join(thread_catch_pokemon, NULL);

		pthread_create(&thread_get_pokemon, NULL, (void *)recibir_mensajes_get_pokemon, NULL);
		pthread_detach(thread_get_pokemon);
		//pthread_join(thread_get_pokemon, NULL);

	}
}

void suscribirse_a_new_pokemon() {
	socket_cliente_np = crear_conexion();
	if(socket_cliente_np != -1) {
		enviar_mensaje_suscripcion(NEW_POKEMON, socket_cliente_np, pid_gamecard);
		recv(socket_cliente_np, &(acks_gamecard.ack_new), sizeof(int), MSG_WAITALL);
		log_info(logger, "ACK RECIVIDO PARA COLA NEW_POKEMON: %d", acks_gamecard.ack_new);
	}
}

void suscribirse_a_catch_pokemon() {
	socket_cliente_cp = crear_conexion();
	if(socket_cliente_cp != -1) {
		enviar_mensaje_suscripcion(CATCH_POKEMON, socket_cliente_cp, pid_gamecard);
		recv(socket_cliente_cp, &(acks_gamecard.ack_catch), sizeof(int), MSG_WAITALL);
		log_info(logger, "ACK RECIVIDO PARA COLA CATCH_POKEMON: %d", acks_gamecard.ack_catch);
	}
}

void suscribirse_a_get_pokemon() {
	socket_cliente_gp = crear_conexion();
	if(socket_cliente_gp != -1) {
		enviar_mensaje_suscripcion(GET_POKEMON, socket_cliente_gp, pid_gamecard);
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
			// pthread_create(&thread_new_pokemon, NULL , NULL, NULL);
			// pthread_detach(thread_new_pokemon);

			free(catch_pokemon->nombre);
			free(catch_pokemon);
			free(valor);
			enviar_ack(CATCH_POKEMON, catch_pokemon->id_mensaje, pid_gamecard, socket_cliente_cp);
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
			NPokemon *newPokemon = malloc(sizeof(NPokemon));
			deserealizar_new_pokemon_gameboy(stream, newPokemon);

			printf("ID MENSAJE: %d\n", newPokemon->id_mensaje);
			printf("POKEMON: %s\n", newPokemon->nombre);
			printf("PosX: %d\n", newPokemon->posicion.posX);
			printf("PosY: %d\n", newPokemon->posicion.posY);
			printf("Cantidad: %d\n", newPokemon->cantidad);

			free(newPokemon);
			free(stream);
		break;

		case CATCH_POKEMON:
			log_info(logger, "SE RECIBIO UN MENSAJE CON OPERACION CATCH_POKEMON");
			CPokemon *catchPokemon = malloc(sizeof(CPokemon));
			deserealizar_catch_pokemon_gameboy(stream, catchPokemon);

			printf("ID MENSAJE: %d\n", catchPokemon->id_mensaje);
			printf("POKEMON: %s\n", catchPokemon->nombre);
			printf("PosX: %d\n", catchPokemon->posicion.posX);
			printf("PosY: %d\n", catchPokemon->posicion.posY);

			free(catchPokemon);
			free(stream);
		break;

		case GET_POKEMON:
			log_info(logger, "SE RECIBIO UN MENSAJE CON OPERACION GET_POKEMON");
			GPokemon *getPokemon = malloc(sizeof(GPokemon));
			deserealizar_get_pokemon_gameboy(stream, getPokemon);

			printf("ID MENSAJE: %d\n", getPokemon->id_mensaje);
			printf("POKEMON: %s\n", getPokemon->nombre);

			free(getPokemon);
			free(stream);
		break;

		default:
			log_warning(logger, "NO SE RECIBIO NINGUNA DE LAS ANTERIORES");
		break;
	}
}

// Deserealizacion para Gameboy

void deserealizar_new_pokemon_gameboy(void *stream, NPokemon *newPokemon) {
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
