#include "utils.h"
t_log* iniciar_logger(const char * process){
    strcat(process, ".log")
	return log_create("gameboy.log",process,1,LOG_LEVEL_INFO);
}

t_config* leer_config(void){
	return config_create("gameboy.config");
}