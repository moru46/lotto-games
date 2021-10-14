#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <signal.h>
#include <stdbool.h>

#define BUFFER_SIZE 4096

struct Schedina 
{
	char utente[1024]; //utente che ha fatto la giocata
	int ruote[11]; //Bari,Cagliari,Firenze,Genova,Milano,Napoli,Palermo,Roma,Torino,Venezia,Nazionale a partire da 0, se 1, la corrispondente ruota è scelta
	int numeri[90]; //settata come numero-1 per il numero giocato
	float importo[5]; //posso giocare anche importi non interi, quindi float, 0 estratto 1 ambo ecc
};

int sd; //socket ascolto
int new_sd; // socket client
int tempo;
int connesso = 0; //variabile passata nel ciclo while del processo figlio 
int lmsg;
struct sockaddr_in my_addr, cl_addr;
int tentativiLogin = 3;
char sessionID[10] = "9999999999"; //stringa che memorizzerà la sessionID
char utenteLoggato[1024]; //variabile in cui viene salvato l'username dell'utente attualmente loggato usate sia per la schedina inserita dall'utente, sia per calcolare le vincite dopo ogni estrazione
/*Schedine: la prima quando si gioca una nuova schedina, la seconda per verificare le eventuali vincite e confronto. 
Uso due schede diverse anche per risolvere evenutali problemi di concorrenza.
Infatti potrei giocare una schedina mentre sta per essere verificata una giocata e usando la stessa struttura 
rischierei di sovrascrivere i dati */
struct Schedina s, schedaAttesa; 


void invioMessaggio(char* buffer){ //funzione per inviare messaggi al client

	int ret;
	int len = strlen(buffer) + 1; //invio annche il carattere fine stringa
	lmsg = htons(len); //conversione endianess
	ret = send(new_sd, (void*)&lmsg, sizeof(uint16_t),0);
	ret = send(new_sd, (void*)buffer, len, 0);

	if(ret < 0){
		perror("--- Errore Server in fase di invio ---\n");
		exit(-1);
	}

}

void ricezioneMessaggio(char* buffer){ //funzione per ricevere messaggi dal client

	int ret;
	int len;
	ret = recv(new_sd, (void*)&lmsg, sizeof(uint16_t),0);
	len = ntohs(lmsg);
	ret = recv(new_sd, (void*) buffer, len, 0);

	if(ret < 0){
		perror("--- Errore Server in fase di ricezione ---\n");
		exit(-1);
	}

	if(ret == 0){
		printf("--- Chiusura Client ---\n");
		exit(-1);
	}
}

//metodo veloce che ritorna 1 se le parole sono uguali, zero altrimenti
//si confronta prendendo il numero di caratteri della seconda stringa
//funzione strncmp != strcmp

int confronta(const char* buffer, const char* word){

   if(strncmp(buffer, word, strlen(word)) == 0) return 1; //true
   return 0; //false
}

//funzione che controlla se l'user inserito è già presente
int controlloUserPresente(char* username){ 

  	FILE* user;
  	char temp[40];

  	sprintf(temp,"%s.txt",username);

  	if((user=fopen(temp, "r"))==NULL) 
  		return 1;//utente non registrato

  	return 0; //utente registrato

}

/*
comando signup:
1: per prima cosa verifico se l'utente è gia presente in memoria,
in caso negativo registro l'utente e carico la scheda
2:se l'utente è gia presente, lo notifico al client e aspetto per un nuovo username
mentre la password viene mantenuta.
*/

void signup(char* buffer){

	//spezzo buffer per estrarre le credenziali
	int quanteParole = 0;
	char delimiter[2] = " "; //delimitatori sono semplicemente spazi bianchi
	char* token = strtok(buffer, delimiter);
	//per salvare le parole, devo utilizzare un array di strighe 
	char parole[3][1024]; // parole[1] e parole[2] contenenti user e psw
	int presente = 0;
	int userlen = 0, pswlen = 0;

	int i;
	for(i = 0; i<3; i++) strcpy(parole[i], ""); //devo inizializzarlo

	while( token != NULL){ //uso while per salvare le parole nell'array
		strcpy(parole[quanteParole++],token);
		token = strtok(NULL, delimiter);
	}

	presente = controlloUserPresente(parole[1]);

	char newUsr[1024]; //nuovo utente, sarà inizilizzato in entrambi i casi

	if(presente == 1){ //user non presente. OK lo registro
	
	FILE* fd;

	userlen = strlen(parole[1]);
	pswlen = strlen(parole[2]);
	strcpy(newUsr,parole[1]); 

	char filePath[userlen + pswlen + 5]; //user + psw + .txt
	sprintf(filePath,"%s.txt",newUsr);

	if ((fd=fopen(filePath, "w"))==NULL) printf("--- Errore nell’apertura del file! ---\n");
	// contenuto che verrà inserito in tale file
	
	fprintf(fd,"username: %s\n",newUsr);
	fprintf(fd,"password: %s\n\n",parole[2]);
	fprintf(fd,"ELENCO DI TUTTE LE GIOCATE:\n\n");
	fclose(fd);

	invioMessaggio("Il suo username è stato registrato correttamente!\n");

	} else {

		while(presente == 0){ //aspetto il nuovo username
			invioMessaggio("Errore1: Username già presente. Inserisci nuovo username!\n");
			ricezioneMessaggio(newUsr);	//aspetto il nuovo username
			strtok(newUsr,"\n");
			presente = controlloUserPresente(newUsr); //faccio un altro controllo finchè l'utente non mi manda un usr corretto

		}

	FILE* fd;

	userlen = strlen(newUsr);
	pswlen = strlen(parole[2]); //la psw è la stessa di prima

	char filePath[userlen + pswlen + 5]; //user + psw + .txt
	sprintf(filePath,"%s.txt",newUsr);
	
	if ((fd=fopen(filePath, "w"))==NULL) printf("Errore nell’apertura del file!\n");
	
	fprintf(fd,"username: %s\n",newUsr);
	fprintf(fd,"password: %s\n\n",parole[2]);
	fprintf(fd,"ELENCO DI TUTTE LE GIOCATE:\n\n");
	fclose(fd);

	invioMessaggio("L'username fornito è stato registrato correttamente\n");

	}

}

int controllaCredenziali(char* usr, char* psw){

	FILE* user;
  	char temp[40];

  	sprintf(temp,"%s.txt",usr);
  	if ((user=fopen(temp, "r"))==NULL) //utente non registrato
  		return 2;
  	
  	//Variabili per la lettura delle righe di un file
	char *line_buf = NULL;
  	size_t line_buf_size = 0;
  	ssize_t line_size;

  	//Formatto la stringa come nella scheda password: pass
	int pswlen = strlen(psw);
	int string = strlen("password: ");
	char concat[pswlen + string + 2];
	sprintf(concat, "password: %s\n", psw); //formato username password\n

	line_size = getline(&line_buf, &line_buf_size, user); //prende la prima linea del file fd

  	while (line_size >= 0) { // Loop in cui analizza tutte le linee del file users.txt

    	if(strcmp(line_buf,concat) == 0) { //se la riga letta dal file è uguale alla mia stringa "usr psw"
    		printf("--- Username e password inseriti dal client sono corretti ---\n");
    		return 0; //controllo andato a buon fine
    	} 
   
    	line_size = getline(&line_buf, &line_buf_size, user); //prendo la prossima linea e continuo

  	}
  	return 1; //errore credenziali
}

//Funzione che genera una stringa di lunghezza data, con caratteri alfanumerici casuali
//Viene utilizzata dalla funzione di login, per generare il sessionID da inviare al client

void randomString(const int len) {

	int i;
     char stringa[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";

    srand(time(NULL));
    for (i = 0; i < len; ++i) {
        sessionID[i] = stringa[rand() % (sizeof(stringa) - 1)];
    }

    stringa[len] = 0; //aggiungo \0

}


//funzione di login
//in caso di 3 erorri in fase di autenticazione
// il client con IP dato, viene bloccato in un file blacklist.txt
void login(char* buffer){

	int quanteParole = 0;
	char delimiter[2] = " ";
	char* token = strtok(buffer, delimiter);
	char parole[3][1024]; // parole[1] e parole[2] contenenti user e psw
	int confermato; //settato dalla controllaCredenziali
	char msgErrore[1024];

	int i;
	for(i = 0; i<3; i++) strcpy(parole[i], ""); //devo inizializzarlo

	while( token != NULL){ //uso while per salvare le parole nell'array
		strcpy(parole[quanteParole++],token);
		token = strtok(NULL, delimiter);
	}

	if(strcmp(utenteLoggato,parole[1])==0){
		invioMessaggio("--- Attenzione: Login già effettuato! ---\n");
		printf("--- Login già effettuato ---\n");
		return;
	}

	confermato = controllaCredenziali(parole[1],parole[2]); //controllo se le credenzuali sono ok

	if(confermato == 2){
		invioMessaggio("--- msgFromServer: Utente non registrato! ---\n");
		printf("--- Utente non registrato ---\n");
		return;
	}

	if(confermato == 0){

		tentativiLogin = 3; //riporto al valore default
		randomString(10); //stringa alfanumerica di 10 char
		strcpy(utenteLoggato, parole[1]);
		printf("--- Login effettuato! ---\n");

		invioMessaggio("--- msgFromServer: Login effettuato! ---\n");
		invioMessaggio(sessionID);
		printf("--- sessionID %s inviato al client ---\n",sessionID);

	}else{

		tentativiLogin--;
		sprintf(msgErrore,"Errore2: credenziali non valide. Tentativi rimasti: %i\n",tentativiLogin);
		printf("--- Tentativo login fallito! ---\n");
		if(tentativiLogin >= 1) invioMessaggio(msgErrore);
		else{
			invioMessaggio("--- Disconnessione server in corso... ---\n");
			printf("--- Troppi tentativi login errati: client disconnesso ---\n");
		}

		if(tentativiLogin == 0){ //blocco l'IP

			//inserisco l'ip del client nella blacklist.txt e lo blocco per 30min	
			char ipClientBlocco[1024]; //var per stampare su file
			char timestamp[50]; //var per timestamp
			time_t t = time(NULL);
			struct tm tm = *localtime(&t); //struct timestamp

			//stampo su file il timestamp
			sprintf(timestamp,"%02d-%02d-%d %02d:%02d:%02d ",tm.tm_mday, tm.tm_mon + 1,tm.tm_year + 1900, tm.tm_hour, tm.tm_min, tm.tm_sec);

			//inet_ntoa mi restituisce l'indirizzo ip come stringa da cl_addr, che vado a concatenare nel file blacklist.txt
    		sprintf(ipClientBlocco, "%d %s %s", (int)time(NULL), inet_ntoa(cl_addr.sin_addr), timestamp); //formato IPClient/date time
    		//time() restituisce il timestamp in millisecondi
    		FILE *fd = fopen("blacklist.txt", "a");
    		fprintf(fd, "%s\n", ipClientBlocco); //inserisco l'ip del client e il timestamp nel file blacklist.txt
    		fclose(fd);

    		connesso = 0;
    		printf("--- IpClient bloccato per 30 min ---\n");

    	}
	}
}

 //ritorna -1 se la stringa è "tutte", altrimenti un valore da 0 a 10 a seconda della ruota specificata
 //Bari, Cagliari, Firenze,  Genova, Milano, Napoli, Palermo,  Roma, Torino, Venezia e Nazionale.
 //inserisco un controllo aggiunitvo per la prima lettera maiuscola o meno(quando opero sui file ecc,
 // i nomi delle ruote sono salvati con la prima lettera maiuscola)

int individuaRuota(char* ruota){
 
  if(strcmp(ruota,"bari") == 0 || strcmp(ruota,"Bari") == 0) return 0;
  if(strcmp(ruota,"cagliari") == 0 || strcmp(ruota,"Cagliari") == 0) return 1;
  if(strcmp(ruota,"firenze") == 0 || strcmp(ruota,"Firenze") == 0 ) return 2;
  if(strcmp(ruota,"genova") == 0 || strcmp(ruota,"Genova") == 0 ) return 3;
  if(strcmp(ruota,"milano") == 0 || strcmp(ruota,"Milano") == 0 ) return 4;
  if(strcmp(ruota,"napoli") == 0 || strcmp(ruota,"Napoli") == 0 ) return 5;
  if(strcmp(ruota,"palermo") == 0 || strcmp(ruota,"Palermo") == 0 ) return 6;
  if(strcmp(ruota,"roma") == 0 || strcmp(ruota,"Roma") == 0 ) return 7;
  if(strcmp(ruota,"torino") == 0 || strcmp(ruota,"Torino") == 0 ) return 8;
  if(strcmp(ruota,"venezia") == 0 || strcmp(ruota,"Venezia") == 0 ) return 9;
  if(strcmp(ruota,"nazionale") == 0 || strcmp(ruota,"Nazionale") == 0 ) return 10;
  return -1; //nel caso sia "tutte"

}

int impostaSchedina(char* buffer){

	int quanteParole = 0;
	char delimiter[2] = " ";
	char* token = strtok(buffer, delimiter);
	char parole[50][1024];

	int i;
	for(i = 0; i<50; i++) strcpy(parole[i], "");

	i = 2;

	while( token != NULL){ //uso while per salvare le parole nell'array
		strcpy(parole[quanteParole++],token);
		token = strtok(NULL, delimiter);
	}

	//inizializzo la schedina di partenza

	int j;
	for(j = 0; j<11; j++) s.ruote[j] = 0;
		for(j = 0; j<90; j++) s.numeri[j] = 0;
			for(j = 0; j<5; j++) s.importo[j] = 0;

	bool impostaRuota = true;
	bool impostaNumeri = false;
	bool impostaImporti = false;
	int indexRuota;
	int indexImporto = 0;
	int numeriGiocati = 0;//utilizzo per veirficare che l'utente non giochi partite non consentite, tipo: 3 numeri e quaterna

	while(strcmp(parole[i],"") != 0){

		if(impostaRuota == true){ //ruote
			if(strcmp(parole[i],"-n") == 0){ //ho finito le ruote
				impostaRuota = false;
				impostaNumeri = true;
				goto fine; //salto alla fine del while, salto indicato con "fine"
			}

			indexRuota = individuaRuota(parole[i]); //controllo la ruota giocata

			if(indexRuota == -1){ //"tutte"
				int k;
				for(k = 0; k<11; k++) s.ruote[k] = 1; //le imposto tutte a 1 (sono tutte selezionate)
       		} else  s.ruote[indexRuota] = 1;
      		
     	 }

     	 if(impostaNumeri == true){ //numeri
     	 	if(strcmp(parole[i],"-i") == 0){
				impostaImporti = true;
				impostaNumeri = false;
				goto fine;
			}

			int numero;
			numero = atoi(parole[i]); //converto in int 
			s.numeri[numero-1] = 1;
			numeriGiocati++; //per verificare i numeri giocati
		}

		if(impostaImporti == true){ //importi

			float importo;

			if(strcmp(parole[i+1],"")==0){ //sono alla fine
				if(strcmp(parole[i],sessionID) != 0) return 0; //errore sul sessionID

			impostaImporti = false;
			goto fine;
			}

			importo = atof(parole[i]); //conversione da float
			s.importo[indexImporto] = importo; //utente gioca 0 se non considera la giocata e un numero != 0 altrimenti
			if(s.importo[indexImporto] != 0 && numeriGiocati < indexImporto+1 ) return 2; //serve per il controllo sul totale dei numeri, in relazione al tipo di giocata
			indexImporto++;
			
			}

     	 fine:
     	 i++;

		}

		return 1;
}


/*Registro la giocata, se corretta, sia nella scheda del giocatore, 
sia nel file giocate_attesa.txt*/

void registraGiocataFile(){

int i;

char strScheda[1024] = " ";//stampo nella schedaUtente

char strAttesa[1024];//stampo in giocate_attesa.txt

//NB strScheda e strAttesa avranno due formati diversi di formattazione

char path[1024]; //path per il percorso 

sprintf(strAttesa,"%s:", utenteLoggato); //il record inizia con usr:

for(i = 0;i<11;i++){//scorro s.ruote[] settata prima in invia_giocata
	if(s.ruote[i] == 1){//ruota giocata
	  if(i == 0)  strcat(strScheda,"Bari ");
      if(i == 1)  strcat(strScheda,"Cagliari ");
      if(i == 2)  strcat(strScheda,"Firenze ");
      if(i == 3)  strcat(strScheda,"Genova ");
      if(i == 4)  strcat(strScheda,"Milano ");
      if(i == 5)  strcat(strScheda,"Napoli ");
      if(i == 6)  strcat(strScheda,"Palermo ");
      if(i == 7)  strcat(strScheda,"Roma ");
      if(i == 8)  strcat(strScheda,"Torino ");
      if(i == 9)  strcat(strScheda,"Venezia ");
      if(i == 10) strcat(strScheda,"Nazionale ");
      }
  }

strcat(strAttesa,strScheda); //ho settato l'insieme delle ruote
strcat(strAttesa,"-n ");//separatore come nel formato standard, cosi' facilito le future operazioni

//numeri
for(i=0; i<90;i++){
	char intToStr[4];
	sprintf(intToStr,"%i ",i+1);//poichè l'indice è n-1

	if(s.numeri[i] == 1){
		strcat(strScheda,intToStr);//concateno
		strcat(strAttesa,intToStr);//concateno
	}
}

//importi
strcat(strAttesa,"-i ");

for(i=0; i<5; i++){
	char strPuntate[1024];//contiene il formato "x importo Estratto"..
	char puntata[9];
	char importo[1024]; //importo singolo

	if(s.importo[i] != 0){//concateno solo quelli diversi da 0
	  if(i == 0) strcpy(puntata,"Estratto");
      if(i == 1) strcpy(puntata,"Ambo");
      if(i == 2) strcpy(puntata,"Terno");
      if(i == 3) strcpy(puntata,"Quaterna");
      if(i == 4) strcpy(puntata,"Cinquina");
     
	  sprintf(strPuntate,"x %f %s ",s.importo[i],puntata);
      sprintf(importo,"%f ",s.importo[i]);

      strcat(strScheda,strPuntate);
      strcat(strAttesa,importo); //per strAttesa mi interessano solo gli importi in ordine

    } else { //anche se è a 0 voglio inserirli nel file giocate_attesa, poichè questo rimane nel formato standard ex: -i 0 0 1 1 0

      sprintf(importo,"%f ",s.importo[i]);
      strcat(strAttesa,importo); //concateno anche gli importi a 0 alla strAttesa

    }
  }

 sprintf(path,"%s.txt",utenteLoggato); //inserisco il path per scrivere nella scheda dell'utente loggato

  //inserisco la giocata nella scheda relativa all'utente
  FILE *fd = fopen(path, "a");

  if (!fd) {
      printf("--- Errore nell’apertura del file! ---\n");
      return;
    }

    fprintf(fd, "%s\n", strScheda);
    fclose(fd);
    printf("--- Inserita la giocata nel file relativo all'utente ---\n");

    //inserisco la giocata tra le giocate in attesa di estrazione

    FILE *fd2 = fopen("giocate_attesa.txt", "a");

  if (!fd2) {
      printf("--- Errore nell’apertura del file! ---\n");
      return;
    }

    fprintf(fd2, "%s\n", strAttesa);
    fclose(fd2);
    printf("--- La giocata è in attesa di estrazione ---\n");

}


void invia_giocata(char* buffer){

	int giocataOK;
	giocataOK = impostaSchedina(buffer);

	if(giocataOK == 0) invioMessaggio("--- Devi essere loggato per inviare una giocata! ---\n");
		else if(giocataOK == 2){ 

				invioMessaggio("--- Giocata non consentita! ---\n");
				printf("--- Giocata scartata poichè non consentita! ---\n");

			}else{

			invioMessaggio("--- Giocata registrata correttamente! ---\n");
			registraGiocataFile();

		}	
}

//chiamata da vedi_giocata tipo, stampa le giocate a seconda del tipo selezionato
//il tipo di formattazione dell'output è lo stesso per entrambi i comandi

void leggiFileGiocate(int tipo){

	char *line_buf = NULL;
    size_t line_buf_size = 0;
    ssize_t line_size;
    FILE* fd;

    char output[4096] = ""; //stringa contenente tutte le giocate da stampare
 	int numeroRiga = 1; // mi serve per la formattare la stampa in modo che ogni riga stampata abbia 1) 2) etc

  //in base al valore di tipo cambia il file da cui leggere
    if(tipo == 1) fd = fopen("giocate_attesa.txt", "r");
    if(tipo == 0) fd = fopen("giocate_estratte.txt", "r");

    if (!fd) {
      printf("--- Errore nell’apertura del file! ---\n");
      return;
    }

    line_size = getline(&line_buf, &line_buf_size, fd); //prende la prima linea del file fd

    while (line_size >= 0) {

    int quanteParole = 0;
	char delimiter[2] = " ";
	char* token = strtok(line_buf, delimiter);
	 
	char parole[40][1024];

	int i;
	for(i = 0; i<40; i++) strcpy(parole[i], ""); //devo inizializzarlo

	while( token != NULL){ 
		strcpy(parole[quanteParole++],token);
		token = strtok(NULL, delimiter);
	}

	char giocatore[1024];
	sprintf(giocatore,"%s:",utenteLoggato);

	if(strcmp(giocatore,parole[0]) == 0){

		int i = 1; 
		bool ruote = true;
		bool numeri = false;
		bool importi = false;

		int indexImporto = 0; //ambo, terno...

		char indice[5]; //per la stampa 1)...2)...
		sprintf(indice,"%i) ",numeroRiga);
		strcat(output,indice);

		while(strcmp(parole[i],"")!=0){

			if(ruote == true){

				char ruota[1024];
				if(strcmp(parole[i],"-n") == 0){
					numeri = true;
					ruote = false;
					goto fine;
				}

				sprintf(ruota,"%s ",parole[i]);
				strcat(output,ruota);
			}

			if(numeri == true){

				char numero[100];
				if(strcmp(parole[i],"-i") == 0){
					numeri = false;
					importi = true;
					goto fine;
				}

				sprintf(numero,"%s ",parole[i]);
				strcat(output,numero);

			}

			if(importi == true){

				if(indexImporto == 5) importi = false;

				float importo;
				importo = atof(parole[i]);

				if(importo != 0){

					char cifra[1024];
					char tipoImporto[1024];

					if(indexImporto == 0) strcpy(tipoImporto,"Estratto");
		            if(indexImporto == 1) strcpy(tipoImporto,"Ambo");
		            if(indexImporto == 2) strcpy(tipoImporto,"Terno");
		            if(indexImporto == 3) strcpy(tipoImporto,"Quaterna");
		            if(indexImporto == 4) strcpy(tipoImporto,"Cinquina"); 

		            sprintf(cifra,"* %f %s ",importo, tipoImporto);
		            strcat(output,cifra);

					}
					indexImporto++;
			}

			fine:
			i++;

		}

		strcat(output,"\n");
		numeroRiga++;

	}

      line_size = getline(&line_buf, &line_buf_size, fd); //prendo la prossima linea e continuo

    }
    //Se outputstr non è stata modificata significa che non ho giocate per quell'utente
    if(strcmp(output,"") == 0) invioMessaggio("--- Siamo spiacenti, non è stata trovata alcuna giocata ---\n");
      else invioMessaggio(output); //Altrimenti invio 
    fclose(fd);
}

//dopo ogni estrazione, trasferiso le giocate effettuate nel file giocate estratte
void spostaGiocateEstratte(){

	FILE *fd;
	FILE* fd1;

    if ((fd=fopen("giocate_attesa.txt", "r"))==NULL) printf("--- Errore apertura del file\n");
    if ((fd1=fopen("giocate_estratte.txt", "a"))==NULL) printf("--- Errore apertura del file\n");

    char *line_buf = NULL;
    size_t line_buf_size = 0;
    ssize_t line_size;

    line_size = getline(&line_buf, &line_buf_size, fd); //prende la prima linea del file fd

    while (line_size >= 0) { 

      fprintf(fd1, "%s", line_buf); //stampo la riga nell'altro file, cosi come è
   
      line_size = getline(&line_buf, &line_buf_size, fd); //prendo la prossima linea e continuo

    }

    fclose(fd);
    fclose(fd1);

}

//funzione vedi_giocata <tipo> con tipo che se 1, apre il file delle giocate in attesa di estrazione
//se 0 il file delle giocate già estratte
void vedi_giocata(char* buffer){
	int quanteParole = 0;
	char delimiter[2] = " ";
	char* token = strtok(buffer, delimiter);
	 
	char parole[3][1024];
	int i;
	for(i = 0; i<3; i++) strcpy(parole[i], ""); //devo inizializzarlo

	while( token != NULL){ 
		strcpy(parole[quanteParole++],token);
		token = strtok(NULL, delimiter);
	}

	if(strcmp(parole[2],sessionID) != 0){
		invioMessaggio("--- Devi essere loggato per poter inserire questo comando! ---\n");
	}else{

		if(strcmp(parole[1],"1")==0) //stampo giocate da estrarre
			leggiFileGiocate(1);
		if(strcmp(parole[1],"0")==0) //stampo giocate estratte
			leggiFileGiocate(0);

		}
	}

//funzione vedi_estrazione n ruota

void vedi_estrazione(char* buffer){

	int quanteParole = 0;
	char delimiter[2] = " ";
	char* token = strtok(buffer, delimiter);
	char ruota[100] = " ";
	char parole[4][1024];//parole[1] numero estrazioni, parole[2] ruota

	int i;
	for(i = 0; i<4; i++) strcpy(parole[i], ""); //devo inizializzarlo

	while( token != NULL){ //uso while per salvare le parole nell'array
		strcpy(parole[quanteParole++],token);
		token = strtok(NULL, delimiter);
	}

	/*La ruota potrebbe non essere stata specificta, devo analizzare entrambe le opzioni
	Se la ruota è specificata, avrò in parole[3] il sessionID, altrimenti nel parole[2].
	L'errore dovuto alla selezione di più ruote, è gestito a livello client*/

	if(strcmp(parole[3],"")!= 0){ //vedi_estrazione n ruota sessionID
		if(strcmp(parole[3],sessionID) != 0)
			invioMessaggio("--- Devi essere loggato per poter inserire questo comando! ---\n");
		else strcpy(ruota,parole[2]);

	} else if(strcmp(parole[2],"")!= 0){ //vedi_estrazione n sessionID
		if(strcmp(parole[2],sessionID) != 0)
			invioMessaggio("--- Devi essere loggato per poter inserire questo comando! ---\n");

	}

	char output[4096] = ""; //output per inviare la risposta
	char estrazioni[12][1024]; //array stringhe, contiene le varie estrazioni

	int carattereLetto;
	int contaCaratteri = 0;

	int numeroEstrazioni = atoi(parole[1]);
	int indexRuota = individuaRuota(ruota); //se ruota è assente o vale "tutte" allora restiuisce -1

	int index = 11;

	FILE* fd;
	fd=fopen("estrazioni.txt","r");
	if(!fd){
		printf("--- Errore apertura del file ---\n");
		return;
	}

	//funzione che mi sposta il cursore alla fine del file aperto
	//i caratteri letti saranno in ordine inverso poichè parto dal fondo del file(SEEK_END)
	// e torno indietro	
	fseek(fd,0,SEEK_END); 

	//ftell mi ritorna la dimenzione attuale del file, controllo anche la ruota
	while(ftell(fd) > 1 && index >=0){
		
		char stringa[1024];
		fseek(fd,-2,SEEK_CUR); //riparte dalla posizione corrente del cursore nel file
		
		if(ftell(fd) <= 2)	//sono all'ultimo byte del file
			break;

		carattereLetto = fgetc(fd);//prendo il carattere puntato

		if(carattereLetto != '\n') stringa[contaCaratteri++] = carattereLetto;

		if(carattereLetto == '\n'){ //sono alla fine della riga, allora inverto

			int len = strlen(stringa);
			int colonna = 0;
			int i;
			//inverto la stringa se la lunghezza non è zero
			if(len > 0)
				for(i=len-1;i>=0;i--){
					estrazioni[index][colonna] = stringa[i];
					colonna++;
				}
				
			//printf("--- invertito stringa ---\n");

			index--; //ruota successiva

			if(index == -1){

				if(indexRuota == -1){//tutte le ruote

					int j;
					for(j=0; j<11; j++){//concateno tutte 
						strcat(output,estrazioni[j]); //ogni riga ha una ruota
						strcat(output,"\n");
					}

				}else{ 
						//concateno solo quelle desiderate
						strcat(output,estrazioni[indexRuota]);
						strcat(output,"\n");
				}

				if(numeroEstrazioni > 1){ 
					//se ho selezionato più estrazioni
					index = 11; //indice riparte
					numeroEstrazioni--;
					if(indexRuota == -1) strcat(output,"\n");
				}
			}

			contaCaratteri = 0;
		}
	}
		invioMessaggio(output);
		fclose(fd);
}



//Funzione che viene invocata non appena il server riceve il comando !vedi_vincite

void vedi_vincite(char* buffer){

	  int quanteParole1 = 0;
	  char delimiter1[2] = " ";
	  char* token1 = strtok(buffer, delimiter1);
	  char parole1[2][1024];

   //parole[0] conterrà !vedi_vincite
    //parole[1] conterrà il sessionID

 	 while (token1 != NULL) { 
    strcpy(parole1[quanteParole1++],token1);
        token1 = strtok(NULL,delimiter1); 
    }

    if(strcmp(parole1[1],sessionID) != 0) { //controllo che l'utente sia loggato
      invioMessaggio("--- Devi essere loggato per eseguire questo comando ---\n");
    } else{

    char *line_buf = NULL;
    size_t line_buf_size = 0;
    ssize_t line_size;

    char outputstr[1024] = ""; //stringa che verrà inviata al client
    char riepilogo[1024] = ""; //stringa per contenere il riepilogo delle vincite
    char date[10];  //stringa per contenere la data 
    char time[10]; //stringa per contenere l'ora 
    int trovato = 0;  
    float importi[5] = {0}; //array per memorizzare le somme degli importi di tutte le vincite 

    FILE* fd;

    fd = fopen("giocate_vincenti.txt", "r");

    if (!fd) {
      printf("--- Errore apertura del file ---\n");
      return;
    }

    line_size = getline(&line_buf, &line_buf_size, fd); //prende la prima linea del file giocate_vincenti.txt

    while (line_size >= 0) { // analizzo tutte le righe del file giocate_vincenti.txt 
   
    char stringaUtente[1024]; //controllo corrispondenza utente

    int numeroParole = 0;
    char delimiter[2] = " ";
    char* token = strtok(line_buf, delimiter);
    char parole[15][1024];  

    int k;
    for(k=0; k<15; k++) strcpy(parole[k],"");

    sprintf(stringaUtente,"%s:",utenteLoggato); //preparo la stringa per il controllo sull'utente loggato

    while (token != NULL) { 
      strcpy(parole[numeroParole],token);
      numeroParole++;
          token = strtok(NULL,delimiter); 
      }

      if(strcmp(parole[0],stringaUtente) == 0){ //se l'utente è loggato

        char firstline[1024] = "";

        if(trovato == 0){ // prima volta che trovo l'utente

          trovato = 1;
      //mi salvo il timestamp della giocata
          strcpy(date,parole[1]); 
          strcpy(time,parole[2]);
          sprintf(firstline,"Estrazione del %s ore %s\n",parole[1],parole[2]);
          strcat(outputstr,firstline);

        }

        if(trovato == 1){ 
        //altrimenti se l'ho già trovato devo controllare se è una nuova estrazione o la stessa
    		//controllo se è una nuova estrazione

          if(strcmp(parole[1],date) != 0 || strcmp(parole[2],time) != 0) { //se non è la stessa estrazione
          	//salvo il nuovo timestamp
            strcpy(date,parole[1]); 
            strcpy(time,parole[2]);
            strcat(outputstr,"***********************************************************\n");
            sprintf(firstline,"Estrazione del %s ore %s\n",parole[1],parole[2]); //nuova estrazione
            strcat(outputstr,firstline);

          }

          int i = 3; //poiche 0 contiene user, 1 date e 2 time, da 3 iniziano ruote e numeri

          while(strcmp(parole[i],"") != 0 ){ 

            char str[1024] = "";

            if(strcmp(parole[i+1],"") != 0 ) { //finche non sono all'ultima parola della riga
              if(strcmp(parole[i-1],"Estratto") == 0) importi[0] += atof(parole[i]);
              if(strcmp(parole[i-1],"Ambo") == 0) importi[1] +=  atof(parole[i]);
              if(strcmp(parole[i-1],"Terno") == 0) importi[2] +=  atof(parole[i]);
              if(strcmp(parole[i-1],"Quaterna") == 0) importi[3] +=  atof(parole[i]);
              if(strcmp(parole[i-1],"Cinquina") == 0) importi[4] +=  atof(parole[i]);

              sprintf(str,"%s ",parole[i]); //aggiusto la formattazione della stringa

            }

            else sprintf(str,"%s",parole[i]); //stampo ruote e numeri

            strcat(outputstr,str);
            i++;

          }
        }      
      }
   
      line_size = getline(&line_buf, &line_buf_size, fd); //prendo la prossima linea e continuo

    }

    if(strcmp(outputstr,"") == 0 ) 
    	invioMessaggio("--- Nessuna giocata registrata per l'utente ---\n");
      else {

    strcat(outputstr,"***********************************************************\n");
    sprintf(riepilogo,"Vincite su ESTRATTO: %f\nVincite su AMBO: %f\nVincite su TERNO: %f\nVincite su QUATERNA: %f\nVincite su CINQUINA: %f\n",importi[0],importi[1],importi[2],importi[3],importi[4]);
    strcat(outputstr,riepilogo);
    invioMessaggio(outputstr);}
    //se non è stata modificata outputstr significa che non ho trovato vincite per l'utente in questione
    }
}


//!esci, invalido il sessionID, setto connesso a 0, e invio il messaggio al client

void esci(char* buffer){

	int quanteParole = 0;
	char delimiter[2] = " ";
	char* token = strtok(buffer, delimiter);
	 
	char parole[2][1024];

	int i;
	for(i = 0; i<2; i++) strcpy(parole[i], ""); //devo inizializzarlo

	while( token != NULL){ //uso while per salvare le parole nell'array
		strcpy(parole[quanteParole++],token);
		token = strtok(NULL, delimiter);
	}

	if(strcmp(parole[1],sessionID) == 0){
		strcpy(sessionID,"999999999");
		invioMessaggio("--- Disconnessione avvenuta con successo ---\n");
		printf("--- Client disconnesso correttamente ---\n");
		connesso = 0;
	}else{
		invioMessaggio("--- Devi essere loggato per inviare questo comando ---\n");
	}
}



int tipoComando(char* buffer){

	char temp[2048];
	strcpy(temp,buffer);

	if(confronta(buffer,"!signup") == 1){
		signup(temp);
		return 2;
	}

	if(confronta(buffer,"!login") == 1){
		login(temp);
		return 3;
	}

	if(confronta(buffer,"!invia_giocata") == 1){
		invia_giocata(temp);
		return 4;
	}

	if(confronta(buffer,"!vedi_giocate") == 1){
		vedi_giocata(temp);
		return 5;
	}

	if(confronta(buffer,"!vedi_estrazione") == 1){
		vedi_estrazione(temp);
		return 6;
	}

	if(confronta(buffer,"!vedi_vincite") == 1){
		vedi_vincite(temp);
		return 7;
	}

	if(confronta(buffer,"!esci")==1){
		esci(temp);
		return 8;
	}

	return 0;

}



//funzione che genera un numero random da 1 a 90
int randomNumber(){ //genera un numero casuale da 1 a 90 per le estrazioni

  int random_number;
  struct timeval t; //per utilizzare un seed che dipende dai secondi e nanosecondi correnti
  gettimeofday(&t, NULL);
  srand(t.tv_usec * t.tv_sec);
    random_number = rand() % 90; //numero da 0 a 89
    random_number ++;
    return random_number;

}

void schedaInAttesa(char* scheda){

	int quanteParole = 0;
	char delimiter[2] = " ";
	char* token = strtok(scheda, delimiter);
	char parole[50][1024];

	int i = 1; //stavolta parto da 1, poiche in 0 c'è l'user

	while( token != NULL){ 
		strcpy(parole[quanteParole++],token);
		token = strtok(NULL, delimiter);
	}

	//inizializzo la schedina di partenza

	int j; //inizializzo a zero la schedaAttesa
	for(j = 0; j<11; j++) schedaAttesa.ruote[j] = 0;
		for(j = 0; j<90; j++) schedaAttesa.numeri[j] = 0;
			for(j = 0; j<5; j++) schedaAttesa.importo[j] = 0;
	
	bool impostaRuota = true;
	bool impostaNumeri = false;
	bool impostaImporti = false;
	int indexImporto = 0;
	
	//inserisco l'utente
	strcpy(schedaAttesa.utente,parole[0]); 

	while(strcmp(parole[i],"") != 0){ 

		if(impostaRuota == true){ //ruote
			int indexRuota;
			if(strcmp(parole[i],"-n") == 0){

				impostaRuota = false;
				impostaNumeri = true;
				goto fine;

			}

			indexRuota = individuaRuota(parole[i]);
          	schedaAttesa.ruote[indexRuota] = 1;      	
      		
     	 }

     	 if(impostaNumeri == true){ //numeri
     	 	if(strcmp(parole[i],"-i") == 0){

				impostaNumeri = false;
				impostaImporti = true;
				goto fine;
			}

			int numero;
			numero = atoi(parole[i]);
			schedaAttesa.numeri[numero-1] = 1;
		}

		if(impostaImporti == true){ //importi

			float importo;
			
			importo = atof(parole[i]);
			schedaAttesa.importo[indexImporto++] = importo;

			if(indexImporto == 5) 
				impostaImporti = false;

			}

     	 fine:
     	 i++;

		}	
	}

//funzioni per calcolare le possibili combinazioni

int fattoriale(int n){
	int c = 1, f=1;
	for (c = 1; c <= n; c++)
    f = f * c;
	return f;
}

int coeffBinomiale(int a, int b){
	int risu = 0;
	risu = (fattoriale(a)/(fattoriale(b)*fattoriale(a-b)));
	return risu;
}

float calcolaSomma(int tipoVincita, float importoGiocato){

	float estratto = 11.23, ambo = 250, terno = 4500, quaterna = 120000, cinquina = 6000000; //valori default della tabella
	float vincita = 0;
	int numeri = 0, ruote = 0;
	int i;

	for(i=0;i<90;i++) //calcolo quanti numeri ho giocato
		if(schedaAttesa.numeri[i] != 0)
			numeri++;

	for(i=0;i<11;i++) //calcolo quante ruote ho giocato
		if(schedaAttesa.ruote[i] != 0)
			ruote++;

	/*
	Calcolo come:
		cifra * quota_fissa * combinazioni_prese
		-----------------------------------------
		combinazioni_tot * ruote_giocate

	per le comb_tot posso usare il coeff binomiale
	n!
	----------
	k! * (n-k)!
	*/

	if(tipoVincita == 1){

		vincita = estratto;
		vincita = vincita/numeri;
		vincita = vincita/ruote;
		vincita = vincita*importoGiocato;
		return vincita;

	}

	if(tipoVincita == 2){

		vincita = ambo;
		vincita = coeffBinomiale(numeri,2);
		vincita = vincita/ruote;
		vincita = vincita*importoGiocato;
		return vincita;
	}

	if(tipoVincita == 2){

		vincita = terno;
		vincita = coeffBinomiale(numeri,2);
		vincita = vincita/ruote;
		vincita = vincita*importoGiocato;
		return vincita;
	}

	if(tipoVincita == 3){

		vincita = quaterna;
		vincita = coeffBinomiale(numeri,3);
		vincita = vincita/ruote;
		vincita = vincita*importoGiocato;
		return vincita;
	}

	if(tipoVincita == 4){

		vincita = cinquina;
		vincita = coeffBinomiale(numeri,4);
		vincita = vincita/ruote;
		vincita = vincita*importoGiocato;
		return vincita;
	}

	if(tipoVincita == 5){

		vincita = 6000000;
		vincita = coeffBinomiale(numeri,5);
		vincita = vincita/ruote;
		vincita = vincita*importoGiocato;
		return vincita;
	}

	return vincita; 

}




//controllo per ogni riga dell'estrazione, se nelle giocate in attesa c'è una posibile vincita

void controllaEstrazione(char* riga){

	//ogni riga dell'estrazione è nella forma: ruona n n n n n
	int quanteParole = 0;
	char delimiter[2] = " ";
	char* token = strtok(riga, delimiter);
	char parole[6][1024];

	char *line_buf = NULL;
    size_t line_buf_size = 0;
    ssize_t line_size;

    FILE* fd; //giocate_attesa.txt
    FILE* fd1; //giocate_vincenti.txt

    int indexRuota;
    int match; //per il tipo di risultato ottenuto nell'estrazione su una data riga
    int j;
    char bufferTemp[1024];
    char formatStr[1024]; //fomrattata "usr: time ruota numeri giocate vincite "
    bool vittoria = false; //settato se ho vinto

    // timestamp corrente
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    char timestamp[50]; //stringa dove verrà salvato il timestamp

    while (token != NULL) { //while per riempire l'array parole[]
    strcpy(parole[quanteParole++],token);
        token = strtok(NULL,delimiter); 
    }


    if ((fd=fopen("giocate_attesa.txt", "r"))==NULL) 
    	printf("--- Errore apertura del file ---\n");

    if ((fd1=fopen("giocate_vincenti.txt", "a"))==NULL) 
    	printf("--- Errore apertura del file ---\n");

    line_size = getline(&line_buf, &line_buf_size, fd); //estraggo la prima giocata in attesa e la inserisco in line_buf

    while (line_size >= 0) { //analizza tutte le righe del file delle giocate_attesa.txt

    	match = 0;
    	vittoria = false;

    	strcpy(bufferTemp,line_buf);

    	schedaInAttesa(bufferTemp);

    	//sul file delle giocate_vincenti,devo inserire correttamente anche il timestamp e data
    	sprintf(formatStr,"%s ",schedaAttesa.utente);
      	sprintf(timestamp,"%02d-%02d-%d %02d:%02d ",tm.tm_mday, tm.tm_mon + 1,tm.tm_year + 1900, tm.tm_hour, tm.tm_min);
      	strcat(formatStr,timestamp);
      	strcat(formatStr,parole[0]); //stampo ruota per ruota

      	indexRuota = individuaRuota(parole[0]);

      	if(schedaAttesa.ruote[indexRuota] == 1){ //se ho giacato la ruota che sto controllando nell'estrazione
      		for(j=1;j<6;j++){

      			int estratto = atoi(parole[j]); //parole[0] contiene la ruota, i numeri partono da parole[1]

      			if(schedaAttesa.numeri[estratto-1] == 1){ //se ho una corrispondenza con i numeri giocati

      				char stringa[1024];
      				sprintf(stringa," %i",estratto); //copio il numero estratto
      				strcat(formatStr,stringa);

      				if(match == 0)
      					 match = 1;

      				if(match >= 1) 
      					 match++;
      			}
      		}
      	
      	match--;

      	strcat(formatStr," >> ");

      	//guardo quanti ne ho trovati
      	//notare che se trovo 5 numeri uguali, ho per forza
      	// anche le sottoVincite(se sono state giocate ovviamente)
      	//per questo il controllo viene effettuato usando >= e non solo =

    	if(match == 5){

      		if(schedaAttesa.importo[4] != 0){ //ho fatto cinquina

      			float vincita;
      			char strVincita[1024];

      			vincita = calcolaSomma(5,schedaAttesa.importo[4]);
      			sprintf(strVincita,"%f ",vincita);
      			strcat(formatStr,"Cinquina ");
	            strcat(formatStr,strVincita);

	            vittoria = 1; //ho vinto 

	          }
        	}

        if(match >= 4){

        		if(schedaAttesa.importo[3] != 0){

				float vincita;
      			char strVincita[1024];

      			vincita = calcolaSomma(4,schedaAttesa.importo[3]);
      			sprintf(strVincita,"%f ",vincita);
      			strcat(formatStr,"Quaterna ");
	            strcat(formatStr,strVincita);

	            vittoria = 1; 
	          }
        	}

        if(match >= 3){

        		if(schedaAttesa.importo[2] != 0){

				float vincita;
      			char strVincita[1024];

      			vincita = calcolaSomma(3,schedaAttesa.importo[2]);
      			sprintf(strVincita,"%f ",vincita);
      			strcat(formatStr,"Terno ");
	            strcat(formatStr,strVincita);

	            vittoria = 1; 
	          }
        	}

        if(match >= 2){

        		if(schedaAttesa.importo[1] != 0){

				float vincita;
      			char strVincita[1024];

      			vincita = calcolaSomma(2,schedaAttesa.importo[1]);
      			sprintf(strVincita,"%f ",vincita);
      			strcat(formatStr,"Ambo ");
	            strcat(formatStr,strVincita);

	            vittoria = 1; 
	          }
        	}

        if(match >= 1){

        		if(schedaAttesa.importo[0] != 0){

				float vincita;
      			char strVincita[1024];

      			vincita = calcolaSomma(1,schedaAttesa.importo[0]);
      			sprintf(strVincita,"%f ",vincita);
      			strcat(formatStr,"Estratto ");
	            strcat(formatStr,strVincita);

	            vittoria = 1; 
	          }
        	}

        //controlli sulle vincite sono scritti in ordine inverso per rispettare
        // la formattazione della stampa richiesta nella documentazione Cinquina, quaterna...
      	}

      	if(vittoria == 1) fprintf(fd1, "%s\n", formatStr ); //se ho vinto stampo

        line_size = getline(&line_buf, &line_buf_size, fd); //prendo la prossima linea dell'estrazione

    }

    fclose(fd);
    fclose(fd1);

}



void vincite(){

	FILE* fd;
	
	char *line_buf = NULL;
    size_t line_buf_size = 0;
    ssize_t line_size;

    if ((fd=fopen("ultima_estrazione.txt", "r"))==NULL) 
    	printf("--- Errore apertura file ---\n");

    line_size = getline(&line_buf, &line_buf_size, fd); //prende la prima linea del file fd

    while (line_size >= 0) { // analizza tutte le linee del file ultima_estrazione.txt 

      controllaEstrazione(line_buf); //per ogni riga di ultima_estrazione.txt controllo se vi è una vittoria
   
      line_size = getline(&line_buf, &line_buf_size, fd); //prendo la prossima linea e continuo

    }
}


void estraiNumeri(int* ruota) {

    int i, j;

    unsigned u = rand()%1000+1;

    srand((unsigned)time(NULL) +u);

    for(i = 0; i<5; i++) {

        ruota[i] = rand()%90 + 1;

        for(j=0;j<i; j++) {

            if(ruota[i] == ruota[j]) {
                i--;
                break;
            }
        }
    }
}

void estrazione(){

	sleep(tempo*60);
	
	FILE* fd;
	FILE* fd1; //ultima_estrazione.txt
	FILE* fd2; //giocate_in_attesa.txt


	if ((fd=fopen("estrazioni.txt", "a"))==NULL) printf("--- Errore apertura del file ---\n");
	if ((fd1=fopen("ultima_estrazione.txt", "w"))==NULL) printf("--- Errore apertura del file ---\n");
	
	int i;
	for(i=0;i<11;i++){

		char riga[1020]="";
		int numeri[5];

		if(i==0) strcat(riga,"Bari      "); 
	    if(i==1) strcat(riga,"Cagliari  "); 
	    if(i==2) strcat(riga,"Firenze   "); 
	    if(i==3) strcat(riga,"Genova    "); 
	    if(i==4) strcat(riga,"Milano    "); 
	    if(i==5) strcat(riga,"Napoli    "); 
	    if(i==6) strcat(riga,"Palermo   "); 
	    if(i==7) strcat(riga,"Roma      "); 
	    if(i==8) strcat(riga,"Torino    "); 
	    if(i==9) strcat(riga,"Venezia   "); 
	    if(i==10) strcat(riga,"Nazionale "); 

		estraiNumeri(numeri);

		char formatStr[20];
		int k;

		for(k=0;k<5;k++){

			if(numeri[k] >= 10 && numeri[k] <= 90) //aggiusto il formato della stampa
				sprintf(formatStr,"%i ",numeri[k]);

			else
				sprintf(formatStr," %i ",numeri[k]);

			strcat(riga,formatStr);
		}
	
		 fprintf(fd, "%s\n", riga); //inserisce la stringa nel formato impostato nel file estrazioni.txt
    	fprintf(fd1, "%s\n", riga); //inserisce la stringa nel formato impostato nel file ultima_estrazione.txt

	}

	fprintf(fd, "\n");
    fclose(fd);
    fclose(fd1);

    vincite(); //controllo le vincite
    spostaGiocateEstratte();//sposto nel file giocate_estratte le estrazioni controllate precedentemente
 
    //svuoto giocate_attesa.txt
    if ((fd2=fopen("giocate_attesa.txt", "w"))==NULL) 
    	printf("--- Errore apertura del file ---\n");

  	printf("--- Estrazione effettuata con successo ---\n");
  	fclose(fd2);

  }

/*
Funzione che controlla l'ip del processo client che tenta di loggarsi
se l'ip risulta bloccato, il client non viene fatto connettere
La funzione aspetta un tempo di 30 min per far si che l'utente possa poi
loggarsi nuovamente
*/
int controllaBlackList(){

	char *line_buf = NULL;
  	size_t line_buf_size = 0;
  	ssize_t line_size;

	FILE *fd = fopen("blacklist.txt", "r"); //apro blacklist.txt in lettura

  	if (!fd) {
    	printf("--- Errore nell’apertura del file! ---\n");
  	}

  	//sprintf(timestampCorrente,"%d",(int)time(NULL)); //salvo timestamp corrente
  	int timestampAttuale = time(NULL); //timestamp attuale

  	line_size = getline(&line_buf, &line_buf_size, fd); //prende la prima linea del file fd

  	while (line_size >= 0) {

  	int quanteParole = 0;
	char delimiter[2] = " ";
	char* token = strtok(line_buf, delimiter);
	 
	char parole[3][1024]; 
	//utile solo parole[0] e parole[1] contenenti timestamp e ipClient 

	int i;
	for(i = 0; i<3; i++) strcpy(parole[i], ""); //devo inizializzarlo

	while( token != NULL){ //uso while per salvare le parole nell'array
		strcpy(parole[quanteParole++],token);
		token = strtok(NULL, delimiter);
	}

	if(strcmp(inet_ntoa(cl_addr.sin_addr),parole[1]) == 0) //mi restiuisce l'ip del client connesso
		if(timestampAttuale - atoi(parole[0]) <= 30*60 ) return 0; //se trovo una corrispondenza, sblocco
   
      line_size = getline(&line_buf, &line_buf_size, fd); //prendo la prossima linea e continuo

    }
    return 1; //ok login consentito

}

int main(int argc, char* argv[]){
    int ret;
    int porta;
    int idComando = 0;
    socklen_t len;
    pid_t pid, pid2;
    char msgFromToClient[BUFFER_SIZE]; //messaggio ricevuto e reinviato al client
    char temp[BUFFER_SIZE]; //buffer appoggio

    if(argc < 2 || argc > 3) { //controllo argomenti passati al server per l'avvio
		printf("Errore: specificare almeno il numero di porta del server!\n");
		exit(-1);
	}
    
    //passaggio parametri porta e tempo estrazione
    porta = atoi(argv[1]);
    if(argc == 3) tempo = atoi(argv[2]);
   		else tempo = 5;
    
    /* Creazione socket */
    sd = socket(AF_INET, SOCK_STREAM, 0);
    printf("Socket creato correttamente\n");
    /* Creazione indirizzo di bind */
    memset(&my_addr, 0, sizeof(my_addr)); // Pulizia 
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(porta);
    my_addr.sin_addr.s_addr = INADDR_ANY;
    
    ret = bind(sd, (struct sockaddr*)&my_addr, sizeof(my_addr) );
    printf("Bind Server OK\n");

    ret = listen(sd, 10);
    printf("Server in attesa connessione\n");

    if(ret < 0){
        perror("Errore in fase di bind: \n");
        exit(-1);
    }

    //fork con un secondo processo per gestire l'estrazione
    pid2 = fork();

    if(pid2 == 0){

    	while(1){
    	
    		estrazione();

    	}
    }


    while(1){
        
        len = sizeof(cl_addr);
    
        //Accetto nuove connessioni
        new_sd = accept(sd, (struct sockaddr*) &cl_addr, &len);
        connesso = 1;

        //la funzione interviene quanto il client con lo stesso ip bloccato, tenta nuovamente di connettersi
        if(controllaBlackList() == 1){

        	printf("Connessione con il Client OK\n");
        	invioMessaggio("Connessione con il server stabilita\n");

    	}else{

    		printf("--- Connessione bloccata, utente in blacklist ---\n");
        	invioMessaggio("Errore3: utente bloccato! Riprova più tardi\n");
        	connesso = 0;

        }
               
        pid = fork();
		
		    if( pid == 0 ){
            // Sono nel processo figlio
            close(sd);
        
            while(connesso == 1){

            ricezioneMessaggio(msgFromToClient);

            printf("Comando ricevuto : %s\n",msgFromToClient );

            strcpy(temp, msgFromToClient);

            idComando = tipoComando(temp);

            if(idComando == 0){

            	printf("Errore, comando inserito non valido\n");
            	exit(-1);

            	}
            }

            close(new_sd);
            exit(1);

        } else {

            close(new_sd);

        	}
    	}
	}