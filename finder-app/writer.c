#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>

int main(int argc, char **argv){
    int errsv;
    openlog(NULL,0,LOG_USER);
//    fprintf(stdout, "argc = %d\n", argc);
    if (argc < 2) // no arguments were passed
    {
//        fprintf(stderr,"Invalid Number of arguments: %d",argc);
        syslog(LOG_ERR,"Invalid Number of arguments: %d",argc);
        return(1);
    }


    int fd;
    fd = open(argv[1], O_WRONLY | O_CREAT | O_EXCL, 0666);
    if (fd == -1){
	errsv = errno;
//        fprintf(stderr,"Error creating file, %s\n", strerror(errsv));
        syslog(LOG_ERR,"Error creating file, %s\n", strerror(errsv));
        return(1);
    }
    
    const char *word = argv[2];
    size_t count;
    ssize_t nr;
    count = strlen(word);
    nr = write(fd, word, count);

//    fprintf(stdout, "Print %lu bytes of %lu.\n%s\n", nr, count, word);
    syslog(LOG_DEBUG, "Writing %s to %s", word, argv[1]);
    if (nr == -1){
    /* error, check errno */
    	errsv = errno;
        //fprintf(stderr,"Error writing data, %s\n", strerror(errsv));
        syslog(LOG_ERR,"Error writing data, %s\n", strerror(errsv));
        return(1);
    }
    else if (nr != count){
        //fprintf(stderr,"Write size does not match size of string to write");
        syslog(LOG_ERR,"Write size does not match size of string to write");
        
    }    
    nr = write(fd, "\n", 1); 
    return(0);
}


/*
    Accepts the following arguments: the first argument is a full path to a file (including filename) on the filesystem, referred to below as writefile; the second argument is a text string which will be written within this file, referred to below as writestr

    Exits with value 1 error and print statements if any of the arguments above were not specified

    Creates a new file with name and path writefile with content writestr, overwriting any existing file and creating the path if it doesn’t exist. Exits with value 1 and error print statement if the file could not be created.
    
    One difference from the write.sh instructions in Assignment 1:  You do not need to make your "writer" utility create directories which do not exist.  You can assume the directory is created by the caller.

    Setup syslog logging for your utility using the LOG_USER facility.

    Use the syslog capability to write a message “Writing <string> to <file>” where <string> is the text string written to file (second argument) and <file> is the file created by the script.  This should be written with LOG_DEBUG level.

    Use the syslog capability to log any unexpected errors with LOG_ERR level.
*/
