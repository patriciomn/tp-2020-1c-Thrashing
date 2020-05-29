#include "team.h"

team* equipo;
t_log* logger;
t_config* config;
config_team* datos_config;
conexion* conexion_broker;
pthread_t servidor_gameboy;
pthread_t suscripcion_appeared;
pthread_t suscripcion_localized;
pthread_t suscripcion_caught;
t_list* mensajes;
sem_t semExecTeam;
sem_t semPlanificar;
sem_t semExecEntre[CANT_ENTRE];
sem_t semPoks;
sem_t semCaught;
pthread_rwlock_t lockPoksRequeridos =  PTHREAD_RWLOCK_INITIALIZER;
pthread_rwlock_t lockEntrePoks = PTHREAD_RWLOCK_INITIALIZER;
pthread_rwlock_t lockColaReady = PTHREAD_RWLOCK_INITIALIZER;
pthread_rwlock_t lockEntrenadores = PTHREAD_RWLOCK_INITIALIZER;
pthread_mutex_t mutexCPU = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

int main(int argc,char* argv[]){
	iniciar_team(argv[1]);
	ejecutar_equipo();
	return 0;
}

void iniciar_semaforos(){
	sem_init(&semExecTeam,0,1);
	sem_init(&semPoks,0,0);
	sem_init(&semPlanificar,0,0);
	for(int i=0;i<list_size(equipo->entrenadores);i++){
		sem_init(&semExecEntre[i],0,0);
	}
}

void iniciar_team(char* teamConfig){
	iniciar_config(teamConfig);
	logger = log_create(datos_config->log_file,"team",1,LOG_LEVEL_INFO);
	conexion_broker = malloc(sizeof(conexion));
	mensajes = list_create();
	crear_team();
	iniciar_semaforos();
	suscribirse_broker();
	enviar_mensajes_get_pokemon();
	pthread_create(&servidor_gameboy,NULL,(void*)iniciar_servidor,NULL);
	list_iterate(equipo->entrenadores,(void*)crear_hilo_entrenador);
}

void iniciar_config(char* teamConfig){
	config = leer_config(teamConfig);
	datos_config = malloc(sizeof(config_team));
	datos_config->log_file = config_get_string_value(config,"LOG_FILE");
	datos_config->retardo_cpu = config_get_int_value(config,"RETARDO_CICLO_CPU");
	datos_config->ip_broker = config_get_string_value(config,"IP_BROKER");
	datos_config->puerto_broker = config_get_string_value(config,"PUERTO_BROKER");
	datos_config->tiempo_reconexion = config_get_int_value(config,"TIEMPO_RECONEXION");
	datos_config->algoritmo = config_get_string_value(config,"ALGORITMO_PLANIFICACION");
	datos_config->posiciones = config_get_string_value(config,"POSICIONES_ENTRENADORES");
	datos_config->pokemones = config_get_string_value(config,"POKEMON_ENTRENADORES");
	datos_config->objetivos = config_get_string_value(config,"OBJETIVOS_ENTRENADORES");
	datos_config->alpha = config_get_double_value(config,"ALPHA");
	datos_config->estimacion_inicial = config_get_double_value(config,"ESTIMACION_INICIAL");
	datos_config->quantum = config_get_int_value(config,"QUANTUM");
}

//CREACION---------------------------------------------------------------------------------------------------------------------
void crear_team(){
	equipo = malloc(sizeof(team));
	equipo->pid = getpid();
	equipo->entrenadores = list_create();
	equipo->objetivos = list_create();
	equipo->poks_requeridos = list_create();
	equipo->cant_ciclo = 0;
	equipo->cola_ready = list_create();

	char** pos = string_get_string_as_array(datos_config->posiciones);
	char** poks = string_get_string_as_array(datos_config->pokemones);
	char** objs = string_get_string_as_array(datos_config->objetivos);
	int j = 0;
	for(int i=0;i<cant_pokemones(poks);i++){
		char** posicion = string_split(pos[i],"|");
		int posx = atoi(posicion[0]);
		int posy = atoi(posicion[1]);
		int tid = j;
		char* pok = poks[i];
		char* obj = objs[i];
		crear_entrenador(tid,posx,posy,pok,obj);
		free(posicion);
		free(poks[i]);
		free(objs[i]);
		free(pos[i]);
		j++;
	}

	free(poks);
	free(objs);
	free(pos);
}

void crear_hilo_entrenador(entrenador* entre){
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
	if(pthread_create(&entre->hilo,&attr,(void*)ejecutar_entrenador,entre) == 0){
		printf("Hilo De Entrenador%d Create\n",entre->tid);
	}
}

void crear_entrenador(int tid,int posx,int posy,char* pokemones,char* objetivos){
	entrenador* entre = malloc(sizeof(entrenador));
	entre->tid = tid;
	entre->posx = posx;
	entre->posy = posy;
	entre->estado = NEW;
	entre->pokemones = list_create();
	entre->objetivos = list_create();
	entre->espera_caught = list_create();
	entre->ciclos_totales = 0;
	entre->arrival_time = time(NULL);
	entre->service_time = 0;
	entre->start_time = 0;
	entre->finish_time = 0;
	entre->turnaround_time = 0;
	entre->estimacion_anterior = datos_config->estimacion_inicial;

	char** poks = string_split(pokemones,"|");
	for(int i=0;i<cant_pokemones(poks);i++){
		pokemon* poke = crear_pokemon(poks[i]);
		list_add(entre->pokemones,poke);
	}
	char** objs = string_split(objetivos,"|");
	for(int i=0;i<cant_pokemones(objs);i++){
		list_add(entre->objetivos,crear_pokemon(objs[i]));
		list_add(equipo->objetivos,crear_pokemon(objs[i]));
	}

	list_add(equipo->entrenadores,entre);
	printf("Entrenador%d En Posicion: [%d,%d]\n",entre->tid,entre->posx,entre->posy);
}

void salir_equipo(){
	log_info(logger,"Team%d Ha Cumplido Su Objetivo",equipo->pid);
	log_info(logger,"Cantidad Total De Ciclos De CPU Del TEAM: %d",equipo->cant_ciclo);
	void cant_ciclos(entrenador* aux){
		log_info(logger,"Cantidad De Ciclos De CPU Del Entrenador%d: %d",aux->tid,aux->ciclos_totales);
	}
	list_iterate(equipo->entrenadores,(void*)cant_ciclos);

	void free_entrenador(entrenador* aux){
		free(aux);
	}

	list_destroy_and_destroy_elements(equipo->entrenadores,(void*)free_entrenador);
	list_destroy(equipo->objetivos);
	list_destroy(equipo->poks_requeridos);
	free(equipo);
	free(datos_config->log_file);
	free(datos_config->algoritmo);
	free(datos_config->ip_broker);
	free(datos_config->objetivos);
	free(datos_config->pokemones);
	free(datos_config->puerto_broker);
	free(datos_config->posiciones);
	free(datos_config);
	log_destroy(logger);
	exit(0);
}

//EJECUCION----------------------------------------------------------------------------------------------------------------
pokemon* pokemon_a_atrapar(){
	pokemon* pok_elegido;

	bool disponible(pokemon* aux){
		return aux->espera_caught == 0;
	}
	pok_elegido = list_find(equipo->poks_requeridos,(void*)disponible);
	return pok_elegido;
}

void ejecutar_equipo(){
	while(!cumplir_objetivo_team()){
		sem_wait(&semExecTeam);
		entrenador* entre;
		if(!cumplir_objetivo_team()){
			if(!verificar_deadlock_equipo()){
				sem_wait(&semPoks);
				entre = algoritmo_corto_plazo();
			}
			else{//USANDO FIFO
				log_warning(logger,"EN DEADLOCK");
				t_list* deadlocks = list_filter(equipo->entrenadores,(void*)verificar_deadlock_entrenador);
				entre = algoritmo_fifo(deadlocks);
				despertar_entrenador(entre);
				list_destroy(deadlocks);
			}
			pthread_mutex_lock(&mutexCPU);
			activar_entrenador(entre);
			pthread_mutex_unlock(&mutexCPU);
		}
		else{
			salir_equipo();
		}
	}
}

void ejecutar_entrenador(entrenador* entre){
	inicio:	sem_wait(&semExecEntre[entre->tid]);

	while(entre->estado == EXEC){
		if(!verificar_deadlock_equipo()){
			pokemon* pok = pokemon_a_atrapar();
			move_entrenador(entre,pok->posx,pok->posy);
			int caso_default = catch_pokemon(entre,pok);
			if(caso_default != 1){
				log_warning(logger,"Entrenador%d BLOCKED En Espera De Caught!",entre->tid);
				list_add(entre->espera_caught,pok);
				pok->espera_caught = 1;
				bloquear_entrenador(entre);
				goto inicio;
			}
		}
		else{
			bool entrenador_espera_deadlock(entrenador* aux){
				return aux->estado == BLOCKED && verificar_deadlock_entrenador(aux);
			}
			t_list* entrenadores = equipo->entrenadores;
			entrenador* entre2 = list_find(entrenadores,(void*)entrenador_espera_deadlock);
			move_entrenador(entre,entre2->posx,entre2->posy);
			intercambiar_pokemon(entre,entre2);
			if(cumplir_objetivo_entrenador(entre2) && cumplir_objetivo_entrenador(entre)){
				log_info(logger,"Deadlock Ha Solucionado");
				salir_entrenador(entre2);
			}
			else if(cumplir_objetivo_entrenador(entre2)){
				log_info(logger,"Deadlock Del Entrenador%d Ha Solucionado",entre2->tid);
				salir_entrenador(entre2);
			}
			else if(cumplir_objetivo_entrenador(entre)){
				log_info(logger,"Deadlock Del Entrenador%d Ha Solucionado",entre->tid);
			}
			else{
				log_info(logger,"Deadlock No Ha Solucionado");
			}
		}

		if(!cumplir_objetivo_entrenador(entre) && !verificar_cantidad_pokemones(entre)){
			log_warning(logger,"Entrenador%d BLOCKED En Espera De Solucionar Deadlock!",entre->tid);
			bloquear_entrenador(entre);
			goto inicio;
		}
		else if(!cumplir_objetivo_entrenador(entre)){
			log_warning(logger,"Entrenador%d BLOCKED Finaliza Su Recorrido!",entre->tid);
			bloquear_entrenador(entre);
			goto inicio;
		}
		else{
			salir_entrenador(entre);
			sem_post(&semExecTeam);
		}
	}
}

void despertar_entrenador(entrenador* entre){
	if(entre->estado == NEW || entre->estado == BLOCKED || entre->estado == EXEC){
		log_info(logger,"Entrenador%d READY",entre->tid);
		entre->estado = READY;
		list_add(equipo->cola_ready,entre);
	}
}

void activar_entrenador(entrenador* entre){
	if(entre->estado == READY){
		bool by_tid(entrenador* aux){
			return aux->tid == entre->tid;
		}
		pthread_rwlock_wrlock(&lockColaReady);
		list_remove_by_condition(equipo->cola_ready,(void*)by_tid);
		pthread_rwlock_unlock(&lockColaReady);
		printf("Entrenador%d Sale De Cola Ready\n",entre->tid);
		printf("Entrenador%d EXEC\n",entre->tid);
		entre->estado = EXEC;
		entre->start_time = time(NULL);
		sem_post(&semExecEntre[entre->tid]);
	}
}

void bloquear_entrenador(entrenador* entre){
	if(entre->estado == EXEC){
		printf("Entrenador%d BLOCKED\n",entre->tid);
		entre->estado = BLOCKED;
		sem_post(&semExecTeam);
	}
}

void salir_entrenador(entrenador* entre){
	if(entre->estado == EXEC || entre->estado == BLOCKED){
		printf("Entrenador%d Ha Cumplido Su Objetivo\n",entre->tid);
		printf("Entrenador%d EXIT\n",entre->tid);
		entre->estado = EXIT;
		entre->finish_time = time(NULL);
		entre->turnaround_time = entre->finish_time - entre->arrival_time;
	}
}

void move_entrenador(entrenador* entre,int posx,int posy){
	log_info(logger,"Entrenador%d Moviendose A Posicion: [%d,%d]",entre->tid,posx,posy);
	int ciclos = abs(entre->posx-posx)+abs(entre->posy-posy) * CICLOS_MOVER;
	for(int i=0;i<ciclos;i++){
		printf("Entrenador%d moviendose ...\n",entre->tid);
		entre->service_time++;
		sumar_ciclos(entre,1);
		sleep(datos_config->retardo_cpu);
	}
	entre->posx = posx;
	entre->posy = posy;
}

bool catch_pokemon(entrenador* entre,pokemon* pok){
	bool caso_default = -1;
	log_info(logger,"Entrenador%d Atrapando %s",entre->tid,pok->name);
	sleep(datos_config->retardo_cpu);

	conexion_broker->catch = crear_conexion(datos_config->ip_broker,datos_config->puerto_broker);
	if(conexion_broker->catch != -1){
		for(int i=0;i<CICLOS_ENVIAR;i++){
			printf("TEAM %d enviando mensaje a BROKER ...\n",equipo->pid);
			sleep(datos_config->retardo_cpu);
			entre->service_time++;
			sumar_ciclos(entre,1);
		}
		enviar_mensaje_catch_pokemon(pok->name,pok->posx,pok->posy,conexion_broker->catch);
		int id = recibir_id_mensaje(conexion_broker->catch);
		msg* mensaje = crear_mensaje(id,CATCH_POKEMON,pok);
		list_add(mensajes,mensaje);
		log_info(logger,"Mensaje CATCH_POKEMON ID_Mensaje: %d",id);
	}
	else{
		caso_default = 1;
		log_error(logger,"DEFAULT: Entrenador%d Ha Atrapado Pokemon %s",entre->tid,pok->name);
		pthread_rwlock_wrlock(&lockEntrePoks);
		list_add(entre->pokemones,pok);
		pthread_rwlock_unlock(&lockEntrePoks);
		remove_pokemon_requeridos(pok);
	}
	return caso_default;
}

pokemon* pokemon_a_intercambiar(entrenador* entre){
	t_list* no_necesarios;
	bool no_necesitar(pokemon* aux){
		return !necesitar_pokemon(entre,aux->name);
	}
	no_necesarios = list_filter(entre->pokemones,(void*)no_necesitar);
	pokemon* pok = list_get(no_necesarios,0);
	list_destroy(no_necesarios);
	return pok;
}

void intercambiar_pokemon(entrenador* entre1,entrenador* entre2){
	log_info(logger,"Entrenador%d y Entrenador%d Intercambian Pokemones",entre1->tid,entre2->tid);
	for(int i=0;i<CICLOS_INTERCAMBIAR;i++){
		printf("Entrenador%d y Entrenador%d intercambiando pokemones ...\n",entre1->tid,entre2->tid);
		sleep(datos_config->retardo_cpu);
		entre1->service_time++;
		sumar_ciclos(entre1,1);
	}
	pokemon* pok1 = pokemon_a_intercambiar(entre1);
	pokemon* pok2 = pokemon_a_intercambiar(entre2);
	bool by_name1(pokemon* aux){
		return aux->name = pok1->name;
	}
	bool by_name2(pokemon* aux){
		return aux->name = pok2->name;
	}
	list_add(entre1->pokemones,pok2);
	list_add(entre2->pokemones,pok1);
	list_remove_by_condition(entre1->pokemones,(void*)by_name1);
	list_remove_by_condition(entre2->pokemones,(void*)by_name2);
}

//PLANIFICACION----------------------------------------------------------------------------------------------------------------------
void algoritmo_largo_plazo(pokemon* pok){
	bool by_estado(entrenador* aux){
		return (aux->estado == NEW || aux->estado == BLOCKED) && (!verificar_deadlock_entrenador(aux)
				&& list_is_empty(aux->espera_caught));
	}
	bool by_distancia(entrenador* aux,entrenador* next){
		return (distancia_pokemon(aux,pok)) < (distancia_pokemon(next,pok));
	}
	pthread_rwlock_wrlock(&lockEntrenadores);
	t_list* entrenadores = list_filter(equipo->entrenadores,(void*)by_estado);
	pthread_rwlock_unlock(&lockEntrenadores);
	if(!list_is_empty(entrenadores)){
		list_sort(entrenadores,(void*)by_distancia);
		entrenador* elegido = list_find(entrenadores,(void*)by_estado);
		float distancia = distancia_pokemon(elegido,pok);
		void pasar_ready(entrenador* aux){
			if(distancia_pokemon(aux,pok) == distancia){
				despertar_entrenador(aux);
			}
		}
		list_iterate(entrenadores,(void*)pasar_ready);
		list_destroy(entrenadores);
	}
}

entrenador* algoritmo_corto_plazo(){
	entrenador* entre;
	if(string_equals_ignore_case(datos_config->algoritmo,"FIFO")){
		entre = algoritmo_fifo(equipo->cola_ready);
	}
	else if(string_equals_ignore_case(datos_config->algoritmo,"SJF-SD")){
		entre = algoritmo_sjf_sin_desalojo(equipo->cola_ready);
	}
	else if(string_equals_ignore_case(datos_config->algoritmo,"SJF-CD")){
		entre = algoritmo_sjf_con_desalojo(equipo->cola_ready);
	}
	else{
		entre = algoritmo_round_robin(equipo->cola_ready);
	}
	return entre;
}

entrenador* algoritmo_fifo(t_list* cola_ready){
	printf("==========FIFO==========\n");
	entrenador* entre;
	bool fifo(entrenador* aux1,entrenador* aux2){
		return aux1->arrival_time < aux2->arrival_time;
	}
	t_list* cola_fifo = list_sorted(cola_ready,(void*)fifo);
	entre = list_get(cola_fifo,0);
	list_destroy(cola_fifo);
	return entre;
}

double estimacion(entrenador* entre){
	double service_time = entre->service_time;
	double alpha = datos_config->alpha;
	double comp1 = service_time * alpha;
	double comp2 = (1 - alpha) * entre->estimacion_anterior;
	double res = comp1 + comp2;
	return res;
}

entrenador* algoritmo_sjf_sin_desalojo(t_list* cola_ready){
	printf("==========SJF SIN DESALOJO==========\n");
	entrenador* entre;
	bool sjf_sin_desalojo(entrenador* aux1,entrenador* aux2){
		if(estimacion(aux1) == estimacion(aux2)){
			return aux1->arrival_time < aux2->arrival_time;
		}
		return estimacion(aux1) < estimacion(aux2);
	}
	t_list* cola_sjf = list_sorted(cola_ready,(void*)sjf_sin_desalojo);
	entre = list_get(cola_sjf,0);
	entre->estimacion_anterior = estimacion(entre);
	list_destroy(cola_sjf);
	return entre;
}

entrenador* algoritmo_sjf_con_desalojo(t_list* cola_ready){//implementar
	printf("==========SJF CON DESALOJO==========\n");
	entrenador* entre;

	return entre;
}

entrenador* algoritmo_round_robin(t_list* cola_ready){//IMPLEMENTAR
	printf("==========ROUND ROBIN==========\n");
	entrenador* entre = list_get(equipo->cola_ready,0);

	struct sigaction action;
	action.sa_handler = (void*)fin_de_quantum;
	action.sa_flags = 0;
	sigemptyset(&action.sa_mask);
	sigaction(SIGALRM, &action,NULL);
	alarm(datos_config->quantum);
	return entre;
}

void fin_de_quantum(){
	printf("Fin De Quantum\n");
	alarm(datos_config->quantum);
}

//DEADLOCK------------------------------------------------------------------------------------------------------------
bool verificar_deadlock_entrenador(entrenador* entre){
	return (verificar_cantidad_pokemones(entre) != 1) && (!cumplir_objetivo_entrenador(entre));
}

bool verificar_deadlock_equipo(){
	return list_all_satisfy(equipo->entrenadores,(void*)verificar_deadlock_entrenador)
			&& list_all_satisfy(equipo->entrenadores,(void*)verificar_pokemon_exceso_no_necesario)
			&& list_all_satisfy(equipo->entrenadores,(void*)verificar_espera_circular);
}

bool verificar_pokemon_exceso_no_necesario(entrenador* entre){
	bool exceso(pokemon* aux){
		return cant_especie_pokemon(entre,aux->name) > cant_especie_objetivo(entre,aux->name);
	}
	bool noNecesario(pokemon* aux){
		return !necesitar_pokemon(entre,aux->name);
	}
	return list_any_satisfy(entre->pokemones,(void*)exceso) || list_any_satisfy(entre->pokemones,(void*)noNecesario);
}

pokemon* pokemon_no_necesario(entrenador* entre){
	bool no_necesario(pokemon* aux){
		return !necesitar_pokemon(entre,aux->name) || pokemon_exceso(entre,aux->name);
	}
	return list_find(entre->pokemones,(void*)no_necesario);
}

bool verificar_espera_circular(entrenador* entre1){
	bool en_espera(entrenador* aux2){
		pokemon* no1 = pokemon_no_necesario(entre1);
		pokemon* no2 = pokemon_no_necesario(aux2);
		return necesitar_pokemon(entre1,no2->name) && necesitar_pokemon(aux2,no1->name);
	}
	return list_any_satisfy(equipo->entrenadores,(void*)en_espera);
}

//AUXILIARES-----------------------------------------------------------------------------------------------------------------------------
float distancia_pokemon(entrenador* entrenador,pokemon* pok){
	float x = abs(entrenador->posx-pok->posx);
	float y = abs(entrenador->posy-pok->posy);
	float dis = sqrt((x*x)+(y*y));
	return dis;
}

void sumar_ciclos(entrenador* entre,int ciclos){
	entre->ciclos_totales += ciclos;
	equipo->cant_ciclo += ciclos;
}

bool cumplir_objetivo_team(){
	return list_all_satisfy(equipo->entrenadores,(void*)cumplir_objetivo_entrenador);
}

bool cumplir_objetivo_entrenador(entrenador* entre){
	bool by_especie(pokemon* aux){
		return contener_pokemon(entre,aux->name);
	}
	bool by_cantidad(pokemon* aux){
			return cant_especie_objetivo(entre,aux->name) == cant_especie_pokemon(entre,aux->name);
	}
	return list_all_satisfy(entre->objetivos,(void*)by_especie) && list_all_satisfy(entre->objetivos,(void*)by_cantidad) ;
}

bool contener_pokemon(entrenador* entre,char* name){
	bool res = 0;
	bool by_name(pokemon* aux){
		return (strcmp(aux->name,name) == 0);
	}
	pthread_rwlock_rdlock(&lockEntrePoks);
	res = list_any_satisfy(entre->pokemones,(void*)by_name);
	pthread_rwlock_unlock(&lockEntrePoks);
	return res;
}

bool pokemon_exceso(entrenador* entre,char* name){
	int cant = cant_especie_pokemon(entre,name);
	return cant > 0 && cant > cant_especie_objetivo(entre,name);
}

bool necesitar_pokemon(entrenador* entre,char* name){
	return !contener_pokemon(entre,name) || (cant_especie_pokemon(entre,name) < cant_especie_objetivo(entre,name));
}

int cant_especie_objetivo_team(char* name){
	int res = 0;
	bool by_name(pokemon* aux){
		return (strcmp(aux->name,name) == 0);
	}
	res = list_count_satisfying(equipo->objetivos,(void*)by_name);
	return res;
}

int cant_especie_objetivo(entrenador* entre,char* name){
	int res = 0;
	bool by_name(pokemon* aux){
		return (strcmp(aux->name,name) == 0);
	}
	res = list_count_satisfying(entre->objetivos,(void*)by_name);
	return res;
}

int cant_especie_pokemon(entrenador* entre,char* name){
	int res = 0;
	bool by_name(pokemon* aux){
		return (strcmp(aux->name,name) == 0);
	}
	pthread_rwlock_rdlock(&lockEntrePoks);
	res = list_count_satisfying(entre->pokemones,(void*)by_name);
	pthread_rwlock_unlock(&lockEntrePoks);
	return res;
}

bool verificar_cantidad_pokemones(entrenador* entre){
	int permite = 0;
	if(list_size(entre->pokemones) < list_size(entre->objetivos)){
		permite = 1;
	}
	return permite;
}

t_list* especies_objetivo_team(){
	bool repetido(pokemon* aux){
		return cant_especie_objetivo_team(aux->name) > 1;
	}
	t_list* objetivos = list_duplicate(equipo->objetivos);
	t_list* especies = list_create();
	for(int i=0;i<list_size(objetivos);i++){
		pokemon* obj = list_get(objetivos,i);
		bool by_name(pokemon* aux){
			return strcmp(aux->name,obj->name) == 0;
		}
		if(!list_any_satisfy(especies,(void*)by_name)){
			list_add(especies,obj);
		}
	}
	list_destroy(objetivos);
	return especies;
}

bool requerir_atrapar(char* pok){
	int requiere = 0;
	int cant_team = cant_especie_objetivo_team(pok);
	int cant_especie = 0;
	int contador = 0;
	entrenador* entre = list_get(equipo->entrenadores,contador);
	while(entre){
		cant_especie += cant_especie_pokemon(entre,pok);
		contador++;
		entre = list_get(equipo->entrenadores,contador);
	}
	if(cant_team != 0 && cant_especie < cant_team){
		requiere = 1;
	}
	return requiere;
}

//POKEMON-------------------------------------------------------------------------------------------------------------------------
int cant_pokemones(char** poks){
	int cant = 0;
	char* pok = poks[0];
	while(pok){
		cant++;
		pok = poks[cant];
	}
	return cant;
}

void remove_pokemon_requeridos(pokemon* pok){
	bool by_name_pos(pokemon* aux){
		return ((strcmp(aux->name,pok->name)==0) && (aux->posx==pok->posx) && (aux->posy==pok->posy));
	}
	pthread_rwlock_wrlock(&lockPoksRequeridos);
	list_remove_by_condition(equipo->poks_requeridos,(void*)by_name_pos);
	pthread_rwlock_unlock(&lockPoksRequeridos);
	printf("POKEMON %s Eliminado\n",pok->name);
}

pokemon* crear_pokemon(char* name){
	pokemon* pok = malloc(sizeof(pokemon));
	pok->name = name;
	pok->espera_caught = 0;
	return pok;
}

void set_pokemon(pokemon* pok,int posx,int posy){
	pok->posx = posx;
	pok->posy = posy;
}

pokemon* find_pokemon(char* name){
	bool by_name(pokemon* aux){
		return (strcmp(aux->name,name)==0);
	}
		return list_find(equipo->poks_requeridos,(void*)by_name);
}

//MENSAJES------------------------------------------------------------------------------------------------------------------------
void enviar_mensajes_get_pokemon(){
	void get(void* element){
		pokemon* pok = element;
		get_pokemon(pok->name);
	}
	t_list* especies = especies_objetivo_team();
	list_iterate(especies,(void*)get);
	list_destroy(especies);
}

void get_pokemon(char* pokemon){
	conexion_broker->get = crear_conexion(datos_config->ip_broker,datos_config->puerto_broker);
	if(conexion_broker->get == -1){
		log_error(logger,"DEFAULT: %s No Existe",pokemon);
	}
	else{
		enviar_mensaje_get_pokemon(pokemon,conexion_broker->get);
		int id = recibir_id_mensaje(conexion_broker->get);
		msg* mensaje = crear_mensaje(id,GET_POKEMON,NULL);
		list_add(mensajes,mensaje);
		printf("Tipo_Mensaje: %s ID_Mensaje: %d\n","GET_POKEMON",id);
		liberar_conexion(conexion_broker->get);
	}
}

void appeared_pokemon(int cliente_fd){//gameboy
	int size;
	int desplazamiento = 0;
	int posx,posy,cant;
	char* pokemon_name = recibir_buffer(cliente_fd, &size);
	desplazamiento += strlen(pokemon_name)+1;
	memcpy(&posx,pokemon_name+desplazamiento,sizeof(int));
	desplazamiento += sizeof(int);
	memcpy(&posy,pokemon_name+desplazamiento,sizeof(int));
	log_info(logger,"Llega Un Mensaje Tipo: APPEARED_POKEMON: %s  Pos X:%d  Pos Y:%d\n",pokemon_name,posx,posy);

	if(requerir_atrapar(pokemon_name)){
		pokemon* pok = crear_pokemon(pokemon_name);
		set_pokemon(pok,posx,posy);
		pthread_rwlock_wrlock(&lockPoksRequeridos);
		list_add(equipo->poks_requeridos,pok);
		pthread_rwlock_unlock(&lockPoksRequeridos);
		printf("%s Es Requerido, Agregado A La Lista De Pokemones Requeridos\n",pok->name);
		algoritmo_largo_plazo(pok);
		sem_post(&semPoks);
	}
	else{
		printf("%s No Es Requerido\n",pokemon_name);
	}
}

void recibir_localized_pokemon(int cliente_fd){//IMPLEMENTAR
	int cod_op;
	if(recv(cliente_fd, &cod_op, sizeof(int), MSG_WAITALL) == -1)
		cod_op = -1;

	int size;
	int desplazamiento = 0;
	int posx,posy,cant;
	char* pokemon = recibir_buffer(cliente_fd, &size);
	desplazamiento += strlen(pokemon)+1;
	memcpy(&posx,pokemon+desplazamiento,sizeof(int));
	desplazamiento += sizeof(int);
	memcpy(&posy,pokemon+desplazamiento,sizeof(int));

	printf("LOCALIZED_POKEMON: %s  POS X:%d  POS Y:%d\n",pokemon,posx,posy);
	free(pokemon);
}

void recibir_caught_pokemon(){
	int cod_op;
	while(1){
		if(recv(conexion_broker->caught, &cod_op, sizeof(int), MSG_WAITALL) == -1)
			cod_op = -1;

		if(cod_op != -1){
			t_list* paquete = recibir_paquete(conexion_broker->caught);
			void display(void* ele){
				//enviar_confirmacion(CAUGHT_POKEMON,conexion_broker->caught);
				void* valor = ele;
				int id,resultado;
				memcpy(&id,valor,sizeof(int));
				memcpy(&resultado,valor+sizeof(int),sizeof(int));
				log_info(logger,"Llega Un Mensaje Tipo: CAUGHT_POKEMON ID:%d Resultado:%d \n",id,resultado);

				bool by_tipo_id(msg* aux){
					return aux->tipo_msg==CATCH_POKEMON && aux->id_recibido==id;
				}
				msg* mensaje = list_find(mensajes,(void*)by_tipo_id);
				if(mensaje != NULL && resultado==1){
					bool espera_caught(entrenador* aux){
						bool by_pokemon(pokemon* pok){
							return pok->espera_caught == 1 && pok->name == mensaje->pok->name;
						}
						return list_any_satisfy(aux->espera_caught,(void*)by_pokemon);
					}
					entrenador* entre = list_find(equipo->entrenadores,(void*)espera_caught);
					list_add(entre->pokemones,mensaje->pok);
					printf("Entrenador%d Ha Atrapado Pokemon %s\n",entre->tid,mensaje->pok->name);
					bool es_caught(pokemon* aux){
						return aux->name == mensaje->pok->name;
					}
					list_remove_by_condition(entre->espera_caught,(void*)es_caught);
					list_remove_by_condition(mensajes,(void*)by_tipo_id);
					remove_pokemon_requeridos(mensaje->pok);
					sem_post(&semExecTeam);
				}
				free(valor);
			}
			list_iterate(paquete,(void*)display);
			list_destroy(paquete);
		}
	}
}

void recibir_appeared_pokemon(){
	int cod_op;
	while(1){
		if(recv(conexion_broker->appeared, &cod_op, sizeof(int), MSG_WAITALL) == -1)
			cod_op = -1;
		if(cod_op != -1){
			t_list* paquete = recibir_paquete(conexion_broker->appeared);
			void display(void* ele){
				//enviar_ack(APPEARED_POKEMON,conexion_broker->appeared);
				void* valor = ele;
				int id,tam,posx,posy;
				memcpy(&id,valor,sizeof(int));
				memcpy(&tam,valor+sizeof(int),sizeof(int));
				char* n = (char*)valor+sizeof(int)*2;
				int len = tam+1;
				char* name = malloc(len);
				strcpy(name,n);
				memcpy(&posx,valor+sizeof(int)*2+len,sizeof(int));
				memcpy(&posy,valor+sizeof(int)*3+len,sizeof(int));

				log_info(logger,"Llega Un Mensaje Tipo: APPEARED_POKEMON: ID: %d Pokemon: %s  Pos X:%d  Pos Y:%d\n",id,name,posx,posy);
				if(requerir_atrapar(name)){
					pokemon* pok = crear_pokemon(name);
					set_pokemon(pok,posx,posy);
					pthread_rwlock_wrlock(&lockPoksRequeridos);
					list_add(equipo->poks_requeridos,pok);
					printf("%s Es Requerido, Agregado A La Lista De Pokemones Requeridos\n",pok->name);
					pthread_rwlock_unlock(&lockPoksRequeridos);
					sem_post(&semPoks);
				}
				else{
					printf("%s No Es Requerido\n",name);
					free(name);
				}
				free(valor);
			}
			list_iterate(paquete,(void*)display);
			list_destroy(paquete);
		}
	}
}

void enviar_ack(int tipo,int id,int id_correlativo,int socket_cliente){
	t_paquete* paquete = malloc(sizeof(t_paquete));
	paquete->codigo_operacion = ACK;
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = sizeof(int)*4;
	paquete->buffer->stream = malloc(paquete->buffer->size);
	memcpy(paquete->buffer->stream,&equipo->pid,sizeof(int));
	memcpy(paquete->buffer->stream+sizeof(int), &tipo, sizeof(int));
	memcpy(paquete->buffer->stream+sizeof(int)*2, &id, sizeof(int));
	memcpy(paquete->buffer->stream+sizeof(int)*3, &id_correlativo, sizeof(int));
	int bytes = paquete->buffer->size + 2*sizeof(int);

	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, 0);

	free(a_enviar);
	free(paquete->buffer->stream);
	free(paquete->buffer);
	free(paquete);
}


void enviar_info_suscripcion(int tipo,int socket_cliente){
	t_paquete* paquete = malloc(sizeof(t_paquete));
	paquete->codigo_operacion = SUSCRITO;
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = sizeof(int)*2;
	paquete->buffer->stream = malloc(paquete->buffer->size);
	memcpy(paquete->buffer->stream,&equipo->pid,sizeof(int));
	memcpy(paquete->buffer->stream+sizeof(int), &tipo, sizeof(int));
	int bytes = paquete->buffer->size + 2*sizeof(int);

	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, 0);

	free(a_enviar);
	free(paquete->buffer->stream);
	free(paquete->buffer);
	free(paquete);
}

void enviar_mensaje_get_pokemon(char* pokemon,int socket_cliente){
	equipo->cant_ciclo += 1;
	int tam = strlen(pokemon) + 1;
	int name_size = strlen(pokemon);
	t_paquete* paquete = malloc(sizeof(t_paquete));
	paquete->codigo_operacion = GET_POKEMON;
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = sizeof(int)*2 + tam ;
	paquete->buffer->stream = malloc(paquete->buffer->size);
	memcpy(paquete->buffer->stream,&equipo->pid,sizeof(int));
	memcpy(paquete->buffer->stream+sizeof(int),pokemon,tam);
	memcpy(paquete->buffer->stream+sizeof(int)+tam,&name_size,sizeof(int));
	int bytes = paquete->buffer->size + sizeof(int)*2;

	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, 0);

	printf("GET_POKEMON: %s\n",pokemon);

	free(a_enviar);
	free(paquete->buffer->stream);
	free(paquete->buffer);
	free(paquete);
}

void enviar_mensaje_catch_pokemon(char* pokemon,int posx,int posy,int socket_cliente){
	equipo->cant_ciclo += 1;
	int tam = strlen(pokemon) + 1;
	int name_size = strlen(pokemon);

	t_paquete* paquete = malloc(sizeof(t_paquete));
	paquete->codigo_operacion = CATCH_POKEMON;
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = sizeof(int)*4 + tam ;
	paquete->buffer->stream = malloc(paquete->buffer->size);
	memcpy(paquete->buffer->stream,&equipo->pid,sizeof(int));
	memcpy(paquete->buffer->stream+sizeof(int), pokemon, tam);
	memcpy(paquete->buffer->stream+sizeof(int)+tam,&name_size,sizeof(int));
	memcpy(paquete->buffer->stream+sizeof(int)*2+tam, &posx,sizeof(int));
	memcpy(paquete->buffer->stream+sizeof(int)*3+tam, &posy,sizeof(int));

	int bytes = paquete->buffer->size + 2*sizeof(int);

	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, 0);

	log_info(logger,"CATCH_POKEMON: %s",pokemon);

	free(a_enviar);
	free(paquete->buffer->stream);
	free(paquete->buffer);
	free(paquete);
}

void recibir_confirmacion_suscripcion(int cliente_fd){
	int cod_op;
	if(recv(cliente_fd, &cod_op, sizeof(int), MSG_WAITALL) == -1)
		cod_op = -1;

	printf("Se Ha Suscripto A La Cola\n");
}
//BROKER---GAMEBOY--------------------------------------------------------------------------------------------------------------------------
void suscribirse_broker(){
	suscribirse_appeared();
	suscribirse_localized();
	suscribirse_caught();
	if(conexion_broker->appeared == -1 && conexion_broker->localized == -1 && conexion_broker->caught == -1){
		log_error(logger,"No Se Puede Conectarse A Broker");
		reintento_conectar_broker();
	}
	else{
		pthread_create(&suscripcion_caught,NULL,(void*)recibir_caught_pokemon,NULL);
		pthread_create(&suscripcion_appeared,NULL,(void*)recibir_appeared_pokemon,NULL);
		//pthread_create(&suscripcion_localized,NULL,(void*)recibir_localized_pokemon,NULL);
	}
}

void suscribirse_appeared(){
	conexion_broker->appeared = crear_conexion(datos_config->ip_broker,datos_config->puerto_broker);
	if(conexion_broker->appeared != -1){
		//pthread_cancel(servidor_gameboy);
		log_warning(logger,"Conectado A Broker Suscribirse a APPEARED");
		enviar_info_suscripcion(APPEARED_POKEMON,conexion_broker->appeared);
		recibir_confirmacion_suscripcion(conexion_broker->appeared);
	}
}

void suscribirse_localized(){
	conexion_broker->localized = crear_conexion(datos_config->ip_broker,datos_config->puerto_broker);
	if(conexion_broker->localized != -1){
		//pthread_cancel(servidor_gameboy);
		log_warning(logger,"Conectado A Broker Suscribirse a LOCALIZED");
		enviar_info_suscripcion(LOCALIZED_POKEMON,conexion_broker->localized);
		recibir_confirmacion_suscripcion(conexion_broker->localized);
	}
}

void suscribirse_caught(){
	conexion_broker->caught = crear_conexion(datos_config->ip_broker,datos_config->puerto_broker);
	if(conexion_broker->localized != -1){
		//pthread_cancel(servidor_gameboy);
		log_warning(logger,"Conectado A Broker Suscribirse a CAUGHT");
		enviar_info_suscripcion(CAUGHT_POKEMON,conexion_broker->caught);
		recibir_confirmacion_suscripcion(conexion_broker->caught);
	}
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

void reintento_conectar_broker(){
	struct sigaction action;
	action.sa_handler = (void*)end_of_quantum_handler;
	action.sa_flags = SA_RESTART|SA_NODEFER;
	sigemptyset(&action.sa_mask);
	sigaction(SIGALRM, &action, 0);
	alarm(datos_config->tiempo_reconexion);
}

void end_of_quantum_handler(){
	if(conexion_broker->appeared == -1 || conexion_broker->localized == -1|| conexion_broker->caught == -1){
		log_error(logger,"Reconectando A Broker ...");
		suscribirse_broker(equipo->pid);
		alarm(datos_config->tiempo_reconexion);
	}
}

void iniciar_servidor(void){
	int socket_servidor;

    struct addrinfo hints, *servinfo, *p;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    getaddrinfo(IP, PUERTO, &hints, &servinfo);

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
	log_info(logger,"GAMEBOY Conectado!",socket_cliente);

	int cod_op;
	if(recv(socket_cliente, &cod_op, sizeof(int), MSG_WAITALL) == -1)
		cod_op = -1;
	appeared_pokemon(socket_cliente);
}

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


int recibir_id_mensaje(int cliente_fd){
	int id;
	void* buffer = malloc(sizeof(int));
	recv(cliente_fd,buffer,sizeof(buffer),MSG_WAITALL);
	memcpy(&id,buffer,sizeof(int));
	free(buffer);
	return id;
}

void liberar_conexion(int socket_cliente){
	close(socket_cliente);
}
msg* crear_mensaje(int id,int tipo,pokemon* pok){
	msg* mensaje = malloc(sizeof(msg));
	mensaje->id_recibido = id;
	mensaje->tipo_msg = tipo;
	mensaje->pok = pok;
	return mensaje;
}

t_list* recibir_paquete(int socket_cliente){
	uint32_t size;
	uint32_t desplazamiento = 0;
	void * buffer;
	t_list* valores = list_create();
	uint32_t tamanio;
	buffer = recibir_buffer(socket_cliente,&size);
	while(desplazamiento < size){
		memcpy(&tamanio, buffer + desplazamiento, sizeof(uint32_t));
		desplazamiento+=sizeof(uint32_t);
		void* valor = malloc(tamanio);
		memcpy(valor, buffer+desplazamiento, tamanio);
		desplazamiento+=tamanio;
		list_add(valores, valor);
	}
	free(buffer);
	return valores;
}

t_config* leer_config(char* config){
	return config_create(config);
}
