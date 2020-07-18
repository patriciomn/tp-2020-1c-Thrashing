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
sem_t semExecEntre[CANT_ENTRE];
sem_t semPoks;
sem_t recibir_appeared;
pthread_rwlock_t lockPoksRequeridos =  PTHREAD_RWLOCK_INITIALIZER;
pthread_rwlock_t lockEntrePoks = PTHREAD_RWLOCK_INITIALIZER;
pthread_rwlock_t lockColaReady = PTHREAD_RWLOCK_INITIALIZER;
pthread_rwlock_t lockEntrenadores = PTHREAD_RWLOCK_INITIALIZER;
pthread_rwlock_t lockExec = PTHREAD_RWLOCK_INITIALIZER;
pthread_mutex_t mutexCPU = PTHREAD_MUTEX_INITIALIZER;

int main(int argc,char* argv[]){
	iniciar_team(argv[1]);
	ejecutar_equipo();
	return 0;
}

void iniciar_semaforos(){
	sem_init(&semExecTeam,0,1);
	sem_init(&semPoks,0,0);
	for(int i='A';i<list_size(equipo->entrenadores);i++){
		sem_init(&semExecEntre[i],0,0);
	}
	sem_init(&recibir_appeared,0,0);
}

void iniciar_team(char* teamConfig){
	iniciar_config(teamConfig);
	logger = log_create(datos_config->log_file,"team",1,LOG_LEVEL_INFO);
	conexion_broker = malloc(sizeof(conexion));
	mensajes = list_create();
	crear_team();
	iniciar_semaforos();
	suscribirse_broker();
	pthread_create(&servidor_gameboy,NULL,(void*)iniciar_servidor,NULL);
	pthread_detach(servidor_gameboy);
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
	datos_config->id = config_get_int_value(config,"ID");
}

//CREACION---------------------------------------------------------------------------------------------------------------------
char* crear_string_vacios_con_coma(char* string,int cant){
	char* corchete_i = "[ %s";
	char* corchete_d = "]";
	char* vacio = string_new();
	for(int i=0;i<cant-1;i++){
		string_append_with_format(&vacio, "%s ", ",");
	}
	string = string_from_format(corchete_i,vacio);
	string_append(&string,corchete_d);
	free(vacio);
	return string;
}

void crear_team(){
	equipo = malloc(sizeof(team));
	equipo->pid = datos_config->id;
	equipo->entrenadores = list_create();
	equipo->objetivos = list_create();
	equipo->poks_requeridos = list_create();
	equipo->cant_ciclo = 0;
	equipo->cola_ready = list_create();
	equipo->cola_deadlock = list_create();
	equipo->exit = list_create();
	equipo->blocked = NULL;
	equipo->metrica_deadlock = list_create();
	equipo->exec = NULL;
	conexion_broker->localized = -1;
	conexion_broker->caught = -1;
	conexion_broker->appeared = -1;

	char** pos = string_get_string_as_array(datos_config->posiciones);
	if(string_equals_ignore_case(datos_config->pokemones,"[]")){
		datos_config->pokemones = crear_string_vacios_con_coma(datos_config->pokemones,cant_entrenadores(pos));
	}
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
		printf("Hilo De Entrenador%c Creado\n",entre->tid);
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
	entre->quantum = datos_config->quantum;
	entre->pok_atrapar = NULL;
	entre->cant_cambio_contexto = 0;

	char** poks = string_split(pokemones,"|");
	for(int i=0;i<cant_pokemones(poks);i++){
		pokemon* poke = crear_pokemon(poks[i]);
		list_add(entre->pokemones,poke);
	}
	free(poks);

	char** objs = string_split(objetivos,"|");
	for(int i=0;i<cant_pokemones(objs);i++){
		list_add(entre->objetivos,crear_pokemon(objs[i]));
		list_add(equipo->objetivos,crear_pokemon(objs[i]));
	}
	free(objs);

	list_add(equipo->entrenadores,entre);
	printf("Entrenador%c En Posicion: [%d,%d]\n",entre->tid,entre->posx,entre->posy);
}

void salir_equipo(){
	log_info(logger,"Team%d Cumple Su Objetivo",equipo->pid);
	log_info(logger,"Cantidad Total De Ciclos De CPU Del TEAM: %d",equipo->cant_ciclo);
	void metrica(entrenador* aux){
		log_info(logger,"Cantidad De Cambios De Contexto Del Entrenador%c: %d",aux->tid,aux->cant_cambio_contexto);
		log_info(logger,"Cantidad De Ciclos De CPU Del Entrenador%c: %d",aux->tid,aux->ciclos_totales);
	}
	list_iterate(equipo->exit,(void*)metrica);

	log_info(logger,"Entrenadores En Deadlock:");
	void deadlock(entrenador* aux){
		log_info(logger,"Entrenador%c",aux->tid);
	}
	list_iterate(equipo->metrica_deadlock,(void*)deadlock);

	void limpiar(pokemon* pok){
		free(pok->name);
		free(pok);
	}
	list_destroy_and_destroy_elements(equipo->poks_requeridos,(void*)limpiar);

	free(datos_config->objetivos);
	free(datos_config->pokemones);
	if(conexion_broker->localized != -1 || conexion_broker->appeared != -1 || conexion_broker->caught != -1){
		liberar_conexion(conexion_broker->appeared);
		liberar_conexion(conexion_broker->caught);
		liberar_conexion(conexion_broker->localized);
		pthread_cancel(suscripcion_appeared);
		pthread_cancel(suscripcion_localized);
		pthread_cancel(suscripcion_caught);
	}

	pthread_cancel(reintento);
	pthread_cancel(servidor_gameboy);
	log_destroy(logger);
	return;
}

//EJECUCION----------------------------------------------------------------------------------------------------------------
void ejecutar_equipo(){
	while(1){
		sem_wait(&semExecTeam);
		entrenador* entre;
		if(!cumplir_objetivo_team()){
			if(!verificar_deadlock_equipo()){
				sem_wait(&semPoks);
				if(!list_is_empty(equipo->cola_ready)){
					entre = algoritmo_corto_plazo(equipo->cola_ready);
				}
			}
			else{
				printf("=====Solucionando Deadlock=====\n");
				if(equipo->blocked == NULL){
					equipo->blocked = entrenador_bloqueado_deadlock();
					bool es_bloqueado(entrenador* aux){
						return aux->tid == equipo->blocked->tid;
					}
					list_remove_by_condition(equipo->cola_deadlock,(void*)es_bloqueado);
				}
				entre = algoritmo_deadlock(equipo->cola_deadlock);
				if(entre != NULL){
					despertar_entrenador(entre);
				}
			}

			if(entre != NULL && equipo->exec == NULL){
				pthread_mutex_lock(&mutexCPU);
				activar_entrenador(entre);
				pthread_mutex_unlock(&mutexCPU);
			}
		}
		else{
			salir_equipo();
			exit(0);
		}
	}
}

void ejecutar_entrenador(entrenador* entre){
	if(string_equals_ignore_case(datos_config->algoritmo,"RR") || string_equals_ignore_case(datos_config->algoritmo,"SJF-CD")){
		actuar_entrenador_con_desalojo(entre);
	}
	else{
		actuar_entrenador_sin_desalojo(entre);
	}
}

void replanificar_entrenador(entrenador* entre){
	if(!list_is_empty(equipo->poks_requeridos)){
		bool no_requerido(pokemon* pok){
			return pok->requerido == 0;
		}
		t_list* libres = list_filter(equipo->poks_requeridos,(void*)no_requerido);
		bool distancias(pokemon* p1,pokemon* p2){
			return distancia_pokemon(entre,p1) < distancia_pokemon(entre,p2);
		}
		list_sort(libres,(void*)distancias);
		pokemon* pok = list_remove(libres,0);
		if(pok != NULL){
			algoritmo_largo_plazo(pok);
		}

		list_destroy(libres);
	}
}

void actuar_entrenador_sin_desalojo(entrenador* entre){
	inicio:while(1){
		bool by_id(entrenador* aux){
			return aux->tid == entre->tid;
		}
		sem_wait(&semExecEntre[entre->tid]);
		if(!verificar_deadlock_equipo()){
			pokemon* pok = entre->pok_atrapar;
			move_entrenador(entre,pok->posx,pok->posy);
			if(atrapar_pokemon(entre,pok) == 0){
				log_warning(logger,"Entrenador%c BLOCKED En Espera De Caught!",entre->tid);
				list_add(entre->espera_caught,pok);
				pok->espera_caught = 1;
				bloquear_entrenador(entre);
				if(!list_is_empty(equipo->cola_ready)){
					sem_post(&semPoks);
				}
				goto inicio;
			}
		}
		else{
			list_remove_by_condition(equipo->cola_deadlock,(void*)by_id);
			move_entrenador(entre,equipo->blocked->posx,equipo->blocked->posy);
			intercambiar_pokemon(entre,equipo->blocked);
			if(cumplir_objetivo_entrenador(equipo->blocked) && cumplir_objetivo_entrenador(entre)){
				log_info(logger,"Deadlock Solucionado");
				salir_entrenador(equipo->blocked);
				equipo->blocked = NULL;
			}
			else if(cumplir_objetivo_entrenador(equipo->blocked)){
				log_info(logger,"Deadlock Del Entrenador%c Solucionado",equipo->blocked->tid);
				salir_entrenador(equipo->blocked);
				equipo->blocked = NULL;
			}
			else if(cumplir_objetivo_entrenador(entre)){
				log_info(logger,"Deadlock Del Entrenador%c Solucionado",entre->tid);
			}
			else{
				log_info(logger,"Deadlock No Solucionado");
			}
		}

		if(!cumplir_objetivo_entrenador(entre) && !verificar_cantidad_pokemones(entre)){
			log_warning(logger,"Entrenador%c BLOCKED En Espera De Solucionar Deadlock!",entre->tid);
			if(!list_any_satisfy(equipo->cola_deadlock,(void*)by_id)){
				list_add(equipo->cola_deadlock,entre);
				printf("Entrenador%c Agregado A La Cola De Deadlocks\n",entre->tid);
			}
			if(!list_any_satisfy(equipo->metrica_deadlock,(void*)by_id)){
				list_add(equipo->metrica_deadlock,entre);
			}
			bloquear_entrenador(entre);
			detectar_deadlock();
			goto inicio;
		}
		else if(!cumplir_objetivo_entrenador(entre)){
			log_warning(logger,"Entrenador%c BLOCKED Finaliza Su Recorrido!",entre->tid);
			bloquear_entrenador(entre);
			replanificar_entrenador(entre);
			goto inicio;
		}
		else{
			salir_entrenador(entre);
			sem_post(&semExecTeam);
			detectar_deadlock();
		}
	}

}

void actuar_entrenador_con_desalojo(entrenador* entre){
	inicio:while(1){
		bool by_id(entrenador* aux){
			return aux->tid == entre->tid;
		}
		sem_wait(&semExecEntre[entre->tid]);
		if(!verificar_deadlock_equipo()){
			pokemon* pok = entre->pok_atrapar;
			move_entrenador(entre,pok->posx,pok->posy);
			if((string_equals_ignore_case(datos_config->algoritmo,"RR") && entre->quantum <= 0) ||
				(string_equals_ignore_case(datos_config->algoritmo,"SJF-CD") && con_estimacion_menor(entre))){
				desalojar_entrenador(entre);
				goto inicio;
			}
			if(atrapar_pokemon(entre,pok) == 0){
				log_warning(logger,"Entrenador%c BLOCKED En Espera De Caught!",entre->tid);
				list_add(entre->espera_caught,pok);
				pok->espera_caught = 1;
				bloquear_entrenador(entre);
				if(!list_is_empty(equipo->cola_ready)){
					sem_post(&semPoks);
				}
				goto inicio;
			}
			if((string_equals_ignore_case(datos_config->algoritmo,"RR") && entre->quantum <= 0) ||
				(string_equals_ignore_case(datos_config->algoritmo,"SJF-CD") && con_estimacion_menor(entre))){
				desalojar_entrenador(entre);
				goto inicio;
			}
		}
		else{
			list_remove_by_condition(equipo->cola_deadlock,(void*)by_id);
			move_entrenador(entre,equipo->blocked->posx,equipo->blocked->posy);
			if((string_equals_ignore_case(datos_config->algoritmo,"RR") && entre->quantum <= 0) ||
				(string_equals_ignore_case(datos_config->algoritmo,"SJF-CD") && con_estimacion_menor(entre))){
				list_add(equipo->cola_deadlock,entre);
				desalojar_entrenador(entre);
				goto inicio;
			}
			intercambiar_pokemon(entre,equipo->blocked);
			if(cumplir_objetivo_entrenador(equipo->blocked) && cumplir_objetivo_entrenador(entre)){
				log_info(logger,"Deadlock Solucionado");
				salir_entrenador(equipo->blocked);
				equipo->blocked = NULL;
			}
			else if(cumplir_objetivo_entrenador(equipo->blocked)){
				log_info(logger,"Deadlock Del Entrenador%c Solucionado",equipo->blocked->tid);
				salir_entrenador(equipo->blocked);
				equipo->blocked = NULL;
			}
			else if(cumplir_objetivo_entrenador(entre)){
				log_info(logger,"Deadlock Del Entrenador%c Solucionado",entre->tid);
			}
			else{
				log_info(logger,"Deadlock No Solucionado");
			}
		}

		if(!cumplir_objetivo_entrenador(entre) && !verificar_cantidad_pokemones(entre)){
			log_warning(logger,"Entrenador%c BLOCKED En Espera De Solucionar Deadlock!",entre->tid);
			if(!list_any_satisfy(equipo->cola_deadlock,(void*)by_id)){
				list_add(equipo->cola_deadlock,entre);
				printf("Entrenador%c Agregado A La Cola De Deadlocks\n",entre->tid);
			}
			if(!list_any_satisfy(equipo->metrica_deadlock,(void*)by_id)){
				list_add(equipo->metrica_deadlock,entre);
			}
			bloquear_entrenador(entre);
			detectar_deadlock();
			goto inicio;
		}
		else if(!cumplir_objetivo_entrenador(entre)){
			log_warning(logger,"Entrenador%c BLOCKED Finaliza Su Recorrido!",entre->tid);
			bloquear_entrenador(entre);
			replanificar_entrenador(entre);
			goto inicio;
		}
		else{
			salir_entrenador(entre);
			sem_post(&semExecTeam);
			detectar_deadlock();
		}
	}
}

void desalojar_entrenador(entrenador* entre){
	if(string_equals_ignore_case(datos_config->algoritmo,"RR") && entre->estado == EXEC){
		printf("\033[1;33m-----Fin De Quantum-----\033[0m\n");
		printf("\033[1;35mEntrenador%c Desalojado\033[0m\n",entre->tid);
	}
	else{
		printf("\033[1;35mEntrenador%c Desalojado Por Otro Entrenador Que Tiene Menor Estimacion\033[0m\n",entre->tid);
	}
	entre->cant_cambio_contexto++;
	despertar_entrenador(entre);
	entre->service_time = 0;
	pthread_rwlock_wrlock(&lockExec);
	equipo->exec = NULL;
	pthread_rwlock_unlock(&lockExec);

	if(!list_is_empty(equipo->cola_ready)){
		sem_post(&semPoks);
		sem_post(&semExecTeam);
	}

}

void despertar_entrenador(entrenador* entre){
	if(entre->estado == NEW || entre->estado == BLOCKED || entre->estado == EXEC){
		printf("\033[1;34mEntrenador%c READY\033[0m\n",entre->tid);
		entre->estado = READY;
		entre->estimacion_anterior = estimacion(entre);
		pthread_rwlock_wrlock(&lockColaReady);
		list_add(equipo->cola_ready,entre);
		pthread_rwlock_unlock(&lockColaReady);
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
		printf("\033[1;35mEntrenador%c Sale De Cola Ready\033[0m\n",entre->tid);
		printf("\033[1;34mEntrenador%c EXEC\033[0m\n",entre->tid);
		entre->estado = EXEC;
		pthread_rwlock_wrlock(&lockExec);
		equipo->exec = entre;
		pthread_rwlock_unlock(&lockExec);
		entre->start_time = time(NULL);
		sem_post(&semExecEntre[entre->tid]);
	}
}

void bloquear_entrenador(entrenador* entre){
	if(entre->estado == EXEC){
		printf("\033[1;34mEntrenador%c BLOCKED\033[0m\n",entre->tid);
		entre->estado = BLOCKED;
		entre->cant_cambio_contexto++;
		pthread_rwlock_wrlock(&lockExec);
		equipo->exec = NULL;
		pthread_rwlock_unlock(&lockExec);
		if(!verificar_deadlock_equipo()){
			sem_post(&semExecTeam);
		}
	}
}

void salir_entrenador(entrenador* entre){
	printf("\033[1;35mEntrenador%c Cumple Su Objetivo\033[0m\n",entre->tid);
	printf("\033[1;34mEntrenador%c EXIT\033[0m\n",entre->tid);
	entre->estado = EXIT;
	entre->cant_cambio_contexto++;
	entre->finish_time = time(NULL);
	entre->turnaround_time = entre->finish_time - entre->arrival_time;
	pthread_rwlock_wrlock(&lockExec);
	equipo->exec = NULL;
	pthread_rwlock_unlock(&lockExec);

	for(int i=0;i<list_size(entre->pokemones);i++){
		pokemon* pok = list_get(entre->pokemones,i);
		bool by_name(pokemon* aux){
			return string_equals_ignore_case(aux->name,pok->name);
		}
		pokemon* cumplido = list_remove_by_condition(equipo->objetivos,(void*)by_name);
		free(cumplido);
	}

	bool by_tid(entrenador* aux){
		return aux->tid == entre->tid;
	}

	pthread_rwlock_wrlock(&lockEntrenadores);
	list_remove_by_condition(equipo->entrenadores,(void*)by_tid);
	list_add(equipo->exit,entre);
	pthread_cancel(entre->hilo);
	pthread_rwlock_unlock(&lockEntrenadores);
}

bool con_estimacion_menor(entrenador* entre){
	printf("Entrenador%c Estimacion %lf\n",entre->tid,entre->estimacion_anterior);
	bool menor(entrenador* aux){
		printf("Entrenador%c Estimacion %lf\n",aux->tid,aux->estimacion_anterior);
		return (aux->tid != entre->tid) && (aux->estimacion_anterior < entre->estimacion_anterior);
	}
	return list_any_satisfy(equipo->cola_ready,(void*)menor);
}

void move_entrenador(entrenador* entre,int posx,int posy){
	if(entre->posx != posx || entre->posy != posy){
		log_info(logger,"Entrenador%c Moviendose A Posicion: [%d,%d]",entre->tid,posx,posy);
		int ciclosx = abs(entre->posx-posx) * CICLO_ACCION;
		int ciclosy = abs(entre->posy-posy) * CICLO_ACCION;
		for(int i=0;i<ciclosx;i++){
			if((string_equals_ignore_case(datos_config->algoritmo,"RR") && entre->quantum <= 0) ||
				(string_equals_ignore_case(datos_config->algoritmo,"SJF-CD") && con_estimacion_menor(entre))){
				return;
			}
			movimiento_ejex_entrenador(entre,posx);
			entre->quantum--;
			entre->service_time++;
			sumar_ciclos(entre,CICLO_ACCION);
			sleep(datos_config->retardo_cpu);
		}
		for(int i=0;i<ciclosy;i++){
			if((string_equals_ignore_case(datos_config->algoritmo,"RR") && entre->quantum <= 0) ||
				(string_equals_ignore_case(datos_config->algoritmo,"SJF-CD") && con_estimacion_menor(entre))){
				return;
			}
			movimiento_ejey_entrenador(entre,posy);
			entre->quantum--;
			entre->service_time++;
			sumar_ciclos(entre,CICLO_ACCION);
			sleep(datos_config->retardo_cpu);
		}
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
	printf("\033[1;32mEntrenador%c Moviendose Por El Eje X. En Posicion:[%d,%d]\033[0m\n",entre->tid,entre->posx,entre->posy);
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
	printf("\033[1;32mEntrenador%c Moviendose Por El Eje Y. En Posicion:[%d,%d]\033[0m\n",entre->tid,entre->posx,entre->posy);
}

bool atrapar_pokemon(entrenador* entre,pokemon* pok){
	log_info(logger,"Entrenador%c Atrapando %s",entre->tid,pok->name);
	int caso_default = 1;
	conexion_broker->catch = crear_conexion(datos_config->ip_broker,datos_config->puerto_broker);
	if(conexion_broker->catch != -1){
		for(int i=0;i<CICLO_ACCION;i++){
			printf("Entrenador%c Enviando mensaje CATCH_POKEMON:%s a BROKER ...\n",entre->tid,pok->name);
			remove_pokemon_requeridos(pok);
			entre->quantum--;
			entre->service_time++;
			sumar_ciclos(entre,CICLO_ACCION);
			sleep(datos_config->retardo_cpu);
		}
		enviar_mensaje_catch_pokemon(pok->name,pok->posx,pok->posy,conexion_broker->catch);
		int id = recibir_id_mensaje(conexion_broker->catch);
		msg* mensaje = crear_mensaje(id,CATCH_POKEMON,pok);
		list_add(mensajes,mensaje);
		log_info(logger,"Mensaje CATCH_POKEMON ID_Mensaje: %d",id);
		caso_default = 0;
	}
	else{
		log_warning(logger,"DEFAULT: Entrenador%c Atrapa Pokemon %s",entre->tid,pok->name);
		entre->pok_atrapar = NULL;
		sumar_ciclos(entre,CICLO_ACCION);
		pthread_rwlock_wrlock(&lockEntrePoks);
		list_add(entre->pokemones,pok);
		pthread_rwlock_unlock(&lockEntrePoks);
		remove_pokemon_requeridos(pok);
		bool existe_localized_appeared(msg* m){
			return (m->tipo_msg == LOCALIZED_POKEMON || m->tipo_msg == APPEARED_POKEMON) && string_equals_ignore_case(m->pok->name,pok->name)
					&& m->pok->posx == pok->posx && m->pok->posy == pok->posy;
		}
		msg* borrar = list_remove_by_condition(mensajes,(void*)existe_localized_appeared);
		if(borrar != NULL){
			free(borrar);
		}

	}

	return caso_default;
}

void intercambiar_pokemon(entrenador* entre1,entrenador* entre2){
	log_info(logger,"Entrenador%c y Entrenador%c Intercambian Pokemones",entre1->tid,entre2->tid);
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
		printf("\033[1;32mEntrenador%c y Entrenador%c intercambiando pokemones %s Y %s...\033[0m\n",entre1->tid,entre2->tid,pok1->name,pok2->name);
		entre1->quantum--;
		entre1->service_time++;
		sumar_ciclos(entre1,CICLO_ACCION);
		sleep(datos_config->retardo_cpu);
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
		if(distancia_pokemon(aux,pok) == distancia_pokemon(next,pok)){
			return aux->tid < next->tid;
		}
		return (distancia_pokemon(aux,pok)) < (distancia_pokemon(next,pok));
	}
	pthread_rwlock_wrlock(&lockEntrenadores);
	t_list* entrenadores = list_filter(equipo->entrenadores,(void*)by_estado);
	pthread_rwlock_unlock(&lockEntrenadores);
	if(!list_is_empty(entrenadores)){
		list_sort(entrenadores,(void*)by_distancia);
		entrenador* elegido = list_get(entrenadores,0);
		elegido->pok_atrapar = pok;
		pok->requerido = 1;
		despertar_entrenador(elegido);
		sem_post(&semPoks);
		/*float distancia = distancia_pokemon(elegido,pok);

		void pasar_ready(entrenador* aux){
			if(distancia_pokemon(aux,pok) == distancia){
				aux->pok_atrapar = pok;
				pok->requerido = 1;
				despertar_entrenador(aux);
			}
		}
		list_iterate(entrenadores,(void*)pasar_ready);*/
	}
	list_destroy(entrenadores);
}

bool verificar_espera_caught(entrenador* entre){
	return !list_is_empty(entre->espera_caught);
}

entrenador* algoritmo_corto_plazo(t_list* cola){
	if(string_equals_ignore_case(datos_config->algoritmo,"FIFO")){
		return algoritmo_fifo(cola);
	}
	else if(string_equals_ignore_case(datos_config->algoritmo,"SJF-SD")){
		printf("\033[1;37m==========SJF SIN DESALOJO==========\033[0m\n");
		return algoritmo_sjf(cola);
	}
	else if(string_equals_ignore_case(datos_config->algoritmo,"SJF-CD")){
		printf("\033[1;37m==========SJF CON DESALOJO==========\033[0m\n");
		return algoritmo_sjf(cola);
	}
	else{
		return algoritmo_round_robin(cola);
	}
	return NULL;
}

entrenador* algoritmo_deadlock(t_list* cola){
	//entrenador* entre = algoritmo_corto_plazo(cola);
	entrenador* entre = list_get(cola,0);
	if(entre != NULL && string_equals_ignore_case(datos_config->algoritmo,"RR")){
		entre->quantum = datos_config->quantum;
	}
	return entre;
}

double estimacion(entrenador* entre){
	double service_time = (double)entre->service_time;
	double alpha = datos_config->alpha;
	double comp1 = service_time * alpha;
	double comp2 = (1 - alpha) * entre->estimacion_anterior;
	double res = comp1 + comp2;
	return res;
}

entrenador* algoritmo_fifo(t_list* cola){
	printf("\033[1;37m==========FIFO==========\033[0m\n");
	entrenador* elegido = list_get(cola,0);
	return elegido;
}

entrenador* algoritmo_sjf(t_list* cola){
	entrenador* entre = list_get(cola,0);
	if(entre != NULL){
		bool sjf_sin_desalojo(entrenador* aux1,entrenador* aux2){
			if(estimacion(aux1) == estimacion(aux2)){
				return aux1->tid < aux2->tid;
			}
			return estimacion(aux1) < estimacion(aux2);
		}
		t_list* sorted = list_sorted(cola,(void*)sjf_sin_desalojo);
		entre = list_get(sorted,0);
		list_destroy(sorted);
	}
	return entre;
}

entrenador* algoritmo_round_robin(t_list* cola){
	printf("\033[1;37m==========ROUND ROBIN==========\033[0m\n");
	entrenador* entre = list_get(cola,0);
	if(entre != NULL){
		entre->quantum = datos_config->quantum;
	}
	return entre;
}

//DEADLOCK------------------------------------------------------------------------------------------------------------
entrenador* entrenador_bloqueado_deadlock(){
	bool by_tid(entrenador* aux1,entrenador* aux2){
		return aux1->tid < aux2->tid;
	}
	t_list* sorted = list_sorted(equipo->cola_deadlock,(void*)by_tid);
	entrenador* entre = list_get(sorted,0);
	list_destroy(sorted);
	return entre;
}

void detectar_deadlock(){
	if(verificar_deadlock_equipo()){
		log_warning(logger,"EN DEADLOCK");
		sem_post(&semExecTeam);
	}
}

bool verificar_deadlock_entrenador(entrenador* entre){
	return (verificar_cantidad_pokemones(entre) != 1) && (!cumplir_objetivo_entrenador(entre));
}

bool verificar_deadlock_equipo(){
	bool res = list_all_satisfy(equipo->entrenadores,(void*)verificar_deadlock_entrenador)
					&& list_all_satisfy(equipo->entrenadores,(void*)verificar_pokemon_exceso_no_necesario)
					&& verificar_espera_circular();
	return res;
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
	for(int i=0;i<size;i++){
		entrenador* aux = list_get(equipo->entrenadores,i);

		bool retencion_espera(pokemon* pok){
			bool necesitar(entrenador* entre){
				return necesitar_pokemon(entre,pok->name) && aux->tid != entre->tid;
			}
			return !necesitar_pokemon(aux,pok->name) && list_any_satisfy(equipo->entrenadores,(void*)necesitar);
		}
		bool espera_retencion(entrenador* entre){
			bool espera_retencion_pok(pokemon* pok){
				return entre->tid != aux->tid && !necesitar_pokemon(entre,pok->name)  && necesitar_pokemon(aux,pok->name);
			}
			return list_any_satisfy(entre->pokemones,(void*)espera_retencion_pok);
		}

		if(list_any_satisfy(aux->pokemones,(void*)retencion_espera) && list_any_satisfy(equipo->entrenadores,(void*)espera_retencion)){
			res = 1;
		}
		else{
			return 0;
		}
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
		return string_equals_ignore_case(aux->name,name);
	}
	pthread_rwlock_rdlock(&lockEntrePoks);
	res = list_any_satisfy(entre->pokemones,(void*)by_name);
	pthread_rwlock_unlock(&lockEntrePoks);
	return res;
}

bool contener_pokemon_objetivo(entrenador* entre,char* name){
	bool res = 0;
	bool by_name(pokemon* aux){
		return string_equals_ignore_case(aux->name,name);
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
		return string_equals_ignore_case(aux->name,name);
	}
	res = list_count_satisfying(entre->objetivos,(void*)by_name);
	return res;
}

int cant_especie_pokemon(entrenador* entre,char* name){
	int res = 0;
	bool by_name(pokemon* aux){
		return string_equals_ignore_case(aux->name,name);
	}
	pthread_rwlock_rdlock(&lockEntrePoks);
	res = list_count_satisfying(entre->pokemones,(void*)by_name);
	pthread_rwlock_unlock(&lockEntrePoks);
	return res;
}

int cant_especie_obtenida_team(char* name){
	int res = 0;

	for(int i=0;i<list_size(equipo->entrenadores);i++){
		entrenador* entre = list_get(equipo->entrenadores,i);
		res += cant_especie_pokemon(entre,name);
	}
	return res;
}

int cant_requerida_pokemon(char* name){
	bool by_name(pokemon* aux){
		return (strcmp(aux->name,name) == 0);
	}
	int cant_atrapando = list_count_satisfying(equipo->poks_requeridos,(void*)by_name);

	return cant_especie_objetivo_team(name) - cant_especie_obtenida_team(name) - cant_atrapando;
}

bool verificar_cantidad_pokemones(entrenador* entre){
	int permite = 0;
	if(list_size(entre->pokemones) < list_size(entre->objetivos)){
		permite = 1;
	}
	return permite;
}

t_list* especies_objetivo_team(){
	t_list* obtenidos = list_create();
	void agregar_obtenido(entrenador* entre){
		list_add_all(obtenidos,entre->pokemones);
		if(entre->pok_atrapar != NULL){
			list_add(obtenidos,entre->pok_atrapar);
		}
	}
	list_iterate(equipo->entrenadores,(void*)agregar_obtenido);

	t_list* requeridos = list_duplicate(equipo->objetivos);

	t_list* especies = list_create();
	for(int i=0;i<list_size(equipo->objetivos);i++){
		pokemon* obj = list_get(equipo->objetivos,i);
		bool by_name(pokemon* aux){
			return strcmp(aux->name,obj->name) == 0;
		}
		int cant_requeridos = list_count_satisfying(requeridos,(void*)by_name);
		int cant_obtenidos = list_count_satisfying(obtenidos,(void*)by_name);
		if((list_any_satisfy(requeridos,(void*)by_name) && !list_any_satisfy(obtenidos,(void*)by_name)) || (cant_requeridos >cant_obtenidos)){
			list_add(especies,obj);
			for(int i=0;i<cant_requeridos;i++){
				list_remove_by_condition(requeridos,(void*)by_name);
			}
		}
	}
	list_destroy(requeridos);
	list_destroy(obtenidos);
	return especies;
}

bool requerir_atrapar(char* pok){
	return cant_requerida_pokemon(pok) != 0;
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
	printf("\033[0;31mPOKEMON %s En Posicion:[%d,%d] Eliminado De Los Requeridos\033[0m\n",pok->name,pok->posx,pok->posy);
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
	memcpy(paquete->buffer->stream,&name_size,sizeof(int));
	memcpy(paquete->buffer->stream+sizeof(int),pokemon,tam);

	int bytes = paquete->buffer->size + sizeof(int)*2;

	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, MSG_NOSIGNAL);

	printf("Enviar Mensaje GET_POKEMON: %s\n",pokemon);

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

	free(pos);
	free(a_enviar);
	free(paquete->buffer->stream);
	free(paquete->buffer);
	free(paquete);
}

void enviar_get_pokemon_broker(char* pokemon){
	conexion_broker->get = crear_conexion(datos_config->ip_broker,datos_config->puerto_broker);
	if(conexion_broker->get == -1){
		log_warning(logger,"DEFAULT: %s No Existe",pokemon);
	}
	else{
		enviar_mensaje_get_pokemon(pokemon,conexion_broker->get);
		int id = recibir_id_mensaje(conexion_broker->get);
		msg* mensaje = crear_mensaje(id,GET_POKEMON,NULL);
		list_add(mensajes,mensaje);
		printf("Recibir Tipo_Mensaje: %s ID_Mensaje: %d\n","GET_POKEMON",id);
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
	log_info(logger,"Llega Un Mensaje Tipo: APPEARED_POKEMON: %s  Pos X:%d  Pos Y:%d",pokemon_name,posx,posy);

	if(requerir_atrapar(pokemon_name)){
		pokemon* pok = crear_pokemon(pokemon_name);
		set_pokemon(pok,posx,posy);
		pok->requerido = 0;
		pthread_rwlock_wrlock(&lockPoksRequeridos);
		list_add(equipo->poks_requeridos,pok);
		pthread_rwlock_unlock(&lockPoksRequeridos);
		printf("%s Es Requerido, Agregado A La Lista De Pokemones Requeridos\n",pok->name);
		algoritmo_largo_plazo(pok);
	}
	else{
		printf("\033[0;31m%s No Es Requerido\033[0m\n",pokemon_name);
		free(pokemon_name);
	}
}

void recibir_localized_pokemon(){
	while(1){
		int cod_op;
		if(recv(conexion_broker->localized, &cod_op, sizeof(int), MSG_WAITALL) == -1)
			cod_op = -1;
		if(cod_op <= 0 || check_socket(conexion_broker->localized) != 1){
			conexion_broker->localized = -1;
			reintento_conectar_broker();
			t_list* especies = especies_objetivo_team();
			void no_existe(pokemon* pok){
				log_warning(logger,"DEFAULT: %s No Existe",pok->name);
			}
			list_iterate(especies,(void*)no_existe);
			list_destroy(especies);
			return;
		}
		else{
			t_list* paquete = recibir_paquete(conexion_broker->localized);
			void display(void* valor){
				int id,id_correlacional;
				memcpy(&id,valor,sizeof(int));
				memcpy(&id_correlacional,valor+sizeof(int),sizeof(int));
				localized_pokemon* localized = deserializar_localized(valor+sizeof(int));
				log_info(logger,"Llega Un Mensaje Tipo: LOCALIZED_POKEMON: ID: %d ID_Correlacional: %d Pokemon: %s En Posicion:",id,id_correlacional,localized->name);
				void show(position* pos){
					log_info(logger,"[%d,%d]",pos->posx,pos->posy);
				}
				list_iterate(localized->posiciones,(void*)show);

				if(check_socket(conexion_broker->localized) == 1){
					enviar_ack(LOCALIZED_POKEMON,id,equipo->pid,conexion_broker->localized);
				}

				bool by_tipo_id(msg* m){
					return m->tipo_msg == GET_POKEMON && m->id_recibido == id_correlacional;
				}
				msg* mensaje = list_find(mensajes,(void*)by_tipo_id);


				if(mensaje != NULL){

					t_list* poks_localized = list_create();
					for(int i=0;i<localized->cantidad_posiciones;i++){
						pokemon* pok = crear_pokemon(localized->name);
						position* aux = list_get(localized->posiciones,i);
						set_pokemon(pok,aux->posx,aux->posy);
						pok->requerido = 0;
						bool existe_appeared(msg* aux){
							return aux->tipo_msg == APPEARED_POKEMON && string_equals_ignore_case(aux->pok->name,pok->name)
							&& aux->pok->posx == pok->posx && aux->pok->posy == pok->posy;
						}
						if(!list_any_satisfy(mensajes,(void*)existe_appeared)){
							list_add(poks_localized,pok);
							msg* loc = crear_mensaje(id,LOCALIZED_POKEMON,pok);
							list_add(mensajes,loc);
						}
					}

					double distance(entrenador* entre){
						pokemon* elegido;
						bool mas_cerca_pok(pokemon* aux1,pokemon* aux2){
							return distancia_pokemon(entre,aux1) < distancia_pokemon(entre,aux2);
						}
						list_sort(poks_localized,(void*)mas_cerca_pok);
						elegido = list_get(poks_localized,0);
						return distancia_pokemon(entre,elegido);
					}
					bool mas_cercano(entrenador* e1,entrenador* e2){
						return distance(e1) < distance(e2);
					}
					bool by_estado(entrenador* aux){
						return (aux->estado == NEW || aux->estado == BLOCKED)
								&& !verificar_deadlock_entrenador(aux)
								&& !verificar_espera_caught(aux);
					}

					int cant = cant_requerida_pokemon(localized->name);
					t_list* entrenadores = list_filter(equipo->entrenadores,(void*)by_estado);
					t_list* cercanos = list_sorted(entrenadores,(void*)mas_cercano);

					int min(int x,int y){
						if(x<y) return x;
						return y;
					}

					int cantidad = min(cant,localized->cantidad_posiciones);
					for(int i=0;i<cantidad;i++){
						entrenador* e = list_get(cercanos,i);
						pokemon* pok;
						if(e != NULL){
							bool mas_cerca_pok(pokemon* aux1,pokemon* aux2){
								return distancia_pokemon(e,aux1) < distancia_pokemon(e,aux2);
							}
							list_sort(poks_localized,(void*)mas_cerca_pok);
							pok = list_remove(poks_localized,0);
						}
						else{
							pok = list_remove(poks_localized,0);
						}
						pthread_rwlock_wrlock(&lockPoksRequeridos);
						list_add(equipo->poks_requeridos,pok);
						printf("%s En Posicion:[%d,%d] Es Requerido, Agregado A La Lista De Pokemones Requeridos\n",pok->name,pok->posx,pok->posy);
						pthread_rwlock_unlock(&lockPoksRequeridos);
						algoritmo_largo_plazo(pok);
					}
					list_remove_by_condition(mensajes,(void*)by_tipo_id);
					free(mensaje);
					list_destroy(cercanos);
					list_destroy(entrenadores);
					list_destroy(poks_localized);
				}
				else{
					printf("\033[0;31m%s No Es Requerido\033[0m\n",localized->name);
					free(localized->name);
				}
				free(valor);
				void limpiar(position* pos){
					free(pos);
				}
				list_destroy_and_destroy_elements(localized->posiciones,(void*)limpiar);
				free(localized);
			}

			if(!list_is_empty(paquete)){
				list_iterate(paquete,(void*)display);
			}
			list_destroy(paquete);
		}
	}
}

void recibir_caught_pokemon(){
	while(1){
		int cod_op;
		if(recv(conexion_broker->caught, &cod_op, sizeof(int), MSG_WAITALL) == -1)
			cod_op = -1;
		if(cod_op <= 0 || check_socket(conexion_broker->caught) != 1){
			conexion_broker->caught = -1;
			reintento_conectar_broker();

			void atrapar_default(entrenador* aux){
				void atrapado(pokemon* pok){
					bool by_id(entrenador* e){
						return e->tid == aux->tid;
					}
					log_warning(logger,"DEFAULT: Entrenador%c Atrapa Pokemon %s\n",aux->tid,pok->name);
					aux->pok_atrapar = NULL;
					list_add(aux->pokemones,pok);
					sumar_ciclos(aux,CICLO_ACCION);
					bool es_caught(pokemon* p){
						return p->name == pok->name;
					}
					list_remove_by_condition(aux->espera_caught,(void*)es_caught);
					bool existe_localized_appeared(msg* m){
						return (m->tipo_msg == LOCALIZED_POKEMON || m->tipo_msg == APPEARED_POKEMON) && string_equals_ignore_case(m->pok->name,pok->name)
								&& m->pok->posx == pok->posx && m->pok->posy == pok->posy;
					}
					msg* borrar = list_remove_by_condition(mensajes,(void*)existe_localized_appeared);
					if(borrar != NULL){
						free(borrar);
					}
					if(cumplir_objetivo_entrenador(aux)){
						salir_entrenador(aux);
						sem_post(&semExecTeam);
						detectar_deadlock();
					}
					else if(!cumplir_objetivo_entrenador(aux) && !verificar_cantidad_pokemones(aux)){
						log_warning(logger,"Entrenador%c BLOCKED En Espera De Solucionar Deadlock!",aux->tid);
						if(!list_any_satisfy(equipo->cola_deadlock,(void*)by_id)){
							list_add(equipo->cola_deadlock,aux);
							printf("Entrenador%c Agregado A La Cola De Deadlocks\n",aux->tid);
						}
						if(!list_any_satisfy(equipo->metrica_deadlock,(void*)by_id)){
							list_add(equipo->metrica_deadlock,aux);
						}
						bloquear_entrenador(aux);
						detectar_deadlock();
						sem_post(&semPoks);
					}
					else if(!cumplir_objetivo_entrenador(aux)){
						log_warning(logger,"Entrenador%c BLOCKED Finaliza Su Recorrido!",aux->tid);
						bloquear_entrenador(aux);
						replanificar_entrenador(aux);
					}
				}
				list_iterate(aux->espera_caught,(void*)atrapado);
			}

			t_list* esperas = list_filter(equipo->entrenadores,(void*)verificar_espera_caught);
			if(!list_is_empty(esperas)){
				list_iterate(esperas,(void*)atrapar_default);
				list_destroy(esperas);
			}
			else{
				list_destroy(esperas);
			}
		}
		else{
			t_list* paquete = recibir_paquete(conexion_broker->caught);
			void display(void* valor){
				int id,id_correlacional,res;
				memcpy(&id,valor,sizeof(int));
				memcpy(&id_correlacional,valor+sizeof(int),sizeof(int));
				memcpy(&res,valor+sizeof(int)*2,sizeof(int));
				log_info(logger,"Llega Un Mensaje Tipo: CAUGHT_POKEMON ID:%d ID_Correlacional: %d Resultado:%d ",id,id_correlacional,res);

				if(check_socket(conexion_broker->localized) == 1){
					enviar_ack(CAUGHT_POKEMON,id,equipo->pid,conexion_broker->caught);
				}

				bool by_tipo_id(msg* aux){
					return aux->tipo_msg == CATCH_POKEMON && aux->id_recibido == id_correlacional;
				}
				msg* mensaje = list_remove_by_condition(mensajes,(void*)by_tipo_id);
				bool espera_caught(entrenador* aux){
					bool by_pokemon(pokemon* pok){
						return pok->espera_caught == 1 && string_equals_ignore_case(pok->name,mensaje->pok->name);
					}
					return list_any_satisfy(aux->espera_caught,(void*)by_pokemon);
				}

				if(mensaje != NULL){
					entrenador* entre = list_find(equipo->entrenadores,(void*)espera_caught);
					bool es_caught(pokemon* aux){
						return aux->name == mensaje->pok->name;
					}
					list_remove_by_condition(entre->espera_caught,(void*)es_caught);
					list_remove_by_condition(mensajes,(void*)by_tipo_id);

					bool existe_localized_appeared(msg* m){
						return (m->tipo_msg == LOCALIZED_POKEMON || m->tipo_msg == APPEARED_POKEMON) && string_equals_ignore_case(m->pok->name,mensaje->pok->name)
								&& m->pok->posx == mensaje->pok->posx && m->pok->posy == mensaje->pok->posy;
					}
					msg* borrar = list_remove_by_condition(mensajes,(void*)existe_localized_appeared);
					if(borrar != NULL){
						free(borrar);
					}

					if(res == 1){
						list_add(entre->pokemones,mensaje->pok);
						log_info(logger,"Entrenador%c Atrapa Pokemon %s En Posicion:[%d,%d]",entre->tid,mensaje->pok->name,mensaje->pok->posx,mensaje->pok->posy);
						entre->pok_atrapar = NULL;
						sumar_ciclos(entre,CICLO_ACCION);
					}
					else{
						printf("Entrenador%c NO Pudo Atrapar El Pokemon %s En Posicion:[%d,%d]\n",entre->tid,mensaje->pok->name,mensaje->pok->posx,mensaje->pok->posy);
						entre->pok_atrapar = NULL;				bool existe_localized_appeared(msg* m){
							return (m->tipo_msg == LOCALIZED_POKEMON || m->tipo_msg == APPEARED_POKEMON) && string_equals_ignore_case(m->pok->name,mensaje->pok->name)
									&& m->pok->posx == mensaje->pok->posx && m->pok->posy == mensaje->pok->posy;
						}
						msg* borrar = list_remove_by_condition(mensajes,(void*)existe_localized_appeared);
						if(borrar != NULL){
							free(borrar);
						}
						free(mensaje->pok->name);
						free(mensaje->pok);
					}

					bool by_id(entrenador* aux){
						return aux->tid == entre->tid;
					}
					if(cumplir_objetivo_entrenador(entre)){
						salir_entrenador(entre);
						sem_post(&semExecTeam);
						detectar_deadlock();
					}
					else if(!cumplir_objetivo_entrenador(entre) && !verificar_cantidad_pokemones(entre)){
						log_warning(logger,"Entrenador%c BLOCKED En Espera De Solucionar Deadlock!",entre->tid);
						if(!list_any_satisfy(equipo->cola_deadlock,(void*)by_id)){
							list_add(equipo->cola_deadlock,entre);
							printf("Entrenador%c Agregado A La Cola De Deadlocks\n",entre->tid);
						}
						if(!list_any_satisfy(equipo->metrica_deadlock,(void*)by_id)){
							list_add(equipo->metrica_deadlock,entre);
						}
						bloquear_entrenador(entre);
						detectar_deadlock();
						sem_post(&semPoks);
					}
					else if(!cumplir_objetivo_entrenador(entre)){
						log_warning(logger,"Entrenador%c BLOCKED Finaliza Su Recorrido!",entre->tid);
						bloquear_entrenador(entre);
						replanificar_entrenador(entre);
					}
				}
				free(mensaje);
				free(valor);
			}
			if(!list_is_empty(paquete)){
				list_iterate(paquete,(void*)display);
			}
			list_destroy(paquete);

		}
	}
}

void recibir_appeared_pokemon(){
	while(1){
		sem_wait(&recibir_appeared);
		int cod_op;
		if(recv(conexion_broker->appeared, &cod_op, sizeof(int), MSG_WAITALL) == -1)
			cod_op = -1;
		if(cod_op <= 0 || check_socket(conexion_broker->appeared) != 1){
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
				appeared_pokemon* appeared_pokemon = deserializar_appeared(valor+sizeof(int)*2);
				log_info(logger,"Llega Un Mensaje Tipo: APPEARED_POKEMON: ID: %d ID_Correlacional: %d Pokemon: %s  Pos X:%d  Pos Y:%d",id,id_correlacional,appeared_pokemon->name,appeared_pokemon->pos.posx,appeared_pokemon->pos.posy);

				if(check_socket(conexion_broker->localized) == 1){
					enviar_ack(APPEARED_POKEMON,id,equipo->pid,conexion_broker->appeared);
				}

				bool existe_localized(msg* aux){
					return aux->tipo_msg == LOCALIZED_POKEMON && string_equals_ignore_case(aux->pok->name,appeared_pokemon->name)
					&& aux->pok->posx == appeared_pokemon->pos.posx && aux->pok->posy == appeared_pokemon->pos.posy;
				}

				if(!list_any_satisfy(mensajes,(void*)existe_localized)){
					if(requerir_atrapar(appeared_pokemon->name)){
						pokemon* pok = crear_pokemon(appeared_pokemon->name);
						set_pokemon(pok,appeared_pokemon->pos.posx,appeared_pokemon->pos.posy);
						pok->requerido = 0;
						msg* m = crear_mensaje(id,APPEARED_POKEMON,pok);
						list_add(mensajes,m);
						pthread_rwlock_wrlock(&lockPoksRequeridos);
						list_add(equipo->poks_requeridos,pok);
						printf("%s En Posicion:[%d,%d] Es Requerido, Agregado A La Lista De Pokemones Requeridos\n",pok->name,pok->posx,pok->posy);
						pthread_rwlock_unlock(&lockPoksRequeridos);
						algoritmo_largo_plazo(pok);
						if(equipo->exec == NULL){
							sem_post(&semExecTeam);
						}
					}
					else{
						printf("\033[0;31m%s No Es Requerido\033[0m\n",appeared_pokemon->name);
						free(appeared_pokemon->name);
					}
				}

				free(appeared_pokemon);
				free(valor);
			}
			if(!list_is_empty(paquete)){
				list_iterate(paquete,(void*)display);
			}
			list_destroy(paquete);
		}
		sem_post(&recibir_appeared);
	}
}

//BROKER---GAMEBOY--------------------------------------------------------------------------------------------------------------------------
void suscribirse_broker(){
	suscribirse_localized();
	if(conexion_broker->localized != -1){
		suscribirse_caught();
		suscribirse_appeared();
		enviar_mensajes_get_pokemon();
		sleep(3);
		sem_post(&recibir_appeared);
	}
	if(conexion_broker->localized == -1 || conexion_broker->appeared == -1 || conexion_broker->caught == -1){
		reintento_conectar_broker();
	}
}

void suscribirse_appeared(){
	conexion_broker->appeared = crear_conexion(datos_config->ip_broker,datos_config->puerto_broker);
	if(conexion_broker->appeared != -1){
		log_info(logger,"Conectado A Broker Suscribirse a APPEARED");
		enviar_info_suscripcion(APPEARED_POKEMON,conexion_broker->appeared,equipo->pid);
		recibir_confirmacion_suscripcion(conexion_broker->appeared,APPEARED_POKEMON);

		pthread_create(&suscripcion_appeared,NULL,(void*)recibir_appeared_pokemon,NULL);
		pthread_detach(suscripcion_appeared);
	}
	else{
		log_error(logger,"No Se Puede Conectarse A Broker");
	}
}

void suscribirse_localized(){
	conexion_broker->localized = crear_conexion(datos_config->ip_broker,datos_config->puerto_broker);
	if(conexion_broker->localized != -1){
		log_info(logger,"Conectado A Broker Suscribirse a LOCALIZED");
		enviar_info_suscripcion(LOCALIZED_POKEMON,conexion_broker->localized,equipo->pid);
		recibir_confirmacion_suscripcion(conexion_broker->localized,LOCALIZED_POKEMON);
		pthread_create(&suscripcion_localized,NULL,(void*)recibir_localized_pokemon,NULL);
		pthread_detach(suscripcion_localized);
	}
	else{
		log_error(logger,"No Se Puede Conectarse A Broker");
	}
}

void suscribirse_caught(){
	conexion_broker->caught = crear_conexion(datos_config->ip_broker,datos_config->puerto_broker);
	if(conexion_broker->localized != -1){
		log_info(logger,"Conectado A Broker Suscribirse a CAUGHT");
		enviar_info_suscripcion(CAUGHT_POKEMON,conexion_broker->caught,equipo->pid);
		recibir_confirmacion_suscripcion(conexion_broker->caught,CAUGHT_POKEMON);
		pthread_create(&suscripcion_caught,NULL,(void*)recibir_caught_pokemon,NULL);
		pthread_detach(suscripcion_caught);
	}
	else{
		log_error(logger,"No Se Puede Conectarse A Broker");
	}
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
		suscribirse_broker(equipo->pid);
		alarm(datos_config->tiempo_reconexion);
	}
}

void iniciar_servidor(void){
	printf("\033[1;33mEsperando Mensajes De Gameboy ...\033[0m\n");
	int socket_servidor;
    struct addrinfo hints, *servinfo, *p;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    getaddrinfo(IP, PUERTO, &hints, &servinfo);

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
	log_info(logger,"Gameboy Conectado Con Socket %d",*cliente_fd);

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
