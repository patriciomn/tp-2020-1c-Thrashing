#include"broker.h"

int main(){	
    iniciar_broker();
	iniciar_servidor();

	terminar_broker( conexion, logger, config);
}

void iniciar_servidor(void)
{
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

void esperar_cliente(int socket_servidor)
{
	struct sockaddr_in dir_cliente;

	int tam_direccion = sizeof(struct sockaddr_in);

	int socket_cliente = accept(socket_servidor, (void*) &dir_cliente, &tam_direccion);

	pthread_create(&thread,NULL,(void*)serve_client,&socket_cliente);
	pthread_detach(thread);

}

void serve_client(int* socket)
{
	int cod_op;
	if(recv(*socket, &cod_op, sizeof(int), MSG_WAITALL) == -1)
		cod_op = -1;
	process_request(cod_op, *socket);
}


//ESTO HAY QUE CODEARLO
void process_request(int cod_op, int cliente_fd) {
	int size;
	void* msg;
		switch (cod_op) {
		case NEW_POKEMON:
			msg = recibir_mensaje(cliente_fd, &size); //Aca abria que deserializar el paquete
			//Responder correlation_id al que me envio el req (cliente_fd)
			//Pushear en la cola de NEW con id, correlationId 
			//Salir, luego otro hilo vendra a recoger de la cola donde lo deje y enviara a los suscribers. NO ENVIO ACA
			free(msg);
			break;
		case APPEARED_POKEMON:
			msg = recibir_mensaje(cliente_fd, &size); //Aca abria que deserializar el paquete
			//Responder correlation_id al que me envio el req (cliente_fd)
			//Pushear en la cola de APPEARED con id, correlationId 
			//Salir, luego otro hilo vendra a recoger de la cola donde lo deje y enviara a los suscribers. NO ENVIO ACA
			
			free(msg);
			break;
		case CATCH_POKEMON:
			msg = recibir_mensaje(cliente_fd, &size);
			//Responder correlation_id al que me envio el req (cliente_fd)
			//Pushear en la cola de CATCH con id, correlationId 
			//Salir, luego otro hilo vendra a recoger de la cola donde lo deje y enviara a los suscribers. NO ENVIO ACA
			
			free(msg);
			break;
		case CAUGHT_POKEMON:
			msg = recibir_mensaje(cliente_fd, &size);
			//Responder correlation_id al que me envio el req (cliente_fd)
			//Pushear en la cola de CAUGHT con id, correlationId 
			//Salir, luego otro hilo vendra a recoger de la cola donde lo deje y enviara a los suscribers. NO ENVIO ACA
			
			free(msg);
			break;
		case GET_POKEMON:
			msg = recibir_mensaje(cliente_fd, &size);
			//Responder correlation_id al que me envio el req (cliente_fd)
			//Pushear en la cola de GET con id, correlationId 
			//Salir, luego otro hilo vendra a recoger de la cola donde lo deje y enviara a los suscribers. NO ENVIO ACA
			
			free(msg);
			break;
		case LOCALIZED_POKEMON:
			msg = recibir_mensaje(cliente_fd, &size);
			//Responder correlation_id al que me envio el req (cliente_fd)
			//Pushear en la cola de LOCALIZED con id, correlationId 
			//Salir, luego otro hilo vendra a recoger de la cola donde lo deje y enviara a los suscribers. NO ENVIO ACA
			
			free(msg);
			break;
		
		case 0:
			pthread_exit(NULL);
		case -1:
			pthread_exit(NULL);
		}
}

//ESTO TAMBIEN HAY QUE CODEARLO
void* recibir_mensaje(int socket_cliente, int* size)
{
	void * buffer;

	recv(socket_cliente, size, sizeof(int), MSG_WAITALL);
	buffer = malloc(*size);
	recv(socket_cliente, buffer, *size, MSG_WAITALL);

	return buffer;
}


void iniciar_broker(void){
	logger = log_create("broker.log","broker",1,LOG_LEVEL_INFO);
	config = config_create("broker.config");
	log_info(logger,"BROKER START!");
	
}

void terminar_broker( t_log* logger, t_config* config){
	log_destroy(logger);
	config_destroy(config);
}

