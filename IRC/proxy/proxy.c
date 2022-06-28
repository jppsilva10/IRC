#include <sys/socket.h> 
#include <sys/types.h> 
#include <netinet/in.h> 
#include <unistd.h> 
#include <stdlib.h> 
#include <stdio.h> 
#include <netdb.h> 
#include <string.h> 
#include <sys/types.h> 
#include <sys/wait.h> 
#include <string.h> 
#include <arpa/inet.h>
#include <pthread.h>
#include <semaphore.h> 
#include <fcntl.h> 
#define BUF_SIZE 1024



sem_t *mutex;
pthread_t thr;

int losses;
int save;
char file[100];
char protocolo[4];
char comando[BUF_SIZE];
char endereco_origem[INET_ADDRSTRLEN];
char endereco_destino[INET_ADDRSTRLEN];
int porto_origem;
int porto_destino;

struct hostent *hostServer;

char proxy_ip_address[INET_ADDRSTRLEN];

int numero_clientes;
int fd_tcp_server, fd_tcp_client, fd_tcp_proxy, fd_udp;
struct sockaddr_in server_addr, client_addr, proxy_addr, server_addr_udp, client_addr_udp, addr_udp;
int client_addr_size;
int server_addr_size;
char endServer[100];
int porto;

void list();
void quit();
void download(char *file_name);
void process_client(); 
void erro(char *msg);

char comando_erro[]= "Erro: Comando invalido\n";
char comando_erro_protocolo[]= "Erro: Protocolo invalido\n";
char comando_erro_codificacao[]= "Erro: Codificacao invalida\n";
char comando_erro_ficheiro[]= "Erro: Ficheiro nao existe\n";
char comando_incompleto[]= "Erro: Comando incompleto\n";
char comando_list[]= "LIST";
char comando_quit[]= "QUIT";

int main(int argc, char *argv[]) {

	if (argc != 2) {      
		printf("ircproxy <porto>\n");      
		exit(-1);   
	}
	
	sem_unlink("MUTEX");
	mutex=sem_open("MUTEX",O_CREAT|O_EXCL,0700,1);
	
	//obter o porto
	porto= atoi(argv[1])+2;
	numero_clientes=1;
	losses=0;
	save=0;
	file[0]=0;
 	
	//socket address structure TCP do proxy
	bzero((void *) &proxy_addr, sizeof(proxy_addr));   
	proxy_addr.sin_family = AF_INET;   
	char src[]="127.0.0.1";
	inet_pton(AF_INET, src, &proxy_addr.sin_addr.s_addr);   
	proxy_addr.sin_port = htons((short) porto ); 

	inet_ntop(AF_INET, &proxy_addr.sin_addr.s_addr, proxy_ip_address, INET_ADDRSTRLEN);
	printf("IPv4 address: %s\n", proxy_ip_address);
	printf("Port: %d\n", porto);  

	// socket TCP do proxy
	if ( (fd_tcp_proxy = socket(AF_INET, SOCK_STREAM, 0)) < 0)   
		erro("na funcao socket"); 

	// socket UDP do proxy
	if ( (fd_udp = socket(AF_INET, SOCK_DGRAM, 0)) < 0)   
		erro("na funcao socket");

	if ( bind(fd_tcp_proxy,(struct sockaddr*)&proxy_addr,sizeof(proxy_addr)) < 0)  
		erro("na funcao bind");   
	if( listen(fd_tcp_proxy, numero_clientes) < 0)   
		erro("na funcao listen");   
	client_addr_size = sizeof(client_addr);

	//receber clientes
	while (1) {         
		while(waitpid(-1,NULL,WNOHANG)>0);     
		// wait for new connection     
		fd_tcp_client = accept(fd_tcp_proxy,(struct sockaddr *)&client_addr, (socklen_t *)&client_addr_size);     
		if (fd_tcp_client > 0) {       
			if (fork() == 0) {         
				close(fd_tcp_proxy);     
				process_client();
				exit(0);       
			}     
			close(fd_tcp_client);     
		}   
	}   
	return 0; 
} 

void* proxy_menu(void *arg){
	char menu[100]= "Use um destes comandos:\nLOSSES<n>\nSHOW\nSAVE\n"; 
	char command[20];
	char *ptr;
	char *numero;

	while(1){
		printf("%s\n", menu);
		gets(command);
		if(strcmp(command, "SHOW")==0){
			sem_wait(mutex);
			printf("Endereco de origem: %s\n", endereco_origem);
			printf("Porto de origem: %d\n", porto_origem);
			printf("Endereco de destino: %s\n", endereco_destino);
			printf("Porto de destino: %d\n", porto_destino);
			printf("Protocolo: %s\n\n", protocolo);
			sem_post(mutex);
		}
		else if(strcmp(command, "SAVE")==0){
			sem_wait(mutex);
			if( save== 0){
				save= 1;
				printf("SAVE ON\n\n");
			}
			else {
				save= 0;
				printf("SAVE OFF\n\n");
			}
			sem_post(mutex);
		}
		else{
			ptr = strtok(command, " ");
			numero = strtok(NULL, " ");
			if(strcmp(ptr, "LOSSES")==0){
				if(numero!=NULL){
					if(atoi(numero)<=100 && atoi(numero)>=0){					
						sem_wait(mutex);
						losses = atoi(numero);
						sem_post(mutex);
					}
					else printf("Erro: O numero tem que ser entre 0 e 100\n\n");
				}
				else printf("Erro: Comando incompleto\n\n");
			}
			else printf("%s\n", comando_erro);
		}	
	}
}

void process_client(){
	pthread_create(&thr, NULL, proxy_menu, NULL);
	sem_wait(mutex);
	inet_ntop(AF_INET, &client_addr.sin_addr.s_addr, endereco_destino, INET_ADDRSTRLEN);
	porto_destino= (int) ntohs(client_addr.sin_port);
	sem_post(mutex);

	int nread = 0;  
	char buffer[BUF_SIZE]; 
  	do {   
		nread = read(fd_tcp_client, buffer, BUF_SIZE-1);
		buffer[nread]=0;
		fflush(stdout);  
	} while (nread == 0);

	//endereço do servidor
	char str[50]= "Nao consegui obter o endereco do servidor";
	strcpy(endServer, buffer);   
	if ((hostServer = gethostbyname(endServer)) == 0){     
		erro(str);
	}

	//socket address structure para comunicar com o servidor
	bzero((void *) &server_addr, sizeof(server_addr));   
	server_addr.sin_family = AF_INET;   
	server_addr.sin_addr.s_addr = ((struct in_addr *)(hostServer->h_addr))->s_addr;   
	server_addr.sin_port = htons((short) (porto-2));
	
	//socket TCP para comunicar com o servidor
	if((fd_tcp_server = socket(AF_INET,SOCK_STREAM,0)) == -1)  
		erro("socket");   
	server_addr_size= sizeof(server_addr);

	if( connect(fd_tcp_server,(struct sockaddr *)&server_addr, server_addr_size) < 0)  
		erro("Connect"); 

	sem_wait(mutex);
	inet_ntop(AF_INET, &server_addr.sin_addr.s_addr, endereco_origem, INET_ADDRSTRLEN);
	porto_origem= (int) ntohs(server_addr.sin_port);
	strcpy(protocolo, "TCP");
	sem_post(mutex);

	//obter a socket address structure UDP do servidor
	do {   
		nread = read(fd_tcp_server, (struct sockaddr *)&server_addr_udp, sizeof(server_addr_udp));
		fflush(stdout);  
	} while (nread == 0);	
	
	//enviar ao servidor a socket address structure UDP do proxy
	sendto(fd_udp, NULL, 0, 0, (struct sockaddr *)&server_addr_udp, sizeof(server_addr_udp));
	
	//obter a socket address structure UDP do proxy
	do {   
		nread = read(fd_tcp_server, (struct sockaddr *)&addr_udp, sizeof(addr_udp));
		fflush(stdout);  
	} while (nread == 0);
	
	//enviar a socket address structure UDP do proxy
	write(fd_tcp_client, (struct sockaddr *)&addr_udp, sizeof(addr_udp));
	client_addr_size = sizeof(client_addr_udp);
	//obter a socket address structure UDP do cliente
	recvfrom(fd_udp, NULL, 0, 0, (struct sockaddr *)&client_addr_udp, (socklen_t *)&client_addr_size);	

	nread = 0; 
	while(1){
		// mensagem para o cliente   (menu)
		
		do {   
			nread = read(fd_tcp_server, buffer, BUF_SIZE-1);
			buffer[nread]=0;
			fflush(stdout);  
		} while (nread == 0);
		//printf("%s\n", buffer);  ----	

		write(fd_tcp_client, buffer, strlen(buffer)+1);
		
		// mensagem para o servidor (comando)
		do {   
			nread = read(fd_tcp_client, comando, BUF_SIZE-1);
			comando[nread]=0;
			fflush(stdout);  
		} while (nread == 0);
	
		write(fd_tcp_server, comando, strlen(comando)+1);

		// mensagem para o cliente	(comfirmacao)
		nread = 0; 
		do {   
			nread = read(fd_tcp_server, buffer, BUF_SIZE-1);
			buffer[nread]=0;
			fflush(stdout);  
		} while (nread == 0);

		write(fd_tcp_client, buffer, strlen(buffer)+1);

		// execuÇão do comando
		if(strcmp(buffer, comando_list)==0) list();
		else if (strcmp(buffer, comando_quit)==0) quit();
		else if (strcmp(buffer, comando_erro)==0) {}
		else if (strcmp(buffer, comando_erro_protocolo)==0) {}
		else if (strcmp(buffer, comando_erro_codificacao)==0){}
		else if (strcmp(buffer, comando_erro_ficheiro)==0) {}
		else if (strcmp(buffer, comando_incompleto)==0) {}
		else download(buffer);
	}
}

void list(){

	char buffer[BUF_SIZE];
	int nread = 0; 
	do {   
		nread = read(fd_tcp_server, buffer, BUF_SIZE-1);
		buffer[nread]=0;
		fflush(stdout);  
	} while (nread == 0);

	write(fd_tcp_client, buffer, strlen(buffer)+1);

}

void quit(){
	close(fd_tcp_client);
	close(fd_tcp_server);
	close(fd_udp);
	pthread_cancel(thr);
	if(strlen(file)!=0) remove(file);
	exit(0);
}

void download(char *file_name){
	int nread=0;
	int size;
	
	char* ptr = strtok(comando, " ");
	ptr= strtok(NULL, " ");

	if(strcmp(ptr, "TCP")==0){
		do {   
			nread = read(fd_tcp_server, &size, sizeof(int));
			fflush(stdout);  
		} while (nread == 0);
	
	
		write(fd_tcp_client, &size, sizeof(size));
	
		char buffer[size];
		do {   
			nread = read(fd_tcp_server, buffer, size);
			fflush(stdout);  
		} while (nread == 0);
	

		write(fd_tcp_client, buffer, size);
		
		ptr= strtok(NULL, " ");
		if(strcmp(ptr, "ENC")==0){
			unsigned char nonce[BUF_SIZE];
			do {   
				nread = read(fd_tcp_server, nonce, BUF_SIZE);
				fflush(stdout);  
			} while (nread == 0);
			
			write(fd_tcp_client, nonce, nread);
		
		}

		FILE *fp;
		sem_wait(mutex);
		if(save==1){
			if(strlen(file)!=0) remove(file); 

			strcpy(file, file_name);
			fp = fopen(file_name, "wb");
			fwrite(buffer, size, 1, fp);
			fclose(fp);
		}
		sem_post(mutex);
	}
	else{
		recvfrom(fd_udp, &size, sizeof(int), 0, NULL, NULL);
		
		sendto(fd_udp, &size, sizeof(size), 0, (struct sockaddr *)&client_addr_udp, sizeof(client_addr_udp));

		char buffer[size];
		
		recvfrom(fd_udp, buffer, size, 0, NULL, NULL);
		
	
		//perdas
		sem_wait(mutex);
		size*= (100-losses);
		size= (int) (size/100);
		sem_post(mutex);

		sendto(fd_udp, buffer, size, 0, (struct sockaddr *)&client_addr_udp, sizeof(client_addr_udp));

		ptr= strtok(NULL, " ");
		if(strcmp(ptr, "ENC")==0){
			unsigned char nonce[BUF_SIZE];
			nread= recvfrom(fd_udp, nonce, BUF_SIZE, 0, NULL, NULL);
			sendto(fd_udp, nonce, nread, 0, (struct sockaddr *)&client_addr_udp, sizeof(client_addr_udp));
		
		}
		
		FILE *fp;
		sem_wait(mutex);
		if(save==1){
			if(strlen(file)!=0) remove(file); 

			strcpy(file, file_name);
			fp = fopen(file_name, "wb");
			fwrite(buffer, size, 1, fp);
			fclose(fp);
		}
		sem_post(mutex);
	}

}

void erro(char *msg){  
	printf("Erro: %s\n", msg);  
	exit(-1); 
} 


