#include "gameboy.h"

pid_t pid;
t_log* logger;
t_config* config;
int conexion;

int main(int argc,char* argv[]){
	iniciar_gameboy(argc,argv);
	terminar_gameboy(conexion, logger, config);
}

void iniciar_gameboy(int argc,char* argv[]){
	logger = log_create("gameboy.log","gameboy",1,LOG_LEVEL_INFO);
	config = leer_config("gameboy.config");
	pid = getpid();

	//argv[1]:proceso argv[2]:tipo_mensaje
	int process,tipo_msg;
	if(argv[1]!=NULL &&argv[2]!=NULL){
		process = proceso(argv[1]);//identificar BROKER/GAMECARD/TEAM/SUSCRIPTOR
		tipo_msg = tipo_mensaje(argv[2]);
		if(process == SUSCRIPTOR){
			conectar_proceso(process);
			suscriptor(tipo_msg);
			//argv[3] tiempoLimitado
			recibir_mensajes(atoi(argv[3]));
		}
	}
	switch(tipo_msg){
		case NEW_POKEMON:
			//argv[3]:nombre_pokemon,argv[4]:posx,argv[5]:posy,argv[6]:cantidad,argv[7]:id_correlacional
			if(process == BROKER){
				if(argv[3]!=NULL &&argv[4]!=NULL&&argv[5]!=NULL&&argv[6]!=NULL){
					conectar_proceso(process);
					enviar_mensaje_new_pokemon_broker(argv[3],atoi(argv[4]),atoi(argv[5]),atoi(argv[6]),conexion);
				}
				else{
					printf("\033[1;31mParametros Incompletos\033[0m\n");
				}
			}
			else{
				if(argv[3]!=NULL &&argv[4]!=NULL&&argv[5]!=NULL&&argv[6]!=NULL&&argv[7]){
					conectar_proceso(process);
					enviar_mensaje_new_pokemon(argv[3],atoi(argv[4]),atoi(argv[5]),atoi(argv[6]),atoi(argv[7]),conexion);
				}
				else{
					printf("\033[1;31mParametros Incompletos\033[0m\n");
				}
			}

			break;
		case CATCH_POKEMON:
			//argv[3]:nombre_pokemon,argv[4]:posx,argv[5]:posy
			if(process == BROKER){
				if(argv[3]!=NULL &&argv[4]!=NULL&&argv[5]!=NULL){
					conectar_proceso(process);
					enviar_mensaje_catch_pokemon_broker(argv[3],atoi(argv[4]),atoi(argv[5]),conexion);
				}
				else{
					printf("\033[1;31mParametros Incompletos\033[0m\n");
				}
			}
			else{
				if(argv[3]!=NULL &&argv[4]!=NULL&&argv[5]!=NULL &&argv[6]!=NULL){
					conectar_proceso(process);
					enviar_mensaje_catch_pokemon(argv[3],atoi(argv[4]),atoi(argv[5]),atoi(argv[6]),conexion);
				}
				else{
					printf("\033[1;31mParametros Incompletos\033[0m\n");
				}
			}
			break;
		case APPEARED_POKEMON:
			//argv[3]:nombre_pokemon,argv[4]:posx,argv[5]:posy,argv[6]:id_mensaje
			if(process == BROKER){
				if(argv[3]!=NULL &&argv[4]!=NULL&&argv[5]!=NULL&&argv[6]!=NULL){
					conectar_proceso(process);
					enviar_mensaje_appeared_pokemon_broker(argv[3],atoi(argv[4]),atoi(argv[5]),atoi(argv[6]),conexion);
				}
				else{
					printf("\033[1;31mParametros Incompletos\033[0m\n");
				}
			}else{
				if(argv[3]!=NULL &&argv[4]!=NULL&&argv[5]!=NULL){
					conectar_proceso(process);
					enviar_mensaje_appeared_pokemon(argv[3],atoi(argv[4]),atoi(argv[5]),conexion);
				}
				else{
					printf("\033[1;31mParametros Incompletos\033[0m\n");
				}
			}
			break;
		case CAUGHT_POKEMON:
			//argv[3]:id_mensaje,argv[4]:ok/fail
			if(argv[3]!=NULL &&argv[4]!=NULL){
				conectar_proceso(process);
				bool res = 0;
				if(string_equals_ignore_case(argv[4],"OK")) res = 1;
				enviar_mensaje_caught_pokemon(atoi(argv[3]),res,conexion);
			}
			else{
				printf("\033[1;31mParametros Incompletos\033[0m\n");
			}
			break;
		case GET_POKEMON:
			//argv[3]:nombre_pokemon
			if(process == BROKER){
				if(argv[3]!=NULL){
					conectar_proceso(process);
					enviar_mensaje_get_pokemon_broker(argv[3],conexion);
				}
				else{
					printf("\033[1;31mParametros Incompletos\033[0m\n");
				}
			}
			else{
				if(argv[3]!=NULL&&argv[4]!=NULL){
					conectar_proceso(process);
					enviar_mensaje_get_pokemon(argv[3],atoi(argv[4]),conexion);
				}
				else{
					printf("\033[1;31mParametros Incompletos\033[0m\n");
				}
			}
			break;
		default:
			printf("\033[1;31mTIPO DE MENSAJE INVALIDO!\033[0m\n");
			exit(0);
	}
}

int proceso(char* proceso){
	if(strcmp(proceso,"BROKER") == 0){
		return BROKER;
	}
	else if(strcmp(proceso,"TEAM") == 0){
		return TEAM;
	}
	else if(strcmp(proceso,"GAMECARD") == 0){
		return GAMECARD;
	}
	else{
		return SUSCRIPTOR;
	}
	return -1;
}

int tipo_mensaje(char* tipo_mensaje){
	if(strcmp(tipo_mensaje,"NEW_POKEMON") == 0){
		return NEW_POKEMON;
	}
	else if(strcmp(tipo_mensaje,"APPEARED_POKEMON") == 0){
		return APPEARED_POKEMON;
	}
	else if(strcmp(tipo_mensaje,"CATCH_POKEMON") == 0){
		return CATCH_POKEMON;
	}
	else if(strcmp(tipo_mensaje,"CAUGHT_POKEMON") == 0){
		return CAUGHT_POKEMON;
	}
	else if(strcmp(tipo_mensaje,"GET_POKEMON") == 0){
		return GET_POKEMON;
	}
	else{
		return LOCALIZED_POKEMON;
	}
	return -1;
}

void conectar_proceso(int proceso){
	char* ip;
	char* puerto;
	switch(proceso){
		case BROKER:
			ip = config_get_string_value(config,"BROKER_IP");
			puerto = config_get_string_value(config,"BROKER_PUERTO");
			log_info(logger,"Conectado A BROKER");
			break;
		case SUSCRIPTOR:
			ip = config_get_string_value(config,"BROKER_IP");
			puerto = config_get_string_value(config,"BROKER_PUERTO");
			log_info(logger,"Conectado A BROKER");
			break;
		case TEAM:
			ip = config_get_string_value(config,"TEAM_IP");
			puerto = config_get_string_value(config,"TEAM_PUERTO");
			log_info(logger,"Conectado A TEAM");
			break;
		case GAMECARD:
			ip = config_get_string_value(config,"GAMECARD_IP");
			puerto = config_get_string_value(config,"GAMECARD_PUERTO");
			log_info(logger,"Conectado A GAMECARD");
			break;
		case -1:
			printf("\033[1;31mPROCESO INVALIDO\033[0m\n");
			exit(0);
	}
	conexion = crear_conexion(ip,puerto);
}


void suscriptor(int tipo){
	enviar_info_suscripcion(tipo,conexion,pid);
	recibir_confirmacion_suscripcion(conexion,tipo);
}

void fin_tiempo_handler(){
	printf("GAMEBOY Desconectado\n");
	exit(0);
}

void fin_tiempo(int tiempo){
	struct sigaction action;
	action.sa_handler = (void*)fin_tiempo_handler;
	action.sa_flags = SA_RESTART|SA_NODEFER;
	sigemptyset(&action.sa_mask);
	sigaction(SIGALRM, &action, 0);
	alarm(tiempo);
}

void recibir_mensajes(int tiempo){
	fin_tiempo(tiempo);

	while(1){
		int cod_op;
		if(recv(conexion, &cod_op, sizeof(int), MSG_WAITALL) == -1)
			cod_op = -1;
		switch(cod_op){
		sleep(1);
			case GET_POKEMON:
				recibir_get_pokemon();
				break;
			case NEW_POKEMON:
				recibir_new_pokemon();
				break;
			case CATCH_POKEMON:
				recibir_catch_pokemon();
				break;
			case CAUGHT_POKEMON:
				recibir_caught_pokemon();
				break;
			case APPEARED_POKEMON:
				recibir_appeared_pokemon();
				break;
			case LOCALIZED_POKEMON:
				recibir_localized_pokemon();
				break;
		}
	}
}

void terminar_gameboy(int conexion, t_log* logger, t_config* config){
	log_destroy(logger);
	config_destroy(config);
	liberar_conexion(conexion);
	printf("GAMEBOY Desconectado\n");
}

//ENVIAR--------------------------------------------------------------------------------------------------------------------

void enviar_mensaje_new_pokemon_broker(char* pokemon,int posx,int posy,int cant,int socket_cliente){
	int tam = strlen(pokemon)+1;
	int name_size = strlen(pokemon);
	position* pos = malloc(sizeof(position));
	pos->posx = posx;
	pos->posy = posy;
	t_paquete* paquete = malloc(sizeof(t_paquete));

	paquete->codigo_operacion = NEW_POKEMON;
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = sizeof(int)*2 + tam + sizeof(position);
	paquete->buffer->stream = malloc(paquete->buffer->size);
	memcpy(paquete->buffer->stream,&name_size,sizeof(int));
	memcpy(paquete->buffer->stream+sizeof(int),pokemon,tam);
	memcpy(paquete->buffer->stream+sizeof(int)+tam,pos,sizeof(position));
	memcpy(paquete->buffer->stream+sizeof(int)+tam+sizeof(position), &cant,sizeof(int));

	int bytes = paquete->buffer->size + 2*sizeof(int);

	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, 0);

	free(a_enviar);
	free(paquete->buffer->stream);
	free(paquete->buffer);
	free(paquete);
}

void enviar_mensaje_new_pokemon(char* pokemon,int posx,int posy,int cant, int id_correlacional,int socket_cliente){
	int tam = strlen(pokemon)+1;
	int name_size = strlen(pokemon);
	position* pos = malloc(sizeof(position));
	pos->posx = posx;
	pos->posy = posy;
	t_paquete* paquete = malloc(sizeof(t_paquete));

	paquete->codigo_operacion = NEW_POKEMON;
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = sizeof(int)*3 + tam +sizeof(position);
	paquete->buffer->stream = malloc(paquete->buffer->size);
	memcpy(paquete->buffer->stream,&id_correlacional,sizeof(int));
	memcpy(paquete->buffer->stream+sizeof(int),&name_size,sizeof(int));
	memcpy(paquete->buffer->stream+sizeof(int)*2,pokemon,tam);
	memcpy(paquete->buffer->stream+sizeof(int)*2+tam,pos,sizeof(position));
	memcpy(paquete->buffer->stream+sizeof(int)*2+tam+sizeof(position), &cant,sizeof(int));

	int bytes = paquete->buffer->size + 2*sizeof(int);

	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, 0);

	free(a_enviar);
	free(paquete->buffer->stream);
	free(paquete->buffer);
	free(paquete);
}

void enviar_mensaje_appeared_pokemon_broker(char* pokemon,int posx,int posy,int id_correlacional, int socket_cliente){
	int tam = strlen(pokemon) + 1;
	int name_size = strlen(pokemon);
	position* pos = malloc(sizeof(position));
	pos->posx = posx;
	pos->posy = posy;
	t_paquete* paquete = malloc(sizeof(t_paquete));

	paquete->codigo_operacion = APPEARED_POKEMON;
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = tam + sizeof(int)*2 + sizeof(position);
	paquete->buffer->stream = malloc(paquete->buffer->size);
	memcpy(paquete->buffer->stream, &id_correlacional, sizeof(int));
	memcpy(paquete->buffer->stream+sizeof(int),&name_size,sizeof(int));
	memcpy(paquete->buffer->stream+sizeof(int)*2, pokemon,tam);
	memcpy(paquete->buffer->stream+tam+sizeof(int)*2,pos, sizeof(position));

	int bytes = paquete->buffer->size + 2*sizeof(int);
	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, 0);

	free(pos);
	free(a_enviar);
	free(paquete->buffer->stream);
	free(paquete->buffer);
	free(paquete);
}

void enviar_mensaje_appeared_pokemon(char* pokemon,int posx,int posy,int socket_cliente){
	int tam = strlen(pokemon) + 1;
	position* pos = malloc(sizeof(position));
	pos->posx = posx;
	pos->posy = posy;
	t_paquete* paquete = malloc(sizeof(t_paquete));

	paquete->codigo_operacion = APPEARED_POKEMON;
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = tam + sizeof(position);
	paquete->buffer->stream = malloc(paquete->buffer->size);
	memcpy(paquete->buffer->stream,pokemon,tam);
	memcpy(paquete->buffer->stream+tam,pos,sizeof(position));

	int bytes = paquete->buffer->size + 2*sizeof(int);
	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, 0);

	free(pos);
	free(a_enviar);
	free(paquete->buffer->stream);
	free(paquete->buffer);
	free(paquete);
}

void enviar_mensaje_caught_pokemon(int id_mensaje,bool ok_fail,int socket_cliente){
	t_paquete* paquete = malloc(sizeof(t_paquete));

	paquete->codigo_operacion = CAUGHT_POKEMON;
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = sizeof(int)*2 ;
	paquete->buffer->stream = malloc(paquete->buffer->size);
	memcpy(paquete->buffer->stream,&id_mensaje,sizeof(int));
	memcpy(paquete->buffer->stream+sizeof(int),&ok_fail,sizeof(bool));

	int bytes = paquete->buffer->size + 2*sizeof(int);

	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, 0);

	log_info(logger,"CAUGHT_POKEMON");

	free(a_enviar);
	free(paquete->buffer->stream);
	free(paquete->buffer);
	free(paquete);
}

void enviar_mensaje_catch_pokemon_broker(char* pokemon,int posx,int posy,int socket_cliente){
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

	send(socket_cliente, a_enviar, bytes, 0);

	log_info(logger,"CATCH_POKEMON:%s",pokemon);

	free(a_enviar);
	free(paquete->buffer->stream);
	free(paquete->buffer);
	free(paquete);
}

void enviar_mensaje_catch_pokemon(char* pokemon,int posx,int posy,int id_correlacional,int socket_cliente){
	int tam = strlen(pokemon) + 1;
	int name_size = strlen(pokemon);
	position* pos = malloc(sizeof(position));
	pos->posx = posx;
	pos->posy = posy;
	t_paquete* paquete = malloc(sizeof(t_paquete));

	paquete->codigo_operacion = CATCH_POKEMON;
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = sizeof(int)*2 + tam +sizeof(position);
	paquete->buffer->stream = malloc(paquete->buffer->size);
	memcpy(paquete->buffer->stream,&id_correlacional,sizeof(int));
	memcpy(paquete->buffer->stream+sizeof(int),&name_size,sizeof(int));
	memcpy(paquete->buffer->stream+sizeof(int)*2,pokemon,tam);
	memcpy(paquete->buffer->stream+sizeof(int)*2+tam,pos,sizeof(position));

	int bytes = paquete->buffer->size + 2*sizeof(int);

	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, 0);

	log_info(logger,"CATCH_POKEMON:%s",pokemon);

	free(a_enviar);
	free(paquete->buffer->stream);
	free(paquete->buffer);
	free(paquete);
}

void enviar_mensaje_get_pokemon_broker(char* pokemon,int socket_cliente){
	int tam = strlen(pokemon)+1;
	int name_size = strlen(pokemon);
	t_paquete* paquete = malloc(sizeof(t_paquete));

	paquete->codigo_operacion = GET_POKEMON;
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = sizeof(int) + tam ;
	paquete->buffer->stream = malloc(paquete->buffer->size);
	memcpy(paquete->buffer->stream,&name_size,sizeof(int));
	memcpy(paquete->buffer->stream+sizeof(int), pokemon,tam);

	int bytes = paquete->buffer->size + sizeof(int)*2;

	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, 0);

	free(a_enviar);
	free(paquete->buffer->stream);
	free(paquete->buffer);
	free(paquete);
}

void enviar_mensaje_get_pokemon(char* pokemon,int id_correlacional,int socket_cliente){
	int tam = strlen(pokemon)+1;
	int name_size = strlen(pokemon);
	t_paquete* paquete = malloc(sizeof(t_paquete));

	paquete->codigo_operacion = GET_POKEMON;
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = tam+sizeof(int)*2 ;
	paquete->buffer->stream = malloc(paquete->buffer->size);
	memcpy(paquete->buffer->stream,&id_correlacional,sizeof(int));
	memcpy(paquete->buffer->stream+sizeof(int),&name_size,sizeof(int));
	memcpy(paquete->buffer->stream+sizeof(int)*2,pokemon,tam);

	int bytes = paquete->buffer->size + sizeof(int)*2;

	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, 0);

	free(a_enviar);
	free(paquete->buffer->stream);
	free(paquete->buffer);
	free(paquete);
}

//RECIBIR-----------------------------------------------------------------------------------------------------------------
void recibir_get_pokemon(){
	t_list* paquete = recibir_paquete(conexion);
	void display(void* valor){
		int id;
		memcpy(&id,valor,sizeof(int));
		get_pokemon* get = deserializar_get(valor+sizeof(int));
		log_info(logger,"Llega Un Mensaje Tipo: GET_POKEMON ID:%d POKEMON:%s \n",id,get->name);
		free(valor);
		//conectar_proceso(BROKER);
		enviar_ack(GET_POKEMON,id,pid,conexion);
		free(get->name);
		free(get);
	}
	list_iterate(paquete,(void*)display);
	list_destroy(paquete);
}

void recibir_new_pokemon(){
	t_list* paquete = recibir_paquete(conexion);

	void display(void* valor){
		int id;
		memcpy(&id,valor,sizeof(int));
		new_pokemon* new = deserializar_new(valor+sizeof(int));
		log_info(logger,"Llega Un Mensaje Tipo: NEW_POKEMON ID:%d POKEMON:%s POSX:%d POSY:%d CANT:%d\n",id,new->name,new->pos.posx,new->pos.posy,new->cantidad);
		free(valor);
		//conectar_proceso(BROKER);
		enviar_ack(NEW_POKEMON,id,pid,conexion);
		free(new->name);
		free(new);
	}
	list_iterate(paquete,(void*)display);
	list_destroy(paquete);
}

void recibir_catch_pokemon(){
	t_list* paquete = recibir_paquete(conexion);

	void display(void* valor){
		int id;
		memcpy(&id,valor,sizeof(int));
		catch_pokemon* catch = deserializar_catch(valor+sizeof(int));
		log_info(logger,"Llega Un Mensaje Tipo: CATCH_POKEMON ID:%d POKEMON:%s POSX:%d POSY:%d\n",id,catch->name,catch->pos.posx,catch->pos.posy);
		free(valor);
		//conectar_proceso(BROKER);
		enviar_ack(CATCH_POKEMON,id,pid,conexion);
		free(catch->name);
		free(catch);
	}
	list_iterate(paquete,(void*)display);
	list_destroy(paquete);
}

void recibir_caught_pokemon(){
	t_list* paquete = recibir_paquete(conexion);
	void display(void* valor){
		int id,id_correlacional;
		memcpy(&id,valor,sizeof(int));
		memcpy(&id_correlacional,valor+sizeof(int),sizeof(int));
		caught_pokemon* caught = deserializar_caught(valor+sizeof(int));
		log_info(logger,"Llega Un Mensaje Tipo: CAUGHT_POKEMON ID:%d,ID_Correlacional:%d Resultado:%d\n",id,id_correlacional,caught->caught);
		free(valor);
		//conectar_proceso(BROKER);
		enviar_ack(CAUGHT_POKEMON,id,pid,conexion);
		free(caught);
	}
	list_iterate(paquete,(void*)display);
	list_destroy(paquete);
}

void recibir_appeared_pokemon(){
	t_list* paquete = recibir_paquete(conexion);

	void display(void* valor){
		int id,id_correlacional;
		memcpy(&id,valor,sizeof(int));
		memcpy(&id_correlacional,valor+sizeof(int),sizeof(int));
		appeared_pokemon* appeared = deserializar_appeared(valor+sizeof(int));
		log_info(logger,"Llega Un Mensaje Tipo: APPEARED_POKEMON: ID: %d ID_Correlacional: %d Pokemon: %s  Pos X:%d  Pos Y:%d\n",id,id_correlacional,appeared->name,appeared->pos.posx,appeared->pos.posy);
		free(valor);
		//conectar_proceso(BROKER);
		enviar_ack(APPEARED_POKEMON,id,pid,conexion);
		free(appeared->name);
		free(appeared);
	}
	list_iterate(paquete,(void*)display);
	list_destroy(paquete);
}

void recibir_localized_pokemon(){
	t_list* paquete = recibir_paquete(conexion);

	void display(void* valor){
		int id,id_correlacional;
		memcpy(&id,valor,sizeof(int));
		memcpy(&id_correlacional,valor+sizeof(int),sizeof(int));
		localized_pokemon* localized = deserializar_localized(valor+sizeof(int));
		log_info(logger,"Llega Un Mensaje Tipo: APPEARED_POKEMON: ID: %d ID_Correlacional: %d Pokemon: %s \n",id,id_correlacional,localized->name);
		void show(position* pos){
			log_info(logger,"Pos:[%d,%d]",pos->posx,pos->posy);
			free(pos);
		}
		list_iterate(localized->posiciones,(void*)show);
		free(valor);
		//conectar_proceso(BROKER);
		enviar_ack(LOCALIZED_POKEMON,id,pid,conexion);
		list_destroy(localized->posiciones);
		free(localized->name);
		free(localized);
	}
	list_iterate(paquete,(void*)display);
	list_destroy(paquete);
}
