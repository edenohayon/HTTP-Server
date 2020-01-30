#include <sys/types.h> 
#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h> 
#include <time.h>
#include <fcntl.h>       
#include <dirent.h>
#include <sys/stat.h>
#include "threadpool.h"




 
#define MAXONSOCKET 5
/*#define PORT 1
#define POOL_SIZE 2
#define MAX_NUM_OF_REQ*/
#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"

void errorUsage();
void legalArgs(int argc, char const *argv[]);
int buildRes(void* p);
int checkInput(char* msg_client, int newsd);
void badRequest(int newsd);
int checkMethod(char* msg_client,int newsd);
void notSupported(int newsd);
int checkPath(char* msg_client, int newsd);
void notFound(int newsd);
int checkDirectory(int newsd, char* msg_client);
void found(int newsd, char* msg_client);
void dir_content(int newsd,char* path);
int digitsInNum(int num);
void appendIndexHtml(char* path , int newsd);
void Forbidden(int newsd);
int checkFile(int newsd,char* msg_client);
void file(int newsd, char* path);
void internalServerError(int newsd);
char *get_mime_type(char *name);

//argv[1] = port , argv[2] = poolSize, argv[3], = max num of req
int main(int argc, char const *argv[])
{
    legalArgs(argc,argv);

    threadpool* pool = create_threadpool(atoi(argv[2]));
    if(pool == NULL)
    {
        printf("error trying to allocate threadpool\n");
        exit(1);
    }

    int sockfd;
	struct sockaddr_in serv_addr;
	int* newsockfd = (int*)malloc(sizeof(int)*atoi(argv[3]));

	//server init
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0){
        perror("socket");
        exit(1);
    }
    
    printf("socket created\n");

	//init servaddress
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(atoi(argv[1]));

    memset(serv_addr.sin_zero, '\0', sizeof(serv_addr.sin_zero));


	if (bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("bind");
        exit(1);
    }

    printf("bined seccess\n");


	if(listen(sockfd, MAXONSOCKET) < 0 )
    {
        perror("listen");
        exit(1);
    }

    printf("listen seccess\n");

    socklen_t servLen = sizeof(serv_addr);

    for(int i = 0 ; i < atoi( argv[3] ) ; i++)
    {
        newsockfd[i] = accept(sockfd, (struct sockaddr*) &serv_addr, &servLen);
        if (newsockfd < 0)              
        {
            perror("accept");    
            continue;
        }
        printf("accepted - newsd = %d\n", newsockfd[i]);
        
        dispatch(pool,buildRes,(void*)&newsockfd[i]);
    }

    destroy_threadpool(pool);
    close(sockfd);  //or goto accept to wait for another clients
    free(newsockfd);
    return 0; 
}

void errorUsage()
{
    printf("Usage: server <port> <pool-size> <max-number-of-request>\n");
}
void legalArgs(int argc, char const *argv[])
{
    if(argc !=  4)
    {
        errorUsage();
        exit(1);
    }

    if(atoi(argv[1]) == 0|| atoi(argv[2]) == 0 || atoi(argv[3]) == 0)
    {
        errorUsage();
        exit(1);
    }
}

int buildRes(void* p)
{
    int newsd = *(int*)p;
    printf("newsd = %d\n",newsd);
    char msg_client[1024];
    int n;

    bzero(msg_client,1024);
    n = read(newsd, msg_client, 1023);  
    if (n < 0) 
    {
        perror("read");
        internalServerError(newsd);
        return 0;
    }
    //char * msg_client = "GET /chen/bla.txt HTTP/1.0\r\nHost: www.ynet.co.il\r\n\r\n";
    printf("read seccess:%s\n",msg_client);


    if(checkInput(msg_client,newsd) == -1)  
    {
        badRequest(newsd);
        return -1;
    }

    if(checkMethod(msg_client,newsd) == -1)
       {
           notSupported(newsd);
           return -1;
       }

    if(checkPath(msg_client,newsd) == -1)
    {
        notFound(newsd);
        return -1;
    }

    int check = checkDirectory(newsd,msg_client);

    if(check == -1)
    {
        found(newsd,msg_client);
        return -1;
    }

    if(check == 0)
        return 1;


    if(checkFile(newsd,msg_client) == -1)
    {
        Forbidden(newsd);
        return -1;
    }

  close(newsd);  
  return 0;


}


int checkInput(char* msg_client, int newsd)
{
    int i = 0;

    char* temp = (char*)malloc(sizeof(char)*(strlen(msg_client)+1));
    if(temp == NULL)
    {
        printf("error trying to allocate\n");
        internalServerError(newsd);
        return 1;
    }
    strcpy(temp,msg_client);

    char* pch = strtok (temp," ");
    while (pch != NULL)
    {
        i++;
        if(i == 3 && strstr(pch,"HTTP/1.1") != NULL)
        {
            free(temp);
            return 0;
        }
        if(i==3)
            break;

        pch = strtok (NULL, " ");
    }

   free(temp);
   return -1;
}

//400 Bad Request
void badRequest(int newsd)
{

    char* str1 = "HTTP/1.0 400 Bad Request\r\nServer: webserver/1.0\r\nDate: ";

    //get current time and date by this format -Date: Fri, 05 Nov 2010 13:50:33 GMT
    time_t now;
    char timebuf[128];
    now = time(NULL);
    strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));

    char* str2 = "\r\nContent-Type: text/html\r\nContent-Length: 113\r\nConnection: close\r\n\r\n";
    char* html = "<HTML><HEAD><TITLE>400 Bad Request</TITLE></HEAD>\r\n<BODY><H4>400 Bad request</H4>\r\nBad Request.\r\n</BODY></HTML>\r\n";

    char ret[1024];
    sprintf(ret, "%s%s%s%s", str1,timebuf,str2,html);
    int n = write(newsd,ret,strlen(ret) + 1);
    if (n < 0) 
	{
        perror("write");
        internalServerError(newsd);
        return;
    }
    printf("%s\n",ret);

}

int checkMethod(char* msg_client, int newsd)
{
    char* temp = (char*)malloc(sizeof(char)*(strlen(msg_client)+1));
    if(temp == NULL)
    {
        printf("error trying to allocate\n");
        internalServerError(newsd);
        return 1;
    }
    strcpy(temp,msg_client);

    char* pch = strtok (temp," ");
    if (pch != NULL && strcmp(pch,"GET") == 0)
    {
        free(temp);
        return 0;
    }
    free(temp);
    return -1;
}

//501 not supported
void notSupported(int newsd)
{
    char* str1 = "HTTP/1.0 501 Not supported\r\nServer: webserver/1.0\r\nDate: ";

    //get current time and date by this format -Date: Fri, 05 Nov 2010 13:50:33 GMT
    time_t now;
    char timebuf[128];
    now = time(NULL);
    strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));

    char* str2 = "\r\nContent-Type: text/html\r\nContent-Length: 129\r\nConnection: close\r\n\r\n";
    char* html = "<HTML><HEAD><TITLE>501 Not supported</TITLE></HEAD>\r\n<BODY><H4>501 Not supported</H4>\r\nMethod is not supported.</BODY></HTML>\r\n";

    char ret[1024];
    sprintf(ret, "%s%s%s%s", str1,timebuf,str2,html);
    int n = write(newsd,ret,strlen(ret) + 1);
    if (n < 0) 
	{
        perror("write");
        internalServerError(newsd);
        return;
    }
    printf("%s\n",ret);
}


//check if requested path exist. return 1 if exist and -1 otherwise
int checkPath(char* msg_client, int newsd)
{
    char* path =(char*)malloc(sizeof(char)*(strlen(msg_client)+1));
    if(path == NULL)
    {
        printf("error trying to allocate\n");
        internalServerError(newsd);
        return 1;
    }
    strcpy(path,msg_client);

    char *temp = strtok(path," ");
    temp = strtok(NULL," ");

    //remove first "/";
    temp++;

    //check if path exist
    struct stat sb;

    if (stat(temp, &sb) != 0 )
    {
        perror("stat");
        free(path);
        return -1;
    }

    free(path);
    return 1;

}


//404 Not Found
void notFound(int newsd)
{
    char* str1 = "HTTP/1.0 404 Not Found\r\nServer: webserver/1.0\r\nDate: ";

    //get current time and date by this format -Date: Fri, 05 Nov 2010 13:50:33 GMT
    time_t now;
    char timebuf[128];
    now = time(NULL);
    strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));

    char* str2 = "\r\nContent-Type: text/html\r\nContent-Length: 112\r\nConnection: close\r\n\r\n";
    char* html = "<HTML><HEAD><TITLE>404 Not Found</TITLE></HEAD>\r\n<BODY><H4>404 Not Found</H4>\r\nFile not found.\r\n</BODY></HTML>\r\n";

    char ret[1024];
    sprintf(ret, "%s%s%s%s", str1,timebuf,str2,html);
    int n = write(newsd,ret,strlen(ret) + 1);
    if (n < 0) 
	{
        perror("write");
        internalServerError(newsd);
        return;
    }
    printf("%s\n",ret);
}

/*check if path is directory and if it is - check if it ends with '/'.
**if its a directory and it doesnt ends with '/' return -1 else return 0
*/
int checkDirectory(int newsd, char* msg_client)
{
     char* temp =(char*)malloc(sizeof(char)*(strlen(msg_client)+1));
     if(temp == NULL)
     {
         printf("error trying to allocate\n");
         internalServerError(newsd);
         return 0;
     }
    strcpy(temp,msg_client);

    //get second token
    char *path = strtok(temp," ");
    path = strtok(NULL," ");

    //remove first "/";
    path++;
    printf("path is : %s\n",path);
    //check if path exist
    struct stat sb;

    //if its a dir
    if (stat(path, &sb) == 0 && S_ISDIR(sb.st_mode))
    {
        //and last char of path is not '/'
        if(path[strlen(path)-1] != '/')
        {
            free(temp);
            return -1;

        }
        else
        {
            struct dirent **namelist;
            int i,n;


            n = scandir(path, &namelist, 0, alphasort);
            if (n < 0){
                perror("scandir");
                internalServerError(newsd);
                free(temp);
                return 0;
            }
            else
            {
                for (i = 0; i < n; i++) {
                    if(strcmp(namelist[i]->d_name,"index.html") == 0)
                    {
                        appendIndexHtml(path,newsd);

                        free(temp);
                        for (int j = 0; j < n; j++)
                            free(namelist[j]);
                        free(namelist);

                        return 0;
                    }
                } 
            }

            dir_content(newsd,path); 

            for (int i = 0; i < n; i++)
                free(namelist[i]);
            free(temp);
            free(namelist);

            return 0;
        }
 
    }
    free(temp);
    return 1;
}

//302 Found response
void found(int newsd, char* msg_client)
{
    char* str1 = "HTTP/1.0 302 Found\r\nServer: webserver/1.0\r\nDate: ";

    //get current time and date by this format -Date: Fri, 05 Nov 2010 13:50:33 GMT
    time_t now;
    char timebuf[128];
    now = time(NULL);
    strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));

    //get path
    char* path =(char*)malloc(sizeof(char)*(strlen(msg_client)+1));
    if(path == NULL)
    {
        printf("error trying to allocate\n");
        internalServerError(newsd);
        return;
    }
    strcpy(path,msg_client);

    //get second token
    char *str2 = strtok(path," ");
    str2 = strtok(NULL," ");

    //remove first "/";
    str2++;

    char* str3 = "\r\nContent-Type: text/html\r\nContent-Length: 123\r\nConnection: close\r\n\r\n";
    char* html = "<HTML><HEAD><TITLE>302 Found</TITLE></HEAD>\r\n<BODY><H4>302 Found</H4>\r\nDirectories must end with a slash.\r\n</BODY></HTML>\r\n";

    char ret[1024];
    sprintf(ret, "%s%s\r\nLocation: %s/%s%s", str1,timebuf,str2,str3,html);
    int n = write(newsd,ret,strlen(ret) + 1);
    if (n < 0) 
	{
        perror("write");
        internalServerError(newsd);
        return;
    }
    printf("%s\n",ret);

    free(path);
}

void dir_content(int newsd,char* path)
{
    //construct html part
    char* s1 ="<HTML>\r\n<HEAD><TITLE>Index of ";
    char* s2 = "</TITLE></HEAD>\r\n<BODY>\r\n<H4>Index of ";
    char* s3 = "</H4>\r\n<table CELLSPACING=8>\r\n<tr><th>Name</th><th>Last Modified</th><th>Size</th></tr>\r\n";

    int size = strlen(s1) + strlen(s2) + strlen(s3) + strlen(path)*2 + 1;

    char* html = (char*)malloc(sizeof(char)*size);
    if(html == NULL)
    {
        printf("error trying to allocate\n");
        internalServerError(newsd);
        return;
    }

    sprintf(html,"%s%s%s%s%s",s1,path,s2,path,s3);

    struct dirent **namelist;
    int i,n;

    n = scandir(path, &namelist, 0, alphasort);
    if (n < 0){
        perror("scandir");
        internalServerError(newsd);
        free(html);
        return;
    }
    else {
        for (i = 0; i < n; i++) {

            char fullPath[500];
            sprintf(fullPath,"%s%s",path,namelist[i]->d_name);
        
            //check when file was last modified
            struct stat file_stat;
            if( stat(fullPath, &file_stat) != 0)
            {
                perror("stat");
                internalServerError(newsd);
                free(html);
                return;
            }
            time_t now;
            char date[128];
            now = time(NULL);
            strftime(date, sizeof(date), RFC1123FMT, gmtime(&now));
            char* s4 ="<tr><td><A HREF=";

            char* s5 = "</A></td><td>";

            char line[1024];
            //<tr><td><A HREF="<entity-name>"><entity-name (file or sub-directory)></A></td><td><modification time></td><td><if entity is a file, add file size, otherwise, leave empty></td></tr>
            sprintf(line,"%s\"%s\">%s%s%s</td><td>",s4,namelist[i]->d_name,namelist[i]->d_name,s5,date);

            //if file  - add filesize
            if(S_ISDIR(file_stat.st_mode) != 1)
            {
                char dirSize[12]; 
                sprintf(dirSize, "%d", (int)file_stat.st_size);
                strcat(line, dirSize);
                strcat(line, " Bytes");
            }
               /* int dirS = file_stat.st_size;
                int num = digitsInNum(dirS);

                char* dirSize = (char*)malloc(sizeof(char)*(num+1));
                if(dirSize == NULL)
                {
                    printf("error trying to allocate\n");
                    internalServerError(newsd);
                    free(html);
                    return;
                }

                sprintf(dirSize,"%d",dirS);
                strcat(line,dirSize);
                free(dirSize);
            }*/
            strcat(line,"</td></tr>\r\n");
            html = realloc(html,sizeof(char)*(strlen(html) + strlen(line) + 1 ));
            if(html == NULL)
           {
               printf("error trying to realloc\n");
               internalServerError(newsd);
               free(html);
               return;
           }
            strcat(html,line);
        }


        char* end = "</table>\r\n<HR>\r\n<ADDRESS>webserver/1.0</ADDRESS>\r\n</BODY></HTML>\r\n";
        html = realloc(html,sizeof(char)*(strlen(html) + strlen(end) + 1));
        if(html == NULL)
           {
               printf("error trying to realloc\n");
               internalServerError(newsd);
               free(html);
               return;
           }
        strcat(html,end);

       // printf("%s\n",html);

    }

    for (int i = 0; i < n; i++)
        free(namelist[i]);
        
    free(namelist);
    //-------------------end of html constrction---------------------------------------------------------

    // ---------------------build total response ----------------------------------------------------------
    char* str1 = "HTTP/1.0 200 OK\r\nServer: webserver/1.0\r\nDate: ";

    //get current time and date by this format -Date: Fri, 05 Nov 2010 13:50:33 GMT
    time_t now;
    char timebuf[128];
    now = time(NULL);
    strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));

    char* str2 = "\r\nContent-Type: text/html\r\nContent-Length: ";

    //contentlen num to char
    int contLen = digitsInNum(strlen(html));
    char* num = (char*)malloc(sizeof(char) * (contLen+1));
    if(num == NULL)
    {
        printf("error trying to allocate\n");
        internalServerError(newsd);
        free(html);
        return;
    }
    sprintf(num,"%ld",strlen(html));

    //response until content len
    char* res = (char*)malloc(sizeof(char)*( strlen(str1) + strlen(timebuf) + strlen(str2) + strlen(num) + 10));
    if(res == NULL)
    {
        printf("error trying to allocate\n");
        internalServerError(newsd);
        free(html);
        free(num);
        return;        
    }
    sprintf(res, "%s%s%s%s\r\n", str1,timebuf,str2,num);
    free(num);

    char* str3 = "Last-Modified: ";
    char* str4 = "\r\nConnection: close\r\n\r\n";

    //get last modify
    struct stat file_stat;
    if( stat(path, &file_stat) != 0)
    {
        perror("stat");
        internalServerError(newsd);
        free(html);
        free(res);
        return;
    }
    strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&file_stat.st_ctime));

    res = realloc(res, sizeof(char)*(strlen(res) + strlen(str3) + strlen(timebuf) + strlen(str4) + strlen(html) + 1));
    if(res == NULL)
    {
        printf("error trying to realloc\n");
        internalServerError(newsd);
        free(html);
        free(res);
        return;
    }

    strcat(res,str3);
    strcat(res,timebuf);
    strcat(res,str4);
    strcat(res,html);
    int t = write(newsd,res,strlen(res) + 1);
    if (t < 0) 
	{
        perror("write");
        internalServerError(newsd);
        return;
    }
    printf("%s\n",res);

    free(res);
    free(html);    
}

int digitsInNum(int num)
{
    int count = 0;
    while(num != 0)
    {
        count++;
        num /=10;
    }
    return count;
}

void appendIndexHtml(char* path , int newsd)
{
    char* str1 = "HTTP/1.0 200 OK\r\nServer: webserver/1.0\r\nDate: ";

    //get current time and date by this format -Date: Fri, 05 Nov 2010 13:50:33 GMT
    time_t now;
    char timebuf[128];
    now = time(NULL);
    strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));

    char* str2 = "\r\nContent-Type: text/html\r\nContent-Length: ";


    char* buffer = 0;
    long length;
    char fullPath[500];
    sprintf(fullPath,"%s%s",path,"index.html");

    FILE * f = fopen (fullPath, "r");
    if(f == NULL)
    {
        perror("fopen:");
        internalServerError(newsd);
        return;
    }

    fseek (f, 0, SEEK_END);
    length = ftell (f);
    fseek (f, 0, SEEK_SET);
    buffer = (char*)calloc(sizeof(char)*(length+1), sizeof(char));
    if(buffer == NULL)
    {
        printf("error trying to allocate\n");
        internalServerError(newsd);
        fclose(f);
        return;
    }

    if (buffer)
       {
          fread (buffer, 1, length, f);
    }
    fclose (f);


    //contentlen num to char
    int contLen = digitsInNum(length);
    char* num = (char*)malloc(sizeof(char) * (contLen + 1));
    if(num == NULL)
    {
        printf("error tryig to allocate\n");
        internalServerError(newsd);
        free(buffer);
        return;
    }
    sprintf(num,"%ld",length);
    
    //get last modify
    struct stat file_stat;
    if( stat(path, &file_stat) != 0)
    {
        perror("stat");
        internalServerError(newsd);
        return;
    }
    char modbuf[128];
    strftime(modbuf, sizeof(timebuf), RFC1123FMT, gmtime(&file_stat.st_ctime));

   

    char* str3 = "\r\nLast-Modified: ";
    char* str4 = "\r\nConnection: close\r\n\r\n";

    int size = strlen(str1) + strlen(timebuf) + strlen(str2) + strlen(num) + strlen(str3) + strlen(modbuf) + strlen(str4) + strlen(buffer) + 1;

    char* res = (char*)malloc(sizeof(char)* size);
    if(res == NULL)
    {
        printf("error trying to realloc\n");
        internalServerError(newsd);
        free(num);
        free(buffer);
        return;
    }

    sprintf(res,"%s%s%s%s%s%s%s%s",str1,timebuf,str2,num,str3,modbuf,str4,buffer);
    int n = write(newsd,res,strlen(res) + 1);
    if (n < 0) 
	{
        perror("write");
        internalServerError(newsd);
        return;
    }
    printf("%s\n",res);

    free(num);
    free(buffer);
    free(res);
}

void Forbidden(int newsd)
{
    char* str1 = "HTTP/1.0 403 Forbidden\r\nServer: webserver/1.0\r\nDate: ";

    //get current time and date by this format -Date: Fri, 05 Nov 2010 13:50:33 GMT
    time_t now;
    char timebuf[128];
    now = time(NULL);
    strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));

    char* str2 = "\r\nContent-Type: text/html\r\nContent-Length: 111\r\nConnection: close\r\n\r\n<HTML><HEAD><TITLE>403 Forbidden</TITLE></HEAD>\r\n<BODY><H4>403 Forbidden</H4>\r\nAccess denied.\r\n</BODY></HTML>\r\n";

    char ret[1024];
    sprintf(ret, "%s%s%s", str1,timebuf,str2);
    int n = write(newsd,ret,strlen(ret) + 1);
    if (n < 0) 
	{
        perror("write");
        internalServerError(newsd);
        return;
    }
    printf("%s\n",ret);
}

int checkFile(int newsd,char* msg_client)
{
    char* temp =(char*)malloc(sizeof(char)*(strlen(msg_client)+1));
    if(temp == NULL)
    {
        printf("error tryint to allocate\n");
        internalServerError(newsd);
        return 1;
    }
    strcpy(temp,msg_client);

    //get second token
    char *path = strtok(temp," ");
    path = strtok(NULL," ");

    //remove first "/";
    path++;
    struct stat path_stat;
    stat(path, &path_stat);

    //if file is not regular
    if( !S_ISREG(path_stat.st_mode) )
    {
        free(temp);
        return -1;
    }
    
    if(stat(path,&path_stat)  == -1)
    {
        perror("stat");
        internalServerError(newsd);
        free(temp);
        return 1;
    } 
    mode_t perm = path_stat.st_mode;
    if(( perm & S_IROTH )&& (perm & S_IXOTH))
        file(newsd,path);
    else{
        free(temp);
        return -1;
    }
        

    free(temp);
    return 0;
  
}

char *get_mime_type(char *name)
{
    char *ext = strrchr(name, '.');
    if (!ext) return NULL;
    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) return "text/html";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".gif") == 0) return "image/gif";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".css") == 0) return "text/css";
    if (strcmp(ext, ".au") == 0) return "audio/basic";
    if (strcmp(ext, ".wav") == 0) return "audio/wav";
    if (strcmp(ext, ".avi") == 0) return "video/x-msvideo";
    if (strcmp(ext, ".mpeg") == 0 || strcmp(ext, ".mpg") == 0) return "video/mpeg";
    if (strcmp(ext, ".mp3") == 0) return "audio/mpeg";
    return NULL;
}

void file(int newsd, char* path)
{
    char* str1 = "HTTP/1.0 200 OK\r\nServer: webserver/1.0\r\nDate: ";

    //get current time and date by this format -Date: Fri, 05 Nov 2010 13:50:33 GMT
    time_t now;
    char timebuf[128];
    now = time(NULL);
    strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));

    char* str2 = "\r\nContent-Type: ";
    char* type = get_mime_type(path);
    char* str3 = "\r\nContent-Length: ";
    char* str4 = "\r\nLast-Modified: ";

    char date[128];
    //get last modify
    struct stat file_stat;
    if( stat(path, &file_stat) != 0)
    {
        perror("stat");
        internalServerError(newsd);
        return;
    }
    strftime(date, sizeof(date), RFC1123FMT, gmtime(&file_stat.st_ctime));

    char* str5 = "\r\nConnection: close\r\n\r\n";

    unsigned char* buffer;
    unsigned long length;

    FILE * f = fopen (path, "r");
    if(f == NULL)
    {
        perror("fopen:");
        internalServerError(newsd);
        return;
    }

    fseek (f, 0, SEEK_END);
    length = ftell (f);
    fseek (f, 0, SEEK_SET);
    buffer = (unsigned char*)calloc(sizeof(unsigned char)*length  , sizeof(unsigned char));
    if(buffer == NULL)
    {
        printf("error trying to calloc\n");
        internalServerError(newsd);
        return;
    }

    if (buffer)
    {
          fread (buffer, 1, length, f);
    }
    fclose (f);
    

    int size = strlen(str1) + strlen(timebuf) + strlen(str3) + digitsInNum(length) + strlen(str4) + strlen(str5) + length + 1;
    if(type != NULL)
        size += strlen(type) + strlen(str2);

    char* res = (char*)calloc(sizeof(char)*size,sizeof(char));
    if(res == NULL)
    {
        printf("error trying to calloc\n");
        internalServerError(newsd);
        free(buffer);
        return;
    }
    if(type != NULL)
        sprintf(res,"%s%s%s%s%s%ld%s%s%s",str1,timebuf,str2,type,str3,length,str4,date,str5);
    else
        sprintf(res,"%s%s%s%ld%s%s%s",str1,timebuf,str3,length,str4,date,str5);


    int n = write(newsd,res,strlen(res) );
    if (n < 0) 
	{
        perror("write");
        internalServerError(newsd);
        return;
    }

    n = write(newsd,buffer,length );
    if (n < 0) 
	{
        perror("write");
        internalServerError(newsd);
        return;
    }
    printf("%s\n",res);

    free(buffer);
    free(res);
}

void internalServerError(int newsd)
{
     char* str1 = "HTTP/1.0 500 Internal Server Error\r\nServer: webserver/1.0\r\nDate: ";

    //get current time and date by this format -Date: Fri, 05 Nov 2010 13:50:33 GMT
    time_t now;
    char timebuf[128];
    now = time(NULL);
    strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));

    char* str2 = "\r\nContent-Type: text/html\r\nContent-Length: 144\r\nConnection: close\r\n\r\n";
    char* html = "<HTML><HEAD><TITLE>500 Internal Server Error</TITLE></HEAD>\r\n<BODY><H4>500 Internal Server Error</H4>\r\nSome server side error.\r\n</BODY></HTML>\r\n";

    int size = strlen(str1) + strlen(timebuf) + strlen(str2) + strlen(html) + 1 ;

    char* ret = (char*)calloc(sizeof(char)*size,sizeof(char));

    sprintf(ret, "%s%s%s%s", str1,timebuf,str2,html);
    int n = write(newsd,ret,strlen(ret) + 1);
    if (n < 0) 
	{
        perror("write");
        return;
    }
    printf("%s\n",ret);

}