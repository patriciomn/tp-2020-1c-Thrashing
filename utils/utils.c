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

	memcpy(magic + desplazamiento, &(paquete->codigo_operacion), sizeof(int));
	desplazamiento+= sizeof(int);
	memcpy(magic + desplazamiento, &(paquete->buffer->size), sizeof(int));
	desplazamiento+= sizeof(int);
	memcpy(magic + desplazamiento, paquete->buffer->stream, paquete->buffer->size);
	desplazamiento+= paquete->buffer->size;

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

void* serializar_paq(t_paquete* paquete, int * bytes){
	int size_serializado = sizeof(paquete->codigo_operacion) + sizeof(paquete->buffer->id) + sizeof(paquete->buffer->correlation_id);
    
	paquete->buffer->stream = serializar_any(paquete, & paquete->buffer->size, paquete->codigo_operacion);

    size_serializado += paquete->buffer->size + sizeof(paquete->buffer->size);

	void * magic = malloc(size_serializado );
	int desplazamiento = 0;

	memcpy(magic + desplazamiento, &(paquete->codigo_operacion), sizeof(int));
	desplazamiento+= sizeof(int);
	memcpy(magic + desplazamiento, &(paquete->buffer->id), sizeof(int));
	desplazamiento+= sizeof(int);

	memcpy(magic + desplazamiento, &(paquete->buffer->correlation_id), sizeof(int));
	desplazamiento+= sizeof(int);
	memcpy(magic + desplazamiento, &(paquete->buffer->size), sizeof(int));
	desplazamiento+= sizeof(int);
	memcpy(magic + desplazamiento, paquete->buffer->stream, paquete->buffer->size);

	(*bytes) = size_serializado;

	return magic;
}

void* serializar_any(void* paquete, int * bytes, int cod_op){
	if (cod_op == NEW_POKEMON)				return serializar_new(paquete, bytes);
	else if (cod_op == APPEARED_POKEMON)	return serializar_appeared(paquete, bytes);
	else if (cod_op == CATCH_POKEMON)		return serializar_catch(paquete, bytes);
	else if (cod_op == CAUGHT_POKEMON)		return serializar_caught(paquete, bytes);
	else if (cod_op == GET_POKEMON)			return serializar_get(paquete, bytes);
	else if (cod_op == LOCALIZED_POKEMON)	return serializar_localized(paquete, bytes);
	else return paquete;
}



void* serializar_new(new_pokemon* paquete, int * bytes){

	int size_serializado = sizeof(paquete->name_size)  + paquete->name_size + sizeof(position) + sizeof(paquete->cantidad);

	void * magic = malloc(size_serializado );
	int desplazamiento = 0;

	memcpy(magic + desplazamiento, &(paquete->name_size), sizeof(paquete->name_size));
	desplazamiento+= sizeof(paquete->name_size);

    memcpy(magic + desplazamiento, paquete->name, paquete->name_size);
	desplazamiento+= paquete->name_size;

	memcpy(magic + desplazamiento, &(paquete->pos), sizeof(position));
	desplazamiento+= sizeof(position);

    memcpy(magic + desplazamiento, &(paquete->cantidad), sizeof(paquete->cantidad));
	desplazamiento+= sizeof(paquete->cantidad);
	

	(*bytes) = size_serializado;

	return magic;
}

void* serializar_appeared(appeared_pokemon* paquete, int * bytes){

	int size_serializado = sizeof(paquete->name_size)  + paquete->name_size + sizeof(position) ;

	void * magic = malloc(size_serializado );
	int desplazamiento = 0;

	memcpy(magic + desplazamiento, &(paquete->name_size), sizeof(paquete->name_size));
	desplazamiento+= sizeof(paquete->name_size);

    memcpy(magic + desplazamiento, paquete->name, paquete->name_size);
	desplazamiento+= paquete->name_size;

	memcpy(magic + desplazamiento, &(paquete->pos), sizeof(position));
	desplazamiento+= sizeof(position);
	

	(*bytes) = size_serializado;

	return magic;
}

void* serializar_catch(catch_pokemon* paquete, int * bytes){

	int size_serializado = sizeof(paquete->name_size)  + paquete->name_size + sizeof(position) ;

	void * magic = malloc(size_serializado );
	int desplazamiento = 0;

	memcpy(magic + desplazamiento, &(paquete->name_size), sizeof(paquete->name_size));
	desplazamiento+= sizeof(paquete->name_size);

    memcpy(magic + desplazamiento, paquete->name, paquete->name_size);
	desplazamiento+= paquete->name_size;

	memcpy(magic + desplazamiento, &(paquete->pos), sizeof(position));
	desplazamiento+= sizeof(position);

	
	(*bytes) = size_serializado;

	return magic;
}

void* serializar_caught(caught_pokemon* paquete, int * bytes){

	int size_serializado = sizeof(paquete->caught);

	void * magic = malloc(size_serializado );
	int desplazamiento = 0;

	memcpy(magic + desplazamiento, &(paquete->caught), sizeof(paquete->caught));
	desplazamiento+= sizeof(paquete->caught);
	
	(*bytes) = size_serializado;

	return magic;
}




void* serializar_get(get_pokemon* paquete, int * bytes){

	int size_serializado = sizeof(paquete->name_size)  + paquete->name_size ;

	void * magic = malloc(size_serializado );
	int desplazamiento = 0;

	memcpy(magic + desplazamiento, &(paquete->name_size), sizeof(paquete->name_size));
	desplazamiento+= sizeof(paquete->name_size);

    memcpy(magic + desplazamiento, paquete->name, paquete->name_size);
	desplazamiento+= paquete->name_size;


	
	(*bytes) = size_serializado;

	return magic;
}

void* serializar_localized(localized_pokemon* paquete, int * bytes){

	int size_serializado = sizeof(paquete->name_size)  + paquete->name_size + sizeof(position) + sizeof(paquete->cantidad_posiciones);

	void * magic = malloc(size_serializado );
	int desplazamiento = 0;

	memcpy(magic + desplazamiento, &(paquete->name_size), sizeof(paquete->name_size));
	desplazamiento+= sizeof(paquete->name_size);

    memcpy(magic + desplazamiento, paquete->name, paquete->name_size);
	desplazamiento+= paquete->name_size;

	memcpy(magic + desplazamiento, &(paquete->pos), sizeof(position));
	desplazamiento+= sizeof(position);

    memcpy(magic + desplazamiento, &(paquete->cantidad_posiciones), sizeof(paquete->cantidad_posiciones));
	desplazamiento+= sizeof(paquete->cantidad_posiciones);
	
	(*bytes) = size_serializado;

	return magic;
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
    
    void* stream = buffer+sizeof(int);
    memcpy(&(appeared->name_size), stream, sizeof(int));
    stream += sizeof(int);
	appeared->name = malloc(appeared->name_size+1);
	memcpy(appeared->name, stream, appeared->name_size+1);
	stream += (appeared->name_size+1);
    memcpy(&(appeared->pos), stream, sizeof(position));

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
    memcpy(&(caught->caught), buffer+sizeof(int), sizeof(int));
		
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
    
    void* stream = buffer+sizeof(int);
    memcpy(&(localized->name_size), stream, sizeof(int));
    stream += sizeof(int);
	localized->name = malloc(localized->name_size+1);
	memcpy(localized->name, stream, localized->name_size+1);
	stream += sizeof(localized->name_size+1);
	memcpy(&localized->cantidad_posiciones,stream,sizeof(int));
	stream += sizeof(int);
	for(int i=0;i<localized->cantidad_posiciones;i++){
		memcpy(&(localized->pos[i]), stream, sizeof(position));
		stream += sizeof(position);
	}
	
    return localized;
}
