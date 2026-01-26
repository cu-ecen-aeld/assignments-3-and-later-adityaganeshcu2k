/*
* Date :- 1/23/26
* Author :- Aditya Ganesh
* Reference :- https://linux.die.net/man/3/syslog 
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <libgen.h>
#include <syslog.h>

#define MODE 0755


int main(int argc, char *argv[])
{ 

    /*
    * LOGS_PID - logging the process id
    * LOGS_CONS - prints to console if syslog fails
    * LOGS_USER - generic user level messages
    */
    openlog("writer", LOG_PID| LOG_CONS, LOG_USER);
    if(argc != 3)
    {
    	syslog(LOG_ERR, "ERROR:Invalid number of arguments");
    	closelog();
    	return 1;
    }
    
    char *writerfile = argv[1];
    char *writerstr = argv[2];
    
    
    syslog(LOG_DEBUG, "Writing %s to %s", writerstr, writerfile);
    
    FILE *fp = fopen(writerfile, "w");
    
    if(!fp)
    {
      syslog(LOG_ERR, "Failed to open file %s: %s", writerfile, strerror(errno));
      closelog();
      return 1;  
    }
    
    fprintf(fp, "%s",writerstr);
    fclose(fp);
    
    closelog();
    return 0;
}
