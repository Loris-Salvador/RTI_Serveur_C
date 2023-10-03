.SILENT:

SERV_OVESP = ./OVESP
SOCK = ./LibSocket

all:	Serveur

Serveur:	$(SERV_OVESP)/OVESP.o $(SOCK)/socket.o
	echo Creation Serveur
	g++ -o ./Serveur ./Serveur.cpp $(SOCK)/socket.o $(SERV_OVESP)/OVESP.o -I/usr/include/mysql -lpthread -L/usr/lib64/mysql -lmysqlclient


$(SERV_OVESP)/OVESP.o:	$(SERV_OVESP)/OVESP.cpp
	echo Creation OVESP...
	g++ -c $(SERV_OVESP)/OVESP.cpp -o $(SERV_OVESP)/OVESP.o -I/usr/include/mysql -lpthread -L/usr/lib64/mysql -lmysqlclient -lz -lm -lrt -lssl -lcrypto -ldl

$(SOCK)/socket.o:	$(SOCK)/socket.cpp
	echo Creation LibSocket...
	g++ -c $(SOCK)/socket.cpp -o $(SOCK)/socket.o


clean:
	rm $(SERV_OVESP)/*o
	rm $(SOCK)/*o
	echo Objets supprimes

clobber:
	rm ./Serveur
	echo Executable supprimes
