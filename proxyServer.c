#include "threadpool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <ctype.h>


#define TRUE 1
#define FALSE 0
#define MAX_SIZE 1024

#define PORT_BOOL 0
#define PORT_PORT 1
#define PORT_INDEX 2
#define PORT_SIZE 3

#define HTTP1_0 1
#define HTTP1_1 2

typedef struct blockedIP{
    char ip[MAX_SIZE];
    int IPcount;
    struct blockedIP *next;
}blockedIP;

typedef struct blockedURL{
    char url[MAX_SIZE];
    int URLcount;
    struct blockedURL *next;
}blockedURL;

typedef struct Errors{
    int errorNumber;
    char *error;
    char *errorDescription;
    int errorCount;
    struct Errors *nextError;
}Errors;

int countFolders(char *URL);
char** urlDivider(char *URL, int subFolders);
int findFile(char* URL);
void findPort(int returnValue[],char *URL);
void createSubDirectory(char *subURL);
void makeDirectory(char **dividedURL,int count);
int localHeader(int contentLength,char *URL);
void HTMLrequest(char **dividedURL,int port,int SubLinks, char* fullURL,int subFolderCount,int htmlVer,Errors *errorHead,blockedIP *IPhead,blockedURL *URLhead);
int check200(char*str, int stat);
char *fullPath(char **dividedURL,int size, int subFolders);
char *stringCut(char *str, int index);
int checkHTTP(char* str);

int isDigit(char checkDigit);
void addIP(char *ip);
void addURL(char *url);
int isBlocked(char *chk);
void importFile(char *path);
void destroyLists();
char *errorHandler(int errorNumber,char* url);
char *get_mime_type(char *name);
void addError(int errorNumber,char *error,char *errorDescription);
Errors *findError(int errorNumber);
void initErrors();
char *convertGet(char *Get);
int driver(void *socket);
int findVer(char *Get);
int check(char* mainString);

void printIP(blockedIP *head);
void printURL(blockedURL *head);
void printErrors(Errors *head);
int printTPool(void* i);

blockedIP *IPhead=NULL;
blockedURL *URLhead=NULL;
Errors *errorHead=NULL;

int main(int argc, char* argv[]) {
if(argc!=5){
    printf("proxyServer <port> <pool-size> <max-number-of-request> <filter>\n");
    exit(EXIT_FAILURE);
}

int port=atoi(argv[1]);
int poolSize= atoi(argv[2]);
int maxRequest= atoi(argv[3]);
    importFile(argv[4]);
    initErrors();
    struct sockaddr_in server;
    int fd;
    if ((fd=socket(AF_INET,SOCK_STREAM, 0)) < 0) {
        perror("error: socket\n");
        exit(1);
    }
    server.sin_family=AF_INET;
    server.sin_addr.s_addr=htonl(INADDR_ANY);
    server.sin_port= htons(port);

    if(bind(fd,(struct sockaddr*)&server,sizeof (server))<0){
        perror("error: bind\n");
        exit(1);
    }
threadpool *t=create_threadpool(5);
    int *httpReq=(int*)(malloc(sizeof (int)*maxRequest));
    if(httpReq==NULL){
        free(httpReq);
        perror("error: malloc\n");
    }
    listen(fd,5);
    int i;
    for(i=0;i<maxRequest;i++){
        int clientSocket= accept(fd,NULL,NULL);
        httpReq[i]=clientSocket;
        dispatch(t,driver,&httpReq[i]);
    }
    close(fd);
    free(httpReq);
    destroyLists();
    destroy_threadpool(t);
    pthread_exit(NULL);
    return 0;
}

/**
 * coverts a get request to a normal URL address
 * @param Get
 * @return URL address
 */
char *convertGet(char *Get){
    char getLC[1024];
    int k;
    for (k = 0; k < strlen(Get) ; k++) {
        getLC[k]= tolower(Get[k]);
    }
    getLC[k+1]='\0';
        char path[1024] = "";
        int i;
        char c = getLC[2];
        if (getLC[3] != ' ') {
            i = 3;
        } else
            i = 4;
        while (c != ' ') {
            c = getLC[i];
            strncat(path, &c, 1);
            i++;
        }
        char *host = strstr(getLC, "host:");
        host[strlen(host) - 4] = '\0';
        c = host[4];
        if (host[5] == ' ') {
            i = 6;
        } else
            i = 5;
        char mainHost[1024] = "";

        while (i < strlen(host)) {
            c = host[i];
            strncat((char *) mainHost, &c, 1);
            i++;
        }
        char URL[1024] = "http://";
        strcat(URL, mainHost);
        strcat(URL, path);
        URL[strlen(URL) - 1] = '\0';
        char *rtrnURL = (char *) (malloc(sizeof(char) * strlen(URL)));
        strcpy(rtrnURL, URL);
        return rtrnURL;
}
/**
 * checks for http version
 * @param Get
 * @return HTTP1_0 if http/1.0, HTTP1_1 if http1.1
 */
int findVer(char *Get){
    char getLC[1024];
    int j;
    for (j = 0; j < strlen(Get) ; j++) {
        getLC[j]= tolower(Get[j]);
    }
    getLC[j+1]='\0';
    char *res=strstr(getLC,"http/1.0");
    if(res==NULL){
        return HTTP1_1;
    }
    else
        return HTTP1_0;
}

/**
 * Takes a socket descriptor number and handels that client
 * @param socket
 * @return
 */
int driver(void *socket){
    int sd=*(int*)socket;
    if(sd<0){
        errorHandler(500,"index.html");
        return -1;
    }
    char BUF[4000]="";
    char GET[4000]="";
    int rd;
    while((rd= read(sd,BUF,4000))!=0){
        strcat(GET,BUF);
        char *checkEnd= strstr(GET,"\r\n");
        if(checkEnd!=NULL){
            break;
        }
    }
    if(rd<0){
        perror("error: read\n");
        errorHandler(500,"index.html");
        return -1;
    }
    int chk=check(GET);
    if(chk==-1){
        return-1;
    }
    char *URL= convertGet(GET);
    int httpVer= findVer(GET);
    char tmp[strlen(URL)+1];
    char * tmp1;
    strcpy(tmp,URL);
    if(tmp[strlen(tmp)-1]=='/'){
        strcat(tmp,"index.html");
    }
    int check=checkHTTP(tmp);
    if(check==TRUE){
        tmp1 = strstr(tmp, "http://")+7;
    }
    int subFolderCount= countFolders(tmp1);
    if(subFolderCount==0){//index.html
        strcat(tmp,"/index.html");
        subFolderCount= countFolders(tmp1);
    }
    int len=strlen(tmp1);
    char **dividedURL = urlDivider(tmp1, subFolderCount);//divide into arrays.
    int portReturn[4]={-1,-1,-1,-1};
    findPort(portReturn,dividedURL[0]);
    int port=portReturn[PORT_PORT];
    if(portReturn[PORT_BOOL]==TRUE){
        dividedURL[0]= stringCut(dividedURL[0],portReturn[PORT_INDEX]-1);
    }
    len =len-portReturn[PORT_SIZE];
    char *fullURL= fullPath(dividedURL,len,subFolderCount+1);

    int find=findFile(fullURL);//find if file exists

    if(find==TRUE){//if file found.
        FILE *fp=fopen(fullURL,"r");
        struct stat st;
        fstat(fileno(fp),&st);
        int fileSize=(int)st.st_size;
        if (fp==NULL){
            perror("error: fopen\n");
            exit(1);
        }
        int headerSize=localHeader(fileSize,fullURL);
        int numOfBites;
        unsigned char sentence[1024]="";
        while((numOfBites=(int)read(fileno(fp),sentence,1024))!=0){
            write(1,sentence,numOfBites);
        }
        char buff[1024];
        fscanf(fp, "%s", buff);
        printf("\nTotal response bytes: %d\n",fileSize+headerSize);
    }
    else {//if file is not found
        HTMLrequest(dividedURL,port,subFolderCount,fullURL,subFolderCount,httpVer,errorHead,IPhead,URLhead);
    }
    free(fullURL);
    int i;
    for (i=0;i<subFolderCount+1;i++){
        free(dividedURL[i]);
    }

    free(dividedURL);
    free(URL);
    close(sd);
    return 0;
}

/**
 * checks if the character is a digit or a text
 * @param checkDigit
 * @return TRUE if digit, FALSE otherwise
 */
int isDigit(char checkDigit){
    if(checkDigit>47&&checkDigit<58){
        return TRUE;
    }
    else
        return FALSE;
}
/**
 * adds ip to blocked ip list struct
 * @param ip
 */
void addIP(char *ip){
    blockedIP **head=&IPhead;
    blockedIP *newNode= (blockedIP*)(malloc(sizeof (blockedIP)));
    strcpy(newNode->ip,ip);
    newNode->next=NULL;
    if(*head==NULL){
        newNode->IPcount=1;
        *head=newNode;
    } else{
        blockedIP *curr=*head;
        curr->IPcount++;
        while (curr->next!=NULL){
            curr=curr->next;
        }
        curr->next=newNode;
    }
}
/** adds url to blocked url list struct
 * @param url
 */
void addURL(char *url){
    blockedURL **head=&URLhead;
    blockedURL *newNode= (blockedURL*)(malloc(sizeof (blockedURL)));
    strcpy(newNode->url,url);
    newNode->next=NULL;
    if(*head==NULL){
        newNode->URLcount=1;
        *head=newNode;
    } else{
        blockedURL *curr=*head;
        curr->URLcount++;
        while (curr->next!=NULL){
            curr=curr->next;
        }
        curr->next=newNode;
    }
}
/**
 * checks if input is blocked on ether ip or url level
 * @param chk
 * @return TRUE if blocked, FALSE otherwise
 */
int isBlocked(char *chk){
    if(isDigit(chk[0])){
        blockedIP *curr=IPhead;
        while (curr!=NULL){
            if (strcmp(curr->ip,chk)==0){
                return TRUE;
            }
            curr=curr->next;
        }
        return FALSE;
    }
    else{
        blockedURL *curr=URLhead;
        while (curr!=NULL){
            if (strcmp(curr->url,chk)==0){
                return TRUE;
            }
            curr=curr->next;
        }
        return FALSE;
    }
}
/**
 * imports the filter file and adds the blocked items to a struct.
 * @param path
 */
void importFile(char *path){
    FILE *fp =fopen(path,"r");
    if(fp==0){
        perror("error: fopen\n");
        exit(EXIT_FAILURE);
    }

    char buffer[MAX_SIZE];
    while(fscanf(fp,"%s",buffer)!=EOF){
        if(isDigit(buffer[0])){
            addIP(buffer);
        }
        else{
            addURL(buffer);
        }
    }
    fclose(fp);
}
/**
 * destroies blocked lists and error structs.
 */
void destroyLists(){
    blockedIP *currIP;
    blockedURL *currURL;
    Errors *currError;
    while(IPhead!=NULL){
        currIP=IPhead;
        IPhead=IPhead->next;
        free(currIP);
    }
    while(URLhead!=NULL){
        currURL=URLhead;
        URLhead=URLhead->next;
        free(currURL);
    }
    while(errorHead!=NULL){
        currError=errorHead;
        errorHead=errorHead->nextError;
        free(currError);
    }

}
/**
 * adds an error to the error Struct list.
 * @param errorNumber
 * @param error
 * @param errorDescription
 */
void addError(int errorNumber,char *error,char *errorDescription){
    Errors **head=&errorHead;
    Errors *newNode= (Errors*)(malloc(sizeof (Errors)));
    newNode->errorNumber=errorNumber;
    newNode->error=error;
    newNode->errorDescription=errorDescription;
    newNode->nextError=NULL;

    if(*head==NULL){
        newNode->errorCount=1;
        *head=newNode;
    } else{
        Errors *curr=*head;
        curr->errorCount++;
        while (curr->nextError!=NULL){
            curr=curr->nextError;
        }
        curr->nextError=newNode;
    }
}
/**
 * searches for error in struct based on the error number
 * @param errorNumber
 * @return
 */
Errors *findError(int errorNumber){
    Errors *head=errorHead;
    Errors *curr=head;
    int count=curr->errorCount;
    int i;
    for(i=0;i<count;i++){
        if (curr->errorNumber==errorNumber){
            return curr;
        }
        curr=curr->nextError;
    }
    return NULL;
}
/**
 * returns a structured error depending on the error number.
 * @param errorNumber
 * @param url
 * @return
 */
char *errorHandler(int errorNumber,char* url){
    Errors *head=errorHead;
    Errors *errorNode= findError(errorNumber);
    char *error=errorNode->error;
    char *errorType=errorNode->errorDescription;
    char *header=(char*)(malloc(sizeof (char) * 1024));
    char *HTML=(char*)(malloc(sizeof (char) * 1024));
    strcpy(header, "HTTP/1.0 ");
    strcat(header, error);
    strcat(header, "\nContent-Type: ");
    strcat(header, get_mime_type(url));

    strcat(header, "\nContent-Length: ");
    strcpy(HTML,"<HTML><HEAD><TITLE>");

    strcat(HTML,error);
    strcat(HTML,"</TITLE></HEAD>\n");
    strcat(HTML,"<BODY><H4>");
    strcat(HTML,error);

    strcat(HTML,"</H4>\n");
    strcat(HTML,errorType);
    strcat(HTML,"\n</BODY></HTML>");
    char len[20];
    sprintf(len,"%d", (int)strlen(HTML));
    strcat(header,len);
    strcat(header, "\nconnection: Close\n");
    char *retrunString=(char*)(malloc(sizeof (char)*1024));
    strcpy(retrunString,header);
    strcat(retrunString,"\r\n");
    strcat(retrunString,HTML);
    free(errorNode);
    return retrunString;
}
/**
 * initializes errors in a struct.
 */
void initErrors(){
    addError(400,"400 Bad Request","Bad Request.");
    addError(403,"403 Forbidden","Access denied.");
    addError(404,"404 Not Found","File not found.");
    addError(500,"500 Internal Server Error","Some server side error.");
    addError(501,"501 Not supported","Method is not supported.");
}

/**
 * returns type based on url
 * @param name
 * @return
 */
char *get_mime_type(char *name) {
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
/**
 * checks the get request structure and prints an error if necessary
 * @param mainString
 * @return
 */
int check(char* mainString){

    char getLC[1024];
    int k;
    for (k = 0; k < strlen(mainString) ; k++) {
        getLC[k]= tolower(mainString[k]);
    }
    getLC[k+1]='\0';
    char *chkPath= strstr(getLC,"/");
    if(chkPath==NULL){
        printf("%s",errorHandler(400,"index.html"));
        return -1;
    }
    else{
        char*chkHTTP= strstr(getLC,"http/1");
        if(chkHTTP==NULL){
            printf("%s",errorHandler(400,"index.html"));
            return -1;
        }
        else{
            char *chkHost= strstr(getLC,"host:");
            if(chkHost==NULL){
                printf("%s",errorHandler(400,"index.html"));
                return -1;
            } else{
                char *checkGet=strstr(getLC,"get");
                if(checkGet==NULL) {
                    printf("%s", errorHandler(501, "index.html"));
                    return -1;
                }
                else{
                    return 0;
                }
            }
        }
    }
}


/**
 * counts how many sub-folders are in a provided URL.
 * @param URL a full URL string to count Sub-files.
 * @return a counter for Sub-files.
 **/
int countFolders(char *URL){
    int counter=0;
    int size= (int)strlen(URL);
    int i;
    for(i=0;i<size;i++){
        if (URL[i] == '/' ){
            counter++;
        }
    }
    return counter;
}

/**
 * the function takes a string and extracts a port if available, and returns it with other useful information in a numbers array.
 * @param URL a URL to extract the port from.
 * @param returnValue a provided int array to receive values from the function as follows:
 *  returnValue[PORT_BOOL] - returns TRUE if the provided URL had a port, FALSE otherwise.
 *  returnValue[PORT_PORT] - stores the port, if provided it will store the provided port, otherwise 80 will be stored
 *  returnValue[PORT_INDEX] - returns the index that the port starts at if provided, otherwise 0 will be placed
 *  returnValue[PORT_SIZE] - returs how many digits is the provided port.
 **/
void findPort(int returnValue[],char *URL){
    int port=80;
    int len= (int)strlen(URL);
    int found=FALSE;
    int index=0;
    int i;
    //check if a port is provided;
    for(i=0;i<len;i++){
        if(URL[i] == ':'){
            found=TRUE;
            index=i;
        }
    }
    //get port number;
    if (found==TRUE){
        int newSize=len-index;
        newSize--;
        char *strPort=(char*)(malloc(sizeof(char)*newSize));
        int j=0;
        for(i=index+1;i<len;i++){
            strPort[j]=URL[i];
            j++;
        }
        port= atoi(strPort);
        returnValue[PORT_SIZE]=len-index;
        free(strPort);
    }
    else{
        returnValue[PORT_SIZE]=0;
    }
    returnValue[PORT_BOOL]=found;
    returnValue[PORT_PORT]=port;
    returnValue[PORT_INDEX]=index;
}

/**
 * the function takes a URL and breaks it to sub strings according to the sub-folders.
 * @param URL a string containing a URL.
 * @param subFolders the number of sub-folders.
 * @return a 2D Array, with divided sub-folders.
 **/
char** urlDivider(char URL[], int subFolders){
    int i = 0;
    char **string;
    string= (char **) malloc(sizeof(char*) * subFolders);
    char *p = strtok (URL, "/");
    while (p != NULL) {
        string[i] = (char *) malloc(sizeof(char) * strlen(p) + 1);
        string[i][strlen(p)] = '\0';
        strcpy(string[i++], p);
        p = strtok(NULL, "/");
    }
    return string;
}

/**
 * a function that checks if a file already exists on the local storage or not.
 * @param URL a URL to check if a file matches with.
 * @return TRUE if the file exists, FALSE otherwise.
 **/
int findFile(char* URL){
    FILE *fp=fopen(URL, "r");
    if (fp==NULL){
        return FALSE;
    }
    else{
        fclose(fp);
        return TRUE;
    }

}

/**
 * a function that creates folders in the local storage.
 * @param dividedURL a divided URL 2D array.
 * @param count how many sub-folders are in the Provided URL.
 **/
void makeDirectory(char **dividedURL,int count){
    int i;
    char *path;
    path=(char*)malloc(sizeof(char)*1);
    for(i=0;i<count-1;i++){
        if (realloc(path,strlen(dividedURL[i])+1)==NULL){
            perror("error: realloc\n");
            exit(1);
        }
        strcat(path,dividedURL[i]);
        createSubDirectory(path);
        strcat(path,"/");
    }
    free(path);
}

/**
 * a helper function for "makeDirectory", that creates a folder in local storage.
 * @param subURL a sub-folder name.
 **/
void createSubDirectory(char *subURL){
    int chk= access(subURL, F_OK);
    if (chk==-1){
        mkdir(subURL, 0700);
    }
}

/**
 * a function that creates and prints a header.
 * @param contentLength a content length to print.
 * @return the length of the printed header.
 */
int localHeader(int contentLength,char *URL){
    int i=contentLength;
    int count=0;
    while(i!=0){
        i=i/10;
        count++;
    }
    printf("File is given from local filesystem\n");
    char *str="HTTP/1.0 200 OK\r\nContent-Length: ";
    int returnResult=((int)strlen(str))+count+4;
    printf("%s%d\r\n\r\n",str,contentLength);
    printf("Content-type: ");
    printf("%s", get_mime_type(URL));
    printf("connection: Close");
    return returnResult;

}

/**
 * Creates an HTML get request, and stores a file if 200 ok, and prints a page content on the screen.
 * @param dividedURL the divided url link.
 * @param port the port to connect to.
 * @param SubLinks how many sub-links are in a url.
 * @param fullURL the full URL string.
 * @param subFolderCount how many Sub-folders there is ,to be sent to create folders locally.
 */
void HTMLrequest(char **dividedURL,int port,int SubLinks, char* fullURL,int subFolderCount,int htmlVer,Errors *errorHead,blockedIP *IPhead,blockedURL *URLhead){
   if(isBlocked(dividedURL[0])){
       errorHandler(403,fullURL);
       return;
   }
    struct hostent *hp;
    struct sockaddr_in peeraddr;
    char *name=dividedURL[0];
    peeraddr.sin_family=AF_INET;
    hp= (struct hostent*) gethostbyname(name);
    if(hp==NULL){
        errorHandler(404,fullURL);
        exit(EXIT_FAILURE);
    }
    if(isBlocked((char *) hp)){
        errorHandler(403,fullURL);
        return;
    }
    peeraddr.sin_addr.s_addr=((struct in_addr*)(hp->h_addr))->s_addr;
    struct sockaddr_in srv;
    srv.sin_family=AF_INET;
    srv.sin_port=htons(port);
    srv.sin_addr.s_addr=peeraddr.sin_addr.s_addr;
    int fd;        /* socket descriptor */

    if ((fd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        perror("error: socket\n");
        exit(1);
    }

    if(connect(fd, (struct sockaddr*) &srv, sizeof(srv)) < 0) {
        perror("error: connect\n");
        exit(1);
    }

    char getStr[1024]="GET ";
    int i;
    for(i=1;i<SubLinks+1;i++){
        strcat(getStr,"/");
        strcat(getStr,dividedURL[i]);
    }
    if(htmlVer==HTTP1_0){
        strcat(getStr," HTTP/1.0\r\nHost: ");
    }
    else if(htmlVer==HTTP1_1){
        strcat(getStr," HTTP/1.1\r\nHost: ");
    }
    strcat(getStr,dividedURL[0]);
    strcat(getStr,"\r\nconnection: Close");
    strcat(getStr,"\r\n\r\n");
    printf("%s",getStr);
    printf("HTTP request=\n%s\nLEN=%d\n",getStr, (int)strlen(getStr));
    write(fd,getStr, strlen(getStr));
    int bool=FALSE;
    int checkFirst=FALSE;
    int numOfBites;
    unsigned char sentence[1024]="";
    while((numOfBites=(int)read(fd,sentence,1024))!=0){

        int check= check200((char*)sentence,checkFirst);

        if(check==TRUE){
            checkFirst=TRUE;
            makeDirectory(dividedURL,subFolderCount+1);
            FILE *fp=fopen(fullURL,"a");
            unsigned char *removeHeader=(unsigned char*)strstr((char*)sentence,"\r\n\r\n")+4;
            if(removeHeader!=NULL&&bool==FALSE){
                write(fileno(fp),removeHeader,numOfBites-(removeHeader - sentence));
                removeHeader=NULL;
                bool=TRUE;
            }
            else{
                write(fileno(fp),sentence,numOfBites);
            }

            fclose(fp);
        }
        write(1,sentence,numOfBites);
    }
    close(fd);
}

/**
 * checks if a provided header contains 200 OK.
 * @param str a header string.
 * @param stat a boolean that helps with avoiding loop reset.
 * @return TRUE if header contains 200 OK, FALSE otherwise.
 **/
int check200(char *str,int stat){
    if (stat==FALSE) {
        char *check = strstr(str, "200 OK");
        if (check != NULL) {
            return TRUE;
        } else
            return FALSE;
    }
    else
        return TRUE;
}

/**
 * checks if the inputted URL contains HTTP:// in it.
 * @param str the user-inputted URL.
 * @return TRUE if the user-inputted URL contains HTTP:// , FALSE otherwise.
 */
int checkHTTP(char* str){
    char *http = "http://";
    int count = 0;
    int i;
    for (i = 0; i < 7; i++) {
        if (http[i] == str[i]) {
            count++;
        }
    }
    if (count == 7) {
        return TRUE;
    } else
        return FALSE;
}

/**
 * the function takes a divided 2d array URL and returns a full URL in a string format.
 * @param str 2D array, divided URL.
 * @param size the length of the full path URL.
 * @param subFolders how many sub-folders are in a link.
 * @return a URL in the format of a string.
 */
char *fullPath(char **dividedURL,int size, int subFolders) {
    char *string=(char*)(malloc(sizeof(char)*size));
    strcpy(string,dividedURL[0]);
    int i;
    for(i=1;i<subFolders;i++){
        strcat(string,"/");
        strcat(string,dividedURL[i]);
    }
    return string;
}

/**
 * a function that cuts a provided string from a specified index.
 * @param str a string that will be cut.
 * @param index an index to be cut from.
 * @return a newly cut string.
 */
char *stringCut(char *str, int index){
    char *string;
    string=(char*)(malloc(sizeof (char)*index));
    int i;
    for (i=0;i<index+1;i++){
        string[i]=str[i];
    }
    return string;
}

