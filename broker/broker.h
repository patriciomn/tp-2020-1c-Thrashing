#include"../utils/utils.h"


#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<commons/log.h>
#include<math.h>
#include<sys/socket.h>
#include<unistd.h>
#include<netdb.h>
#include<commons/collections/list.h>
#include<commons/collections/queue.h>
#include<commons/config.h>
#include<commons/string.h>
#include<pthread.h>
#include<assert.h>
#include<signal.h>

t_log* logger;
t_config* config;
int conexion;

void terminar_broker(int conexion, t_log* logger, t_config* config);
void iniciar_broker(void);