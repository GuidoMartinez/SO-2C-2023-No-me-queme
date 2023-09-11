#include "shared_utils.h"

char *mi_funcion_compartida()
{ // TODO -- BORRAR -- PARA PROBAR CONFIG DE SHARED
	return "Hice uso de la shared!";
}

void enviar_mensaje(char *mensaje, int socket_cliente)
{
	t_paquete *paquete = malloc(sizeof(t_paquete));

	paquete->codigo_operacion = MENSAJE;
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = strlen(mensaje) + 1;
	paquete->buffer->stream = malloc(paquete->buffer->size);
	memcpy(paquete->buffer->stream, mensaje, paquete->buffer->size);

	int bytes = paquete->buffer->size + 2 * sizeof(int);
	void *a_enviar = serializar_paquete(paquete, bytes);
	send(socket_cliente, a_enviar, bytes, 0);

	free(a_enviar);
	eliminar_paquete(paquete);
}

void *serializar_paquete(t_paquete *paquete, int bytes)
{
	void *magic = malloc(bytes);
	int desplazamiento = 0;

	memcpy(magic + desplazamiento, &(paquete->codigo_operacion), sizeof(int));
	desplazamiento += sizeof(int);
	memcpy(magic + desplazamiento, &(paquete->buffer->size), sizeof(int));
	desplazamiento += sizeof(int);
	memcpy(magic + desplazamiento, paquete->buffer->stream, paquete->buffer->size);
	desplazamiento += paquete->buffer->size;

	return magic;
}

void crear_buffer(t_paquete *paquete)
{
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = 0;
	paquete->buffer->stream = NULL;
}

t_paquete *crear_paquete(void)
{
	t_paquete *paquete = malloc(sizeof(t_paquete));
	paquete->codigo_operacion = PAQUETE;
	crear_buffer(paquete);
	return paquete;
}

t_paquete *crear_paquete_con_codigo_de_operacion(uint32_t codigo)
{
	t_paquete *paquete = malloc(sizeof(t_paquete));

	paquete->codigo_operacion = codigo;
	crear_buffer(paquete);
	return paquete;
}

void agregar_a_paquete(t_paquete *paquete, void *valor, int tamanio)
{
	paquete->buffer->stream = realloc(paquete->buffer->stream, paquete->buffer->size + tamanio + sizeof(int));

	memcpy(paquete->buffer->stream + paquete->buffer->size, &tamanio, sizeof(int));
	memcpy(paquete->buffer->stream + paquete->buffer->size + sizeof(int), valor, tamanio);

	paquete->buffer->size += tamanio + sizeof(int);
}

void enviar_paquete(t_paquete *paquete, int socket_cliente)
{
	int bytes = paquete->buffer->size + 2 * sizeof(int);
	void *a_enviar = serializar_paquete(paquete, bytes);
	send(socket_cliente, a_enviar, bytes, 0);

	free(a_enviar);
}

void eliminar_paquete(t_paquete *paquete)
{

	free(paquete->buffer->stream);
	free(paquete->buffer);
	free(paquete);
}

// CONEXION INICIAL

int crear_conexion(char *ip, char *puerto)
{
	struct addrinfo hints;
	struct addrinfo *server_info;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	getaddrinfo(ip, puerto, &hints, &server_info);

	int socket_cliente = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);

	if (connect(socket_cliente, server_info->ai_addr, server_info->ai_addrlen) == -1)
	{
		printf("error"); // TODO // eliminar
		// freeaddrinfo(server_info); // TODO -- VER SI TIENE QUE ESTAR
		return -1;
	}
	freeaddrinfo(server_info);

	return socket_cliente;
}

int iniciar_servidor(t_log *logger, char *IP, char *PUERTO)
{
	int socket_servidor;
	struct addrinfo hints;
	struct addrinfo *serverInfo;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	int rv = getaddrinfo(IP, PUERTO, &hints, &serverInfo);
	if (rv != 0)
	{
		printf("getaddrinfo error: %s\n", gai_strerror(rv));
		return EXIT_FAILURE;
	}

	socket_servidor = socket(serverInfo->ai_family,
							 serverInfo->ai_socktype,
							 serverInfo->ai_protocol);

	if (socket_servidor == -1)
	{
		printf("Socket creation error\n%s", strerror(errno));
		return EXIT_FAILURE;
	}
	bind(socket_servidor, serverInfo->ai_addr, serverInfo->ai_addrlen);
	listen(socket_servidor, SOMAXCONN);

	freeaddrinfo(serverInfo);
	return socket_servidor;
}

int esperar_cliente(int socket_servidor, t_log *logger)
{
	int socket_cliente = accept(socket_servidor, NULL, NULL);
	// log_info(logger, "Se conecto un cliente!");

	return socket_cliente;
}

int crear_socket_escucha(char *puerto_escucha)
{
	struct sockaddr_in direccion_servidor;
	memset(&direccion_servidor, 0, sizeof(direccion_servidor));
	direccion_servidor.sin_family = AF_INET;
	direccion_servidor.sin_addr.s_addr = INADDR_ANY;
	direccion_servidor.sin_port = htons(atoi(puerto_escucha));

	int socket_servidor = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_servidor == -1)
	{
		perror("Error al crear el socket");
		exit(EXIT_FAILURE);
	}

	int activado = 1;
	if (setsockopt(socket_servidor, SOL_SOCKET, SO_REUSEADDR, &activado, sizeof(activado)) == -1)
	{
		perror("Error al configurar el socket");
		exit(EXIT_FAILURE);
	}

	if (bind(socket_servidor, (void *)&direccion_servidor, sizeof(direccion_servidor)) != 0)
	{
		perror("Error al bindear el socket");
		exit(EXIT_FAILURE);
	}

	if (listen(socket_servidor, SOMAXCONN) == -1)
	{
		perror("Error al poner el servidor en modo escucha");
		exit(EXIT_FAILURE);
	}

	return socket_servidor;
}

int recibir_operacion(int socket_cliente)
{
	int cod_op;
	if (recv(socket_cliente, &cod_op, sizeof(int), MSG_WAITALL) > 0)
		return cod_op;
	else
	{
		close(socket_cliente);
		return -1;
	}
}

void *recibir_buffer(int *size, int socket_cliente)
{
	void *buffer;

	recv(socket_cliente, size, sizeof(int), MSG_WAITALL);
	buffer = malloc(*size);
	recv(socket_cliente, buffer, *size, MSG_WAITALL);

	return buffer;
}

void *recibir_mensaje(int socket_cliente, t_log *logger)
{
	int size;
	char *buffer = recibir_buffer(&size, socket_cliente);
	log_info(logger, "Me llego el mensaje %s", buffer);
	return buffer;
}

t_list *recibir_paquete(int socket_cliente)
{
	int size;
	int desplazamiento = 0;
	void *buffer;
	t_list *valores = list_create();
	int tamanio;

	buffer = recibir_buffer(&size, socket_cliente);
	while (desplazamiento < size)
	{
		memcpy(&tamanio, buffer + desplazamiento, sizeof(int));
		desplazamiento += sizeof(int);
		char *valor = malloc(tamanio);
		memcpy(valor, buffer + desplazamiento, tamanio);
		desplazamiento += tamanio;
		list_add(valores, valor);
	}
	free(buffer);
	return valores;
	return NULL;
}

void realizar_handshake(int conexion, op_code codigo_op, t_log *logger)
{

	t_paquete *paquete_handshake = crear_paquete_con_codigo_de_operacion(codigo_op);
	enviar_paquete(paquete_handshake, conexion);
	eliminar_paquete(paquete_handshake);
	op_code recibir_cod_op = recibir_operacion(conexion);
	if (recibir_cod_op == MENSAJE)
	{
		char *mensaje = recibir_mensaje(conexion, logger);
		free(mensaje);
	}
	else
	{
		log_warning(logger, "Operación desconocida. No se pudo recibir la respuesta de la memoria.");
	}
}

const char *obtener_nombre_instruccion(nombre_instruccion instruccion)
{
	switch (instruccion)
	{
	case SET:
		return "SET";
	case ADD:
		return "ADD";
	case SUB:
		return "SUB";
	case JNZ:
		return "JNZ";
	case SLEEP:
		return "SLEEP";
	case MOV_IN:
		return "MOV_IN";
	case MOV_OUT:
		return "MOV_OUT";
	case WAIT:
		return "WAIT";
	case SIGNAL:
		return "SIGNAL";
	case F_OPEN:
		return "F_OPEN";
	case F_TRUNCATE:
		return "F_TRUNCATE";
	case F_SEEK:
		return "F_SEEK";
	case F_CREATE:
		return "F_CREATE";
	case F_WRITE:
		return "F_WRITE";
	case F_READ:
		return "F_READ";
	case F_CLOSE:
		return "F_CLOSE";
	case EXIT:
		return "EXIT";
	default:
		printf("imprimo el num %d", instruccion);
		return "DESCONOCIDO";
	}
}

uint32_t str_to_uint32(char *str)
{
    char *endptr;
    uint32_t result = (uint32_t)strtoul(str, &endptr, 10);

    // Comprobar si hubo errores durante la conversión
    if (*endptr != '\0')
    {
        fprintf(stderr, "Error en la conversión de '%s' a uint32_t.\n", str);
        exit(EXIT_FAILURE);
    }

    return result;
}