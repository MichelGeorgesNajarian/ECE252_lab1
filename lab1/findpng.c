/*
@ Code by Michel Georges Najarian
*/
#include <stdio.h>    /* for printf(), perror()...   */
#include <stdlib.h>   /* for malloc()                */
#include <errno.h>    /* for errno                   */
#include <dirent.h>   /* for directory manipulation*/
#include <sys/types.h>/* for data types*/
#include <sys/stat.h> /* stats of data i.e. last access , READ MAN*/
#include <unistd.h>   /* for standard symbolic constants and types*/
#include <string.h>


int findPng(char *, int);
int fileType(char *, char*, int);
int isPng(char *);

char* concatenation(const char *s1, const char *s2) {
    //printf("First string is: %s | Second string is: %s\n", s1, s2);
    char *con = malloc(strlen(s1) + strlen(s2) + 3); /*length of s1 + length of s2 + \0 + "/" since it's added between the concatenations*/
    strcpy(con, s1);
    con[strlen(s1)] = '/'; /*adding it right after s1*/
    con[strlen(s1)+1] = '\0';
    strcat(con, s2);
    con[strlen(con)] = '\0';
    return con;
}

int main(int argc, char *argv[]){
    int fd;
    /*struct dirent *p_dirent;*/

    if (argc == 1) {
        printf("Need to enter a directory as well. \nRe-run program with a valid starting directory\n");
        exit(1);
    }

    if (argv[1][strlen(argv[1])-1] == '/'){
        argv[1][strlen(argv[1])-1] = '\0';
    }

    int count = 0;
    count = findPng(argv[1], count);
    if (count == 0){
        printf("findpng: No PNG file found\n");
    }
    return 0;
}

int findPng(char *parent, int count) {
    printf("Beginning findPng function, count: %d\n", count);
    DIR *curr_dir;
    char str[64];
    if ((curr_dir = opendir(parent))== NULL) {
        printf(str,"opendir(%s)",parent);
        perror(str);
        return(2);
    }
    struct dirent *p_dirent;
    p_dirent = readdir(curr_dir); /* Not displaying current directory "." */
    p_dirent = readdir(curr_dir);  /*Not displaying parent directory ".." */
    int i = 0;
    while((p_dirent = readdir(curr_dir)) != NULL) {
        char *str_path = p_dirent->d_name;  /* relative path name! */
        //printf("In while loop %i: %s\n",i,parent);
        i++;
        if (str_path == NULL) {
            fprintf(stderr,"Null pointer found!");
            exit(3);
        } else {
            //printf("%s/%s\n", parent,str_path);
            count += fileType(str_path,parent,count);
            printf("count updated: %d\n", count);
        }
    }
    //printf("------------------------------------- Exited the while loop --------------------------------------\n");
    //printf("Closing directory\n");
    closedir(curr_dir);
    return count;
}

int fileType(char *fileName, char *parentDirectory, int count){
    printf("beginning fileType function count: %d\n",count);
    char *ptr, *newParent, *str, *path2file;
    struct stat buf;
    path2file = concatenation(parentDirectory, fileName);
    if (lstat(path2file, &buf) < 0) {
        perror("lstat error");
    }
    if      (S_ISREG(buf.st_mode)) {
        if(isPng(path2file)) {
            printf("%s\n",path2file);
            count++;
            printf("fileType count updated count: %d\n",count);
        }
    }
    else if (S_ISDIR(buf.st_mode)){
        newParent = concatenation(parentDirectory,fileName);
        findPng(newParent,count);
        free(newParent);
    }
    return count;
}

int isPng(char *fullPath) {
    //printf("isPng path: %s\n",fullPath);
    FILE *png_file;
    int trueFalse = 0;
    unsigned char *bufferSize = malloc(8+1); // 8 bytes + 1 for the \0

    png_file = fopen(fullPath, "r");
    fread(bufferSize, 1, 4, png_file);
    bufferSize[9] = '\0';
    fclose(png_file);
    if(bufferSize[1] == 'P' && bufferSize[2] == 'N' && bufferSize[3] == 'G') {
        trueFalse = 1;
    }
    free(bufferSize);
    return trueFalse;
}

