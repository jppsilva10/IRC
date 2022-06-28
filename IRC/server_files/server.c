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

#include <sodium.h>

#define BUF_SIZE 1024
#define ENDSERVER 

struct hostent *hostServer;
int numero_clientes;
int fd_tcp, fd_tcp_proxy, fd_udp;   
struct sockaddr_in addr, addr_udp, proxy_addr, proxy_addr_udp;   
int proxy_addr_size;
char server_ip_address[INET_ADDRSTRLEN];

void list();
void quit();
int  download_file(char* protocolo, char* codificacao,char* file);
void process_client(); 
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

	if (argc != 3) {      
		printf("server <porto> <numero maximo de clientes>\n");      
		exit(-1);   
	} 
	//numero máximo de clientes
	numero_clientes= atoi(argv[2]);
 
	//socket address structure TCP do servidor 
	bzero((void *) &addr, sizeof(addr));   
	addr.sin_family = AF_INET; 
	char src[]="127.0.0.1";
	inet_pton(AF_INET, src, &addr.sin_addr.s_addr);
	addr.sin_port = htons((short) atoi(argv[1]) ); 

	inet_ntop(AF_INET, &addr.sin_addr.s_addr, server_ip_address, INET_ADDRSTRLEN);
	printf("IPv4 address: %s\n", server_ip_address);
	printf("Port: %d\n", atoi(argv[1]));  
		
	//socket address structure UDP do servidor
	bzero((void *) &addr_udp, sizeof(addr_udp));   
	addr_udp.sin_family = AF_INET; 
	inet_pton(AF_INET, src, &addr_udp.sin_addr.s_addr);
	addr_udp.sin_port = htons((short) (atoi(argv[1])+1) ); 

	// socket UDP do servidor	
	if ( (fd_udp = socket(AF_INET, SOCK_DGRAM, 0)) < 0)   
		erro("na funcao socket");  

	if ( bind(fd_udp,(struct sockaddr*)&addr_udp,sizeof(addr_udp)) < 0)  
		erro("na funcao bind");   

	// socket TCP do servidor
	if ( (fd_tcp = socket(AF_INET, SOCK_STREAM, 0)) < 0)   
		erro("na funcao socket"); 
  
	if ( bind(fd_tcp,(struct sockaddr*)&addr,sizeof(addr)) < 0)  
		erro("na funcao bind");   
	if( listen(fd_tcp, numero_clientes) < 0)   
		erro("na funcao listen");   
	proxy_addr_size = sizeof(proxy_addr);

	while (1) {         
		while(waitpid(-1,NULL,WNOHANG)>0);     
		// wait for new connection     
		fd_tcp_proxy = accept(fd_tcp,(struct sockaddr *)&proxy_addr, (socklen_t *)&proxy_addr_size);     
		if (fd_tcp_proxy > 0) {       
			if (fork() == 0) {         
				close(fd_tcp);     
				process_client();
				exit(0);       
			}     
			close(fd_tcp_proxy);     
		}   
	}   
	return 0; 
} 

void process_client(){ 
	char mensagem[100]= "Use um destes comandos:\nLIST\nDOWNLOAD<TCP/UDP><ENC/NOR><nome>\nQUIT\n"; 
	char *download;
	char *protocolo;
	char *codificacao;
	char* file;
	
	usleep(1);
	//enviar a socket address structure UDP do servidor
	write(fd_tcp_proxy, (struct sockaddr *)&addr_udp, sizeof(addr_udp));
	proxy_addr_size = sizeof(proxy_addr_udp);
	//obter a socket address structure UDP do proxy
	recvfrom(fd_udp, NULL, 0, 0, (struct sockaddr *)&proxy_addr_udp, (socklen_t *)&proxy_addr_size);
	//enviar a socket address structure UDP do proxy
	write(fd_tcp_proxy, (struct sockaddr *)&proxy_addr_udp, sizeof(proxy_addr_udp));	

	int nread = 0;  
	char buffer[BUF_SIZE];
	while(1){ 
		nread = 0; 
		usleep(1);
		write(fd_tcp_proxy, mensagem, strlen(mensagem)+1);
		do {   
			nread = read(fd_tcp_proxy, buffer, BUF_SIZE-1);
			buffer[nread]=0; 
			fflush(stdout);  
		} while (nread == 0);
		if(strcmp(buffer,"LIST")==0){
			list();
		}   
		else if(strcmp(buffer,"QUIT")==0){
			quit();
		} 
		else {
			download= strtok(buffer, " ");
			if(download==NULL){
				write(fd_tcp_proxy, comando_erro, strlen(comando_erro)+1);
				continue;
			}
			protocolo= strtok(NULL, " ");
			if(protocolo==NULL){
				write(fd_tcp_proxy, comando_incompleto, strlen(comando_incompleto)+1);
				continue;
			}
			codificacao= strtok(NULL, " ");
			if(codificacao==NULL){
				write(fd_tcp_proxy, comando_incompleto, strlen(comando_incompleto)+1);
				continue;
			}
			file= strtok(NULL, " ");
			if(file==NULL){
				write(fd_tcp_proxy, comando_incompleto, strlen(comando_incompleto)+1);
				continue;
			}
			if(strcmp(download,"DOWNLOAD")==0){
				download_file(protocolo, codificacao, file);
			}
			else{
				write(fd_tcp_proxy, comando_erro, strlen(comando_erro)+1);
			}
		}
	}  
	close(fd_tcp_proxy); 
}

void list(){
	char confirmation[5]= "LIST";
	char file_names[100]= "facebook_logo.png\nredes_sociais.jpg\nvideo.mp4\nsaxriff.wav\nIRC_projeto.pdf";

	write(fd_tcp_proxy, confirmation, strlen(confirmation)+1);

	usleep(1);
	write(fd_tcp_proxy, file_names, strlen(file_names)+1);
}

void quit(){
	char confirmation[5]= "QUIT";
	write(fd_tcp_proxy, confirmation, 1 + strlen(confirmation));

	close(fd_tcp_proxy);
	exit(0);
}

int download_file(char* protocolo, char* codificacao,char* file){
	
	if (strcmp(protocolo, "TCP")!=0 && strcmp(protocolo, "UDP")!=0){
		write(fd_tcp_proxy, comando_erro_protocolo, strlen(comando_erro_protocolo)+1);
		return -1;
	}
	
	if(strcmp(codificacao, "ENC")!=0 && strcmp(codificacao, "NOR")!=0){
		write(fd_tcp_proxy, comando_erro_codificacao, strlen(comando_erro_codificacao)+1);
		return -1;
	}

	FILE *fp = fopen(file, "rb");
	if (fp==NULL){
		write(fd_tcp_proxy, comando_erro_ficheiro, strlen(comando_erro_ficheiro)+1);
		return -1;
	}

	write(fd_tcp_proxy, file, strlen(file)+1);

	int size;
	
	fseek(fp, 0, SEEK_END);
	size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	char buffer[size];
	
	if(strcmp(protocolo, "TCP")==0){
		if(strcmp(codificacao, "ENC")==0)size+= crypto_secretbox_MACBYTES;
		usleep(1);
		write(fd_tcp_proxy, &size, sizeof(size));
		
		fread(buffer, sizeof(buffer), 1, fp);

		if(strcmp(codificacao, "ENC")==0) {
			char ciphertext[size];
			unsigned char nonce[crypto_secretbox_NONCEBYTES];
			randombytes_buf(nonce, sizeof nonce);
			crypto_secretbox_easy(ciphertext, buffer, sizeof(buffer), nonce, k);

			usleep(1);
			write(fd_tcp_proxy, ciphertext, sizeof(ciphertext));
			usleep(1);
			write(fd_tcp_proxy, nonce, sizeof(nonce));
		}
		else{
			usleep(1);
			write(fd_tcp_proxy, buffer, sizeof(buffer));
		}
	}
	else{
		if(strcmp(codificacao, "ENC")==0)size+= crypto_secretbox_MACBYTES;
		usleep(1);
		sendto(fd_udp, &size, sizeof(size), 0, (struct sockaddr *)&proxy_addr_udp, sizeof(proxy_addr_udp));

		fread(buffer, sizeof(buffer), 1, fp);
	
		char ciphertext[size];
		if(strcmp(codificacao, "ENC")==0){ 
			char ciphertext[size];
			unsigned char nonce[crypto_secretbox_NONCEBYTES];
			randombytes_buf(nonce, sizeof nonce);
			crypto_secretbox_easy(ciphertext, buffer, sizeof(buffer), nonce, k);
		
			usleep(1);
			sendto(fd_udp, ciphertext, sizeof(ciphertext), 0, (struct sockaddr *)&proxy_addr_udp, sizeof(proxy_addr_udp));
			usleep(1);
			sendto(fd_udp, nonce, sizeof(nonce), 0, (struct sockaddr *)&proxy_addr_udp, sizeof(proxy_addr_udp));
		}
		else{
			usleep(1);
			sendto(fd_udp, buffer, sizeof(buffer), 0, (struct sockaddr *)&proxy_addr_udp, sizeof(proxy_addr_udp));
		}
	}
   
	fclose(fp);
	return 0;
}

void erro(char *msg){  
	printf("Erro: %s\n", msg);  
	exit(-1); 
} 


