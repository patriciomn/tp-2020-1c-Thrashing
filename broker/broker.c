#include"broker.h"

int main(){	
    iniciar_broker();
	log_info(logger,"creating server");
	iniciar_servidor();

	// terminar_broker( logger, config);
}

void iniciar_servidor(void)
{
	int socket_servidor;

    struct addrinfo hints, *servinfo, *p;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

	
    getaddrinfo("127.0.0.1", "4444", &hints, &servinfo);

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
	log_info(logger,"Waiting Client");
	int socket_cliente = accept(socket_servidor, (void*) &dir_cliente, &tam_direccion);
	log_info(logger,"Client received");
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
	queue_item *item;
	
	log_info(logger,"Processing request, cod_op: %d", cod_op);
	msg = recibir_mensaje(cliente_fd, &size);
	log_info(logger,"%s", msg);

	item->id = get_id();
	item->correlation_id = get_correlation_id();
	
	//CADA VEZ QUE SE PUSHEA EN queues[i] HAY QUE DUMPEAR EN MEMORIA
	if (cod_op == NEW_POKEMON){
		new_pokemon* new = deserializar_new(msg);
		item->message = new;
		//Responder correlation_id al que me envio el req (cliente_fd)
		//Pushear en la cola de NEW con id, correlationId 
		//Salir, luego otro hilo vendra a recoger de la cola donde lo deje y enviara a los suscribers. NO ENVIO ACA
	}
	else if (cod_op == APPEARED_POKEMON){
		appeared_pokemon* appeared = deserializar_appeared(msg);
		item->message = appeared;
		//Responder correlation_id al que me envio el req (cliente_fd)
		//Pushear en la cola de APPEARED con id, correlationId 
		//Salir, luego otro hilo vendra a recoger de la cola donde lo deje y enviara a los suscribers. NO ENVIO ACA
	}
	else if (cod_op == CATCH_POKEMON){
		catch_pokemon* catch = deserializar_catch(msg);
		item->message = catch;
		//Responder correlation_id al que me envio el req (cliente_fd)
		//Pushear en la cola de CATCH con id, correlationId 
		//Salir, luego otro hilo vendra a recoger de la cola donde lo deje y enviara a los suscribers. NO ENVIO ACA
		free(msg);
	}
	else if (cod_op == CAUGHT_POKEMON){
		caught_pokemon* caught = deserializar_caught(msg);
		item->message = caught;
		//Responder correlation_id al que me envio el req (cliente_fd)
		//Pushear en la cola de CAUGHT con id, correlationId 
		//Salir, luego otro hilo vendra a recoger de la cola donde lo deje y enviara a los suscribers. NO ENVIO ACA
		free(msg);
	}
	else if (cod_op == GET_POKEMON){
		get_pokemon* get = deserializar_get(msg);
		item->message = get;
		//Responder correlation_id al que me envio el req (cliente_fd)
		//Pushear en la cola de GET con id, correlationId 
		//Salir, luego otro hilo vendra a recoger de la cola donde lo deje y enviara a los suscribers. NO ENVIO ACA
	}
	else if (cod_op == LOCALIZED_POKEMON){
		localized_pokemon* localized = deserializar_localized(msg);
		item->message = localized;
		//Responder correlation_id al que me envio el req (cliente_fd)
		//Pushear en la cola de LOCALIZED con id, correlationId
		//Salir, luego otro hilo vendra a recoger de la cola donde lo deje y enviara a los suscribers. NO ENVIO ACA
		
	}
	else if (cod_op == SUSCRITO){
		msg = recibir_mensaje(cliente_fd, &size);
		//En msg voy a tener el TIPO de la cola a la que se suscribieron
		//Agregar cliente_fd a una lista de suscribers
		//Enviar confirmacion
		//Enviar todos los mensajes anteriores de la cola $TIPO
		//Dejo abierto este hilo a la espera de que haya un nuevo mensaje en la cola $TIPO
		//Entro en while 1 y recorro constantemente la cola $TIPO hasta que haya un mensaje que tenga en cliente_fd un ack false.
		//En caso de existir mensaje, hago send y vuelvo al while 1
		
	}
	else if (cod_op == ACK){
		msg = recibir_mensaje(cliente_fd, &size);
		//en msg viene id, correlation_id y queue_id
		//cambiar estado de ack a true del mensaje en suscribers[queue_id][cliente_fd].sendes.ack
		//En caso de que esten todos los mensajes enviados
		pthread_exit(NULL);
	}
	else if (cod_op == 0 || cod_op == -1  ){
		pthread_exit(NULL);

	}
	send(cliente_fd, (const void*) item->correlation_id, sizeof(item->correlation_id), 0);
	queue_push(queues[cod_op -1], item);
	free(msg);
}


void* recibir_mensaje(int socket_cliente, int* size)
{
	void * buffer;

	recv(socket_cliente, size, sizeof(int), MSG_WAITALL);
	buffer = malloc(*size);
	recv(socket_cliente, buffer, *size, MSG_WAITALL);

	return buffer;
}

int get_id(){
	//PROGRAMAME
	return 10;
}

int get_correlation_id(){
	//PROGRAMAME
	return 15;
}

//ESTO TAMBIEN HAY QUE CODEARLO



void iniciar_broker(void){
	logger = log_create("broker.log","broker",1,LOG_LEVEL_INFO);
	config = config_create("broker.config");
	build_queues();
	build_suscribers();
	start_sender_thread();
	log_info(logger,"BROKER START!");
	
}


void build_queues(void){
	for (int i = 0; i < 6; i++)
		queues[i] =	queue_create();
}
void build_suscribers(void){
	//Aca hay que levantar de la memoria todos los suscribers y generar suscribers 
}

void start_sender_thread(void){
	//Aca hay que hacer pthread create de sender thread
}
void sender_thread(void){
	while (1){
		for (int i = 0; i < 6; i++){
			for (int j = 0; j < sizeof(suscribers)/sizeof(suscriber); j++){
				for (int k = 0; k < sizeof(suscribers[i][j].sended)/sizeof(sent); k++){
					if (suscribers[i][j].sended[k].ack){
						//Send a suscribers[i][j].cliente_fd el mensaje con id suscribers[i][j].sended[k].id ;
					}
					
				}
				
				
			}
			
			
		}
		
	}
	
}

void terminar_broker( t_log* logger, t_config* config){
	for (int i = 0; i < 6; i++)
		queue_destroy(queues[i]);
	log_destroy(logger);
	config_destroy(config);
}

