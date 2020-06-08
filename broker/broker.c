#include"broker.h"

t_log* logger;
t_config* config;
config_broker* datos_config;

t_list* queues[6];
t_list* suscribers[6];
pthread_t thread_servidor;
sem_t semSend;

int cant_busqueda;
uint32_t cache_size;
uint32_t min_size;
uint32_t mem_asignada;
uint32_t mem_total;
uint32_t id_particion;
t_list* cache;//una lista de particiones
void* memoria;

sem_t semHayEspacio;
sem_t semNoVacio;
pthread_rwlock_t lockSus = PTHREAD_RWLOCK_INITIALIZER;


int main(){	
    iniciar_broker();
	terminar_broker( logger, config);
}

void iniciar_broker(void){
	logger = log_create("broker.log","broker",1,LOG_LEVEL_INFO);
	iniciar_config("broker.config");
	build_queues();
	build_suscribers();
	sem_init(&semSend,0,1);
	start_sender_thread();
	log_info(logger,"BROKER START!");
	iniciar_memoria();
	iniciar_servidor();
	log_info(logger,"creating server");
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

    freeaddrinfo(servinfo);

    while(1)
    	esperar_cliente(socket_servidor);
}

void esperar_cliente(int socket_servidor){
	struct sockaddr_in dir_cliente;

	int tam_direccion = sizeof(struct sockaddr_in);
	log_info(logger,"Waiting Client");
	int socket_cliente = accept(socket_servidor, (void*) &dir_cliente, &tam_direccion);
	int* cliente_fd = malloc(sizeof(int));
	*cliente_fd = socket_cliente;
	log_info(logger,"Client received");
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
	log_info(logger,"Enter to process request, cod_op: %d", cod_op);
	process_request(cod_op, cliente_fd);
}

void process_request(int cod_op, int cliente_fd) {
	queue_item *item = malloc(sizeof(queue_item));
	void * confirmation = malloc(sizeof(int)*2);;
	
	log_info(logger,"Processing request, cod_op: %d", cod_op);

	if (cod_op == SUSCRITO) atender_suscripcion(cliente_fd);
	
	else if (cod_op == ACK) atender_ack( cliente_fd);

	void* msg = recibir_mensaje(cliente_fd);
	item->id = get_id();

	if (cod_op == NEW_POKEMON){
		printf("NEW_POKEMON\n");
		new_pokemon* new = deserializar_new(msg);
		printf("Pokemon:%s\n",new->name);
		printf("Size:%d\n",new->name_size);
		printf("Pos:[%d,%d]\n",new->pos.posx,new->pos.posy);
		printf("Cantidad:%d\n",new->cantidad);
		//almacena
		particion* part_aux = malloc_cache(new->name_size+sizeof(uint32_t)*4);
		memcpy_cache(part_aux,item->id,cod_op,part_aux->inicio,&new->name_size,sizeof(uint32_t));
		memcpy_cache(part_aux,item->id,cod_op,part_aux->inicio+sizeof(uint32_t),new->name,new->name_size);
		memcpy_cache(part_aux,item->id,cod_op,part_aux->inicio+sizeof(uint32_t)+new->name_size,&new->pos.posx,sizeof(uint32_t));
		memcpy_cache(part_aux,item->id,cod_op,part_aux->inicio+sizeof(uint32_t)*2+new->name_size,&new->pos.posy,sizeof(uint32_t));
		memcpy_cache(part_aux,item->id,cod_op,part_aux->inicio+sizeof(uint32_t)*3+new->name_size,&new->cantidad,sizeof(uint32_t));
		log_warning(logger,"MENSAJE_ID:%d NEW_POKEMON Almacenado En El Cache Inicio:%p",item->id,part_aux->inicio);
		item->message = new;
	}
	else if (cod_op == APPEARED_POKEMON){
		printf("APPEARED_POKEMON\n");
		memcpy(&(item->correlation_id), msg, sizeof(int));
		printf("Correlation id:%d\n",item->correlation_id);
		appeared_pokemon* appeared = deserializar_appeared(msg);
		printf("Pokemon:%s\n",appeared->name);
		printf("Size:%d\n",appeared->name_size);
		printf("Pos:[%d,%d]\n",appeared->pos.posx,appeared->pos.posy);
		//almacena
		particion* part_aux = malloc_cache(appeared->name_size+sizeof(uint32_t)*3);
		memcpy_cache(part_aux,item->id,cod_op,part_aux->inicio,&appeared->name_size,sizeof(uint32_t));
		memcpy_cache(part_aux,item->id,cod_op,part_aux->inicio+sizeof(uint32_t),appeared->name,appeared->name_size);
		memcpy_cache(part_aux,item->id,cod_op,part_aux->inicio+sizeof(uint32_t)+appeared->name_size,&appeared->pos.posx,sizeof(uint32_t));
		memcpy_cache(part_aux,item->id,cod_op,part_aux->inicio+sizeof(uint32_t)*2+appeared->name_size,&appeared->pos.posy,sizeof(uint32_t));
		log_warning(logger,"MENSAJE_ID:%d APPEARED_POKEMON Almacenado En El Cache Inicio:%p",item->id,part_aux->inicio);
		item->message = appeared;
	}
	else if (cod_op == CATCH_POKEMON){
		printf("CATCH_POKEMON\n");
		catch_pokemon* catch = deserializar_catch(msg);
		printf("Pokemon:%s\n",catch->name);
		printf("Size:%d\n",catch->name_size);
		printf("Pos:[%d,%d]\n",catch->pos.posx,catch->pos.posy);
		//almacena
		particion* part_aux = malloc_cache(catch->name_size+sizeof(uint32_t)*3);
		memcpy_cache(part_aux,item->id,cod_op,part_aux->inicio,&catch->name_size,sizeof(uint32_t));
		memcpy_cache(part_aux,item->id,cod_op,part_aux->inicio+sizeof(uint32_t),catch->name,catch->name_size);
		memcpy_cache(part_aux,item->id,cod_op,part_aux->inicio+sizeof(uint32_t)+catch->name_size,&catch->pos.posx,sizeof(uint32_t));
		memcpy_cache(part_aux,item->id,cod_op,part_aux->inicio+sizeof(uint32_t)*2+catch->name_size,&catch->pos.posx,sizeof(uint32_t));
		log_warning(logger,"MENSAJE_ID:%d CATCH_POKEMON Almacenado En El Cache Inicio:%p",item->id,part_aux->inicio);
		item->message = catch;
	}
	else if (cod_op == CAUGHT_POKEMON){
		printf("CAUGHT_POKEMON\n");
		memcpy(&(item->correlation_id), msg, sizeof(int));
		printf("Correlation id:%d\n",item->correlation_id);
		caught_pokemon* caught = deserializar_caught(msg);
		printf("Resultado:%d\n",caught->caught);
		particion* part_aux = malloc_cache(sizeof(uint32_t));
		//almacena
		memcpy_cache(part_aux,item->id,cod_op,part_aux->inicio,&caught->caught,sizeof(uint32_t));
		log_warning(logger,"MENSAJE_ID:%d CAUGHT_POKEMON Almacenado En El Cache. Inicio:%p",item->id,part_aux->inicio);
		item->message = caught;
	}
	else if (cod_op == GET_POKEMON){
		printf("GET_POKEMON\n");
		get_pokemon* get = deserializar_get(msg);
		printf("Pokemon:%s\n",get->name);
		printf("Size:%d\n",get->name_size);
		//almacena
		particion* part_aux = malloc_cache(get->name_size+sizeof(uint32_t));
		memcpy_cache(part_aux,item->id,cod_op,part_aux->inicio,&get->name_size,sizeof(uint32_t));
		memcpy_cache(part_aux,item->id,cod_op,part_aux->inicio+sizeof(uint32_t),get->name,get->name_size);
		log_warning(logger,"MENSAJE_ID:%d GET_POKEMON Almacenado En El Cache. Inicio:%p",item->id,part_aux->inicio);
		item->message = get;
	}
	else if (cod_op == LOCALIZED_POKEMON){
		printf("LOCALIZED_POKEMON\n");
		memcpy(&(item->correlation_id), msg, sizeof(int));
		printf("Correlation id:%d\n",item->correlation_id);
		localized_pokemon* localized = deserializar_localized(msg);
		printf("Pokemon:%s\n",localized->name);
		printf("Size:%d\n",localized->name_size);
		printf("Cant_posiciones:%d\n",localized->cantidad_posiciones);
		for(int i=0;i<localized->cantidad_posiciones;i++){
			printf("Pos[%d]:[%d,%d]\n",i,localized->pos[i].posx,localized->pos[i].posy);
		}
		//almacena
		particion* part_aux = malloc_cache(localized->name_size+sizeof(uint32_t)*2+sizeof(position)*localized->cantidad_posiciones);
		memcpy_cache(part_aux,item->id,cod_op,part_aux->inicio,&localized->name_size,sizeof(uint32_t));
		memcpy_cache(part_aux,item->id,cod_op,part_aux->inicio+sizeof(uint32_t),localized->name,localized->name_size);
		memcpy_cache(part_aux,item->id,cod_op,part_aux->inicio+sizeof(uint32_t)+localized->name_size,&localized->cantidad_posiciones,sizeof(uint32_t));
		int desplazamineto = sizeof(uint32_t)*2+localized->name_size;
		for(int i=0;i<localized->cantidad_posiciones;i++){
			memcpy_cache(part_aux,item->id,cod_op,part_aux->inicio+desplazamineto,&localized->pos[i].posx,sizeof(uint32_t));
			desplazamineto+=sizeof(uint32_t);
			memcpy_cache(part_aux,item->id,cod_op,part_aux->inicio+desplazamineto,&localized->pos[i].posy,sizeof(uint32_t));
			desplazamineto+=sizeof(uint32_t);
		}
		log_warning(logger,"MENSAJE_ID:%d LOCALIZED_POKEMON Almacenado En El Cache. Inicio:%p",item->id,part_aux->inicio);
		item->message = localized;
	}
	display_cache();
	
	//Pushear en la cola correspondiente con id, correlationId 
	list_add(queues[cod_op -1], item);
	
	//Responder id al que me envio el req (cliente_fd)
	enviar_id(item,cliente_fd);

	//Aca deberia levantar algun semaforo que habilite un proceso que me mande el mensaje que se agrego a la cola a los demas
	sem_post(&semSend);

	free(msg);
	free(confirmation);
	pthread_exit(NULL);
}

void enviar_id(queue_item *item,int socket_cliente){
	send(socket_cliente,&item->id, sizeof(int), 0);
	printf("ID_Mensaje Enviado\n");
}

void enviar_confirmacion_suscripcion(suscriber* sus){
	int ack = ACK;
	send(sus->cliente_fd,&ack, sizeof(int), 0);
	printf("Confirmacion De Suscripcion Enviada\n");
}

void atender_suscripcion(int cliente_fd){
	printf("SUSCRIPCION\n");
	void * msg = recibir_mensaje(cliente_fd);
	int queue_id,pid;
	memcpy(&queue_id,msg,sizeof(uint32_t));
	memcpy(&pid,msg+sizeof(uint32_t),sizeof(uint32_t));
	printf("Pid:%d\n",pid);
	printf("Queue_id:%d\n",queue_id);

	memcpy(&(queue_id), msg, sizeof(int));
	msg+=sizeof(int);
	memcpy(&(pid), msg, sizeof(int));

	if (pid == -1){
		//Esto pasa cuando entra por primera vez
		pid = generate_pid();
	}

	suscriber * sus = malloc(sizeof(suscriber));
	sus->cliente_fd = cliente_fd;
	sus->pid = pid;
	
	bool existe(suscriber* aux){
		return aux->pid == pid;
	}
	if (!list_any_satisfy(suscribers[queue_id -1],(void*)existe) ){
		//Agregar cliente_fd a una lista de suscribers
		list_add(suscribers[queue_id -1], sus);
		log_info(logger,"Proceso %d Suscripto A La Cola %d SUSCRIPCION",sus->pid,queue_id);
	}	

	//Enviar confirmacion
	enviar_confirmacion_suscripcion(sus);

	//Enviar todos los mensajes anteriores de la cola $queue_id
	enviar_cacheados( sus,  queue_id);
		
	while(1){
		//Dejo abierto este hilo a la espera de que haya un nuevo mensaje en la cola $queue_id
		sem_wait(&semSend);
		enviar_cacheados( sus,  queue_id);
	}
		
}

//enviar mensajes
void enviar_cacheados(suscriber * sus, int queue_id){//modificar
	void enviar(queue_item* queue_item){
		bool existe(suscriber * sus_aux){
			return sus_aux->pid == sus->pid;
		}
		if (!list_any_satisfy(queue_item->recibidos,(void*)existe) ){
			int size;
			size =0;
			//void * paquete = crear_paquete(queue_id, queue_item, &size);
			//send(sus->cliente_fd, paquete, size, 0);
			if(!list_any_satisfy(queue_item->enviados, (void*)existe)){
				list_add(queue_item->enviados, sus);
			}
		}
	}
	list_iterate(queues[queue_id -1],(void*)enviar);
}

//pueden estar en utils
void* serializar_paq(t_paquete* paquete, int bytes){
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
	void* a_enviar = serializar_paq(paquete, bytes);
	send(socket_cliente, a_enviar, bytes, 0);

	free(a_enviar);
}

void eliminar_paquete(t_paquete* paquete){
	free(paquete->buffer->stream);
	free(paquete->buffer);
	free(paquete);
}

//------------------------------------------------------------------------------------------------------------------------------
void atender_ack( int cliente_fd){
	int pid, queue_id, id;
	void *msg=malloc(sizeof(int) * 3);
	suscriber * sus = malloc(sizeof(suscriber));
	queue_item * new;
	//en msg viene id, correlation_id y queue_id


	//recibo 12 bytes restantes
	recv(cliente_fd, msg, sizeof(int) * 3, MSG_WAITALL);

	memcpy(&(pid), msg, sizeof(int));
	msg+=sizeof(int);
	memcpy(&(queue_id), msg, sizeof(int));
	msg+=sizeof(int);
	memcpy(&(id), msg, sizeof(int));

	sus->pid = pid;
	sus->cliente_fd = cliente_fd;

	bool existe(queue_item* aux){
		return aux->id == id;
	}
	new = list_find(queues[queue_id -1],(void*)existe);
	list_remove_by_condition(queues[queue_id -1], (void*)existe);
	list_add(new->recibidos, sus);
	list_add(queues[queue_id -1], new);
	
	pthread_exit(NULL);
}

int get_id(){
	//PROGRAMAME
	return 10;
}
int generate_pid(){
	//PROGRAMAME
	return 1;
}


void build_queues(void){
	for (int i = 0; i < 6; i++)
		queues[i] =	list_create();
}
void build_suscribers(void){
	for (int i = 0; i < 6; i++)
		suscribers[i] =	list_create();
	//Aca hay que levantar de la memoria todos los suscribers y generar suscribers 
}

void start_sender_thread(void){
	//Aca hay que hacer pthread create de sender thread
}
// void sender_thread(void){
// 	while (1){
// 		for (int i = 0; i < 6; i++){
// 			for (int j = 0; j < sizeof(suscribers)/sizeof(suscriber); j++){
// 				for (int k = 0; k < sizeof(suscribers[i][j].sended)/sizeof(sent); k++){
// 					if (!suscribers[i][j].sended[k].ack){
// 						//Send a suscribers[i][j].cliente_fd el mensaje con id suscribers[i][j].sended[k].id ;
// 					}
					
// 				}
				
				
// 			}
			
			
// 		}
		
// 	}
	
// }

void terminar_broker( t_log* logger, t_config* config){
	for (int i = 0; i < 6; i++)
		list_destroy(queues[i]);
	log_destroy(logger);
	config_destroy(config);
}

//MEMORIA------------------------------------------------------------------------------------------------------------------
void iniciar_memoria(){
	iniciar_semaforos();
	iniciar_cache();
	//signal(SIGINT,sig_handler);

}

void iniciar_semaforos(){
	sem_init(&semHayEspacio,0,1);
	sem_init(&semNoVacio,0,0);
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
	mem_asignada -= borrar->size;
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
		id_particion++;
		mem_asignada += part_size;
		particion* next = malloc(sizeof(particion));
		next->id_particion = id_particion;
		next->libre = 1;
		next->size = mem_total-mem_asignada;
		next->inicio = elegida->fin;
		next->fin = next->inicio+next->size;
		next->tiempo = time(NULL);
		next->id_buffer = -1;
		next->tipo_cola = -1;
		list_add(cache,next);
	}
	else if(elegida != NULL && elegida->id_particion != ultima->id_particion){
		uint32_t size_original = elegida->size;
		elegida->size = part_size;
		elegida->fin = elegida->inicio+elegida->size;
		elegida->libre = 0;
		elegida->tiempo = time(NULL);
		id_particion++;
		mem_asignada += part_size;
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
			int pos = list_count_satisfying(cache,(void*)posicion);
			list_add_in_index(cache,pos,next);
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
			elegida = list_find(cache,(void*)libre_suficiente);
			if(elegida!=NULL){
				elegida->libre = 0;
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
					return p->libre == 1 && p->size >= size_particion && p->size >= datos_config->tamanio_min_compactacion;
				}
				particion* vic = list_find(cache,(void*)libre_suficiente);
				particion* ultima = list_get(cache,list_size(cache)-1);
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
					list_add(cache,next);
				}
				else if(aux->inicio==vic->inicio && aux->size==size_particion){
					vic->size = size_particion;
					vic->libre = 0;
					mem_asignada += elegida->size;
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
		return aux->libre == 1 && aux->size >= datos_config->tamanio_min_compactacion;
	}
	if(string_equals_ignore_case(datos_config->algoritmo_particion_libre,"FF")){
		printf("==========FIRST FIT==========\n");
		elegida = list_find(cache,(void*)libre_suficiente);
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
			return aux->libre == 0;
		}
		aux = list_sorted(cache,(void*)by_id);
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
		list_iterate(cache,(void*)imprimir);
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
	list_iterate(cache,(void*)imprimir);
	printf("------------------------------------------------------------------------------\n");
}

void limpiar_cache(){
	printf("=====LIMPIANDO=====\n");

}
