#include <stdio.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <stdlib.h> 
#include <string.h> 
#include <unistd.h> 
#include <netdb.h>
#include <netinet/in.h> 
#include <arpa/inet.h>
#include<time.h>

#include <sodium.h>

#define BUF_SIZE 1024 


char protocolo[10];
char endProxy[100];
char endServer[100]; 
char comando[100];  
int fd_tcp;
int fd_udp;
int port;
struct sockaddr_in proxy_addr, proxy_addr_udp;   
struct hostent *hostProxy;
char client_ip_address[INET_ADDRSTRLEN];

void list();
void quit();
void download_file(char* buffer);
void protocolo_tcp();
void protocolo_udp();
void erro(char *msg); 

char comando_erro[]= "Erro: Comando invalido\n";
char comando_erro_protocolo[]= "Erro: Protocolo invalido\n";
char comando_erro_codificacao[]= "Erro: Codificacao invalida\n";
char comando_erro_ficheiro[]= "Erro: Ficheiro nao existe\n";
char comando_incompleto[]= "Erro: Comando incompleto\n";
char comando_list[]= "LIST";
char comando_quit[]= "QUIT";

unsigned char k[crypto_secretbox_KEYBYTES];

int main(int argc, char *argv[]) { 
	if (sodium_init() == -1) {
        	return 1;
	}

	FILE *fp = fopen("key.txt", "wb");
	fread(k, sizeof(k), 1, fp);
	fclose(fp);

	if (argc != 5) {      
		printf("cliente <endereco do proxy> <endereco do servidor> <porto> <protocolo>\n");      
		exit(-1);   
	} 

	port = atoi(argv[3])+2;
	//endereço do servidor
	strcpy(endServer, argv[2]);
	
	//endereço do proxy
	strcpy(endProxy, argv[1]);   
	if ((hostProxy = gethostbyname(endProxy)) == 0)      
		erro("Nao consegui obter o endereco do proxy");
	
	//  socket address structure para comunicar com o proxy
	bzero((void *) &proxy_addr, sizeof(proxy_addr));   
	proxy_addr.sin_family = AF_INET;   
	proxy_addr.sin_addr.s_addr = ((struct in_addr *)(hostProxy->h_addr))->s_addr;   
	proxy_addr.sin_port = htons((short) port);

	
	// socket UDP do client
	if ( (fd_udp = socket(AF_INET, SOCK_DGRAM, 0)) < 0)   
		erro("na funcao socket"); 

	//protocolo
	strcpy(protocolo, argv[4]);
	if(strcmp(protocolo, "TCP")==0)
		protocolo_tcp();
	else if(strcmp(protocolo, "UDP")==0)
		protocolo_udp();
	else erro("Protocolo invalido");

	close(fd_tcp);   
	exit(0); 
} 

void protocolo_tcp(){
	//socket TCP para comunicar com o proxy
	if((fd_tcp = socket(AF_INET,SOCK_STREAM,0)) == -1)  
		erro("socket");   
	if( connect(fd_tcp,(struct sockaddr *)&proxy_addr,sizeof (proxy_addr)) < 0)  
		erro("Connect");
	
	int nread = 0;  
	char buffer[BUF_SIZE];

	// enviar o enderço do servidor para o proxy
	usleep(1);
	write(fd_tcp, endServer, strlen(endServer)+1); 

	//obter a socket adress structure UDP do proxy
	do {   
		nread = read(fd_tcp, (struct sockaddr *)&proxy_addr_udp, sizeof(proxy_addr_udp));
		fflush(stdout);  
	} while (nread == 0);

	//enviar a socket adress structure UDP do cliente
	sendto(fd_udp, NULL, 0, 0, (struct sockaddr *)&proxy_addr_udp, sizeof(proxy_addr_udp));

	while(1){ 
		
		// receber menu
  		do {   
			nread = read(fd_tcp, buffer, BUF_SIZE-1);
			buffer[nread]=0;
			fflush(stdout);  
		} while (nread == 0);
		printf("%s\n", buffer);

		// enviar o comando	
		gets(comando);
		write(fd_tcp, comando, strlen(comando)+1);

		// receber comfirmação
		do {   
			nread = read(fd_tcp, buffer, BUF_SIZE-1);
			buffer[nread]=0;
			fflush(stdout);  
		} while (nread == 0);

		//execução do comando
		if(strcmp(buffer, comando_list)==0) list();
		else if (strcmp(buffer, comando_quit)==0) quit();
		else if (strcmp(buffer, comando_erro)==0) printf("%s\n", buffer);
		else if (strcmp(buffer, comando_erro_protocolo)==0) printf("%s\n", buffer);
		else if (strcmp(buffer, comando_erro_codificacao)==0) printf("%s\n", buffer);
		else if (strcmp(buffer, comando_erro_ficheiro)==0) printf("%s\n", buffer);
		else if (strcmp(buffer, comando_incompleto)==0) printf("%s\n", buffer);
		else download_file(buffer);
	}

}

void protocolo_udp(){
	printf("Protocolo indisponivel\n");
	exit(0);
}

void list(){
	int nread=0;
	char buffer[BUF_SIZE];
	do {   
		nread = read(fd_tcp, buffer, BUF_SIZE-1);
		buffer[nread]=0;
		fflush(stdout);  
	} while (nread == 0);
	printf("%s\n", buffer);
}

void quit(){
	close(fd_tcp);
	close(fd_udp);
	exit(0);
}

void download_file(char *file_name){
	time_t tempo= time(NULL);

	printf("Nome do ficheiro: %s\n", file_name);
	FILE *fp = fopen(file_name, "wb");
	int size;
	int nread = 0;  
       
	char* ptr = strtok(comando, " ");
	ptr= strtok(NULL, " ");

	if(strcmp(ptr, "TCP")==0){
		printf("Protocolo utilizado: TCP\n");
		do {   
			nread = read(fd_tcp, &size, sizeof(int));
			fflush(stdout);  
		} while (nread == 0);

		char buffer[size];
		do {   
			nread = read(fd_tcp, buffer, size);
			fflush(stdout);  
		} while (nread == 0);
		printf("Numero de bytes transferidos: %d\n", nread);
		
		ptr= strtok(NULL, " ");
		if(strcmp(ptr, "ENC")==0){
			unsigned char nonce[crypto_secretbox_NONCEBYTES];
			do {   
				nread = read(fd_tcp, nonce, sizeof(nonce));
				fflush(stdout);  
			} while (nread == 0);

			char decrypted[size-crypto_secretbox_MACBYTES];
			crypto_secretbox_open_easy(decrypted, buffer, size, nonce, k);
			fwrite(decrypted, sizeof(decrypted), 1, fp);
			
		}
		else fwrite(buffer, nread, 1, fp);

		fclose(fp);
	}
	else{
		printf("Protocolo utilizado: UDP\n");
		recvfrom(fd_udp, &size, sizeof(int), 0, NULL, NULL);
		
		char buffer[size];
		
		nread= recvfrom(fd_udp, buffer, size, 0, NULL, NULL);
		printf("Numero de bytes transferidos: %d\n", nread);

		ptr= strtok(NULL, " ");
		if(strcmp(ptr, "ENC")==0){
			unsigned char nonce[crypto_secretbox_NONCEBYTES];
			nread= recvfrom(fd_udp, nonce, sizeof(nonce), 0, NULL, NULL);
		
			char decrypted[size-crypto_secretbox_MACBYTES];
			crypto_secretbox_open_easy(decrypted, buffer, size, nonce, k);
			fwrite(decrypted, sizeof(decrypted), 1, fp);
			
		}
		else fwrite(buffer, nread, 1, fp);

		fclose(fp);
	}
	
	time_t dif= time(NULL) - tempo;
	dif*=1000;
	printf("Tempo de download: %3.6ld ms\n\n", dif);

}

void erro(char *msg){  
	printf("Erro: %s\n", msg);  
	exit(-1); 
} 
