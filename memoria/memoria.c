#include "memoria.h"

t_log* logger;
t_config* config;
config_broker* datos_config;

int cant_busqueda;
uint32_t cache_size;
uint32_t min_size;
uint32_t mem_asignada;
uint32_t mem_total;
uint32_t id_particion;
t_list* cache;//una lista de particiones
void* memoria;
uint32_t cant_fallida;

sem_t semHayEspacio;
sem_t semNoVacio;
pthread_rwlock_t lockSus = PTHREAD_RWLOCK_INITIALIZER;

int main(void){
	iniciar_memoria();

	//EJEMPLO:un mensaje de GET_POKEMON
	char* name = "Pikachu";
	int name_size = strlen(name);
	int tam = sizeof(int) + name_size;

	//malloc espacio respecto al algoritmo
	particion* part = malloc_cache(tam);

	//memcpy los datos a la particion reservada
	memcpy_cache(part,0,5,part->inicio,&tam,sizeof(int));
	memcpy_cache(part,0,5,part->inicio+sizeof(int),name,name_size);
	display_cache();

	//liberar el cache respecto al algoritmo
	free_cache();

	//compactacion
	compactar_cache();
	display_cache();

	limpiar_cache();

	return EXIT_SUCCESS;
}

void sig_handler(int signo){
	handler_dump(SIGUSR1);
}

void iniciar_memoria(){
	logger = log_create("memoria.log","memoria",1,LOG_LEVEL_INFO);
	iniciar_config("memoria.config");
	iniciar_semaforos();
	iniciar_cache();
	signal(SIGINT,sig_handler);

}

void iniciar_config(char* memoria_config){
	config = leer_config(memoria_config);
	datos_config = malloc(sizeof(config_broker));
	datos_config->tamanio_memoria = atoi(config_get_string_value(config,"TAMANO_MEMORIA"));
	datos_config->tamanio_min_compactacion = atoi(config_get_string_value(config,"TAMANO_MINIMO_PARTICION"));
	datos_config->algoritmo_memoria = config_get_string_value(config,"ALGORITMO_MEMORIA");
	datos_config->algoritmo_particion_libre = config_get_string_value(config,"ALGORITMO_PARTICION_LIBRE");
	datos_config->algoritmo_reemplazo = config_get_string_value(config,"ALGORITMO_REEMPLAZO");
	datos_config->frecuencia_compactacion = atoi(config_get_string_value(config,"FRECUENCIA_COMPACTACION"));
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
	cant_fallida = 0;
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
	sem_wait(&semHayEspacio);
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

t_config* leer_config(char* config){
	return config_create(config);
}
