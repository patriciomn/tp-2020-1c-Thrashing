#include"broker.h"

t_log* logger;
t_log* dump;
t_config* config;
config_broker* datos_config;

mq* cola_get;
mq* cola_localized;
mq* cola_catch;
mq* cola_caught;
mq* cola_new;
mq* cola_appeared;

pthread_t thread_servidor;
pthread_t thread_ack;

int cant_liberadas;
int inicio;
uint32_t mem_asignada;
uint32_t mem_total;
uint32_t id_particion;
t_list* cache;
void* memoria;

pthread_rwlock_t lockNew = PTHREAD_RWLOCK_INITIALIZER;
pthread_rwlock_t lockGet = PTHREAD_RWLOCK_INITIALIZER;
pthread_rwlock_t lockCatch = PTHREAD_RWLOCK_INITIALIZER;
pthread_rwlock_t lockAppeared = PTHREAD_RWLOCK_INITIALIZER;
pthread_rwlock_t lockLocalized = PTHREAD_RWLOCK_INITIALIZER;
pthread_rwlock_t lockCaught = PTHREAD_RWLOCK_INITIALIZER;
pthread_rwlock_t lockEnviados = PTHREAD_RWLOCK_INITIALIZER;
pthread_mutex_t mutexMalloc = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexMemcpy = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexCache = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexAck = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexAsignada = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexConsol = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexDelete = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexCola = PTHREAD_MUTEX_INITIALIZER;

int main(){	
    iniciar_broker();
}

void iniciar_broker(void){
	iniciar_config("broker.config");
	logger = log_create(datos_config->log_file,"broker",1,LOG_LEVEL_INFO);
	dump = log_create("dump.log","broker",1,LOG_LEVEL_INFO);
	iniciar_colas_mensaje();
	printf("\033[1;33mBROKER START!\033[0m\n");
	iniciar_cache();
	iniciar_servidor();
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
	log_info(logger,"Nuevo Mensaje Tipo_Mensaje:%s ID_Mensaje:%d",get_cola(m->tipo_msg),m->id);
	return m;
}

void iniciar_config(char* memoria_config){
	config = leer_config(memoria_config);
	datos_config = malloc(sizeof(config_broker));
	datos_config->log_file = config_get_string_value(config,"LOG_FILE");
	datos_config->ip_broker = config_get_string_value(config,"IP_BROKER");
	datos_config->puerto_broker = config_get_string_value(config,"PUERTO_BROKER");
	datos_config->tamanio_memoria = atoi(config_get_string_value(config,"TAMANO_MEMORIA"));
	datos_config->tamanio_min_particion = atoi(config_get_string_value(config,"TAMANO_MINIMO_PARTICION"));
	datos_config->algoritmo_memoria = config_get_string_value(config,"ALGORITMO_MEMORIA");
	datos_config->algoritmo_particion_libre = config_get_string_value(config,"ALGORITMO_PARTICION_LIBRE");
	datos_config->algoritmo_reemplazo = config_get_string_value(config,"ALGORITMO_REEMPLAZO");
	datos_config->frecuencia_compactacion = atoi(config_get_string_value(config,"FRECUENCIA_COMPACTACION"));
}

void terminar_broker( t_log* logger, t_config* config){
	log_destroy(logger);
	config_destroy(config);
}

void iniciar_servidor(void){
	int socket_servidor;
    struct addrinfo hints, *servinfo, *p;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    getaddrinfo(datos_config->ip_broker, datos_config->puerto_broker, &hints, &servinfo);

    for(p=servinfo; p != NULL; p = p->ai_next){
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
	sleep(1);
	log_info(logger,"Proceso Conectado Con Socket %d",*cliente_fd);
	pthread_create(&thread_servidor,NULL,(void*)serve_client,cliente_fd);
	pthread_detach(thread_servidor);
}

void serve_client(int* socket){
	int cod_op;
	memset(&cod_op, 0 ,sizeof(int));
	if(recv(*socket, &cod_op, sizeof(int), MSG_WAITALL) == -1){
		printf("\033[0;31mERROR: socket error\033[0m\n" );
    	exit(0);
	}
	if (cod_op <= 0 ){
		printf("\033[0;31mSe Desconectó El Socket: %d\033[0m\n", *socket);
    	pthread_exit(NULL);
	}	

	int cliente_fd = *socket;
	free(socket);
	process_request(cod_op, cliente_fd);
}

void process_request(int cod_op, int cliente_fd){
	if (cod_op == SUSCRITO){
		atender_suscripcion(cliente_fd);
	}
	else{
		void* msg = recibir_mensaje(cliente_fd);
			switch(cod_op){
				case NEW_POKEMON:
					mensaje_new_pokemon(msg,cliente_fd);
					break;
				case APPEARED_POKEMON:
					mensaje_appeared_pokemon(msg,cliente_fd);
					break;
				case CATCH_POKEMON:
					mensaje_catch_pokemon(msg,cliente_fd);
					break;
				case CAUGHT_POKEMON:
					mensaje_caught_pokemon(msg,cliente_fd);
					break;
				case GET_POKEMON:
					mensaje_get_pokemon(msg,cliente_fd);
					break;
				case LOCALIZED_POKEMON:
					mensaje_localized_pokemon(msg,cod_op);
					break;
			}
		free(msg);

		mq* cola = cola_mensaje(cod_op);
		if(cola != NULL && !list_is_empty(cola->suscriptors)){
			void enviar(suscriber* sus){
				if(check_socket(sus->cliente_fd) == 1){
					enviar_mensajes(sus,cod_op);
				}
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
	return NULL;
}

void atender_suscripcion(int cliente_fd){
	void * msg = recibir_mensaje(cliente_fd);
	int queue_id;
	pid_t pid;
	memcpy(&queue_id,msg,sizeof(uint32_t));
	memcpy(&pid,msg+sizeof(uint32_t),sizeof(pid_t));
	printf("\033[1;35mSuscripcion:Pid:%d|Cola:%s|Socket:%d\033[0m\n",pid,get_cola(queue_id),cliente_fd);

	suscriber * sus = malloc(sizeof(suscriber));
	sus->cliente_fd = cliente_fd;
	sus->pid = pid;

	mq* cola = cola_mensaje(queue_id);
	bool existe(suscriber* aux){
		return aux->pid == sus->pid;
	}
	if (!list_any_satisfy(cola->suscriptors,(void*)existe) ){
		list_add(cola->suscriptors, sus);
		log_info(logger,"Proceso %d Suscrito A La Cola %s",pid,get_cola(queue_id));
	}
	else{
		log_warning(logger,"Proceso %d Ya Habia Suscrito A La Cola %s",pid,get_cola(queue_id));
		suscriber* sus = list_find(cola->suscriptors,(void*)existe);
		sus->cliente_fd = cliente_fd;
	}
	enviar_confirmacion_suscripcion(sus);

	if(!list_is_empty(cola->mensajes)){
		if(check_socket(sus->cliente_fd) == 1){
			enviar_mensajes(sus,queue_id);
		}
	}
	free(msg);
}

void atender_ack(suscriber* sus){
	int tipo;
	int res = recv(sus->cliente_fd, &tipo, sizeof(int),MSG_WAITALL);

	if(res == -1)
		tipo = -1;
	if(tipo > -1 && check_socket(sus->cliente_fd) == 1){
		int id;
		pid_t pid;

		void* msg = recibir_mensaje(sus->cliente_fd);
		if(msg != NULL){
			memcpy(&(id), msg, sizeof(int));
			memcpy(&pid,msg+sizeof(int),sizeof(pid_t));

			mq* cola = cola_mensaje(tipo);
			if(cola != NULL){
				log_info(logger,"ACK Recibido:Proceso:%d,Tipo:%s,ID:%d",pid,get_cola(tipo),id);
				bool by_id(mensaje* aux){
					return aux->id == id;
				}

				mensaje* item = list_find(cola->mensajes,(void*)by_id);
				if(item != NULL){
					list_add(item->suscriptors_ack,sus);

					if(confirmado_todos_susciptors_mensaje(item) == 1){
						printf("\033[1;35mMensaje  Tipo: %s ID: %d Ya Ha Recibido Los ACKs Por Todos Sus Suscriptors\033[0m\n",get_cola(item->tipo_msg),item->id);
						//borrar_mensaje(item);
					}
				}
			}

			free(msg);
		}
	}
}

bool confirmado_todos_susciptors_mensaje(mensaje* m){
	for(int i=0;i<list_size(m->suscriptors_enviados);i++){
		pthread_rwlock_rdlock(&lockEnviados);
		suscriber* sus = list_get(m->suscriptors_enviados,i);
		pthread_rwlock_unlock(&lockEnviados);
		bool by_pid(suscriber* aux){
			return aux->pid == sus->pid;
		}
		if(!list_any_satisfy(m->suscriptors_ack,(void*)by_pid)){
			return 0;
		}
	}
	return 1;
}

void borrar_mensaje(mensaje* m){
	bool by_id_tipo(particion* aux){
		return aux->id_mensaje == m->id && aux->tipo_cola == m->tipo_msg;
	}

	particion* borrar = list_find(cache,(void*)by_id_tipo);

	delete_particion(borrar);

}

//recibir mensajes==================================================================================================================================================================
void mensaje_new_pokemon(void* msg,int cliente_fd){
	new_pokemon* new = deserializar_new(msg);
	printf("\033[1;35mDatos Recibidos: NEW_POKEMON:Pokemon:%s|Size:%d|Pos:[%d,%d]|Cantidad:%d\033[0m\n",new->name,new->name_size,new->pos.posx,new->pos.posy,new->cantidad);
	pthread_rwlock_rdlock(&lockNew);
	mensaje* item = crear_mensaje(NEW_POKEMON,cliente_fd,cola_new->id,new);
	pthread_rwlock_unlock(&lockNew);

	//almacena
	size_t size = new->name_size + sizeof(uint32_t)*4;
	particion* part_aux = malloc_cache(size);
	if(part_aux != NULL){
		memcpy_cache(part_aux,item->id,NEW_POKEMON,part_aux->inicio,msg,size);
		log_warning(logger,"Mensaje NEW_POKEMON ID:%d Almacenado En El Cache Inicio:%d",item->id,part_aux->start);
		pthread_rwlock_wrlock(&lockNew);
		list_add(cola_new->mensajes,item);
		cola_new->id++;
		enviar_id(item,cliente_fd);
		pthread_rwlock_unlock(&lockNew);
	}
	else{
		free(item);
	}
	free(new->name);
	free(new);
}

void mensaje_appeared_pokemon(void* msg,int cliente_fd){
	int correlation_id;
	memcpy(&correlation_id, msg, sizeof(int));
	appeared_pokemon* appeared = deserializar_appeared(msg);
	printf("\033[1;35mDatos Recibidos:APPEARED_POKEMON:Correlation_ID:%d|Pokemon:%s|Size:%d|Pos:[%d,%d]\033[0m\n",correlation_id,appeared->name,appeared->name_size,appeared->pos.posx,appeared->pos.posy);
	pthread_rwlock_rdlock(&lockAppeared);
	mensaje* item = crear_mensaje(APPEARED_POKEMON,cliente_fd,cola_appeared->id,appeared);
	pthread_rwlock_unlock(&lockAppeared);
	item->id_correlacional = correlation_id;

	bool new_correlacionl(mensaje* ele){
		return ele->id == correlation_id;
	}
	mensaje* new = list_find(cola_new->mensajes,(void*)new_correlacionl);
	if(new){
		new->id_correlacional = item->id;
	}

	//almacena
	size_t size = appeared->name_size + sizeof(uint32_t)*3;
	particion* part_aux = malloc_cache(size);
	if(part_aux != NULL){
		memcpy_cache(part_aux,item->id,APPEARED_POKEMON,part_aux->inicio,msg+sizeof(uint32_t),size);
		log_warning(logger,"Mensaje APPEARED_POKEMON ID:%d Almacenado En El Cache. Inicio:%d",item->id,part_aux->start);
		pthread_rwlock_wrlock(&lockAppeared);
		list_add(cola_appeared->mensajes,item);
		cola_appeared->id++;
		enviar_id(item,cliente_fd);
		pthread_rwlock_unlock(&lockAppeared);
	}
	else{
		free(item);
	}
	free(appeared->name);
	free(appeared);
}

void mensaje_catch_pokemon(void* msg,int cliente_fd){
	catch_pokemon* catch = deserializar_catch(msg);
	printf("\033[1;35mDatos Recibidos:CATCH_POKEMON:Pokemon:%s|Size:%d|Pos:[%d,%d]\033[0m\n",catch->name,catch->name_size,catch->pos.posx,catch->pos.posy);
	pthread_rwlock_rdlock(&lockCatch);
	mensaje* item = crear_mensaje(CATCH_POKEMON,cliente_fd,cola_catch->id,catch);
	pthread_rwlock_unlock(&lockCatch);

	//almacena
	size_t size = catch->name_size + sizeof(uint32_t)*3;
	particion* part_aux = malloc_cache(size);
	if(part_aux != NULL){
		memcpy_cache(part_aux,item->id,CATCH_POKEMON,part_aux->inicio,msg,size);
		log_warning(logger,"Mensaje CATCH_POKEMON ID:%d Almacenado En El Cache. Inicio:%d",item->id,part_aux->start);
		pthread_rwlock_wrlock(&lockCatch);
		list_add(cola_catch->mensajes,item);
		cola_catch->id++;
		enviar_id(item,cliente_fd);
		pthread_rwlock_unlock(&lockCatch);
	}
	else{
		free(item);
	}
	free(catch->name);
	free(catch);
}

void mensaje_caught_pokemon(void* msg,int cliente_fd){
	int correlation_id;
	memcpy(&correlation_id, msg, sizeof(int));
	caught_pokemon* caught = deserializar_caught(msg);
	printf("\033[1;35mDatos Recibidos:CAUGHT_POKEMON:Correlation_id:%d|Resultado:%d\033[0m\n",correlation_id,caught->caught);
	pthread_rwlock_rdlock(&lockCaught);
	mensaje* item = crear_mensaje(CAUGHT_POKEMON,cliente_fd,cola_caught->id,caught);
	pthread_rwlock_unlock(&lockCaught);
	item->id_correlacional = correlation_id;

	bool catch_correlacionl(mensaje* aux){
		return aux->id == correlation_id;
	}
	mensaje* catch = list_find(cola_catch->mensajes,(void*)catch_correlacionl);
	if(catch){
		catch->id_correlacional = item->id;
	}

	//almacena
	particion* part_aux = malloc_cache(sizeof(uint32_t));
	if(part_aux != NULL){
		memcpy_cache(part_aux,item->id,CAUGHT_POKEMON,part_aux->inicio,&(caught->caught),sizeof(bool));
		log_warning(logger,"Mensaje CAUGHT_POKEMON ID:%d Almacenado En El Cache. Inicio:%d",item->id,part_aux->start);
		pthread_rwlock_wrlock(&lockCaught);
		list_add(cola_caught->mensajes,item);
		cola_caught->id++;
		enviar_id(item,cliente_fd);
		pthread_rwlock_unlock(&lockCaught);
	}
	else{
		free(item);
	}
	free(caught);
}

void mensaje_get_pokemon(void* msg,int cliente_fd){
	get_pokemon* get = deserializar_get(msg);
	printf("\033[1;35mDatos Recibidos:GET_POKEMON:Pokemon:%s|Size:%d\033[0m\n",get->name,get->name_size);
	pthread_rwlock_rdlock(&lockGet);
	mensaje* item = crear_mensaje(GET_POKEMON,cliente_fd,cola_get->id,get);
	pthread_rwlock_unlock(&lockGet);

	//almacena
	size_t size = get->name_size + sizeof(uint32_t);

	particion* part_aux = malloc_cache(size);
	if(part_aux != NULL){
		memcpy_cache(part_aux,item->id,GET_POKEMON,part_aux->inicio,msg,size);
		log_warning(logger,"Mensaje GET_POKEMON ID:%d Almacenado En El Cache. Inicio:%d",item->id,part_aux->start);
		pthread_rwlock_wrlock(&lockGet);
		list_add(cola_get->mensajes,item);
		cola_get->id++;
		enviar_id(item,cliente_fd);
		pthread_rwlock_unlock(&lockGet);
	}
	else{
		free(item);
	}
	free(get->name);
	free(get);
}

void mensaje_localized_pokemon(void* msg,int cliente_fd){
	int correlation_id;
	memcpy(&correlation_id, msg, sizeof(int));
	localized_pokemon* localized = deserializar_localized(msg);
	printf("\033[1;35mDatos Recibidos:LOCALIZED_POKEMON:Correlation_id:%d|Pokemon:%s|Size:%d|Cant_posiciones:%d\033[0m\n",correlation_id,localized->name,localized->name_size,localized->cantidad_posiciones);
	void show(position* pos){
		printf("Pos:[%d,%d]\n",pos->posx,pos->posy);
		free(pos);
	}
	list_iterate(localized->posiciones,(void*)show);

	pthread_rwlock_rdlock(&lockLocalized);
	mensaje* item = crear_mensaje(LOCALIZED_POKEMON,cliente_fd,cola_localized->id,localized);
	pthread_rwlock_unlock(&lockLocalized);
	item->id_correlacional = correlation_id;

	bool get_correlacionl(mensaje* aux){
		return aux->id == correlation_id;
	}
	mensaje* get = list_find(cola_get->mensajes,(void*)get_correlacionl);
	if(get){
		get->id_correlacional = item->id;
	}

	//almacena
	size_t size = localized->name_size+sizeof(uint32_t)*2+sizeof(position)*localized->cantidad_posiciones;
	particion* part_aux = malloc_cache(size);
	if(part_aux != NULL){
		memcpy_cache(part_aux,item->id,LOCALIZED_POKEMON,part_aux->inicio,msg+sizeof(uint32_t),size);
		log_warning(logger,"Mensaje LOCALIZED_POKEMON ID:%d Almacenado En El Cache. Inicio:%d",item->id,part_aux->start);

		pthread_rwlock_wrlock(&lockLocalized);
		list_add(cola_localized->mensajes,item);
		cola_localized->id++;
		enviar_id(item,cliente_fd);
		pthread_rwlock_unlock(&lockLocalized);
	}
	else{
		free(item);
	}
	free(localized->name);
	list_destroy(localized->posiciones);
	free(localized);
}

//enviar mensajes===========================================================================================================================================================
void enviar_mensajes(suscriber* sus,int tipo_cola){
	mq* cola = cola_mensaje(tipo_cola);
	bool no_enviado(mensaje* m){
		bool by_id(suscriber* aux){
			return aux->pid == sus->pid;
		}
		pthread_rwlock_rdlock(&lockEnviados);
		bool res = !list_any_satisfy(m->suscriptors_enviados,(void*)by_id);
		pthread_rwlock_unlock(&lockEnviados);
		return res;
	}

	t_list* mensajes = list_filter(cola->mensajes,(void*)no_enviado);


	bool es_tipo(particion* aux){
		return aux->tipo_cola == tipo_cola;
	}

	t_list* particiones_tipo  = list_filter(cache,(void*)es_tipo);

	t_list* particiones_filtradas = list_create();

	for(int i=0;i<list_size(mensajes);i++){
		mensaje* m = list_get(mensajes,i);
		bool by_id(particion* p){
			return p->id_mensaje == m->id;
		}
		particion* aux = list_find(particiones_tipo,(void*)by_id);

		list_add(particiones_filtradas,aux);
	}


	t_paquete* enviar = crear_paquete(tipo_cola);
	void agregar(particion* aux){
		agregar_paquete(enviar,aux,sus,tipo_cola);
		aux->tiempo_actual = time(NULL);
	}

	list_iterate(particiones_filtradas,(void*)agregar);
	enviar_paquete(enviar,sus->cliente_fd);

	for(int i=0;i<list_size(particiones_filtradas);i++){
		pthread_create(&thread_ack,NULL,(void*)atender_ack,sus);
		pthread_detach(thread_ack);
	}
	list_destroy(mensajes);
	list_destroy(particiones_tipo);
	list_destroy(particiones_filtradas);

	eliminar_paquete(enviar);
}

void agregar_paquete(t_paquete* enviar,particion* aux,suscriber* sus,uint32_t tipo){
	void* buffer;
	uint32_t size,id,id_correlacional;
	switch(tipo){
		case NEW_POKEMON:
		case GET_POKEMON:
		case CATCH_POKEMON:{
			id = aux->id_mensaje;
			size = sizeof(uint32_t)+aux->size+1;
			buffer = malloc(size);
			memset(buffer,0,size);
			memcpy(buffer,&id,sizeof(uint32_t));
			memcpy(buffer+sizeof(uint32_t),aux->inicio,aux->size);
			break;
		}
		case CAUGHT_POKEMON:{
			bool by_id(mensaje* msj){
				return msj->id == aux->id_mensaje;
			}
			mensaje* caught = list_find(cola_caught->mensajes,(void*)by_id);
			id = aux->id_mensaje;
			id_correlacional = caught->id_correlacional;
			size = sizeof(uint32_t)*3;
			buffer = malloc(size);
			memset(buffer,0,size);
			memcpy(buffer,&id,sizeof(uint32_t));
			memcpy(buffer+sizeof(uint32_t),&id_correlacional,sizeof(uint32_t));
			memcpy(buffer+sizeof(uint32_t)*2,aux->inicio,sizeof(bool));
			break;
		}
		case APPEARED_POKEMON:{
			bool by_id(mensaje* msj){
				return msj->id == aux->id_mensaje;
			}
			mensaje* appeared = list_find(cola_appeared->mensajes,(void*)by_id);
			id = aux->id_mensaje;
			id_correlacional = appeared->id_correlacional;
			size = sizeof(uint32_t)*2+aux->size+1;
			buffer = malloc(size);
			memset(buffer,0,size);
			memcpy(buffer,&id,sizeof(uint32_t));
			memcpy(buffer+sizeof(uint32_t),&id_correlacional,sizeof(uint32_t));
			memcpy(buffer+sizeof(uint32_t)*2,aux->inicio,aux->size);
			break;
		}
		case LOCALIZED_POKEMON:{
			bool by_id(mensaje* msj){
				return msj->id == aux->id_mensaje;
			}
			mensaje* localized = list_find(cola_localized->mensajes,(void*)by_id);
			id = aux->id_mensaje;
			id_correlacional = localized->id_correlacional;
			size = sizeof(uint32_t)*2+aux->size+1;
			buffer = malloc(size);
			memset(buffer,0,size);
			memcpy(buffer,&id,sizeof(uint32_t));
			memcpy(buffer+sizeof(uint32_t),&id_correlacional,sizeof(uint32_t));
			memcpy(buffer+sizeof(uint32_t)*2,aux->inicio,aux->size);
			break;
		}
	}
	bool by_id(mensaje* m){
		return m->id == aux->id_mensaje;
	}
	mq* cola = cola_mensaje(tipo);

	mensaje* m = list_find(cola->mensajes,(void*)by_id);

	pthread_rwlock_wrlock(&lockEnviados);
	list_add(m->suscriptors_enviados,sus);
	pthread_rwlock_unlock(&lockEnviados);
	agregar_a_paquete(enviar,buffer,size);
	log_info(logger,"Mensaje %s ID:%d Enviado A Proceso %d",get_cola(tipo),id,sus->pid);
	free(buffer);
}

void enviar_id(mensaje *item,int socket_cliente){
	send(socket_cliente,&item->id, sizeof(int), MSG_NOSIGNAL);
	printf("ID_Mensaje:%d Enviado\n",item->id);
}

void enviar_confirmacion_suscripcion(suscriber* sus){
	int ack = ACK;
	send(sus->cliente_fd,&ack, sizeof(int), MSG_NOSIGNAL);
	printf("Confirmacion De Suscripcion Enviada\n");
}

//MEMORIA============================================================================================================================================================
void iniciar_cache(){
	cant_liberadas = 0;
	inicio = 0;
	mem_total = datos_config->tamanio_memoria;
	mem_asignada = 0;
	id_particion = 0;
	memoria = malloc(mem_total);
	cache = list_create();
	particion* header = malloc(sizeof(particion));
	header->libre = 'L';
	header->id_particion = id_particion;
	header->size = mem_total;
	header->buddy_size = mem_total;
	header->start = inicio;
	header->inicio = memoria;
	header->fin = header->inicio + mem_total;
	header->end = header->start + mem_total;
	header->tiempo_inicial = time(NULL);
	header->tiempo_actual = time(NULL);
	header->intervalo = 0;
	list_add(cache,header);
	//en otra consola: kill -USR1 [pid del broker]
	signal(SIGUSR1,handler_dump);
	signal(SIGINT,handler_dump);
}

particion* malloc_cache(uint32_t size){
	particion* elegida;
	////pthread_mutex_lock(&mutexMalloc);
	if(size <= mem_total - datos_config->tamanio_min_particion){
		if(string_equals_ignore_case(datos_config->algoritmo_memoria,"PD")){
			bool libre_suficiente(particion* aux){
				return aux->libre == 'L' && aux->size >= datos_config->tamanio_min_particion && aux->size >= size;
			}
			printf("\033[1;37m==========PARTICIONES DINAMICAS==========\033[0m\n");
			if(size <= mem_total - mem_asignada &&  list_any_satisfy(cache,(void*)libre_suficiente)){
				elegida = particiones_dinamicas(size);
			}
			else{
				while(!list_any_satisfy(cache,(void*)libre_suficiente)){
					printf("\033[1;31mNo Hay Suficiente Espacio\033[0m\n");
					free_cache();
					cant_liberadas++;
					printf("\033[1;34mCantidad De Liberadas:%d\033[0m\n",cant_liberadas);
					if(cant_liberadas == datos_config->frecuencia_compactacion){
						compactar_particiones_dinamicas();
						cant_liberadas = 0;
					}
				}
				elegida = particiones_dinamicas(size);
			}
		}
		else{
			bool libre_suficiente(particion* aux){
				return aux->libre == 'L' && aux->buddy_size >= datos_config->tamanio_min_particion && aux->buddy_size >= size;
			}
			printf("\033[1;37m==========BUDDY SYSTEM==========\033[0m\n");
			if(size <= mem_total - mem_asignada &&  list_any_satisfy(cache,(void*)libre_suficiente)){
				elegida = buddy_system(size);
			}
			else{
				while(!list_any_satisfy(cache,(void*)libre_suficiente)){
					printf("\033[1;31mNo Hay Suficiente Espacio\033[0m\n");
					free_cache();
				}
				elegida = buddy_system(size);
			}
		}
	}
	else{
		printf("\033[1;31mSize Superado Al Size Maximo De La Memoria!\033[0m\n");
		return 0;
	}

	elegida->tiempo_inicial = time(NULL);
	elegida->tiempo_actual = time(NULL);
	elegida->intervalo = 0;


	return elegida;
}

void* memcpy_cache(particion* part,uint32_t id_buf,uint32_t tipo_cola,void* destino,void* buf,uint32_t size){
	if(part != NULL){
		part->id_mensaje = id_buf;
		part->tipo_cola = tipo_cola;
		part->libre = 'X';
	}

	void* res = memcpy(destino,buf,size);


	return res;
}

void free_cache(){
	algoritmo_reemplazo();
}

particion* particiones_dinamicas(uint32_t size){
	particion* elegida;
	uint32_t part_size = size;
	if(size < datos_config->tamanio_min_particion){
		part_size =  datos_config->tamanio_min_particion;
	}
	elegida = algoritmo_particion_libre(part_size);

	particion* ultima =  list_get(cache,list_size(cache)-1);

	if(elegida != NULL && elegida->start == ultima->start){
		elegida->size = part_size;
		elegida->fin = elegida->inicio+part_size;
		elegida->end = elegida->start+part_size;
		elegida->id_particion = id_particion;
		elegida->libre = 'X';
		id_particion++;
		pthread_mutex_lock(&mutexAsignada);
		mem_asignada += part_size;
		pthread_mutex_unlock(&mutexAsignada);
		if(mem_asignada < mem_total && (elegida->start+size) < mem_total){
			particion* next = malloc(sizeof(particion));
			next->id_particion = id_particion;
			next->libre = 'L';
			next->size = mem_total-mem_asignada;
			next->inicio = elegida->fin;
			next->start = elegida->end;
			next->fin = next->inicio+next->size;
			next->end = next->start+next->size;
			next->id_mensaje = -1;
			next->tipo_cola = -1;
			list_add(cache,next);
		}
	}
	else if(elegida != NULL && elegida->start != ultima->start){
		uint32_t size_original = elegida->size;
		elegida->size = part_size;
		elegida->fin = elegida->inicio+elegida->size;
		elegida->end = elegida->start+elegida->size;
		elegida->libre = 'X';
		id_particion++;
		pthread_mutex_lock(&mutexAsignada);
		mem_asignada += part_size;
		pthread_mutex_unlock(&mutexAsignada);
		if(size_original - size > 0){
			particion* next = malloc(sizeof(particion));
			next->libre = 'L';
			next->size = size_original - part_size;
			next->inicio = elegida->fin;
			next->start = elegida->end;
			next->fin = next->inicio+next->size;
			next->end = next->start+next->size;
			next->id_mensaje = -1;
			next->tipo_cola = -1;
			bool posicion(particion* aux){
				return aux->inicio <= elegida->fin;
			}
			int pos = list_count_satisfying(cache,(void*)posicion);

			list_add_in_index(cache,pos,next);

		}
	}
	return elegida;
}

particion* buddy_system(uint32_t size){
	uint32_t size_particion = calcular_size_potencia_dos(size);
	if(size_particion < datos_config->tamanio_min_particion){
		size_particion =  datos_config->tamanio_min_particion;
	}
	particion* elegida;
	bool libre_suficiente(particion* aux){
		return aux->libre == 'L' && aux->buddy_size >= size_particion;
	}

	if(size_particion > (mem_total/2)){

		elegida = list_find(cache,(void*)libre_suficiente);

		if(elegida != NULL){
			elegida->libre = 'X';
			elegida->size = size;
			elegida->buddy_size = size_particion;
			pthread_mutex_lock(&mutexAsignada);
			mem_asignada += elegida->buddy_size;
			pthread_mutex_unlock(&mutexAsignada);
		}
	}
	else{
		uint32_t pow_size = log_dos(size_particion);

		particion* primera = list_find(cache,(void*)libre_suficiente);

		uint32_t pow_pri = log_dos(primera->buddy_size);

		void aplicar(particion* aux){
			bool libre_suficiente(particion* p){
				return p->libre == 'L' && p->buddy_size >= size_particion && p->buddy_size >= datos_config->tamanio_min_particion;
			}

			particion* vic = list_find(cache,(void*)libre_suficiente);

			particion* ultima = list_get(cache,list_size(cache)-1);
			if(aux->inicio==vic->inicio){
				uint32_t s = aux->buddy_size/2;
				vic->buddy_size = s ;
				vic->size = s ;
				vic->fin = vic->inicio+vic->buddy_size;
				vic->end = vic->start+vic->buddy_size;
				vic->libre = 'L';
				particion* next = malloc(sizeof(particion));
				next->buddy_size = s;
				next->size = s ;
				next->inicio = vic->fin;
				next->start = vic->end;
				next->fin = next->inicio+next->buddy_size;
				next->end = next->start+next->buddy_size;
				next->libre = 'L';
				next->id_mensaje = -1;
				next->tipo_cola = -1;
				if(aux->fin==ultima->fin){
					list_add(cache,next);
				}
				else{
					bool posicion(particion* aux){
						return aux->inicio <= vic->fin;
					}

					int pos = list_count_satisfying(cache,(void*)posicion);
					list_add_in_index(cache,pos,next);
				}
			}
			else if(aux->inicio==vic->inicio && aux->buddy_size>=size_particion && aux->buddy_size/2<size_particion){
				vic->buddy_size = size_particion;
				vic->size = size_particion ;
				vic->libre = 'L';
			}
		}
		for(int i=pow_size;i < pow_pri ;i++){
			list_iterate(cache,(void*)aplicar);
		}
		elegida = list_find(cache,(void*)libre_suficiente);
	}

	elegida->id_particion = id_particion;
	elegida->buddy_size = size_particion;
	elegida->size = size;
	elegida->libre = 'X';
	pthread_mutex_lock(&mutexAsignada);
	mem_asignada += size_particion;
	pthread_mutex_unlock(&mutexAsignada);
	id_particion++;

	return elegida;
}

particion* algoritmo_particion_libre(uint32_t size){
	particion* elegida;
	bool libre_suficiente(particion* aux){
		return aux->libre == 'L' && aux->size >= datos_config->tamanio_min_particion && aux->size >= size;
	}
	if(string_equals_ignore_case(datos_config->algoritmo_particion_libre,"FF")){
		printf("\33[1;37m==========ALGORITEMO DE PARTICION LIBRE:FIRST FIT==========\033[0m\n");
		elegida = list_find(cache,(void*)libre_suficiente);
	}
	else{
		printf("\33[1;37m==========ALGORITEMO DE PARTICION LIBRE:BEST FIT==========\033[0m\n");
		bool espacio_min(particion* aux1,particion* aux2){
			return aux1->size <= aux2->size;
		}
		t_list* aux = list_filter(cache,(void*)libre_suficiente);
		list_sort(aux,(void*)espacio_min);

		elegida = list_get(aux,0);
		list_destroy(aux);
	}
	return elegida;
}

void compactar_particiones_dinamicas(){
	log_warning(logger,"Compactando El Cache ...");
	bool libre_no_ultima(particion* aux){
		return aux->libre == 'L' && aux->end != mem_total;
	}

	while(1){
		particion* borrar = list_find(cache,(void*)libre_no_ultima);
		if(borrar == NULL){
			break;
		}
		uint32_t size = borrar->size;
		bool by_start(particion* aux){
			return aux->start == borrar->start;
		}
		list_remove_by_condition(cache,(void*)by_start);
		void* inicio = borrar->inicio;
		void compac(particion* aux){
			if(aux->end == mem_total && aux->libre == 'L'){
				aux->inicio = inicio;
				aux->start -= size;
				aux->size += size;
			}
			else if(aux->end == mem_total && aux->libre == 'X'){
				memmove(inicio,aux->inicio,aux->size);
				aux->inicio = inicio;
				aux->start -= size;
				aux->end = aux->start+aux->size;
				aux->fin = aux->inicio+aux->size;
				particion* next = malloc(sizeof(particion));
				next->libre = 'L';
				next->size = size;
				next->inicio = aux->fin;
				next->start = aux->end;
				next->fin = next->inicio+next->size;
				next->end = next->start+next->size;
				next->id_mensaje = -1;
				next->tipo_cola = -1;
				list_add(cache,next);
				inicio = aux->fin;
			}
			else if(aux->fin <= borrar->inicio){
				//No tocar las particiones anteriores de la particion a borrar
			}
			else{
				memmove(inicio,aux->inicio,aux->size);
				aux->inicio = inicio;
				aux->start -= size;
				aux->end = aux->start+aux->size;
				aux->fin = aux->inicio + aux->size;
				inicio = aux->fin;
			}
		}
		list_iterate(cache,(void*)compac);
		free(borrar);
	}
}

void consolidar_particiones_dinamicas(){
	bool es_libre(particion* aux){
		return aux->libre == 'L';
	}
	for(int i=0;i<list_size(cache);i++){
		particion* libre = list_get(cache,i);
		if(!es_libre(libre)){
			continue;
		}
		else{
			bool existe_buddy(particion* aux){
				return (aux->end == libre->start || aux->start == libre->end) && aux->libre == 'L';
			}
			if(list_any_satisfy(cache,(void*)existe_buddy)){
				void aplicar(particion* aux){
					bool anterior(particion* aux){
						return aux->start != libre->start && aux->end == libre->start && aux->libre == 'L';
					}
					bool next(particion* aux){
						return aux->start != libre->start && aux->start == libre->end && aux->libre == 'L';
					}
					particion* ant = list_find(cache,(void*)anterior);
					particion* pos = list_find(cache,(void*)next);
					if(ant != NULL){
						log_warning(logger,"Consolidando El Cache De Las Particiones Con Posiciones Iniciales: %d Y %d ...",ant->start,libre->start);
						uint32_t size = aux->size;
						bool by_start(particion* a){
							return a->start == aux->start;
						}

						particion* borrar = list_find(cache,(void*)by_start);
						list_remove_by_condition(cache,(void*)by_start);

						free(borrar);
						ant->size += size;
						ant->fin = ant->inicio+ant->size;
						ant->end = ant->start+ant->size;
					}
					else if(pos != NULL ){
						log_warning(logger,"Consolidando El Cache De Las Particiones Con Posiciones Iniciales: %d Y %d ...",libre->start,pos->start);
						uint32_t size = pos->size;
						bool by_start(particion* aux){
							return aux->start == pos->start;
						}

						particion* borrar = list_find(cache,(void*)by_start);
						list_remove_by_condition(cache,(void*)by_start);

						free(borrar);
						aux->size += size;
						aux->fin = aux->inicio+aux->size;
						aux->end = aux->start+aux->size;;
					}
				}
				aplicar(libre);
			}
			else{
				continue;
			}
		}
	}
}

void consolidar_buddy_system(){
	bool es_libre(particion* aux){
		return aux->libre == 'L';
	}
	for(int i=0;i<list_size(cache);i++){
		particion* libre = list_get(cache,i);
		if(!es_libre(libre)){
			continue;
		}
		else{
			bool existe_buddy(particion* aux){
				return (aux->end == libre->start || aux->start == libre->end) && aux->libre == 'L' && aux->buddy_size == libre->buddy_size;
			}
			if(list_any_satisfy(cache,(void*)existe_buddy)){
				void aplicar(particion* aux){
					if(libre != NULL){
						bool anterior(particion* aux){
							return aux->start != libre->start && aux->end == libre->start && es_libre(aux) && aux->buddy_size == libre->buddy_size;
						}
						bool next(particion* aux){
							return  aux->start != libre->start && aux->start == libre->end && es_libre(aux) && aux->buddy_size == libre->buddy_size;
						}
						particion* ant = list_find(cache,(void*)anterior);
						particion* pos = list_find(cache,(void*)next);
						if(ant != NULL){
							log_warning(logger,"Consolidando El Cache De Las Particiones Con Posiciones Iniciales: %d Y %d ...",ant->start,libre->start);
							uint32_t end = aux->end;
							bool by_start(particion* a){
								return a->start == aux->start;
							}
							particion* borrar = list_find(cache,(void*)by_start);
							list_remove_by_condition(cache,(void*)by_start);
							free(borrar);
							ant->fin = ant->inicio+ant->buddy_size*2;
							ant->end = end;
							ant->buddy_size = ant->buddy_size*2;
						}
						else if(pos != NULL){
							log_warning(logger,"Consolidando El Cache De Las Particiones Con Posiciones Iniciales: %d Y %d ...",libre->start,pos->start);
							uint32_t end = pos->end;
							bool by_start(particion* aux){
								return aux->start == pos->start;
							}
							particion* borrar = list_find(cache,(void*)by_start);
							list_remove_by_condition(cache,(void*)by_start);
							free(borrar);
							aux->fin = aux->inicio+aux->buddy_size*2;
							aux->end = end;
							aux->buddy_size =aux->buddy_size* 2;
						}
					}
				}
				aplicar(libre);
			}
			else{
				continue;
			}
		}
	}
}

void algoritmo_reemplazo(){
	particion* victima;
	bool ocupada(particion* aux){
		return aux->libre == 'X';
	}
	t_list* ocupadas = list_filter(cache,(void*)ocupada);

	if(string_equals_ignore_case(datos_config->algoritmo_reemplazo,"FIFO")){
		printf("\033[1;37m==========ALGORITMO DE REEMPLAZO:FIFO==========\033[0m\n");
		bool by_id(particion* aux1,particion* aux2){
			return aux1->tiempo_inicial <= aux2->tiempo_inicial;
		}

		list_sort(ocupadas,(void*)by_id);
		victima = list_get(ocupadas,0);
		delete_particion(victima);
	}
	else{
		printf("\033[1;37m==========ALGORITMO DE REEMPLAZO:LRU==========\033[0m\n");
		time_t actual;
		actual = time(NULL);

		bool by_intervalo(particion* aux1,particion* aux2){
			aux1->intervalo = actual-aux1->tiempo_actual;
			aux2->intervalo = actual-aux2->tiempo_actual;
			return aux1->intervalo > aux2->intervalo;
		}

		list_sort(ocupadas,(void*)by_intervalo);
		victima = list_get(ocupadas,0);
		delete_particion(victima);
	}

	list_destroy(ocupadas);
}

void delete_particion(particion* borrar){
	mq* cola = cola_mensaje(borrar->tipo_cola);
	bool by_id(mensaje* aux){
		return aux->id == borrar->id_mensaje;
	}
	mensaje* m = list_find(cola->mensajes,(void*)by_id);

	log_warning(logger,"Particion:%d De Tipo: %s Eliminada  Inicio: %d",borrar->id_particion,get_cola(borrar->tipo_cola),borrar->start);
	borrar->libre = 'L';
	memset(borrar->inicio,0,borrar->size);

	bool libre(particion* p){
		return p->libre == 'L';
	}
	list_remove_by_condition(cola->mensajes,(void*)by_id);


	list_destroy(m->suscriptors_ack);
	list_destroy(m->suscriptors_enviados);

	printf("\033[1;33mMensaje ID:%d De Cola:%s Eliminado\033[0m\n",m->id,get_cola(m->tipo_msg));
	free(m);

	if(string_equals_ignore_case(datos_config->algoritmo_memoria,"PD")){
		pthread_mutex_lock(&mutexAsignada);
		mem_asignada -= borrar->size;
		pthread_mutex_unlock(&mutexAsignada);
		consolidar_particiones_dinamicas();
	}
	else{
		pthread_mutex_lock(&mutexAsignada);
		mem_asignada -=	calcular_size_potencia_dos(borrar->size);
		pthread_mutex_unlock(&mutexAsignada);
		consolidar_buddy_system();
	}

}

void limpiar_cache(){
	printf("\033[1;33mLIMPIANDO CACHE ...\033[0m\n");
	void limpiar(particion* aux){
		free(aux);
	}

	list_destroy_and_destroy_elements(cache,(void*)limpiar);

	free(memoria);
}

uint32_t calcular_size_potencia_dos(uint32_t size){
	uint32_t potencia = 0;
	uint32_t res = 0;
	potencia = log_dos(size);
	res = pow(2,potencia);
	if(res < datos_config->tamanio_min_particion){
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

//dump=====================================================================================================================================================================

void handler_dump(int signo){
	if(signo == SIGUSR1 || signo == SIGINT){
		log_info(logger,"Dump De Cache ...");
		time_t tiempo;
		time(&tiempo);
		struct tm* p;
		p = gmtime(&tiempo);

		log_info(dump,"---------------------------------------------------------------------------------------------------------------------------------------------\n");
		log_info(dump,"Dump:%d/%d/%d %d:%d:%d\n",p->tm_mday,p->tm_mon+1,p->tm_year+1900,p->tm_hour,p->tm_min,p->tm_sec);

		void imprimir(void* ele){
			particion* aux = ele;
			if(aux->libre == 'X'){
				if(string_equals_ignore_case(datos_config->algoritmo_reemplazo,"FIFO")){
					log_info(dump,"Particion %d:%d-%d   [%c]   Size:%db   %s   Cola:<%s>   ID:<%d>\n",
								aux->id_particion,aux->start,aux->end,aux->libre,aux->size,datos_config->algoritmo_reemplazo,get_cola(aux->tipo_cola),aux->id_mensaje);
				}
				else{
					log_info(dump,"Particion %d:%d-%d   [%c]   Size:%db   %s:<%d>   Cola:<%s>   ID:<%d>\n",
								aux->id_particion,aux->start,aux->end,aux->libre,aux->size,datos_config->algoritmo_reemplazo,aux->intervalo,get_cola(aux->tipo_cola),aux->id_mensaje);
				}
			}
			else{
				if(string_equals_ignore_case(datos_config->algoritmo_memoria,"PD")){
					log_info(dump,"Espacio    :%d-%d   [%c]   Size:%db\n",aux->start,aux->end,aux->libre,aux->size);
				}
				else{
					log_info(dump,"Espacio    :%d-%d   [%c]   Size:%db\n",aux->start,aux->end,aux->libre,aux->buddy_size);
				}
			}
		}

		list_iterate(cache,(void*)imprimir);
		log_info(dump,"---------------------------------------------------------------------------------------------------------------------------------------------\n");

		printf("\033[1;33mSIGUSR1 RUNNING...\033[0m\n");
		limpiar_cache();
		terminar_broker(logger, config);
		exit(0);
	}
}

void display(){
	printf("---------------------------------------------------------------------------------------------------------------------------------------------\n");
	void imprimir(void* ele){
		particion* aux = ele;
		if(aux->libre == 'X'){
			if(string_equals_ignore_case(datos_config->algoritmo_reemplazo,"FIFO")){
				printf("Particion %d:%d-%d   [%c]   Size:%db   %s   Cola:<%s>   ID:<%d>\n",
							aux->id_particion,aux->start,aux->end,aux->libre,aux->size,datos_config->algoritmo_reemplazo,get_cola(aux->tipo_cola),aux->id_mensaje);
			}
			else{
				printf("Particion %d:%d-%d   [%c]   Size:%db   %s:<%d>   Cola:<%s>   ID:<%d>\n",
							aux->id_particion,aux->start,aux->end,aux->libre,aux->size,datos_config->algoritmo_reemplazo,aux->intervalo,get_cola(aux->tipo_cola),aux->id_mensaje);
			}
		}
		else{
			if(string_equals_ignore_case(datos_config->algoritmo_memoria,"PD")){
				printf("Espacio    :%d-%d   [%c]   Size:%db\n",aux->start,aux->end,aux->libre,aux->size);
			}
			else{
				printf("Espacio    :%d-%d   [%c]   Size:%db\n",aux->start,aux->end,aux->libre,aux->buddy_size);
			}
		}
	}

	list_iterate(cache,(void*)imprimir);

	printf("---------------------------------------------------------------------------------------------------------------------------------------------\n");

}
