#include "team.h"

int status = STOP;
int main(int argc,char* argv[]){
	inicializar(argv[1]);

	//para el test----------------------------------------------------------
	pokemon* pikachu = crear_pokemon("pikachu");
	set_pokemon(pikachu,3,3);
	pokemon* CHU = crear_pokemon("CHU");
	set_pokemon(CHU,5,3);
	list_add(poks_requeridos,pikachu);
	list_add(poks_requeridos,CHU);
	//----------------------------------------------------------------------

	entrenador* entre;
	while(!cumplir_objetivo_team()){
		pok_elegido = aparecer_nuevo_pokemon();
		entre = planificar_entrenadores(pok_elegido);
		pthread_join(hilos_entrenador[entre->tid],NULL);
	}

	if(cumplir_objetivo_team()){
		salir_grupo();
	}

	return 0;
}

void inicializar(char* teamConfig){
	iniciar_config(teamConfig);
	char* log_file = config_get_string_value(config,"LOG_FILE");
	equipo_name = config_get_string_value(config,"NAME");
	logger = log_create(log_file,equipo_name,1,LOG_LEVEL_INFO);
	retardo_cpu = config_get_int_value(config,"RETARDO_CICLO_CPU");
	ip_broker = config_get_string_value(config,"IP_BROKER");
	puerto_broker = config_get_string_value(config,"PUERTO_BROKER");
	tiempo_reconexion = config_get_int_value(config,"TIEMPO_RECONEXION");
	algoritmo = config_get_string_value(config,"ALGORITMO_PLANIFICACION");

	poks_requeridos = list_create();
	hilo_counter = 0;

	conexion_broker = crear_conexion(ip_broker,puerto_broker);

	//reintento_conectar_broker con syscall signal y alarm, cada x segundos ejecuta una vez
	if(conexion_broker == -1){
		reintento_conectar_broker();
	}
	else{
		suscribirse_broker(equipo_name);
	}
	crear_team(equipo_name);

	pthread_create(&servidor,NULL,(void*)iniciar_servidor,NULL);
	pthread_detach(servidor);
}



//ENTRENADOR----TEAM-------------------------------------------------------------------------------------------------------------
pokemon* aparecer_nuevo_pokemon(){
	return list_get(poks_requeridos,0);
}

void salir_grupo(){
	if(conexion_broker != -1){
		liberar_conexion(conexion_broker);
	}

	void free_poks(void* ele){
		entrenador* aux = ele;
		list_destroy_and_destroy_elements(aux->pokemones,free);
		list_destroy_and_destroy_elements(aux->objetivos,free);
		free(aux);
	}

	list_destroy_and_destroy_elements(equipo->entrenadores,(void*)free_poks);
	list_destroy_and_destroy_elements(equipo->objetivos,free);
	list_destroy_and_destroy_elements(poks_requeridos,free);
	log_info(logger,"%s HA CUMPLIDO SU OBJETIVO!!!",equipo->name);
	free(equipo);
}

bool deadlock(){
	int res = 0;
	if(equipo_en_blocked()){
		res = 1;
		log_error(logger,"EN DEADLOCK!!!");
	}
	return res;
}

bool equipo_en_blocked(){
	bool blocked(void* ele){
		entrenador* aux = ele;
		return aux->estado == BLOCKED;
	}
	return list_all_satisfy(equipo->entrenadores,(void*)blocked);
}


void ejecutar_entrenador(entrenador* entre){
	while(!cumplir_objetivo_entrenador(entre)){
		pthread_mutex_lock(&mut);
		while(!status || entre->estado != EXEC){
			pthread_cond_wait(&cond,&mut);
		}
		pthread_mutex_unlock(&mut);

		if(!deadlock()){
			moverse_entrenador(entre,pok_elegido->posx,pok_elegido->posy);
			catch_pokemon(pok_elegido);
			atrapar_pokemon(entre,pok_elegido);
		}
		else{
			bool by_name(void* element){
				entrenador* aux = element;
				return aux->name!=entre->name;
			}
			bool es_blocked(void* element){
				entrenador* aux = element;
				return aux->estado == BLOCKED;
			}
			t_list* entrenadores = equipo->entrenadores;
			t_list* interbloqueados = list_filter(entrenadores,(void*)es_blocked);
			entrenador* entre2 = list_find(interbloqueados,(void*)by_name);
			moverse_entrenador(entre,entre2->posx,entre2->posy);
			intercambiar_pokemon(entre,entre2);
			if(cumplir_objetivo_entrenador(entre2)){
				log_info(logger,"%s HA CUMPLIDO SU OBJETIVO",entre2->name);
				salir_entrenador(entre2);
			}
		}
	}

	if(cumplir_objetivo_entrenador(entre)){
		log_info(logger,"%s HA CUMPLIDO SU OBJETIVO",entre->name);
		salir_entrenador(entre);
	}
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
		return strcmp(aux->name,name) == 0;
	}
	return list_any_satisfy(entre->pokemones,(void*)by_name);
}

bool necesitar_pokemon(entrenador* entre,char* name){
	bool by_name(void* element){
		pokemon* aux = element;
		return strcmp(aux->name,name) == 0;
	}
	return list_any_satisfy(entre->objetivos,(void*)by_name);
}

int cant_especie_objetivo_team(char* name){
	bool by_name(void* element){
		pokemon* aux = element;
		return strcmp(aux->name,name) == 0;
	}
	return list_count_satisfying(equipo->objetivos,(void*)by_name);
}

int cant_especie_objetivo(entrenador* entre,char* name){
	bool by_name(void* element){
		pokemon* aux = element;
		return strcmp(aux->name,name) == 0;
	}
	return list_count_satisfying(entre->objetivos,(void*)by_name);
}

int cant_especie_pokemon(entrenador* entre,char* name){
	bool by_name(void* element){
		pokemon* aux = element;
		return strcmp(aux->name,name) == 0;
	}
	return list_count_satisfying(entre->pokemones,(void*)by_name);
}

entrenador* find_entrenador_cerca(pokemon* pok){
	bool by_estado(void* element1){
		entrenador* aux = element1;
		return (aux->estado == NEW || aux->estado == BLOCKED) && verificar_cantidad_pokemones(aux) == 1;
	}
	bool by_distancia(void* element1, void* element2){
		entrenador* aux = element1;
		entrenador* next = element2;
		return (distancia_pokemon(aux,pok)) < (distancia_pokemon(next,pok));
	}
	t_list* entrenadores = list_filter(equipo->entrenadores,(void*)by_estado);

	list_sort(entrenadores,(void*)by_distancia);

	entrenador* elegido = list_find(entrenadores,(void*)by_estado);

	return elegido;
}

entrenador* planificar_entrenadores(pokemon* pok){
	entrenador* elegido;
	if(string_equals_ignore_case(algoritmo,"FIFO")){
		log_info(logger,"ALGORITMO FIFO");
		t_list* libres;
		if(!deadlock()){
			bool by_estado(void* element1){
				entrenador* aux = element1;
				return (aux->estado == NEW || aux->estado == BLOCKED) && verificar_cantidad_pokemones(aux) == 1;
			}
			t_list* entrenadores = equipo->entrenadores;
			libres = list_filter(entrenadores,(void*)by_estado);
		}
		else{
			bool by_estado(void* element1){
				entrenador* aux = element1;
				return aux->estado == BLOCKED;
			}
			t_list* entrenadores = equipo->entrenadores;
			libres = list_filter(entrenadores,(void*)by_estado);
		}

		elegido = list_get(libres,0);//list_find(equipo->entrenadores,(void*)by_estado);
		list_destroy(libres);
	}
	else{
		log_info(logger,"ENTRENADOR MAS CERCA");
		elegido = find_entrenador_cerca(pok);
	}

	despertar_entrenador(elegido);
	activar_entrenador(elegido);
	return elegido;
}

void despertar_entrenador(entrenador* entre){
	if(entre->estado == NEW || entre->estado == BLOCKED || entre->estado == EXEC){
		pthread_mutex_lock(&mut);
		entre->estado = READY;
		log_warning(logger,"%s READY",entre->name);
		pthread_mutex_unlock(&mut);
	}
	else if(entre->estado == EXIT){
		log_error(logger,"OPERACION NO PERMITIDA!");
	}
	else{
		log_info(logger,"%s ALREADY READY",entre->name);
	}
}

void activar_entrenador(entrenador* entre){
	if(status == STOP || entre->estado == READY){
		pthread_mutex_lock(&mut);
		status = RUN;
		entre->estado = EXEC;
		log_warning(logger,"%s RUN",entre->name);
		pthread_cond_signal(&cond);
		pthread_mutex_unlock(&mut);
	}
	else if(entre->estado == NEW || entre->estado == BLOCKED || entre->estado == EXIT){
		log_error(logger,"OPERACION NO PERMITIDA!");
	}
	else{
		log_info(logger,"%s ALREADY RUN",entre->name);
	}
}

void bloquear_entrenador(entrenador* entre){
	if(status == RUN || entre->estado == EXEC){
		status = STOP;
		entre->estado = BLOCKED;
		log_warning(logger,"%s BLOCKED",entre->name);
		pthread_exit(&hilos_entrenador[entre->tid]);
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
		pthread_mutex_lock(&mut);
		entre->estado = EXIT;
		log_warning(logger,"%s EXIT",entre->name);
		pthread_mutex_unlock(&mut);
		pthread_cancel(hilos_entrenador[entre->tid]);
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


void moverse_entrenador(entrenador* entre,int posx,int posy){
	entre->estado = EXEC;
	if(verificar_cantidad_pokemones(entre) != 1){
		log_error(logger,"%s NO PUEDE ATRAPAR MAS POKEMONES!",entre->name);
		bloquear_entrenador(entre);
	}
	for(int i=0;i<retardo_cpu;i++){
		printf("%s moviendose ...\n",entre->name);
		sleep(1);
	}
	entre->posx = posx;
	entre->posy = posy;
	log_info(logger,"%s EN POSICION:[%d,%d]",entre->name,entre->posx,entre->posy);
}

void atrapar_pokemon(entrenador* entre,pokemon* pok){
	entre->estado = EXEC;
	if(pok != NULL){
		if(verificar_cantidad_pokemones(entre) == 1){
			for(int i=0;i<retardo_cpu;i++){
				printf("%s atrapando a %s ...\n",entre->name,pok->name);
				sleep(1);
			}
			list_add(entre->pokemones,pok);
			log_info(logger,"%s HA ATRAPADO POKEMON %s",entre->name,pok->name);
			remove_pokemon_map(pok);
		}
		else{
			log_error(logger,"%s NO PUEDE ATRAPAR MAS POKEMONES!",entre->name);
			bloquear_entrenador(entre);
		}
	}
	else{
		log_error(logger,"POKEMON %s NO ESTA EN POSICION[%d,%d] EN EL MAP!",pok->name,pok->posx,pok->posy);
	}
}

void intercambiar_pokemon(entrenador* entre1,entrenador* entre2){
	entre1->estado = EXEC;
	for(int i=0;i<retardo_cpu*5;i++){
		printf("%s y %s intercambiando pokemones ...\n",entre1->name,entre2->name);
		sleep(1);
	}
	pokemon* pok1 = pokemon_intercambiar(entre1);
	pokemon* pok2 = pokemon_intercambiar(entre2);
	bool by_name1(void* ele){
		pokemon* aux = ele;
		return aux->name = pok1->name;
	}
	bool by_name2(void* ele){
		pokemon* aux = ele;
		return aux->name = pok2->name;
	}
	list_remove_by_condition(entre1->pokemones,(void*)by_name1);
	list_remove_by_condition(entre2->pokemones,(void*)by_name2);
	list_add(entre1->pokemones,pok2);
	list_add(entre2->pokemones,pok1);
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


void crear_team(char* name){
	equipo = malloc(sizeof(team));
	equipo->name = name;
	equipo->entrenadores = list_create();
	equipo->objetivos = list_create();

	log_info(logger,"%s START!",equipo->name);

	crear_entrenador("entrenador1",1,1,"[PI]","[PI,CHU]");
	crear_entrenador("entrenador2",3,2,"[PI]","[PI,KA]");

	void get(void* element){
		pokemon* pok = element;
		get_pokemon(pok->name);
	}
	t_list* especies = especies_objetivo_team();
	list_iterate(especies,(void*)get);

	list_iterate(equipo->entrenadores,(void*)crear_hilo);
}

void crear_hilo(entrenador* entre){
	pthread_create(&hilos_entrenador[entre->tid],NULL,(void*)ejecutar_entrenador,entre);
}

void crear_entrenador(char* name,int posx,int posy,char* pokemones,char* objetivos){
	entrenador* entre = malloc(sizeof(entrenador));
	entre->name = name;
	entre->posx = posx;
	entre->posy = posy;
	entre->pokemones = list_create();
	entre->objetivos = list_create();
	entre->estado = NEW;
	entre->tid = hilo_counter;

	char** poks = string_get_string_as_array(pokemones);
	for(int i=0;i<cant_pokemones(poks);i++){
		list_add(entre->pokemones,crear_pokemon(poks[i]));
	}
	char** objs = string_get_string_as_array(objetivos);
	for(int i=0;i<cant_pokemones(objs);i++){
		list_add(entre->objetivos,crear_pokemon(objs[i]));
		list_add(equipo->objetivos,crear_pokemon(objs[i]));
	}

	list_add(equipo->entrenadores,entre);

	log_info(logger,"ENTRENADOR:%s EN POSICION:[%d,%d]",entre->name,entre->posx,entre->posy);
	log_warning(logger,"%s NEW",entre->name);

	hilo_counter++;

}

t_list* especies_objetivo_team(){
	bool repetido(void* element){
		pokemon* aux = element;
		return cant_especie_objetivo_team(aux->name) > 1;
	}
	t_list* especies = equipo->objetivos;

	while(hay_repetidos(especies)){
		list_remove_by_condition(especies,(void*)repetido);
	}
	return especies;
}

bool hay_repetidos(t_list* especies){
	bool repetido(void* element){
		pokemon* aux = element;
		return cant_especie_objetivo_team(aux->name) > 1;
	}
	return list_any_satisfy(especies,(void*)repetido);
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

pokemon* pokemon_intercambiar(entrenador* entre){
	t_list* necesidades;
	bool no_contener(void* ele){
		pokemon* aux = ele;
		return contener_pokemon(entre,aux->name)!=1;
	}
	necesidades = list_filter(entre->objetivos,(void*)no_contener);
	return list_get(necesidades,0);
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
		return strcmp(aux->name,pok->name)==0 && aux->posx==pok->posx && aux->posy==pok->posy;
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
		char* id = recibir_id_mensaje(conexion_broker);
		list_add(id_mensajes,id);
		log_info(logger,"ID_MENSAJE:%s",id);
	}
}

void catch_pokemon(pokemon* pok){
	//funcion default
	if(conexion_broker != -1){
		for(int i=0;i<retardo_cpu;i++){
			printf("%s enviando mensaje a BROKER ...\n",equipo->name);
			sleep(1);
		}
		enviar_mensaje_catch_pokemon(pok->name,conexion_broker);
		char* id = recibir_id_mensaje(conexion_broker);
		list_add(id_mensajes,id);
		log_info(logger,"ID_MENSAJE:%s",recibir_id_mensaje(conexion_broker));
		//espera caught_pokemon
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
		list_add(poks_requeridos,pok);
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
}

void suscribirse_broker(char* name){
	enviar_process_name(name,conexion_broker);
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
	alarm(tiempo_reconexion);
}

void end_of_quantum_handler(){
	if(conexion_broker == -1){
		log_error(logger,"CONECTARSE A BROKER ...");
		conexion_broker = crear_conexion(ip_broker,puerto_broker);
		if(conexion_broker != -1){
			suscribirse_broker(equipo_name);
		}
		else{
			alarm(tiempo_reconexion);
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

void enviar_process_name(char* process,int socket_cliente){
	t_paquete* paquete = malloc(sizeof(t_paquete));
	int tam = strlen(process) + 1;

	paquete->codigo_operacion = TEAM;
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = tam;
	paquete->buffer->stream = malloc(paquete->buffer->size);
	memcpy(paquete->buffer->stream, process, tam);

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

