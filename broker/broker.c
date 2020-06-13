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
pthread_t thread_suscripcion;

int cant_busqueda;
int inicio;
uint32_t mem_asignada;
uint32_t mem_total;
uint32_t id_particion;
t_list* cache;
void* memoria;

pthread_rwlock_t lockCache = PTHREAD_RWLOCK_INITIALIZER;
pthread_rwlock_t lockCola = PTHREAD_RWLOCK_INITIALIZER;
pthread_rwlock_t lockMemAsignada = PTHREAD_RWLOCK_INITIALIZER;
pthread_rwlock_t lockParticion = PTHREAD_RWLOCK_INITIALIZER;
pthread_rwlock_t lockImprimir = PTHREAD_RWLOCK_INITIALIZER;
pthread_mutex_t mutexMensaje = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexParticion = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexDump = PTHREAD_MUTEX_INITIALIZER;
pthread_rwlock_t lockId = PTHREAD_RWLOCK_INITIALIZER;
sem_t semHayEspacio;
sem_t semACK;
sem_t semNew;
sem_t semGet;
sem_t semCatch;

int main(){	
    iniciar_broker();
	terminar_broker( logger, config);
}

void sig_handler(int signo){
	handler_dump(SIGUSR1);
}

void iniciar_broker(void){
	iniciar_config("broker.config");
	logger = log_create(datos_config->log_file,"broker",1,LOG_LEVEL_INFO);
	dump = log_create("dump.log","broker",1,LOG_LEVEL_INFO);
	iniciar_colas_mensaje();
	printf("BROKER START!\n");
	iniciar_cache();
	iniciar_servidor();
}

void iniciar_semaforos(){
	sem_init(&semHayEspacio,0,0);
	sem_init(&semACK,0,0);
	sem_init(&semCatch,0,0);
	sem_init(&semGet,0,0);
	sem_init(&semNew,0,0);
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
	process_request(cod_op, cliente_fd);
}

void process_request(int cod_op, int cliente_fd) {
	pthread_mutex_lock(&mutexMensaje);
	if (cod_op == SUSCRITO){
		atender_suscripcion(cliente_fd);
	}
	else{
		if(mem_total-mem_asignada >= datos_config->tamanio_min_particion){
			sem_post(&semHayEspacio);
		}

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

		free(msg);

		mq* cola = cola_mensaje(cod_op);
		if(!list_is_empty(cola->suscriptors)){
			void enviar(suscriber* sus){
				if(check_socket(sus->cliente_fd) == 1){
					enviar_mensajes(sus,cod_op);
				}
			}
			list_iterate(cola->suscriptors,(void*)enviar);
			void ack(suscriber* sus){
				if(check_socket(sus->cliente_fd) == 1){
					atender_ack(sus->cliente_fd);
				}
			}
			list_iterate(cola->suscriptors,(void*)ack);
		}
	}
	pthread_mutex_unlock(&mutexMensaje);
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
	pid_t pid;
	memcpy(&queue_id,msg,sizeof(uint32_t));
	memcpy(&pid,msg+sizeof(uint32_t),sizeof(pid_t));
	printf("Suscripcion:Pid:%d|Queue_id:%d|Socket:%d\n",pid,queue_id,cliente_fd);

	suscriber * sus = malloc(sizeof(suscriber));
	sus->cliente_fd = cliente_fd;
	sus->pid = pid;

	mq* cola = cola_mensaje(queue_id);
	bool existe(suscriber* aux){
		return aux->pid == sus->pid;
	}
	if (!list_any_satisfy(cola->suscriptors,(void*)existe) ){
		list_add(cola->suscriptors, sus);
		log_info(logger,"Proceso %d Suscripto A La Cola %d SUSCRIPCION",pid,queue_id);
		enviar_confirmacion_suscripcion(sus);
	}

	if(!list_is_empty(cola->mensajes)){
		if(check_socket(sus->cliente_fd) == 1){
			enviar_mensajes(sus,queue_id);
			while(!list_is_empty(cola->mensajes)){
				atender_ack(sus->cliente_fd);
			}
		}
	}
	free(msg);
}

void atender_ack(int cliente_fd){
	int tipo,id;
	pid_t pid;
	if(recv(cliente_fd, &tipo, sizeof(int), MSG_WAITALL) == -1)
		tipo = -1;
	if(tipo !=-1){
		void* msg = recibir_mensaje(cliente_fd);
		memcpy(&(id), msg, sizeof(int));
		memcpy(&pid,msg+sizeof(int),sizeof(pid_t));

		printf("ACK Recibido:Pid:%d,Tipo:%d,ID:%d\n",pid,tipo,id);

		bool existe(mensaje* aux){
			return aux->id == id;
		}

		bool by_pid(suscriber* aux){
			return aux->pid == pid;
		}

		mq* cola = cola_mensaje(tipo);
		suscriber* sus = list_find(cola->suscriptors,(void*)by_pid);
		mensaje* item = list_find(cola->mensajes,(void*)existe);

		list_add(item->suscriptors_ack,sus);

		bool estar_ack(suscriber* sus){
			return list_any_satisfy(item->suscriptors_ack,(void*)by_pid);
		}
		if(confirmado_todos_susciptors_mensaje(item) == 1){
			printf("Mensaje Ya Ha Recibido Los ACKs De Todos Sus Suscriptors\n");
			borrar_mensaje(item);
		}
		free(msg);
	}
}

bool confirmado_todos_susciptors_mensaje(mensaje* m){
	bool res = 0;
	suscriber* sus;

	bool by_id(suscriber* aux){
		return aux->pid == sus->pid;
	}
	for(int i=0;i<list_size(m->suscriptors_enviados);i++){
		sus = list_get(m->suscriptors_enviados,i);
		res = list_any_satisfy(m->suscriptors_ack,(void*)by_id);
	}
	return res;
}

void borrar_mensaje(mensaje* m){
	bool by_id_tipo(particion* aux){
		return aux->id_buffer == m->id && aux->tipo_cola == m->tipo_msg;
	}

	particion* borrar = list_find(cache,(void*)by_id_tipo);
	delete_particion(borrar);

	mq* cola = cola_mensaje(m->tipo_msg);
	bool by_id_tipo_mensaje(mensaje* aux){
		return aux->id == m->id && aux->tipo_msg == m->tipo_msg;
	}
	list_remove_by_condition(cola->mensajes,(void*)by_id_tipo_mensaje);
	list_destroy(m->suscriptors_ack);
	list_destroy(m->suscriptors_enviados);
	free(m);
}

//recibir mensajes
void mensaje_new_pokemon(void* msg,int cliente_fd){
	new_pokemon* new = deserializar_new(msg);
	printf("Datos Recibidos: NEW_POKEMON:Pokemon:%s|Size:%d|Pos:[%d,%d]|Cantidad:%d\n",new->name,new->name_size,new->pos.posx,new->pos.posy,new->cantidad);

	mensaje* item = crear_mensaje(NEW_POKEMON,cliente_fd,cola_new->id,new);
	cola_new->id++;

	//almacena
	size_t size = new->name_size + sizeof(uint32_t)*4;
	particion* part_aux = malloc_cache(size);
	if(part_aux != NULL){
		memcpy_cache(part_aux,item->id,NEW_POKEMON,part_aux->inicio,msg,size);
		log_warning(logger,"MENSAJE_ID:%d NEW_POKEMON Almacenado En El Cache Inicio:%d",item->id,part_aux->start);
		list_add(cola_new->mensajes,item);
		enviar_id(item,cliente_fd);
		sem_post(&semNew);
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
	printf("Datos Recibidos:APPEARED_POKEMON:Correlation_ID:%d|Pokemon:%s|Size:%d|Pos:[%d,%d]\n",correlation_id,appeared->name,appeared->name_size,appeared->pos.posx,appeared->pos.posy);

	mensaje* item = crear_mensaje(APPEARED_POKEMON,cliente_fd,cola_appeared->id,appeared);
	cola_appeared->id++;
	bool new_correlacionl(mensaje* ele){
		return ele->id == correlation_id;
	}

	sem_wait(&semNew);
	mensaje* new = list_find(cola_new->mensajes,(void*)new_correlacionl);
	if(new){
		new->id_correlacional = item->id;
		item->id_correlacional = new->id;
	}

	//almacena
	size_t size = appeared->name_size + sizeof(uint32_t)*3;
	particion* part_aux = malloc_cache(size);
	if(part_aux != NULL){
		memcpy_cache(part_aux,item->id,APPEARED_POKEMON,part_aux->inicio,msg+sizeof(uint32_t),size);
		log_warning(logger,"Mensaje APPEARED_POKEMON ID:%d Almacenado En El Cache. Inicio:%d",item->id,part_aux->start);
		list_add(cola_appeared->mensajes,item);
		enviar_id(item,cliente_fd);
	}
	else{
		free(item);
	}
	free(appeared->name);
	free(appeared);
}

void mensaje_catch_pokemon(void* msg,int cliente_fd){
	catch_pokemon* catch = deserializar_catch(msg);
	printf("Datos Recibidos:CATCH_POKEMON:Pokemon:%s|Size:%d|Pos:[%d,%d]\n",catch->name,catch->name_size,catch->pos.posx,catch->pos.posy);
	mensaje* item = crear_mensaje(CATCH_POKEMON,cliente_fd,cola_catch->id,catch);
	cola_catch->id++;

	//almacena
	size_t size = catch->name_size + sizeof(uint32_t)*3;
	particion* part_aux = malloc_cache(size);
	if(part_aux != NULL){
		memcpy_cache(part_aux,item->id,CATCH_POKEMON,part_aux->inicio,msg,size);
		log_warning(logger,"MENSAJE_ID:%d CATCH_POKEMON Almacenado En El Cache. Inicio:%d",item->id,part_aux->start);
		list_add(cola_catch->mensajes,item);
		enviar_id(item,cliente_fd);
		sem_post(&semCatch);
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
	printf("Datos Recibidos:CAUGHT_POKEMON:Correlation_id:%d|Resultado:%d\n",correlation_id,caught->caught);

	mensaje* item = crear_mensaje(CAUGHT_POKEMON,cliente_fd,cola_caught->id,caught);
	cola_caught->id++;
	bool catch_correlacionl(mensaje* aux){
		return aux->id == correlation_id;
	}

	sem_wait(&semCatch);
	mensaje* catch = list_find(cola_catch->mensajes,(void*)catch_correlacionl);
	if(catch){
		catch->id_correlacional = item->id;
		item->id_correlacional = catch->id;
	}

	//almacena
	particion* part_aux = malloc_cache(sizeof(uint32_t));
	if(part_aux != NULL){
		memcpy_cache(part_aux,item->id,CAUGHT_POKEMON,part_aux->inicio,msg+sizeof(uint32_t),sizeof(uint32_t));
		log_warning(logger,"Mensaje CAUGHT_POKEMON ID:%d Almacenado En El Cache. Inicio:%d",item->id,part_aux->start);
		list_add(cola_caught->mensajes,item);
		enviar_id(item,cliente_fd);
	}
	else{
		free(item);
	}
	free(caught);
}

void mensaje_get_pokemon(void* msg,int cliente_fd){
	get_pokemon* get = deserializar_get(msg);
	printf("Datos Recibidos:GET_POKEMON:Pokemon:%s|Size:%d\n",get->name,get->name_size);

	mensaje* item = crear_mensaje(GET_POKEMON,cliente_fd,cola_get->id,get);
	cola_get->id++;
	//almacena
	size_t size = get->name_size + sizeof(uint32_t);

	particion* part_aux = malloc_cache(size);
	if(part_aux != NULL){
		memcpy_cache(part_aux,item->id,GET_POKEMON,part_aux->inicio,msg,size);
		log_warning(logger,"MENSAJE_ID:%d GET_POKEMON Almacenado En El Cache. Inicio:%d",item->id,part_aux->start);
		list_add(cola_get->mensajes,item);
		enviar_id(item,cliente_fd);
		sem_post(&semGet);
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
	printf("Datos Recibidos:LOCALIZED_POKEMON:Correlation_id:%d|Pokemon:%s|Size:%d|Cant_posiciones:%d\n",correlation_id,localized->name,localized->name_size,localized->cantidad_posiciones);
	void show(pos_cant* pos){
		printf("Pos:[%d,%d]|Cantidad:%d\n",pos->posx,pos->posy,pos->cant);
		free(pos);
	}
	list_iterate(localized->pos_cant,(void*)show);
	mensaje* item = crear_mensaje(LOCALIZED_POKEMON,cliente_fd,cola_localized->id,localized);
	cola_localized->id++;
	bool get_correlacionl(mensaje* aux){
		return aux->id == correlation_id;
	}

	sem_wait(&semGet);
	mensaje* get = list_find(cola_get->mensajes,(void*)get_correlacionl);
	if(get){
		get->id_correlacional = item->id;
		item->id_correlacional = get->id;
	}

	//almacena
	size_t size = localized->name_size+sizeof(uint32_t)*2+sizeof(pos_cant)*localized->cantidad_posiciones;
	particion* part_aux = malloc_cache(size);
	if(part_aux != NULL){
		memcpy_cache(part_aux,item->id,LOCALIZED_POKEMON,part_aux->inicio,msg+sizeof(uint32_t),size);
		log_warning(logger,"Mensaje LOCALIZED_POKEMON ID:%d Almacenado En El Cache. Inicio:%d",item->id,part_aux->start);
		list_add(cola_localized->mensajes,item);
		enviar_id(item,cliente_fd);
	}
	else{
		free(item);
	}
	free(localized->name);
	free(localized);
}

//enviar mensajes
void enviar_mensajes(suscriber* sus,int tipo_cola){
	mq* cola = cola_mensaje(tipo_cola);
	bool no_enviado(mensaje* m){
		bool by_id(suscriber* aux){
			return aux->pid == sus->pid;
		}
		return !list_any_satisfy(m->suscriptors_enviados,(void*)by_id);
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
			return p->id_buffer == m->id;
		}
		particion* aux = list_find(particiones_tipo,(void*)by_id);
		list_add(particiones_filtradas,aux);
	}

	t_paquete* enviar = crear_paquete(tipo_cola);
	void agregar(particion* aux){
		agregar_paquete(enviar,aux,sus,tipo_cola);
	}

	list_iterate(particiones_filtradas,(void*)agregar);
	enviar_paquete(enviar,sus->cliente_fd);
	log_info(logger,"Mensaje Enviado A Proceso %d",sus->pid);
	list_destroy(mensajes);
	list_destroy(particiones_tipo);
	list_destroy(particiones_filtradas);

	eliminar_paquete(enviar);
}

void agregar_paquete(t_paquete* enviar,particion* aux,suscriber* sus,uint32_t tipo){
	void* buffer;
	uint32_t size,id;
	switch(tipo){
		case NEW_POKEMON:{
			id = aux->id_buffer;
			size = sizeof(uint32_t)+aux->size+1;
			buffer = malloc(size);
			memset(buffer,0,size);
			memcpy(buffer,&id,sizeof(uint32_t));
			memcpy(buffer+sizeof(uint32_t),aux->inicio,aux->size);
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
			id = catch->id;
			//id = aux->id_buffer;
			size = sizeof(uint32_t)*2;
			buffer = malloc(size);
			memset(buffer,0,size);
			memcpy(buffer,&id,sizeof(uint32_t));
			memcpy(buffer+sizeof(uint32_t),aux->inicio,sizeof(bool));
			break;
		}
		case APPEARED_POKEMON:{
			bool new_correlacional(mensaje* msj){
				return msj->id == aux->id_buffer;
			}
			mensaje* new = list_find(cola_new->mensajes,(void*)new_correlacional);
			id = new->id;
			//id = aux->id_buffer;
			size = sizeof(uint32_t)+aux->size+1;
			buffer = malloc(size);
			memset(buffer,0,size);
			memcpy(buffer,&id,sizeof(uint32_t));
			memcpy(buffer+sizeof(uint32_t),aux->inicio,aux->size);
			break;
		}
		case LOCALIZED_POKEMON:{
			bool get_correlacional(mensaje* msj){
				return msj->id == aux->id_buffer;
			}
			mensaje* get = list_find(cola_get->mensajes,(void*)get_correlacional);
			id = get->id;

			//id = aux->id_buffer;
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

void enviar_id(mensaje *item,int socket_cliente){
	send(socket_cliente,&item->id, sizeof(int), 0);
	printf("ID_Mensaje:%d Enviado\n",item->id);
}

void enviar_confirmacion_suscripcion(suscriber* sus){
	int ack = ACK;
	send(sus->cliente_fd,&ack, sizeof(int), 0);
	printf("Confirmacion De Suscripcion Enviada\n");
}

//MEMORIA------------------------------------------------------------------------------------------------------------------
void iniciar_cache(){
	cant_busqueda = 0;
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
	header->start = inicio;
	header->inicio = memoria;
	header->fin = header->inicio + mem_total;
	header->tiempo = time(NULL);
	list_add(cache,header);
	signal(SIGINT,sig_handler);
}

particion* malloc_cache(uint32_t size){
	sem_wait(&semHayEspacio);
	particion* elegida;
	if(size <= mem_total - mem_asignada){
		if(string_equals_ignore_case(datos_config->algoritmo_memoria,"PD")){
			printf("==========PARTICIONES DINAMICAS==========\n");
			elegida = particiones_dinamicas(size);
		}
		else{
			printf("==========BUDDY SYSTEM==========\n");
			elegida = buddy_system(size);
		}
	}
	else if(size > mem_total - mem_asignada){
		printf("No Hay Suficiente Espacio!\n");
		return 0;
	}
	else{
		printf("Size Superado Al Size Maximo De La Memoria!\n");
		return 0;
	}
	return elegida;
}

void* memcpy_cache(particion* part,uint32_t id_buf,uint32_t tipo_cola,void* destino,void* buf,uint32_t size){
	if(part != NULL){
		part->id_buffer = id_buf;
		part->tipo_cola = tipo_cola;
		part->libre = 'X';
	}
	return memcpy(destino,buf,size);
}

void free_cache(){
	algoritmo_reemplazo();
}

void delete_particion(particion* borrar){
	log_warning(logger,"Particion: %d Eliminada  Inicio: %p",borrar->id_particion,borrar->inicio);
	borrar->libre = 'L';
	borrar->tipo_cola = -1;
	borrar->id_buffer = -1;
	mem_asignada -= borrar->size;
}

void compactar_cache(){
	log_warning(logger,"Compactando El Cache ...");
	particion* ultima = list_get(cache,list_size(cache)-1);
	bool libre_no_ultima(particion* aux){
		return aux->libre == 'L' && aux->fin != ultima->fin;
	}

	while(1){
		particion* borrar = list_find(cache,(void*)libre_no_ultima);
		if(borrar == NULL){
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
	pthread_mutex_lock(&mutexParticion);
	particion* elegida;
	uint32_t part_size = size;
	if(size < datos_config->tamanio_min_particion){
		part_size =  datos_config->tamanio_min_particion;
	}
	elegida = algoritmo_particion_libre(part_size);
	particion* ultima =  list_get(cache,list_size(cache)-1);
	if(elegida != NULL && elegida->id_particion == ultima->id_particion){
		elegida->size = part_size;
		elegida->fin = elegida->inicio+part_size;
		elegida->end = elegida->start+part_size;
		elegida->libre = 'X';
		elegida->tiempo = time(NULL);
		id_particion++;
		mem_asignada += part_size;
		particion* next = malloc(sizeof(particion));
		next->id_particion = id_particion;
		next->libre = 'L';
		next->size = mem_total-mem_asignada;
		next->inicio = elegida->fin;
		next->start = elegida->end;
		next->fin = next->inicio+next->size;
		next->end = next->start+next->size;
		next->tiempo = time(NULL);
		next->id_buffer = -1;
		next->tipo_cola = -1;
		list_add(cache,next);
	}
	else if(elegida != NULL && elegida->id_particion != ultima->id_particion){
		uint32_t size_original = elegida->size;
		elegida->size = part_size;
		elegida->fin = elegida->inicio+elegida->size;
		elegida->end = elegida->start+elegida->size;
		elegida->libre = 'X';
		elegida->tiempo = time(NULL);
		id_particion++;
		mem_asignada += part_size;
		if(size_original - size > 0){
			particion* next = malloc(sizeof(particion));
			next->libre = 'X';
			next->size = size_original - part_size;
			next->inicio = elegida->fin;
			next->start = elegida->end;
			next->fin = next->inicio+next->size;
			next->end = next->start+next->size;
			next->tiempo = time(NULL);
			next->id_buffer = -1;
			next->tipo_cola = -1;
			bool posicion(particion* aux){
				return aux->inicio <= elegida->fin;
			}
			int pos = list_count_satisfying(cache,(void*)posicion);
			pthread_rwlock_wrlock(&lockCache);
			list_add_in_index(cache,pos,next);
			pthread_rwlock_unlock(&lockCache);
		}
	}
	else{
		cant_busqueda++;
		printf("No Hay Suficiente Espacio  Cantidad De Busqueda:%d\n",cant_busqueda);
		if(cant_busqueda <= datos_config->frecuencia_compactacion){
			if(cant_busqueda == datos_config->frecuencia_compactacion){
				compactar_cache();
			}
		}
		else{
			free_cache();
			cant_busqueda = 0;
		}
	}
	pthread_mutex_unlock(&mutexParticion);
	return elegida;
}

particion* buddy_system(uint32_t size){
	uint32_t size_particion = calcular_size_potencia_dos(size);
	if(size_particion < datos_config->tamanio_min_particion){
		size_particion =  datos_config->tamanio_min_particion;
	}
	particion* elegida;
	bool libre_suficiente(particion* aux){
		return aux->libre == 'L' && aux->size >= size_particion;
	}
	if(size_particion < mem_total){
		if(size_particion > (mem_total/2)){
			elegida = list_find(cache,(void*)libre_suficiente);
			if(elegida!=NULL){
				elegida->libre = 'X';
				elegida->size = size_particion;
				mem_asignada += elegida->size;
			}
		}
		else{
			uint32_t pow_size = log_dos(size_particion);
			particion* primera = list_find(cache,(void*)libre_suficiente);
			uint32_t pow_pri = log_dos(primera->size);
			void aplicar(particion* aux){
				bool libre_suficiente(particion* p){
					return p->libre == 'L' && p->size >= size_particion && p->size >= datos_config->tamanio_min_particion;
				}
				particion* vic = list_find(cache,(void*)libre_suficiente);
				particion* ultima = list_get(cache,list_size(cache)-1);
				if(aux->inicio==vic->inicio && aux->fin==ultima->fin){
					uint32_t s = aux->size/2;
					vic->size = s ;
					vic->fin = vic->inicio+vic->size;
					vic->end = vic->start+vic->size;
					particion* next = malloc(sizeof(particion));
					next->size = s;
					next->inicio = vic->fin;
					next->start = vic->end;
					next->fin = next->inicio+next->size;
					next->end = next->start+next->size;
					next->libre = 'L';
					next->id_buffer = -1;
					next->tipo_cola = -1;
					list_add(cache,next);
				}
				else if(aux->inicio==vic->inicio && aux->size==size_particion){
					vic->size = size_particion;
					vic->libre = 'X';
					mem_asignada += elegida->size;
				}
				else if(aux->inicio==vic->inicio){
					uint32_t s = aux->size/2;
					vic->size = s ;
					vic->fin = vic->inicio+vic->size;
					vic->end = vic->start+vic->size;
					particion* next = malloc(sizeof(particion));
					next->size = s;
					next->inicio = vic->fin;
					next->start = vic->end;
					next->fin = next->inicio+next->size;
					next->end = next->start+next->size;
					next->libre = 'L';
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
				list_map(cache,(void*)aplicar);
			}
			elegida = list_find(cache,(void*)libre_suficiente);
		}
		elegida->id_particion = id_particion;
		elegida->size = size;
		id_particion++;
	}
	return elegida;
}

particion* algoritmo_particion_libre(uint32_t size){
	particion* elegida;
	bool libre_suficiente(particion* aux){
		return aux->libre == 'L' && aux->size >= datos_config->tamanio_min_particion && aux->size >= size;
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
		t_list* aux = list_filter(cache,(void*)libre_suficiente);
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
			return aux->libre == 'X';
		}
		aux = list_sorted(cache,(void*)by_id);
		victima = list_find(aux,(void*)ocupada);
		delete_particion(victima);
		list_destroy(aux);
	}
	else{
		printf("==========LRU==========\n");
		t_list* sorted = list_duplicate(cache);
		time_t actual;
		particion* ultima = list_get(cache,list_size(cache)-1);
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
		list_destroy(sorted);
	}
	return victima;
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

void handler_dump(int signo){//.txt|.log
	if(signo == SIGUSR1){
		log_info(logger,"Dump De Cache ...");
		//FILE *dump;
		//dump = fopen("dump.txt","w+");
		pthread_mutex_lock(&mutexDump);
		time_t tiempo;
		time(&tiempo);
		struct tm* p;
		p = gmtime(&tiempo);

		/*fprintf(dump,"---------------------------------------------------------------------------------------------------------------------------------------------\n");
		fprintf(dump,"Dump:%d/%d/%d %d:%d:%d\n",p->tm_mday,p->tm_mon+1,p->tm_year+1900,p->tm_hour-3,p->tm_min,p->tm_sec);
		char caracter(uint32_t nro){
			char res = 'L';
			if(nro != 1){
				res = 'X';
			}
			return res;
		}
		pthread_rwlock_wrlock(&lockImprimir);
		void imprimir(void* ele){
			pthread_rwlock_rdlock(&lockParticion);
			particion* aux = ele;
			pthread_rwlock_unlock(&lockParticion);
			if(aux->libre == 0){
				fprintf(dump,"Paricion %d:%d-%d   [%c]   Size:%db   %s:<%d>   Cola:<%d>   ID:<%d>\n",
							aux->id_particion,aux->start,aux->end,caracter(aux->libre),aux->size,
							datos_config->algoritmo_reemplazo,1,aux->tipo_cola,aux->id_buffer);
			}
			else{
				fprintf(dump,"Espacio    :%d-%d   [%c]   Size:%db\n",aux->start,aux->end,caracter(aux->libre),aux->size);
			}
		}
		pthread_rwlock_unlock(&lockImprimir);
		pthread_rwlock_rdlock(&lockCache);
		list_iterate(cache,(void*)imprimir);
		pthread_rwlock_unlock(&lockCache);
		fprintf(dump,"---------------------------------------------------------------------------------------------------------------------------------------------\n");
		fclose(dump);*/

		log_info(dump,"---------------------------------------------------------------------------------------------------------------------------------------------\n");
		log_info(dump,"Dump:%d/%d/%d %d:%d:%d\n",p->tm_mday,p->tm_mon+1,p->tm_year+1900,p->tm_hour,p->tm_min,p->tm_sec);

		void imprimir(void* ele){
			particion* aux = ele;
			if(aux->libre == 'X'){
				log_info(dump,"Paricion %d:%d-%d   [%c]   Size:%db   %s:<%d>   Cola:<%d>   ID:<%d>\n",
							aux->id_particion,aux->start,aux->end,aux->libre,aux->size,
							datos_config->algoritmo_reemplazo,1,aux->tipo_cola,aux->id_buffer);
			}
			else{
				log_info(dump,"Espacio    :%d-%d   [%c]   Size:%db\n",aux->start,aux->end,aux->libre,aux->size);
			}
		}
		pthread_rwlock_rdlock(&lockCache);
		list_iterate(cache,(void*)imprimir);
		pthread_rwlock_unlock(&lockCache);
		log_info(dump,"---------------------------------------------------------------------------------------------------------------------------------------------\n");
		pthread_mutex_unlock(&mutexDump);

		printf("SIGUSR1 RUNNING...\n");
		exit(0);
	}
}

void limpiar_cache(){
	printf("=====LIMPIANDO=====\n");

}
