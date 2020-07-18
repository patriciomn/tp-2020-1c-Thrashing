#include "gamecard.h"

t_log *logger;
t_bitarray *bitarray;			// variable global bitmap, para manejar el bitmap siempre utilizamos esta variable
t_config *config_tall_grass;
char *ruta_archivo_bitmap;
void *bitmap_memoria;	// puntero para el mmap

ack_t acks_gamecard;

FILE *bitmapFilePointer;

// Los siguientes structs son para recibir mensajes del gameboy
struct metadata_info *metadataTxt; // este puntero es para el metadata.txt que ya existe en el fs
struct config_tallGrass *datos_config; // este struct es para almacenar los datos de las config. Tambien lo utilizamos para setear los valores de metadata.txt cuando se crea la primera vez

// Hilos para gamecard
pthread_t servidor_gamecard; // este hilo es para iniciar el gamecard como servidor e interacturar con gameboy si el broker cae
pthread_t cliente_gamecard; // este hilo es para iniciar gamecard como cliente del broker
pthread_t thread_new_pokemon;		// hilo para recibir mensajes de la cola new_pokemon
pthread_t thread_catch_pokemon;		// hilo para recibir mensajes de la cola catch_pokemon
pthread_t thread_get_pokemon;		// hilo para recibir mensajes de la cola get_pokemon

pthread_mutex_t mutexNEW = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexGET = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexCATCH = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t mutexSemaforo = PTHREAD_MUTEX_INITIALIZER; // mutex para acceder a los semaforos

pthread_mutex_t mxTallgrassMeta = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexBITMAP = PTHREAD_MUTEX_INITIALIZER;
t_list* sem_MetadataPoke; // esto seria para los metadata de los pokemon. opera con semaphore.h

t_dictionary *diccionario_semaforos;

int main () {

	iniciar_gamecard();

	signal(SIGINT,terminar_gamecard);
    return 0;
}

void iniciar_gamecard() {

	iniciar_logger_config();

	obtener_datos_archivo_config();

	verificar_punto_de_montaje();

	suscripcion_colas_broker();

	pthread_create(&servidor_gamecard, NULL, (void *)iniciar_servidor, NULL);
	pthread_join(servidor_gamecard, NULL);
}

void terminar_gamecard(int sig){
    munmap(bitmap_memoria, datos_config->blocks / BITS);
    pthread_cancel(thread_get_pokemon);
    pthread_cancel(thread_catch_pokemon);
    pthread_cancel(thread_new_pokemon);
    pthread_cancel(servidor_gamecard);
    bitarray_destroy(bitarray);
    log_destroy(logger);
    config_destroy(config_tall_grass);
    free(metadataTxt);
    free(datos_config->ip_broker);
    free(datos_config->pto_de_montaje);
    free(datos_config);
}

/*
void mostrar_bitmap() {

	int j = 1;

	for(int i = 1 ; i <= bitarray_get_max_bit(bitarray) ; i+=7) {

		if(i % 8 == 0) {
			j++;
		}

		printf("%d) %d %d %d %d %d %d %d %d \n", j, bitarray_test_bit(bitarray, i), bitarray_test_bit(bitarray, i + 1), bitarray_test_bit(bitarray, i + 2), bitarray_test_bit(bitarray, i + 3) ,bitarray_test_bit(bitarray, i + 4), bitarray_test_bit(bitarray, i + 5), bitarray_test_bit(bitarray, i + 6), bitarray_test_bit(bitarray, i + 7));
	}
}

*/

void verificar_punto_de_montaje() {
	DIR *rta = opendir(datos_config->pto_de_montaje);
	if(rta == NULL) {
        // si no existe, se crea
		log_error(logger,"DIRECTORIO %s NO EXISTE",datos_config->pto_de_montaje);
	    log_info(logger,"CREANDO DIRECTORIO %s",datos_config->pto_de_montaje);
	    crear_pto_de_montaje(datos_config->pto_de_montaje);
	} else {
	    log_info(logger,"DIRECTORIO %s ENCONTRADO",datos_config->pto_de_montaje);
	    //verificamos el bitmap, el archivo metadata.txt, los archivos pokemones, entre otras cosas.
	    verificar_metadata_fs(datos_config->pto_de_montaje);
	}
	closedir(rta);
}


void iniciar_logger_config() {

	diccionario_semaforos = dictionary_create();

	datos_config = malloc(sizeof(struct config_tallGrass));
	metadataTxt = malloc(sizeof(struct metadata_info));

	logger = log_create("Tall_Grass_Logger","TG",1,LOG_LEVEL_INFO);
	config_tall_grass = config_create("gamecard.config");
}


void obtener_datos_archivo_config() {

	datos_config->tiempo_reintento_conexion = config_get_int_value(config_tall_grass, "TIEMPO_DE_REINTENTO_CONEXION");
	datos_config->tiempo_reintento_operacion = config_get_int_value(config_tall_grass,"TIEMPO_DE_REINTENTO_OPERACION");
	datos_config->tiempo_retardo_operacion = config_get_int_value(config_tall_grass, "TIEMPO_RETARDO_OPERACION");

	int size_ip = strlen(config_get_string_value(config_tall_grass, "IP_BROKER")) + 1;
	datos_config->ip_broker = malloc(size_ip);
	memcpy(datos_config->ip_broker, config_get_string_value(config_tall_grass, "IP_BROKER"), size_ip);

	datos_config->puerto_broker = config_get_int_value(config_tall_grass, "PUERTO_BROKER");

	int size_pto_montaje = strlen(config_get_string_value(config_tall_grass, "PUNTO_MONTAJE_TALLGRASS")) + 1;
	datos_config->pto_de_montaje = malloc(size_pto_montaje);
	memcpy(datos_config->pto_de_montaje, config_get_string_value(config_tall_grass, "PUNTO_MONTAJE_TALLGRASS"), size_pto_montaje);

	datos_config->blocks = config_get_int_value(config_tall_grass, "BLOCKS");
	datos_config->size_block = config_get_int_value(config_tall_grass, "SIZE_BLOCK");

	datos_config->pid = config_get_int_value(config_tall_grass, "PID");
}


void verificar_metadata_fs(char *path_pto_montaje) {

    int size_new_string = strlen(path_pto_montaje) + strlen(METADATA_TXT_PATH) + 1;
    char *path_metadata_txt = malloc(size_new_string);
    strcpy(path_metadata_txt, path_pto_montaje);
    string_append(&path_metadata_txt, METADATA_TXT_PATH);

    log_info(logger, "ABRIENDO METADATA FS CON RUTA <%s>", path_metadata_txt);
    FILE *filePointer = fopen(path_metadata_txt,"r");
    if(filePointer == NULL) {
        log_error(logger,"ERROR AL ABRIR EL ARCHIVO metadata.txt");
        exit(1);
    }

    //obtenemos los valores del archivo metadata.txt
    t_config *metadata_txt_datos = config_create(path_metadata_txt);
    metadataTxt->block_size = config_get_int_value(metadata_txt_datos,"BLOCK_SIZE");
    log_info(logger,"OBTENIENDO BLOCK_SIZE: %d",metadataTxt->block_size);
    metadataTxt->blocks = config_get_int_value(metadata_txt_datos,"BLOCKS");
    log_info(logger,"OBTENIENDO CANTIDAD DE BLOCKS: %d",metadataTxt->blocks);
	
    config_destroy(metadata_txt_datos);
    fclose(filePointer);
    free(path_metadata_txt);

    // creamos un nuevo string para armar el path del bitmap
    size_new_string = strlen(path_pto_montaje) + strlen(BITMAP_PATH) + 1;
    char *path_bitmap_bin = malloc(size_new_string);
    strcpy(path_bitmap_bin, path_pto_montaje);
    string_append(&path_bitmap_bin, BITMAP_PATH);

    int sizeBitmap = fileSize(path_bitmap_bin);

    log_info(logger, "TAMAÑO DEL BITMAP: %d", sizeBitmap);

    int fd = open(path_bitmap_bin, O_RDWR , (mode_t)0600);
    if(fd != -1) {
    	bitmap_memoria = mmap(0, sizeBitmap, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    	if(bitmap_memoria == MAP_FAILED) {
    		log_error(logger, "ERROR AL CARGAR BITMAP A MEMORIA");
    		close(fd);
    		exit(1);
    	}

    close(fd);

    bitarray = bitarray_create_with_mode(bitmap_memoria, sizeBitmap, LSB_FIRST);

    }

    free(path_bitmap_bin);
}

// Funciones para crear el pto de montaje y otros directorios

void crear_pto_de_montaje(char *path_pto_montaje) {
    int rta_mkdir = mkdir(path_pto_montaje,S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    if(rta_mkdir == 0) {
        log_info(logger,"DIRECTORIO <%s> CREADO", path_pto_montaje);
        crear_metadata_tall_grass(path_pto_montaje);
        crear_directorio_files(path_pto_montaje);
        crear_directorio_blocks(path_pto_montaje);
    } else {
        log_error(logger,"ERROR AL CREAR EL DIRECTORIO <%s>",path_pto_montaje);
    }
}


void crear_directorio_files(char *path_pto_montaje) {
    int size_new_string = strlen(path_pto_montaje) + strlen(FILES_DIR) + 1;
    char *path_files = malloc(size_new_string);
    strcpy(path_files, path_pto_montaje);
    string_append(&path_files, FILES_DIR);

    int rta_mkdir = mkdir(path_files, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    if(rta_mkdir == 0) {
        log_info(logger,"DIRECTORIO <%s> CREADO", path_files);
    } else {
        log_error(logger,"ERROR AL CREAR EL DIRECTORIO <%s>",path_files);
    }

    free(path_files);
}


void crear_directorio_blocks(char *path_pto_montaje) {
    int size_new_string = strlen(path_pto_montaje) + strlen(BLOCKS_DIR) + 1;
    char *path_blocks = malloc(size_new_string);
    strcpy(path_blocks, path_pto_montaje);
    string_append(&path_blocks, BLOCKS_DIR);

    int rta_mkdir = mkdir(path_blocks, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    if(rta_mkdir == 0) {
        log_info(logger,"DIRECTORIO <%s> CREADO", path_blocks);
    } else {
        log_error(logger,"ERROR AL CREAR EL DIRECTORIO <%s>",path_blocks);
    }

    free(path_blocks);
}


void crear_metadata_tall_grass(char *path_pto_montaje) { // TODO incluir en bitmap, supongo que en el primer bit
    int size_new_string = strlen(path_pto_montaje) + strlen(METADATA_DIR) + 1;
    char *path_metadata = malloc(size_new_string);
    strcpy(path_metadata, path_pto_montaje);
    string_append(&path_metadata, METADATA_DIR);

    int rta_mkdir = mkdir(path_metadata,S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    if(rta_mkdir == 0) {
        log_info(logger,"DIRECTORIO <%s> CREADO",path_metadata);
        crear_archivos_metadata(path_metadata);
    } else {
        log_error(logger,"ERROR AL CREAR EL DIRECTORIO <%s>",path_metadata);
    }

    free(path_metadata);
}


void crear_archivos_metadata(char *path_metadata) {

    //creamos un nuevo string que va a ser el path del metadata.txt
	int size_new_string = strlen(path_metadata) + strlen(METADATA_FILE) + 1;
    char *path_metadata_file = malloc(size_new_string);
    strcpy(path_metadata_file, path_metadata);
    string_append(&path_metadata_file, METADATA_FILE);
    FILE *metadataPointerFile = fopen(path_metadata_file,"w+");
    if(metadataPointerFile == NULL) {
    	log_error(logger, "ERROR AL CREAR EL ARCHIVO metadata.txt");
    	exit(1);
    }

    char *info_metadataTxt = string_new();
    string_append(&info_metadataTxt, "BLOCK_SIZE=");
    char* size_block = string_itoa(datos_config->size_block);
    string_append(&info_metadataTxt, size_block);
    string_append(&info_metadataTxt, "\n");
    string_append(&info_metadataTxt, "BLOCKS=");
    char* blocks = string_itoa(datos_config->blocks);
    string_append(&info_metadataTxt, blocks);
    string_append(&info_metadataTxt, "\n");
    string_append(&info_metadataTxt, "MAGIC_NUMBER=TALL_GRASS");
    string_append(&info_metadataTxt, "\n");

    if(fwrite(info_metadataTxt, string_length(info_metadataTxt), 1, metadataPointerFile) == 0) {
    	log_error(logger, "ERROR AL ESCRIBIR EL ARCHIVO metadata.txt");
    	exit(1);	// despues ver de implementar un goto en vez de exit()
    }

    fclose(metadataPointerFile);

    //creamos un nuevo string que va a ser el path del bitmap.bin
    size_new_string = strlen(path_metadata) + strlen(BITMAP_FILE) + 1; // uso la misma variable que utilice en metadata.txt
    char *ruta_archivo_bitmap = malloc(size_new_string);
    strcpy(ruta_archivo_bitmap, path_metadata);
    string_append(&ruta_archivo_bitmap, BITMAP_FILE);

    int size_bytes_bitmap = (datos_config->blocks / BITS);

    int fd = open (ruta_archivo_bitmap, O_RDWR | O_CREAT | O_APPEND,  S_IRUSR | S_IWUSR);
    write(fd, "", size_bytes_bitmap);

    bitmap_memoria = mmap(0, size_bytes_bitmap, PROT_WRITE, MAP_SHARED, fd, 0);

    bitarray = bitarray_create_with_mode(bitmap_memoria, size_bytes_bitmap, LSB_FIRST);

    close(fd);

    // limpiamos el bitarray
    for(int i = 0 ; i < bitarray_get_max_bit(bitarray) ; i++) {
    	bitarray_clean_bit(bitarray,i);
    }

    free(size_block);
    free(blocks);
    free(path_metadata_file);
    free(info_metadataTxt);
    free(ruta_archivo_bitmap);
}


char* crear_directorio_pokemon(char *path_pto_montaje, char* pokemon) {
		
    int size_new_string = strlen(path_pto_montaje) + strlen(POKEMON_DIR)+strlen(pokemon) + 1;
    char *path_files = malloc(size_new_string);
    strcpy(path_files, path_pto_montaje);
    string_append(&path_files, POKEMON_DIR);
	string_append(&path_files, pokemon);


    int rta_mkdir = mkdir(path_files, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    if(rta_mkdir == 0) {
        log_info(logger,"DIRECTORIO <%s> CREADO", path_files);
    } else {
        log_error(logger,"ERROR AL CREAR EL DIRECTORIO <%s>",path_files);
    }
	return path_files;
    free(path_files);
}

//----------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------
// Operacion NEW POKEMON

void operacion_new_pokemon(new_pokemon *newPokemon) {

	pthread_mutex_lock(&mutexSemaforo);
	key_semaforo_presente(newPokemon->name);
	pthread_mutex_unlock(&mutexSemaforo);

    char *path_directorio_pokemon = string_new();
    string_append_with_format(&path_directorio_pokemon, "%s%s%s%s",datos_config->pto_de_montaje, FILES_DIR, "/", newPokemon->name);
    log_info(logger, "EN BUSCA DEL DIRECTORIO DEL POKEMON CON RUTA <%s>", path_directorio_pokemon);
    DIR* dir = opendir(path_directorio_pokemon);
    LOOP:
    waitSemaforo(newPokemon->name);

    if(dir == NULL) {

    	log_warning(logger, "EL DIRECTORIO <%s> NO EXISTE", path_directorio_pokemon);

    	pthread_mutex_lock(&mutexBITMAP);
    	int nro_bloque_libre = obtener_bloque_libre();
    	pthread_mutex_unlock(&mutexBITMAP);

    	if(nro_bloque_libre == -1) {
    		signalSemaforo(newPokemon->name);
    		log_error(logger, "NO HAY SUFICIENTE ESPACIO DISPONIBLE");
    		pthread_exit(NULL);

    	} else {

    		log_info(logger, "CREANDO EL DIRECTORIO <%s>", path_directorio_pokemon);
    		mkdir(path_directorio_pokemon,S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

    		crear_pokemon(newPokemon, path_directorio_pokemon, nro_bloque_libre);

    		signalSemaforo(newPokemon->name);

    	}

    } else {

    	//LOOP:

    	signalSemaforo(newPokemon->name);

    	waitSemaforo(newPokemon->name);
    	char *valor = get_valor_campo_metadata(path_directorio_pokemon, "DIRECTORY");

    	if(string_equals_ignore_case(valor, "Y")) {

    		log_info(logger, "EL DIRECTORIO <%s> EXISTE Y ES SOLO UN DIRECTORIO", path_directorio_pokemon);

    		pthread_mutex_lock(&mutexBITMAP);
    		int nro_bloque_libre = obtener_bloque_libre();
    		pthread_mutex_unlock(&mutexBITMAP);

    		if(nro_bloque_libre == -1) {

    		    log_error(logger, "NO HAY SUFICIENTE ESPACIO DISPONIBLE");

    		    pthread_exit(NULL);
     		    signalSemaforo(newPokemon->name);

    		} else {

    		    crear_pokemon(newPokemon, path_directorio_pokemon, nro_bloque_libre);

    		    signalSemaforo(newPokemon->name);

    		}

    	} else {



			log_info(logger, "EL DIRECTORIO <%s> EXISTE JUNTO CON EL ARCHIVO POKEMON", path_directorio_pokemon);
			signalSemaforo(newPokemon->name);
    		waitSemaforo(newPokemon->name);
    		char *valor_open = get_valor_campo_metadata(path_directorio_pokemon, "OPEN");

    		if(string_equals_ignore_case(valor_open, "N")) {

    			cambiar_valor_metadata(path_directorio_pokemon, "OPEN", "Y");
    			signalSemaforo(newPokemon->name);
    			buscar_linea_en_el_archivo(newPokemon, path_directorio_pokemon);
    			signalSemaforo(newPokemon->name);

    		} else {

    			reintentar_operacion(newPokemon);
    			signalSemaforo(newPokemon->name);
    			goto LOOP;
    		}
    		free(valor_open);
    	}
    	free(valor);
    }
    closedir(dir);
    free(path_directorio_pokemon);
    free(newPokemon->name);
	free(newPokemon);
}

void reintentar_operacion(void *pokemon) {

	log_warning(logger, "HILO EN REINTENTO DE OPERACION");
	sleep(datos_config->tiempo_reintento_operacion);
	log_info(logger, "HILO RETOMANDO LA OPERACION");
}


void buscar_linea_en_el_archivo(new_pokemon *newPokemon, char *path_directorio_pokemon) { // verificamos si existe o no la linea en el archivo existente

	char *path_archivo_pokemon = string_new();
	string_append(&path_archivo_pokemon, path_directorio_pokemon);
	string_append_with_format(&path_archivo_pokemon, "%s%s%s", "/", newPokemon->name, POKEMON_FILE_EXT);

	int tamanioArchivo = fileSize(path_archivo_pokemon);

	log_info(logger, "ABRIENDO ARCHIVO CON RUTA <%s>", path_archivo_pokemon);

	int fdPokemon = open(path_archivo_pokemon, O_RDWR);

	char *file_memory = mmap(0, tamanioArchivo, PROT_READ | PROT_WRITE, MAP_SHARED, fdPokemon, 0);

	char *linea = string_new();
	char* posx = string_itoa(newPokemon->pos.posx);
	char* posy =  string_itoa(newPokemon->pos.posy);
	string_append_with_format(&linea, "%s%s%s",posx , GUION,posy);

	if(string_contains(file_memory, linea)) { // buscar en el archivo y modificar

		//log_info(logger, "COORDENADA <%s> ENCONTRADO", linea);
		//log_info(logger, "LINEA <%s>", linea);
		modificar_linea_en_archivo(file_memory, newPokemon, path_directorio_pokemon, linea);

	} else { // se agrega al final del archivo

		//log_info(logger, "COORDENADA <%s> NO ENCONTRADO!", linea);
		char* cant = string_itoa(newPokemon->cantidad);
		string_append_with_format(&linea, "%s%s%s", IGUAL, cant, "\n");
		//log_info(logger, "LINEA <%s>", linea);
		munmap(file_memory, tamanioArchivo);
		insertar_linea_en_archivo(newPokemon, path_directorio_pokemon, linea);
		enviar_respuesta_new_pokemon(newPokemon);
		free(cant);
	}

	free(path_archivo_pokemon);
	free(linea);
	free(posx);
	free(posy);
}


void insertar_linea_en_archivo(new_pokemon *newPokemon, char *path_directorio_pokemon, char *linea) { // si hay espacio en el ultimo bloque, se escribe en la ultima posicion del archivo. Caso contrario se busca un nuevo bloque

	char *path_archivo_pokemon = string_new();
	string_append(&path_archivo_pokemon, path_directorio_pokemon);
	string_append_with_format(&path_archivo_pokemon, "%s%s%s", "/", newPokemon->name, ".txt");

	if(hay_espacio_ultimo_bloque(path_directorio_pokemon, linea)) {

		escribir_archivo(path_archivo_pokemon, linea, "a");

		int ultimo_bloque = ultimo_bloque_array_blocks(path_directorio_pokemon);
		escribir_block(ultimo_bloque, linea);

		// actualizar size_archivo
		int viejoSize = valor_campo_size_metadata(path_directorio_pokemon);
		int nuevoSize = viejoSize + strlen(linea);
		char* aux = string_itoa(nuevoSize);
		cambiar_valor_metadata(path_directorio_pokemon, "SIZE", aux);
		retardo_operacion();
		cambiar_valor_metadata(path_directorio_pokemon, "OPEN", "N");
		free(aux);

	} else {

		// buscar un bloque para la nueva linea
		pthread_mutex_lock(&mutexBITMAP);
		int	nro_bloque_libre = obtener_bloque_libre();
		pthread_mutex_unlock(&mutexBITMAP);

		if(nro_bloque_libre == -1) {
			log_error(logger, "NO HAY SUFICIENTE ESPACIO DISPONIBLE PARA ESCRIBIR NUEVOS DATOS");
			pthread_exit(NULL);
		}

		escribir_archivo(path_archivo_pokemon, linea, "a");

		int ultimo_bloque = ultimo_bloque_array_blocks(path_directorio_pokemon);

		escribir_blocks(ultimo_bloque, nro_bloque_libre, linea);

		agregar_bloque_metadata_pokemon(path_directorio_pokemon, nro_bloque_libre);

		int viejoSize = valor_campo_size_metadata(path_directorio_pokemon);
		int nuevoSize = viejoSize + strlen(linea);
		char* aux = string_itoa(nuevoSize);
		cambiar_valor_metadata(path_directorio_pokemon, "SIZE",aux);
		retardo_operacion();
		cambiar_valor_metadata(path_directorio_pokemon, "OPEN", "N");
		free(aux);
	}
	free(path_archivo_pokemon);
}


void retardo_operacion() {

	log_info(logger, "INICIO RETARDO OPERACION");
	sleep(datos_config->tiempo_retardo_operacion);
	log_info(logger, "FIN RETARDO OPERACION");

}


void escribir_archivo(char *path_archivo, char *contenido, char *modo) {

	FILE *pfPokemon = fopen(path_archivo, modo);

	int size_line = strlen(contenido);

	log_info(logger, "ESCRIBIENDO EN EL ARCHIVO CON RUTA <%s>", path_archivo);
	if(fwrite(contenido, size_line, 1, pfPokemon) == 0) {
		log_error(logger, "NO SE PUDO ESCRIBIR EN EL ARCHIVO CON RUTA <%s>", path_archivo);
		exit(1);
	}

	fclose(pfPokemon);
}


void escribir_blocks(int ultimo_bloque, int nuevo_bloque, char *linea) { // se escribe parte en el ultimo bloque y el resto en el nuevo

	char *path_ultimo_bloque = string_new();
	char* aux =  string_itoa(ultimo_bloque);
	string_append_with_format(&path_ultimo_bloque, "%s%s%s%s%s", datos_config->pto_de_montaje, BLOCKS_DIR, "/",aux, ".txt");

	int tamanioArchivoBloque = fileSize(path_ultimo_bloque);

	int firstChars = datos_config->size_block - tamanioArchivoBloque;
	int start = 0;

	char* subPrimeros = string_substring(linea, start, firstChars);
	char *primerosChars = string_duplicate(subPrimeros); // los primeros n caracteres a agregar en el ultimo bloque
	escribir_archivo(path_ultimo_bloque, primerosChars, "a");

	start += firstChars;

	char *path_nuevo_bloque = string_new();
	char* aux1 = string_itoa(nuevo_bloque);
	string_append_with_format(&path_nuevo_bloque, "%s%s%s%s%s", datos_config->pto_de_montaje, BLOCKS_DIR, "/", aux1, ".txt");

	int size_linea = strlen(linea);

	char* sub = string_substring(linea, start, size_linea - start);
	char *restoChars = string_duplicate(sub); // los ultimos n caracteres que restan agregar
	escribir_archivo(path_nuevo_bloque, restoChars, "a");

	free(path_ultimo_bloque);
	free(restoChars);
	free(subPrimeros);
	free(primerosChars);
	free(path_nuevo_bloque);
	free(sub);
	free(aux);
	free(aux1);
}


void escribir_block(int ultimo_bloque, char *linea) { // cuando solo se modifica el ultimo bloque

	char *path_ultimo_bloque = string_new();
	char* aux = string_itoa(ultimo_bloque);
	string_append_with_format(&path_ultimo_bloque, "%s%s%s%s%s", datos_config->pto_de_montaje, BLOCKS_DIR, "/",aux, ".txt");

	escribir_archivo(path_ultimo_bloque, linea, "a");

	free(path_ultimo_bloque);
	free(aux);
}


char* get_valor_campo_metadata(char *ruta_dir_pokemon, char *campo) { // para todos los campos menos el array blocks

	char *path_metadata_pokemon = string_new();
	string_append(&path_metadata_pokemon, ruta_dir_pokemon);
	string_append(&path_metadata_pokemon, METADATA_FILE);

	t_config *metadata_pokemon = config_create(path_metadata_pokemon);

	char* aux_valor = config_get_string_value(metadata_pokemon, campo);
	char *valor = string_duplicate(aux_valor);

	config_destroy(metadata_pokemon);
	free(path_metadata_pokemon);

	return valor;
}


int valor_campo_size_metadata(char *ruta_dir_pokemon) { // se puede generalizar

	char *path_metadata_pokemon = string_new();
	string_append(&path_metadata_pokemon, ruta_dir_pokemon);
	string_append(&path_metadata_pokemon, METADATA_FILE);

	t_config *metadata_pokemon = config_create(path_metadata_pokemon);

	int size = config_get_int_value(metadata_pokemon, "SIZE");
	config_destroy(metadata_pokemon);
	free(path_metadata_pokemon);
	return size;
}


void crear_pokemon(new_pokemon *newPokemon, char *path_directorio_pokemon, int nro_bloque_libre) {

	//log_info(logger, "CREANDO EL DIRECTORIO <%s>", path_directorio_pokemon);
	//mkdir(path_directorio_pokemon,S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

	char *linea = string_new();
	char* posx = string_itoa(newPokemon->pos.posx);
	char* posy = string_itoa(newPokemon->pos.posy);
	char* cant = string_itoa(newPokemon->cantidad);
	string_append_with_format(&linea, "%s%s%s%s%s",posx , GUION,posy , IGUAL, cant);
	string_append(&linea, "\n");

	char *ruta_archivo_pokemon = string_new();
	string_append(&ruta_archivo_pokemon, path_directorio_pokemon);
	string_append_with_format(&ruta_archivo_pokemon, "%s%s%s", "/", newPokemon->name, POKEMON_FILE_EXT);

	log_info(logger, "LINEA <%s> A AGREGAR EN EL ARCHIVO <%s>", linea, ruta_archivo_pokemon);

	crear_metadata_pokemon(path_directorio_pokemon, newPokemon->name);

	escribir_archivo(ruta_archivo_pokemon, linea, "a");

	escribir_block(nro_bloque_libre, linea);

	agregar_bloque_metadata_pokemon(path_directorio_pokemon, nro_bloque_libre);

	char* size = string_itoa(strlen(linea));
	cambiar_valor_metadata(path_directorio_pokemon, "SIZE", size);

	retardo_operacion();

	cambiar_valor_metadata(path_directorio_pokemon, "OPEN", "N");

	enviar_respuesta_new_pokemon(newPokemon);

	free(size);
	free(linea);
    free(ruta_archivo_pokemon);
    free(posx);
    free(posy);
    free(cant);
}


void cambiar_valor_metadata(char *ruta_directorio_pokemon, char *campo, char *valor) {

	char *ruta_metadata_pokemon = string_new();
	string_append(&ruta_metadata_pokemon, ruta_directorio_pokemon);
	string_append(&ruta_metadata_pokemon, METADATA_FILE);

	log_info(logger, "MODIFICANDO CAMPO <%s> CON VALOR <%s> EN <%s>", campo, valor, ruta_metadata_pokemon);

	t_config *metadata_pokemon = config_create(ruta_metadata_pokemon);

	config_set_value(metadata_pokemon, campo, valor);

	config_save(metadata_pokemon);

	config_destroy(metadata_pokemon);

	free(ruta_metadata_pokemon);
}


// nos devuelve el nro de bloque libre del bitmap
int obtener_bloque_libre() {
	int bloque = -1;
	for(int i = 0 ; i < bitarray_get_max_bit(bitarray) ; i++ ) {
		if(bitarray_test_bit(bitarray, i) == 0) {
			log_info(logger, "NUMERO DE BLOQUE LIBRE:%d",i);
			log_info(logger, "ACTUALIZANDO BITARRAY");
			bitarray_set_bit(bitarray, i);
			bloque = i;
			break;
		}
	}

	log_info(logger, "ACTUALIZANDO BITMAP");
	if(msync(bitmap_memoria, datos_config->blocks / BITS, MS_SYNC) == -1) {
		log_error(logger, "ERROR MSYNC BITMAP BITARRAY SET");
		exit(1);
	}

	return bloque;
}


bool hay_espacio_ultimo_bloque(char *path_dir_pokemon, char *datos_a_agregar) { // chequea si se puede agregar los datos en el ultimo bloque

	char *path_metadata_file = string_new();
	string_append(&path_metadata_file, path_dir_pokemon);
	string_append(&path_metadata_file, METADATA_FILE);

	int ultimo_bloque = ultimo_bloque_array_blocks(path_dir_pokemon);
	//log_info(logger, "ultimo bloque: %d", ultimo_bloque);

	char *path_block = string_new();
	string_append(&path_block, datos_config->pto_de_montaje);
	string_append(&path_block, BLOCKS_DIR);
	char* aux =  string_itoa(ultimo_bloque);
	string_append_with_format(&path_block, "%s%s%s", "/",aux, TXT_FILE_EXT);

	int file_size_block = fileSize(path_block);

	int data_size = strlen(datos_a_agregar);

	int nuevoTamanio = file_size_block + data_size; // el tamaño actual del archivo bloque + el tamaño de la linea a agregar

	free(path_metadata_file);
	free(path_block);
	free(aux);

	if(datos_config->size_block >= nuevoTamanio) {
		log_info(logger, "SE PUEDE AGREGAR LA LINEA EN EL ULTIMO BLOQUE [%d]", ultimo_bloque);
		return true;
	} else {
		log_warning(logger, "SE NECESITA UN NUEVO BLOQUE PARA LA ESCRITURA DE DATOS");
		return false;
	}
}


int ultimo_bloque_array_blocks(char *path_directorio_pokemon) { // nos devuelve el ultimo bloque del array blocks de la metadata

	int size_new_string = strlen(path_directorio_pokemon) + strlen(METADATA_FILE) + 1;
	char *path_metadata_file = malloc(size_new_string);
	strcpy(path_metadata_file, path_directorio_pokemon);
	string_append(&path_metadata_file, METADATA_FILE);

	t_config *metadata_pokemon = config_create(path_metadata_file);
	char **array_blocks = config_get_array_value(metadata_pokemon, "BLOCKS");

	int i = 0;

	while(array_blocks[i] != NULL) {
		i++;
	}

	int res =  atoi(array_blocks[i - 1]);

	int j=0;
	while(array_blocks[j] != NULL) {
		free(array_blocks[j]);
		j++;
	}

	free(array_blocks);
	config_destroy(metadata_pokemon);
	free(path_metadata_file);

	return res;
}


void agregar_bloque_metadata_pokemon(char *ruta_directorio_pokemon, int nro_bloque) { // agregar un nro de bloque al campo blocks de la metadata

	char* size = string_itoa(nro_bloque);
	int size_nro_bloque = strlen(size);

	int size_new_string = strlen(ruta_directorio_pokemon) + strlen(METADATA_FILE) + 1;
	char *ruta_metadata_pokemon = malloc(size_new_string);
	strcpy(ruta_metadata_pokemon, ruta_directorio_pokemon);
	string_append(&ruta_metadata_pokemon, METADATA_FILE);

	t_config *metadata_pokemon = config_create(ruta_metadata_pokemon);

	char *blocks = config_get_string_value(metadata_pokemon, "BLOCKS");
	int sizeBlocks = strlen(blocks);

	if(sizeBlocks == 2) { // si esta vacio el array de blocks
		sizeBlocks += size_nro_bloque;
	} else {
		sizeBlocks += size_nro_bloque + 1; // mas uno por la coma
	}

	log_info(logger, "AGREGANDO BLOQUE [%d] A LA ESTRUCTURA BLOCKS EN <%s>", nro_bloque, ruta_metadata_pokemon);
	char *newBlocks = malloc(sizeBlocks + 1);
	int posicion = 0;

	if(strlen(blocks) < 3) {
		strcpy(newBlocks + posicion, "[");
		posicion ++;
		char* nro =  string_itoa(nro_bloque);
		strcpy(newBlocks + posicion,nro);
		posicion += size_nro_bloque;
		strcpy(newBlocks + posicion, "]");
		free(nro);
	} else {
		char* sub = string_substring(blocks, 1, strlen(blocks) - 2);
		char *auxiliar = strdup(sub); // se resta 2 por los corchetes []
		strcpy(newBlocks + posicion, "[");
		posicion ++;
		strcpy(newBlocks + posicion, auxiliar);
		posicion += strlen(auxiliar);
		strcpy(newBlocks + posicion, ",");
		posicion ++;
		char* nro = string_itoa(nro_bloque);
		strcpy(newBlocks + posicion,nro );
		posicion += size_nro_bloque;
		strcpy(newBlocks + posicion, "]");
		free(auxiliar);
		free(sub);
		free(nro);
	}

	config_set_value(metadata_pokemon, "BLOCKS", newBlocks);
	config_save(metadata_pokemon);
	config_destroy(metadata_pokemon);
	free(newBlocks);
	free(ruta_metadata_pokemon);
	free(size);
}


// No hay memleak (funciona bien)
void crear_metadata_pokemon(char *ruta_directorio_pokemon, char *pokemon) {

    //creamos un nuevo string que va a ser el path del metadata.txt
	char *ruta_metadata_pokemon = string_new();
	string_append(&ruta_metadata_pokemon, ruta_directorio_pokemon);
    string_append(&ruta_metadata_pokemon, METADATA_FILE);

    log_info(logger, "CREANDO METADATA CON RUTA <%s>", ruta_metadata_pokemon);
    FILE *metadataPointerFile = fopen(ruta_metadata_pokemon, "w+");
    if(metadataPointerFile == NULL) {
    	log_error(logger, "ERROR AL CREAR EL ARCHIVO CON RUTA <%s>", ruta_metadata_pokemon);
    	exit(1);
    }

    char *campos_metadataTxt = string_new();
    string_append(&campos_metadataTxt, "DIRECTORY=N");
    string_append(&campos_metadataTxt, "\n");
    string_append(&campos_metadataTxt, "SIZE=0");
    string_append(&campos_metadataTxt, "\n");
    string_append(&campos_metadataTxt, "BLOCKS=[]");
    string_append(&campos_metadataTxt, "\n");
    string_append(&campos_metadataTxt, "OPEN=Y");
    string_append(&campos_metadataTxt, "\n");
	
    if(fwrite(campos_metadataTxt, string_length(campos_metadataTxt), 1, metadataPointerFile) == 0) {
    	log_error(logger, "ERROR AL ESCRIBIR EL ARCHIVO CON RUTA <%s>", ruta_metadata_pokemon);
    	exit(1);
    }
	
    free(campos_metadataTxt);
    fclose(metadataPointerFile);
    free(ruta_metadata_pokemon);
}


void modificar_linea_en_archivo(char* file_memory, new_pokemon *newPokemon, char *ruta_directorio_pokemon, char *coordenada) {

	char *ruta_archivo_pokemon = string_new();
	string_append(&ruta_archivo_pokemon, ruta_directorio_pokemon);
	string_append_with_format(&ruta_archivo_pokemon, "%s%s%s", "/", newPokemon->name, ".txt");

	int tamArchivo = fileSize(ruta_archivo_pokemon);

	int posicionLinea = get_posicion_linea_en_archivo(coordenada, file_memory);

	int tamanioLineaVieja = get_tamanio_linea(file_memory + posicionLinea);
	//log_info(logger, "tamanio vieja linea: %d", tamanioLineaVieja);

	char* sub = string_substring(file_memory, posicionLinea, tamanioLineaVieja);
	char *viejaLinea = string_duplicate(sub);
	log_info(logger, "LINEA ANTES: %s", viejaLinea);

	char *lineaActualizada = get_linea_nueva_cantidad(viejaLinea, coordenada, newPokemon->cantidad); // devuelve la linea actualizada pero con el \n
	log_info(logger, "LINEA DESPUES: %s", lineaActualizada);

	int diferencia = strlen(lineaActualizada) - tamanioLineaVieja;
	//log_info(logger, "diferencia vieja y nueva: %d", diferencia);

	if(diferencia == 0) {

		log_info(logger, "ACTUALIZANDO ARCHIVO <%s>", ruta_archivo_pokemon);
		memcpy(file_memory + posicionLinea, lineaActualizada, strlen(lineaActualizada));
		actualizar_contenido_blocks(ruta_directorio_pokemon, file_memory);

		if(msync(file_memory, tamArchivo, MS_SYNC) == -1) {
				log_error(logger, "ERROR MSYNC EN ARHCIVO <%s>", ruta_archivo_pokemon);
				exit(1);
		}

		munmap(file_memory, tamArchivo);
		retardo_operacion();
		cambiar_valor_metadata(ruta_directorio_pokemon, "OPEN", "N");
		enviar_respuesta_new_pokemon(newPokemon);

	} else {

		if(hay_espacio_ultimo_bloque(ruta_directorio_pokemon, lineaActualizada)) { // si hay espacio en el ultimo bloque, se reescribe en el mismo

			modificar_linea_pokemon(file_memory, viejaLinea, lineaActualizada, posicionLinea, ruta_directorio_pokemon, newPokemon->name);

			enviar_respuesta_new_pokemon(newPokemon);

		} else { // buscamos un bloque libre y si lo hay, se lo asignamos a la metadata

			pthread_mutex_lock(&mutexBITMAP);
			int nuevo_nro_bloque = obtener_bloque_libre();
			pthread_mutex_unlock(&mutexBITMAP);

			if(nuevo_nro_bloque == -1) {
				log_error(logger, "NO HAY SUFICIENTE ESPACIO PARA ALMACENAR NUEVOS DATOS");
				pthread_cancel(thread_new_pokemon);
			}

			agregar_bloque_metadata_pokemon(ruta_directorio_pokemon, nuevo_nro_bloque);

			modificar_linea_pokemon(file_memory, viejaLinea, lineaActualizada, posicionLinea, ruta_directorio_pokemon, newPokemon->name);

			enviar_respuesta_new_pokemon(newPokemon);
		}
	}

	free(sub);
	free(lineaActualizada);
	free(viejaLinea);
	free(ruta_archivo_pokemon);
}


int get_posicion_linea_en_archivo(char *linea, char *mmaped) { // devuelve la posicion donde esta situada la linea dentro del mmap (se contabiliza el \n)

	int i = 0;
	int sizeLinea = 0;

	char **lineas = string_split(mmaped, "\n");

	while(!string_contains(lineas[i], linea)) {
		sizeLinea += strlen(lineas[i]) + 1; // el + 1 por el \n
		i++;
	}

	int j=0;
	while(lineas[j] != NULL){
		free(lineas[j]);
		j++;
	}

	log_info(logger, "Posicion Linea: %d", sizeLinea);

	free(lineas);
	return sizeLinea;
}


int get_tamanio_linea(char *mapped) {

	int i = 0;

	while(mapped[i] != '\n') {
		i++;
	}
	i++;

	return i++;
}


char* get_linea_nueva_cantidad(char *linea, char *coordenada, int cantidad_a_agregar) { // nos devuelve un char* que tiene la cantidad modificada

	int sizeCoordenada = strlen(coordenada) + 1; // 1 por el caracter '='

	int sizeLinea = strlen(linea);

	char* sub = string_substring(linea, sizeCoordenada, sizeLinea - sizeCoordenada);
	char *viejaCantidad = string_duplicate(sub);

	int oldCantidad = atoi(viejaCantidad);

	int nuevaCantidad = oldCantidad + cantidad_a_agregar;

	char *newCantidad = string_itoa(nuevaCantidad);

	char *lineaModificada = string_new();
	string_append_with_format(&lineaModificada, "%s%s%s%s", coordenada, IGUAL, newCantidad, "\n");

	free(sub);
	free(viejaCantidad);
	free(newCantidad);

	return lineaModificada;
}


void actualizar_contenido_blocks(char *path_directorio_pokemon, char *mapped) {

	int desplazamiento = 0;

	char **array_blocks_metadata = get_array_blocks_metadata(path_directorio_pokemon);

	int cantidadBloques = cantidad_de_bloques(array_blocks_metadata);

	for(int i = 0 ; i < cantidadBloques ; i++) {
		actualizar_bloque(mapped, desplazamiento, array_blocks_metadata[i]);
		desplazamiento += datos_config->size_block;
		free(array_blocks_metadata[i]);
	}

	free(array_blocks_metadata);
}


char** get_array_blocks_metadata(char *path_directorio_pokemon) {

	char *path_metadata_pokemon = string_new();
	string_append_with_format(&path_metadata_pokemon, "%s%s%s", path_directorio_pokemon,"/", METADATA_FILE);

	t_config *metadata_pokemon = config_create(path_metadata_pokemon);

	char** value = config_get_array_value(metadata_pokemon, "BLOCKS");

	free(path_metadata_pokemon);
	config_destroy(metadata_pokemon);

	return value;
}


int cantidad_de_bloques(char **arrayBlocks) {

	int i = 0;

	while(arrayBlocks[i] != NULL) {
		i++;
	}

	return i;
}


void actualizar_bloque(char *mapped, int desplazamiento, char *bloque) {

	log_info(logger, "ACTUALIZANDO BLOQUES");

	char * path_archivo_bloque = string_new();
	string_append_with_format(&path_archivo_bloque, "%s%s%s%s%s", datos_config->pto_de_montaje, BLOCKS_DIR, "/", bloque, ".txt");
	//log_info(logger, "RUTA ARCHIVO BLOQUE <%s>", path_archivo_bloque);

	char* sub = string_substring(mapped, desplazamiento, datos_config->size_block);
	char *contenido = string_duplicate(sub);

	escribir_archivo(path_archivo_bloque, contenido, "w+");

	free(path_archivo_bloque);
	free(contenido);
	free(sub);
}


// modifica el archivo pokemon con la nueva cantidad (sin importar si se agrando o no el tamaño)
void modificar_linea_pokemon(char *fileMemory, char *viejaLinea, char *lineaActualizada, int posicionLinea, char *path_directorio_pokemon, char *pokemon) {

	char *ruta_archivo_pokemon = string_new();
	string_append(&ruta_archivo_pokemon, path_directorio_pokemon);
	string_append_with_format(&ruta_archivo_pokemon, "%s%s%s", "/", pokemon, ".txt");

	int tamArchivo = fileSize(ruta_archivo_pokemon);

	int sizeViejaLinea = strlen(viejaLinea);

	int sizeNuevaLinea = strlen(lineaActualizada);

	int diferencia = sizeNuevaLinea - sizeViejaLinea;


	//log_info(logger, "ES NECESARIO MOVER LAS LINEAS DE DATOS");
	int desplazamiento = 0;
	// mover los bloques para adelante
	char *buffer = malloc(tamArchivo + diferencia + 1); // + 1 por el \0

	if(posicionLinea == 0) { // esta en la primera linea

		memcpy(buffer, lineaActualizada, sizeNuevaLinea);
		desplazamiento += sizeNuevaLinea;

		memcpy(buffer + desplazamiento, fileMemory + sizeViejaLinea, tamArchivo - sizeViejaLinea);
		desplazamiento += tamArchivo - sizeViejaLinea;

		buffer[desplazamiento] = '\0';

		munmap(fileMemory, tamArchivo);

		//log_info(logger, "contenido actualizado:");
		//log_info(logger, "%s", buffer);

		escribir_archivo(ruta_archivo_pokemon, buffer, "w+");

		int oldValorSize = valor_campo_size_metadata(path_directorio_pokemon);
		int newValorSize = oldValorSize + diferencia;
		char* aux = string_itoa(newValorSize);
		cambiar_valor_metadata(path_directorio_pokemon, "SIZE", aux);

		actualizar_contenido_blocks(path_directorio_pokemon, buffer);

		retardo_operacion();

		cambiar_valor_metadata(path_directorio_pokemon, "OPEN", "N");

		free(buffer);
		free(aux);

	} else if(posicionLinea + sizeViejaLinea == tamArchivo) { // esta en la ultima linea

		memcpy(buffer, fileMemory, posicionLinea);
		desplazamiento += posicionLinea;

		memcpy(buffer + desplazamiento, lineaActualizada, sizeNuevaLinea);
		desplazamiento += sizeNuevaLinea;

		buffer[desplazamiento] = '\0';

		munmap(fileMemory, tamArchivo);

		//log_info(logger, "contenido actualizado:");
		//log_info(logger, "%s", buffer);

		escribir_archivo(ruta_archivo_pokemon, buffer, "w+");

		int oldValorSize = valor_campo_size_metadata(path_directorio_pokemon);
		int newValorSize = oldValorSize + diferencia;
		char* aux = string_itoa(newValorSize);
		cambiar_valor_metadata(path_directorio_pokemon, "SIZE", aux);
		// actualizamos el valod de los bloques
		actualizar_contenido_blocks(path_directorio_pokemon, buffer);
		// cambiamos el valor de OPEN a N
		retardo_operacion();

		cambiar_valor_metadata(path_directorio_pokemon, "OPEN", "N");

		free(buffer);
		free(aux);

	} else { // esta en el medio

		memcpy(buffer, fileMemory, posicionLinea);
		desplazamiento += posicionLinea;

		memcpy(buffer + desplazamiento, lineaActualizada, sizeNuevaLinea);
		desplazamiento += sizeNuevaLinea;

		memcpy(buffer + desplazamiento, fileMemory + posicionLinea + sizeViejaLinea, tamArchivo - posicionLinea - sizeViejaLinea);
		desplazamiento += tamArchivo - posicionLinea - sizeViejaLinea;

		buffer[desplazamiento] = '\0';

		munmap(fileMemory, tamArchivo);

		//log_info(logger, "contenido actualizado:");
		//log_info(logger, "%s", buffer);

		escribir_archivo(ruta_archivo_pokemon, buffer, "w+");

		int oldValorSize = valor_campo_size_metadata(path_directorio_pokemon);
		int newValorSize = oldValorSize + diferencia;
		char* aux = string_itoa(newValorSize);
		cambiar_valor_metadata(path_directorio_pokemon, "SIZE", aux);
		actualizar_contenido_blocks(path_directorio_pokemon, buffer);
		retardo_operacion();
		cambiar_valor_metadata(path_directorio_pokemon, "OPEN", "N");
		free(aux);
	}

	//munmap(fileMemory, tamArchivo);
	free(ruta_archivo_pokemon);
}


//-------------------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------------
// catch_pokemon

void operacion_catch_pokemon(catch_pokemon *catchPokemon) {

	pthread_mutex_lock(&mutexSemaforo);
	key_semaforo_presente(catchPokemon->name);
	pthread_mutex_unlock(&mutexSemaforo);

	char *path_directorio_pokemon = string_new();
	string_append_with_format(&path_directorio_pokemon, "%s%s%s%s",datos_config->pto_de_montaje, FILES_DIR, "/", catchPokemon->name);
	log_info(logger, "EN BUSCA DEL DIRECTORIO DEL POKEMON CON PATH <%s>", path_directorio_pokemon);
        LOOP:

	waitSemaforo(catchPokemon->name);
	DIR* dir = opendir(path_directorio_pokemon);

	if(dir == NULL) { // si esxiste o no el directorio (FAIL)

		signalSemaforo(catchPokemon->name);

		log_error(logger, "EL DIRECTORIO <%s> NO EXISTE", path_directorio_pokemon);
		enviar_respuesta_catch_pokemon(catchPokemon, false);

	} else {

		//LOOP:

		signalSemaforo(catchPokemon->name);

		waitSemaforo(catchPokemon->name);
		char *valor = get_valor_campo_metadata(path_directorio_pokemon, "DIRECTORY");
		if(string_equals_ignore_case( valor, "Y")) { // si es un directorio, se envia al broker la respuesta (FAIL)
			signalSemaforo(catchPokemon->name);

			log_info(logger, "LA RUTA <%s> ES SOLO UN DIRECTORIO", path_directorio_pokemon);
			enviar_respuesta_catch_pokemon(catchPokemon, false);

		} else {
			signalSemaforo(catchPokemon->name);

			log_info(logger, "EL DIRECTORIO <%s> EXISTE JUNTO CON EL ARCHIVO POKEMON", path_directorio_pokemon);
	    	// el archivo existe y hay que verificar si existe la linea

	    	waitSemaforo(catchPokemon->name);
			char *valor_open = get_valor_campo_metadata(path_directorio_pokemon, "OPEN");

	    	if(string_equals_ignore_case(valor_open, "N")) {

	    		cambiar_valor_metadata(path_directorio_pokemon, "OPEN", "Y");
	    		signalSemaforo(catchPokemon->name);

	    		buscar_linea_en_el_archivo_catch(catchPokemon, path_directorio_pokemon);

	    	} else {

	    		signalSemaforo(catchPokemon->name);
	    		reintentar_operacion(catchPokemon);
	    		goto LOOP;
	    	}
	    	free(valor_open);
	    }
		free(valor);
	}

	free(catchPokemon->name);
	free(catchPokemon);
	closedir(dir);
	free(path_directorio_pokemon);
}

// areglar esta funcion, se tiene que verificar que no este abierto el archivo por otro proceso
void buscar_linea_en_el_archivo_catch(catch_pokemon *catchPokemon, char *path_directorio_pokemon) { // verificamos si existe o no la linea en el archivo existente

	char *path_archivo_pokemon = string_new();
	string_append(&path_archivo_pokemon, path_directorio_pokemon);
	string_append_with_format(&path_archivo_pokemon, "%s%s%s", "/", catchPokemon->name, POKEMON_FILE_EXT);

	int tamanioArchivo = fileSize(path_archivo_pokemon);

	log_info(logger, "ABRIENDO ARCHIVO CON RUTA <%s>", path_archivo_pokemon);

	int fdPokemon = open(path_archivo_pokemon, O_RDWR);

	char *file_memory = mmap(0, tamanioArchivo, PROT_READ | PROT_WRITE, MAP_SHARED, fdPokemon, 0);

	close(fdPokemon);

	char *linea = string_new();
	char* posx = string_itoa(catchPokemon->pos.posx);
	char* posy = string_itoa(catchPokemon->pos.posy);
	string_append_with_format(&linea, "%s%s%s",posx , GUION, posy);

	if(string_contains(file_memory, linea)) { // buscar en el archivo y modificar

		//log_info(logger, "COORDENADA <%s> ENCONTRADO", linea);
		//log_info(logger, "LINEA <%s>", linea); // es la coordenada
		modificar_linea_pokemon_catch(file_memory, catchPokemon, path_directorio_pokemon, linea);

	} else { // se envia resultado al broker (FAIL)

		munmap(file_memory, tamanioArchivo);
		//log_info(logger, "COORDENADA <%s> NO ENCONTRADO!", linea);
		enviar_respuesta_catch_pokemon(catchPokemon, false);
		cambiar_valor_metadata(path_directorio_pokemon, "OPEN", "N");

	}

	free(path_archivo_pokemon);
	free(linea);
	free(posx);
	free(posy);
}


void modificar_linea_pokemon_catch(char* file_memory, catch_pokemon *catchPokemon, char *ruta_directorio_pokemon, char *coordenada) {

	char *ruta_archivo_pokemon = string_new();
	string_append(&ruta_archivo_pokemon, ruta_directorio_pokemon);
	string_append_with_format(&ruta_archivo_pokemon, "%s%s%s", "/", catchPokemon->name, ".txt");

	int tamArchivo = fileSize(ruta_archivo_pokemon);

	int posicionLinea = get_posicion_linea_en_archivo(coordenada, file_memory);

	int tamanioLineaVieja = get_tamanio_linea(file_memory + posicionLinea);
	//log_info(logger, "tamanio vieja linea: %d", tamanioLineaVieja);
	char* sub_vieja = string_substring(file_memory, posicionLinea, tamanioLineaVieja);
	char *viejaLinea = string_duplicate(sub_vieja);
	log_info(logger, "LINEA ANTES: %s", viejaLinea);

	char *lineaActualizada = get_linea_nueva_cantidad_catch(viejaLinea, coordenada); // devuelve la linea actualizada pero con el \n
	log_info(logger, "LINEA DESPUES: %s", lineaActualizada);

	int diferencia = tamanioLineaVieja - strlen(lineaActualizada);
	//log_info(logger, "diferencia vieja y nueva: %d", diferencia);

	int sizeCoordenada = strlen(coordenada) + 1; // + 1 por el =

	char* sub = string_substring(lineaActualizada, sizeCoordenada, strlen(lineaActualizada) - sizeCoordenada - 1);
	char* dup = string_duplicate(sub);
	int cantidadLineaActualizada = atoi(dup);
	//log_info(logger, "cantidad actualizada:%d", cantidadLineaActualizada);

	if(cantidadLineaActualizada != 0) { // no se borra la linea entera, solo uno o mas bytes de la misma

		if(diferencia == 0) { // no cambia nada, es solo reemplazar la misma linea con la nueva cantidad

			log_info(logger, "ACTUALIZANDO ARCHIVO <%s>", ruta_archivo_pokemon);
			memcpy(file_memory + posicionLinea, lineaActualizada, strlen(lineaActualizada));
			actualizar_contenido_blocks(ruta_directorio_pokemon, file_memory);

			if(msync(file_memory, tamArchivo, MS_SYNC) == -1) {
				log_error(logger, "ERROR MSYNC ARCHIVO <%s>", ruta_archivo_pokemon);
				exit(1);
			}

			munmap(file_memory, tamArchivo);
			retardo_operacion();
			cambiar_valor_metadata(ruta_directorio_pokemon, "OPEN", "N");
			enviar_respuesta_catch_pokemon(catchPokemon, true);

		} else {

			if(ultimo_bloque_queda_en_cero(diferencia, ruta_directorio_pokemon)) {
				// borro el ultimo bloque de la lista y tambien el archivo en blocks
				//log_info(logger, "ULTIMO BLOQUE DE DATOS DE <%s> VACIO", catchPokemon->name);

				int ultimo_bloque = ultimo_bloque_array_blocks(ruta_directorio_pokemon);
				borrar_ultimo_bloque_metadata_blocks(ruta_directorio_pokemon, ultimo_bloque);

				char *ultimoBloque = string_itoa(ultimo_bloque);
				borrar_archivo(ultimoBloque, 'B');

				pthread_mutex_lock(&mutexBITMAP);
				liberar_bloque_bitmap(ultimo_bloque);
				pthread_mutex_unlock(&mutexBITMAP);

				modificar_archivo_pokemon_catch_con_linea(file_memory, viejaLinea, lineaActualizada, posicionLinea, ruta_directorio_pokemon, catchPokemon->name);

				retardo_operacion(); // revisar, cuando se pasa de archivo a directorio, se hace retardo?

				enviar_respuesta_catch_pokemon(catchPokemon, true);

				free(ultimoBloque);

			} else {
				// solo modifico el ultimo bloque
				//log_info(logger, "SE MODIFICA EL ULTIMO BLOQUE DE DATOS DE <%s>", catchPokemon->name);
				modificar_archivo_pokemon_catch_con_linea(file_memory, viejaLinea, lineaActualizada, posicionLinea, ruta_directorio_pokemon, catchPokemon->name);
				enviar_respuesta_catch_pokemon(catchPokemon, true);
			}
		}

	} else { // se borra la linea entera

		if(tamArchivo == strlen(lineaActualizada)) { // se borra todo: el archvio, los bloques y en la metadata queda solamente el DIRECTORY

			cambiar_archivo_a_directorio(file_memory, ruta_archivo_pokemon, ruta_directorio_pokemon);
			enviar_respuesta_catch_pokemon(catchPokemon, true);

		} else {

			if(ultimo_bloque_queda_en_cero(strlen(lineaActualizada), ruta_directorio_pokemon)) {
				// borro el ultimo bloque de la lista y tambien el archivo en blocks
				//log_info(logger, "SE BORRA LA LINEA ENTERA Y EL ULTIMO BLOQUE");

				int ultimo_bloque = ultimo_bloque_array_blocks(ruta_directorio_pokemon);
				borrar_ultimo_bloque_metadata_blocks(ruta_directorio_pokemon, ultimo_bloque);

				char *ultimoBloque = string_itoa(ultimo_bloque);
				borrar_archivo(ultimoBloque, 'B');

				pthread_mutex_lock(&mutexBITMAP);
				liberar_bloque_bitmap(ultimo_bloque);
				pthread_mutex_unlock(&mutexBITMAP);

				modificar_archivo_pokemon_catch_sin_linea(file_memory, viejaLinea, lineaActualizada, posicionLinea, ruta_directorio_pokemon, catchPokemon->name);

				enviar_respuesta_catch_pokemon(catchPokemon, true);

				free(ultimoBloque);
			} else {
				// solo modifico el ultimo bloque
				//log_info(logger, "SOLO MODIFICAMOS EL ULTIMO BLOQUE");
				modificar_archivo_pokemon_catch_sin_linea(file_memory, viejaLinea, lineaActualizada, posicionLinea, ruta_directorio_pokemon, catchPokemon->name);
				enviar_respuesta_catch_pokemon(catchPokemon, true);
			}
		}
	}

	free(sub_vieja);
	free(sub);
	free(dup);
	free(lineaActualizada);
	free(viejaLinea);
	free(ruta_archivo_pokemon);
}


// esta es para cuando se borra la linea entera
void modificar_archivo_pokemon_catch_sin_linea(char *fileMemory, char *viejaLinea, char *lineaActualizada, int posicionLinea, char *path_directorio_pokemon, char *pokemon) {

	char *ruta_archivo_pokemon = string_new();
	string_append(&ruta_archivo_pokemon, path_directorio_pokemon);
	string_append_with_format(&ruta_archivo_pokemon, "%s%s%s", "/", pokemon, ".txt");

	int tamArchivo = fileSize(ruta_archivo_pokemon);

	int sizeViejaLinea = strlen(viejaLinea);

	int sizeNuevaLinea = strlen(lineaActualizada);

	int nuevoTamArchivo = tamArchivo - sizeNuevaLinea;

	int desplazamiento = 0;
	// mover los bloques para atras
	char *buffer = malloc(nuevoTamArchivo + 1); // + 1 por el \0

	if(posicionLinea == 0) { // esta en la primera linea

		memcpy(buffer, fileMemory + sizeViejaLinea, tamArchivo - sizeNuevaLinea);
		desplazamiento += sizeNuevaLinea;

		buffer[desplazamiento] = '\0';

		munmap(fileMemory, tamArchivo);

		//log_info(logger, "contenido actualizado:");
		//log_info(logger, "%s", buffer);

		escribir_archivo(ruta_archivo_pokemon, buffer, "w+");
		char* tam = string_itoa(nuevoTamArchivo);
		cambiar_valor_metadata(path_directorio_pokemon, "SIZE",tam );


		actualizar_contenido_blocks(path_directorio_pokemon, buffer);
		retardo_operacion();
		cambiar_valor_metadata(path_directorio_pokemon, "OPEN", "N");

		free(buffer);
		free(tam);

	} else if(posicionLinea + sizeViejaLinea == tamArchivo) { // esta en la ultima linea

			memcpy(buffer, fileMemory, posicionLinea);
			desplazamiento += posicionLinea;

			buffer[desplazamiento] = '\0';

			munmap(fileMemory, tamArchivo);

			//log_info(logger, "contenido actualizado:");
			//log_info(logger, "%s", buffer);

			escribir_archivo(ruta_archivo_pokemon, buffer, "w+");
			char* tam = string_itoa(nuevoTamArchivo);
			cambiar_valor_metadata(path_directorio_pokemon, "SIZE", tam);
			actualizar_contenido_blocks(path_directorio_pokemon, buffer);
			retardo_operacion();
			cambiar_valor_metadata(path_directorio_pokemon, "OPEN", "N");

			free(buffer);
			free(tam);

		} else { // esta en el medio

			memcpy(buffer, fileMemory, posicionLinea);
			desplazamiento += posicionLinea;

			memcpy(buffer + desplazamiento, fileMemory + posicionLinea + sizeViejaLinea, tamArchivo - posicionLinea - sizeViejaLinea);
			desplazamiento += tamArchivo - posicionLinea - sizeViejaLinea;

			buffer[desplazamiento] = '\0';

			munmap(fileMemory, tamArchivo);

			//log_info(logger, "contenido actualizado:");
			//log_info(logger, "%s", buffer);

			escribir_archivo(ruta_archivo_pokemon, buffer, "w+");
			char* tam = string_itoa(nuevoTamArchivo);
			cambiar_valor_metadata(path_directorio_pokemon, "SIZE", tam);
			actualizar_contenido_blocks(path_directorio_pokemon, buffer);
			retardo_operacion();
			cambiar_valor_metadata(path_directorio_pokemon, "OPEN", "N");
			
			free(buffer);
			free(tam);
		}

	free(ruta_archivo_pokemon);
	//free(buffer);
}


// esta funcion es para cuando la linea se reduce 1 byte o mas. Se mantiene la coordenada
void modificar_archivo_pokemon_catch_con_linea(char *fileMemory, char *viejaLinea, char *lineaActualizada, int posicionLinea, char *path_directorio_pokemon, char *pokemon) {

	char *ruta_archivo_pokemon = string_new();
	string_append(&ruta_archivo_pokemon, path_directorio_pokemon);
	string_append_with_format(&ruta_archivo_pokemon, "%s%s%s", "/", pokemon, ".txt");

	int tamArchivo = fileSize(ruta_archivo_pokemon);

	int sizeViejaLinea = strlen(viejaLinea);

	int sizeNuevaLinea = strlen(lineaActualizada);

	int nuevoTamArchivo = tamArchivo - (sizeViejaLinea - sizeNuevaLinea);

	int desplazamiento = 0;

	// mover los bloques para atras
	char *buffer = malloc(nuevoTamArchivo + 1); // + 1 por el \0

	if(posicionLinea == 0) { // esta en la primera linea

		memcpy(buffer, lineaActualizada, sizeNuevaLinea);
		desplazamiento += sizeNuevaLinea;

		memcpy(buffer + desplazamiento, fileMemory + sizeViejaLinea, tamArchivo - sizeViejaLinea);
		desplazamiento += tamArchivo - sizeViejaLinea;

		buffer[desplazamiento] = '\0';

		munmap(fileMemory, tamArchivo);

		//log_info(logger, "contenido actualizado:");
		//log_info(logger, "%s", buffer);

		escribir_archivo(ruta_archivo_pokemon, buffer, "w+");
		char* tam = string_itoa(nuevoTamArchivo);
		cambiar_valor_metadata(path_directorio_pokemon, "SIZE", tam);

		actualizar_contenido_blocks(path_directorio_pokemon, buffer);
		retardo_operacion();
		cambiar_valor_metadata(path_directorio_pokemon, "OPEN", "N");

		free(buffer);
		free(tam);

	} else if(posicionLinea + sizeViejaLinea == tamArchivo) { // esta en la ultima linea

			memcpy(buffer, fileMemory, posicionLinea);
			desplazamiento += posicionLinea;

			memcpy(buffer + desplazamiento, lineaActualizada, sizeNuevaLinea);
			desplazamiento += sizeNuevaLinea;

			buffer[desplazamiento] = '\0';

			munmap(fileMemory, tamArchivo);

			//log_info(logger, "contenido actualizado:");
			//log_info(logger, "%s", buffer);

			escribir_archivo(ruta_archivo_pokemon, buffer, "w+");
			char* tam = string_itoa(nuevoTamArchivo);
			cambiar_valor_metadata(path_directorio_pokemon, "SIZE",tam);
			actualizar_contenido_blocks(path_directorio_pokemon, buffer);
			retardo_operacion();
			cambiar_valor_metadata(path_directorio_pokemon, "OPEN", "N");

			free(buffer);
			free(tam);

		} else { // esta en el medio

			memcpy(buffer, fileMemory, posicionLinea);
			desplazamiento += posicionLinea;

			memcpy(buffer + desplazamiento, lineaActualizada, sizeNuevaLinea);
			desplazamiento += sizeNuevaLinea;

			buffer[desplazamiento] = '\0';

			munmap(fileMemory, tamArchivo);

			//log_info(logger, "contenido actualizado:");
			//log_info(logger, "%s", buffer);

			escribir_archivo(ruta_archivo_pokemon, buffer, "w+");
			char* tam = string_itoa(nuevoTamArchivo);
			cambiar_valor_metadata(path_directorio_pokemon, "SIZE", tam);

			actualizar_contenido_blocks(path_directorio_pokemon, buffer);
			retardo_operacion();
			cambiar_valor_metadata(path_directorio_pokemon, "OPEN", "N");

			free(buffer);
			free(tam);
		}

	free(ruta_archivo_pokemon);
}


char* get_linea_nueva_cantidad_catch(char *linea, char *coordenada) { // nos devuelve un char* que tiene la cantidad modificada pero disminuido en 1 (x - 1)

	int sizeCoordenada = strlen(coordenada) + 1; // 1 por el caracter '='

	int sizeLinea = strlen(linea);

	char* sub = string_substring(linea, sizeCoordenada, sizeLinea - sizeCoordenada);
	char *viejaCantidad = string_duplicate(sub);

	int oldCantidad = atoi(viejaCantidad);

	int nuevaCantidad = oldCantidad - 1;

	char *newCantidad = string_itoa(nuevaCantidad);

	char *lineaModificada = string_new();
	string_append_with_format(&lineaModificada, "%s%s%s%s", coordenada, IGUAL, newCantidad, "\n");
	//printf("size nueva linea: %d\n", strlen(lineaModificada));

	free(sub);
	free(newCantidad);
	free(viejaCantidad);
	return lineaModificada;
}


bool ultimo_bloque_queda_en_cero(int bytes_a_mover, char *path_directorio_pokemon) { // nos va a determinar si el ultimo bloque queda vacio o no

	//char **array_blocks = get_array_blocks_metadata(path_directorio_pokemon);

	char *ultimoBloque = string_itoa(ultimo_bloque_array_blocks(path_directorio_pokemon));

	char *ruta_archivo_bloque = string_new();
	string_append_with_format(&ruta_archivo_bloque, "%s%s%s%s%s", datos_config->pto_de_montaje, BLOCKS_DIR, "/", ultimoBloque, ".txt");

	int fileSizeLastBlock = fileSize(ruta_archivo_bloque);

	free(ultimoBloque);
	free(ruta_archivo_bloque);
	if(bytes_a_mover >= fileSizeLastBlock) {
		// puedo asumir que el ultimo bloque queda vacio
		return true;

	} else {
		// asumo que va a seguir habiendo datos en el ultimo bloque
		return false;
	}
}


void cambiar_archivo_a_directorio(char *file_memory, char *path_archivo_pokemon, char *path_directorio_pokemon) {

	int tamArchivo = fileSize(path_archivo_pokemon);

	munmap(file_memory, tamArchivo);
	remove(path_archivo_pokemon);
	int ultimo_bloque = ultimo_bloque_array_blocks(path_directorio_pokemon);

	pthread_mutex_lock(&mutexBITMAP);
	liberar_bloque_bitmap(ultimo_bloque);
	pthread_mutex_unlock(&mutexBITMAP);

	char *path_ultimo_bloque = string_new();
	string_append_with_format(&path_ultimo_bloque, "%s%s%s%s%s", datos_config->pto_de_montaje, BLOCKS_DIR, "/", string_itoa(ultimo_bloque), ".txt");
	remove(path_ultimo_bloque);

	cambiar_metadata_archivo_a_directorio(path_directorio_pokemon);
	retardo_operacion();
	cambiar_valor_metadata(path_directorio_pokemon, "DIRECTORY", "Y");

}


void borrar_archivo(char *nombre, char flag) {

	char *ruta_archivo = string_new();
	string_append(&ruta_archivo, datos_config->pto_de_montaje);

	if(flag == 'P') { // quiero borrar un archivo pokemon
		string_append_with_format(&ruta_archivo, "%s%s%s", POKEMON_DIR, nombre, ".txt");
		log_info(logger, "BORRANDO ARCHIVO <%s>", ruta_archivo);
		remove(ruta_archivo);
	} else {
		string_append_with_format(&ruta_archivo, "%s%s%s%s", BLOCKS_DIR, "/", nombre, ".txt");
		log_info(logger, "BORRANDO ARCHIVO <%s>", ruta_archivo);
		remove(ruta_archivo);
	}

	free(ruta_archivo);
}


void cambiar_metadata_archivo_a_directorio(char *path_directorio_pokemon) {

	char *ruta_metadata_pokemon = string_new();
	string_append_with_format(&ruta_metadata_pokemon, "%s%s", path_directorio_pokemon, METADATA_FILE);

	log_info(logger, "CAMBIANDO METADATA CON FORMATO ARCHIVO A METADATA CON FORMATO DIRECTORIO EN <%s>", ruta_metadata_pokemon);

	t_config *metadataArchivo = config_create(ruta_metadata_pokemon);

	config_remove_key(metadataArchivo, "OPEN");

	config_remove_key(metadataArchivo, "BLOCKS");

	config_remove_key(metadataArchivo, "SIZE");

	config_save_in_file(metadataArchivo, ruta_metadata_pokemon);

	config_destroy(metadataArchivo);

	free(ruta_metadata_pokemon);
}


void borrar_ultimo_bloque_metadata_blocks(char *ruta_directorio_pokemon, int nro_bloque) { // borra el ultimo bloque en el campo blocks de la metadata

	char* nro = string_itoa(nro_bloque);
	int size_nro_bloque = strlen(nro);

	int size_new_string = strlen(ruta_directorio_pokemon) + strlen(METADATA_FILE) + 1;
	char *ruta_metadata_pokemon = malloc(size_new_string);
	strcpy(ruta_metadata_pokemon, ruta_directorio_pokemon);
	string_append(&ruta_metadata_pokemon, METADATA_FILE);

	log_info(logger, "BORRANDO BLOQUE [%d] EN CAMPO BLOCKS EN LA METADATA <%s>",nro_bloque,  ruta_metadata_pokemon);

	t_config *metadata_pokemon = config_create(ruta_metadata_pokemon);

	char *blocks = config_get_string_value(metadata_pokemon, "BLOCKS");
	int sizeBlocks = strlen(blocks);

	int newSizeBlocks = sizeBlocks - size_nro_bloque - 1; // -1 por la coma

	char *newBlocks = malloc(newSizeBlocks + 1);
	char* sub = string_substring(blocks, 1, newSizeBlocks - 2);
	char *auxiliar = strdup(sub); // se resta 2 por los corchetes []
	//log_info(logger, "auxiliar: %s", auxiliar);

	int posicion = 0;

	strcpy(newBlocks, "[");
	posicion++;

	strcpy(newBlocks + posicion, auxiliar);
	posicion += strlen(auxiliar);

	strcpy(newBlocks + posicion, "]");
	posicion++;

	config_set_value(metadata_pokemon, "BLOCKS", newBlocks);
	config_save(metadata_pokemon);
	config_destroy(metadata_pokemon);
	free(nro);
	free(sub);
	free(auxiliar);
	free(newBlocks);
	free(ruta_metadata_pokemon);
}

void liberar_bloque_bitmap(int nro_bloque_a_liberar) {

	log_info(logger, "ACTUALIZANDO BITARRAY  OPERACION CLEAN");

	bitarray_clean_bit(bitarray, nro_bloque_a_liberar);

	log_info(logger, "ACUTALIZANDO BITMAP OPERACION CLEAN");
	if(msync(bitmap_memoria, datos_config->blocks / BITS, MS_SYNC) == -1) {
		log_error(logger, "ERROR MSYNC BITMAP BITARRAY CLEAN");
		exit(1);
	}
}

//-------------------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------------

int fileSize(char* file) { 

	FILE* fp = fopen(file, "r");
    if (fp == NULL) {
    	log_error(logger, "No existe el archivo");
        exit(1);
    } 

    fseek(fp, 0L, SEEK_END);
    int res = (int) ftell(fp); 
    fclose(fp); 
    return res; 
} 


// si no estoy mal, esta funcion trata de poner lo que hay en el archivo en un buffer
//sip
char *read_file_into_buf (char * source /*,FILE *fp*/) {

	int size_file = fileSize(source);
	if(size_file == 0) {
		return NULL;
	}

	char *buffer_file = malloc(size_file);

	FILE *pointerFile = fopen(source, "r");

	fread(buffer_file, size_file, 1, pointerFile);

	fclose(pointerFile);

	buffer_file[size_file-1] = '\0';

	return buffer_file;

}


void operacion_get_pokemon(get_pokemon *getPokemon) {

	pthread_mutex_lock(&mutexSemaforo);
	key_semaforo_presente(getPokemon->name);
	pthread_mutex_unlock(&mutexSemaforo);

	char *path_directorio_pokemon = string_new();
	string_append_with_format(&path_directorio_pokemon, "%s%s%s%s",datos_config->pto_de_montaje, FILES_DIR, "/", getPokemon->name);
	log_info(logger, "EN BUSCA DEL DIRECTORIO DEL POKEMON CON PATH <%s>", path_directorio_pokemon);
        LOOP:

	waitSemaforo(getPokemon->name);
	DIR* dir = opendir(path_directorio_pokemon);
	if(dir == NULL) { // si esxiste o no el directorio (FAIL)

		signalSemaforo(getPokemon->name);

		log_error(logger, "EL DIRECTORIO <%s> NO EXISTE", path_directorio_pokemon);

		log_info(logger, "ID MENSAJE: %d | POKEMON: %s | POSICIONES: NULL", getPokemon->id_mensaje, getPokemon->name);

	} else {

		//LOOP:

		signalSemaforo(getPokemon->name);

		waitSemaforo(getPokemon->name);
		char *valor = get_valor_campo_metadata(path_directorio_pokemon, "DIRECTORY");
		if(string_equals_ignore_case( valor, "Y")) { // si es un directorio, se envia al broker la respuesta (FAIL)

			signalSemaforo(getPokemon->name);

			log_info(logger, "LA RUTA <%s> ES SOLO UN DIRECTORIO ", path_directorio_pokemon);

			log_info(logger, "ID MENSAJE: %d | POKEMON: %s | POSICIONES: 0", getPokemon->id_mensaje, getPokemon->name);

		} else {

			signalSemaforo(getPokemon->name);

			log_info(logger, "EL DIRECTORIO <%s> EXISTE JUNTO CON EL ARCHIVO POKEMON",path_directorio_pokemon);

	    	waitSemaforo(getPokemon->name);
	    	char *valor_open = get_valor_campo_metadata(path_directorio_pokemon, "OPEN");

	    	if(string_equals_ignore_case(valor_open, "N")) {

	    		cambiar_valor_metadata(path_directorio_pokemon, "OPEN", "Y");
	    		signalSemaforo(getPokemon->name);

	    		buscar_lineas_get_pokemon(getPokemon, path_directorio_pokemon);

	    	} else {
	    		signalSemaforo(getPokemon->name);
	    		reintentar_operacion(getPokemon);
	    		goto LOOP;
	    	}
	    	free(valor_open);
	    }
		free(valor);
	}

	//list_destroy(listaCoordenadas);
	free(getPokemon->name);
	free(getPokemon);
	closedir(dir);
	free(path_directorio_pokemon);
}


void buscar_lineas_get_pokemon(get_pokemon *getPokemon, char *path_directorio_pokemon) {

	char *ruta_archivo_pokemon = string_new();
	string_append_with_format(&ruta_archivo_pokemon, "%s%s%s%s", path_directorio_pokemon,"/", getPokemon->name, ".txt");

	char *contenido_archivo = read_file_into_buf(ruta_archivo_pokemon);

	char **lineas = string_split(contenido_archivo, "\n");

	int i = 0;

	t_list *listaElementosCoordenadas = list_create();

	log_info(logger, "OBTENIENDO POSICIONES (X,Y) EN EL ARCHIVO <%s>", ruta_archivo_pokemon);

	while(lineas[i] != NULL) {

		char *linea = lineas[i];
		log_info(logger, "linea: %s", linea);

		position *elementos = obtener_elementos_coordenadas(linea);

		list_add(listaElementosCoordenadas, elementos);

		free(lineas[i]);
		i++;
	}

	enviar_respuesta_get_pokemon(getPokemon, 1, listaElementosCoordenadas);

	cambiar_valor_metadata(path_directorio_pokemon, "OPEN", "N");

	void limpiar(position* pos){
		free(pos);
	}
	list_destroy_and_destroy_elements(listaElementosCoordenadas,(void*)limpiar);

	free(lineas);
	free(ruta_archivo_pokemon);
	free(contenido_archivo);
}


position* obtener_elementos_coordenadas(char *linea) {

	int posSeparators[2] = {0 , 0}; // sabemos que solo tenemos dos separadores, el "-" y el "="

	int j = 0;

	for(int i = 0 ; i < strlen(linea) ; i++) {
		if(!isdigit(linea[i])) {
			posSeparators[j] = i;
			j++;
		}
	}

	position *coordenadas = malloc(sizeof(position));
	char* aux_posx = string_substring(linea, 0, posSeparators[0]);
	char* dup_posx = string_duplicate(aux_posx);
	char* aux_posy = string_substring(linea, posSeparators[0] + 1, posSeparators[1] - posSeparators[0] - 1);
	char* dup_posy = string_duplicate(aux_posy);

	coordenadas->posx = atoi(dup_posx);
	coordenadas->posy = atoi(dup_posy);

	free(aux_posx);
	free(aux_posy);
	free(dup_posx);
	free(dup_posy);

	return coordenadas;
}



//-----------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------
// Funciones de sockets (segui bajando si es que necesitas ver algo)


void suscripcion_colas_broker() {

	log_info(logger, "ESTABLECIENDO CONEXION CON EL BROKER");
	suscribirse_a_new_pokemon();
	suscribirse_a_catch_pokemon();
	suscribirse_a_get_pokemon();

	if(socket_cliente_np == -1 || socket_cliente_cp == -1 || socket_cliente_gp == -1) {

		log_error(logger, "BROKER NO ESTA DISPONIBLE PARA LA CONEXION");
		reintento_conectar_broker();

	} else {
		log_info(logger, "GAMECARD CONECTADO AL BROKER");

		pthread_create(&thread_new_pokemon, NULL, (void *)recibir_mensajes_new_pokemon, NULL);
		pthread_detach(thread_new_pokemon);

		pthread_create(&thread_catch_pokemon, NULL, (void *)recibir_mensajes_catch_pokemon, NULL);
		pthread_detach(thread_catch_pokemon);

		pthread_create(&thread_get_pokemon, NULL, (void *)recibir_mensajes_get_pokemon, NULL);
		pthread_detach(thread_get_pokemon);
	}

}


void suscribirse_a_new_pokemon() {
	char* puerto = string_itoa(datos_config->puerto_broker);
	socket_cliente_np = crear_conexion(datos_config->ip_broker, puerto);
	if(socket_cliente_np != -1) {

		enviar_info_suscripcion(NEW_POKEMON, socket_cliente_np, datos_config->pid);
		recv(socket_cliente_np, &(acks_gamecard.ack_new), sizeof(int), MSG_WAITALL);
		log_info(logger, "ACK RECIVIDO PARA COLA NEW_POKEMON: %d", acks_gamecard.ack_new);
	}
	free(puerto);
}


void suscribirse_a_catch_pokemon() {
	char* puerto = string_itoa(datos_config->puerto_broker);
	socket_cliente_cp = crear_conexion(datos_config->ip_broker, puerto);
	if(socket_cliente_cp != -1) {

		enviar_info_suscripcion(CATCH_POKEMON, socket_cliente_cp, datos_config->pid);
		recv(socket_cliente_cp, &(acks_gamecard.ack_catch), sizeof(int), MSG_WAITALL);
		log_info(logger, "ACK RECIVIDO PARA COLA CATCH_POKEMON: %d", acks_gamecard.ack_catch);
	}
	free(puerto);
}


void suscribirse_a_get_pokemon() {
	char* puerto = string_itoa(datos_config->puerto_broker);
	socket_cliente_gp = crear_conexion(datos_config->ip_broker, puerto);
	if(socket_cliente_gp != -1) {

		enviar_info_suscripcion(GET_POKEMON, socket_cliente_gp, datos_config->pid);
		recv(socket_cliente_gp, &(acks_gamecard.ack_get), sizeof(int), MSG_WAITALL);
		log_info(logger, "ACK RECIVIDO PARA COLA GET_POKEMON: %d", acks_gamecard.ack_get);
	}
	free(puerto);
}


// Funciones del Gamecard como Cliente del Broker

void recibir_mensajes_new_pokemon(){

	while(1) {

		int codigo_operacion;
		if(recv(socket_cliente_np, &(codigo_operacion), sizeof(int), MSG_WAITALL) == -1) {
			log_warning(logger, "ERROR EN RECV DE CODIGO DE OPERACION");
			codigo_operacion = -1;
		}

		if(codigo_operacion <= 0 || check_socket(socket_cliente_np) != 1) {
			log_error(logger, "BROKER CAIDO");
			socket_cliente_np = -1;
			reintento_conectar_broker();
			return;
		}

		t_list* paquete = recibir_paquete(socket_cliente_np);

		void display(void* valor) {

			int desplazamiento = 0;

			int id_mensaje;
			memcpy(&(id_mensaje), valor, sizeof(int));
			desplazamiento += sizeof(int);

			new_pokemon *newPokemon = deserializar_new(valor + desplazamiento);
			newPokemon->id_mensaje = id_mensaje;

			log_info(logger,"Llega Un Mensaje Tipo: NEW_POKEMON ID:%d POKEMON:%s POSX:%d POSY:%d CANT:%d\n", newPokemon->id_mensaje, newPokemon->name, newPokemon->pos.posx, newPokemon->pos.posy, newPokemon->cantidad);

			enviar_ack(NEW_POKEMON, newPokemon->id_mensaje, datos_config->pid, socket_cliente_np);

			free(valor);

			// ejecutar hilo new_pokemon
			pthread_t hilo_new_pokemon_broker;
			pthread_create(&hilo_new_pokemon_broker, NULL, (void *) operacion_new_pokemon, (void *) newPokemon);
			pthread_detach(hilo_new_pokemon_broker);
		}

		list_iterate(paquete,(void*)display);
		list_destroy(paquete);
	}
}


void recibir_mensajes_catch_pokemon(){

	while(1) {

		int codigo_operacion;
		if(recv(socket_cliente_cp, &(codigo_operacion), sizeof(int), MSG_WAITALL) == -1) {
			log_warning(logger, "ERROR EN RECV DE CODIGO DE OPERACION");
			codigo_operacion = -1;
		}

		if(codigo_operacion <= 0  || check_socket(socket_cliente_cp) != 1) {
			log_error(logger, "BROKER CAIDO");
			socket_cliente_cp = -1;
			reintento_conectar_broker();
			return;
		}

		t_list* paquete = recibir_paquete(socket_cliente_cp);

		void display(void* valor){

			int desplazamiento = 0;

			int id_mensaje;
			memcpy(&(id_mensaje), valor + desplazamiento, sizeof(int));
			desplazamiento += sizeof(int);

			catch_pokemon *catchPokemon = deserializar_catch(valor + desplazamiento);
			catchPokemon->id_mensaje = id_mensaje;

			free(valor);

			log_info(logger,"Llega Un Mensaje Tipo: CATCH_POKEMON ID:%d POKEMON:%s POSX:%d POSY:%d\n", catchPokemon->id_mensaje, catchPokemon->name, catchPokemon->pos.posx, catchPokemon->pos.posy);
			enviar_ack(CATCH_POKEMON, catchPokemon->id_mensaje, datos_config->pid, socket_cliente_cp);

			pthread_t hilo_catch_pokemon_broker;
			pthread_create(&hilo_catch_pokemon_broker, NULL , (void *) operacion_catch_pokemon, (void *) catchPokemon);
			pthread_detach(hilo_catch_pokemon_broker);

		}
		list_iterate(paquete,(void*)display);
		list_destroy(paquete);
	}
}


void recibir_mensajes_get_pokemon(){

	while(1) {

		int codigo_operacion;
		if(recv(socket_cliente_gp, &(codigo_operacion), sizeof(int), MSG_WAITALL) == -1) {
			log_warning(logger, "ERROR EN RECV DE CODIGO DE OPERACION");
			codigo_operacion = -1;
		}

		if(codigo_operacion <= 0  || check_socket(socket_cliente_gp) != 1) {
			log_error(logger, "BROKER CAIDO");
			socket_cliente_gp = -1;
			reintento_conectar_broker();
			return;
		}

		t_list* paquete = recibir_paquete(socket_cliente_gp);

		void display(void* valor){

			int desplazamiento = 0;

			int id_mensaje;
			memcpy(&(id_mensaje), valor + desplazamiento, sizeof(int));
			desplazamiento += sizeof(int);

			get_pokemon *getPokemon = deserializar_get(valor + desplazamiento);

			getPokemon->id_mensaje = id_mensaje;

			free(valor);

			log_info(logger,"Llega Un Mensaje Tipo: GET_POKEMON ID:%d POKEMON:%s \n", getPokemon->id_mensaje, getPokemon->name);
			enviar_ack(GET_POKEMON, getPokemon->id_mensaje, datos_config->pid, socket_cliente_gp);

			pthread_t hilo_get_pokemon_broker;
			pthread_create(&hilo_get_pokemon_broker, NULL , (void *) operacion_get_pokemon, (void *) getPokemon);
			pthread_detach(hilo_get_pokemon_broker);
		}

		list_iterate(paquete,(void*)display);
		list_destroy(paquete);
	}
}


// Funciones del Gamecard como Servidor

void iniciar_servidor(void) {

	int socket_servidor;

    struct addrinfo hints, *servinfo, *p;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    getaddrinfo(IP_SERVIDOR, PUERTO_SERVIDOR, &hints, &servinfo);

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
	//log_info(logger, "GAMECARD INICIADO COMO SERVIDOR PARA GAMEBOY, ESPERANDO MENSAJES...");
    freeaddrinfo(servinfo);

    while(1)
    	esperar_cliente(socket_servidor);
}


void esperar_cliente(int socket_servidor) {

	struct sockaddr_in dir_cliente;
	int tam_direccion = sizeof(struct sockaddr_in);

	int socket_cliente = accept(socket_servidor, (void*) &dir_cliente, (socklen_t *)&tam_direccion);
	log_info(logger, "GAMEBOY CONECTADO!",socket_cliente);

	int cod_op;
	if(recv(socket_cliente, &cod_op, sizeof(int), MSG_WAITALL) == -1) {
		cod_op = -1;
	}
	//log_info(logger, "codigo de operacion: %d", cod_op);

	atender_peticion(socket_cliente, cod_op);
}


void atender_peticion(int socket_cliente, int cod_op) {

	int sizeStream;
	if(recv(socket_cliente, &sizeStream, sizeof(int), MSG_WAITALL) == -1) {
		sizeStream = -1;
	}
	//printf("size_stream: %d\n", sizeStream);

	void *stream = malloc(sizeStream);
	if(recv(socket_cliente, stream, sizeStream, MSG_WAITALL) == -1) {

	}

	switch(cod_op) {
		case NEW_POKEMON:
			log_info(logger, "SE RECIBIO UN MENSAJE CON OPERACION NEW_POKEMON DE GAMEBOY");

			int id_mensaje_new;
			memcpy(&(id_mensaje_new), stream, sizeof(int));
			//printf("ID MENSAJE: %d\n", id_mensaje_new);

			new_pokemon *newPokemon = deserializar_new(stream + sizeof(int));

			//printf("POKEMON: %s | ", newPokemon->name);
			//printf("PosX: %d | ", newPokemon->pos.posx);
			//printf("PosY: %d | ", newPokemon->pos.posy);
			//printf("Cantidad: %d | \n", newPokemon->cantidad);
			newPokemon->id_mensaje = id_mensaje_new;
			log_info(logger, "ID MENSAJE: %d | POKEMON: %s | POS X: %d | POS Y: %d", newPokemon->id_mensaje, newPokemon->name, newPokemon->pos.posx, newPokemon->pos.posy);

			//free(stream);

			pthread_t hilo_new_pokemon;
			pthread_create(&hilo_new_pokemon, NULL, (void *) operacion_new_pokemon, (void *) newPokemon);
			pthread_detach(hilo_new_pokemon);
			//free(stream);
		break;

		case CATCH_POKEMON:
			log_info(logger, "SE RECIBIO UN MENSAJE CON OPERACION CATCH_POKEMON DE GAMEBOY");

			int id_mensaje_catch;
			memcpy(&(id_mensaje_catch), stream, sizeof(int));
			//printf("ID MENSAJE: %d\n", id_mensaje_catch);

			catch_pokemon *catchPokemon = deserializar_catch(stream + sizeof(int));

			//printf("POKEMON: %s\n", catchPokemon->name);
			//printf("PosX: %d\n", catchPokemon->pos.posx);
			//printf("PosY: %d\n", catchPokemon->pos.posy);
			catchPokemon->id_mensaje = id_mensaje_catch;

			log_info(logger, "ID MENSAJE: %d | POKEMON: %s | POS X: %d | POS Y: %d", catchPokemon->id_mensaje, catchPokemon->name, catchPokemon->pos.posx, catchPokemon->pos.posy);

			//free(stream);

			pthread_t hilo_catch_pokemon;
			pthread_create(&hilo_catch_pokemon, NULL, (void *) operacion_catch_pokemon, (void *) catchPokemon);
			pthread_detach(hilo_catch_pokemon);
			//free(stream);
		break;

		case GET_POKEMON:
			log_info(logger, "SE RECIBIO UN MENSAJE CON OPERACION GET_POKEMON DE GAMEBOY");

			int id_mensaje_get;
			memcpy(&(id_mensaje_get), stream, sizeof(int));

			get_pokemon *getPokemon = deserializar_get(stream + sizeof(int));

			//printf("ID MENSAJE: %d\n", id_mensaje_get);
			//printf("POKEMON: %s\n", getPokemon->name);
			getPokemon->id_mensaje = id_mensaje_get;

			log_info(logger, "ID MENSAJE: %d | POKEMON: %s", getPokemon->id_mensaje, getPokemon->name);

			//free(stream);

			pthread_t hilo_get_pokemon;
			pthread_create(&hilo_get_pokemon, NULL, (void *) operacion_get_pokemon, (void *) getPokemon);
			pthread_detach(hilo_get_pokemon);
		break;

		default:
			log_warning(logger, "NO SE RECIBIO NINGUNA DE LAS ANTERIORES");
		break;
	}

	//free(stream);
}


void reintento_conectar_broker() {

	struct sigaction action;
	action.sa_handler = (void*) reconexion_broker;
	action.sa_flags = SA_RESTART|SA_NODEFER;
	sigemptyset(&action.sa_mask);
	sigaction(SIGALRM, &action, 0);
	alarm(datos_config->tiempo_reintento_conexion);

}


void reconexion_broker() {

	if(socket_cliente_np == -1 || socket_cliente_cp == -1 || socket_cliente_gp == -1) {
		log_warning(logger,"Reconectando A Broker...");
		suscripcion_colas_broker();
		alarm(datos_config->tiempo_reintento_conexion);
	}

}


void enviar_respuesta_new_pokemon(new_pokemon *newPokemon) {
	char* puerto = string_itoa(datos_config->puerto_broker);
	int socket_temporal_broker = crear_conexion(datos_config->ip_broker, puerto);

	if(socket_temporal_broker != -1){
		int desplazamiento = 0;
		t_paquete* paquete = malloc(sizeof(t_paquete));

		paquete->codigo_operacion = APPEARED_POKEMON;
		paquete->buffer = malloc(sizeof(t_buffer));
		paquete->buffer->size = sizeof(int) * 2 + sizeof(position) + newPokemon->name_size+1;
		paquete->buffer->stream = malloc(paquete->buffer->size);

		memcpy(paquete->buffer->stream, &(newPokemon->id_mensaje), sizeof(int));
		desplazamiento += sizeof(int);

		memcpy(paquete->buffer->stream + desplazamiento, &(newPokemon->name_size), sizeof(int));
		desplazamiento += sizeof(int);

		memcpy(paquete->buffer->stream + desplazamiento, newPokemon->name, newPokemon->name_size + 1);
		desplazamiento += newPokemon->name_size + 1;

		memcpy(paquete->buffer->stream + desplazamiento, &(newPokemon->pos), sizeof(position));
		desplazamiento += sizeof(position);

		enviar_paquete(paquete, socket_temporal_broker);

		close(socket_temporal_broker);
		free(paquete->buffer->stream);
		free(paquete->buffer);
		free(paquete);
	} else {

		log_error(logger, "BROKER NO DISPONIBLE");
		log_info(logger, "ID MENSAJE: %d | POKEMON: %s | POSICIONES: [%d,%d]", newPokemon->id_mensaje, newPokemon->name, newPokemon->pos.posx, newPokemon->pos.posy);

	}


	free(puerto);
}


void enviar_respuesta_catch_pokemon(catch_pokemon *catchPokemon, bool valor) {
	char* puerto = string_itoa(datos_config->puerto_broker);
	int socket_temporal_broker = crear_conexion(datos_config->ip_broker, puerto);

	if(socket_temporal_broker != -1){
		int desplazamiento = 0;
		t_paquete* paquete = malloc(sizeof(t_paquete));

		paquete->codigo_operacion = CAUGHT_POKEMON;
		paquete->buffer = malloc(sizeof(t_buffer));
		paquete->buffer->size = sizeof(int) + sizeof(bool);
		paquete->buffer->stream = malloc(paquete->buffer->size);

		memcpy(paquete->buffer->stream, &(catchPokemon->id_mensaje), sizeof(int));
		desplazamiento += sizeof(int);

		memcpy(paquete->buffer->stream + desplazamiento, &(valor), sizeof(bool));
		desplazamiento += sizeof(bool);

		enviar_paquete(paquete, socket_temporal_broker);

		close(socket_temporal_broker);
		free(paquete->buffer->stream);
		free(paquete->buffer);
		free(paquete);
	} else {

		log_error(logger, "BROKER NO DISPONIBLE");
		log_info(logger, "ID MENSAJE: %d | RESULTADO: %d", catchPokemon->id_mensaje, valor);

	}
	free(puerto);
}


void enviar_respuesta_get_pokemon(get_pokemon *getPokemon, int valor_rta, t_list *info_lineas) {

	t_paquete* paquete = malloc(sizeof(t_paquete));

	paquete->codigo_operacion = LOCALIZED_POKEMON;
	paquete->buffer = malloc(sizeof(t_buffer));

	if(valor_rta == 1) {

		enviar_rta_con_exito_get(getPokemon, paquete, info_lineas);

	}
	else {

		log_info(logger, "ID MENSAJE: %d | POKEMON: %s | POSICIONES: 0", getPokemon->id_mensaje, getPokemon->name);
	}

	free(paquete->buffer);
	free(paquete);
}


void enviar_rta_con_exito_get(get_pokemon *getPokemon, t_paquete *paquete, t_list *info_lineas) {
	char* puerto = string_itoa(datos_config->puerto_broker);
	int socket_temporal_broker = crear_conexion(datos_config->ip_broker, puerto);

	int cantidad_posiciones = list_size(info_lineas);

	if(socket_temporal_broker == -1) {

		log_info(logger, "ID MENSAJE: %d | POKEMON: %s | CANTIDAD POSICIONES: %d", getPokemon->id_mensaje, getPokemon->name, cantidad_posiciones);

		void display(position *e) {
			log_info(logger, "POS X: %d | POS Y: %d ", e->posx, e->posy);
		}

		list_iterate(info_lineas, (void *)display);

	} else {

		int desplazamiento = 0;

		paquete->buffer->size = sizeof(int) * 2 + getPokemon->name_size + 1 + sizeof(int) + list_size(info_lineas) * sizeof(position);
		paquete->buffer->stream = malloc(paquete->buffer->size);

		memcpy(paquete->buffer->stream, &(getPokemon->id_mensaje), sizeof(int));
		desplazamiento += sizeof(int);

		memcpy(paquete->buffer->stream + desplazamiento, &(getPokemon->name_size), sizeof(int));
		desplazamiento += sizeof(int);

		memcpy(paquete->buffer->stream + desplazamiento, getPokemon->name, getPokemon->name_size + 1);
		desplazamiento += getPokemon->name_size + 1;

		memcpy(paquete->buffer->stream + desplazamiento, &(cantidad_posiciones), sizeof(int));
		desplazamiento += sizeof(int);

		for(int i = 0 ; i < cantidad_posiciones ; i++) {
			memcpy(paquete->buffer->stream + desplazamiento, list_get(info_lineas, i), sizeof(position));
			desplazamiento += sizeof(position);
		}

		enviar_paquete(paquete, socket_temporal_broker);
		free(paquete->buffer->stream);
		close(socket_temporal_broker);
	}
	free(puerto);
}


//--------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------
//SEMAFOROS DEL METADATA POKEMON
void key_semaforo_presente(char *pokemon) { // si la key no esta presente, se crea el semaforo y luego se lo agrega a la coleccion con la key

	if(!dictionary_has_key(diccionario_semaforos, pokemon)) {

		sem_t *sem_pokemon = malloc(sizeof(sem_t));
		sem_init(sem_pokemon, 0, 1);

		dictionary_put(diccionario_semaforos, pokemon, sem_pokemon);
		//agregarSemaforo(pokemon);
	}

}


sem_t *get_semaforo(char *pokemon) {

	return dictionary_get(diccionario_semaforos, pokemon);

}

void waitSemaforo(char* pokemon){
	//pthread_mutex_lock(&mutexSemaforo);
	sem_t *semAux = get_semaforo(pokemon);
	sem_wait(semAux);
	//pthread_mutex_unlock(&mutexSemaforo);
	//HAY QUE HACER FREE DE ESTE SEMAFORO AUXILIAR??
}

void signalSemaforo(char* pokemon){
	//pthread_mutex_lock(&mutexSemaforo);
	sem_t *semAux = get_semaforo(pokemon);
	sem_post(semAux);
	//pthread_mutex_unlock(&mutexSemaforo);
	//HAY QUE HACER FREE DE ESTE SEMAFORO AUXILIAR??
}

void agregarSemaforo(char* pokemon){
	sem_t *sem_pokemon = malloc(sizeof(sem_t));
	sem_init(sem_pokemon, 0, 1);

	dictionary_put(diccionario_semaforos, pokemon, sem_pokemon);
}
void vaciarDiccionarioSemaforos(){
	pthread_mutex_lock(&mutexSemaforo);
	dictionary_clean_and_destroy_elements(diccionario_semaforos, (void*)eliminarSemaforo);
	pthread_mutex_unlock(&mutexSemaforo);
}
void eliminarSemaforo(sem_t* semAux){
	free(semAux);
}
