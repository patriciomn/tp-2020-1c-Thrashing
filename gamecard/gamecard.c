#include "gamecard.h"
#include "sockets.h"

int main () {

	pid_gamecard = getpid();

    iniciar_logger_config();

    //obtener_datos_archivo_config();

    //verificar_punto_de_montaje();

    suscripcion_colas_broker();
    
    //crear_archivos_pokemon("pepa", 200, 100, 100);
    //crear_archivos_pokemon("chancha",50,10,2);

    pthread_join(thread_new_pokemon, NULL);
    pthread_join(thread_catch_pokemon, NULL);
    pthread_join(thread_get_pokemon, NULL);

    bitarray_destroy(bitarray);
    log_destroy(logger);
    config_destroy(config_tall_grass);
    free(metadataTxt);
    free(datos_config->ip_broker);
    free(datos_config->pto_de_montaje);
    free(datos_config);

    return 0;
}

void verificar_punto_de_montaje() {
	DIR *rta = opendir(datos_config->pto_de_montaje);
	if(rta == NULL) {
		log_warning(logger,"DIRECTORIO %s NO EXISTE",datos_config->pto_de_montaje);
	    log_info(logger,"CREANDO DIRECTORIO %s",datos_config->pto_de_montaje);
	    crear_pto_de_montaje(datos_config->pto_de_montaje);
	} else {
	    log_info(logger,"DIRECTORIO %s ENCONTRADO",datos_config->pto_de_montaje);
	    //verificamos el bitmap, el archivo metadata.txt, los archivos pokemones, entre otras cosas.
	    verificar_metadata_txt(datos_config->pto_de_montaje);
	}
}

void iniciar_logger_config() {
	datos_config = malloc(sizeof(struct config_tallGrass));
	 metadataTxt = malloc(sizeof(struct metadata_info));

	logger = log_create("Tall_Grass_Logger","TG",1,LOG_LEVEL_INFO);
	config_tall_grass = config_create("/home/utnso/tp-2020-1c-Thrashing/gamecard/tall_grass.config");
}

void obtener_datos_archivo_config() {
	datos_config->tiempo_reintento_conexion = config_get_int_value(config_tall_grass, "TIEMPO_DE_REINTENTO_CONEXION");
	datos_config->tiempo_reintento_operacion = config_get_int_value(config_tall_grass,"TIEMPO_DE_REINTENTO_OPERACION");
	//datos_config->tiempo_retardo_operacion = config_get_int_value(config_tall_grass, "TIEMPO_RETARDO_OPERACION");

	int size_ip = strlen(config_get_string_value(config_tall_grass, "IP_BROKER")) + 1;
	datos_config->ip_broker = malloc(size_ip);
	memcpy(datos_config->ip_broker, config_get_string_value(config_tall_grass, "IP_BROKER"), size_ip);

	int size_pto_montaje = strlen(config_get_string_value(config_tall_grass, "PUNTO_MONTAJE_TALLGRASS")) + 1;
	datos_config->pto_de_montaje = malloc(size_pto_montaje);
	memcpy(datos_config->pto_de_montaje, config_get_string_value(config_tall_grass, "PUNTO_MONTAJE_TALLGRASS"), size_pto_montaje);

	datos_config->blocks = config_get_int_value(config_tall_grass, "BLOCKS");
	datos_config->size_block = config_get_int_value(config_tall_grass, "SIZE_BLOCK");
}

// Nota: Revisar memory leaks :-(
// Nota2: Para un futuro lejano hacer un dump del bitmap

void verificar_metadata_txt(char *path_pto_montaje) {

	//
    int size_new_string = strlen(path_pto_montaje) + strlen(METADATA_TXT_PATH) + 1;
    char *path_metadata_txt = malloc(size_new_string);
    strcpy(path_metadata_txt, path_pto_montaje);
    string_append(&path_metadata_txt, METADATA_TXT_PATH);

    FILE *filePointer = fopen(path_metadata_txt,"r");
    if(filePointer == NULL) {
        log_error(logger,"ERROR AL ABRIR EL ARCHIVO metadata.txt");
        exit(1);
    }

    //obtenemos los valores del archivo metadata.txt
    t_config *metadata_txt_datos = config_create(path_metadata_txt);
    metadataTxt->block_size = config_get_int_value(metadata_txt_datos,"BLOCK_SIZE");
    log_info(logger,"OBTENIENDO BLOCK_SIZE NUMBER METADATA.TXT: %d",metadataTxt->block_size);
    metadataTxt->blocks = config_get_int_value(metadata_txt_datos,"BLOCKS");
    log_info(logger,"OBTENIENDO CANTIDAD DE BLOCKS NUMBER METADATA.TXT: %d",metadataTxt->blocks);

    config_destroy(metadata_txt_datos);
    fclose(filePointer);
    free(path_metadata_txt);


    // creamos un nuevo string para armar el path de la metadata
    size_new_string = strlen(path_pto_montaje) + strlen(BITMAP_PATH) + 1;
    char *path_bitmap_bin = malloc(size_new_string);
    strcpy(path_bitmap_bin, path_pto_montaje);
    string_append(&path_bitmap_bin, BITMAP_PATH);

    log_info(logger,"OBTENER BITARRAY DEL ARCHIVO bitmap.bin");
    // abrimos el archivo bitmap.bin para obtener el bitmap
    FILE *bitmap_fp = fopen(path_bitmap_bin, "rb+");
    if(bitmap_fp == NULL) {
    	log_error(logger, "ERROR AL ABRIR EL ARCHIVO bitmap.bin");
    	exit(1);
    }

    fseek(bitmap_fp, 0, SEEK_END);				// posiciono el puntero hasta el final del archivo (EOF)
    int size_bitmap = (int) ftell(bitmap_fp);	// obtengo la posicion actual del puntero y lo casteo a un int. Lo hago para sacar el tamaño del archivo
    fseek(bitmap_fp, 0, SEEK_SET);				// vuelvo a posicionar el puntero al inicio del archivo
    //log_info(logger, "tamaño bitmap:%d",size_bitmap);

    void *buffer_bitmap = malloc(size_bitmap);

    if(fread(buffer_bitmap, sizeof(char), size_bitmap, bitmap_fp) == 0) {
    	log_error(logger, "NO SE HA PODIDO LEER EL CONTENIDO DEL ARCHIVO bitmap.bin");
    	exit(1);
    }

    bitarray = bitarray_create_with_mode(buffer_bitmap, size_bitmap, LSB_FIRST);

    for(int i = 0 ; i < bitarray_get_max_bit(bitarray) ; i++) {
        printf("%d",bitarray_test_bit(bitarray,i));
    }
    printf("\n");

    fclose(bitmap_fp);
    free(path_bitmap_bin);
}

// Funciones para crear el pto de montaje y otros directorios

void crear_pto_de_montaje(char *path_pto_montaje) {
    int rta_mkdir = mkdir(path_pto_montaje,S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    if(rta_mkdir == 0) {
        log_info(logger,"DIRECTORIO %s CREADO", path_pto_montaje);
        crear_metadata_tall_grass(path_pto_montaje);
        crear_directorio_files(path_pto_montaje);
        crear_directorio_blocks(path_pto_montaje);
    } else {
        log_error(logger,"ERROR AL CREAR EL DIRECTORIO %s",path_pto_montaje);
    }
}

void crear_directorio_files(char *path_pto_montaje) {
    int size_new_string = strlen(path_pto_montaje) + strlen(FILES_DIR) + 1;
    char *path_files = malloc(size_new_string);
    strcpy(path_files, path_pto_montaje);
    string_append(&path_files, FILES_DIR);

    int rta_mkdir = mkdir(path_files, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    if(rta_mkdir == 0) {
        log_info(logger,"DIRECTORIO %s CREADO", path_files);
    } else {
        log_error(logger,"ERROR AL CREAR EL DIRECTORIO %s",path_files);
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
        log_info(logger,"DIRECTORIO %s CREADO", path_blocks);
    } else {
        log_error(logger,"ERROR AL CREAR EL DIRECTORIO %s",path_blocks);
    }

    free(path_blocks);
}

void crear_metadata_tall_grass(char *path_pto_montaje) { //incluir en bitmap, supongo que en el primer bit
    int size_new_string = strlen(path_pto_montaje) + strlen(METADATA_DIR) + 1;
    char *path_metadata = malloc(size_new_string);
    strcpy(path_metadata, path_pto_montaje);
    string_append(&path_metadata, METADATA_DIR);

    int rta_mkdir = mkdir(path_metadata,S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    if(rta_mkdir == 0) {
        log_info(logger,"DIRECTORIO %s CREADO",path_metadata);
        crear_archivos_metadata(path_metadata);
    } else {
        log_error(logger,"ERROR AL CREAR EL DIRECTORIO %s",path_metadata);
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
    string_append(&info_metadataTxt, string_itoa(datos_config->size_block));
    string_append(&info_metadataTxt, "\n");
    string_append(&info_metadataTxt, "BLOCKS=");
    string_append(&info_metadataTxt, string_itoa(datos_config->blocks));
    string_append(&info_metadataTxt, "\n");
    string_append(&info_metadataTxt, "MAGIC_NUMBER=TALL_GRASS");
    string_append(&info_metadataTxt, "\n");

    if(fwrite(info_metadataTxt, string_length(info_metadataTxt), 1, metadataPointerFile) == 0) {
    	log_error(logger, "ERROR AL ESCRIBIR EL ARCHIVO metadata.txt");
    	exit(1);	// despues ver de implementar un goto en vez de exit()
    }

    free(info_metadataTxt);
    fclose(metadataPointerFile);

    //creamos un nuevo string que va a ser el path del bitmap.bin
    size_new_string = strlen(path_metadata) + strlen(BITMAP_FILE) + 1; // uso la misma variable que utilice en metadata.txt
    char *path_bitmap_file = malloc(size_new_string);
    strcpy(path_bitmap_file, path_metadata);
    string_append(&path_bitmap_file, BITMAP_FILE);

    // creamos el archivo bitmap.bin
    FILE *bitmapFilePointer = fopen(path_bitmap_file, "wb+");
    if(bitmapFilePointer == NULL) {
    	log_error(logger, "ERROR AL CREAR EL ARCHIVO bitmap.bin");
    }

    int size_bytes_bitmap = (datos_config->blocks / BITS);
    void *newArrayBit = malloc(size_bytes_bitmap);
    bitarray = bitarray_create_with_mode(newArrayBit, size_bytes_bitmap, LSB_FIRST);

    for(int i = 0 ; i < bitarray_get_max_bit(bitarray) ; i++) {
    	bitarray_clean_bit(bitarray,i);
    }

    //solo seteo dos bloques a modo de prueba, despues lo borro
    //bitarray_set_bit(bitArray,0);
    //bitarray_set_bit(bitArray,1);

    //escribimos el bitarray en el archivo
    if(fwrite(bitarray->bitarray, sizeof(char), size_bytes_bitmap, bitmapFilePointer) == 0) {
    	log_error(logger, "NO SE PUDO ESCRIBIR EL BITARRAY EN EL ARCHIVO bitmap.bin");
    	exit(1);
    }

    //fclose(bitmapFilePointer);

    free(path_metadata_file);
    free(path_bitmap_file);
}

//CREO QUE ESTO ES NECESARIO, SI VES EN LA PAG 18 EL SEGUNDO EJEMPLO DE PATH
char* crear_directorio_pokemon(char *path_pto_montaje, char* pokemon) {
		
    int size_new_string = strlen(path_pto_montaje) + strlen(POKEMON_DIR)+strlen(pokemon) + 1;
    char *path_files = malloc(size_new_string);
    strcpy(path_files, path_pto_montaje);
    string_append(&path_files, POKEMON_DIR);
	string_append(&path_files, pokemon);


    int rta_mkdir = mkdir(path_files, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    if(rta_mkdir == 0) {
        log_info(logger,"DIRECTORIO %s CREADO", path_files);
    } else {
        log_error(logger,"ERROR AL CREAR EL DIRECTORIO %s",path_files);
    }
	return path_files;
    free(path_files);
}

void crear_archivos_pokemon(char *pokemon, int posX, int posY, int cantidad) {
	char *path_file = string_new();
	string_append(&path_file, (crear_directorio_pokemon(datos_config->pto_de_montaje, pokemon))); 
	
	char *pokemon_file = string_new();
	string_append_with_format(&pokemon_file, "/%s%s", pokemon, POKEMON_FILE_EXT);

	char *path_pokemon_file = string_new();
	string_append(&path_pokemon_file, path_file);
	string_append(&path_pokemon_file, pokemon_file);
	

	log_info(logger, "CREANDO ARCHIVO %s", pokemon_file);
    FILE *pointerFile = fopen(path_pokemon_file,"w");
    if(pointerFile == NULL) {
    	log_error(logger, "ERROR AL CREAR EL ARCHIVO %s", path_pokemon_file);
    	exit(1);
    }

    crear_metadata_pokemon(path_file); // creamos la metadata del archivo pokemon y solo rellenamos el campo DIRECTORY y OPEN
	char *coordenadas = string_new();
	string_append_with_format(&coordenadas, "%s%s%s%s%s", string_itoa(posX), "-", string_itoa(posY), "=", string_itoa(cantidad));
	string_append(&coordenadas, "\n");


	int coordenadas_size = strlen(coordenadas);
	int cant_blocks = (int) ceil((double)coordenadas_size / datos_config->size_block);
	// aca determino la cantidad de bloques en base a lo que vaya a escribir en el archivo

	int desplazamiento = 0;
	for(int i = 0 ; i < cant_blocks ; i++) {
		int bloque = obtener_bloque_libre();
		if(bloque == -1) {
			log_warning(logger, "NO HAY ESPACIO SUFICIENTE EN EL BITMAP");
			break;
		}
		bitarray_set_bit(bitarray, bloque);
		agregar_bloque_metadata_pokemon(path_file, bloque);
		// crear el bloque en el directorio BLOCKS, crear el archivo y escribir la info
		char *auxiliar = strdup(string_substring(coordenadas, desplazamiento, datos_config->size_block));

		char *path_block_dir = string_new();
		string_append_with_format(&path_block_dir, "%s%s%s", datos_config->pto_de_montaje, BLOCKS_DIR, "/");

		escribir_datos_bloque(path_block_dir, auxiliar, bloque);
		desplazamiento += datos_config->size_block;

		free(path_block_dir);
	}

    //escribimos las coordenadas en el archivo
    if(fwrite(coordenadas, strlen(coordenadas) + 1, 1, pointerFile) == 0) {
    	log_error(logger, "NO SE PUDO ESCRIBIR LAS COORDENADAS EN EL ARCHIVO CON RUTA %s", pokemon_file);
    	exit(1);
    } else {
    	log_info(logger, "SE ESCRIBIERON LOS DATOS EN EL ARCHIVO CON RUTA %s", pokemon_file);
    }

	free(coordenadas);
    fclose(pointerFile);
    free(path_pokemon_file);
    free(pokemon_file);
    free(path_file);
}

int obtener_bloque_libre() {
	int resul = -1;
	for(int i = 0 ; i < bitarray_get_max_bit(bitarray) ; i++ ) {
		if(bitarray_test_bit(bitarray, i) == 0) {
			log_info(logger, "NUMERO DE BLOQUE LIBRE:%d",i);
			return i;
		}
	}
	return resul;
}
/*
void verificar_espacio_bloque_de_datos(char *path_metadata_pokemon, char *datos_a_agregar) { // chequea que haya espacio en el bloque , si supera el maximo se tiene que crear otro archivo

	char *path_block = string_new();
	string_append(&path_block, datos_config->pto_de_montaje);
	string_append(&path_block, BLOCKS_DIR);
	string_append(&path_block, "/");

	int size_new_string = strlen(path_metadata_pokemon) + strlen(METADATA_FILE) + 1;
	char *path_metadata_file = malloc(size_new_string);
	strcpy(path_metadata_file, path_metadata_pokemon);
	string_append(&path_metadata_file, METADATA_FILE);

	t_config *metadata_pokemon = config_create(path_metadata_file);
	char **array_blocks = config_get_array_values(metadata_pokemon, "BLOCKS");

	int data_size = strlen(datos_a_agregar);
	int cant_blocks = (int) ceil((double)data_size / datos_config->size_block);

	if(array_blocks == NULL) {
		for(int i = 0 ; i < cant_blocks ; i++) {
			int nro_bloque = obtener_bloque_libre();
			if(nro_bloque == -1)
				log_warning(logger, "NO HAY BLOQUE DE DATOS DISPONIBLE");
				break;
			int size_to_write = data_size - datos_config->size_block;
			if(size_to_write < 0) { // este if es para escribir datos con lengitud menor al tamaño del bloque de datos
				escribir_datos_bloque(path_block, datos_a_agregar, nro_bloque);
			}
			escribir_datos_bloque();
		}

	}
}
*/
void escribir_datos_bloque(char *path_block_dir, char *datos_a_agregar, int nro_bloque) {

	char *block_file_path = string_new();
	string_append(&block_file_path, path_block_dir);
	string_append(&block_file_path, string_itoa(nro_bloque));
	string_append(&block_file_path, ".txt"); //por ahora txt para ver que los datos se escribieron correctamente, despues se cambia a bin

	log_info(logger, "CREANDO EL ARCHIVO CON RUTA %s", block_file_path);
	FILE *pf = fopen(block_file_path, "ab");
	if(pf == NULL) {
		log_error(logger, "ERROR AL CREAR EL ARCHIVO POKEMON");
		exit(1);
	}

	log_info(logger, "ESCRIBIENDO DATOS EN EL BLOQUE %d EN EL DIRECTORIO %s", nro_bloque, block_file_path);
	if(fwrite(datos_a_agregar, strlen(datos_a_agregar), 1, pf) == 0)
		log_error(logger, "NO SE HA PODIDO ESCRIBIR DATOS EN EL DIRECTORIO %s", block_file_path);

	fclose(pf);
	free(block_file_path);
}

void agregar_bloque_metadata_pokemon(char *path_pokemon_file, int nro_bloque) {

	int size_nro_bloque = strlen(string_itoa(nro_bloque));

	int size_new_string = strlen(path_pokemon_file) + strlen(METADATA_FILE) + 1;
	char *path_metadata_file = malloc(size_new_string);
	strcpy(path_metadata_file, path_pokemon_file);
	string_append(&path_metadata_file, METADATA_FILE);

	t_config *metadata_pokemon = config_create(path_metadata_file);

	char *blocks = config_get_string_value(metadata_pokemon, "BLOCKS");
	int sizeBlocks = strlen(blocks);

	if(sizeBlocks == 2) {
		sizeBlocks += size_nro_bloque;
	} else {
		sizeBlocks += size_nro_bloque + 1;
	}

	log_info(logger, "AGREGANDO BLOQUE %d A LA ESTRUCTURA BLOCKS", nro_bloque);
	char *newBlocks = malloc(sizeBlocks);
	int posicion = 0;

	if(strlen(blocks) < 3) {
		strcpy(newBlocks + posicion, "[");
		posicion ++;
		strcpy(newBlocks + posicion, string_itoa(nro_bloque));
		posicion += size_nro_bloque;
		strcpy(newBlocks + posicion, "]");
	} else {
		//[10] --->
		char *auxiliar = strdup(string_substring(blocks,1, strlen(blocks)-2));
		strcpy(newBlocks + posicion, "[");
		posicion ++;
		strcpy(newBlocks + posicion, auxiliar);
		posicion += strlen(auxiliar); // revisar esta linea
		strcpy(newBlocks + posicion, ",");
		posicion ++;
		strcpy(newBlocks + posicion, string_itoa(nro_bloque));
		posicion += size_nro_bloque;
		strcpy(newBlocks + posicion, "]");
		free(auxiliar);
	}

	config_set_value(metadata_pokemon, "BLOCKS", newBlocks);
	config_save(metadata_pokemon);
	config_destroy(metadata_pokemon);

}

void crear_metadata_pokemon(char *path_file) {

    //creamos un nuevo string que va a ser el path del metadata.txt
	int size_new_string = strlen(path_file) + strlen(METADATA_FILE) + 1;
    char *path_metadata_file = malloc(size_new_string);
    strcpy(path_metadata_file, path_file);
    string_append(&path_metadata_file, METADATA_FILE);

    log_info(logger, "CREANDO ARCHIVO %s", METADATA_FILE);
    FILE *metadataPointerFile = fopen(path_metadata_file, "w+");
    if(metadataPointerFile == NULL) {
    	log_error(logger, "ERROR AL CREAR EL ARCHIVO %s", path_metadata_file);
    	exit(1);
    }
    int tamanio = fileSize(path_file);

    char *info_metadataTxt = string_new();
    string_append(&info_metadataTxt, "DIRECTORY=N");
    string_append(&info_metadataTxt, "\n");
    string_append(&info_metadataTxt, "SIZE=");
    string_append(&info_metadataTxt, string_itoa(tamanio));
    string_append(&info_metadataTxt, "\n");
    string_append(&info_metadataTxt, "BLOCKS=[]");
    string_append(&info_metadataTxt, "\n");
    string_append(&info_metadataTxt, "OPEN= N");
    string_append(&info_metadataTxt, "\n");
	
    if(fwrite(info_metadataTxt, string_length(info_metadataTxt), 1, metadataPointerFile) == 0) {
    	log_error(logger, "ERROR AL ESCRIBIR EL ARCHIVO metadata.txt");
    	exit(1);	// despues ver de implementar un goto en vez de exit()
    }
	
    free(info_metadataTxt);
    fclose(metadataPointerFile);
    free(path_metadata_file);

}

int fileSize(char* file) { 
        FILE* fp = fopen(file, "r"); 
        if (fp == NULL) { 
       log_error(logger, "No existe el archivo.");
        return -1; 
    } 
      fseek(fp, 0L, SEEK_END);  
    int res = (int) ftell(fp); 
    fclose(fp); 
    return res; 
} 
FILE* existePokemon(char* nombrePokemon){
    char* pathArchivo = string_new();
    string_append(&pathArchivo,PATH_POKECARPETA);
    string_append_with_format(&pathArchivo,"%s%s%s%s", nombrePokemon, "/",nombrePokemon,POKEMON_FILE_EXT);
    FILE *fp = fopen(pathArchivo,"r");
    printf("%s\n",pathArchivo);
    printf("%p",fp);
    printf("\nDeberia impreso el puntero\n");
    return fp;

}
char *read_file_into_buf (char **filebuf, long fplen, FILE *fp)
{
    fseek (fp, 0, SEEK_END);
    if ((fplen = ftell (fp)) == -1) {  /* get file length */
        fprintf (stderr, "error: unable to determine file length.\n");
        return NULL;
    }
    fseek (fp, 0, SEEK_SET);  /* allocate memory for file */
    if (!(*filebuf = calloc (fplen, sizeof *filebuf))) {
        fprintf (stderr, "error: virtual memory exhausted.\n");
        return NULL;
    }

    /* read entire file into filebuf */
    if (!fread (*filebuf, sizeof *filebuf, fplen, fp)) {
        fprintf (stderr, "error: file read failed.\n");
        return NULL;
    }

    return *filebuf;
}

//CAMBIE  NOMBRE PORQUE ROMPE AHORA QUE ESTAN LOS UTILS :)
void newPokemon(char* pokemon,int posx,int posy,int cant){}

void catchPokemon(char* pokemon,int posx,int posy){}

/*rtaGet* getPokemon(int idMensaje, char* pokemon){
    rtaGet* respuesta = malloc(sizeof(rtaGet));
    respuesta->id_mensaje = idMensaje;
    respuesta->name = pokemon;
    FILE *fp = existePokemon(pokemon);
    printf("%p",fp);
    if(fp != NULL){
        char* pathMetadata = string_new();
        string_append(&pathMetadata,PATH_POKECARPETA);
        string_append_with_format(&pathMetadata,"%s%s", pokemon ,METADATA_FILE);
        t_config *metadata_pokemon = config_create(pathMetadata);
        
        char* abierto = config_get_string_value(metadata_pokemon,"OPEN");
        if(abierto == "Y"){
            //finalizar hilo y reintentar despues del tiempo que dice el config
        } else{
            //settear OPEN = Y
            log_warning(logger, "NO HAY ESPACIO SUFICIENTE EN EL BITMAP");
            char* pathFILE = string_new();
            string_append(&pathFILE,PATH_POKECARPETA);
            string_append_with_format(&pathFILE,"%s%s%s%s", pokemon, "/",pokemon, POKEMON_FILE_EXT);
            char* scaneo = NULL;
            long fplen = (long) fileSize(pathFILE);
            read_file_into_buf (&scaneo, fplen, fp);
            log_info(logger, "AGREGANDO BLOQUE %d A LA ESTRUCTURA BLOCKS", string_length(scaneo));
              printf("deberia saber el length\n");
            //settear OPEN = N
            //falta separar uno a uno las lineas

            //setear la respuesta

            free(scaneo);
        }
    
    free(abierto);
    free(pathMetadata);
    }

    return respuesta;
}*/
