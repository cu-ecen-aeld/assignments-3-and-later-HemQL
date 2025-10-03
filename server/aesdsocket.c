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
#include <pthread.h>
#include <sys/queue.h>
#include <time.h>
#define CHUNK_SIZE 256

int sfd, client_fd;
int fd;

char* rdfile;
int bufsize = 256;
int rdsize = 256;
int status;
int errsv;
pthread_t timestamps;
time_t lastStamp;
pthread_mutex_t fd_mut = PTHREAD_MUTEX_INITIALIZER;

// Define a structure for the list entries
struct thread_entry {
    pthread_t thread;
    int id;
    int cfd;
    int complete;
    SLIST_ENTRY(thread_entry) entries; // Singly-linked list pointer
};

// Declare the head of the list
SLIST_HEAD(slisthead, thread_entry) head;



void cleanup(int exit_code) {
    syslog(LOG_USER, "Cleaning up, prepare for exit %i.", exit_code);
    if (remove("/var/tmp/aesdsocketdata")== 0) {
        syslog(LOG_USER,"File deleted successfully.");
    } else {
        syslog(LOG_ERR,"Error: Unable to delete the file.");
    }
    //free(buf);
    free(rdfile);
    
    if (sfd != -1) {
        shutdown(sfd, SHUT_RDWR);
        close(sfd);
    }

    struct thread_entry *n1;
    while (!SLIST_EMPTY(&head)) {           /* List deletion */
        n1 = SLIST_FIRST(&head);
        if (n1->cfd != -1) {
            shutdown(n1->cfd, SHUT_RDWR);
        }
        SLIST_REMOVE_HEAD(&head, entries);
        free(n1);


    }
    pthread_mutex_destroy(&fd_mut);
    exit(exit_code);
    
}

void handle_signal(int sig, siginfo_t *info, void *context) {

    syslog(LOG_USER, "Caught signal %d, exiting", sig);
    int errsv = info->si_errno;
    context = context;
    cleanup(errsv == 0);
}

static void *writeTimestamp(){

    char outstr[33];
    time_t t;
    
    struct tm *tmp;
    //printf("%s", outstr);
    while (1){
        t = time(NULL);
        tmp = localtime(&t);
        if((long)t-(long)lastStamp>10){
        int len = strftime(outstr, sizeof(outstr), "%a, %d %b %Y %T %z\n", tmp);
            if (len == 0) {
                syslog(LOG_DEBUG, "strftime returned 0");
            }
            else {
                syslog(LOG_DEBUG, "Writing timestamp to file: %s", outstr);
                pthread_mutex_lock(&fd_mut);
                if(write(fd, outstr, len) < len){
                    syslog(LOG_DEBUG, "Write smaller than string");
                }
                pthread_mutex_unlock(&fd_mut);
                lastStamp = t;
                
            }

    	}
        else{
        	//syslog(LOG_DEBUG, "Too soon for timestamp.");
        	sleep(1);
        } 
        
    }
    pthread_exit(0);
}

static void *writeToFile(void * entry){

    
    ssize_t nr;
    char* buf;
    buf = malloc(bufsize);    
    int errsv;
    int rcvd;

    struct thread_entry *thread_info = (struct thread_entry *) entry;
    int cfd = thread_info->cfd;
    //int cfd = client_fd;
    syslog(LOG_DEBUG, "Thread %d using cfd = %d, and fd = %d", thread_info->id, thread_info->cfd, fd);

    rcvd = recv(cfd, buf, bufsize, MSG_WAITALL);
    if (rcvd <= 0) {
        errsv = errno;
        syslog(LOG_ERR, "recv failed or returned 0, %s", strerror(errsv));
    }

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
    pthread_mutex_lock(&fd_mut);
    nr = write(fd, (char *)buf, rcvd);

    //printf("Writing %s to file\n", buf);
  
    syslog(LOG_DEBUG, "Writing %i bytes to file", rcvd);
    if (nr == -1){
        errsv = errno;
        syslog(LOG_ERR,"Error writing data, %s", strerror(errsv));
        syslog(LOG_DEBUG,"fd = %i", (int)fd);
        cleanup(-1);
    }
    else if (nr != rcvd){
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
    
    int bytesRead = read(fd, rdfile, filesize);
    if (bytesRead == -1){
        errsv = errno;
        syslog(LOG_ERR,"Error reading file, %s\n", strerror(errsv));
        cleanup(-1);
    }
    syslog(LOG_DEBUG,"Read %i bytes from file", bytesRead);
    //pthread_mutex_unlock(&fd_mut);
    int sent = send(cfd, rdfile, filesize, 0);
            
    if (sent == -1){
        errsv = errno;
        syslog(LOG_ERR,"Error sending back file contents, %s\n", strerror(errsv));
        syslog(LOG_DEBUG,"cfd = %i\n", cfd);
        cleanup(-1);
    }
    
    syslog(LOG_USER,"Sent %i bytes from file", sent);
    pthread_mutex_unlock(&fd_mut);
    usleep(filesize*100);
    if (close(cfd) == -1){
        errsv = errno;
        syslog(LOG_ERR,"Thread %i: close error: %s", thread_info->id, strerror(errsv));
        cleanup(-1);
    }
    free(buf);
    thread_info->complete = 1;
    pthread_exit(0);    
}

int main(int argc, char **args){
    lastStamp = time(NULL);
    int count = 1;
    SLIST_INIT(&head);
    
    struct sigaction act = { 0 };
    act.sa_sigaction = &handle_signal;

    
    rdfile = malloc(rdsize);
        
    struct addrinfo hints;
    struct addrinfo *servinfo;
    
    syslog(LOG_USER, "Initializing aesdsocket server...");
    int i;
    if(argc > 1){
        for(i=0; i<argc; i++){
            fprintf(stdout, "%s ", (char *)args[i]);
        }
        int pid = fork();
        if (pid == -1){
            syslog(LOG_USER, "aesdsocket daemon failed to start.");
            return 0;
        }
        else if (pid !=0){
            syslog(LOG_USER, "aesdsocket daemon started, PID: %u", pid);
            return 0;
        }
        
    }
    
    //int root_pid = getpid();
    openlog(NULL,0,LOG_USER);
    
    
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
    struct sockaddr_storage client_addr;
    socklen_t clen = sizeof(client_addr);
    //socklen_t clen = servinfo->ai_addrlen;
     

    fd = open("/var/tmp/aesdsocketdata", O_RDWR | O_CREAT | O_APPEND, 0666);

    
    if (fd == -1){
	errsv = errno;
        syslog(LOG_ERR,"Error creating file, %s\n", strerror(errsv));
        return(-1);
    }
    
    pthread_create(&timestamps, NULL, writeTimestamp, NULL);
    
    
    while(1){
        if (sigaction(SIGTERM, &act, NULL) == -1) {
            perror("sigaction");
            exit(EXIT_FAILURE);
        }
        if (sigaction(SIGINT, &act, NULL) == -1) {
            perror("sigaction");
            exit(EXIT_FAILURE);
        }
        syslog(LOG_DEBUG, "Listening socket sfd = %d", sfd);

        int cfd = accept(sfd, (struct sockaddr *)&client_addr, &clen);
        if (cfd == -1) {
            errsv = errno;
            syslog(LOG_ERR,"accept error: %s\n", strerror(errsv));
            cleanup(-1);
        }
        syslog(LOG_DEBUG, "Connected socket cfd = %d", cfd);
        //writeTimestamp();  
        struct in_addr ipAddr = pV4Addr->sin_addr;
        char str[INET_ADDRSTRLEN];
        inet_ntop( AF_INET, &ipAddr, str, INET_ADDRSTRLEN );
        syslog(LOG_USER,"Accepted connection from %s", str);
        
        struct thread_entry *node1 = malloc(sizeof(struct thread_entry));

        
        node1->id = count;
        count++;  
        node1->complete = 0;
        node1->cfd = cfd;
        //client_fd = cfd;     
        //writeToFile((void*)node1);
        SLIST_INSERT_HEAD(&head, node1, entries); // Insert at the head
        syslog(LOG_DEBUG, "Passing cfd = %d to thread", node1->cfd);
        pthread_create(&node1->thread, NULL, writeToFile, node1);
        struct thread_entry *current;
        
        SLIST_FOREACH(current, &head, entries){
            if (current->complete == 1){
                //syslog(LOG_USER, "%i: write complete", current->id);
                //printf("%i: write complete, removing entry\n", current->id);
                pthread_join(current->thread, NULL);
                SLIST_REMOVE(&head, current, thread_entry, entries);
            }
   
        }
    }
    freeaddrinfo(servinfo); 
    cleanup(1);

}



    
    
