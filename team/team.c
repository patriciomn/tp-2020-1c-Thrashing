#include "team.h"

int main(int argc,char* argv[]){
	iniciar_team(argv[1]);

	//para el test----------------------------------------------------------
		pokemon* Pikachu = crear_pokemon("Pikachu");
		set_pokemon(Pikachu,3,3);
		pokemon* Charmander = crear_pokemon("Charmander");
		set_pokemon(Charmander,5,3);
		pokemon* Squirtle = crear_pokemon("Squirtle");
		set_pokemon(Squirtle,5,10);
		//list_add(poks_requeridos,Charmander);
		list_add(poks_requeridos,Pikachu);
		//list_add(poks_requeridos,Squirtle);
		list_add(poks_requeridos,Charmander);
		//----------------------------------------------------------------------*/

	while(1){
		if(!detectar_deadlock()){
			pokemon_a_atrapar();
		}
		execution();
	}

	return 0;
}

void iniciar_team(char* teamConfig){
	iniciar_config(teamConfig);
	datos_config->log_file = config_get_string_value(config,"LOG_FILE");
	datos_config->equipo_name = config_get_string_value(config,"NAME");
	logger = log_create(datos_config->log_file,datos_config->equipo_name,1,LOG_LEVEL_INFO);
	datos_config->retardo_cpu = config_get_int_value(config,"RETARDO_CICLO_CPU");
	datos_config->ip_broker = config_get_string_value(config,"IP_BROKER");
	datos_config->puerto_broker = config_get_string_value(config,"PUERTO_BROKER");
	datos_config->tiempo_reconexion = config_get_int_value(config,"TIEMPO_RECONEXION");
	datos_config->algoritmo = config_get_string_value(config,"ALGORITMO_PLANIFICACION");
	datos_config->posiciones = config_get_string_value(config,"POSICIONES_ENTRENADORES");
	datos_config->pokemones = config_get_string_value(config,"POKEMON_ENTRENADORES");
	datos_config->objetivos = config_get_string_value(config,"OBJETIVOS_ENTRENADORES");
	poks_requeridos = list_create();
	sem_init(&semExec,0,1);
	sem_init(&semPoks,0,0);
	sem_init(&semHilo,0,1);

	conexion_broker = crear_conexion(datos_config->ip_broker,datos_config->puerto_broker);

	if(conexion_broker == -1){
		reintento_conectar_broker();
	}
	else{
		suscribirse_broker(datos_config->equipo_name,APPEARED_POKEMON);
		suscribirse_broker(datos_config->equipo_name,CAUGHT_POKEMON);
		suscribirse_broker(datos_config->equipo_name,LOCALIZED_POKEMON);
	}

	crear_team(datos_config->equipo_name);

	pthread_create(&servidor,NULL,(void*)iniciar_servidor,NULL);
}



//ENTRENADOR----TEAM-------------------------------------------------------------------------------------------------------------
void pokemon_a_atrapar(){
	if(list_is_empty(poks_requeridos)){
		log_error(logger,"NO HAY MAS POKEMONES A ATRAPAR");
		sem_wait(&semPoks);
	}
	pok_elegido = list_get(poks_requeridos,0);
}

void salir_grupo(){
	log_info(logger,"%s HA CUMPLIDO SU OBJETIVO!!!",equipo->name);
	if(conexion_broker != -1){
		liberar_conexion(conexion_broker);
	}

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
	list_destroy_and_destroy_elements(poks_requeridos,(void*)free_pok);
	free(equipo->name);
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

bool deadlock_entrenador(entrenador* entre){
	return (verificar_cantidad_pokemones(entre) != 1) && (!cumplir_objetivo_entrenador(entre));
}

bool detectar_deadlock(){
	return list_all_satisfy(equipo->entrenadores,(void*)deadlock_entrenador)
			&& list_all_satisfy(equipo->entrenadores,(void*)poseer_exceso_noNecesario)
			&& list_all_satisfy(equipo->entrenadores,(void*)espera_circular);
}

bool poseer_exceso_noNecesario(entrenador* entre){
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

bool espera_circular(entrenador* entre1){
	bool en_espera(void* ele2){
		entrenador* aux2 = ele2;
		pokemon* no1 = pokemon_no_necesario(entre1);
		pokemon* no2 = pokemon_no_necesario(aux2);
		return necesitar_pokemon(entre1,no2->name) && necesitar_pokemon(aux2,no1->name);
	}
	return list_any_satisfy(equipo->entrenadores,(void*)en_espera);
}

void crear_team(char* name){
	equipo = malloc(sizeof(team));
	equipo->name = name;
	equipo->entrenadores = list_create();
	equipo->objetivos = list_create();
	equipo->cant_ciclo = 0;

	log_info(logger,"%s START!",equipo->name);

	char** pos = string_split(datos_config->posiciones,"|");
	char** poks = string_split(datos_config->pokemones,"|");
	char** objs = string_split(datos_config->objetivos,"|");
	int j = 1;
	for(int i=0;i<cant_pokemones(poks);i++){
		char* p = pos[i];
		char** posicion = string_get_string_as_array(p);
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

	void get(void* element){
		pokemon* pok = element;
		get_pokemon(pok->name);
	}
	t_list* especies = especies_objetivo_team();
	list_iterate(especies,(void*)get);

	list_iterate(equipo->entrenadores,(void*)crear_hilo);

	list_destroy(especies);
	free(poks);
	free(objs);
	free(pos);
}

void crear_hilo(entrenador* entre){
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
	if(pthread_create(&entre->hilo,&attr,(void*)ejecutar_entrenador,entre) == 0){
		log_info(logger,"Hilo de %s create",entre->name);
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

	char** poks = string_get_string_as_array(pokemones);
	for(int i=0;i<cant_pokemones(poks);i++){
		char* pok = poks[i];
		pokemon* poke = crear_pokemon(pok);
		list_add(entre->pokemones,poke);
	}
	char** objs = string_get_string_as_array(objetivos);
	for(int i=0;i<cant_pokemones(objs);i++){
		char* obj = objs[i];
		list_add(entre->objetivos,crear_pokemon(obj));
		list_add(equipo->objetivos,crear_pokemon(obj));
	}

	list_add(equipo->entrenadores,entre);
	log_info(logger,"ENTRENADOR:%s EN POSICION:[%d,%d]",entre->name,entre->posx,entre->posy);
	log_warning(logger,"%s NEW",entre->name);

	free(poks);
	free(objs);
}


void ejecutar_entrenador(entrenador* entre){
	loop:
	while(entre->estado != EXEC){
		pthread_mutex_lock(&mut);
		pthread_cond_wait(&cond,&mut);
		pthread_mutex_unlock(&mut);
	}
	if(cumplir_objetivo_entrenador(entre)){
			log_info(logger,"%s HA CUMPLIDO SU OBJETIVO",entre->name);
			salir_entrenador(entre);
	}

	if(!detectar_deadlock()){
		move_entrenador(entre,pok_elegido->posx,pok_elegido->posy);
		catch_pokemon(entre,pok_elegido);
	}
	else{
		bool espera_deadlock(void* element){
			entrenador* aux = element;
			return aux->estado == BLOCKED && deadlock_entrenador(aux);
		}
		t_list* entrenadores = equipo->entrenadores;
		entrenador* entre2 = list_find(entrenadores,(void*)espera_deadlock);
		move_entrenador(entre,entre2->posx,entre2->posy);
		intercambiar_pokemon(entre,entre2);
		if(cumplir_objetivo_entrenador(entre2)){
			log_info(logger,"%s HA CUMPLIDO SU OBJETIVO",entre2->name);
			salir_entrenador(entre2);
		}
	}

	if(!cumplir_objetivo_entrenador(entre) && verificar_cantidad_pokemones(entre) != 1){
		log_error(logger,"%s EN ESPERA DE SOLUCIONAR DEADLOCK!",entre->name);
		bloquear_entrenador(entre);
		sem_post(&semExec);
		goto loop;
	}
	else{
		log_info(logger,"%s HA CUMPLIDO SU OBJETIVO",entre->name);
		salir_entrenador(entre);
		sem_post(&semExec);
	}
}

void execution(){
	sem_wait(&semExec);
	entrenador* entre;
	if(!detectar_deadlock() && !cumplir_objetivo_team()){
		entre = planificar_entrenador(pok_elegido);
	}
	else if(detectar_deadlock()){
		log_error(logger,"EN DEADLOCK!!!");
		entre = list_find(equipo->entrenadores,(void*)deadlock_entrenador);
		despertar_entrenador(entre);
	}
	else{
		salir_grupo();
	}
	if(cpu_libre()){
		activar_entrenador(entre);
	}
}

bool cpu_libre(){
	bool by_estado(void* ele){
		entrenador* aux = ele;
		return aux->estado == EXEC;
	}
	return !(list_any_satisfy(equipo->entrenadores,by_estado));
}

entrenador* planificar_entrenador(pokemon* pok){
	entrenador* entre = find_entrenador_cerca(pok);
	despertar_entrenador(entre);
	return entre;
}

void despertar_entrenador(entrenador* entre){
	if(entre->estado == NEW || entre->estado == BLOCKED || entre->estado == EXEC){
		entre->estado = READY;
		log_warning(logger,"%s READY",entre->name);
	}
	else if(entre->estado == EXIT){
		log_error(logger,"OPERACION NO PERMITIDA!");
	}
	else{
		log_info(logger,"%s ALREADY READY",entre->name);
	}
}

void activar_entrenador(entrenador* entre){
	if(entre->estado == READY){
		entre->estado = EXEC;
		log_warning(logger,"%s EXEC",entre->name);
		pthread_cond_signal(&cond);
	}
	else if(entre->estado == NEW || entre->estado == BLOCKED || entre->estado == EXIT){
		log_error(logger,"OPERACION NO PERMITIDA!");
	}
	else{
		log_info(logger,"%s ALREADY RUN",entre->name);
	}
}

void bloquear_entrenador(entrenador* entre){
	if(entre->estado == EXEC){
		entre->estado = BLOCKED;
		log_warning(logger,"%s BLOCKED",entre->name);
		//sem_post(&semExec);
	}
	else if(entre->estado == NEW || entre->estado == READY || entre->estado == EXIT){
		log_error(logger,"OPERACION NO PERMITIDA!");
	}
	else{
		log_info(logger,"%s ALREADY BLOCKED",entre->name);
		sleep(10);
	}
}

void salir_entrenador(entrenador* entre){
	if(entre->estado == EXEC || entre->estado == BLOCKED){
		entre->estado = EXIT;
		log_warning(logger,"%s EXIT",entre->name);
	}
	else if(entre->estado == NEW || entre->estado == READY){
		log_error(logger,"OPERACION NO PERMITIDA!");
	}
	else{
		log_info(logger,"%s ALREADY EXIT",entre->name);
	}
}

float distancia_pokemon(entrenador* entrenador,pokemon* pok){
	float x = abs(entrenador->posx-pok->posx);
	float y = abs(entrenador->posy-pok->posy);
	float dis = sqrt((x*x)+(y*y));
	return dis;
}


void move_entrenador(entrenador* entre,int posx,int posy){
	int ciclos = abs(entre->posx-posx)+abs(entre->posy-posy);
	sumar_ciclos(entre,ciclos);
	for(int i=0;i<entre->cant_ciclo;i++){
		printf("%s moviendose ...\n",entre->name);
		sleep(datos_config->retardo_cpu);
	}
	entre->posx = posx;
	entre->posy = posy;
	log_info(logger,"%s EN POSICION:[%d,%d]",entre->name,entre->posx,entre->posy);
}

void catch_pokemon(entrenador* entre,pokemon* pok){
	printf("%s atrapando a %s ...\n",entre->name,pok->name);
	sleep(datos_config->retardo_cpu);
	if(conexion_broker != -1){
		for(int i=0;i<datos_config->retardo_cpu;i++){
			printf("%s enviando mensaje a BROKER ...\n",equipo->name);
			sleep(1);
		}
		enviar_mensaje_catch_pokemon(pok->name,conexion_broker);
		char* id = recibir_id_mensaje(conexion_broker);
		list_add(id_mensajes,id);
		log_info(logger,"ID_MENSAJE:%s",recibir_id_mensaje(conexion_broker));
		//espera caught_pokemon
	}
	list_add(entre->pokemones,pok);
	sumar_ciclos(entre,1);
	log_info(logger,"%s HA ATRAPADO POKEMON %s",entre->name,pok->name);
	remove_pokemon_map(pok);
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
	for(int i=0;i<5;i++){
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
	list_add(entre1->pokemones,pok2);
	list_add(entre2->pokemones,pok1);
	list_remove_by_condition(entre1->pokemones,(void*)by_name1);
	list_remove_by_condition(entre2->pokemones,(void*)by_name2);
	log_info(logger,"%s DEL %s HA INTERCAMBIADO POR %s DEL %s",pok1->name,entre1->name,pok2->name,entre2->name);
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
	bool by_name(void* element){
		pokemon* aux = element;
		return (strcmp(aux->name,name) == 0);
	}
	return list_count_satisfying(equipo->objetivos,(void*)by_name);
}

int cant_especie_objetivo(entrenador* entre,char* name){
	bool by_name(void* element){
		pokemon* aux = element;
		return (strcmp(aux->name,name) == 0);
	}
	return list_count_satisfying(entre->objetivos,(void*)by_name);
}

int cant_especie_pokemon(entrenador* entre,char* name){
	bool by_name(void* element){
		pokemon* aux = element;
		return (strcmp(aux->name,name) == 0);
	}
	return list_count_satisfying(entre->pokemones,(void*)by_name);
}

entrenador* find_entrenador_cerca(pokemon* pok){
	bool by_estado(void* element1){
		entrenador* aux = element1;
		return (aux->estado == NEW || aux->estado == BLOCKED) && (!deadlock_entrenador(aux));
	}
	bool by_distancia(void* element1, void* element2){
		entrenador* aux = element1;
		entrenador* next = element2;
		return (distancia_pokemon(aux,pok)) < (distancia_pokemon(next,pok));
	}
	t_list* entrenadores = list_filter(equipo->entrenadores,(void*)by_estado);

	list_sort(entrenadores,(void*)by_distancia);

	entrenador* elegido = list_find(entrenadores,(void*)by_estado);
	list_destroy(entrenadores);
	return elegido;
}

int verificar_cantidad_pokemones(entrenador* entre){
	int permiso = 0;
	if(list_size(entre->pokemones) < list_size(entre->objetivos)){
		permiso = 1;
	}
	return permiso;
}


entrenador* find_entrenador_name(char* name){
	bool by_name(void* element){
		entrenador* aux = element;
		return string_equals_ignore_case(aux->name,name);
	}
	return list_find(equipo->entrenadores,(void*)by_name);
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
	pthread_mutex_lock(&mut);
	entrenador* entre = list_get(equipo->entrenadores,contador);
	while(entre){
		cant_especie += cant_especie_pokemon(entre,pok);
		contador++;
		entre = list_get(equipo->entrenadores,contador);
	}
	pthread_mutex_unlock(&mut);

	if(cant_team != 0 && cant_especie < cant_team){
		requiere = 1;
	}
	return requiere;
}

//POKEMON-----------------------------------------------------------------------------------------------------

int cant_pokemones(char** poks){
	int cant = 0;
	char* pok = poks[0];
	while(pok){
		cant++;
		pok = poks[cant];
	}
	return cant;
}

void remove_pokemon_map(pokemon* pok){
	bool by_name_pos(void* element){
		pokemon* aux = element;
		return ((strcmp(aux->name,pok->name)==0) && (aux->posx==pok->posx) && (aux->posy==pok->posy));
	}
	list_remove_by_condition(poks_requeridos,(void*)by_name_pos);
	log_info(logger,"POKEMON %s ELIMINADO DEL MAPA",pok->name);
}

pokemon* crear_pokemon(char* name){
	pokemon* pok = malloc(sizeof(pokemon));
	pok->name = name;
	return pok;
}

void set_pokemon(pokemon* pok,int posx,int posy){
	pok->posx = posx;
	pok->posy = posy;
	log_info(logger,"POKEMON:%s EN POSICION:[%d,%d]",pok->name,pok->posx,pok->posy);
}

pokemon* find_pokemon_map(char* name){
	bool by_name(void* element){
		pokemon* aux = element;
		return (strcmp(aux->name,name)==0);
	}
		return list_find(poks_requeridos,(void*)by_name);
}

//MENSAJES------------------------------------------------------------------------------------------------------------------------
void get_pokemon(char* pokemon){
	//funcion default
	if(conexion_broker == -1){
		log_error(logger,"DEFAULT:%s NO EXISTE",pokemon);
	}
	else{
		enviar_mensaje_get_pokemon(pokemon,conexion_broker);
		equipo->cant_ciclo += 1;
		char* id = recibir_id_mensaje(conexion_broker);
		list_add(id_mensajes,id);
		log_info(logger,"ID_MENSAJE:%s",id);
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

	if(requerir_atrapar(pokemon_name)){
		pokemon* pok = crear_pokemon(pokemon_name);
		set_pokemon(pok,posx,posy);
		pthread_mutex_lock(&mut);
		list_add(poks_requeridos,pok);
		pthread_mutex_unlock(&mut);
		sem_post(&semPoks);
		log_info(logger,"APPEARED_POKEMON: %s  POS X:%d  POS Y:%d\n",pok->name,pok->posx,pok->posy);
	}
	else{
		log_error(logger,"%s NO REQUERIDO",pokemon_name);
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

void caught_pokemon(int cliente_fd){
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

void enviar_mensaje_get_pokemon(char* pokemon,int socket_cliente){
	t_paquete* paquete = malloc(sizeof(t_paquete));
	int tam = strlen(pokemon) + 1;

	paquete->codigo_operacion = GET_POKEMON;
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = tam;
	paquete->buffer->stream = malloc(paquete->buffer->size);
	memcpy(paquete->buffer->stream, pokemon, tam);

	int bytes = paquete->buffer->size + 2*sizeof(int);

	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, 0);

	log_info(logger,"GET_POKEMON:%s",pokemon);

	free(a_enviar);
	free(paquete->buffer->stream);
	free(paquete->buffer);
	free(paquete);
}

void enviar_mensaje_catch_pokemon(char* pokemon,int socket_cliente){
	equipo->cant_ciclo += 1;
	t_paquete* paquete = malloc(sizeof(t_paquete));
	int tam = strlen(pokemon) + 1;

	paquete->codigo_operacion = CATCH_POKEMON;
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = tam;
	paquete->buffer->stream = malloc(paquete->buffer->size);
	memcpy(paquete->buffer->stream, pokemon, tam);

	int bytes = paquete->buffer->size + 2*sizeof(int);

	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, 0);

	log_info(logger,"CATCH_POKEMON:%s",pokemon);

	free(a_enviar);
	free(paquete->buffer->stream);
	free(paquete->buffer);
	free(paquete);
}



//BROKER---GAMEBOY--------------------------------------------------------------------------------------------------------------------------

void iniciar_config(char* teamConfig){
	config = leer_config(teamConfig);
	datos_config = malloc(sizeof(config_team));
}

void suscribirse_broker(char* name,int tipo){
	enviar_info_suscripcion(name,tipo,conexion_broker);
	recibir_confirmacion_suscripcion(conexion_broker);
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
		log_error(logger,"NO SE PUEDE CONECTARSE A BROKER");
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
	if(conexion_broker == -1){
		log_error(logger,"CONECTARSE A BROKER ...");
		conexion_broker = crear_conexion(datos_config->ip_broker,datos_config->puerto_broker);
		if(conexion_broker != -1){
			suscribirse_broker(datos_config->equipo_name,APPEARED_POKEMON);
			suscribirse_broker(datos_config->equipo_name,CAUGHT_POKEMON);
			suscribirse_broker(datos_config->equipo_name,LOCALIZED_POKEMON);
		}
		else{
			alarm(datos_config->tiempo_reconexion);
		}
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

void serve_client(int* socket){
	int cod_op;
	if(recv(*socket, &cod_op, sizeof(int), MSG_WAITALL) == -1)
		cod_op = -1;
	process_request(cod_op, *socket);
}

void process_request(int cod_op, int cliente_fd) {
	switch (cod_op) {
		case APPEARED_POKEMON:

			break;
		case 0:
			pthread_exit(NULL);
		case -1:
			pthread_exit(NULL);
	}
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

void devolver_mensaje(void* payload, int size, int socket_cliente){
	t_paquete* paquete = malloc(sizeof(t_paquete));

	paquete->codigo_operacion = APPEARED_POKEMON;
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = size;
	paquete->buffer->stream = malloc(paquete->buffer->size);
	memcpy(paquete->buffer->stream, payload, paquete->buffer->size);

	int bytes = paquete->buffer->size + 2*sizeof(int);

	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, 0);

	free(a_enviar);
	free(paquete->buffer->stream);
	free(paquete->buffer);
	free(paquete);
}

t_config* leer_config(char* config){
	return config_create(config);
}

void enviar_info_suscripcion(char* process,int tipo,int socket_cliente){
	t_paquete* paquete = malloc(sizeof(t_paquete));
	int tam = strlen(process) + 1;

	paquete->codigo_operacion = TEAM;
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = tam + sizeof(int);
	paquete->buffer->stream = malloc(paquete->buffer->size);
	memcpy(paquete->buffer->stream, process, tam);
	memcpy(paquete->buffer->stream+tam, &tipo, sizeof(int));

	int bytes = paquete->buffer->size + 2*sizeof(int);

	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, 0);

	free(a_enviar);
	free(paquete->buffer->stream);
	free(paquete->buffer);
	free(paquete);
}


void recibir_confirmacion_suscripcion(int cliente_fd){
	int cod_op;
	if(recv(cliente_fd, &cod_op, sizeof(int), MSG_WAITALL) == -1)
		cod_op = -1;

	int size;
	int desplazamiento = 0;
	char* confirmacion = recibir_buffer(cliente_fd, &size);

	log_info(logger,"%s",confirmacion);
	free(confirmacion);
}

char* recibir_id_mensaje(int cliente_fd){
	int cod_op;
	if(recv(cliente_fd, &cod_op, sizeof(int), MSG_WAITALL) == -1)
		cod_op = -1;

	int size;
	int desplazamiento = 0;
	char* id_mensaje = recibir_buffer(cliente_fd, &size);

	return id_mensaje;
}

void liberar_conexion(int socket_cliente){
	close(socket_cliente);
}

