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
pthread_t reintento;
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
	datos_config->quantum = (config_get_int_value(config,"QUANTUM") * datos_config->retardo_cpu);
	datos_config->id = config_get_int_value(config,"ID");
}

//CREACION---------------------------------------------------------------------------------------------------------------------
void crear_team(){
	equipo = malloc(sizeof(team));
	equipo->pid = datos_config->id;
	equipo->entrenadores = list_create();
	equipo->objetivos = list_create();
	equipo->poks_requeridos = list_create();
	equipo->cant_ciclo = 0;
	equipo->cola_ready = list_create();
	equipo->exit = list_create();

	char** pos = string_get_string_as_array(datos_config->posiciones);
	char** poks = string_get_string_as_array(datos_config->pokemones);
	char** objs = string_get_string_as_array(datos_config->objetivos);
	char** posicion;
	char j = 'A';
	for(int i=0;i<cant_entrenadores(pos);i++){
		posicion = string_split(pos[i],"|");
		int posx = atoi(posicion[0]);
		int posy = atoi(posicion[1]);
		int tid = j;
		char* pok = poks[i];
		char* obj = objs[i];
		crear_entrenador(tid,posx,posy,pok,obj);
		free(posicion[0]);
		free(posicion[1]);
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
		printf("Hilo De Entrenador%c Create\n",entre->tid);
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


	if(pokemones){
		char** poks = string_split(pokemones,"|");
		for(int i=0;i<cant_pokemones(poks);i++){
			pokemon* poke = crear_pokemon(poks[i]);
			list_add(entre->pokemones,poke);
		}
		free(poks);
	}
	if(objetivos){
		char** objs = string_split(objetivos,"|");
		for(int i=0;i<cant_pokemones(objs);i++){
			list_add(entre->objetivos,crear_pokemon(objs[i]));
			list_add(equipo->objetivos,crear_pokemon(objs[i]));
		}
		free(objs);
	}
	list_add(equipo->entrenadores,entre);
	printf("Entrenador%c En Posicion: [%d,%d]\n",entre->tid,entre->posx,entre->posy);
	sleep(1);
}

void salir_equipo(){
	log_info(logger,"Team%d Ha Cumplido Su Objetivo",equipo->pid);
	log_info(logger,"Cantidad Total De Ciclos De CPU Del TEAM: %d",equipo->cant_ciclo);
	void cant_ciclos(entrenador* aux){
		log_info(logger,"Cantidad De Ciclos De CPU Del Entrenador%c: %d",aux->tid,aux->ciclos_totales);
	}
	list_iterate(equipo->exit,(void*)cant_ciclos);
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
				pokemon* pok = pokemon_a_atrapar();
				algoritmo_largo_plazo(pok);
				entre = algoritmo_corto_plazo();
			}
			else{//USANDO FIFO
				log_warning(logger,"EN DEADLOCK");
				t_list* deadlocks = list_filter(equipo->entrenadores,(void*)verificar_deadlock_entrenador);
				entre = list_get(deadlocks,0);
				despertar_entrenador(entre);
				list_destroy(deadlocks);
			}

			if(entre != NULL){
				pthread_mutex_lock(&mutexCPU);
				activar_entrenador(entre);
				pthread_mutex_unlock(&mutexCPU);
			}
		}
		else{
			salir_equipo();
		}
	}
}

void ejecutar_entrenador(entrenador* entre){
	void fin_quantum_handler(){
		printf("\033[1;33m=====Fin De Quantum=====\033[0m\n");
		despertar_entrenador(entre);
		sem_post(&semPoks);
		sem_post(&semExecTeam);
	}
	void fin_de_quantum(){//IMPLEMENTAR
		struct sigaction action;
		action.sa_handler = (void*)fin_quantum_handler;
		action.sa_flags = SA_RESTART|SA_NODEFER;
		sigemptyset(&action.sa_mask);
		sigaction(SIGALRM, &action, 0);
		alarm(datos_config->quantum);
	}

	inicio:	while(1){
		sem_wait(&semExecEntre[entre->tid]);
		while(entre->estado == EXEC){
			if(string_equals_ignore_case(datos_config->algoritmo,"RR")){
				fin_de_quantum();
			}

			if(!verificar_deadlock_equipo()){
				pokemon* pok = pokemon_a_atrapar();
				move_entrenador(entre,pok->posx,pok->posy);
				if(atrapar_pokemon(entre,pok) == 0){
					log_warning(logger,"Entrenador%c BLOCKED En Espera De Caught!",entre->tid);
					list_add(entre->espera_caught,pok);
					pok->espera_caught = 1;
					bloquear_entrenador(entre);
					goto inicio;
				}
			}
			else{//DEADLOCK
				bool entrenador_espera_deadlock(entrenador* aux){
					return aux->estado == BLOCKED && verificar_deadlock_entrenador(aux) && aux->tid != entre->tid;
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
					log_info(logger,"Deadlock Del entrenador%c Ha Solucionado",entre2->tid);
					salir_entrenador(entre2);
				}
				else if(cumplir_objetivo_entrenador(entre)){
					log_info(logger,"Deadlock Del entrenador%c Ha Solucionado",entre->tid);
				}
				else{
					log_info(logger,"Deadlock No Ha Solucionado");
				}
			}

			if(!cumplir_objetivo_entrenador(entre) && !verificar_cantidad_pokemones(entre)){
				log_warning(logger,"entrenador%c BLOCKED En Espera De Solucionar Deadlock!",entre->tid);
				bloquear_entrenador(entre);
				goto inicio;
			}
			else if(!cumplir_objetivo_entrenador(entre)){
				log_warning(logger,"entrenador%c BLOCKED Finaliza Su Recorrido!",entre->tid);
				bloquear_entrenador(entre);
				goto inicio;
			}
			else{
				salir_entrenador(entre);
				sem_post(&semExecTeam);
			}
		}
	}
}

void despertar_entrenador(entrenador* entre){
	if(entre->estado == NEW || entre->estado == BLOCKED || entre->estado == EXEC){
		log_info(logger,"entrenador%c READY",entre->tid);
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
		printf("\033[1;35mentrenador%c Sale De Cola Ready\033[0m\n",entre->tid);
		printf("\033[1;34mentrenador%c EXEC\033[0m\n",entre->tid);
		entre->estado = EXEC;
		equipo->exec = entre;
		entre->start_time = time(NULL);
		sem_post(&semExecEntre[entre->tid]);
	}
}

void bloquear_entrenador(entrenador* entre){
	if(entre->estado == EXEC){
		printf("\033[1;34mentrenador%c BLOCKED\033[0m\n",entre->tid);
		entre->estado = BLOCKED;
		sem_post(&semExecTeam);
	}
}

void salir_entrenador(entrenador* entre){
	if(entre->estado == EXEC || entre->estado == BLOCKED){
		printf("\033[1;35mentrenador%c Ha Cumplido Su Objetivo\033[0m\n",entre->tid);
		printf("\033[1;34mentrenador%c EXIT\033[0m\n",entre->tid);
		entre->estado = EXIT;
		entre->finish_time = time(NULL);
		entre->turnaround_time = entre->finish_time - entre->arrival_time;

		bool by_tid(entrenador* aux){
			return aux->tid == entre->tid;
		}
		pthread_rwlock_wrlock(&lockEntrenadores);
		list_remove_by_condition(equipo->entrenadores,(void*)by_tid);
		list_add(equipo->exit,entre);
		pthread_cancel(entre->hilo);
		pthread_rwlock_unlock(&lockEntrenadores);
	}
}

void move_entrenador(entrenador* entre,int posx,int posy){
	log_info(logger,"entrenador%c Moviendose A Posicion: [%d,%d]",entre->tid,posx,posy);

	int ciclosx = abs(entre->posx-posx) * CICLOS_MOVER;
	int ciclosy = abs(entre->posy-posy) * CICLOS_MOVER;
	for(int i=0;i<ciclosx;i++){
		printf("\033[1;32mentrenador%c Moviendose Por El Eje X ...\033[0m\n",entre->tid);
		movimiento_ejex_entrenador(entre,posx);
		entre->service_time++;
		sumar_ciclos(entre,1);
		sleep(datos_config->retardo_cpu);
	}
	for(int i=0;i<ciclosy;i++){
		printf("\033[1;32mentrenador%c Moviendose Por El Eje Y ...\033[0m\n",entre->tid);
		movimiento_ejey_entrenador(entre,posy);
		entre->service_time++;
		sumar_ciclos(entre,1);
		sleep(datos_config->retardo_cpu);
	}
}

void movimiento_ejex_entrenador(entrenador* entre,int posx){
	if(entre->posx != posx){
		if(entre->posx < posx){
			entre->posx++;
		}
		else if(entre->posx > posx){
			entre->posx--;
		}
	}
	printf("Entrenador%c En Posicion:[%d,%d]\n",entre->tid,entre->posx,entre->posy);
}

void movimiento_ejey_entrenador(entrenador* entre,int posy){
	if(entre->posy != posy){
		if(entre->posy < posy){
			entre->posy++;
		}
		else if(entre->posy > posy){
			entre->posy--;
		}
	}
	printf("Entrenador%c En Posicion:[%d,%d]\n",entre->tid,entre->posx,entre->posy);
}

bool atrapar_pokemon(entrenador* entre,pokemon* pok){
	int caso_default = 1;
	log_info(logger,"entrenador%c Atrapando %s",entre->tid,pok->name);
	sleep(datos_config->retardo_cpu);

	conexion_broker->catch = crear_conexion(datos_config->ip_broker,datos_config->puerto_broker);
	if(conexion_broker->catch != -1){
		for(int i=0;i<CICLOS_ENVIAR;i++){
			printf("TEAM enviando mensaje a BROKER ...\n");
			sleep(datos_config->retardo_cpu);
			entre->service_time++;
			sumar_ciclos(entre,1);
		}
		enviar_mensaje_catch_pokemon(pok->name,pok->posx,pok->posy,conexion_broker->catch);
		int id = recibir_id_mensaje(conexion_broker->catch);
		msg* mensaje = crear_mensaje(id,CATCH_POKEMON,pok);
		list_add(mensajes,mensaje);
		log_info(logger,"Mensaje CATCH_POKEMON ID_Mensaje: %d",id);
		caso_default = 0;
	}
	else{
		log_error(logger,"DEFAULT: entrenador%c Ha Atrapado Pokemon %s",entre->tid,pok->name);
		pthread_rwlock_wrlock(&lockEntrePoks);
		list_add(entre->pokemones,pok);
		pthread_rwlock_unlock(&lockEntrePoks);
		remove_pokemon_requeridos(pok);
	}
	return caso_default;
}

void intercambiar_pokemon(entrenador* entre1,entrenador* entre2){
	log_info(logger,"entrenador%c y entrenador%c Intercambian Pokemones",entre1->tid,entre2->tid);
	pokemon* retenido1 = pokemon_retenido_espera(entre1,entre2);
	pokemon* retenido2 = pokemon_retenido_espera(entre2,entre1);
	if(retenido1 != NULL && retenido2 != NULL){
		actuar_intercambio(entre1,entre2,retenido1,retenido2);
		agregar_eliminar_pokemon(entre1,retenido2,retenido1);
		agregar_eliminar_pokemon(entre2,retenido1,retenido2);
	}
	else if(retenido1 == NULL){
		if(retenido2 == NULL){
			pokemon* pok1 = pokemon_a_intercambiar(entre1);
			pokemon* pok2 = pokemon_a_intercambiar(entre2);
			actuar_intercambio(entre1,entre2,pok1,pok2);
			agregar_eliminar_pokemon(entre1,pok2,pok1);
			agregar_eliminar_pokemon(entre2,pok1,pok2);
		}
		else{
			pokemon* pok1 = pokemon_a_intercambiar(entre1);
			actuar_intercambio(entre1,entre2,pok1,retenido2);
			agregar_eliminar_pokemon(entre1,retenido2,pok1);
			agregar_eliminar_pokemon(entre2,pok1,retenido2);
		}
	}
	else if(retenido2 == NULL){
		pokemon* pok2 = pokemon_a_intercambiar(entre2);
		actuar_intercambio(entre1,entre2,retenido1,pok2);
		agregar_eliminar_pokemon(entre1,pok2,retenido1);
		agregar_eliminar_pokemon(entre2,retenido1,pok2);
	}
}

void actuar_intercambio(entrenador* entre1,entrenador* entre2,pokemon* pok1,pokemon* pok2){
	for(int i=0;i<CICLOS_INTERCAMBIAR;i++){
		printf("\033[1;32mentrenador%c y entrenador%c intercambiando pokemones %s Y %s...\033[0m\n",entre1->tid,entre2->tid,pok1->name,pok2->name);
		sleep(datos_config->retardo_cpu);
		entre1->service_time++;
		sumar_ciclos(entre1,1);
	}
}

void agregar_eliminar_pokemon(entrenador* entre,pokemon* pok_agregar,pokemon* pok_eliminar){
	bool by_name(pokemon* aux){
		return aux->name == pok_eliminar->name;
	}
	list_add(entre->pokemones,pok_agregar);
	list_remove_by_condition(entre->pokemones,(void*)by_name);
}

//PLANIFICACION----------------------------------------------------------------------------------------------------------------------
void algoritmo_largo_plazo(pokemon* pok){
	bool by_estado(entrenador* aux){
		return (aux->estado == NEW || aux->estado == BLOCKED)
				&& !verificar_deadlock_entrenador(aux)
				&& !verificar_espera_caught(aux);
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

bool verificar_espera_caught(entrenador* entre){
	return !list_is_empty(entre->espera_caught);
}

entrenador* algoritmo_corto_plazo(){
	entrenador* entre;
	if(string_equals_ignore_case(datos_config->algoritmo,"FIFO")){
		algoritmo_fifo(equipo->cola_ready);
	}
	else if(string_equals_ignore_case(datos_config->algoritmo,"SJF-SD")){
		algoritmo_sjf_sin_desalojo(equipo->cola_ready);
	}
	else if(string_equals_ignore_case(datos_config->algoritmo,"SJF-CD")){
		algoritmo_sjf_con_desalojo(equipo->cola_ready);
	}
	else{
		algoritmo_round_robin(equipo->cola_ready);
	}

	entre = list_get(equipo->cola_ready,0);
	return entre;
}

void algoritmo_fifo(t_list* cola_ready){
	printf("\033[1;37m==========FIFO==========\033[0m\n");
	bool fifo(entrenador* aux1,entrenador* aux2){
		return aux1->arrival_time < aux2->arrival_time;
	}
	list_sort(cola_ready,(void*)fifo);
}

double estimacion(entrenador* entre){
	double service_time = entre->service_time;
	double alpha = datos_config->alpha;
	double comp1 = service_time * alpha;
	double comp2 = (1 - alpha) * entre->estimacion_anterior;
	double res = comp1 + comp2;
	return res;
}

void algoritmo_sjf_sin_desalojo(t_list* cola_ready){
	printf("\033[1;37m==========SJF SIN DESALOJO==========\033[0m\n");
	bool sjf_sin_desalojo(entrenador* aux1,entrenador* aux2){
		if(estimacion(aux1) == estimacion(aux2)){
			return aux1->arrival_time < aux2->arrival_time;
		}
		return estimacion(aux1) < estimacion(aux2);
	}
	list_sort(cola_ready,(void*)sjf_sin_desalojo);
}

void algoritmo_sjf_con_desalojo(t_list* cola_ready){//implementar
	printf("\033[1;37m==========SJF CON DESALOJO==========\033[0m\n");
	entrenador* exec = equipo->exec;
	entrenador* ready = list_get(cola_ready,0);
	bool sjf_sin_desalojo(entrenador* aux1,entrenador* aux2){
		if(estimacion(aux1) == estimacion(aux2)){
			return aux1->arrival_time < aux2->arrival_time;
		}
		return estimacion(aux1) < estimacion(aux2);
	}
	if(exec->estimacion_anterior > ready->estimacion_anterior){
		list_add(cola_ready,exec);
		list_sort(cola_ready,(void*)sjf_sin_desalojo);
	}
}

void desalojar_entrenador(entrenador* entre){
//IMPLEMENTAR
}

void algoritmo_round_robin(t_list* cola_ready){//IMPLEMENTAR
	printf("\033[1;37m==========ROUND ROBIN==========\033[0m\n");
	bool fifo(entrenador* aux1,entrenador* aux2){
		return aux1->arrival_time < aux2->arrival_time;
	}
	list_sort(cola_ready,(void*)fifo);
}

//DEADLOCK------------------------------------------------------------------------------------------------------------
bool verificar_deadlock_entrenador(entrenador* entre){
	return (verificar_cantidad_pokemones(entre) != 1) && (!cumplir_objetivo_entrenador(entre));
}

bool verificar_deadlock_equipo(){
	return list_all_satisfy(equipo->entrenadores,(void*)verificar_deadlock_entrenador)
			&& list_all_satisfy(equipo->entrenadores,(void*)verificar_pokemon_exceso_no_necesario)
			&& verificar_espera_circular();
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

bool verificar_espera_circular(){
	bool res = 0;
	int size = list_size(equipo->entrenadores);
	for(int i=0;i<size-1;i++){
		entrenador* aux1 = list_get(equipo->entrenadores,i);
		entrenador* aux2 = list_get(equipo->entrenadores,i+1);
		bool retencion_espera(pokemon* pok){
			return !necesitar_pokemon(aux1,pok->name) && necesitar_pokemon(aux2,pok->name);
		}

		if(list_any_satisfy(aux1->pokemones,(void*)retencion_espera)){
			res = 1;
		}
		else{
			res = 0;
		}
	}
	entrenador* primero = list_get(equipo->entrenadores,0);
	entrenador* ultimo = list_get(equipo->entrenadores,size-1);
	bool retencion_espera(pokemon* pok){
		return !necesitar_pokemon(ultimo,pok->name) && necesitar_pokemon(primero,pok->name);
	}

	if(list_any_satisfy(ultimo->pokemones,(void*)retencion_espera)){
		res = 1;
	}
	return res;
}

pokemon* pokemon_a_intercambiar(entrenador* entre1){
	t_list* elegidos;
	bool elegir(pokemon* aux){
		return !necesitar_pokemon(entre1,aux->name);
	}
	elegidos = list_filter(entre1->pokemones,(void*)elegir);
	pokemon* pok = list_get(elegidos,0);
	list_destroy(elegidos);
	return pok;
}

pokemon* pokemon_retenido_espera(entrenador* entre1,entrenador* entre2){
	t_list* elegidos;
	bool elegir(pokemon* aux){
		return !necesitar_pokemon(entre1,aux->name) && necesitar_pokemon(entre2,aux->name);
	}
	elegidos = list_filter(entre1->pokemones,(void*)elegir);
	pokemon* pok = list_get(elegidos,0);
	list_destroy(elegidos);
	return pok;
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
	pthread_rwlock_rdlock(&lockEntrenadores);
	bool res = list_all_satisfy(equipo->entrenadores,(void*)cumplir_objetivo_entrenador);
	pthread_rwlock_unlock(&lockEntrenadores);
	return res;
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

bool contener_pokemon_objetivo(entrenador* entre,char* name){
	bool res = 0;
	bool by_name(pokemon* aux){
		return (strcmp(aux->name,name) == 0);
	}
	pthread_rwlock_rdlock(&lockEntrePoks);
	res = list_any_satisfy(entre->objetivos,(void*)by_name);
	pthread_rwlock_unlock(&lockEntrePoks);
	return res;
}

bool pokemon_exceso(entrenador* entre,char* name){
	int cant = cant_especie_pokemon(entre,name);
	return cant > 0 && cant > cant_especie_objetivo(entre,name);
}

bool necesitar_pokemon(entrenador* entre,char* name){
	return contener_pokemon_objetivo(entre,name) && !contener_pokemon(entre,name);
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

int cant_requerida_pokemon(char* name){
	bool by_name(pokemon* pok){
		return strcmp(pok->name,name) == 0;
	}
	int cant_obtenida = list_count_satisfying(equipo->poks_requeridos,(void*)by_name);
	return cant_especie_objetivo_team(name) - cant_obtenida;
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
	int cant_team = cant_requerida_pokemon(pok);
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

int cant_entrenadores(char** posiciones){
	int cant = 0;
	char* pos = posiciones[0];
	while(pos){
		cant++;
		pos = posiciones[cant];
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
	printf("\033[0;31mPOKEMON %s Eliminado\033[0m\n",pok->name);
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
	void get(pokemon* pok){
		enviar_get_pokemon_broker(pok->name);
	}
	t_list* especies = especies_objetivo_team();
	list_iterate(especies,(void*)get);
	list_destroy(especies);
}


void enviar_mensaje_get_pokemon(char* pokemon,int socket_cliente){
	equipo->cant_ciclo += 1;
	int tam = strlen(pokemon) + 1;
	int name_size = strlen(pokemon);
	t_paquete* paquete = malloc(sizeof(t_paquete));
	paquete->codigo_operacion = GET_POKEMON;
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = sizeof(int) + tam ;
	paquete->buffer->stream = malloc(paquete->buffer->size);
	//memcpy(paquete->buffer->stream,&equipo->pid,sizeof(int));
	memcpy(paquete->buffer->stream,&name_size,sizeof(int));
	memcpy(paquete->buffer->stream+sizeof(int),pokemon,tam);

	int bytes = paquete->buffer->size + sizeof(int)*2;

	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, MSG_NOSIGNAL);

	printf("GET_POKEMON: %s\n",pokemon);

	free(a_enviar);
	free(paquete->buffer->stream);
	free(paquete->buffer);
	free(paquete);
}


void enviar_mensaje_catch_pokemon(char* pokemon,int posx,int posy,int socket_cliente){
	int tam = strlen(pokemon) + 1;
	int name_size = strlen(pokemon);
	position* pos = malloc(sizeof(position));
	pos->posx = posx;
	pos->posy = posy;
	t_paquete* paquete = malloc(sizeof(t_paquete));

	paquete->codigo_operacion = CATCH_POKEMON;
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = sizeof(int) + tam + sizeof(position) ;
	paquete->buffer->stream = malloc(paquete->buffer->size);
	memcpy(paquete->buffer->stream,&name_size,sizeof(int));
	memcpy(paquete->buffer->stream+sizeof(int),pokemon,tam);
	memcpy(paquete->buffer->stream+sizeof(int)+tam,pos,sizeof(position));

	int bytes = paquete->buffer->size + 2*sizeof(int);

	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, MSG_NOSIGNAL);

	log_info(logger,"CATCH_POKEMON:%s",pokemon);

	free(a_enviar);
	free(paquete->buffer->stream);
	free(paquete->buffer);
	free(paquete);
}

void enviar_get_pokemon_broker(char* pokemon){
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

void recibir_appeared_pokemon_gameboy(int cliente_fd){
	int size;
	int desplazamiento = 0;
	int posx,posy;
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
		sem_post(&semPoks);
	}
	else{
		printf("%s No Es Requerido\n",pokemon_name);
	}
}

void recibir_localized_pokemon(){
	while(1){
		int cod_op;
		if(recv(conexion_broker->localized, &cod_op, sizeof(int), MSG_WAITALL) == -1)
			cod_op = -1;
		if(cod_op <= 0){
			conexion_broker->localized = -1;
			reintento_conectar_broker();
			return;
		}
		else{
			t_list* paquete = recibir_paquete(conexion_broker->localized);
			void display(void* valor){
				int id,id_correlacional;
				memcpy(&id,valor,sizeof(int));
				memcpy(&id_correlacional,valor+sizeof(int),sizeof(int));
				localized_pokemon* localized = deserializar_localized(valor+sizeof(int));
				log_info(logger,"Llega Un Mensaje Tipo: LOCALIZED_POKEMON: ID: %d ID_Correlacional: %d Pokemon: %s \n",id,id_correlacional,localized->name);
				void show(pos_cant* pos){
					log_info(logger,"Pos:[%d,%d]|Cantidad:%d",pos->posx,pos->posy,pos->cant);
				}
				list_iterate(localized->pos_cant,(void*)show);
				enviar_ack(LOCALIZED_POKEMON,id,equipo->pid,conexion_broker->localized);

				bool by_id(msg* m){
					return m->id_recibido == id;
				}
				if(list_find(mensajes,(void*)by_id)){
					int cant = cant_requerida_pokemon(localized->name);
					for(int i=0;i<cant;i++){
						pokemon* pok = crear_pokemon(localized->name);
						pos_cant* aux = list_get(localized->pos_cant,i);
						set_pokemon(pok,aux->posx,aux->posy);
						pthread_rwlock_wrlock(&lockPoksRequeridos);
						list_add(equipo->poks_requeridos,pok);
						printf("%s Es Requerido, Agregado A La Lista De Pokemones Requeridos\n",pok->name);
						pthread_rwlock_unlock(&lockPoksRequeridos);
						sem_post(&semPoks);
					}
				}
				else{
					printf("\033[0;31m%s No Es Requerido\033[0m\n",localized->name);
				}
				free(valor);
			}
			list_iterate(paquete,(void*)display);
			list_destroy(paquete);
		}
	}
}

void recibir_caught_pokemon(){
	while(1){
		int cod_op;
		if(recv(conexion_broker->caught, &cod_op, sizeof(int), MSG_WAITALL) == -1)
			cod_op = -1;
		if(cod_op <= 0){
			conexion_broker->caught = -1;
			reintento_conectar_broker();
			return;
		}
		else{
			t_list* paquete = recibir_paquete(conexion_broker->caught);
			void display(void* valor){
				int id,id_correlacional,res;
				memcpy(&id,valor,sizeof(int));
				memcpy(&id_correlacional,valor+sizeof(int),sizeof(int));
				memcpy(&res,valor+sizeof(int)*2,sizeof(int));
				log_info(logger,"Llega Un Mensaje Tipo: CAUGHT_POKEMON ID:%d ID_Correlacional: %d Resultado:%d \n",id,id_correlacional,res);
				enviar_ack(CAUGHT_POKEMON,id,equipo->pid,conexion_broker->caught);

				bool by_tipo_id(msg* aux){
					return aux->tipo_msg==CATCH_POKEMON && aux->id_recibido==id;
				}
				msg* mensaje = list_find(mensajes,(void*)by_tipo_id);
				if(mensaje != NULL && res == 1){
					bool espera_caught(entrenador* aux){
						bool by_pokemon(pokemon* pok){
							return pok->espera_caught == 1 && pok->name == mensaje->pok->name;
						}
						return list_any_satisfy(aux->espera_caught,(void*)by_pokemon);
					}
					entrenador* entre = list_find(equipo->entrenadores,(void*)espera_caught);
					list_add(entre->pokemones,mensaje->pok);
					printf("entrenador%c Ha Atrapado Pokemon %s\n",entre->tid,mensaje->pok->name);
					bool es_caught(pokemon* aux){
						return aux->name == mensaje->pok->name;
					}
					list_remove_by_condition(entre->espera_caught,(void*)es_caught);
					list_remove_by_condition(mensajes,(void*)by_tipo_id);
					remove_pokemon_requeridos(mensaje->pok);
				}
				free(valor);
				//sem_post(&semPoks);
				sem_post(&semExecTeam);
			}
			list_iterate(paquete,(void*)display);
			list_destroy(paquete);
		}
	}
}

void recibir_appeared_pokemon(){
	while(1){
		int cod_op;
		if(recv(conexion_broker->appeared, &cod_op, sizeof(int), MSG_WAITALL) == -1)
			cod_op = -1;
		if(cod_op <= 0){
			conexion_broker->appeared = -1;
			reintento_conectar_broker();
			return;
		}
		else{
			t_list* paquete = recibir_paquete(conexion_broker->appeared);
			void display(void* valor){
				int id,id_correlacional;
				memcpy(&id,valor,sizeof(int));
				memcpy(&id_correlacional,valor+sizeof(int),sizeof(int));
				appeared_pokemon* appeared_pokemon = deserializar_appeared(valor+sizeof(int));
				log_info(logger,"Llega Un Mensaje Tipo: APPEARED_POKEMON: ID: %d ID_Correlacional: %d Pokemon: %s  Pos X:%d  Pos Y:%d\n",id,id_correlacional,appeared_pokemon->name,appeared_pokemon->pos.posx,appeared_pokemon->pos.posy);
				enviar_ack(APPEARED_POKEMON,id,equipo->pid,conexion_broker->appeared);

				if(requerir_atrapar(appeared_pokemon->name)){
					pokemon* pok = crear_pokemon(appeared_pokemon->name);
					set_pokemon(pok,appeared_pokemon->pos.posx,appeared_pokemon->pos.posy);
					pthread_rwlock_wrlock(&lockPoksRequeridos);
					list_add(equipo->poks_requeridos,pok);
					printf("%s Es Requerido, Agregado A La Lista De Pokemones Requeridos\n",pok->name);
					pthread_rwlock_unlock(&lockPoksRequeridos);
					sem_post(&semPoks);
				}
				else{
					printf("\033[0;31m%s No Es Requerido\033[0m\n",appeared_pokemon->name);
				}
				free(valor);
			}
			list_iterate(paquete,(void*)display);
			list_destroy(paquete);
		}
	}
}

//BROKER---GAMEBOY--------------------------------------------------------------------------------------------------------------------------
void suscribirse_broker(){
	if(suscribirse_appeared() == 1 && suscribirse_localized() == 1 && suscribirse_caught() == 1){
		recibir_mensajes();
		equipo->suscrito = 1;
		printf("TEAM Se Ha Suscrito\n");
	}
	else{
		log_error(logger,"No Se Puede Conectarse A Broker");
		reintento_conectar_broker();
	}
}

void conectar_broker(){
	conexion_broker->localized = crear_conexion(datos_config->ip_broker,datos_config->puerto_broker);
	conexion_broker->caught = crear_conexion(datos_config->ip_broker,datos_config->puerto_broker);
	conexion_broker->appeared = crear_conexion(datos_config->ip_broker,datos_config->puerto_broker);
	recibir_mensajes();
}

void recibir_mensajes(){
	pthread_create(&suscripcion_caught,NULL,(void*)recibir_caught_pokemon,NULL);
	pthread_detach(suscripcion_caught);
	pthread_create(&suscripcion_appeared,NULL,(void*)recibir_appeared_pokemon,NULL);
	pthread_detach(suscripcion_appeared);
	pthread_create(&suscripcion_localized,NULL,(void*)recibir_localized_pokemon,NULL);
	pthread_detach(suscripcion_localized);
}

int suscribirse_appeared(){
	conexion_broker->appeared = crear_conexion(datos_config->ip_broker,datos_config->puerto_broker);
	int res = 0;
	if(conexion_broker->appeared != -1){
		log_warning(logger,"Conectado A Broker Suscribirse a APPEARED");
		enviar_info_suscripcion(APPEARED_POKEMON,conexion_broker->appeared,equipo->pid);
		if(recibir_confirmacion_suscripcion(conexion_broker->appeared,APPEARED_POKEMON) != -1){
			res = 1;
		}
	}
	return res;
}

int suscribirse_localized(){
	conexion_broker->localized = crear_conexion(datos_config->ip_broker,datos_config->puerto_broker);
	int res = 0;
	if(conexion_broker->localized != -1){
		log_warning(logger,"Conectado A Broker Suscribirse a LOCALIZED");
		enviar_info_suscripcion(LOCALIZED_POKEMON,conexion_broker->localized,equipo->pid);
		if(recibir_confirmacion_suscripcion(conexion_broker->localized,LOCALIZED_POKEMON) !=-1){
			res = 1;
		}
	}
	return res;
}

int suscribirse_caught(){
	conexion_broker->caught = crear_conexion(datos_config->ip_broker,datos_config->puerto_broker);
	int res = 0;
	if(conexion_broker->localized != -1){
		log_warning(logger,"Conectado A Broker Suscribirse a CAUGHT");
		enviar_info_suscripcion(CAUGHT_POKEMON,conexion_broker->caught,equipo->pid);
		if(recibir_confirmacion_suscripcion(conexion_broker->caught,CAUGHT_POKEMON) != -1){
			res = 1;
		}
	}
	return res;
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
	if(conexion_broker->appeared == -1 || conexion_broker->localized == -1 || conexion_broker->caught == -1){
		log_warning(logger,"Reconectando A Broker...");
		if(!equipo->suscrito){
			suscribirse_broker(equipo->pid);
		}
		else{
			log_error(logger,"No Se Puede Conectarse A Broker.");
			conectar_broker();
		}
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

	socklen_t tam_direccion = sizeof(struct sockaddr_in);
	int socket_cliente = accept(socket_servidor, (void*) &dir_cliente, &tam_direccion);
	int* cliente_fd = malloc(sizeof(int));
	*cliente_fd = socket_cliente;
	log_info(logger,"Proceso Conectado Con Socket %d",*cliente_fd);

	int cod_op;
	if(recv(socket_cliente, &cod_op, sizeof(int), MSG_WAITALL) == -1)
		cod_op = -1;
	recibir_appeared_pokemon_gameboy(socket_cliente);
	free(cliente_fd);
}


msg* crear_mensaje(int id,int tipo,pokemon* pok){
	msg* mensaje = malloc(sizeof(msg));
	mensaje->id_recibido = id;
	mensaje->tipo_msg = tipo;
	mensaje->pok = pok;
	return mensaje;
}
