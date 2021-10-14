#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>


#define BUFFER_SIZE 4096

int sd; //desc. socket 
int len; //dim. messaggio da inviare
uint16_t lmsg; //lunghezza messaggio (net_order)
int connesso = 0;
int loggato = 0;
char sessionID[10];

void benvenuto(){
printf("\n\n***************************** GIOCO DEL LOTTO *****************************\n"
		   "Sono disponibili i seguenti comandi: \n"
		   "1)!help <comando> --> mostra i dettagli di un comando\n"
		   "2)!signup <username> <password> --> crea un nuovo utente\n"
		   "3)!login <username> <password> --> autentica un utente\n"
		   "4)!invia_giocata g --> invia una giocata g al server\n"
		   "5)!vedi_giocate tipo --> visualizza le giocate precedenti dove tipo = {0,1}\n"
		   "                         e permette di visualizzare le giocate passate ‘0’\n"
 		   "                         oppure le giocate attive ‘1’ (ancora non estratte)\n"
 		   "6)!vedi_estrazione <n> <ruota> --> mostra i numeri delle ultime n estrazioni\n"
 		   "                                    sulla ruota specificata\n"
 		   "7)!esci --> termina il client\n"
		   );
}

void invioMessaggio(char* buffer){ //funzione per inviare mess. al server

	int ret;
	int len = strlen(buffer) + 1; //invio annche il carattere fine stringa
	lmsg = htons(len); //conversione endianess
	ret = send(sd, (void*)&lmsg, sizeof(uint16_t),0); //invio la dimensione del messaggio
	ret = send(sd, (void*)buffer, len, 0);

	if(ret < 0){
		perror("Errore Client in fase di invio\n");
		exit(-1);
	}

}

void ricezioneMessaggio(char* buffer){ //funzione per ricevere mess. dal server

	int ret;
	int len;
	ret = recv(sd, (void*)&lmsg, sizeof(uint16_t),0); //ricevo la dimensione
	len = ntohs(lmsg); //la converto in formato host
	ret = recv(sd, (void*) buffer, len, 0);

	if(ret < 0){
		perror("Errore Client in fase di ricezione\n");
		exit(-1);
	}
}


int confronta(const char* buffer, const char* word){

   if(strncmp(buffer, word, strlen(word)) == 0) return 1; //true
   return 0; //false
}

void help(char* comando){ //funzione utilizzata per ottenere maggiori informazioni 
	//su un determinato comando

	if(strcmp(comando, "") == 0) //ovvero digito solo help senza niente
		printf("Eccoti una breve sintesi di alcuni comandi disponibili:\n"
										"1)!signup <username> <password> --> registra un nuovo utente\n"
										"2)!login <username> <password> --> autentica un utente precedentemente registrato\n"
										"3)!invia_giocata g --> invia una giocata g al server (solo utente loggato)\n"
										"4)!vedi_giocate tipo --> tipo può valere 0 oppure 1\n"
										"  se tipo vale 0 allora potrai visualizzare le giocate passate\n"
										"  se tipo vale 1 allora potrai visualizzare le giocate in attesa di estrazione\n"
										"  (solo utente loggato)\n"
										"5)!vedi_estrazione <n> <ruota> --> ti permette di visualizzare le n estrazioni più recenti\n"
										"  se la ruota non è specificata le visualizza tutte, altrimenti solo la ruota selezionata\n"
										"  (solo utente loggato)\n"
										"6)!vedi_vincite --> ti permette di visualizzare le tue vincite (solo utente loggato)\n"
										"7)!esci --> l'utente loggato viene disconnesso e il client viene chiuso\n"
										);

	if(strcmp(comando,"!signup\n") == 0) 
		printf("----- !signup <username> <password> ----\n"
				"  registra un nuovo utente con username e password specificati\n"
			   " altrimenti se è già presente un utente con lo stesso username, verrà chiesto di sceglierne un altro.\n"
										);

	if(strcmp(comando,"!login\n") == 0)	printf("---- !login <username> <password> ----\n"
											   "  autentica l'utente con username e password specificati (se corretti)\n"
											   "  in caso vengano inseriti dati che non corrispondono ad alcun utente, il login fallirà.\n"
											   "  L'utente potrà riprovare per altre 2 volte, esauriti i tentativi verrà bloccato per 30 min.\n"
											  );

	if(strcmp(comando,"!invia_giocata\n") == 0) printf("---- !invia_giocata g ----\n"
													  "  se l'utente ha eseguito il login potrà inviare giocate al server\n"
													  "  la giocata ha la seguente formattazione:\n"
													  "  !invia_giocata -r bari roma . . -n 22 33 44 . . -i 0 1 2 3\n"
													  "  -r indica che a seguire ci saranno le ruote, posso specificare solo ruote esistenti\n"
													  "  -n indica che a seguire ci saranno i numeri puntati nel range [1,90]\n"
													  "  -i indica che a seguire ci saranno gli importi delle puntate, la prima per l'estratto etc\n"
													  "  la giocata verrà inserita nella scheda utente e sarà in attesa di estrazione.\n"
													 );
	if(strcmp(comando,"!vedi_giocate\n") == 0) printf("---- !vedi_giocate <tipo> ----\n"
													 "  se l'utente ha eseguito il login potrà visualizzare le giocate effettuate\n"
													 "  se tipo=0 visualizzerà le giocate passate (già state estratte)\n"
													 "  se tipo=1 visualizzerà le giocate in attesa di estrazione.\n"
													);
	if(strcmp(comando,"!vedi_estrazione\n") == 0) printf("---- !vedi_estrazione <n> <ruota> ----\n"
														"  se l'utente ha eseguito il login potrà vedere le estrazioni più recenti\n"
														"  <n> va sempre specificato e indica quante estrazioni voglio visualizzare\n"
														"  se <ruota> non è specificata visualizzerò l'intera estrazione\n"
														"  altrimenti solo la riga dell'estrazione corrispondente alla ruota selezionata.\n"
													   );
	if(strcmp(comando,"!vedi_vincite\n") == 0) printf("---- !vedi_vincite ----\n"
													 "  se l'utente ha eseguito il login potrà vedere il suo storico vincite.\n"
													);
	if(strcmp(comando,"!esci\n") == 0) printf("---- !esci ----\n"
											 "  l'utente attualmente loggato viene disconnesso e il client viene chiuso.\n"
											);
}

/*funzione per verificare il corretto inserimento di un comando
	la funzione restiusce:
	0 se il comando non è corretto
	1 se il comando è !help e quindi non deve essere inviato al server
	un intero maggiore di 1 se il comando è da inviare al server (!signup, login...)
*/
int comandoInserito(char* buffer){ 
	
	//occorre usare delle variabili di appoggio
	//per spezzare le stringhe date in input al client 
	//al fine di anallizzare il comando correttamente
	int quanteParole = 0;
	char delimiter[2] = " "; //delimitatori sono semplicemente spazi bianchi
	char* token = strtok(buffer, delimiter);
	//per salvare le parole, devo utilizzare un array di strighe 
	char parole[40][1024];

	int i;
	for(i = 0; i<40; i++) strcpy(parole[i], ""); //devo inizializzarlo

	while( token != NULL){ //uso while per salvare le parole nell'array
		strcpy(parole[quanteParole++],token);
		token = strtok(NULL, delimiter);
	}

	if(strcmp(parole[0], "!help\n") == 0){
		//comando help senza argomenti
		help("");
		return 1;

	}else if(strcmp(parole[0],"!help") == 0){
		//comando help con altro al seguito 
			if(strcmp(parole[1],"!signup\n")==0 || 
				strcmp(parole[1],"!login\n") == 0 || 
				strcmp(parole[1],"!invia_giocata\n") == 0 || 
				strcmp(parole[1],"!vedi_giocate\n") == 0 || 
				strcmp(parole[1],"!vedi_estrazione\n") == 0 || 
				strcmp(parole[1],"!vedi_vincite\n") == 0 || 
				strcmp(parole[1],"!esci\n") == 0){

			help(parole[1]);
			return 1;

		}
	}

	if(strcmp(parole[0],"!signup") == 0 && strcmp(parole[1],"") != 0 && strcmp(parole[2],"") != 0 && strcmp(parole[3],"") == 0)
		//comanod inserito: !signup username psw
		return 2;		

	if(strcmp(parole[0],"!login") == 0 && strcmp(parole[1],"") != 0 && strcmp(parole[2],"") != 0 && strcmp(parole[3],"") == 0){
		//comando inserito !login usr psw
		return 3;
	}

	if(strcmp(parole[0],"!invia_giocata") == 0 && strcmp(parole[1],"-r") == 0){
		/*Devo controllare che il formato sia esatto quindi che i separatori
		"-n" e "-i" per numeri e importi siano corretti
		verificare che le ruote siano corrette e non duplicate
		verificare che i numeri siano da 1-90 e non duplicati
		verificare che ci sia almeno un numero ma non più di 10
		verificare l'inserimento di un importo >0 (non negativo)
		*/

		//setto a true se controllo un elemento della schedina, false altrimenti
		bool prendiRuota = true;
		bool prendiNumeri = false;
		bool prendiImporto = false;

		int i=2; //parto da parole[2], prima non mi interessa

		//devo specificare almeno uno di tutto(ruote,numeri,importi)
		int quanteRuote = 0;
		int quantiNumeri = 0;
		int quantiImporti = 0;
		//almeno uno dei 5 importi sulla giocata != 0
		bool importoOk = false;

		int ruote[11] = {0}; //di base a zero,set a 1 se scelta, lo uso anche per i doppioni
		int numeri[90] = {0}; 

		//l'analisi stringa parte da parola[2] quindi dalla prima ruota
		while(strcmp(parole[i],"") != 0){ //non sono alla fine

			if(prendiRuota == true){

				if(strcmp(parole[i],"-n") != 0){//non ho finito le ruote

					int ruota;
					quanteRuote++;

					if(strcmp(parole[i],"tutte") == 0) goto fine;

					if(strcmp(parole[i],"bari") == 0) ruota = 0;
					else if(strcmp(parole[i],"cagliari") == 0) ruota = 1;
					else if(strcmp(parole[i],"firenze") == 0) ruota = 2;
					else if(strcmp(parole[i],"genova") == 0) ruota = 3;
					else if(strcmp(parole[i],"milano") == 0) ruota = 4;
					else if(strcmp(parole[i],"napoli") == 0) ruota = 5;
					else if(strcmp(parole[i],"palermo") == 0) ruota = 6;
					else if(strcmp(parole[i],"roma") == 0) ruota = 7;
					else if(strcmp(parole[i],"torino") == 0) ruota = 8;
					else if(strcmp(parole[i],"venezia") == 0) ruota = 9;
					else if(strcmp(parole[i],"nazionale") == 0) ruota = 10;
					else ruota = -1;

					if(ruota == -1) return 0; //non corretta

					if(ruote[ruota] == 0) ruote[ruota] = 1; //verifico se è doppiata
						else return 0;

					if(quanteRuote > 11) return 0;

				}else{

					prendiRuota = false;
					prendiNumeri = true;
					if(quanteRuote == 0) return 0; //nessuna ruota specificata
					goto fine;

				}
			}

			if(prendiNumeri == true){

				if(strcmp(parole[i],"-i") != 0){ //se la stringa non è "-i" e quindi è un numero

					int numero = atoi(parole[i]); //cast
					if(numero <1 || numero >90) return 0;
					if(numeri[numero-1] == 0) numeri[numero-1] = 1;
						else return 0; //duplicato
					quantiNumeri++;
					if(quantiNumeri > 10) return 0;

			}else{

				prendiNumeri = false;
				prendiImporto = true;
				if(quantiNumeri == 0) return 0;
				goto fine;

				}
			}

			if(prendiImporto == true){

				float importo = atof(parole[i]); //cast
				if(importo > 0) importoOk = true; //se ho specificato almeno un importo > 0

				quantiImporti++;

				if(strcmp(parole[i+1],"") == 0) prendiImporto = false;

				if(importo < 0) return 0; //importo non consentito poichè < 0

				if(quantiImporti > 5) return 0; //troppi importi specificati

			}

			fine:
			i++;

		}

		if(importoOk == false) return 0;
		if(quantiImporti == 0) return 0;
		return 4;

	}

	if(strcmp(parole[0],"!vedi_giocate")==0 && (strcmp(parole[1],"1\n")==0 || strcmp(parole[1],"0\n")==0))
		return 5;

	if(strcmp(parole[0],"!vedi_estrazione") == 0 && strcmp(parole[1],"")!=0 && strcmp(parole[3],"") == 0){ //l'ultimo controllo server per vedere se non ho specificato più ruote

		int numeroEstrazioni; //quante estrazioni
		int ruota;

		if(strcmp(parole[2],"") == 0) strtok(parole[1],"\n");
		numeroEstrazioni = atoi(parole[1]); //prendo il numero di estrazioni

		if(numeroEstrazioni < 1 || numeroEstrazioni > 5) //5 numero max di estrazioni che posso far vedere
				return 0;

		if(strcmp(parole[2],"")!=0){
			strtok(parole[2],"\n");
			if(strcmp(parole[2],"bari") == 0) ruota = 0;
			else if(strcmp(parole[2],"cagliari") == 0) ruota = 1;
			else if(strcmp(parole[2],"firenze") == 0) ruota = 2;
			else if(strcmp(parole[2],"genova") == 0) ruota = 3;
			else if(strcmp(parole[2],"milano") == 0) ruota = 4;
			else if(strcmp(parole[2],"napoli") == 0) ruota = 5;
			else if(strcmp(parole[2],"palermo") == 0) ruota = 6;
			else if(strcmp(parole[2],"roma") == 0) ruota = 7;
			else if(strcmp(parole[2],"torino") == 0) ruota = 8;
			else if(strcmp(parole[2],"venezia") == 0) ruota = 9;
			else if(strcmp(parole[2],"nazionale") == 0) ruota = 10;
			else if(strcmp(parole[2],"tutte") == 0) ruota = -1;

			if(ruota == -1) return 0;//solo una ruota è consentita
		}

		return 6;

	}

	if(strcmp(parole[0],"!vedi_vincite\n") == 0){
		return 7;
	}

	if(strcmp(parole[0], "!esci\n") == 0)
		return 8;

	return 0;
}



int main(int argc, char* argv[]){
  
    int ret;
	struct sockaddr_in indexServer; //struttura indexServer
	int tipoComando = 0; //per il tipo di comando ritornato dalla funzione di identificazione
	char buffer[BUFFER_SIZE]; //buffer per invio e ricezione
	char temp[BUFFER_SIZE]; //buffer temporaneo

	//controllo sull'avvio del client
    if(argc != 3) 
    	exit(-1);
    

    /*crea socket*/
	sd = socket(AF_INET, SOCK_STREAM, 0); //restituisce -1 in caso di errore
	if(sd == -1) { 
        perror("Errore nella creazione del socket\n");
        exit(1);
    }
    /* Creazione indirizzo del server */
    memset(&indexServer, 0, sizeof(indexServer)); // Pulizia 
    indexServer.sin_family = AF_INET;
    indexServer.sin_port = htons(atoi(argv[2])); //preparazione porta dichiarata
    inet_pton(AF_INET, argv[1] , &indexServer.sin_addr);
    
    ret = connect(sd, (struct sockaddr*)&indexServer, sizeof(indexServer));
    
    if(ret < 0){
        perror("Errore in fase di connessione: \n");
        exit(1);} else connesso = 1;

    ricezioneMessaggio(buffer); //mess. ricevuto dal server con info sulla connessione
    printf("%s\n",buffer );

    if(confronta(buffer,"Errore3:") == 1) connesso = 0; //utente in blacklist.txt

    if(connesso == 1) benvenuto();

    while(connesso == 1){

    	fgets(buffer, 1024, stdin);

    	strcpy(temp, buffer);

    	tipoComando = comandoInserito(temp);

    	if(tipoComando == 0)
    		printf("Attenione, comando non riconosciuto!\n");

    	if(tipoComando > 1){

    	strtok(buffer,"\n"); //rimozione del carattere di fine stringa

    	if(loggato == 1){ //aggiungo il sessionId
    		strcat(buffer," ");
    		strcat(buffer, sessionID);
    	}

    	invioMessaggio(buffer); //invio il comando con il formato corretto al server

      	ricezioneMessaggio(buffer); //attendo una risposta dal server

        printf("%s",buffer); //stampo la risposta del server

        if(tipoComando == 2){ //user duplicato
        	while(confronta(buffer,"Errore1") == 1){
        		fgets(buffer, 1024, stdin);
        		invioMessaggio(buffer);
        		ricezioneMessaggio(buffer);
        		printf("%s",buffer );
        	}
       	 }

       	 if(tipoComando == 3){
       	 	if(strcmp(buffer,"--- msgFromServer: Login effettuato! ---\n") == 0){
       	 		ricezioneMessaggio(buffer); //sessioId
       	 		strcpy(sessionID,buffer);
       	 		loggato = 1;
       	 		printf("--- SessionID ricevuto con successo! ---\n");
       	 	}

       	 	if(strcmp(buffer,"--- Disconnessione server in corso... ---\n") == 0){
       	 		connesso = 0;

       	 	}
       	 }

       	 if(tipoComando == 8){
       	 	if(strcmp(buffer,"--- Disconnessione avvenuta con successo ---\n") == 0)
       	 		connesso = 0;
       	 }

       }

    }

    printf("Disconnessione Client\n");
    close(sd);
    return 0;
        
}