#include"broker.h"

int main(){	
    iniciar_broker();
	log_info(logger,"creating server");
	iniciar_servidor();
	terminar_broker( logger, config);
}

void iniciar_servidor(void){
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

	if(recv(*socket, &cod_op, sizeof(int), MSG_WAITALL) == -1){
		log_error(logger,"ERROR: socket error" );
		pthread_exit(NULL);	
	}
		
	if (cod_op <= 0 ){
		log_error(logger,"ERROR: Bad operation code: %d", cod_op);
		pthread_exit(NULL);
	}	
	log_info(logger,"Enter to process request, cod_op: %d", cod_op);
	process_request(cod_op, *socket);
}



void process_request(int cod_op, int cliente_fd) {
	int size;
	void* msg;		
	queue_item *item = malloc(sizeof(queue_item));
	void * confirmation = malloc(sizeof(int)*2);;
	
	log_info(logger,"Processing request, cod_op: %d", cod_op);
	
	msg = recibir_mensaje(cliente_fd, &size);	

	if (cod_op == SUSCRITO) atender_suscripcion(cliente_fd, msg );
	
	else if (cod_op == ACK) atender_ack( cliente_fd,  msg);

	item->id = get_id();
	item->correlation_id = -1;
	

	if (cod_op == NEW_POKEMON){
		new_pokemon* new = deserializar_new(msg);
		item->message = new;
	}
	else if (cod_op == APPEARED_POKEMON){
		memcpy(&(item->correlation_id), msg, sizeof(int));
		msg += sizeof(int);
		appeared_pokemon* appeared = deserializar_appeared(msg);
		item->message = appeared;
	}
	else if (cod_op == CATCH_POKEMON){
		catch_pokemon* catch = deserializar_catch(msg);
		item->message = catch;
	}
	else if (cod_op == CAUGHT_POKEMON){
		memcpy(&(item->correlation_id), msg, sizeof(int));
		msg += sizeof(int);
		caught_pokemon* caught = deserializar_caught(msg);
		item->message = caught;
	}
	else if (cod_op == GET_POKEMON){
		get_pokemon* get = deserializar_get(msg);
		item->message = get;
	}
	else if (cod_op == LOCALIZED_POKEMON){
		memcpy(&(item->correlation_id), msg, sizeof(int));
		msg += sizeof(int);
		localized_pokemon* localized = deserializar_localized(msg);
		item->message = localized;
	}
	
	//Pushear en la cola correspondiente con id, correlationId 
	list_add(queues[cod_op -1], item);
	
	//Responder id al que me envio el req (cliente_fd)
	sprintf(confirmation, "%d", ACK);
	memcpy(confirmation + sizeof(int) , &item->id, sizeof(int));
	send(cliente_fd, confirmation, sizeof(item->id), 0);


	//Aca deberia levantar algun semaforo que habilite un proceso que me mande el mensaje que se agrego a la cola a los demas
	sem_post(&semSend);

	free(msg);
	free(confirmation);
	pthread_exit(NULL);
}


void atender_suscripcion(int cliente_fd,  void * msg ){
	int queue_id;
	int pid;
	
	suscriber * sus = malloc(sizeof(suscriber));
	void * confirmation = malloc(sizeof(int)*2);


	memcpy(&(queue_id), msg, sizeof(int));
	msg+=sizeof(int);
	memcpy(&(pid), msg, sizeof(int));

	log_info(logger,"Received, queue_id: %d, pid: %d", queue_id, pid);
	if (queue_id>6 || queue_id<1 ){
		log_error(logger, "Invalid queue_id %d. ", queue_id);
		pthread_exit(NULL);
	}

	if (pid == -1){
		//Esto pasa cuando entra por primera vez
		pid = generate_pid();
	}

	sus->cliente_fd = cliente_fd;
	sus->pid = pid;
	
	bool existe(suscriber* aux){
		return aux->pid == pid;
	}
	if (!list_find(suscribers[queue_id -1],(void*)existe) ){
		//Agregar cliente_fd a una lista de suscribers
		list_add(suscribers[queue_id -1], sus);
	}	

	//Enviar confirmacion
	sprintf(confirmation, "%d", ACK);
	memcpy(confirmation + sizeof(int) , &pid, sizeof(int));

	send(cliente_fd, confirmation, sizeof(int)*2, 0);

	//Enviar todos los mensajes anteriores de la cola $queue_id
	enviar_cacheados( sus,  queue_id);
		
	while(1){
		//Dejo abierto este hilo a la espera de que haya un nuevo mensaje en la cola $queue_id
		sem_wait(&semSend);
		enviar_cacheados( sus,  queue_id);
	}
		
}




void enviar_cacheados(suscriber * sus, int queue_id){
	void enviar(queue_item* queue_item){
		bool existe(suscriber * sus_aux){
			return sus_aux->pid == sus->pid;
		}
		if (!list_any_satisfy(queue_item->recibidos,(void*)existe) ){
			t_paquete * paquete = crear_paquete(queue_id, queue_item);

			int size=0;
			void * a_enviar = serializar_paquete(paquete, &size);

			send(sus->cliente_fd, a_enviar, size, 0);
			if(!list_any_satisfy(queue_item->enviados, (void*)existe)){
				list_add(queue_item->enviados, sus);
			}
		}
	}
	list_iterate(queues[queue_id -1],(void*)enviar);
	
}

t_paquete* crear_paquete(int op, queue_item * queue_item) {
	t_paquete* paquete = malloc(sizeof(t_paquete));
	paquete->codigo_operacion = op;
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size=0;
	paquete->buffer->stream = serializar_any(queue_item->message, &paquete->buffer->size, op);
	paquete->buffer->size+= sizeof(queue_item->id) *2 ;
	paquete->buffer->stream->id = queue_item->id;
	paquete->buffer->stream->correlation_id = queue_item->correlation_id;
	return paquete;

}


void atender_ack(int cliente_fd,  void * msg){
	int pid, queue_id, id;
	suscriber * sus = malloc(sizeof(suscriber));
	queue_item * new;
	//en msg viene id, correlation_id y queue_id


	memcpy(&(pid), msg, sizeof(int));
	msg+=sizeof(int);
	memcpy(&(queue_id), msg, sizeof(int));
	msg+=sizeof(int);
	memcpy(&(id), msg, sizeof(int));

	sus->pid = pid;
	sus->cliente_fd = cliente_fd;

	bool existe(queue_item* aux){
		return aux->id == id;
	}
	new = list_find(queues[queue_id -1],(void*)existe);
	list_remove_by_condition(queues[queue_id -1], (void*)existe);
	list_add(new->recibidos, sus);
	list_add(queues[queue_id -1], new);
	
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
	return 1;
}



void iniciar_broker(void){
	logger = log_create("broker.log","broker",1,LOG_LEVEL_INFO);
	config = config_create("broker.config");
	build_queues();
	build_suscribers();
	sem_init(&semSend,0,1);
	start_sender_thread();
	log_info(logger,"BROKER START!");
	
}


void build_queues(void){
	for (int i = 0; i < 6; i++)
		queues[i] =	list_create();
}
void build_suscribers(void){
	for (int i = 0; i < 6; i++)
		suscribers[i] =	list_create();
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

