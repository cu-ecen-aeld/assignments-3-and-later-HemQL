#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <signal.h>
#include <arpa/inet.h>
#define CHUNK_SIZE 256

int sfd, cfd;
int fd;
char* buf;
char* rdfile;



void cleanup(int exit_code) {
    syslog(LOG_USER, "Cleaning up, prepare for exit %i.", exit_code);
    if (remove("/var/tmp/aesdsocketdata")== 0) {
        syslog(LOG_USER,"File deleted successfully.");
    } else {
        syslog(LOG_ERR,"Error: Unable to delete the file.");
    }
    free(buf);
    free(rdfile);
    if (cfd != -1) {
        shutdown(cfd, SHUT_RDWR);
    }
    if (sfd != -1) {
        shutdown(sfd, SHUT_RDWR);
    }

    exit(exit_code);
    
}

void handle_signal(int sig, siginfo_t *info, void *context) {

    syslog(LOG_USER, "Caught signal %d, exiting", sig);
    int errsv = info->si_errno;
    context = context;
    cleanup(errsv == 0);
}


int main(int argc, char **args){

    struct sigaction act = { 0 };
    act.sa_sigaction = &handle_signal;

    int errsv;
    int rcvd;
    int bufsize = 256;
    int rdsize = 256;
    buf = malloc(bufsize);
    rdfile = malloc(rdsize);
    
    syslog(LOG_USER, "Initializing aesdsocket server...");
    int i;
    if(argc > 1){
        for(i=0; i<argc; i++){
            fprintf(stdout, "%s ", (char *)args[i]);
        }
        int pid = fork();
 
        if (pid !=0){
            syslog(LOG_USER, "aesdsocket daemon started, PID: %u", pid);
            return 0;
        }
        
    }
    
    openlog(NULL,0,LOG_USER);
    
    fd = open("/var/tmp/aesdsocketdata", O_RDWR | O_CREAT, 0666);
    if (fd == -1){
	errsv = errno;
//        fprintf(stderr,"Error creating file, %s\n", strerror(errsv));
        syslog(LOG_ERR,"Error creating file, %s\n", strerror(errsv));
        return(-1);
    }
    int status;
    
    ssize_t nr;
    struct addrinfo hints;
    struct addrinfo *servinfo;
    
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    

    
    if ((status = getaddrinfo(NULL, "9000", &hints, &servinfo)) !=0) {
        syslog(LOG_ERR,"getaddrinfo error: %s\n", gai_strerror(status));
        cleanup(-1);
    }
     
    sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0){
        errsv = errno;
        syslog(LOG_ERR,"setsockopt error: %s\n", strerror(errsv));
        cleanup(-1);
    }   
    if (bind(sfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
        errsv = errno;
        syslog(LOG_ERR,"bind error: %s\n", strerror(errsv));
        cleanup(-1);
    }    
    
    if(listen(sfd, 3) == -1) {
        errsv = errno;
        syslog(LOG_ERR,"listen error: %s\n", strerror(errsv));
        cleanup(-1);
    }

    struct sockaddr *c_addr = servinfo->ai_addr;
    struct sockaddr_in* pV4Addr = (struct sockaddr_in*)&c_addr;
    socklen_t clen = servinfo->ai_addrlen;
    freeaddrinfo(servinfo);  
    
    while(1){        
        if (sigaction(SIGTERM, &act, NULL) == -1) {
               perror("sigaction");
               exit(EXIT_FAILURE);
        }
        if (sigaction(SIGINT, &act, NULL) == -1) {
               perror("sigaction");
               exit(EXIT_FAILURE);
        }
        
        cfd = accept(sfd, c_addr, &clen);
        if (cfd == -1) {
            errsv = errno;
            syslog(LOG_ERR,"accept error: %s\n", strerror(errsv));
            cleanup(-1);
        }
        
        
        
    
        /* Code to deal with incoming connection(s)... */
        
        struct in_addr ipAddr = pV4Addr->sin_addr;

        char str[INET_ADDRSTRLEN];
        inet_ntop( AF_INET, &ipAddr, str, INET_ADDRSTRLEN );
        syslog(LOG_USER,"Accepted connection from %s", str);
    
        rcvd = recv(cfd, buf, bufsize, 0);
        while (rcvd == bufsize) {
            buf = realloc(buf, bufsize + CHUNK_SIZE);
            if (!buf) {
                syslog(LOG_ERR,"Unable to realloc CHUNK_SIZE more to %i", bufsize);
                cleanup(1);
            }
            ssize_t rcvd_more = recv(cfd, buf+bufsize, CHUNK_SIZE, 0);
            rcvd = rcvd + rcvd_more;
            bufsize = bufsize+CHUNK_SIZE;
        }
        

        
        nr = write(fd, (char *)buf, rcvd);
  
        syslog(LOG_DEBUG, "Writing %s to file, total %i bytes", (char *)buf, rcvd);
        if (nr == -1){
    /* error, check errno */
    	errsv = errno;
    //fprintf(stderr,"Error writing data, %s\n", strerror(errsv));
            syslog(LOG_ERR,"Error writing data, %s", strerror(errsv));
            cleanup(-1);
        }
        else if (nr != rcvd){
    //fprintf(stderr,"Write size does not match size of string to write");
            syslog(LOG_ERR,"Write size does not match size of string to write");
    
        }
        
        int filesize = (int)lseek(fd,0,SEEK_END); 
        lseek(fd,0,SEEK_SET);    
        
        if (filesize>rdsize){
            free(rdfile);
            rdfile = malloc(filesize);
            if (rdfile == NULL){
                syslog(LOG_ERR,"malloc failed to allocate %i bytes.",filesize);
                cleanup(1);
                
            }
        }

        if (read(fd, rdfile, filesize) == -1){
            errsv = errno;
            syslog(LOG_ERR,"Error reading file, %s\n", strerror(errsv));
            cleanup(-1);
        }
        int sent = send(cfd, rdfile, filesize, 0);
            
        if (sent == -1){
            errsv = errno;
            syslog(LOG_ERR,"Error sending data, %s\n", strerror(errsv));
            cleanup(-1);
        }
        syslog(LOG_USER,"Sent %i bytes from file", sent);
        usleep(filesize*100);
    

        if (close(cfd) == -1){
            syslog(LOG_ERR,"close error: %s", strerror(status));
            cleanup(-1);
        }
        
    }
    
    cleanup(1);

}


    
    
