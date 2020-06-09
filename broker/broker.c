#include"broker.h"

t_log* logger;
t_config* config;
config_broker* datos_config;
mq* cola_get;
mq* cola_localized;
mq* cola_catch;
mq* cola_caught;
mq* cola_new;
mq* cola_appeared;
pthread_t thread_servidor;
pthread_t thread_suscripcion;
pthread_t thread_ack;

int cant_busqueda;
uint32_t cache_size;
uint32_t min_size;
uint32_t mem_asignada;
uint32_t mem_total;
uint32_t id_particion;
t_list* cache;//una lista de particiones
void* memoria;
pthread_rwlock_t lockSus = PTHREAD_RWLOCK_INITIALIZER;
pthread_rwlock_t lockCache = PTHREAD_RWLOCK_INITIALIZER;
pthread_rwlock_t lockCola = PTHREAD_RWLOCK_INITIALIZER;
pthread_rwlock_t lockMemAsignada = PTHREAD_RWLOCK_INITIALIZER;
pthread_rwlock_t lockParticion = PTHREAD_RWLOCK_INITIALIZER;


int main(){	
    iniciar_broker();
	terminar_broker( logger, config);
}

void sig_handler(int signo){
	handler_dump(SIGUSR1);
}

void iniciar_broker(void){
	logger = log_create("broker.log","broker",1,LOG_LEVEL_INFO);
	iniciar_config("broker.config");
	iniciar_colas_mensaje();
	log_info(logger,"BROKER START!");
	iniciar_memoria();
	iniciar_servidor();
	signal(SIGINT,sig_handler);
	log_info(logger,"creating server");
}

void iniciar_semaforos(){

}

mq* crear_cola_mensaje(int tipo){
	mq* message_queue = malloc(sizeof(mq));
	message_queue->tipo_queue = tipo;
	message_queue->mensajes = list_create();
	message_queue->suscriptors = list_create();
	message_queue->id = 0;
	return message_queue;
}

void iniciar_colas_mensaje(){
	cola_new = crear_cola_mensaje(NEW_POKEMON);
	cola_appeared = crear_cola_mensaje(APPEARED_POKEMON);
	cola_catch = crear_cola_mensaje(CATCH_POKEMON);
	cola_caught = crear_cola_mensaje(CAUGHT_POKEMON);
	cola_get = crear_cola_mensaje(GET_POKEMON);
	cola_localized = crear_cola_mensaje(LOCALIZED_POKEMON);
}

mensaje* crear_mensaje(int tipo_msg,int socket,int nro_id,void* msg){
	mensaje* m = malloc(sizeof(mensaje));
	m->tipo_msg = tipo_msg;
	m->id = nro_id;
	m->suscriptors_enviados = list_create();
	m->suscriptors_ack = list_create();
	m->msj = msg;
	log_info(logger,"Nuevo Mensaje Tipo_Mensaje:%d ID_Mensaje:%d",m->tipo_msg,m->id);
	return m;
}

void iniciar_config(char* memoria_config){
	config = leer_config(memoria_config);
	datos_config = malloc(sizeof(config_broker));
	datos_config->ip_broker = config_get_string_value(config,"IP_BROKER");
	datos_config->puerto_broker = config_get_string_value(config,"PUERTO_BROKER");
	datos_config->tamanio_memoria = atoi(config_get_string_value(config,"TAMANO_MEMORIA"));
	datos_config->tamanio_min_compactacion = atoi(config_get_string_value(config,"TAMANO_MINIMO_PARTICION"));
	datos_config->algoritmo_memoria = config_get_string_value(config,"ALGORITMO_MEMORIA");
	datos_config->algoritmo_particion_libre = config_get_string_value(config,"ALGORITMO_PARTICION_LIBRE");
	datos_config->algoritmo_reemplazo = config_get_string_value(config,"ALGORITMO_REEMPLAZO");
	datos_config->frecuencia_compactacion = atoi(config_get_string_value(config,"FRECUENCIA_COMPACTACION"));
}

void iniciar_servidor(void){
	int socket_servidor;
    struct addrinfo hints, *servinfo, *p;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    getaddrinfo(datos_config->ip_broker, datos_config->puerto_broker, &hints, &servinfo);

    for (p=servinfo; p != NULL; p = p->ai_next){
        if ((socket_servidor = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
            continue;

        if (bind(socket_servidor, p->ai_addr, p->ai_addrlen) == -1) {
            close(socket_servidor);
            continue;
        }
        break;
    }

	listen(socket_servidor, SOMAXCONN);

    freeaddrinfo(servinfo);

    while(1)
    	esperar_cliente(socket_servidor);
}

void esperar_cliente(int socket_servidor){
	struct sockaddr_in dir_cliente;

	socklen_t tam_direccion = sizeof(struct sockaddr_in);
	int socket_cliente = accept(socket_servidor, (void*) &dir_cliente, &tam_direccion);
	int* cliente_fd = malloc(sizeof(int));
	*cliente_fd = socket_cliente;
	log_info(logger,"Proceso Conectado Con Socket %d",*cliente_fd);
	pthread_create(&thread_servidor,NULL,(void*)serve_client,cliente_fd);
	pthread_detach(thread_servidor);
}

void serve_client(int* socket){
	int cod_op;

	if(recv(*socket, &cod_op, sizeof(int), MSG_WAITALL) == -1){
		log_error(logger,"ERROR: socket error" );
		pthread_exit(NULL);	
	}
		
	if (cod_op <= 0 ){
		log_error(logger,"ERROR: Bad operation code: %d", cod_op);
		pthread_exit(NULL);
	}	
	int cliente_fd = *socket;
	free(socket);
	printf("Enter to process request, cod_op: %d\n", cod_op);
	process_request(cod_op, cliente_fd);
}

void process_request(int cod_op, int cliente_fd) {
	if (cod_op == SUSCRITO){
		atender_suscripcion(cliente_fd);
	}
	else if (cod_op == ACK){
		atender_ack(cliente_fd);
	}
	else{
		void* msg = recibir_mensaje(cliente_fd);

		if (cod_op == NEW_POKEMON){
			mensaje_new_pokemon(msg,cliente_fd);
		}
		else if (cod_op == APPEARED_POKEMON){
			mensaje_appeared_pokemon(msg,cliente_fd);
		}
		else if (cod_op == CATCH_POKEMON){
			mensaje_catch_pokemon(msg,cliente_fd);
		}
		else if (cod_op == CAUGHT_POKEMON){
			mensaje_caught_pokemon(msg,cliente_fd);
		}
		else if (cod_op == GET_POKEMON){
			mensaje_get_pokemon(msg,cliente_fd);
		}
		else if (cod_op == LOCALIZED_POKEMON){
			mensaje_localized_pokemon(msg,cod_op);
		}
		display_cache();
		free(msg);

		mq* cola = cola_mensaje(cod_op);
		if(!list_is_empty(cola->suscriptors)){
			void enviar(suscriber* sus){
				enviar_mensajes(sus,cod_op);
			}
			list_iterate(cola->suscriptors,(void*)enviar);
		}
	}
}

mq* cola_mensaje(uint32_t tipo){
	switch(tipo){
		case NEW_POKEMON:
			return cola_new;
			break;
		case GET_POKEMON:
			return cola_get;
			break;
		case CAUGHT_POKEMON:
			return cola_caught;
			break;
		case CATCH_POKEMON:
			return cola_catch;
			break;
		case APPEARED_POKEMON:
			return cola_appeared;
			break;
		case LOCALIZED_POKEMON:
			return cola_localized;
			break;
	}
	return 0;
}

void atender_suscripcion(int cliente_fd){
	void * msg = recibir_mensaje(cliente_fd);
	int queue_id;
	uuid_t pid;
	memcpy(&queue_id,msg,sizeof(uint32_t));
	memcpy(&pid,msg+sizeof(uint32_t),sizeof(uuid_t));
	char buf[1024];
	uuid_unparse(pid,buf);
	printf("Suscripcion:Pid:%s|Queue_id:%d\n",buf,queue_id);

	suscriber * sus = malloc(sizeof(suscriber));
	sus->cliente_fd = cliente_fd;
	uuid_copy(sus->pid,pid);

	mq* cola = cola_mensaje(queue_id);
	bool existe(suscriber* aux){
		return uuid_compare(aux->pid,pid) == 0;
	}
	if (!list_any_satisfy(cola->suscriptors,(void*)existe) ){
		list_add(cola->suscriptors, sus);
		log_info(logger,"Proceso %s Suscripto A La Cola %d SUSCRIPCION",buf,queue_id);
	}
	enviar_confirmacion_suscripcion(sus);

	if(!list_is_empty(cola->mensajes)){
		enviar_mensajes(sus,queue_id);
	}
}

void atender_ack(int cliente_fd){
	uuid_t pid;
	int queue_id, id;
	void* msg = recibir_mensaje(cliente_fd);

	memcpy(&(pid), msg, sizeof(uuid_t));
	memcpy(&(queue_id), msg+sizeof(uuid_t), sizeof(int));
	memcpy(&(id), msg+sizeof(uuid_t)+sizeof(int), sizeof(int));
	char buf[1024];
	uuid_unparse(pid,buf);
	printf("ACK Recibido:Pid:%s,Tipo:%d,ID:%d\n",buf,queue_id,id);

	bool existe(mensaje* aux){
		return aux->id == id;
	}

	bool by_pid(suscriber* aux){
		return uuid_compare(aux->pid,pid) == 0;
	}

	mq* cola = cola_mensaje(queue_id);

	pthread_rwlock_rdlock(&lockCola);
	suscriber* sus = list_find(cola->suscriptors,(void*)by_pid);

	mensaje* item = list_find(cola->mensajes,(void*)existe);
	pthread_rwlock_unlock(&lockCola);
	list_add(item->suscriptors_ack,sus);
	borrar_mensaje(item);
}

void borrar_mensaje(mensaje* m){
	bool by_id_tipo(particion* aux){
		return aux->id_buffer == m->id && aux->tipo_cola == m->tipo_msg;
	}
	pthread_rwlock_rdlock(&lockCache);
	particion* borrar = list_find(cache,(void*)by_id_tipo);
	pthread_rwlock_unlock(&lockCache);
	delete_particion(borrar);
}

void mensaje_new_pokemon(void* msg,int cliente_fd){
	new_pokemon* new = deserializar_new(msg);
	printf("Mensaje NEW_POKEMON:Pokemon:%s|Size:%d|Pos:[%d,%d]|Cantidad:%d\n",new->name,new->name_size,new->pos.posx,new->pos.posy,new->cantidad);
	pthread_rwlock_wrlock(&lockCola);
	mensaje* item = crear_mensaje(NEW_POKEMON,cliente_fd,cola_new->id,new);
	cola_new->id++;
	pthread_rwlock_unlock(&lockCola);

	//almacena
	particion* part_aux = malloc_cache(new->name_size+sizeof(uint32_t)*4);
	memcpy_cache(part_aux,item->id,NEW_POKEMON,part_aux->inicio,&new->name_size,sizeof(uint32_t));
	memcpy_cache(part_aux,item->id,NEW_POKEMON,part_aux->inicio+sizeof(uint32_t),new->name,new->name_size);
	memcpy_cache(part_aux,item->id,NEW_POKEMON,part_aux->inicio+sizeof(uint32_t)+new->name_size,&new->pos.posx,sizeof(uint32_t));
	memcpy_cache(part_aux,item->id,NEW_POKEMON,part_aux->inicio+sizeof(uint32_t)*2+new->name_size,&new->pos.posy,sizeof(uint32_t));
	memcpy_cache(part_aux,item->id,NEW_POKEMON,part_aux->inicio+sizeof(uint32_t)*3+new->name_size,&new->cantidad,sizeof(uint32_t));
	log_warning(logger,"MENSAJE_ID:%d NEW_POKEMON Almacenado En El Cache Inicio:%p",item->id,part_aux->inicio);
	pthread_rwlock_wrlock(&lockCola);
	list_add(cola_new->mensajes,item);
	pthread_rwlock_unlock(&lockCola);
	enviar_id(item,cliente_fd);
}

void mensaje_appeared_pokemon(void* msg,int cliente_fd){
	int correlation_id;
	memcpy(&correlation_id, msg, sizeof(int));
	appeared_pokemon* appeared = deserializar_appeared(msg);
	pthread_rwlock_wrlock(&lockCola);
	mensaje* item = crear_mensaje(APPEARED_POKEMON,cliente_fd,cola_appeared->id,appeared);
	cola_appeared->id++;
	pthread_rwlock_unlock(&lockCola);
	item->id_correlacional = correlation_id;
	printf("Mensaje APPEARED_POKEMON:Correlation_id:%d|Pokemon:%s|Size:%d|Pos:[%d,%d]\n",item->id_correlacional,appeared->name,appeared->name_size,appeared->pos.posx,appeared->pos.posy);

	//almacena
	particion* part_aux = malloc_cache(appeared->name_size+sizeof(uint32_t)*4);
	memcpy_cache(part_aux,item->id,NEW_POKEMON,part_aux->inicio,&appeared->name_size,sizeof(uint32_t));
	memcpy_cache(part_aux,item->id,NEW_POKEMON,part_aux->inicio+sizeof(uint32_t),appeared->name,appeared->name_size);
	memcpy_cache(part_aux,item->id,NEW_POKEMON,part_aux->inicio+sizeof(uint32_t)+appeared->name_size,&appeared->pos.posx,sizeof(uint32_t));
	memcpy_cache(part_aux,item->id,NEW_POKEMON,part_aux->inicio+sizeof(uint32_t)*2+appeared->name_size,&appeared->pos.posy,sizeof(uint32_t));
	log_warning(logger,"MENSAJE_ID:%d APPEARED_POKEMON Almacenado En El Cache Inicio:%p",item->id,part_aux->inicio);
	pthread_rwlock_wrlock(&lockCola);
	list_add(cola_appeared->mensajes,item);
	pthread_rwlock_unlock(&lockCola);
	enviar_id(item,cliente_fd);
}

void mensaje_catch_pokemon(void* msg,int cliente_fd){
	catch_pokemon* catch = deserializar_catch(msg);
	printf("Mensaje CATCH_POKEMON:Pokemon:%s|Size:%d|Pos:[%d,%d]\n",catch->name,catch->name_size,catch->pos.posx,catch->pos.posy);
	pthread_rwlock_wrlock(&lockCola);
	mensaje* item = crear_mensaje(CATCH_POKEMON,cliente_fd,cola_catch->id,catch);
	cola_catch->id++;
	pthread_rwlock_unlock(&lockCola);

	//almacena
	particion* part_aux = malloc_cache(catch->name_size+sizeof(uint32_t)*3);
	memcpy_cache(part_aux,item->id,CATCH_POKEMON,part_aux->inicio,&catch->name_size,sizeof(uint32_t));
	memcpy_cache(part_aux,item->id,CATCH_POKEMON,part_aux->inicio+sizeof(uint32_t),catch->name,catch->name_size);
	memcpy_cache(part_aux,item->id,CATCH_POKEMON,part_aux->inicio+sizeof(uint32_t)+catch->name_size,&catch->pos.posx,sizeof(uint32_t));
	memcpy_cache(part_aux,item->id,CATCH_POKEMON,part_aux->inicio+sizeof(uint32_t)*2+catch->name_size,&catch->pos.posx,sizeof(uint32_t));
	log_warning(logger,"MENSAJE_ID:%d CATCH_POKEMON Almacenado En El Cache Inicio:%p",item->id,part_aux->inicio);
	pthread_rwlock_wrlock(&lockCola);
	list_add(cola_catch->mensajes,item);
	pthread_rwlock_unlock(&lockCola);
	enviar_id(item,cliente_fd);
}

void mensaje_caught_pokemon(void* msg,int cliente_fd){
	int correlation_id;
	memcpy(&correlation_id, msg, sizeof(int));
	caught_pokemon* caught = deserializar_caught(msg);
	pthread_rwlock_wrlock(&lockCola);
	mensaje* item = crear_mensaje(CAUGHT_POKEMON,cliente_fd,cola_caught->id,caught);
	cola_caught->id++;
	pthread_rwlock_unlock(&lockCola);
	item->id_correlacional = correlation_id;
	printf("Mensaje CAUGHT_POKEMON:Correlation_id:%d|Resultado:%d\n",item->id_correlacional,caught->caught);
	particion* part_aux = malloc_cache(sizeof(uint32_t));

	//almacena
	memcpy_cache(part_aux,item->id,CAUGHT_POKEMON,part_aux->inicio,&caught->caught,sizeof(uint32_t));
	log_warning(logger,"MENSAJE_ID:%d CAUGHT_POKEMON Almacenado En El Cache. Inicio:%p",item->id,part_aux->inicio);
	pthread_rwlock_wrlock(&lockCola);
	list_add(cola_caught->mensajes,item);
	enviar_id(item,cliente_fd);
}

void mensaje_get_pokemon(void* msg,int cliente_fd){
	get_pokemon* get = deserializar_get(msg);
	printf("Mensaje GET_POKEMON:Pokemon:%s|Size:%d\n",get->name,get->name_size);
	pthread_rwlock_wrlock(&lockCola);
	mensaje* item = crear_mensaje(GET_POKEMON,cliente_fd,cola_get->id,get);
	cola_get->id++;
	pthread_rwlock_unlock(&lockCola);

	//almacena
	particion* part_aux = malloc_cache(get->name_size+sizeof(uint32_t));
	memcpy_cache(part_aux,item->id,GET_POKEMON,part_aux->inicio,&get->name_size,sizeof(uint32_t));
	memcpy_cache(part_aux,item->id,GET_POKEMON,part_aux->inicio+sizeof(uint32_t),get->name,get->name_size);
	log_warning(logger,"MENSAJE_ID:%d GET_POKEMON Almacenado En El Cache. Inicio:%p",item->id,part_aux->inicio);
	pthread_rwlock_wrlock(&lockCola);
	list_add(cola_get->mensajes,item);
	pthread_rwlock_unlock(&lockCola);
	enviar_id(item,cliente_fd);
}

void mensaje_localized_pokemon(void* msg,int cliente_fd){
	int correlation_id;
	memcpy(&correlation_id, msg, sizeof(int));
	localized_pokemon* localized = deserializar_localized(msg);
	pthread_rwlock_wrlock(&lockCola);
	mensaje* item = crear_mensaje(LOCALIZED_POKEMON,cliente_fd,cola_localized->id,localized);
	cola_localized->id++;
	pthread_rwlock_unlock(&lockCola);
	item->id_correlacional = correlation_id;
	printf("Mensaje LOCALIZED_POKEMON:Correlation_id:%d|Pokemon:%s|Size:%d|Cant_posiciones:%d\n",item->id_correlacional,localized->name,localized->name_size,localized->cantidad_posiciones);
	for(int i=0;i<localized->cantidad_posiciones;i++){
		printf("Pos[%d]:[%d,%d]\n",i,localized->pos[i].posx,localized->pos[i].posy);
	}

	//almacena
	particion* part_aux = malloc_cache(localized->name_size+sizeof(uint32_t)*2+sizeof(position)*localized->cantidad_posiciones);
	memcpy_cache(part_aux,item->id,LOCALIZED_POKEMON,part_aux->inicio,&localized->name_size,sizeof(uint32_t));
	memcpy_cache(part_aux,item->id,LOCALIZED_POKEMON,part_aux->inicio+sizeof(uint32_t),localized->name,localized->name_size);
	memcpy_cache(part_aux,item->id,LOCALIZED_POKEMON,part_aux->inicio+sizeof(uint32_t)+localized->name_size,&localized->cantidad_posiciones,sizeof(uint32_t));
	int desplazamineto = sizeof(uint32_t)*2+localized->name_size;
	for(int i=0;i<localized->cantidad_posiciones;i++){
		memcpy_cache(part_aux,item->id,LOCALIZED_POKEMON,part_aux->inicio+desplazamineto,&localized->pos[i].posx,sizeof(uint32_t));
		desplazamineto+=sizeof(uint32_t);
		memcpy_cache(part_aux,item->id,LOCALIZED_POKEMON,part_aux->inicio+desplazamineto,&localized->pos[i].posy,sizeof(uint32_t));
		desplazamineto+=sizeof(uint32_t);
	}
	log_warning(logger,"MENSAJE_ID:%d LOCALIZED_POKEMON Almacenado En El Cache. Inicio:%p",item->id,part_aux->inicio);
	pthread_rwlock_wrlock(&lockCola);
	list_add(cola_get->mensajes,item);
	pthread_rwlock_unlock(&lockCola);
	enviar_id(item,cliente_fd);
}

void enviar_id(mensaje *item,int socket_cliente){
	send(socket_cliente,&item->id, sizeof(int), 0);
	printf("ID_Mensaje:%d De Cola:%d Enviado\n",item->id,item->tipo_msg);
}

void enviar_confirmacion_suscripcion(suscriber* sus){
	int ack = ACK;
	send(sus->cliente_fd,&ack, sizeof(int), 0);
	printf("Confirmacion De Suscripcion Enviada\n");
}

//enviar mensajes
void enviar_mensajes(suscriber* sus,int tipo_cola){
	bool es_tipo(particion* aux){
		return aux->tipo_cola == tipo_cola;
	}
	pthread_rwlock_rdlock(&lockCache);
	t_list* tipo  = list_filter(cache,(void*)es_tipo);
	pthread_rwlock_unlock(&lockCache);

	t_paquete* enviar = crear_paquete(tipo_cola);
	void agregar(particion* aux){
		agregar_paquete(enviar,aux,sus,tipo_cola);
	}
	list_iterate(tipo,(void*)agregar);
	enviar_paquete(enviar,sus->cliente_fd);
	char buf[1024];
	uuid_unparse(sus->pid,buf);
	log_info(logger,"Mensaje Enviado A Proceso %s",buf);
	list_destroy(tipo);
	eliminar_paquete(enviar);
}

void agregar_paquete(t_paquete* enviar,particion* aux,suscriber* sus,uint32_t tipo){
	uint32_t id,size;
	void* buffer;
	switch(tipo){
		case NEW_POKEMON:{
			id = aux->id_buffer;
			size = sizeof(uint32_t)+aux->size+1;
			buffer = malloc(size);
			memset(buffer,0,size);
			memcpy(buffer,&id,sizeof(uint32_t));
			memcpy(buffer+sizeof(uint32_t),aux->inicio,aux->size+1);
			break;
		}
		case GET_POKEMON:{
			id = aux->id_buffer;
			size = sizeof(uint32_t)+aux->size+1;
			buffer = malloc(size);
			memset(buffer,0,size);
			memcpy(buffer,&id,sizeof(uint32_t));
			memcpy(buffer+sizeof(uint32_t),aux->inicio,aux->size);
			break;
		}
		case CATCH_POKEMON:{
			id = aux->id_buffer;
			size = sizeof(uint32_t)+aux->size+1;
			buffer = malloc(size);
			memset(buffer,0,size);
			memcpy(buffer,&id,sizeof(uint32_t));
			memcpy(buffer+sizeof(uint32_t),aux->inicio,aux->size);
			break;
		}
		case CAUGHT_POKEMON:{
			bool catch_correlacional(mensaje* msj){
				return msj->id == aux->id_buffer;
			}
			mensaje* catch = list_find(cola_catch->mensajes,(void*)catch_correlacional);
			id = catch->id_correlacional;
			size = sizeof(uint32_t)*2;
			buffer = malloc(size);
			memset(buffer,0,size);
			memcpy(buffer,&id,sizeof(uint32_t));
			memcpy(buffer+sizeof(uint32_t),aux->inicio,sizeof(uint32_t));
			break;
		}
		case APPEARED_POKEMON:{
			bool new_correlacional(mensaje* msj){
				return msj->id == aux->id_buffer;
			}
			mensaje* new = list_find(cola_new->mensajes,(void*)new_correlacional);
			id = new->id_correlacional;
			size = sizeof(uint32_t)+aux->size+1;
			buffer = malloc(size);
			memset(buffer,0,size);
			memcpy(buffer,&id,sizeof(uint32_t));
			memcpy(buffer+sizeof(uint32_t),aux->inicio,aux->size);
			break;
		}
	}
	agregar_a_paquete(enviar,buffer,size);

	bool by_id(mensaje* aux){
		return aux->id == id;
	}
	mq* cola = cola_mensaje(tipo);
	mensaje* m = list_find(cola->mensajes,(void*)by_id);
	list_add(m->suscriptors_enviados,sus);
	free(buffer);
}

void terminar_broker( t_log* logger, t_config* config){

	log_destroy(logger);
	config_destroy(config);
}

//MEMORIA------------------------------------------------------------------------------------------------------------------
void iniciar_memoria(){
	iniciar_semaforos();
	iniciar_cache();
	//signal(SIGINT,sig_handler);

}

//CACHE-------------------------------------------------------------------------------------------------------------------
void iniciar_cache(){
	cant_busqueda = 0;
	mem_total = datos_config->tamanio_memoria;
	mem_asignada = 0;
	id_particion = 0;
	memoria = malloc(mem_total);
	cache = list_create();
	particion* header = malloc(sizeof(particion));
	header->libre = 1;
	header->id_particion = id_particion;
	header->size = mem_total;
	header->inicio = memoria;
	header->fin = header->inicio + mem_total;
	header->tiempo = time(NULL);
	list_add(cache,header);
}

particion* malloc_cache(uint32_t size){
	//sem_wait(&semHayEspacio);
	particion* elegida;
	if(size <= mem_total){
		if(string_equals_ignore_case(datos_config->algoritmo_memoria,"PD")){
			printf("==========PARTICIONES DINAMICAS==========\n");
			elegida = particiones_dinamicas(size);
		}
		else{//BUDDY SYSTEM
			printf("==========BUDDY SYSTEM==========\n");
			elegida = buddy_system(size);
		}
	}
	else{
		printf("Size Superado Al Ssize Maximo De La Memoria!\n");
		return 0;
	}
	return elegida;
}

void* memcpy_cache(particion* part,uint32_t id_buf,uint32_t tipo_cola,void* destino,void* buf,uint32_t size){
	if(part != NULL){
		part->id_buffer = id_buf;
		part->tipo_cola = tipo_cola;
		part->libre = 0;
	}
	return memcpy(destino,buf,size);
}

void free_cache(){
	//particion* victima = algoritmo_reemplazo();
	algoritmo_reemplazo();
}


void delete_particion(particion* borrar){
	log_warning(logger,"Particion: %d Eliminada  Inicio: %p",borrar->id_particion,borrar->inicio);
	borrar->libre = 1;
	borrar->tipo_cola = -1;
	borrar->id_buffer = -1;
	pthread_rwlock_wrlock(&lockMemAsignada);
	mem_asignada -= borrar->size;
	pthread_rwlock_unlock(&lockMemAsignada);
	display_cache();
}

void compactar_cache(){
	log_warning(logger,"Compactando El Cache ...");
	particion* ultima = list_get(cache,list_size(cache)-1);
	bool libre_no_ultima(particion* aux){
		return aux->libre && aux->fin != ultima->fin;
	}

	while(1){
		particion* borrar = list_find(cache,(void*)libre_no_ultima);
		if(!borrar){
			break;
		}
		bool by_id(particion* aux){
			return aux->id_particion == borrar->id_particion;
		}
		list_remove_by_condition(cache,(void*)by_id);
		uint32_t size = borrar->size;
		void compac(particion* aux){
			if(aux->id_particion == ultima->id_particion){
				aux->inicio -= size;
				aux->size += size;
			}
			else if(aux->fin <= borrar->inicio){
				//No tocar las particiones anteriores de la particion a borrar
			}
			else{
				aux->inicio -= size;
				aux->fin = aux->inicio+aux->size;
			}
		}
		list_map(cache,(void*)compac);
	}
}

particion* particiones_dinamicas(uint32_t size){
	particion* elegida;
	uint32_t part_size = size;
	if(size < datos_config->tamanio_min_compactacion){
		part_size =  datos_config->tamanio_min_compactacion;
	}
	elegida = algoritmo_particion_libre(part_size);
	particion* ultima =  list_get(cache,list_size(cache)-1);
	if(elegida != NULL && elegida->id_particion == ultima->id_particion){
		elegida->size = part_size;
		elegida->fin = elegida->inicio+part_size;
		elegida->libre = 0;
		elegida->tiempo = time(NULL);
		pthread_rwlock_wrlock(&lockParticion);
		id_particion++;
		pthread_rwlock_unlock(&lockParticion);
		pthread_rwlock_wrlock(&lockMemAsignada);
		mem_asignada += part_size;
		pthread_rwlock_unlock(&lockMemAsignada);
		particion* next = malloc(sizeof(particion));
		pthread_rwlock_rdlock(&lockParticion);
		next->id_particion = id_particion;
		pthread_rwlock_unlock(&lockParticion);
		next->libre = 1;
		pthread_rwlock_rdlock(&lockMemAsignada);
		next->size = mem_total-mem_asignada;
		pthread_rwlock_unlock(&lockMemAsignada);
		next->inicio = elegida->fin;
		next->fin = next->inicio+next->size;
		next->tiempo = time(NULL);
		next->id_buffer = -1;
		next->tipo_cola = -1;
		pthread_rwlock_wrlock(&lockCache);
		list_add(cache,next);
		pthread_rwlock_unlock(&lockCache);
	}
	else if(elegida != NULL && elegida->id_particion != ultima->id_particion){
		uint32_t size_original = elegida->size;
		elegida->size = part_size;
		elegida->fin = elegida->inicio+elegida->size;
		elegida->libre = 0;
		elegida->tiempo = time(NULL);
		pthread_rwlock_wrlock(&lockParticion);
		id_particion++;
		pthread_rwlock_unlock(&lockParticion);
		pthread_rwlock_wrlock(&lockMemAsignada);
		mem_asignada += part_size;
		pthread_rwlock_unlock(&lockMemAsignada);
		if(size_original - size > 0){
			particion* next = malloc(sizeof(particion));
			next->libre = 1;
			next->size = size_original - part_size;
			next->inicio = elegida->fin;
			next->fin = next->inicio+next->size;
			next->tiempo = time(NULL);
			next->id_buffer = -1;
			next->tipo_cola = -1;
			bool posicion(particion* aux){
				return aux->inicio <= elegida->fin;
			}
			pthread_rwlock_rdlock(&lockCache);
			int pos = list_count_satisfying(cache,(void*)posicion);
			pthread_rwlock_unlock(&lockCache);

			pthread_rwlock_wrlock(&lockCache);
			list_add_in_index(cache,pos,next);
			pthread_rwlock_unlock(&lockCache);
		}
	}
	else{
		printf("No Hay Suficiente Espacio!\n");
		cant_busqueda++;
		if(cant_busqueda <= datos_config->frecuencia_compactacion){
			if(cant_busqueda == datos_config->frecuencia_compactacion){
				compactar_cache();
			}
		}
		else{
			free_cache();
		}
	}
	return elegida;
}

particion* buddy_system(uint32_t size){
	uint32_t size_particion = calcular_size_potencia_dos(size);
	particion* elegida;
	bool libre_suficiente(particion* aux){
		return aux->libre == 1 && aux->size >= size_particion;
	}
	if(size_particion < mem_total){
		if(size_particion > (mem_total/2)){
			pthread_rwlock_rdlock(&lockCache);
			elegida = list_find(cache,(void*)libre_suficiente);
			pthread_rwlock_unlock(&lockCache);
			if(elegida!=NULL){
				elegida->libre = 0;
				elegida->size = size_particion;
				pthread_rwlock_wrlock(&lockMemAsignada);
				mem_asignada += elegida->size;
				pthread_rwlock_unlock(&lockMemAsignada);
			}
		}
		else{
			uint32_t pow_size = log_dos(size_particion);
			pthread_rwlock_rdlock(&lockCache);
			particion* primera = list_find(cache,(void*)libre_suficiente);
			pthread_rwlock_unlock(&lockCache);
			uint32_t pow_pri = log_dos(primera->size);
			void aplicar(particion* aux){
				bool libre_suficiente(particion* p){
					return p->libre == 1 && p->size >= size_particion && p->size >= datos_config->tamanio_min_compactacion;
				}
				pthread_rwlock_rdlock(&lockCache);
				particion* vic = list_find(cache,(void*)libre_suficiente);
				particion* ultima = list_get(cache,list_size(cache)-1);
				pthread_rwlock_unlock(&lockCache);
				if(aux->inicio==vic->inicio && aux->fin==ultima->fin){
					uint32_t s = aux->size/2;
					vic->size = s ;
					vic->fin = vic->inicio+vic->size;
					particion* next = malloc(sizeof(particion));
					next->size = s;
					next->inicio = vic->fin;
					next->fin = next->inicio+next->size;
					next->libre = 1;
					next->id_buffer = -1;
					next->tipo_cola = -1;
					pthread_rwlock_wrlock(&lockCache);
					list_add(cache,next);
					pthread_rwlock_unlock(&lockCache);
				}
				else if(aux->inicio==vic->inicio && aux->size==size_particion){
					vic->size = size_particion;
					vic->libre = 0;
					pthread_rwlock_wrlock(&lockMemAsignada);
					mem_asignada += elegida->size;
					pthread_rwlock_unlock(&lockMemAsignada);
				}
				else if(aux->inicio==vic->inicio){
					uint32_t s = aux->size/2;
					vic->size = s ;
					vic->fin = vic->inicio+vic->size;
					particion* next = malloc(sizeof(particion));
					next->size = s;
					next->inicio = vic->fin;
					next->fin = next->inicio+next->size;
					next->libre = 1;
					next->id_buffer = -1;
					next->tipo_cola = -1;
					bool posicion(particion* aux){
						return aux->inicio <= vic->fin;
					}
					int pos = list_count_satisfying(cache,(void*)posicion);
					list_add_in_index(cache,pos,next);
				}
			}
			for(int i=pow_size;i < pow_pri ;i++){
				pthread_rwlock_wrlock(&lockCache);
				list_map(cache,(void*)aplicar);
				pthread_rwlock_unlock(&lockCache);
			}
			pthread_rwlock_rdlock(&lockCache);
			elegida = list_find(cache,(void*)libre_suficiente);
			pthread_rwlock_unlock(&lockCache);
		}
		pthread_rwlock_rdlock(&lockParticion);
		elegida->id_particion = id_particion;
		pthread_rwlock_unlock(&lockParticion);
		elegida->size = size;
		pthread_rwlock_wrlock(&lockParticion);
		id_particion++;
		pthread_rwlock_unlock(&lockParticion);
	}
	return elegida;
}

particion* algoritmo_particion_libre(uint32_t size){
	particion* elegida;
	bool libre_suficiente(particion* aux){
		return aux->libre == 1 && aux->size >= datos_config->tamanio_min_compactacion;
	}
	if(string_equals_ignore_case(datos_config->algoritmo_particion_libre,"FF")){
		printf("==========FIRST FIT==========\n");
		pthread_rwlock_rdlock(&lockCache);
		elegida = list_find(cache,(void*)libre_suficiente);
		pthread_rwlock_unlock(&lockCache);
	}
	else{
		printf("==========BEST FIT==========\n");
		bool espacio_min(particion* aux1,particion* aux2){
			return aux1->size < aux2->size;
		}
		pthread_rwlock_wrlock(&lockCache);
		t_list* aux = list_filter(cache,(void*)libre_suficiente);
		pthread_rwlock_unlock(&lockCache);
		list_sort(aux,(void*)espacio_min);
		elegida = list_get(aux,0);
		list_destroy(aux);
	}
	return elegida;
}

particion* algoritmo_reemplazo(){
	t_list* aux;
	particion* victima;
	if(string_equals_ignore_case(datos_config->algoritmo_reemplazo,"FIFO")){
		printf("==========FIFO==========\n");
		bool by_id(particion* aux1,particion* aux2){
			return aux1->id_particion < aux2->id_particion;
		}
		bool ocupada(particion* aux){
			return aux->libre == 0;
		}
		pthread_rwlock_rdlock(&lockCache);
		aux = list_sorted(cache,(void*)by_id);
		pthread_rwlock_unlock(&lockCache);
		victima = list_find(aux,(void*)ocupada);
		delete_particion(victima);
		//victima->libre = 1;
		//victima->tipo_cola = -1;
		//victima->id_buffer = -1;
		//mem_asignada -= victima->size;
		list_destroy(aux);
	}
	else{
		printf("==========LRU==========\n");
		pthread_rwlock_rdlock(&lockCache);
		t_list* sorted = list_duplicate(cache);
		time_t actual;
		particion* ultima = list_get(cache,list_size(cache)-1);
		pthread_rwlock_unlock(&lockCache);
		actual = time(NULL);

		bool by_time(particion* aux1,particion* aux2){
			return actual-aux1->tiempo > actual-aux2->tiempo;
		}

		list_sorted(sorted,(void*)by_time);

		bool no_ultima(particion* aux){
			return aux->id_particion!=ultima->id_particion;
		}
		victima= list_find(sorted,(void*)no_ultima);
		delete_particion(victima);
		//victima->libre = 1;
		//victima->tipo_cola = -1;
		//victima->id_buffer = -1;
		//mem_asignada -= victima->size;
		list_destroy(sorted);
	}
	return victima;
}

uint32_t calcular_size_potencia_dos(uint32_t size){
	uint32_t potencia = 0;
	uint32_t res = 0;
	potencia = log_dos(size);
	res = pow(2,potencia);
	if(res < datos_config->tamanio_min_compactacion){
		res ++;
	}
	return res;
}

uint32_t log_dos(uint32_t size){
	uint32_t potencia;
	if(size == 1){
			potencia = 0;
	}
	else{
		potencia = 1+log_dos(size>>1);
		if(pow(2,potencia) < size){
			potencia++;
		}
	}
	return potencia;
}

void handler_dump(int signo){
	if(signo == SIGUSR1){
		log_info(logger,"Dump De Cache ...");
		FILE *dump;
		dump = fopen("dump.txt","w+");

		time_t tiempo;
		time(&tiempo);
		struct tm* p;
		p = gmtime(&tiempo);

		fprintf(dump,"---------------------------------------------------------------------------------------------------------------------------------\n");
		fprintf(dump,"Dump:%d/%d/%d %d:%d:%d\n",p->tm_mday,p->tm_mon+1,p->tm_year+1900,p->tm_hour+3,p->tm_min,p->tm_sec);
		//fprintf(dump,"Dump:%s\n",temporal_get_string_time());
		char caracter(uint32_t nro){
			char res = 'L';
			if(nro != 1){
				res = 'X';
			}
			return res;
		}

		void imprimir(void* ele){
			particion* aux = ele;
			if(aux->libre == 0){
				fprintf(dump,"Paricion %d:%p-%p   [%c]   Size:%db   %s:<%d>   Cola:<%d>   ID:<%d>\n",
							aux->id_particion,aux->inicio,aux->fin,caracter(aux->libre),aux->size,
							datos_config->algoritmo_reemplazo,1,aux->tipo_cola,aux->id_buffer);
			}
			else{
				fprintf(dump,"Espacio    :%p-%p   [%c]   Size:%db\n",aux->inicio,aux->fin,caracter(aux->libre),aux->size);
			}
		}
		pthread_rwlock_rdlock(&lockCache);
		list_iterate(cache,(void*)imprimir);
		pthread_rwlock_unlock(&lockCache);
		fprintf(dump,"---------------------------------------------------------------------------------------------------------------------------------\n");

		fclose(dump);
		printf("SIGUSR1 RUNNING...\n");
		exit(0);
	}
}

void display_cache(){
	time_t tiempo;
	time(&tiempo);
	struct tm* p;
	p = gmtime(&tiempo);

	printf("------------------------------------------------------------------------------\n");
	printf("Dump:%d/%d/%d %d:%d:%d\n",p->tm_mday,p->tm_mon+1,p->tm_year+1900,p->tm_hour+3,p->tm_min,p->tm_sec);
	//printf("Dump:%s\n",temporal_get_string_time());
	char caracter(uint32_t nro){
		char res = 'L';
		if(nro != 1){
			res = 'X';
		}
		return res;
	}
	void imprimir(void* ele){
		particion* aux = ele;
		if(aux->libre == 0){
			printf("Paticion %d:%p-%p   [%c]   Size:%db   %s:<%d>   Cola:<%d>   ID:<%d>\n",
						aux->id_particion,aux->inicio,aux->fin,caracter(aux->libre),aux->size,
						datos_config->algoritmo_reemplazo,1,aux->tipo_cola,aux->id_buffer);
		}
		else{
			printf("Espacio   :%p-%p   [%c]   Size:%db\n",aux->inicio,aux->fin,caracter(aux->libre),aux->size);
		}

	}
	pthread_rwlock_rdlock(&lockCache);
	list_iterate(cache,(void*)imprimir);
	pthread_rwlock_unlock(&lockCache);
	printf("------------------------------------------------------------------------------\n");
}

void limpiar_cache(){
	printf("=====LIMPIANDO=====\n");

}
