#include "gameboy.h"

int main(int argc,char* argv[]){
	//argv[1]:proceso argv[2]:tipo_mensaje
	iniciar_gameboy();

	int process = proceso(argv[1]);//identificar BROKER/GAMECARD/TEAM
	conectar_proceso(process);
	int tipo_msg = tipo_mensaje(argv[2]);
	switch(tipo_msg){
		case NEW_POKEMON:
			//argv[3]:nombre_pokemon,argv[4]:posx,argv[5]:posy,argv[6]:cantidad
			new_pokemon(argv[3],atoi(argv[4]),atoi(argv[5]),atoi(argv[6]));
			break;
		case CATCH_POKEMON:
			//argv[3]:nombre_pokemon,argv[4]:posx,argv[5]:posy
			catch_pokemon(argv[3],atoi(argv[4]),atoi(argv[5]));
			break;
		case APPEARED_POKEMON:
			//argv[3]:nombre_pokemon,argv[4]:posx,argv[5]:posy,argv[6]:ide_mensaje
			appeared_pokemon(process,argv[3],atoi(argv[4]),atoi(argv[5]),atoi(argv[6]));
			break;
		case CAUGHT_POKEMON:
			//argv[3]:id_mensaje,argv[4]:ok/fail
			caught_pokemon(atoi(argv[3]),atoi(argv[4]));
			break;
		case GET_POKEMON:
			//argv[3]:nombre_pokemon
			get_pokemon(argv[3]);
			break;
		default:
			log_error(logger,"TIPO_MENSAJE INVALIDO!");
			exit(0);
	}

	terminar_gameboy(conexion, logger, config);
}


void iniciar_gameboy(){
	logger = iniciar_logger();
	config = leer_config();
	log_info(logger,"GAMEBOY START!");
}

int proceso(char* proceso){
	log_info(logger,"PROCESO: %s",proceso);
	if(strcmp(proceso,"BROKER") == 0){
		return BROKER;
	}
	else if(strcmp(proceso,"TEAM") == 0){
		return TEAM;
	}
	else if(strcmp(proceso,"GAMECARD") == 0){
		return GAMECARD;
	}
	/*else if(strcmp(proceso,"SUSCRIPTOR") == 0){
		return SUSCRIPTOR;
	}*/
	return -1;
}

int tipo_mensaje(char* tipo_mensaje){
	log_info(logger,"TIPO_MENSAJE: %s",tipo_mensaje);
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
			break;
		case TEAM:
			ip = config_get_string_value(config,"TEAM_IP");
			puerto = config_get_string_value(config,"TEAM_PUERTO");
			break;
		case GAMECARD:
			ip = config_get_string_value(config,"GAMECARD_IP");
			puerto = config_get_string_value(config,"GAMECARD_PUERTO");
			break;
		case -1:
			log_error(logger,"PROCESO INVALIDO");
			exit(0);
	}
	conexion = crear_conexion(ip,puerto);
}

void new_pokemon(char* pokemon,int posx,int posy,int cant){
	enviar_mensaje_new_pokemon(pokemon,posx,posy,cant,conexion);
}

void appeared_pokemon(enum TIPO_PROCESO pro,char* pokemon,int posx,int posy,int id_mensaje){
	enviar_mensaje_appeared_pokemon(pro,pokemon,posx,posy,id_mensaje,conexion);
}

void catch_pokemon(char* pokemon,int posx,int posy){
	enviar_mensaje_catch_pokemon(pokemon,posx,posy,conexion);
}

void caught_pokemon(int id_mensaje,int ok_fail){
	enviar_mensaje_caught_pokemon(id_mensaje,ok_fail,conexion);
}

void get_pokemon(char*pokemon){
	enviar_mensaje_get_pokemon(pokemon,conexion);
}

t_log* iniciar_logger(void){
	return log_create("gameboy.log","gameboy",1,LOG_LEVEL_INFO);
}

t_config* leer_config(void){
	return config_create("gameboy.config");
}

void terminar_gameboy(int conexion, t_log* logger, t_config* config){
	log_destroy(logger);
	config_destroy(config);
	liberar_conexion(conexion);
}

//--------------------------------------------------------------------------------------------------------------------
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


void enviar_mensaje_new_pokemon(char* pokemon,int posx,int posy,int cant, int socket_cliente){
	t_paquete* paquete = malloc(sizeof(t_paquete));
	int tam = strlen(pokemon) + 1;

	paquete->codigo_operacion = NEW_POKEMON;
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = tam + sizeof(int)*3;
	paquete->buffer->stream = malloc(paquete->buffer->size);
	memcpy(paquete->buffer->stream, pokemon, tam);
	memcpy(paquete->buffer->stream+tam, &posx, sizeof(int));
	memcpy(paquete->buffer->stream+tam+sizeof(int), &posy, sizeof(int));
	memcpy(paquete->buffer->stream+tam+sizeof(int)+sizeof(int), &cant, sizeof(int));

	int bytes = paquete->buffer->size + 2*sizeof(int);

	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, 0);

	free(a_enviar);
	free(paquete->buffer->stream);
	free(paquete->buffer);
	free(paquete);
}

void enviar_mensaje_appeared_pokemon(enum TIPO_PROCESO pro,char* pokemon,int posx,int posy,int id_mensaje, int socket_cliente){
	t_paquete* paquete = malloc(sizeof(t_paquete));
	int tam = strlen(pokemon) + 1;

	paquete->codigo_operacion = APPEARED_POKEMON;
	paquete->buffer = malloc(sizeof(t_buffer));

	if(pro != TEAM){
		paquete->buffer->size = tam + sizeof(int)*3;
		paquete->buffer->stream = malloc(paquete->buffer->size);
		memcpy(paquete->buffer->stream, pokemon, tam);
		memcpy(paquete->buffer->stream+tam, &posx, sizeof(int));
		memcpy(paquete->buffer->stream+tam+sizeof(int), &posy, sizeof(int));
		memcpy(paquete->buffer->stream+tam+sizeof(int)+sizeof(int), &id_mensaje, sizeof(int));
	}
	else{
		paquete->buffer->size = tam + sizeof(int)*2;
		paquete->buffer->stream = malloc(paquete->buffer->size);
		memcpy(paquete->buffer->stream, pokemon, tam);
		memcpy(paquete->buffer->stream+tam, &posx, sizeof(int));
		memcpy(paquete->buffer->stream+tam+sizeof(int), &posy, sizeof(int));
		log_info(logger,"APPEARED A TEAM");
	}

	int bytes = paquete->buffer->size + 2*sizeof(int);
	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, 0);

	free(a_enviar);
	free(paquete->buffer->stream);
	free(paquete->buffer);
	free(paquete);
}


void enviar_mensaje_caught_pokemon(int id_mensaje,int ok_fail,int socket_cliente){
	t_paquete* paquete = malloc(sizeof(t_paquete));

	paquete->codigo_operacion = CAUGHT_POKEMON;
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = sizeof(int)*2;
	paquete->buffer->stream = malloc(paquete->buffer->size);
	memcpy(paquete->buffer->stream, &id_mensaje, sizeof(int));
	memcpy(paquete->buffer->stream+sizeof(int), &ok_fail, sizeof(int));

	int bytes = paquete->buffer->size + 2*sizeof(int);

	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, 0);

	free(a_enviar);
	free(paquete->buffer->stream);
	free(paquete->buffer);
	free(paquete);
}

void enviar_mensaje_catch_pokemon(char* pokemon,int posx,int posy,int socket_cliente){
	t_paquete* paquete = malloc(sizeof(t_paquete));
	int tam = strlen(pokemon) + 1;

	paquete->codigo_operacion = CATCH_POKEMON;
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = tam + sizeof(int)*2;
	paquete->buffer->stream = malloc(paquete->buffer->size);
	memcpy(paquete->buffer->stream, pokemon, tam);
	memcpy(paquete->buffer->stream+tam, &posx, sizeof(int));
	memcpy(paquete->buffer->stream+tam+sizeof(int), &posy, sizeof(int));

	int bytes = paquete->buffer->size + 2*sizeof(int);

	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, 0);

	free(a_enviar);
	free(paquete->buffer->stream);
	free(paquete->buffer);
	free(paquete);
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

	free(a_enviar);
	free(paquete->buffer->stream);
	free(paquete->buffer);
	free(paquete);
}

char* recibir_mensaje(int socket_cliente){
	void* buffer;
	int size;
	int accion = recv(socket_cliente, &accion, sizeof(int), MSG_WAITALL);
	recv(socket_cliente, &size, sizeof(int), MSG_WAITALL);
	buffer = malloc(size);
	recv(socket_cliente, buffer,size, MSG_WAITALL);

	return buffer;
}


void enviar_process_name(char* process,int socket_cliente){
	t_paquete* paquete = malloc(sizeof(t_paquete));
	int tam = strlen(process) + 1;

	paquete->codigo_operacion = GAMEBOY;
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


void liberar_conexion(int socket_cliente){
	close(socket_cliente);
}
