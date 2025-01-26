# Compilador e flags
CC = gcc
CFLAGS = -Wall -I.
LDFLAGS = 

# Diretórios e arquivos
RDT_SRC = rdt/rdt_2.c
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

$(SERVER): $(SERVER_SRC) $(RDT_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(CLIENT): $(CLIENT_SRC) $(RDT_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Execução
run-server: $(SERVER)
	./$(SERVER) $(PORT)

run-client: $(CLIENT)
	./$(CLIENT) $(IP) $(PORT)

# Limpeza
clean:
	rm -f $(SERVER) $(CLIENT) *.o rdt/*.o

.PHONY: all clean run-server run-client