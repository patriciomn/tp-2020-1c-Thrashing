#include"broker.h"

int main(){	
    iniciar_broker();
	log_info(logger,"creating server");
	iniciar_servidor();

	terminar_broker( logger, config);
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

	unsigned int tam_direccion = sizeof(struct sockaddr_in);
	log_info(logger,"Waiting Client");
	int socket_cliente = accept(socket_servidor, (void*) &dir_cliente, &tam_direccion);
	log_info(logger,"Client received");
	pthread_create(&thread,NULL,(void*)serve_client,&socket_cliente);
	pthread_detach(thread);

}


void serve_client(int* socket)
{
	int cod_op;

	if(recv(*socket, &cod_op, sizeof(int), MSG_WAITALL) == -1){
		log_error(logger,"ERROR: socket error" );
		pthread_exit(NULL);	
	}
		
	if (cod_op == 0 || cod_op == -1  ){
		log_error(logger,"ERROR: Bad operation code" );
		pthread_exit(NULL);
	}	
	log_info(logger,"Enter to process request, cod_op: %d", cod_op);
	process_request(cod_op, *socket);
}



void process_request(int cod_op, int cliente_fd) {
	int size;
	void* msg;		
	queue_item *item = malloc(sizeof(queue_item));
	void * correlation_id_str = malloc(sizeof(int));;
	
	log_info(logger,"Processing request, cod_op: %d", cod_op);
	

	if (cod_op == SUSCRITO) atender_suscripcion(msg, cliente_fd );
	
	else if (cod_op == ACK) atender_ack(msg);

	msg = recibir_mensaje(cliente_fd, &size);	

	item->id = get_id();

	memcpy(&(item->correlation_id), msg, sizeof(int));
	msg += sizeof(int);

	if (cod_op == NEW_POKEMON){
		new_pokemon* new = deserializar_new(msg);
		item->message = new;
	}
	else if (cod_op == APPEARED_POKEMON){
		appeared_pokemon* appeared = deserializar_appeared(msg);
		item->message = appeared;
	}
	else if (cod_op == CATCH_POKEMON){
		catch_pokemon* catch = deserializar_catch(msg);
		item->message = catch;
	}
	else if (cod_op == CAUGHT_POKEMON){
		caught_pokemon* caught = deserializar_caught(msg);
		item->message = caught;
	}
	else if (cod_op == GET_POKEMON){
		get_pokemon* get = deserializar_get(msg);
		item->message = get;
	}
	else if (cod_op == LOCALIZED_POKEMON){
		localized_pokemon* localized = deserializar_localized(msg);
		item->message = localized;
	}
	
	//Pushear en la cola correspondiente con id, correlationId 
	list_add(queues[cod_op -1], item);
	
	//Responder correlation_id al que me envio el req (cliente_fd)
	sprintf(correlation_id_str, "%d", item->correlation_id);
	send(cliente_fd, correlation_id_str, sizeof(item->correlation_id), 0);

	free(msg);
	pthread_exit(NULL);
}


void atender_suscripcion(void * msg, int cliente_fd ){
	int queue_id;
	int pid;
	suscriber * sus = malloc(sizeof(suscriber));
	void * confirmation = malloc(sizeof(int)*2);

	memcpy(&(queue_id), msg, sizeof(int));


	sus->cliente_fd = cliente_fd;
	sus->sended = list_create();

	pid = generate_pid();

	//Agregar cliente_fd a una lista de suscribers
	list_add(suscribers[queue_id -1], sus);

	//Enviar confirmacion
	sprintf(confirmation, "%d", ACK);
	memcpy(confirmation + sizeof(int) , &pid, sizeof(int));

	send(cliente_fd, confirmation, sizeof(int)*2, 0);

	//Enviar todos los mensajes anteriores de la cola $queue_id
	enviar_cacheados( cliente_fd,  queue_id);
		
	while(1){
		//Dejo abierto este hilo a la espera de que haya un nuevo mensaje en la cola $queue_id
		//Entro en while 1 y recorro constantemente la cola $queue_id hasta que haya un mensaje que tenga en cliente_fd un ack false.
		//En caso de existir mensaje, hago send y vuelvo al while 1
	}
		
}

void enviar_cacheados(int cliente_fd, int tipo){
	t_list* queue = list_filter(queues[tipo -1],(void*)true);

	if(list_is_empty(queue)) return;

	

	void enviar(void* ele){
		queue_item * item = ele;
		t_paquete * paquete = crear_paquete(NEW_POKEMON);
		paquete->codigo_operacion = tipo;
		paquete->buffer->id =item->id;
		paquete->buffer->correlation_id =item->correlation_id;
		paquete->buffer->stream = item->message;

		int size;
		void * a_enviar = serializar_paquete(paquete,  &size);
		
		send(cliente_fd, a_enviar, sizeof(size), 0);
	}
	list_iterate(queue,(void*)enviar);
	
}

t_paquete* crear_paquete(int op){
	t_paquete* paquete = malloc(sizeof(t_paquete));
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->codigo_operacion = op;
	return paquete;
}

void atender_ack(void *msg){
	int pid;
	int id;
	int queue_id;
	memcpy(&(pid), msg, sizeof(int));
	msg+=sizeof(int);
	memcpy(&(queue_id), msg, sizeof(int));
	msg+=sizeof(int);
	memcpy(&(id), msg, sizeof(int));
	
	
	//en msg viene id, correlation_id y queue_id
	//cambiar estado de ack a true del mensaje en suscribers[queue_id][cliente_fd].sendes.ack
	//En caso de que esten todos los mensajes enviados
	pthread_exit(NULL);
}

void* recibir_mensaje(int socket_cliente, int* size){
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
int generate_pid(){
	//PROGRAMAME
	return 1
}



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
		queues[i] =	list_create();
}
void build_suscribers(void){
	//Aca hay que levantar de la memoria todos los suscribers y generar suscribers 
}

void start_sender_thread(void){
	//Aca hay que hacer pthread create de sender thread
}
// void sender_thread(void){
// 	while (1){
// 		for (int i = 0; i < 6; i++){
// 			for (int j = 0; j < sizeof(suscribers)/sizeof(suscriber); j++){
// 				for (int k = 0; k < sizeof(suscribers[i][j].sended)/sizeof(sent); k++){
// 					if (!suscribers[i][j].sended[k].ack){
// 						//Send a suscribers[i][j].cliente_fd el mensaje con id suscribers[i][j].sended[k].id ;
// 					}
					
// 				}
				
				
// 			}
			
			
// 		}
		
// 	}
	
// }

void terminar_broker( t_log* logger, t_config* config){
	for (int i = 0; i < 6; i++)
		list_destroy(queues[i]);
	log_destroy(logger);
	config_destroy(config);
}

