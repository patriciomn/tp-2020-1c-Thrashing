#include"utils.h"


void* serializar_paquete(t_paquete* paquete, int * bytes){
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
	new->name = malloc(new->name_size);

	memcpy(new->name, stream, new->name_size);
	stream += sizeof(new->name_size);
	
    memcpy(&(new->pos), stream, sizeof(position));
    stream += sizeof(position);

    memcpy(&(new->cantidad), stream, sizeof(int));
	
    return new;
}

appeared_pokemon* deserializar_appeared(void* buffer) {
    appeared_pokemon* appeared = malloc(sizeof(appeared_pokemon));
    
    void* stream = buffer;
    memcpy(&(appeared->name_size), stream, sizeof(int));
    stream += sizeof(int);
	appeared->name = malloc(appeared->name_size);
	memcpy(appeared->name, stream, appeared->name_size);
	stream += sizeof(appeared->name_size);

    memcpy(&(appeared->pos), stream, sizeof(position));
    stream += sizeof(position);

    return appeared;
}

catch_pokemon* deserializar_catch(void* buffer) {
    catch_pokemon* catch = malloc(sizeof(catch_pokemon));
    
    void* stream = buffer;
    memcpy(&(catch->name_size), stream, sizeof(int));
    stream += sizeof(int);
	catch->name = malloc(catch->name_size);
	memcpy(catch->name, stream, catch->name_size);
	stream += sizeof(catch->name_size);
    memcpy(&(catch->pos), stream, sizeof(position));
    stream += sizeof(position);
		
    return catch;
}

caught_pokemon* deserializar_caught(void* buffer) {
    caught_pokemon* caught = malloc(sizeof(caught_pokemon));
    
    memcpy(&(caught->caught), buffer, sizeof(int));
		
    return caught;
}

get_pokemon* deserializar_get(void* buffer) {
    get_pokemon* get = malloc(sizeof(get_pokemon));
    
    void* stream = buffer;
    memcpy(&(get->name_size), stream, sizeof(int));
    stream += sizeof(int);
	get->name = malloc(get->name_size);
	memcpy(get->name, stream, get->name_size);
		
    return get;
}


localized_pokemon* deserializar_localized(void* buffer) {
    localized_pokemon* localized = malloc(sizeof(localized_pokemon));
    
    void* stream = buffer;
    memcpy(&(localized->name_size), stream, sizeof(int));
    stream += sizeof(int);
	localized->name = malloc(localized->name_size);

	memcpy(localized->name, stream, localized->name_size);
	stream += sizeof(localized->name_size);
    memcpy(&(localized->pos), stream, sizeof(position));
    stream += sizeof(position);

    memcpy(&(localized->cantidad_posiciones), stream, sizeof(int));
	
    return localized;
}