# Compilador e flags
CC = gcc
CFLAGS = -Wall -I.
LDFLAGS = 

# Diretórios e arquivos
RDT_SRC = rdt/rdt_3.c
UTILS_SRC = rdt/utils.c
SERVER_SRC = server.c
CLIENT_SRC = client.c

# Executáveis
SERVER = server
CLIENT = client

# Valores padrão
PORT ?= 8080
IP ?= 127.0.0.1

# Regras de compilação
all: $(SERVER) $(CLIENT)

$(SERVER): $(SERVER_SRC) $(RDT_SRC) $(UTILS_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(CLIENT): $(CLIENT_SRC) $(RDT_SRC) $(UTILS_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Execução
run-server: $(SERVER)
	./$(SERVER) $(PORT) > log_server.txt

run-client: $(CLIENT)
	./$(CLIENT) $(IP) $(PORT) > log_client.txt

# Limpeza
clean:
	rm -f $(SERVER) $(CLIENT) *.o rdt/*.o log*.txt
	> received.txt

.PHONY: all clean run-server run-client