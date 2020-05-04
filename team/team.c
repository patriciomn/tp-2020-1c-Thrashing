#include "team.h"

conexion* conexion_broker;
int pid;
t_log* logger;
t_config* config;
pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t list = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
team* equipo;
config_team* datos_config;
t_list* mensajes;
pthread_t servidor;
pthread_t suscripcion;
pthread_t caught;
pthread_t get;
sem_t semExecTeam;
sem_t semExecEntre;
sem_t semPoks;
sem_t semHilo;
sem_t semCaught;

int main(int argc,char* argv[]){
	iniciar_team(argv[1]);

	//para el test----------------------------------------------------------
		pokemon* Pikachu = crear_pokemon("Pikachu");
		set_pokemon(Pikachu,2,2);
		pokemon* Charmander = crear_pokemon("Charmander");
		set_pokemon(Charmander,3,6);
		pokemon* Squirtle = crear_pokemon("Squirtle");
		set_pokemon(Squirtle,5,10);
		list_add(equipo->poks_requeridos,Pikachu);
		list_add(equipo->poks_requeridos,Charmander);
		//list_add(poks_requeridos,Squirtle);
		//list_add(poks_requeridos,Charmander);
		//----------------------------------------------------------------------*/

	while(1){
		ejecutar_equipo();
	}
	pthread_join(suscripcion,NULL);
	pthread_join(servidor,NULL);
	return 0;
}

void iniciar_semaforos(){
	sem_init(&semExecTeam,0,1);
	sem_init(&semExecEntre,0,0);
	sem_init(&semPoks,0,0);
	sem_init(&semHilo,0,1);
}

void iniciar_team(char* teamConfig){
	iniciar_config(teamConfig);
	logger = log_create(datos_config->log_file,"team",1,LOG_LEVEL_INFO);

	conexion_broker = malloc(sizeof(conexion));
	mensajes = list_create();
	iniciar_semaforos();
	pid = getpid();
	crear_team(pid);
	pthread_create(&suscripcion,NULL,(void*)suscribirse_broker,NULL);
	get_pokemones();
	pthread_create(&servidor,NULL,(void*)iniciar_servidor,NULL);
	list_iterate(equipo->entrenadores,(void*)crear_hilo);
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
}

pokemon* pokemon_a_atrapar(){
	pokemon* pok_elegido;

	bool espera_caught(void* ele){
		pokemon* pok = ele;
		return pok->espera_caught == 1;
	}
	if(list_is_empty(equipo->poks_requeridos) || list_all_satisfy(equipo->poks_requeridos,(void*)espera_caught) ){
		printf("NO HAY POKEMONES A ATRAPAR\n");
		sem_wait(&semPoks);
	}
	pthread_mutex_lock(&list);
	bool disponible(void* ele){
		pokemon* aux = ele;
		return aux->espera_caught == 0;
	}
	pok_elegido = list_find(equipo->poks_requeridos,(void*)disponible);
	pthread_mutex_unlock(&list);
	return pok_elegido;
}

//CREACION---------------------------------------------------------------------------------------------------------------------
void crear_team(){
	equipo = malloc(sizeof(team));
	equipo->pid = pid;
	equipo->entrenadores = list_create();
	equipo->objetivos = list_create();
	equipo->poks_requeridos = list_create();
	equipo->cant_ciclo = 0;

	char** pos = string_get_string_as_array(datos_config->posiciones);
	char** poks = string_get_string_as_array(datos_config->pokemones);
	char** objs = string_get_string_as_array(datos_config->objetivos);
	int j = 1;
	for(int i=0;i<cant_pokemones(poks);i++){
		char** posicion = string_split(pos[i],"|");
		int posx = atoi(posicion[0]);
		int posy = atoi(posicion[1]);
		char* nro = string_itoa(j);
		char* nombre = string_from_format("Entrenador%s",nro);
		char* pok = poks[i];
		char* obj = objs[i];
		crear_entrenador(nombre,posx,posy,pok,obj);
		free(nro);
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

void get_pokemones(){
	void get(void* element){
		pokemon* pok = element;
		get_pokemon(pok->name);
	}
	t_list* especies = especies_objetivo_team();
	list_iterate(especies,(void*)get);
	list_destroy(especies);
}

void crear_hilo(entrenador* entre){
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
	if(pthread_create(&entre->hilo,&attr,(void*)ejecutar_entrenador,entre) == 0){
		printf("Hilo de %s create\n",entre->name);
	}
}

void crear_entrenador(char* name,int posx,int posy,char* pokemones,char* objetivos){
	entrenador* entre = malloc(sizeof(entrenador));
	entre->name = name;
	entre->posx = posx;
	entre->posy = posy;
	entre->pokemones = list_create();
	entre->objetivos = list_create();
	entre->estado = NEW;
	entre->cant_ciclo = 0;
	entre->tiempo = time(NULL);

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
	printf("ENTRENADOR:%s EN POSICION:[%d,%d]\n",entre->name,entre->posx,entre->posy);
}

void salir_equipo(){
	log_info(logger,"TEAM %d HA CUMPLIDO SU OBJETIVO!!!\n",equipo->pid);

	void free_pok(void* ele){
		pokemon* aux = ele;
		free(aux);
	}
	void free_entrenador(void* ele){
		entrenador* aux = ele;
		free(aux->name);
		list_destroy_and_destroy_elements(aux->pokemones,(void*)free_pok);
		list_destroy_and_destroy_elements(aux->objetivos,(void*)free_pok);
		free(aux);
	}

	list_destroy_and_destroy_elements(equipo->entrenadores,(void*)free_entrenador);
	list_destroy_and_destroy_elements(equipo->objetivos,(void*)free_pok);
	list_destroy_and_destroy_elements(equipo->poks_requeridos,(void*)free_pok);
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
void ejecutar_entrenador(entrenador* entre){
	inicio:
	if(entre->estado == BLOCKED){
		sem_post(&semExecTeam);
	}

	while(entre->estado != EXEC){
		sem_wait(&semExecEntre);
	}

	if(!verificar_deadlock_equipo()){
		pokemon* pok = pokemon_a_atrapar();
		move_entrenador(entre,pok->posx,pok->posy);
		int caso_default = catch_pokemon(entre,pok);

		if(caso_default != 1){
			pok->espera_caught = 1;
			pthread_create(&caught,NULL,(void*)caught_pokemon,&conexion_broker->caught);
			if(pok->espera_caught == 1){
				log_warning(logger,"%s BLOCKED EN ESPERA DE CAUGHT!",entre->name);
				bloquear_entrenador(entre);
				goto inicio;
			}
			else{
				sem_wait(&semCaught);
				list_add(entre->pokemones,pok);
				sumar_ciclos(entre,1);
				printf("%s HA ATRAPADO POKEMON %s\n",entre->name,pok->name);
				remove_pokemon_requeridos(pok);
			}
		}
	}
	else{
		bool entrenador_espera_deadlock(void* element){
			entrenador* aux = element;
			return aux->estado == BLOCKED && verificar_deadlock_entrenador(aux);
		}
		t_list* entrenadores = equipo->entrenadores;
		entrenador* entre2 = list_find(entrenadores,(void*)entrenador_espera_deadlock);
		move_entrenador(entre,entre2->posx,entre2->posy);
		intercambiar_pokemon(entre,entre2);
		if(cumplir_objetivo_entrenador(entre2)){
			printf("%s HA CUMPLIDO SU OBJETIVO\n",entre2->name);
			salir_entrenador(entre2);
		}
	}

	if(!cumplir_objetivo_entrenador(entre) && !verificar_cantidad_pokemones(entre)){
		log_warning(logger,"%s BLOCKED EN ESPERA DE SOLUCIONAR DEADLOCK!",entre->name);
		bloquear_entrenador(entre);
		goto inicio;
	}
	else if(!cumplir_objetivo_entrenador(entre)){
		log_warning(logger,"%s BLOCKED FINALIZADO SU RECORRIDO!",entre->name);
		bloquear_entrenador(entre);
		goto inicio;
	}
	else{
		printf("%s HA CUMPLIDO SU OBJETIVO\n",entre->name);
		salir_entrenador(entre);
		sem_post(&semExecTeam);
	}
}

void ejecutar_equipo(){
	sem_wait(&semExecTeam);
	entrenador* entre;
	pokemon* pok_elegido;
	if(!cumplir_objetivo_team()){
		if(!verificar_deadlock_equipo()){
			pok_elegido = pokemon_a_atrapar();
			entre = planificar_entrenador(pok_elegido);
		}
		else{
			log_warning(logger,"EN DEADLOCK!!!");
			entre = list_find(equipo->entrenadores,(void*)verificar_deadlock_entrenador);
			despertar_entrenador(entre);
		}

		if(verificar_cpu_libre()){
			activar_entrenador(entre);
		}
	}
	else{
		salir_equipo();
	}
}

bool verificar_cpu_libre(){
	bool by_estado(void* ele){
		entrenador* aux = ele;
		return aux->estado == EXEC;
	}
	return !(list_any_satisfy(equipo->entrenadores,by_estado));
}

void clean_entrenador(entrenador* entre){
	free(entre);
}

entrenador* planificar_entrenador(pokemon* pok){
	t_list* cola_ready = list_create();
	entrenador* entre;
	cola_ready = find_entrenadores_cerca(pok);
	if(string_equals_ignore_case(datos_config->algoritmo,"FIFO")){
		bool fifo(void* ele1,void* ele2){
			entrenador* aux1 = ele1;
			entrenador* aux2 = ele2;
			return aux1->tiempo < aux2->tiempo;
		}
		list_sort(cola_ready,(void*)fifo);
		entre = list_get(cola_ready,0);
		despertar_entrenador(entre);
	}
	list_destroy(cola_ready);
	return entre;
}

t_list* find_entrenadores_cerca(pokemon* pok){
	bool by_estado(void* element1){
		entrenador* aux = element1;
		return (aux->estado == NEW || aux->estado == BLOCKED) && (!verificar_deadlock_entrenador(aux));
	}
	bool by_distancia(void* element1, void* element2){
		entrenador* aux = element1;
		entrenador* next = element2;
		return (distancia_pokemon(aux,pok)) < (distancia_pokemon(next,pok));
	}
	t_list* entrenadores = list_filter(equipo->entrenadores,(void*)by_estado);
	list_sort(entrenadores,(void*)by_distancia);
	entrenador* elegido = list_find(entrenadores,(void*)by_estado);

	t_list* elegidos = list_create();

	void agregar(void* ele){
		entrenador* aux = ele;
		if(distancia_pokemon(aux,pok) == distancia_pokemon(elegido,pok)){
			list_add(elegidos,aux);
		}
	}
	list_iterate(entrenadores,(void*)agregar);
	list_destroy(entrenadores);
	return elegidos;
}

void despertar_entrenador(entrenador* entre){
	if(entre->estado == NEW || entre->estado == BLOCKED || entre->estado == EXEC){
		entre->estado = READY;
		log_info(logger,"%s READY A EJECUTAR",entre->name);
	}
	else if(entre->estado == EXIT){
		perror("OPERACION NO PERMITIDA!");
	}
	else{
		perror("%s ALREADY READY");
	}
}

void activar_entrenador(entrenador* entre){
	if(entre->estado == READY){
		entre->estado = EXEC;
		sem_post(&semExecEntre);
	}
	else if(entre->estado == NEW || entre->estado == BLOCKED || entre->estado == EXIT){
		perror("OPERACION NO PERMITIDA!");
	}
	else{
		perror("%s ALREADY RUN");
	}
}

void bloquear_entrenador(entrenador* entre){
	if(entre->estado == EXEC){
		entre->estado = BLOCKED;
	}
	else if(entre->estado == NEW || entre->estado == READY || entre->estado == EXIT){
		perror("OPERACION NO PERMITIDA!");
	}
	else{
		perror("ALREADY BLOCKED");
	}
}

void salir_entrenador(entrenador* entre){
	if(entre->estado == EXEC || entre->estado == BLOCKED){
		entre->estado = EXIT;
	}
	else if(entre->estado == NEW || entre->estado == READY){
		perror("OPERACION NO PERMITIDA!");
	}
	else{
		perror("%s ALREADY EXIT");
	}
}

float distancia_pokemon(entrenador* entrenador,pokemon* pok){
	float x = abs(entrenador->posx-pok->posx);
	float y = abs(entrenador->posy-pok->posy);
	float dis = sqrt((x*x)+(y*y));
	return dis;
}

void move_entrenador(entrenador* entre,int posx,int posy){
	pthread_mutex_lock(&mut);
	log_info(logger,"%s MOVIENDOSE A POSICION:[%d,%d]",entre->name,posx,posy);
	int ciclos = abs(entre->posx-posx)+abs(entre->posy-posy);
	sumar_ciclos(entre,ciclos);
	for(int i=0;i<entre->cant_ciclo;i++){
		printf("%s moviendose ...\n",entre->name);
		sleep(datos_config->retardo_cpu);
	}
	entre->posx = posx;
	entre->posy = posy;
	pthread_mutex_unlock(&mut);
}

int catch_pokemon(entrenador* entre,pokemon* pok){
	int caso_default = -1;
	pthread_mutex_lock(&mut);
	log_info(logger,"%s ATRAPA A %s",entre->name,pok->name);
	sleep(datos_config->retardo_cpu);

	conexion_broker->catch = crear_conexion(datos_config->ip_broker,datos_config->puerto_broker);
	if(conexion_broker->catch != -1){
		for(int i=0;i<CICLOS_ENVIAR;i++){
			printf("TEAM %d enviando mensaje a BROKER ...\n",equipo->pid);
			sleep(datos_config->retardo_cpu);
		}
		enviar_mensaje_catch_pokemon(pok->name,pok->posx,pok->posy,conexion_broker->catch);
		int id = recibir_id_mensaje(conexion_broker->catch);
		msg* mensaje = crear_mensaje(id,CATCH_POKEMON,pok);
		list_add(mensajes,mensaje);
		log_info(logger,"ID_MENSAJE:%d",id);
	}
	else{
		caso_default = 1;
		log_error(logger,"DEFAULT:%s HA ATRAPADO POKEMON %s",entre->name,pok->name);
		list_add(entre->pokemones,pok);
		sumar_ciclos(entre,1);
		remove_pokemon_requeridos(pok);
	}
	pthread_mutex_unlock(&mut);
	return caso_default;
}

void sumar_ciclos(entrenador* entre,int ciclos){
	entre->cant_ciclo += ciclos;
	equipo->cant_ciclo += ciclos;
}

pokemon* pokemon_a_intercambiar(entrenador* entre){
	t_list* no_necesarios;
	bool no_necesitar(void* ele){
		pokemon* aux = ele;
		return !necesitar_pokemon(entre,aux->name);
	}
	no_necesarios = list_filter(entre->pokemones,(void*)no_necesitar);
	pokemon* pok = list_get(no_necesarios,0);
	list_destroy(no_necesarios);
	return pok;
}

void intercambiar_pokemon(entrenador* entre1,entrenador* entre2){
	log_info(logger,"%s y %s INTERCAMBIAN POKEMONES",entre1->name,entre2->name);
	for(int i=0;i<CICLOS_INTERCAMBIAR;i++){
		printf("%s y %s intercambiando pokemones ...\n",entre1->name,entre2->name);
		sleep(datos_config->retardo_cpu);
	}
	pokemon* pok1 = pokemon_a_intercambiar(entre1);
	pokemon* pok2 = pokemon_a_intercambiar(entre2);
	bool by_name1(void* ele){
		pokemon* aux = ele;
		return aux->name = pok1->name;
	}
	bool by_name2(void* ele){
		pokemon* aux = ele;
		return aux->name = pok2->name;
	}
	pthread_mutex_lock(&mut);
	list_add(entre1->pokemones,pok2);
	list_add(entre2->pokemones,pok1);
	list_remove_by_condition(entre1->pokemones,(void*)by_name1);
	list_remove_by_condition(entre2->pokemones,(void*)by_name2);
	pthread_mutex_unlock(&mut);
}

bool cumplir_objetivo_team(){
	return list_all_satisfy(equipo->entrenadores,(void*)cumplir_objetivo_entrenador);
}

bool cumplir_objetivo_entrenador(entrenador* entre){
	bool by_especie(void* element){
		pokemon* aux = element;
		return contener_pokemon(entre,aux->name);
	}
	bool by_cantidad(void* element){
			pokemon* aux = element;
			return cant_especie_objetivo(entre,aux->name) == cant_especie_pokemon(entre,aux->name);
	}
	return list_all_satisfy(entre->objetivos,(void*)by_especie) && list_all_satisfy(entre->objetivos,(void*)by_cantidad) ;
}

bool contener_pokemon(entrenador* entre,char* name){
	bool by_name(void* element){
		pokemon* aux = element;
		return (strcmp(aux->name,name) == 0);
	}
	return list_any_satisfy(entre->pokemones,(void*)by_name);
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
	bool by_name(void* element){
		pokemon* aux = element;
		return (strcmp(aux->name,name) == 0);
	}
	pthread_mutex_lock(&mut);
	res = list_count_satisfying(equipo->objetivos,(void*)by_name);
	pthread_mutex_unlock(&mut);
	return res;
}

int cant_especie_objetivo(entrenador* entre,char* name){
	int res = 0;
	bool by_name(void* element){
		pokemon* aux = element;
		return (strcmp(aux->name,name) == 0);
	}
	pthread_mutex_lock(&mut);
	res = list_count_satisfying(entre->objetivos,(void*)by_name);
	pthread_mutex_unlock(&mut);
	return res;
}

int cant_especie_pokemon(entrenador* entre,char* name){
	int res = 0;
	bool by_name(void* element){
		pokemon* aux = element;
		return (strcmp(aux->name,name) == 0);
	}
	pthread_mutex_lock(&mut);
	res = list_count_satisfying(entre->pokemones,(void*)by_name);
	pthread_mutex_unlock(&mut);
	return res;
}

bool verificar_cantidad_pokemones(entrenador* entre){
	int permite = 0;
	pthread_mutex_lock(&mut);
	if(list_size(entre->pokemones) < list_size(entre->objetivos)){
		permite = 1;
	}
	pthread_mutex_unlock(&mut);
	return permite;
}

t_list* especies_objetivo_team(){
	bool repetido(void* element){
		pokemon* aux = element;
		return cant_especie_objetivo_team(aux->name) > 1;
	}
	t_list* objetivos = list_duplicate(equipo->objetivos);
	t_list* especies = list_create();
	for(int i=0;i<list_size(objetivos);i++){
		pokemon* obj = list_get(objetivos,i);
		bool by_name(void* ele){
			pokemon* aux = ele;
			return strcmp(aux->name,obj->name) == 0;
		}
		if(!list_any_satisfy(especies,by_name)){
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
	if(cant_team != 0 && cant_especie <= cant_team){
		requiere = 1;
	}
	return requiere;
}

//DEADLOCK------------------------------------------------------------------------------------------------------------
bool verificar_deadlock_entrenador(entrenador* entre){
	return (verificar_cantidad_pokemones(entre) != 1) && (!cumplir_objetivo_entrenador(entre));
}

bool verificar_deadlock_equipo(){
	return list_all_satisfy(equipo->entrenadores,(void*)verificar_deadlock_entrenador)
			&& list_all_satisfy(equipo->entrenadores,(void*)verificar_pokemon_exceso_noNecesario)
			&& list_all_satisfy(equipo->entrenadores,(void*)verificar_espera_circular);
}

bool verificar_pokemon_exceso_noNecesario(entrenador* entre){
	bool exceso(void* element){
		pokemon* aux = element;
		return cant_especie_pokemon(entre,aux->name) > cant_especie_objetivo(entre,aux->name);
	}
	bool noNecesario(void* element){
		pokemon* aux = element;
		return !necesitar_pokemon(entre,aux->name);
	}
	return list_any_satisfy(entre->pokemones,exceso) || list_any_satisfy(entre->pokemones,noNecesario);
}

pokemon* pokemon_no_necesario(entrenador* entre){
	bool no_necesario(void* ele){
		pokemon* aux = ele;
		return !necesitar_pokemon(entre,aux->name) || pokemon_exceso(entre,aux->name);
	}
	return list_find(entre->pokemones,(void*)no_necesario);
}

bool verificar_espera_circular(entrenador* entre1){
	bool en_espera(void* ele2){
		entrenador* aux2 = ele2;
		pokemon* no1 = pokemon_no_necesario(entre1);
		pokemon* no2 = pokemon_no_necesario(aux2);
		return necesitar_pokemon(entre1,no2->name) && necesitar_pokemon(aux2,no1->name);
	}
	return list_any_satisfy(equipo->entrenadores,(void*)en_espera);
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
	bool by_name_pos(void* element){
		pokemon* aux = element;
		return ((strcmp(aux->name,pok->name)==0) && (aux->posx==pok->posx) && (aux->posy==pok->posy));
	}
	list_remove_by_condition(equipo->poks_requeridos,(void*)by_name_pos);
	printf("POKEMON %s ELIMINADO DEL MAPA\n",pok->name);
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
	bool by_name(void* element){
		pokemon* aux = element;
		return (strcmp(aux->name,name)==0);
	}
		return list_find(equipo->poks_requeridos,(void*)by_name);
}

//MENSAJES------------------------------------------------------------------------------------------------------------------------
void get_pokemon(char* pokemon){
	conexion_broker->get = crear_conexion(datos_config->ip_broker,datos_config->puerto_broker);
	if(conexion_broker->get == -1){
		log_error(logger,"DEFAULT:%s NO EXISTE",pokemon);
	}
	else{
		enviar_mensaje_get_pokemon(pokemon,conexion_broker->get);
		equipo->cant_ciclo += 1;
		int id = recibir_id_mensaje(conexion_broker->get);
		msg* mensaje = crear_mensaje(id,GET_POKEMON,NULL);
		list_add(mensajes,mensaje);
		printf("TIPO_MENSAJE:%s ID_MENSAJE:%d\n","GET_POKEMON",id);
		liberar_conexion(conexion_broker->get);
	}
}

void appeared_pokemon(int cliente_fd){
	int size;
	int desplazamiento = 0;
	int posx,posy,cant;
	char* pokemon_name = recibir_buffer(cliente_fd, &size);
	desplazamiento += strlen(pokemon_name)+1;
	memcpy(&posx,pokemon_name+desplazamiento,sizeof(int));
	desplazamiento += sizeof(int);
	memcpy(&posy,pokemon_name+desplazamiento,sizeof(int));
	log_info(logger,"LLEGA UN MENSAJE TIPO:APPEARED_POKEMON :%s  POS X:%d  POS Y:%d\n",pokemon_name,posx,posy);

	if(requerir_atrapar(pokemon_name)){
		pokemon* pok = crear_pokemon(pokemon_name);
		set_pokemon(pok,posx,posy);
		pthread_mutex_lock(&list);
		list_add(equipo->poks_requeridos,pok);
		printf("%s ES REQUERIDO, AGREGADO A LA LIST DE POKEMONES REQUERIDOS\n",pok->name);
		pthread_mutex_unlock(&list);
		sem_post(&semPoks);
	}
	else{
		printf("%s NO REQUERIDO\n",pokemon_name);
	}
}

void localized_pokemon(int cliente_fd){
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

msg* find_mensaje(int tipo,int id){
	bool by_tipo_id(void* ele){
		msg* aux = ele;
		return aux->tipo_msg==tipo && aux->id_recibido==id;
	}
	return list_find(mensajes,(void*)by_tipo_id);
}

void caught_pokemon(int cliente_fd){
	int cod_op;
	if(recv(cliente_fd, &cod_op, sizeof(int), MSG_WAITALL) == -1)
		cod_op = -1;

	if(cod_op != -1){
		int size;
		int desplazamiento = 0;
		int id,resultado;
		char* pokemon = recibir_buffer(cliente_fd, &size);
		desplazamiento += strlen(pokemon)+1;
		memcpy(&id,pokemon+desplazamiento,sizeof(int));
		desplazamiento += sizeof(int);
		memcpy(&resultado,pokemon+desplazamiento,sizeof(int));
		printf("CAUGHT_POKEMON: %s  ID:%d  RESULTADO:%d\n",pokemon,id,resultado);
		msg* mensaje = find_mensaje(cod_op,id);
		if(mensaje && resultado==1){
			sem_post(&semCaught);
		}
	}
}

//BROKER---GAMEBOY--------------------------------------------------------------------------------------------------------------------------

int suscribirse_appeared(int pid){
	int res = conexion_broker->appeared = crear_conexion(datos_config->ip_broker,datos_config->puerto_broker);
	if(conexion_broker->appeared != -1){
			enviar_info_suscripcion(APPEARED_POKEMON,conexion_broker->appeared);
			recibir_confirmacion_suscripcion(conexion_broker->appeared);
	}
	return res;
}

int suscribirse_localized(int pid){
	int res = conexion_broker->localized = crear_conexion(datos_config->ip_broker,datos_config->puerto_broker);
	if(conexion_broker->localized != -1){
			enviar_info_suscripcion(LOCALIZED_POKEMON,conexion_broker->localized);
			recibir_confirmacion_suscripcion(conexion_broker->localized);
	}
	return res;
}

int suscribirse_caught(int pid){
	int res =	conexion_broker->caught = crear_conexion(datos_config->ip_broker,datos_config->puerto_broker);
	if(conexion_broker->localized != -1){
			enviar_info_suscripcion(CAUGHT_POKEMON,conexion_broker->caught);
			recibir_confirmacion_suscripcion(conexion_broker->caught);
	}
	return res;
}

void suscribirse_broker(int pid){
	suscribirse_appeared(pid);
	suscribirse_localized(pid);
	suscribirse_caught(pid);
	if(conexion_broker->appeared==-1&&conexion_broker->caught==-1&&conexion_broker->localized==-1){
		log_error(logger,"NO SE PUEDE CONECTARSE A BROKER");
		reintento_conectar_broker();
	}
	else{
		log_warning(logger,"FUE CONECTADO A BROKER");
	}
	return;
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
		log_error(logger,"RECONECTANDO A BROKER ...");
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
	log_info(logger,"GAMEBOY CONECTADO!",socket_cliente);

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

int recibir_valor(int conexion){
	int valor;
	void* buffer = malloc(sizeof(int));
	recv(conexion,buffer,sizeof(buffer),MSG_WAITALL);
	memcpy(&valor,buffer,sizeof(int));
	free(buffer);
	return valor;
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

t_config* leer_config(char* config){
	return config_create(config);
}

void enviar_info_suscripcion(int tipo,int socket_cliente){
	t_paquete* paquete = malloc(sizeof(t_paquete));
	paquete->codigo_operacion = SUSCRIPCION;
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
	int tam_pokemon = strlen(pokemon) + 1;
	int tipo = GET_POKEMON;

	t_paquete* paquete = malloc(sizeof(t_paquete));
	paquete->codigo_operacion = MENSAJE;
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = sizeof(int)*2 + tam_pokemon ;
	paquete->buffer->stream = malloc(paquete->buffer->size);
	memcpy(paquete->buffer->stream,&equipo->pid,sizeof(int));
	memcpy(paquete->buffer->stream+sizeof(int),&tipo,sizeof(int));
	memcpy(paquete->buffer->stream+sizeof(int)*2, pokemon, tam_pokemon);
	int bytes = paquete->buffer->size + sizeof(int)*2;

	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, 0);

	printf("GET_POKEMON:%s\n",pokemon);

	free(a_enviar);
	free(paquete->buffer->stream);
	free(paquete->buffer);
	free(paquete);
}

void enviar_mensaje_catch_pokemon(char* pokemon,int posx,int posy,int socket_cliente){
	equipo->cant_ciclo += 1;
	int tam_pokemon = strlen(pokemon) + 1;
	int tipo = CATCH_POKEMON;

	t_paquete* paquete = malloc(sizeof(t_paquete));
	paquete->codigo_operacion = MENSAJE;
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = sizeof(int)*4 + tam_pokemon ;
	paquete->buffer->stream = malloc(paquete->buffer->size);
	memcpy(paquete->buffer->stream,&equipo->pid,sizeof(int));
	memcpy(paquete->buffer->stream+sizeof(int),&tipo,sizeof(int));
	memcpy(paquete->buffer->stream+sizeof(int)*2, pokemon, tam_pokemon);
	memcpy(paquete->buffer->stream+sizeof(int)*2+tam_pokemon, &posx,sizeof(int));
	memcpy(paquete->buffer->stream+sizeof(int)*3+tam_pokemon, &posy,sizeof(int));

	int bytes = paquete->buffer->size + 2*sizeof(int);

	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, 0);

	log_info(logger,"CATCH_POKEMON:%s",pokemon);

	free(a_enviar);
	free(paquete->buffer->stream);
	free(paquete->buffer);
	free(paquete);
}

void recibir_confirmacion_suscripcion(int cliente_fd){
	int cod_op;
	if(recv(cliente_fd, &cod_op, sizeof(int), MSG_WAITALL) == -1)
		cod_op = -1;

	printf("SE HA SUSCRIPTO A LA COLA\n");
}

int recibir_id_mensaje(int cliente_fd){
	int id_mensaje = recibir_valor(cliente_fd);

	return id_mensaje;
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
