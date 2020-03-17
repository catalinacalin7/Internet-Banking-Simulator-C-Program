#include <iostream>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <map>
#include <vector>

#define MAX_CLIENTS	10
#define BUFLEN 256
#define MAX_CARDS 1000

using namespace std;

int main(int argc, char *argv[])
{
    int sockfd, newsockfd, portno;
    socklen_t clilen;
    // buffer pentru scriere/citire
    char buffer[BUFLEN];
    char read_buff[BUFLEN];
    struct sockaddr_in serv_addr, cli_addr;
    int n, i, j;
    FILE *fp;
    char * line = NULL;
    size_t len = 0;
    ssize_t read;
    // vector ce va contine pe pozitia i, starea de logare a clientului
    // i :  1 = cand acesta este logat, 0 = cand este nelogat
    int client_login[MAX_CLIENTS];
    // vector ce va contine pe pozitia i, starea de transfer a clientului
    // i :  1 = cand acesta asteapta o confirmare pentru transfer , 
    // 0 = cand nu asteapta nimic
    int client_transfer[MAX_CLIENTS];
    // cheia : nr cardului, valoarea.first : se seteaza la 1 cand un client 
    // se logeaza pe cardul din cheie, valoarea.second : se incrementeaza 
    // cand cineva introduce un pin incorect, == 2 => blocat 
    map<int, pair <int, int> > cards;
    // cheia : un client, valoarea : cardul la care este logat
    map<int, int> client_cards;
    // cheia : un nr de card, valoare : sold cont aferent cardului
    map<int, double> listsolds;
    // vector cu carduri logate
    int cards_logged[MAX_CARDS];
    // dimensiunea vectorului cards_logged
    int size_cards_logged = 0;
    // cheia : cardul de la care urmeaza sa transfer bani, 
    // valoare : first - cardul cui fac transfer, second - suma transferului
    map<int, pair <int, double> > transfers;

    for (int i = 0; i < 10; ++i) {
    	// la inceput nici un client nu este logat
    	client_login[i] = 0;
    }

    // multimea de citire folosita in select()
 	fd_set read_fds;
 	// multime folosita temporar 
 	fd_set tmp_fds;
 	// valoare maxima din multimea read_fds
 	int fdmax;

 	// daca nu este introdusa corect comanda de compilare
    if (argc < 3) {
        fprintf(stderr,"Utilizare : %s <port_server> <users_data_file> \n", argv[0]);
        exit(1);
    }

    //golim multimile
    FD_ZERO(&read_fds);
    FD_ZERO(&tmp_fds);
     
    // cream socketul
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
       perror("IBANK> -10 : Eroare la apel creare socket");
       exit(1);
    }
    
    // portul primit ca parametru
    portno = atoi(argv[1]);


    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    // foloseste adresa IP a masinii
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
    
    // legare socket de port cunoscut
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr)) < 0){
    	perror("IBANK> -10 : Eroare la apel bind");
     	exit(1);
    } 
            
    // socket pasiv, pentru conectare
    listen(sockfd, MAX_CLIENTS);

    //adaugam noul file descriptor (socketul pe care se asculta conexiuni) in multimea read_fds
    FD_SET(sockfd, &read_fds);
    // 0 ca sa citim de la tastatura
    FD_SET(0, &read_fds);
    fdmax = sockfd;

    // main while
	while (1) {
		tmp_fds = read_fds; 
		// pentru controlarea a mai multi descriptori folosesc select
		if (select(fdmax + 1, &tmp_fds, NULL, NULL, NULL) == -1) {
			perror("IBANK> -10 : Eroare la apel select");
			exit(1);
		}

		for(i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &tmp_fds)) {

				if (i == 0){
					// citim de la tastatura
					memset(buffer, 0 , BUFLEN);
                    fgets(buffer, BUFLEN - 1, stdin);
                    char quit[] = {'q','u','i','t'};
                    int diferit_quit = 0;

           			// verificam daca s-a introdus quit
                    for (int j = 0; j < 4; j++){
                    	if (buffer[j] != quit[j]){
                    		diferit_quit = 1;
                    	}
                    }

                    // daca s-a introdus quit
                    if (diferit_quit == 0){
                    	return 0;
                    }

				} else if (i == sockfd) {
					// o noua conexiune a venit pe socketul inactiv(cu listen)
					clilen = sizeof(cli_addr);
					// acceptam conexiunea
					if ((newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen)) == -1) {
						perror("IBANK> -10 : Eroare la apel accept");
						exit(1);
					} else {
						//adaug noul socket la multimea descriptorilor de citire
						FD_SET(newsockfd, &read_fds);
						if (newsockfd > fdmax) { 
							fdmax = newsockfd;
						}
					}
				} else {
					// am primit date pe unul din socketii cu care vorbesc cu clientii
					memset(buffer, 0, BUFLEN);
					// primim datele
					if ((n = recv(i, buffer, sizeof(buffer), 0)) <= 0) {
						if (n == 0) {
							//conexiunea s-a intrerupt
							printf("Clientul %d s-a deconectat\n", i);
						} else {
							exit(1);
						}
						close(i);
						// scoatem din multimea de citire socketul care s-a deconectat
						FD_CLR(i, &read_fds); 
						
					} else { // daca recv > 0, am primit date
						printf ("Am primit de la clientul de pe socketul %d comanda: %s\n", i, buffer);
						char copy_buffer[BUFLEN];
						strcpy(copy_buffer, buffer);
						
						// vedem ce comanda avem
						char * comanda = NULL;
						comanda = strtok(copy_buffer, " ");
						// daca este comanda login si clientul nu este logat anterior
						if (strcmp(comanda, "login") == 0 && client_login[i] == 0){
							// separam numarul cardului si pinul
							char * nr_card_introdus = NULL;
							nr_card_introdus = strtok(NULL, " ");
							
							// verificam daca nu este logat nici un client pe cardul dat
							int logat = 0;
							
							int nr_card_intr_int = atoi(nr_card_introdus);
							for (int c = 0; c < MAX_CARDS; c++){
								if (cards_logged[c] == nr_card_intr_int){
									logat = 1;
								}
							}

							// daca nu este nimeni logat pe acest card
							if (logat == 0){
								// memorizam pinul din comanda
								char * pin_introdus = strtok(NULL, " \n");

								// deschidem fisierul
								fp = fopen(argv[2], "r");
								if (fp == NULL){
									exit(EXIT_FAILURE);
								}

								int first_line = 1;
								int readed_lines = 0;
								int gasit = 0;
								// citim linie cu linie din fisier
								while (fgets(read_buff, sizeof(read_buff), fp) != NULL) {
									// scoatem \n care este adaugat de fgets
	    							read_buff[strlen(read_buff) - 1] = '\0';
	    							// contorizam cate linii am citit
	    							readed_lines++;

	    							// daca e prima linie din fisier, retinem nr de pe ea
	    							int lines;
	    							if (first_line){
	    								lines = read_buff[0] - 48;
	    								first_line = 0;
	    								continue;
	    							}

									char cpy_buffer[BUFLEN];
									strcpy(cpy_buffer, read_buff);
	    							char * word;
	    							char * nume, * prenume, *nr_card_fisier, *pin_fisier;

	    							// memorizam numele, prenumele, cardul din fisier
									nume = word = strtok(cpy_buffer, " ");
									prenume = word = strtok(NULL, " ");
									nr_card_fisier = word = strtok(NULL, " ");

									if (nr_card_fisier != NULL) {
										// nr card introdus corect
										if (strcmp(nr_card_introdus, nr_card_fisier) == 0){
											// memorizam si pinul
											pin_fisier = word = strtok(NULL, " ");
											// am gasit in fisier cardul introdus
											gasit = 1;

											// cautam in mapul cards daca nu este introdus deja
											// cardul dat
											std::map<int, pair <int, int> >::iterator it;
											it = cards.find(nr_card_intr_int);
											pair<int, int> date_init = make_pair(0,0);


											// daca nu este, il introducem
											if (it == cards.end())
												cards[nr_card_intr_int] = date_init;

											// comparam pinul introdus si cel din fisier
											// cardul nu trebuie sa fie logat (.first == 0)
											// si sa nu fie gresit pinul mai mult de 3 ori(.second < 2)
											if (strcmp(pin_introdus, pin_fisier) == 0 && 
												cards[nr_card_intr_int].first == 0 &&
												cards[nr_card_intr_int].second < 2){

												// cream raspunsul
												sprintf(buffer, "IBANK> Welcome %s %s\n", nume, prenume);
												// clientul dat are o sesiune deschisa
												client_login[i] = 1;
												int card_int = atoi(nr_card_introdus);
												// asociem clientului i cardul dat
												client_cards[i] = card_int;
												// cardul devine devine logat
												cards[nr_card_intr_int].first = 1;
												// introducem cardul in vectorul de carduri logate
												cards_logged[size_cards_logged] = nr_card_intr_int; 
												size_cards_logged++;
												

												word = strtok(NULL, " ");
												char * sold = word = strtok(NULL, " ");
												double soldd = atof(sold);
												// memorizam soldul pentru cardul curent in map-ul 
												// listsolds
												std::map<int,double>::iterator it;
												it = listsolds.find(card_int);
												if (it == listsolds.end())
													listsolds[card_int] = soldd;

												// trimitem raspuns
												n = send(i, buffer, strlen(buffer), 0);
												// daca operatia ceruta de client nu a putut fi
												// efectuata cu succes 
							                    if (n < 0) {
							                        perror("IBANK> -6 : Operatie esuata\n");
							                        exit(0);
							                    }
												break;

											// daca pinul nu este introdus corect si a fost
											// gresit anterior mai putin de 3 ori
											} else if (strcmp(pin_introdus, pin_fisier) != 0 && 
												cards[nr_card_intr_int].first == 0 &&
												cards[nr_card_intr_int].second < 2){

												sprintf(buffer, "IBANK> -3 : Pin gresit\n");
												// contorizam nr de introduceri gresite a pinului
												cards[nr_card_intr_int].second ++;
												n = send(i, buffer, strlen(buffer), 0); 
							                    if (n < 0) {
							                        perror("IBANK> -6 : Operatie esuata\n");
							                        exit(0);
							                    }
												break;
											
											// daca pinul a fost gresit de 3 ori
											} else if (cards[nr_card_intr_int].second == 2){
												sprintf(buffer, "IBANK> -5 : Card blocat\n");
												n = send(i, buffer, strlen(buffer), 0); 
							                    if (n < 0) {
							                        perror("IBANK> -6 : Operatie esuata\n");
							                        exit(0);
							                    }
												break;
											} 
										}
									}

									// daca am citit toate liniile din fisier si nu am gasit
									// cardul introdus
									if (readed_lines == lines + 1 && gasit == 0){
										sprintf(buffer, "IBANK> -4 : Numar card inexistent\n");
										n = send(i, buffer, strlen(buffer), 0); 
					                    if (n < 0) {
					                        perror("IBANK> -6 : Operatie esuata\n");
					                        exit(0);
					                    }
										break;
									}
	  							}
	  							// inchidem fisierul
	    						fclose(fp);

    						} else {

    							// daca este alt client logat deja pe cardul introdus
								sprintf(buffer, "IBANK> -2 : Sesiune deja deschisa\n");
								n = send(i, buffer, strlen(buffer), 0); 
			                    if (n < 0) {
			                        perror("IBANK> -6 : Operatie esuata\n");
			                        exit(0);
			                    }
    						}
						
						// daca primim o comanda diferita de login 
						} else if (strcmp(comanda, "login") != 0 && client_login[i] == 0){
							// daca in client nu este deschisa nici o sesiune
							sprintf(buffer, "IBANK> -1 : Clientul nu este autentificat\n");
							n = send(i, buffer, strlen(buffer), 0); 
		                    if (n < 0) {
		                        perror("IBANK> -6 : Operatie esuata\n");
		                        exit(0);
		                    }

						// daca primim comanda logout si este deschisa o sesiune pentru clientul i
						} else if (strcmp(buffer, "logout\n") == 0 && client_login[i] == 1){
							// inchidem sesiunea pentru clientul i
							client_login[i] = 0;
							
							// scoatem cardul dat din vector de cardul logate
							int nr_card_intr_int = client_cards[i];
							for (int c = 0; c < MAX_CARDS; c++){
								if (cards_logged[c] == nr_card_intr_int){
									cards_logged[c] = 0;
								}
							}

							// stergem asocierea client-card din client_cards
							client_cards.erase(i);
							// cardul devine nelogat
							cards[nr_card_intr_int].first = 0;
							// si pinul se reseteaza
							cards[nr_card_intr_int].second = 0;

							sprintf(buffer, "IBANK> Clientul a fost deconectat\n");
							n = send(i, buffer, strlen(buffer), 0); 
		                    if (n < 0) {
		                        perror("IBANK> -6 : Operatie esuata\n");
		                        exit(0);
		                    }

						// daca primim comanda listsold si clientul are o sesiune deschisa
						} else if (strcmp(buffer, "listsold\n") == 0 && client_login[i] == 1){
							// trimitem din vectorul listsolds sold cont aferent clientului i
							double sold = listsolds[client_cards[i]];
							sprintf(buffer, "IBANK> %.2f\n", sold);
							n = send(i, buffer, strlen(buffer), 0); 
		                    if (n < 0) {
		                        perror("IBANK> -6 : Operatie esuata\n");
		                        exit(0);
		                    }

						// daca primim comanda transfer si clientul are o sesiune deschisa
						} else if (strcmp(comanda, "transfer") == 0 && client_login[i] == 1){
							// memorizam cardul introdus caruia trebuie sa-i facem trasfer
							char * card_transfer = strtok(NULL, " ");
							int card_transfer_int = atoi(card_transfer);
							// si suma ce trebuie transferata
							char * suma = strtok(NULL, " \n");
							double suma_double = atof(suma);

							fp = fopen(argv[2], "r");
							if (fp == NULL){
								exit(EXIT_FAILURE);
							}

							int first_line = 1;
							int readed_lines = 0;
							int card_corect = 0;

							// citim linie cu linie din fisier ca sa gasim cardul
							while (fgets(read_buff, sizeof(read_buff), fp) != NULL) {
    							read_buff[strlen(read_buff) - 1] = '\0';
    							readed_lines++;
    							// daca e prima linie din fisier, retinem nr de pe ea
    							int lines;
    							if (first_line){
    								lines = read_buff[0] - 48;
    								first_line = 0;
    								continue;
    							}

								char cpy_buffer[BUFLEN];
								strcpy(cpy_buffer, read_buff);
    							char * word, * nume, * prenume, *nr_card_fisier;
								
								// memorizam numele, prenumele de pe linie
								nume = word = strtok(cpy_buffer, " ");
								prenume = word = strtok(NULL, " ");
								
								nr_card_fisier = word = strtok(NULL, " ");
								word = strtok(NULL, " ");
								word = strtok(NULL, " ");
								// si soldul
								char * sold_beneficiar = word = strtok(NULL, " ");
								double sold_beneficiar_d = atof(sold_beneficiar);

								if (nr_card_fisier != NULL) {
									// daca am gasit in fisier cardul cui trebuie sa-i transferam
									if (strcmp(card_transfer, nr_card_fisier) == 0){
										card_corect = 1;
										fclose(fp);

										// daca nu avem destule fonduri
										if (listsolds[client_cards[i]] < suma_double){
											sprintf(buffer, "IBANK> -8 : Fonduri insuficiente\n");
											n = send(i, buffer, strlen(buffer), 0); 
						                    if (n < 0) {
						                        perror("IBANK> -6 : Operatie esuata\n");
						                        exit(0);
						                    }
											break;

										} else { // daca avem
											// cautam daca avem in listsolds cardul caruia ii transferam
											std::map<int,double>::iterator it;
											it = listsolds.find(card_transfer_int);
											// daca nu este, il adaugam
											if (it == listsolds.end())
												listsolds[card_transfer_int] = sold_beneficiar_d;

											// clientul dat asteapta confirmare de transfer
											client_transfer[i] = 1;
											// memorizam cardul clientului i, cardul cui vrem sa 
											// facem transfer si suma ce trebuie transferata
											transfers[client_cards[i]] = make_pair(card_transfer_int, 
												suma_double);

											sprintf(buffer, "IBANK> Transfer %s catre %s %s? [y/n]\n", 
												suma, nume, prenume);
											n = send(i, buffer, strlen(buffer), 0); 
						                    if (n < 0) {
						                        perror("IBANK> -6 : Operatie esuata\n");
						                        exit(0);
						                    }
											break;
										}
									}
								}

								// daca am citit toate liniile si nu am gasit cardul
								if (readed_lines == lines + 1 && card_corect == 0){
									sprintf(buffer, "IBANK> -4 : Numar card inexistent\n");
									n = send(i, buffer, strlen(buffer), 0); 
				                    if (n < 0) {
				                        perror("IBANK> -6 : Operatie esuata\n");
				                        exit(0);
				                    }
									fclose(fp);
									break;
								}
							}

						// daca primim confirmare si clientul este logat si asteapta confirmare
						} else if (strcmp(buffer, "y\n") == 0 && client_login[i] == 1 && 
							client_transfer[i] == 1){

							// nu mai asteapta confirmare
							client_transfer[i] = 0;

							// scadem soldul cardului ce tranfera bani
							double suma_veche = listsolds[client_cards[i]];
							// transfers[client_cards[i]].second - suma ce trebuie transferata
							listsolds[client_cards[i]] = suma_veche - 
								transfers[client_cards[i]].second;

							// adunam soldului cui facem transfer
							double suma_veche1 = listsolds[transfers[client_cards[i]].first];
							// transfers[client_cards[i]].first : cardul caruia trebuie sa-i 
							// facem transfer 
							listsolds[transfers[client_cards[i]].first] = suma_veche1 + 
								transfers[client_cards[i]].second;

							// stergem din transfers procesul dat
							transfers.erase(client_cards[i]);

							sprintf(buffer, "IBANK> Transfer realizat cu succes\n");
							n = send(i, buffer, strlen(buffer), 0); 
		                    if (n < 0) {
		                        perror("IBANK> -6 : Operatie esuata\n");
		                        exit(0);
		                    }
							break;


						// daca nu primim confirmare ci alta litera
						} else if (strcmp(buffer, "y\n") != 0 && client_login[i] == 1 && 
							client_transfer[i] == 1){

							// clientul nu mai asteapta confirmare
							client_transfer[i] = 0;

							sprintf(buffer, "IBANK> -9 : Operatie anulata\n");
							n = send(i, buffer, strlen(buffer), 0); 
		                    if (n < 0) {
		                        perror("IBANK> -6 : Operatie esuata\n");
		                        exit(0);
		                    }
							break;

						// daca am primit quit de la client
						} else if (strcmp(buffer, "quit\n") == 0) {
							// delogam clientul dat
							client_login[i] = 0;
							
							// il scoatem din vectorul de carduri logate
							int nr_card_intr_int = client_cards[i];
							for (int c = 0; c < MAX_CARDS; c++){
								if (cards_logged[c] == nr_card_intr_int){
									cards_logged[c] = 0;
								}
							}

							// ii stergem asocierea client-card
							client_cards.erase(i);
							// il delogam
							cards[nr_card_intr_int].first = 0;
							// erori de introducere pin devin 0
							cards[nr_card_intr_int].second = 0;
							// il stergem din transfers
							transfers.erase(client_cards[i]);
							printf("IBANK> Clientul %d s-a deconectat prin comanda quit\n", i);
							// il deconectam si il stergem din lista de citire
							close(i);
							FD_CLR(i, &read_fds);
						} else { // daca primim o comanda incorect tastata
							sprintf(buffer, "IBANK> -6 : Operatie esuata\n");
							send(i, buffer, strlen(buffer), 0 );
							break;
						}
					}
				} 
			}
		}
    }


    close(sockfd);
   
    return 0; 
}


