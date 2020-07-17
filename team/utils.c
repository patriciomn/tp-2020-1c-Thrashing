#include"utils.h"

t_config* leer_config(char* config){
	return config_create(config);
}

void* recibir_mensaje(int socket_cliente){
	int size;
	void * buffer;
	recv(socket_cliente,&size, sizeof(int), MSG_WAITALL);
	buffer = malloc(size);
	recv(socket_cliente, buffer,size, MSG_WAITALL);
	return buffer;
}

void* serializar_paquete(t_paquete* paquete, int bytes){
	void * magic = malloc(bytes);
	int desplazamiento = 0;
	memset(magic,0,bytes);

	memcpy(magic + desplazamiento, &(paquete->codigo_operacion), sizeof(int));
	desplazamiento+= sizeof(int);
	memcpy(magic + desplazamiento, &(paquete->buffer->size), sizeof(int));
	desplazamiento+= sizeof(int);
	memcpy(magic + desplazamiento, paquete->buffer->stream, paquete->buffer->size);

	return magic;
}

void crear_buffer(t_paquete* paquete){
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = 0;
	paquete->buffer->stream = NULL;
}

t_paquete* crear_paquete(int op){
	t_paquete* paquete = malloc(sizeof(t_paquete));
	crear_buffer(paquete);
	paquete->codigo_operacion = op;
	return paquete;
}

void agregar_a_paquete(t_paquete* paquete, void* valor, int tamanio){
	paquete->buffer->stream = realloc(paquete->buffer->stream, paquete->buffer->size + tamanio + sizeof(int));

	memcpy(paquete->buffer->stream + paquete->buffer->size, &tamanio, sizeof(int));
	memcpy(paquete->buffer->stream + paquete->buffer->size + sizeof(int), valor, tamanio);

	paquete->buffer->size += tamanio + sizeof(int);
}

void enviar_paquete(t_paquete* paquete, int socket_cliente){
	int bytes = paquete->buffer->size + 2*sizeof(int);
	void* a_enviar = serializar_paquete(paquete, bytes);
	send(socket_cliente, a_enviar, bytes, 0);

	free(a_enviar);
}

void eliminar_paquete(t_paquete* paquete){
	free(paquete->buffer->stream);
	free(paquete->buffer);
	free(paquete);
}

new_pokemon* deserializar_new(void* buffer) {
    new_pokemon* new = malloc(sizeof(new_pokemon));

    void* stream = buffer;
    memcpy(&(new->name_size), stream, sizeof(int));
    stream += sizeof(int);
	new->name = malloc(new->name_size+1);
	memcpy(new->name, stream, new->name_size+1);
	stream += (new->name_size + 1);
    memcpy(&(new->pos), stream, sizeof(position));
    stream += sizeof(position);
    memcpy(&(new->cantidad), stream, sizeof(int));
	
    return new;
}

appeared_pokemon* deserializar_appeared(void* buffer) {
    appeared_pokemon* appeared = malloc(sizeof(appeared_pokemon));
    
    memcpy(&(appeared->name_size), buffer, sizeof(int));
	appeared->name = malloc(appeared->name_size+1);
	memcpy(appeared->name, buffer+sizeof(int), appeared->name_size+1);
    memcpy(&(appeared->pos), buffer+sizeof(int)+(appeared->name_size+1), sizeof(position));

    return appeared;
}

catch_pokemon* deserializar_catch(void* buffer) {
    catch_pokemon* catch = malloc(sizeof(catch_pokemon));
    
    void* stream = buffer;
    memcpy(&(catch->name_size), stream, sizeof(int));
    stream += sizeof(int);
	catch->name = malloc(catch->name_size+1);
	memcpy(catch->name, stream, catch->name_size+1);
	stream += (catch->name_size+1);
    memcpy(&(catch->pos), stream, sizeof(position));
		
    return catch;
}

caught_pokemon* deserializar_caught(void* buffer) {
    caught_pokemon* caught = malloc(sizeof(caught_pokemon));
    void* stream = buffer+sizeof(int);
    memcpy(&(caught->caught),stream, sizeof(bool));
		
    return caught;
}

get_pokemon* deserializar_get(void* buffer) {
    get_pokemon* get = malloc(sizeof(get_pokemon));

    memcpy(&(get->name_size),buffer, sizeof(int));
	get->name = malloc(get->name_size+1);
	memcpy(get->name,buffer+sizeof(int), get->name_size+1);
		
    return get;
}

localized_pokemon* deserializar_localized(void* buffer) {
    localized_pokemon* localized = malloc(sizeof(localized_pokemon));
    localized->posiciones = list_create();

    void* stream = buffer+sizeof(int);
    memcpy(&(localized->name_size), stream, sizeof(int));
    stream += sizeof(int);
	localized->name = malloc(localized->name_size+1);
	memcpy(localized->name, stream, localized->name_size+1);
	stream += (localized->name_size+1);
	memcpy(&localized->cantidad_posiciones,stream,sizeof(int));
	stream += sizeof(int);
	for(int i=0;i<localized->cantidad_posiciones;i++){
		position* pc = malloc(sizeof(position));
		memcpy(pc,stream, sizeof(position));
		list_add(localized->posiciones,pc);
		stream += sizeof(position);
	}
	
    return localized;
}

void enviar_ack(int tipo,int id,pid_t pid,int cliente_fd){
	t_paquete* paquete = malloc(sizeof(t_paquete));
	paquete->codigo_operacion = tipo;
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = sizeof(int)+sizeof(pid_t);
	paquete->buffer->stream = malloc(paquete->buffer->size);
	memcpy(paquete->buffer->stream,&id,sizeof(int));
	memcpy(paquete->buffer->stream+sizeof(int),&pid, sizeof(pid_t));

	int bytes = paquete->buffer->size + 2*sizeof(int);

	void* a_enviar = serializar_paquete(paquete, bytes);

	if(check_socket(cliente_fd) == 1){
		send(cliente_fd, a_enviar, bytes,0);
		printf("ACK enviado\n");
	}

	free(a_enviar);
	free(paquete->buffer->stream);
	free(paquete->buffer);
	free(paquete);
}

void enviar_info_suscripcion(int tipo,int socket_cliente,pid_t pid){
	t_paquete* paquete = malloc(sizeof(t_paquete));
	paquete->codigo_operacion = SUSCRITO;
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = sizeof(int)+sizeof(pid_t);
	paquete->buffer->stream = malloc(paquete->buffer->size);
	memcpy(paquete->buffer->stream, &tipo, sizeof(int));
	memcpy(paquete->buffer->stream+sizeof(int),&pid,sizeof(pid_t));

	int bytes = paquete->buffer->size + 2*sizeof(int);
	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes,0);

	free(a_enviar);
	free(paquete->buffer->stream);
	free(paquete->buffer);
	free(paquete);
}

t_list* recibir_paquete(int socket_cliente){
	int size;
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

void* recibir_buffer(int socket_cliente, int* size){
	void * buffer;

	recv(socket_cliente, size, sizeof(int), MSG_WAITALL);
	buffer = malloc(*size);
	recv(socket_cliente, buffer, *size, MSG_WAITALL);

	return buffer;
}

int recibir_confirmacion_suscripcion(int cliente_fd,int tipo){
	int cod_op;
	if(recv(cliente_fd, &cod_op, sizeof(int), MSG_WAITALL) == -1)
		cod_op = -1;
	if(cod_op != -1){
		printf("\033[1;33mSuscrito A La Cola %s\033[0m\n",get_cola(tipo));
	}
	return cod_op;
}

int crear_conexion(char *ip, char* puerto){
	struct addrinfo hints;
	struct addrinfo *server_info;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	getaddrinfo(ip, puerto, &hints, &server_info);

	int socket_cliente = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);

	if(connect(socket_cliente, server_info->ai_addr, server_info->ai_addrlen) == -1){
		socket_cliente = -1;
	}

	freeaddrinfo(server_info);
	return socket_cliente;
}

void liberar_conexion(int socket_cliente){
	close(socket_cliente);
}

bool check_socket(int sock){
    unsigned char buf;
    int err = recv(sock,&buf,1,MSG_DONTWAIT|MSG_PEEK);
    fflush(stdin);
    return err;
}

int recibir_id_mensaje(int cliente_fd){
	int id;
	void* buffer = malloc(sizeof(int));
	recv(cliente_fd,buffer,sizeof(buffer),MSG_WAITALL);
	memcpy(&id,buffer,sizeof(int));
	free(buffer);
	return id;
}

char* get_cola(uint32_t tipo){
	char* c;
	switch(tipo){
		case NEW_POKEMON:
			c = "NEW";
			break;
		case GET_POKEMON:
			c = "GET";
			break;
		case CAUGHT_POKEMON:
			c= "CAUGHT";
			break;
		case CATCH_POKEMON:
			c= "CATCH";
			break;
		case APPEARED_POKEMON:
			c= "APPEARED";
			break;
		case LOCALIZED_POKEMON:
			c= "LOCALIZED";
			break;
	}
	return c;
}
