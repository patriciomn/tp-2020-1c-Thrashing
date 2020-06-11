#include "gameboy.h"

uuid_t pid;

int main(int argc,char* argv[]){
	iniciar_gameboy(argc,argv);
	terminar_gameboy(conexion, logger, config);
}

void iniciar_gameboy(int argc,char* argv[]){
	logger = iniciar_logger();
	config = leer_config("gameboy.config");
	uuid_generate(pid);
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
					log_error(logger,"Parametros Incompletos");
				}
			}
			else{
				if(argv[3]!=NULL &&argv[4]!=NULL&&argv[5]!=NULL&&argv[6]!=NULL&&argv[7]){
					conectar_proceso(process);
					enviar_mensaje_new_pokemon(argv[3],atoi(argv[4]),atoi(argv[5]),atoi(argv[6]),atoi(argv[7]),conexion);
				}
				else{
					log_error(logger,"Parametros Incompletos");
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
					log_error(logger,"Parametros Incompletos");
				}
			}
			else{
				if(argv[3]!=NULL &&argv[4]!=NULL&&argv[5]!=NULL &&argv[6]!=NULL){
					conectar_proceso(process);
					enviar_mensaje_catch_pokemon(argv[3],atoi(argv[4]),atoi(argv[5]),atoi(argv[6]),conexion);
				}
				else{
					log_error(logger,"Parametros Incompletos");
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
					log_error(logger,"Parametros Incompletos");
				}
			}else{
				if(argv[3]!=NULL &&argv[4]!=NULL&&argv[5]!=NULL){
					conectar_proceso(process);
					enviar_mensaje_appeared_pokemon(argv[3],atoi(argv[4]),atoi(argv[5]),conexion);
				}
				else{
					log_error(logger,"Parametros Incompletos");
				}
			}
			break;
		case CAUGHT_POKEMON:
			//argv[3]:id_mensaje,argv[4]:ok/fail
			if(argv[3]!=NULL &&argv[4]!=NULL){
				conectar_proceso(process);
				enviar_mensaje_caught_pokemon(atoi(argv[3]),atoi(argv[4]),conexion);
			}
			else{
				log_error(logger,"Parametros Incompletos");
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
					log_error(logger,"Parametros Incompletos");
				}
			}
			else{
				if(argv[3]!=NULL&&argv[4]!=NULL){
					conectar_proceso(process);
					enviar_mensaje_get_pokemon(argv[3],atoi(argv[4]),conexion);
				}
				else{
					log_error(logger,"Parametros Incompletos");
				}
			}
			break;
		default:
			log_error(logger,"TIPO_MENSAJE INVALIDO!");
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
			log_error(logger,"PROCESO INVALIDO");
			exit(0);
	}
	conexion = crear_conexion(ip,puerto);
}


void suscriptor(int tipo){//falta localized
	enviar_info_suscripcion(tipo,conexion);
	recibir_confirmacion_suscripcion(conexion);
}

void recibir_mensajes(int tiempo){
	alarm(tiempo);
	while(1){
		int cod_op;
		if(recv(conexion, &cod_op, sizeof(int), MSG_WAITALL) == -1)
			cod_op = -1;
		switch(cod_op){
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
		}
	}
}

t_log* iniciar_logger(void){
	return log_create("gameboy.log","gameboy",1,LOG_LEVEL_INFO);
}

void terminar_gameboy(int conexion, t_log* logger, t_config* config){
	log_destroy(logger);
	config_destroy(config);
	liberar_conexion(conexion);
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

void enviar_mensaje_caught_pokemon(int id_mensaje,int ok_fail,int socket_cliente){
	t_paquete* paquete = malloc(sizeof(t_paquete));

	paquete->codigo_operacion = CAUGHT_POKEMON;
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = sizeof(int)*2 ;
	paquete->buffer->stream = malloc(paquete->buffer->size);
	memcpy(paquete->buffer->stream,&id_mensaje,sizeof(int));
	memcpy(paquete->buffer->stream+sizeof(int),&ok_fail,sizeof(int));

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

void enviar_info_suscripcion(int tipo,int socket_cliente){
	t_paquete* paquete = malloc(sizeof(t_paquete));
	paquete->codigo_operacion = SUSCRITO;
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = sizeof(int) + sizeof(uuid_t);
	paquete->buffer->stream = malloc(paquete->buffer->size);
	memcpy(paquete->buffer->stream, &tipo, sizeof(int));
	memcpy(paquete->buffer->stream+sizeof(int),&pid,sizeof(uuid_t));

	int bytes = paquete->buffer->size + 2*sizeof(int);

	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, 0);

	free(a_enviar);
	free(paquete->buffer->stream);
	free(paquete->buffer);
	free(paquete);
}

void enviar_ack(int tipo,int id){
	conectar_proceso(BROKER);
	t_paquete* paquete = malloc(sizeof(t_paquete));
	paquete->codigo_operacion = ACK;
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = sizeof(int)*2 + sizeof(uuid_t);
	paquete->buffer->stream = malloc(paquete->buffer->size);
	memcpy(paquete->buffer->stream,&pid, sizeof(uuid_t));
	memcpy(paquete->buffer->stream+sizeof(uuid_t),&tipo, sizeof(int));
	memcpy(paquete->buffer->stream+sizeof(uuid_t)+sizeof(int),&id,sizeof(int));

	int bytes = paquete->buffer->size + 2*sizeof(int);

	void* a_enviar = serializar_paquete(paquete, bytes);

	send(conexion, a_enviar, bytes, 0);
	printf("ACK enviado\n");

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
		/*int id,tam;
		memcpy(&id,valor,sizeof(int));
		memcpy(&tam,valor+sizeof(int),sizeof(int));
		char* n = (char*)valor+sizeof(int)*2;
		char* name = malloc(tam+1);
		strcpy(name,n);
		log_info(logger,"Llega Un Mensaje Tipo: GET_POKEMON ID:%d POKEMON:%s \n",id,name);
		free(name);*/
		log_info(logger,"Llega Un Mensaje Tipo: GET_POKEMON ID:%d POKEMON:%s \n",id,get->name);
		free(valor);
		enviar_ack(GET_POKEMON,id);
	}
	list_iterate(paquete,(void*)display);
	list_destroy(paquete);
}

void recibir_new_pokemon(){
	t_list* paquete = recibir_paquete(conexion);

	void display(void* valor){
		int id,tam,posx,posy,cant;
		memcpy(&id,valor,sizeof(int));
		memcpy(&tam,valor+sizeof(int),sizeof(int));
		char* n = (char*)valor+sizeof(int)*2;
		int len = tam+1;
		char* name = malloc(len);
		strcpy(name,n);
		memcpy(&posx,valor+sizeof(int)*2+len,sizeof(int));
		memcpy(&posy,valor+sizeof(int)*3+len,sizeof(int));
		memcpy(&cant,valor+sizeof(int)*4+len,sizeof(int));
		log_info(logger,"Llega Un Mensaje Tipo: NEW_POKEMON ID:%d POKEMON:%s POSX:%d POSY:%d CANT:%d\n",id,name,posx,posy,cant);
		free(name);
		free(valor);
		enviar_ack(NEW_POKEMON,id);
	}
	list_iterate(paquete,(void*)display);
	list_destroy(paquete);
}

void recibir_catch_pokemon(){
	t_list* paquete = recibir_paquete(conexion);

	void display(void* valor){
		int id,tam,posx,posy;
		memcpy(&id,valor,sizeof(int));
		memcpy(&tam,valor+sizeof(int),sizeof(int));
		char* n = (char*)valor+sizeof(int)*2;
		int len = tam+1;
		char* name = malloc(len);
		strcpy(name,n);
		memcpy(&posx,valor+sizeof(int)*2+len,sizeof(int));
		memcpy(&posy,valor+sizeof(int)*3+len,sizeof(int));
		log_info(logger,"Llega Un Mensaje Tipo: CATCH_POKEMON ID:%d POKEMON:%s POSX:%d POSY:%d\n",id,name,posx,posy);
		free(name);
		free(valor);
		enviar_ack(CATCH_POKEMON,id);
	}
	list_iterate(paquete,(void*)display);
	list_destroy(paquete);
}

void recibir_caught_pokemon(){
	t_list* paquete = recibir_paquete(conexion);
	void display(void* valor){
		int id,resultado;
		memcpy(&id,valor,sizeof(int));
		memcpy(&resultado,valor+sizeof(int),sizeof(int));
		log_info(logger,"Llega Un Mensaje Tipo: CAUGHT_POKEMON ID_Correlacional:%d Resultado:%d\n",id,resultado);
		free(valor);
		enviar_ack(CAUGHT_POKEMON,id);
	}
	list_iterate(paquete,(void*)display);
	list_destroy(paquete);
}

void recibir_appeared_pokemon(){
	t_list* paquete = recibir_paquete(conexion);

	void display(void* valor){
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
		free(name);
		free(valor);
		enviar_ack(APPEARED_POKEMON,id);
	}
	list_iterate(paquete,(void*)display);
	list_destroy(paquete);
}

//UTILS-----------------------------------------------------------------------------------------------------------------
void* recibir_buffer(int socket_cliente, uint32_t* size){
	void * buffer;

	recv(socket_cliente, size, sizeof(int), MSG_WAITALL);
	buffer = malloc(*size);
	recv(socket_cliente, buffer, *size, MSG_WAITALL);

	return buffer;
}

void recibir_confirmacion_suscripcion(int cliente_fd){
	int cod_op;
	if(recv(cliente_fd, &cod_op, sizeof(int), MSG_WAITALL) == -1)
		cod_op = -1;

	log_info(logger,"Se Ha suscripto A La Cola De Mensaje");
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
		memset(valor,0,tamanio);
		memcpy(valor, buffer+desplazamiento, tamanio);
		desplazamiento+=tamanio;
		list_add(valores, valor);
	}
	free(buffer);
	return valores;
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

	if(connect(socket_cliente, server_info->ai_addr, server_info->ai_addrlen) == -1)
		printf("error");

	freeaddrinfo(server_info);

	return socket_cliente;
}

void liberar_conexion(int socket_cliente){
	close(socket_cliente);
}
