#include "broker.h"

t_log* logger;
t_config* config;
config_broker* datos_config;
pthread_t thread_servidor;
pthread_t thread_message;
mq* cola_get;
mq* cola_localized;
mq* cola_catch;
mq* cola_caught;
mq* cola_new;
mq* cola_appeared;

int cant_busqueda;
uint32_t cache_size;
uint32_t min_size;
void* buf;
uint32_t mem_asignada;
uint32_t mem_total;
uint32_t id_particion;
t_list* cache;
uint32_t cant_fallida;

sem_t semHayEspacio;
sem_t semNoVacio;

int main(void){
	iniciar_broker();

	return EXIT_SUCCESS;
}

void sig_handler(int signo){
	handler_dump(SIGUSR1);
}

void iniciar_broker(){
	logger = log_create("broker.log","broker",1,LOG_LEVEL_INFO);
	printf("BROKER START!\n");
	iniciar_config("broker.config");
	iniciar_semaforos();
	iniciar_cache();
	signal(SIGINT,sig_handler);
	iniciar_colas_mensaje();
	iniciar_servidor();
}

void iniciar_config(char* broker_config){
	config = leer_config(broker_config);
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

void iniciar_semaforos(){
	sem_init(&semHayEspacio,0,0);
	sem_init(&semNoVacio,0,0);
}

//CACHE-------------------------------------------------------------------------------------------------------------------
void iniciar_cache(){
	cant_busqueda = 0;
	mem_total = datos_config->tamanio_memoria;
	mem_asignada = 0;
	id_particion = 0;
	cant_fallida = 0;
	buf = malloc(mem_total);
	cache = list_create();
	particion* header = malloc(sizeof(particion));
	header->libre = 1;
	header->id_particion = id_particion;
	header->size = mem_total;
	header->inicio = buf;
	header->fin = header->inicio + mem_total;
	header->tiempo = time(NULL);
	list_add(cache,header);
	//sem_post(&semHayEspacio);
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
		printf("SIZE SUPERADO AL SIZE MAXIMO DE LA MEMORIA!\n");
		return 0;
	}
	return elegida;
}

void* memcpy_cache(particion* part,uint32_t id_buf,uint32_t tipo_cola,void* destino,void* buf,uint32_t size){
	if(part != NULL){
		part->id_buffer = id_buf;
		part->tipo_cola = tipo_cola;
		part->libre = 0;
		//sem_post(&semNoVacio);
	}
	return memcpy(destino,buf,size);
}

void free_cache(){
	particion* victima = algoritmo_reemplazo();
	log_warning(logger,"PARTICION %d ELIMINADA INICIO:%p",victima->id_particion,victima->inicio);

}

void compactar_cache(){
	log_warning(logger,"COMPACTANDO EL CACHE ...");
	particion* ultima = list_get(cache,list_size(cache)-1);
	bool libre_no_ultima(void* ele){
		particion* aux = ele;
		return aux->libre && aux->fin != ultima->fin;
	}

	while(1){
		particion* borrar = list_find(cache,(void*)libre_no_ultima);
		if(!borrar){
			break;
		}
		bool by_id(void* ele){
			particion* aux = ele;
			return aux->id_particion == borrar->id_particion;
		}
		list_remove_by_condition(cache,(void*)by_id);
		uint32_t size = borrar->size;
		void compac(void* ele){
			particion* aux = ele;
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
			bool posicion(void* ele){
				particion* aux = ele;
				return aux->inicio <= elegida->fin;
			}
			int pos = list_count_satisfying(cache,(void*)posicion);
			list_add_in_index(cache,pos,next);
		}
	}
	else{
		printf("NO HAY SUFICIENTE ESPACIO!\n");
		cant_fallida++;
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
	bool libre_suficiente(void* ele){
		particion* aux = ele;
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
			void aplicar(void* ele){
				particion* aux = ele;
				bool libre_suficiente(void* ele){
					particion* p = ele;
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
					bool posicion(void* ele){
						particion* aux = ele;
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
	bool libre_suficiente(void* ele){
		particion* aux = ele;
		return aux->libre == 1 && aux->size >= size;
	}
	if(string_equals_ignore_case(datos_config->algoritmo_particion_libre,"FF")){
		printf("==========FIRST FIT==========\n");
		elegida = list_find(cache,(void*)libre_suficiente);
	}
	else{
		printf("==========BEST FIT==========\n");
		bool espacio_min(void* ele1,void* ele2){
			particion* aux1 = ele1;
			particion* aux2 = ele2;
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
		bool by_id(void* ele1,void* ele2){
			particion* aux1 = ele1;
			particion* aux2 = ele2;
			return aux1->id_particion < aux2->id_particion;
		}
		bool ocupada(void* ele){
			particion* aux = ele;
			return aux->libre == 0;
		}
		aux = list_sorted(cache,(void*)by_id);
		victima = list_find(aux,(void*)ocupada);
		victima->libre = 1;
		victima->tipo_cola = -1;
		victima->id_buffer = -1;
		mem_asignada -= victima->size;
		list_destroy(aux);
	}
	else{
		printf("==========LRU==========\n");
		t_list* sorted = list_duplicate(cache);
		time_t actual;
		particion* ultima = list_get(cache,list_size(cache)-1);
		actual = time(NULL);

		bool by_time(void* ele1,void * ele2){
			particion* aux1 = ele1;
			particion* aux2 = ele1;
			return actual-aux1->tiempo > actual-aux2->tiempo;
		}

		list_sorted(sorted,(void*)by_time);

		bool no_ultima(void* ele){
			particion* aux = ele;
			return aux->id_particion!=ultima->id_particion;
		}
		victima= list_find(sorted,(void*)no_ultima);
		victima->libre = 1;
		victima->tipo_cola = -1;
		victima->id_buffer = -1;
		mem_asignada -= victima->size;
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
		log_info(logger,"DUMP DE CACHE ...");
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

//COLAS DE MENSAJE-------------------------------------------------------------------------------------------------------------------------
mq* crear_cola_mensaje(enum TIPO tipo){
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

msg* crear_mensaje(int tipo_msg,int socket,int nro_id){
	msg* mensaje = malloc(sizeof(msg));
	mensaje->tipo_msg = tipo_msg;
	mensaje->id = nro_id;
	mensaje->suscriptors_enviados = list_create();
	mensaje->suscriptors_ack = list_create();
	log_info(logger,"NUEVO MENSAJE TIPO_MENSAJE:%d ID_MENSAJE:%d",mensaje->tipo_msg,mensaje->id);
	return mensaje;
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

	int socket_cliente = accept(socket_servidor, (void*) &dir_cliente, &tam_direccion);
	pthread_create(&thread_servidor,NULL,(void*)serve_client,&socket_cliente);
	pthread_detach(thread_servidor);
}

void serve_client(int* socket){
	int cod_op;
	if(recv(*socket, &cod_op, sizeof(int), MSG_WAITALL) == -1)
		cod_op = -1;

	atender_procesos(cod_op, *socket);
}

void atender_procesos(int operacion,int cliente_fd){
	void* process = recibir_mensaje(cliente_fd);
	uint32_t pid,tipo_cola;
	memcpy(&pid,process,sizeof(uint32_t));
	memcpy(&tipo_cola,process+sizeof(uint32_t),sizeof(uint32_t));
	if(operacion == SUSCRIPCION){
		suscriber* sus = malloc(sizeof(suscriber));
		sus->pid = pid;
		sus->socket = cliente_fd;
		log_info(logger,"PROCESO %d CONECTADO CON SOCKET %d",sus->pid,sus->socket);
		suscribir_cola(tipo_cola,sus);
	}
	else{//MENSAJE
		void* buf = process+sizeof(uint32_t)*2;
		atender_mensajes(cliente_fd,pid,tipo_cola,buf);
	}
	free(process);
}

void suscribir_cola(int tipo_cola,suscriber* sus){
	log_info(logger,"PROCESO %d SUSCRIPTO A LA COLA %d SUSCRIPCION",sus->pid,tipo_cola);
	enviar_confirmacion(sus->socket);
	switch(tipo_cola){
		case GET_POKEMON:
			list_add(cola_get->suscriptors,sus);
			break;
		case LOCALIZED_POKEMON:
			list_add(cola_localized->suscriptors,sus);
			break;
		case CATCH_POKEMON:
			list_add(cola_catch->suscriptors,sus);
			break;
		case CAUGHT_POKEMON:
			list_add(cola_caught->suscriptors,sus);
			break;
		case NEW_POKEMON:
			list_add(cola_new->suscriptors,sus);
			break;
		case APPEARED_POKEMON:
			list_add(cola_appeared->suscriptors,sus);
			break;
		case 0:
			pthread_exit(NULL);
		case -1:
			pthread_exit(NULL);
	}
	//sem_wait(&semNoVacio);
	suscripcion_mensajes(sus,tipo_cola);
}

void atender_mensajes(int socket,int pid,int tipo_mensaje,void* buf){
	log_info(logger,"PROCESO %d CONECTADO CON SOCKET %d MENSAJE",pid,socket);
	switch(tipo_mensaje){
		case NEW_POKEMON:
			mensaje_new_pokemon(buf,socket);
			break;
		case CATCH_POKEMON:
			mensaje_catch_pokemon(buf,socket);
			break;
		case GET_POKEMON:
			mensaje_get_pokemon(buf,socket);
			break;
		case APPEARED_POKEMON:
			mensaje_appeared_pokemon(buf,socket);
			break;
		case CAUGHT_POKEMON:
			mensaje_caught_pokemon(buf,socket);
			break;
		case LOCALIZED_POKEMON:
			mensaje_localized_pokemon(buf,socket);
			break;
		case 0:
			pthread_exit(NULL);
		case -1:
			pthread_exit(NULL);
	}
	display_cache();
	if(mem_total-mem_asignada >= datos_config->tamanio_min_compactacion){
		sem_post(&semHayEspacio);
	}
}

void suscripcion_mensajes(suscriber* sus,int tipo){
	switch(tipo){
		case GET_POKEMON:
			suscripcion_get_pokemon(sus);
			break;
		case LOCALIZED_POKEMON:
			suscripcion_localized_pokemon(sus);
			break;
		case CATCH_POKEMON:
			suscripcion_catch_pokemon(sus);
			break;
		case CAUGHT_POKEMON:
			suscripcion_caught_pokemon(sus);
			break;
		case NEW_POKEMON:
			suscripcion_new_pokemon(sus);
			break;
		case APPEARED_POKEMON:
			suscripcion_appeared_pokemon(sus);
			break;
		case 0:
			pthread_exit(NULL);
		case -1:
			pthread_exit(NULL);
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
	}
	return 0;
}

void agregar_paquete(t_paquete* enviar,particion* aux,suscriber* sus,uint32_t tipo){
	uint32_t id = aux->id_buffer;
	int size = aux->size+sizeof(uint32_t);

	void* buffer = malloc(size);
	memcpy(buffer,&id,sizeof(uint32_t));
	memcpy(buffer+sizeof(uint32_t),aux->inicio,aux->size);

	agregar_a_paquete(enviar,buffer,size);

	bool by_id(void* ele){
		msg* aux = ele;
		return aux->id == id;
	}
	mq* cola = cola_mensaje(tipo);
	msg* mensaje = list_find(cola->mensajes,(void*)by_id);
	list_add(mensaje->suscriptors_enviados,sus);
	log_info(logger,"MENSAJE ENVIADO A PROCESO %d",sus->pid);
	free(buffer);
}

//recibir NEW_POKEMON de GAMEBOY
void mensaje_new_pokemon(void* buf,int cliente_fd){
	msg* mensaje = crear_mensaje(NEW_POKEMON,cliente_fd,cola_new->id);
	cola_new->id++;
	list_add(cola_new->mensajes,mensaje);
	enviar_id_mensaje(mensaje,cliente_fd);

	char* pokemon = buf;
	int tam = strlen(pokemon);

	particion* part_aux = malloc_cache(tam+sizeof(uint32_t)*4);
	memcpy_cache(part_aux,mensaje->id,mensaje->tipo_msg,part_aux->inicio,&tam,sizeof(uint32_t));
	memcpy_cache(part_aux,mensaje->id,mensaje->tipo_msg,part_aux->inicio+sizeof(uint32_t),buf,tam+sizeof(uint32_t)*3);

	log_warning(logger,"MENSAJE:%d NEW_POKEMON ALMACENADO EN EL CACHE INICIO:%p",mensaje->id,part_aux->inicio);
}

//para GAMECARD,cuando suscriba, le manda todos los mensajes de NEW
void suscripcion_new_pokemon(suscriber* sus){
	bool es_new(void* ele){
		particion* aux = ele;
		return aux->tipo_cola == NEW_POKEMON;
	}
	t_list* new = list_filter(cache,(void*)es_new);
	if(list_is_empty(new)){
		//sem_wait(&semNew);
	}
	t_paquete* enviar = crear_paquete(NEW_POKEMON);
	void agregar(void* ele){
		particion* aux = ele;
		agregar_paquete(enviar,aux,sus,NEW_POKEMON);
	}
	list_iterate(new,(void*)agregar);

	enviar_paquete(enviar,sus->socket);
	void agregar_cola(void* ele){
		particion* aux = ele;
		bool by_id(void* ele){
			msg* m = ele;
			return m->id == aux->id_buffer;
		}
		msg* mensaje = list_find(cola_new->mensajes,(void*)by_id);
		recibir_confirmacion_mensaje(sus);
		list_add(mensaje->suscriptors_ack,sus);
	}
	list_iterate(new,(void*)agregar_cola);
	list_destroy(new);
	eliminar_paquete(enviar);
}

//recibir GAMEBOY o GAMECARD
void mensaje_appeared_pokemon(void* buf,int cliente_fd){
	msg* mensaje = crear_mensaje(APPEARED_POKEMON,cliente_fd,cola_appeared->id);
	cola_appeared->id++;
	list_add(cola_appeared->mensajes,mensaje);
	enviar_id_mensaje(mensaje,cliente_fd);

	char* pokemon = buf;
	int tam = strlen(pokemon);
	particion* part_aux = malloc_cache(sizeof(tam)+sizeof(uint32_t)*3);
	memcpy_cache(part_aux,mensaje->id,mensaje->tipo_msg,part_aux->inicio,buf,tam+sizeof(uint32_t)*2);
	memcpy_cache(part_aux,mensaje->id,mensaje->tipo_msg,part_aux->inicio+tam,&tam,sizeof(uint32_t));
}

//para suscripcion de TEAM
void suscripcion_appeared_pokemon(suscriber* sus){
	bool es_appeared(void* ele){
		particion* aux = ele;
		return aux->tipo_cola == APPEARED_POKEMON;
	}
	t_list* appeared = list_filter(cache,(void*)es_appeared);
}

//recibir de GAMEBOY o TEAM
void mensaje_catch_pokemon(void* buf,int cliente_fd){
	msg* mensaje = crear_mensaje(CATCH_POKEMON,cliente_fd,cola_catch->id);
	cola_catch->id++;
	list_add(cola_catch->mensajes,mensaje);
	enviar_id_mensaje(mensaje,cliente_fd);

	char* pokemon = buf;
	int tam = strlen(pokemon);
	particion* part_aux = malloc_cache(sizeof(tam)+sizeof(uint32_t)*3);
	memcpy_cache(part_aux,mensaje->id,mensaje->tipo_msg,part_aux->inicio,buf,tam+sizeof(uint32_t)*2);
	memcpy_cache(part_aux,mensaje->id,mensaje->tipo_msg,part_aux->inicio+tam,&tam,sizeof(uint32_t));
}

//para sucripcion de GAMECARD
void suscripcion_catch_pokemon(suscriber* sus){
	bool es_catch(void* ele){
		particion* aux = ele;
		return aux->tipo_cola == CATCH_POKEMON;
	}
	t_list* catch = list_filter(cache,(void*)es_catch);
}

//recibir de GAMECARD o GAMEBOY
void mensaje_caught_pokemon(void* buf,int cliente_fd){
	msg* mensaje = crear_mensaje(CAUGHT_POKEMON,cliente_fd,cola_caught->id);
	cola_caught->id++;
	list_add(cola_caught->mensajes,mensaje);
	enviar_id_mensaje(mensaje,cliente_fd);

	particion* part_aux = malloc_cache(sizeof(uint32_t));
	memcpy_cache(part_aux,mensaje->id,mensaje->tipo_msg,part_aux->inicio,buf,sizeof(uint32_t));
}

//para suscripcion de TEAM
void suscripcion_caught_pokemon(suscriber* sus){
	bool es_caught(void* ele){
		particion* aux = ele;
		return aux->tipo_cola == CAUGHT_POKEMON;
	}
	t_list* caught = list_filter(cache,(void*)es_caught);
}

//recibir de GAMEBOY o TEAM
void mensaje_get_pokemon(void* buf,int cliente_fd){
	msg* mensaje = crear_mensaje(GET_POKEMON,cliente_fd,cola_get->id);
	cola_get->id++;
	list_add(cola_get->mensajes,mensaje);
	enviar_id_mensaje(mensaje,cliente_fd);

	char* pokemon = buf;
	int tam = strlen(buf);
	particion* part_aux = malloc_cache(tam+sizeof(uint32_t));
	memcpy_cache(part_aux,mensaje->id,mensaje->tipo_msg,part_aux->inicio,&tam,sizeof(uint32_t));
	memcpy_cache(part_aux,mensaje->id,mensaje->tipo_msg,part_aux->inicio+sizeof(uint32_t),buf,tam);
	log_warning(logger,"MENSAJE:%d GET_POKEMON ALMACENADO EN EL CACHE. INICIO:%p",mensaje->id,part_aux->inicio);

	int t;
	char* n;
	memcpy(&t,part_aux->inicio,sizeof(uint32_t));
	n = part_aux->inicio+sizeof(uint32_t);
	printf("+++++++++++++++++++%d %s\n",t,n);
}

//para suscripcion de GAMECARD
void suscripcion_get_pokemon(suscriber* sus){
	bool es_get(void* ele){
		particion* aux = ele;
		return aux->tipo_cola == GET_POKEMON;
	}
	t_list* get = list_filter(cache,(void*)es_get);
	if(list_is_empty(get)){
		//sem_wait(&semGet);
	}
	t_paquete* enviar = crear_paquete(GET_POKEMON);
	void agregar(void* ele){
			particion* aux = ele;
			agregar_paquete(enviar,aux,sus,GET_POKEMON);
		}
	list_iterate(get,(void*)agregar);
	enviar_paquete(enviar,sus->socket);
	void agregar_cola(void* ele){
		particion* aux = ele;
		bool by_id(void* ele){
			msg* m = ele;
			return m->id == aux->id_buffer;
		}
		msg* mensaje = list_find(cola_get->mensajes,(void*)by_id);
		recibir_confirmacion_mensaje(sus);
		list_add(mensaje->suscriptors_ack,sus);
	}
	list_iterate(get,(void*)agregar_cola);
	list_destroy(get);
	eliminar_paquete(enviar);
}

//recibir de GAMECARD
void mensaje_localized_pokemon(void* buf,int cliente_fd){

}

//para suscripcion de TEAM
void suscripcion_localized_pokemon(suscriber* sus){

}

//----------------------------------------------------------------------------------------------------------------
void* recibir_buffer(int socket_cliente, int* size){
	void * buffer;

	recv(socket_cliente, size, sizeof(int), MSG_WAITALL);
	buffer = malloc(*size);
	recv(socket_cliente, buffer, *size, MSG_WAITALL);

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

void recibir_confirmacion_mensaje(suscriber* sus){
	int res;
	if(recv(sus->socket, &res, sizeof(int), MSG_WAITALL) == -1)
		res = -1;

	if(res != -1){
		log_info(logger,"PROCESO %d HA RECIBIDO MENSAJE PREVIO",sus->pid);
	}
}

void enviar_id_mensaje(msg* mensaje,int socket_cliente){
	send(socket_cliente,&mensaje->id, sizeof(int), 0);

	printf("ID_MENSAJE ENVIADO\n");
}

void enviar_confirmacion(int socket_cliente){
	int conf = 1;
	send(socket_cliente,&conf, sizeof(int), 0);
}

t_config* leer_config(char* config){
	return config_create(config);
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

void* recibir_mensaje(int socket_cliente){
	int size;
	void * buffer;
	recv(socket_cliente,&size, sizeof(int), MSG_WAITALL);
	buffer = malloc(size);
	recv(socket_cliente, buffer,size, MSG_WAITALL);
	return buffer;
}
