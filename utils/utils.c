#include"utils.h"

void* serializar_paquete(t_paquete* paquete, int * bytes){

	int size_serializado = sizeof(paquete->queue_id) + sizeof(paquete->buffer->size) + paquete->buffer->size;

	void * magic = malloc(size_serializado );
	int desplazamiento = 0;

	memcpy(magic + desplazamiento, &(paquete->queue_id), sizeof(int));
	desplazamiento+= sizeof(int);
	memcpy(magic + desplazamiento, &(paquete->buffer->size), sizeof(int));
	desplazamiento+= sizeof(int);
	memcpy(magic + desplazamiento, paquete->buffer->stream, paquete->buffer->size);
	desplazamiento+= paquete->buffer->size;

	(*bytes) = size_serializado;

	return magic;
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
void* serializar_localized(appeared_pokemon* paquete, int * bytes){

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
