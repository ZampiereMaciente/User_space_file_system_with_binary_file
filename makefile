############
# Variáveis
############

# Nome do projeto
NAME=usfs

# arquivos .cpp
CPP_SOURCE=$(wildcard ./src/*.cpp)

# Compilador
COMPILE=g++

RM=rm -rf

#############
# Compilação
#############

# Cria o diretório de objetos e compila o projeto
all:
	@ echo 'Compiling main.cpp on binary file: $@'
	$(COMPILE) $(CPP_SOURCE) -o $(NAME)
	@ echo 'Finished compilation job: $@'

clean:
	@ echo 'Removing objects and binary files: $@'
	@ $(RM) *.o *.out $(NAME) *~ *.bin
	@ echo 'Finished cleaning: $@'

# Phony target serve pra não dar conflito com arquivos de mesmo nome (como por exemplo, se existise um
# all.cpp ou clean.cpp).
.PHONY: all clean
