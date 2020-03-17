#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>

#define BUFLEN 256

using namespace std;

int main(int argc, char *argv[])
{
    int sockfd, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    FILE *file;

    int pid = getpid();
    char denumire[10];
    sprintf(denumire, "client-%d.log", pid);

    // daca nu este introdusa corect comanda de compilare
    char buffer[BUFLEN];
    if (argc < 3) {
       fprintf(stderr,"Usage %s <IP_server> <port_server>\n", argv[0]);
       exit(0);
    }  
    

    // cream socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Client -10 : Eroare la apel creare socket");
        exit(0);
    }
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[2]));
    inet_aton(argv[1], &serv_addr.sin_addr);
    
    // ne conectam
    if (connect(sockfd,(struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("Client -10 : Eroare la apel connect"); 
        exit(0);
    }

    fd_set read_fds;
    fd_set tmp_fds;
    FD_SET(sockfd, &read_fds);
    // 0 pentru ca citim de la tastatura
    FD_SET(0, &read_fds);
    int fdmax = sockfd;
    
    int sesiune_deschisa = 0;
    file = fopen(denumire, "w");

    while (1){
        tmp_fds = read_fds;
        select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);

        int i;
        for (i = 0; i <= fdmax; i++){
            if (FD_ISSET(i, &tmp_fds)){
                if (i == 0){
                    //citesc de la tastatura
                    memset(buffer, 0 , BUFLEN);
                    fgets(buffer, BUFLEN - 1, stdin);
                    fwrite(buffer, 1, strlen(buffer), file);

                    char login[] = {'l','o','g','i','n'};
                    char quit[] = {'q','u','i','t'};
                    int diferit_login = 0;
                    int diferit_quit = 0;

                    // verificam daca s-a introdus la tastatura comanda login
                    for (int j = 0; j < 5; j++){
                    	if (buffer[j] != login[j]){
                    		diferit_login = 1;
                    	}
                    }


                    // verificam daca s-a introdus la tastatura comanda quit
                    for (int j = 0; j < 4; j++){
                    	if (buffer[j] != quit[j]){
                    		diferit_quit = 1;
                    	}
                    }

                    // daca s-a introdus quit
                    if (diferit_quit == 0){
                    	
                    	n = send(sockfd,buffer,strlen(buffer), 0); 
                    	if (n < 0) {
                        	perror("Client -10 : Eroare la apel send");
                        	exit(0);
                    	}
                    	fclose(file);
                    	return 0;
                    }
                    

                    // daca s-a introdus login si este deja sesiune deschisa
                    if (diferit_login == 0 && sesiune_deschisa == 1){
                    	// clientul returneaza eroare si nu trimite nimic serverului
                    	fwrite("IBANK> -2 : Sesiune deja deschisa\n", 1, 34, file);
                    	printf("IBANK> -2 : Sesiune deja deschisa\n");
                    	continue;
                    }

                    if (strcmp(buffer, "logout\n") == 0 && sesiune_deschisa == 0){
                    	// daca clientul nu este logat, se intoarce codul de eroare -1
                    	// si nu se trimite nimic la server
                    	fwrite("IBANK> -1 : Clientul nu este autentificat\n", 1, 42, file);
                    	printf("IBANK> -1 : Clientul nu este autentificat\n");
                    	continue;
                    }

                    // trimit mesaj la server

                    n = send(sockfd,buffer,strlen(buffer), 0); 
                    if (n < 0) {
                        perror("Client -10 : Eroare la apel send");
                        exit(0);
                    }

                } else {
                    memset(buffer, 0, BUFLEN);
                    if ((n = recv(i, buffer, sizeof(buffer), 0)) <= 0) {
						if (n == 0) {
							//conexiunea s-a inchis
							printf("Conexiune inchisa de server\n");
							fclose(file);
							exit(1);
						} else {
							perror("Eroare la apel recv");
							exit(1);
						}
					} else { // am primit raspuns de la server
	                    char welcome[] = {'W','e','l','c','o','m','e'};
	                    char deconectat[] = {'d','e','c','o','n','e','c','t','a','t'};
	                    int diferit_welcome = 0;
	                    int diferit_deconectat = 0;

	                    for (int j = 0; j < 7; j++){
	                    	if (buffer[j + 7] != welcome[j]){
	                    		// nui egal cu welcome
	                    		diferit_welcome = 1;
	                    	}
	                    }

	                    for (int j = 0; j < 10; j++){
	                    	if (buffer[j + 23] != deconectat[j]){
	                    		// nui egal cu deconectat
	                    		diferit_deconectat = 1;
	                    	}
	                    }

	                    if (diferit_welcome != 1){
	                    	// daca am primit mesaj de welcome, atunci este deschisa sesiunea
	                    	sesiune_deschisa = 1;
	                    }

	                    if (diferit_deconectat != 1){
	                    	// daca am primit mesaj cu deconectat, atunci sesiunea se inchide
	                    	sesiune_deschisa = 0;
	                    }

	                    // printam raspunsul primit pentru un feedback utilizatorului
	                    if (strlen(buffer) != 0){
	                    	fwrite(buffer, 1, strlen(buffer), file);
	        				printf("%s\n", buffer);
	                	}
                	}
            	}
        	}
        }
    }
    fclose(file);
    return 0;
}


