/*
 * The code is derived from cURL example and paster.c base code.
 * The cURL example is at URL:
 * https://curl.haxx.se/libcurl/c/getinmemory.html
 * Copyright (C) 1998 - 2018, Daniel Stenberg, <daniel@haxx.se>, et al..
 *
 * The paster.c code is 
 * Copyright 2013 Patrick Lam, <p23lam@uwaterloo.ca>.
 *
 * Modifications to the code are
 * Copyright 2018-2019, Yiqing Huang, <yqhuang@uwaterloo.ca>.
 * 
 * This software may be freely redistributed under the terms of the X11 license.
 */

/** 
 * @file main_wirte_read_cb.c
 * @brief cURL write call back to save received data in a user defined memory first
 *        and then write the data to a file for verification purpose.
 *        cURL header call back extracts data sequence number from header.
 * @see https://curl.haxx.se/libcurl/c/getinmemory.html
 * @see https://curl.haxx.se/libcurl/using/
 * @see https://ec.haxx.se/callback-write.html
 */ 


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <getopt.h>
#include <curl/curl.h>
#include <pthread.h>
#include "lab_png.h"
#include "crc.h"
#include "zutil.h"
#include <semaphore.h>

#define IMG_URL "http://ece252-"
#define IMG_URL2 ".uwaterloo.ca:2520/image?img="
#define URL_LEN 44
#define ECE252_HEADER "X-Ece252-Fragment: "
#define BUF_SIZE 1048576  /* 1024*1024 = 1M */
#define BUF_INC  524288   /* 1024*512  = 0.5M */

#define max(a, b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

typedef struct recv_buf2 {
    char *buf;       /* memory to hold a copy of received data */
    size_t size;     /* size of valid data in buf in bytes*/
    size_t max_size; /* max capacity of buf in bytes*/
    int seq;         /* >=0 sequence number extracted from http header */
                     /* <0 indicates an invalid seq number */
} RECV_BUF;


size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata);
size_t write_cb_curl3(char *p_recv, size_t size, size_t nmemb, void *p_userdata);
int recv_buf_init(RECV_BUF *ptr, size_t max_size);
int recv_buf_cleanup(RECV_BUF *ptr);
int getOpt(char **, int *, int *, int);
U8* concatenation(const U8 *, const U32, const U8 *, const U32);
int numDownloaded = 0;
int isFilled[50];
struct simple_PNG strips[50];
struct data_IHDR ihdr_strips[50];
int getInfo(CURL *, CURLcode, RECV_BUF, char *);
int *cURLstart(char *);
int isStored(int);
void init_iHDR(struct data_IHDR *, char *, struct simple_PNG *);
void init_iDAT(data_IHDR_p, char *, simple_PNG_p, int *);
void init_iEND(data_IHDR_p, char *, simple_PNG_p);
void buildPng();
sem_t mutex;
sem_t mutexNumD;


/**
 * @brief  cURL header call back function to extract image sequence number from 
 *         http header data. An example header for image part n (assume n = 2) is:
 *         X-Ece252-Fragment: 2
 * @param  char *p_recv: header data delivered by cURL
 * @param  size_t size size of each memb
 * @param  size_t nmemb number of memb
 * @param  void *userdata user defined data structurea
 * @return size of header data received.
 * @details this routine will be invoked multiple times by the libcurl until the full
 * header data are received.  we are only interested in the ECE252_HEADER line 
 * received so that we can extract the image sequence number from it. This
 * explains the if block in the code.
 */
size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata)
{
    int realsize = size * nmemb;
    RECV_BUF *p = userdata;
    
    if (realsize > strlen(ECE252_HEADER) &&
	strncmp(p_recv, ECE252_HEADER, strlen(ECE252_HEADER)) == 0) {

        /* extract img sequence number */
	p->seq = atoi(p_recv + strlen(ECE252_HEADER));

    }
    return realsize;
}


/**
 * @brief write callback function to save a copy of received data in RAM.
 *        The received libcurl data are pointed by p_recv, 
 *        which is provided by libcurl and is not user allocated memory.
 *        The user allocated memory is at p_userdata. One needs to
 *        cast it to the proper struct to make good use of it.
 *        This function maybe invoked more than once by one invokation of
 *        curl_easy_perform().
 */

size_t write_cb_curl3(char *p_recv, size_t size, size_t nmemb, void *p_userdata)
{
    size_t realsize = size * nmemb;
    RECV_BUF *p = (RECV_BUF *)p_userdata;
 
    if (p->size + realsize + 1 > p->max_size) {/* hope this rarely happens */ 
        /* received data is not 0 terminated, add one byte for terminating 0 */
        size_t new_size = p->max_size + max(BUF_INC, realsize + 1);   
        char *q = realloc(p->buf, new_size);
        if (q == NULL) {
            perror("realloc"); /* out of memory */
            return -1;
        }
        p->buf = q;
        p->max_size = new_size;
    }

    memcpy(p->buf + p->size, p_recv, realsize); /*copy data from libcurl*/
    p->size += realsize;
    p->buf[p->size] = 0;

    return realsize;
}


int recv_buf_init(RECV_BUF *ptr, size_t max_size)
{
    void *p = NULL;
    
    if (ptr == NULL) {
        return 1;
    }

    p = malloc(max_size);
    if (p == NULL) {
	return 2;
    }
    
    ptr->buf = p;
    ptr->size = 0;
    ptr->max_size = max_size;
    ptr->seq = -1;              /* valid seq should be positive */
    return 0;
}

int recv_buf_cleanup(RECV_BUF *ptr)
{
	if (ptr == NULL) {
	return 1;
    }
    
    free(ptr->buf);
    ptr->size = 0;
    ptr->max_size = 0;
    return 0;
}


/**
 * @brief output data in memory to a file
 * @param path const char *, output file path
 * @param in  void *, input data to be written to the file
 * @param len size_t, length of the input data in bytes
 */

int getOpt(char **argv, int *t, int *n, int argc)
{
	int c;
	char *str = "option requires an argument";

	while ((c = getopt(argc, argv, "t:n:")) != -1) {
		switch (c) {
		case 't':
			*t = strtoul(optarg, NULL, 10);
			//printf("option -t specifies a value of %d.\n", *t);
			if (*t <= 0) {
				fprintf(stderr, "%s: %s > 0 -- 't'\n", argv[0], str);
				return -1;
			}
			break;
		case 'n':
			*n = strtoul(optarg, NULL, 10);
			//printf("option -n specifies a value of %d.\n", *n);
			if (*n <= 0 || *n > 3) {
				fprintf(stderr, "%s: %s 1, 2, or 3 -- 'n'\n", argv[0], str);
				return -1;
			}
			break;
		default:
			return -1;
		}
	}
	return 0;
}


int main( int argc, char** argv ) 
{
	sem_init(&mutex, 0, 1);
	sem_init(&mutexNumD, 0, 1);
	for (int i = 0; i < 50; i++) {
		strips[i].p_IDAT = NULL;
		strips[i].p_IHDR = NULL;
		strips[i].p_IEND = NULL;
		isFilled[i] = 0;
	}
	int ret, n, t;
	if ((ret = getOpt(argv, &t, &n, argc)) == -1) {
		printf("Bad use of arguments, exiting...\n");
		return -1;
	}
	pthread_t *p_tids = malloc(sizeof(pthread_t) * t);
	//int numConnection = (t <= 3) ? t : 3; // will distribute thread connection as equally as possible on servers
    U8 **url = malloc(t*((URL_LEN + sizeof(int)) * sizeof(U8)));
	memset(url, 0, t*((URL_LEN + sizeof(int)) * sizeof(U8)));
	//building all the URLs
	for (int i = 0; i < t; i++) {
		url[i] = malloc((URL_LEN + sizeof(int)) * sizeof(U8));
		memset(url[i], 0, (URL_LEN + sizeof(int)) * sizeof(U8));
		strcpy(url[i], IMG_URL);
		char *int_serv;
		int_serv = malloc(sizeof(int));
		sprintf(int_serv, "%d", i % 3 + 1);
		url[i] = concatenation(concatenation(url[i], 14, int_serv, 1), 15, IMG_URL2, URL_LEN - 15);
		char *int_img;
		int_img = malloc(sizeof(int));
		sprintf(int_img, "%d", n);
		url[i] = concatenation(url[i], URL_LEN, int_img, 1);
		//printf("URL is %s\n", url[i]);
		pthread_create(p_tids + i, NULL, cURLstart, url[i]);
		//cURLstart(url[i]);
		free(int_serv);
		free(int_img);
	}
	for (int i = 0; i < t; i++) {
		pthread_join(p_tids[i], NULL);
	}
	sem_destroy(&mutex);
	sem_destroy(&mutexNumD);
	//free (url);	
	free(p_tids);
	buildPng();
	return 0;
}

U8* concatenation(const U8 *s1, const U32 size_s1, const U8 *s2, const U32 size_s2) {
	U8 *con = malloc(size_s1 + size_s2); /*length of s1 + length of s2 + \0 + "/" since it's added between the concatenations*/
	memset(con, 0, size_s1 + size_s2);
	memcpy(con, s1, size_s1);
	memcpy(con + size_s1, s2, size_s2);
#if 0
	printf("First string is: %s | Second string is: %s\n", s1, s2);
	printf("Size of the first string %04X | Size of the second string: %04X\n", strlen(s1), strlen(s2));
	printf("%s\n", con);
#endif
	return con;
}

int *cURLstart(char *url) {
	// starting curl stuff
	CURL *curl_handle;
	CURLcode res;
	RECV_BUF recv_buf;

	recv_buf_init(&recv_buf, BUF_SIZE);


	curl_global_init(CURL_GLOBAL_DEFAULT);
	curl_handle = curl_easy_init();

	if (curl_handle == NULL) {
		fprintf(stderr, "curl_easy_init: returned NULL\n");
		return 1;
	}

	getInfo(curl_handle, res, recv_buf, url);
	//printf("succesfully exited getInfo\n");
	//curl cleanup
	curl_easy_cleanup(curl_handle);
	curl_global_cleanup();
	//recv_buf_cleanup(&recv_buf);
	//printf("think the error is the line before this print statement\n");
	free(url);
}

int getInfo(CURL *curl_handle, CURLcode res, RECV_BUF recv_buf, char *url)
{
	if (numDownloaded == 50) {
		return 0;
	}
	curl_easy_setopt(curl_handle, CURLOPT_URL, url);

	/* register write call back function to process received data */
	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_cb_curl3);
	/* user defined data structure passed to the call back function */
	curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&recv_buf);

	/* register header call back function to process received header data */
	curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, header_cb_curl);
	/* user defined data structure passed to the call back function */
	curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, (void *)&recv_buf);

	/* some servers requires a user-agent field */
	curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
	/* init a curl session */
	res = curl_easy_perform(curl_handle);

	if (res != CURLE_OK) {
		fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
	}
	else {
		//printf("%lu bytes received in memory %p, seq=%d.\n", \
			recv_buf.size, recv_buf.buf, recv_buf.seq);
	}
	int sequence = recv_buf.seq;
	//isStored returns 0 if already have current strip
	if (!isStored(sequence)) {
		isFilled[sequence] = 1;
		init_iHDR(&(ihdr_strips[sequence]), recv_buf.buf, &(strips[sequence]));
		//printf("Finished inserting sequence %d\n", sequence);
		sem_wait(&mutexNumD);
		numDownloaded++;
		sem_post(&mutexNumD);
	}
	recv_buf_cleanup(&recv_buf);
	recv_buf_init(&recv_buf, BUF_SIZE);
	curl_handle = curl_easy_init();

	if (curl_handle == NULL) {
		fprintf(stderr, "curl_easy_init: returned NULL\n");
		return 1;
	}

	getInfo(curl_handle, res, recv_buf, url);
}

int isStored(int inc)
{
	sem_wait(&mutex);
	int ret = isFilled[inc];
	sem_post(&mutex);
	// if true, then sequence has already been downloaded
	return ret;
}



void init_iHDR(struct data_IHDR *test_iHDR, char *png_buf, struct simple_PNG *test) {
	U8 *p_buffer = NULL;  /* a buffer that contains some data to play with */
	int inc = 0;
#if 0
	for (int i = 0; i < 37; i++) {
		printf("%02X ", *(png_buf + i));
	}
	printf("\n");
#endif
	//mallocing all the required info
	int *totalHeight = malloc(sizeof(int));
	test->p_IHDR = malloc(sizeof(struct chunk));
	memset(test->p_IHDR, 0, sizeof(struct chunk));
	test->p_IHDR->p_data = malloc(DATA_IHDR_SIZE);
	memset(test->p_IHDR->p_data, 0, DATA_IHDR_SIZE);
	test->p_IHDR->length = DATA_IHDR_SIZE;
	test->p_IDAT = malloc(sizeof(struct chunk));
	memset(test->p_IDAT, 0, sizeof(struct chunk));
	test->p_IDAT->length = 0;
	test->p_IDAT->p_data = malloc(1);
	test->p_IDAT->p_data[0] = '\0';
	test->p_IEND = malloc(sizeof(struct chunk));
	memset(test->p_IEND, 0, sizeof(struct chunk));
	
	
	/* Step 1: Initialize some data in a buffer */
	/* Step 1.1: Allocate a dynamic buffer */
	p_buffer = malloc(PNG_SIG_SIZE + 1);
	memset(p_buffer, 0, PNG_SIG_SIZE + 1);
	memcpy(p_buffer, png_buf + inc, PNG_SIG_SIZE);
	inc += PNG_SIG_SIZE;
	p_buffer[PNG_SIG_SIZE] = '\0';
	free(p_buffer);

	p_buffer = malloc(CHUNK_LEN_SIZE); //get length of data
	memset(p_buffer, 0, CHUNK_LEN_SIZE);
	memcpy(p_buffer, png_buf + inc, CHUNK_LEN_SIZE);
	inc += CHUNK_LEN_SIZE;

	U32 length_ihdr;

	memcpy(&length_ihdr, p_buffer, CHUNK_LEN_SIZE);
	length_ihdr = htonl(length_ihdr);
	//printf("%08X\n", length_ihdr);
	test->p_IHDR->length = length_ihdr;
	//test->p_IHDR->p_data = malloc(DATA_IHDR_SIZE);
	//test->p_IHDR->length = DATA_IHDR_SIZE;
	free(p_buffer);
	p_buffer = malloc(CHUNK_TYPE_SIZE);
	memset(p_buffer, 0, CHUNK_TYPE_SIZE);
	memcpy(p_buffer, png_buf + inc, CHUNK_TYPE_SIZE);
	inc += CHUNK_TYPE_SIZE;
	for (int i = 0; i < CHUNK_TYPE_SIZE; i++) {
		test->p_IHDR->type[i] = *(p_buffer + i)&0xFF;
		//printf("%02X", test->p_IHDR->type[i]);
	}
	//printf("\n");
	free(p_buffer);
	p_buffer = malloc(test->p_IHDR->length);
	memset(p_buffer, 0, test->p_IHDR->length);
	memcpy(p_buffer, png_buf + inc, length_ihdr);
	//printf("p_data: ");
	for (int i = 0; i < test->p_IHDR->length; i++) {
		*(test->p_IHDR->p_data + i) = *(p_buffer + i);
		//printf("%02X", *(p_buffer + i));
	}
	//printf("\n");
	inc += length_ihdr;
	inc += CHUNK_CRC_SIZE; 
	free(p_buffer);

	int incrementation = 0;

	//doing width
	memcpy(&test_iHDR->width, test->p_IHDR->p_data, sizeof(test_iHDR->width));
	for (int i = 0; i < sizeof(test_iHDR->width); i++) {
		//printf("%02X\n", *(test->p_IHDR->p_data + i));
	}
	
	incrementation += sizeof(test_iHDR->width);
	test_iHDR->width = htonl(test_iHDR->width);
	//printf("Width: %08X\n", test_iHDR->width);
	
	//doing height
	memcpy(&test_iHDR->height, test->p_IHDR->p_data + incrementation, sizeof(test_iHDR->height));
	test_iHDR->height = htonl(test_iHDR->height);
	*(totalHeight) = test_iHDR->height;
	//test_iHDR->height = *(totalHeight); //updating max height
	test_iHDR->height = htonl(test_iHDR->height);
	memcpy(test->p_IHDR->p_data + incrementation, &test_iHDR->height, sizeof(test_iHDR->height));
	test_iHDR->height = htonl(test_iHDR->height);
	incrementation += sizeof(test_iHDR->height);
	//printf("height: %08X\n", test_iHDR->height);

	//doing	bit depth
	memcpy(&test_iHDR->bit_depth, test->p_IHDR->p_data + incrementation, sizeof(test_iHDR->bit_depth));
	incrementation += sizeof(test_iHDR->bit_depth);
	//test_iHDR->bit_depth = htonl(test_iHDR->height);
	//printf("bit depth: %08X\n", test_iHDR->bit_depth);


	//doing color type
	memcpy(&test_iHDR->color_type, test->p_IHDR->p_data + incrementation, sizeof(test_iHDR->color_type));
	incrementation += sizeof(test_iHDR->color_type);
	//test_iHDR->color_type = htonl(test_iHDR->color_type);
	//printf("color type: %08X\n", test_iHDR->color_type);


	//doing compression
	memcpy(&test_iHDR->compression, test->p_IHDR->p_data + incrementation, sizeof(test_iHDR->compression));
	incrementation += sizeof(test_iHDR->compression);
	//test_iHDR->compression = htonl(test_iHDR->compression);
	//printf("compression: %08X\n", test_iHDR->compression);


	//doing filter
	memcpy(&test_iHDR->filter, test->p_IHDR->p_data + incrementation, sizeof(test_iHDR->filter));
	incrementation += sizeof(test_iHDR->filter);
	//test_iHDR->filter = htonl(test_iHDR->filter);
	//printf("filter: %08X\n", test_iHDR->filter);


	//doing interlace
	memcpy(&test_iHDR->interlace, test->p_IHDR->p_data + incrementation, sizeof(test_iHDR->interlace));
	incrementation += sizeof(test_iHDR->interlace);
	//test_iHDR->interlace = htonl(test_iHDR->interlace);
	//printf("interlace: %08X\n", test_iHDR->interlace);
#if 0
	printf("upcoming data is: ");
	for (int i = 1; i <= 4; i++) {
		printf("%02X", *(png_buf + inc + i));
	}
#endif
	init_iDAT(test_iHDR, png_buf + inc, test, totalHeight);
	free(totalHeight);
}

void init_iDAT(data_IHDR_p test_iHDR, char *png_buf, simple_PNG_p test, int *totalHeight)
{
#if 0
	printf("incoming data is ");
	for (int i = 1; i < 5; i++) {
		printf("%02X", *(png_buf + i));
	}
#endif
	U8 *p_buffer = NULL;  /* a buffer that contains some data to play with */
	int ret = 0;          /* return value for various routines             */
	int inc = 0;

	p_buffer = malloc(CHUNK_LEN_SIZE); //get length of data
	memset(p_buffer, 0, CHUNK_LEN_SIZE);

	memcpy(p_buffer, png_buf + inc, CHUNK_LEN_SIZE);
	inc += CHUNK_LEN_SIZE;

	U32 chuck_length;
	memcpy(&chuck_length, p_buffer, CHUNK_LEN_SIZE);
	chuck_length = htonl(chuck_length);

	test->p_IDAT->length = chuck_length;
	//simple_PNG_p test = malloc(sizeof(struct simple_PNG));

	//test->p_IHDR->p_data = malloc(DATA_IHDR_SIZE);
	//test->p_IHDR->length = DATA_IHDR_SIZE;
	free(p_buffer);
	p_buffer = malloc(sizeof(U8) * CHUNK_TYPE_SIZE + 1);
	memset(p_buffer, 0, sizeof(U8) * CHUNK_TYPE_SIZE + 1);
	memcpy(p_buffer, png_buf + inc, sizeof(U8) * CHUNK_TYPE_SIZE);
	inc += sizeof(U8) * CHUNK_TYPE_SIZE;
	for (int i = 0; i < CHUNK_TYPE_SIZE; i++) {
		test->p_IDAT->type[i] = *(p_buffer + i);
	}

	free(p_buffer);
	p_buffer = malloc(chuck_length);
	memset(p_buffer, 0, chuck_length);
	//printf("chuck length: %08X\n", chuck_length);
	memcpy(p_buffer, png_buf + inc, chuck_length);
	inc += chuck_length;
	//printf("Size of p_buffer: %02X\n", sizeof(p_buffer));
	//p_buffer[chuck_length] = '\0';
	//printf("Chuck length of: %02X\n\n\n", chuck_length);
	//printf("p_buffer data: \n")
	U8 *inflated = malloc((test_iHDR->width * 4 + 1)*test_iHDR->height);
	memset(inflated, 0, (test_iHDR->width * 4 + 1)*test_iHDR->height);
	U64 lengthInf = 0;
	U64 lengthCur = 0;
	U64 deflateLength = 0;
	U8 *currData = malloc((test_iHDR->width * 4 + 1) * *(totalHeight));

	memset(currData, 0, (test_iHDR->width * 4 + 1) * *(totalHeight));

	ret = mem_inf(currData, &lengthCur, p_buffer, chuck_length);
	free(p_buffer);
	//ret = mem_inf(gp_buf_inf, &len_inf, gp_buf_def, len_def);
	if (ret != 0) { /* failure */
		fprintf(stderr, "mem_def failed. ret = %d.\n", ret);
	}
	U8 *new_data;
	new_data = malloc(lengthCur);
	new_data = currData;

	free(test->p_IDAT->p_data);
	U8 *deflated_data = malloc((test_iHDR->width * 4 + 1)*test_iHDR->height);
	ret = mem_def(deflated_data, &deflateLength, new_data, lengthCur + lengthInf, Z_DEFAULT_COMPRESSION);
	//ret = mem_def(deflated_data, deflateLength, new_data, lengthCur + lengthInf, -1);
	//    ret = mem_def(gp_buf_def, &len_def, p_buffer, BUF_LEN, Z_DEFAULT_COMPRESSION);
	if (ret != 0) { /* failure */
		fprintf(stderr, "mem_def failed. ret = %d.\n", ret);
		return ret;
	}
	test->p_IDAT->p_data = deflated_data;
	test->p_IDAT->length = deflateLength;

	inc += CHUNK_CRC_SIZE;

	init_iEND(test_iHDR, png_buf + inc, test);
}

void init_iEND(data_IHDR_p test_iHDR, char *png_buf, simple_PNG_p test) {
	U8 *p_buffer = NULL;  /* a buffer that contains some data to play with */
	int inc = 0;
	p_buffer = malloc(CHUNK_LEN_SIZE); //get length of data
	memset(p_buffer, 0, CHUNK_LEN_SIZE);

	memcpy(p_buffer, png_buf + inc, CHUNK_LEN_SIZE);
	inc += CHUNK_LEN_SIZE;

	U32 chuck_length;
	memcpy(&chuck_length, p_buffer, CHUNK_LEN_SIZE);
	chuck_length = htonl(chuck_length);
	test->p_IEND->length = chuck_length;
	//simple_PNG_p test = malloc(sizeof(struct simple_PNG));

	//test->p_IHDR->p_data = malloc(DATA_IHDR_SIZE);
	//test->p_IHDR->length = DATA_IHDR_SIZE;
	free(p_buffer);
	p_buffer = malloc(sizeof(U8) * CHUNK_TYPE_SIZE + 1);
	memset(p_buffer, 0, sizeof(U8) * CHUNK_TYPE_SIZE + 1);
	memcpy(p_buffer, png_buf + inc, sizeof(U8) * CHUNK_TYPE_SIZE);
	inc += sizeof(U8) * CHUNK_TYPE_SIZE;
	for (int i = 0; i < CHUNK_TYPE_SIZE; i++) {
		test->p_IEND->type[i] = *(p_buffer + i);
	}
	free(p_buffer);
	p_buffer = malloc(chuck_length + 1);
	memset(p_buffer, 0, chuck_length);
	memcpy(p_buffer, png_buf + inc, chuck_length);
	inc += chuck_length;
	p_buffer[chuck_length] = '\0';
	test->p_IEND->p_data = p_buffer;
	memcpy(p_buffer, png_buf + inc, CHUNK_CRC_SIZE);
	inc += CHUNK_CRC_SIZE;
	memcpy(&chuck_length, p_buffer, CHUNK_CRC_SIZE);
	chuck_length = htonl(chuck_length);
	test->p_IEND->crc = chuck_length;
	free(p_buffer);
}

void buildPng()
{
	struct simple_PNG final_png;
	struct data_IHDR final_iHDR;
	final_png.p_IHDR = malloc(sizeof(struct chunk));
	final_png.p_IDAT = malloc(sizeof(struct chunk));
	final_png.p_IEND = malloc(sizeof(struct chunk));
	memset(final_png.p_IHDR, 0, sizeof(struct chunk));
	memset(final_png.p_IDAT, 0, sizeof(struct chunk));
	memset(final_png.p_IEND, 0, sizeof(struct chunk));
	
	//Doing iHDR info for final PNG
	U32 totHeight = 0;
	for (int i = 0; i < 50; i++) {
		totHeight += ihdr_strips[i].height;
	}
	final_iHDR.height = htonl(totHeight);
	final_iHDR.width = htonl(ihdr_strips[0].width);
	final_iHDR.bit_depth = ihdr_strips[0].bit_depth;
	final_iHDR.color_type = ihdr_strips[0].color_type;
	final_iHDR.compression = ihdr_strips[0].compression;
	final_iHDR.filter = ihdr_strips[0].filter;
	final_iHDR.interlace = ihdr_strips[0].interlace;
	final_png.p_IHDR->length = htonl(DATA_IHDR_SIZE);
	for (int i = 0; i < CHUNK_LEN_SIZE; i++) {
		final_png.p_IHDR->type[i] = strips[0].p_IHDR->type[i];
	}
	U8 *temp_buffer = malloc(sizeof(U8)*DATA_IHDR_SIZE);
	memcpy(temp_buffer, &final_iHDR.width, DATA_IHDR_SIZE);
#if 0
	for (int i = 0; i < DATA_IHDR_SIZE; i++) {
		printf("%02X ", *(temp_buffer + i));
	}
#endif
	U8 *crc_buffer = malloc(sizeof(U8)*DATA_IHDR_SIZE + CHUNK_LEN_SIZE);
	memcpy(crc_buffer, &final_png.p_IHDR->type, CHUNK_LEN_SIZE);
	memcpy(crc_buffer + CHUNK_LEN_SIZE, temp_buffer, DATA_IHDR_SIZE);
	
	//printf("\n");
	unsigned int crc_return;
	crc_return = crc(crc_buffer, DATA_IHDR_SIZE + CHUNK_TYPE_SIZE);
	//printf("%08X\n", crc_return);
	final_png.p_IHDR->crc = htonl(crc_return);
	free(temp_buffer);
	free(crc_buffer);
	//Final iDATA info for final PNG
	int ret = 0;          /* return value for various routines             */
	U64 len_def = 0;      /* compressed data length                        */
	U64 len_inf = 0;      /* uncompressed data length                      */
	int isFirst = 1;
	for (int i = 0; i < CHUNK_TYPE_SIZE; i++) {
		final_png.p_IDAT->type[i] = strips[0].p_IDAT->type[i];
	}
	for (int i = 0; i < 50; i++) {
		//printf("beginning of for loop - %d\n", i);
		U8 *inflated = malloc((final_iHDR.width * 4 + 1)*final_iHDR.height);
		memset(inflated, 0, (final_iHDR.width * 4 + 1)*final_iHDR.height);
		U64 lengthInf = 0;
		U64 lengthCur = 0;
		U64 deflateLength = 0;
		U8 *currData = malloc((ihdr_strips[i].width * 4 + 1) * (ihdr_strips[i].height));

		memset(currData, 0, (ihdr_strips[i].width * 4 + 1) * (ihdr_strips[i].height));

		if (isFirst == 0) {
			ret = mem_inf(inflated, &lengthInf, final_png.p_IDAT->p_data, final_png.p_IDAT->length);
			if (ret == 0) { /* success */
				//printf("original len = %d, len_def = %lu, len_inf = %lu\n", \
					chuck_length, len_def, lengthCur);
			}
			else { /* failure */
				fprintf(stderr, "mem_def failed. ret = %d.\n", ret);
			}
		}
		
		ret = mem_inf(currData, &lengthCur, strips[i].p_IDAT->p_data, strips[i].p_IDAT->length);
		//ret = mem_inf(gp_buf_inf, &len_inf, gp_buf_def, len_def);
		if (ret == 0) { /* success */
			//printf("original len = %d, len_def = %lu, len_inf = %lu\n", \
				chuck_length, len_def, lengthCur);
		}
		else { /* failure */
			fprintf(stderr, "mem_def failed. ret = %d.\n", ret);
		}
		U8 *new_data;
		if (isFirst == 1) {
			new_data;
			new_data = currData;
		}
		else
		{
			new_data;
			new_data = concatenation(inflated, lengthInf, currData, lengthCur);
		}
		free(inflated);
		//free(currData);
		free(final_png.p_IDAT->p_data);
		U8 *deflated_data = malloc((final_iHDR.width * 4 + 1)*final_iHDR.height);
		ret = mem_def(deflated_data, &deflateLength, new_data, lengthCur + lengthInf, Z_DEFAULT_COMPRESSION);
		//ret = mem_def(deflated_data, deflateLength, new_data, lengthCur + lengthInf, -1);
		//    ret = mem_def(gp_buf_def, &len_def, p_buffer, BUF_LEN, Z_DEFAULT_COMPRESSION);
		if (ret == 0) { /* success */
		   //printf("original len = %d, len_def = %lu\n", lengthCur + lengthInf, deflateLength);
		}
		else { /* failure */
			fprintf(stderr, "mem_def failed. ret = %d.\n", ret);
			return ret;
		}
		final_png.p_IDAT->p_data = deflated_data;
		final_png.p_IDAT->length = deflateLength;
		isFirst = 0;
		free(new_data);
	}

	//printf("bananas3");
	crc_return = crc(concatenation(&final_png.p_IDAT->type, CHUNK_TYPE_SIZE, final_png.p_IDAT->p_data, final_png.p_IDAT->length), CHUNK_TYPE_SIZE + final_png.p_IDAT->length);
	final_png.p_IDAT->crc = htonl(crc_return);
	//final_png.p_IDAT->length = htonl(final_png.p_IDAT->length);
	final_png.p_IEND->length = strips[0].p_IEND->length;
	for (int i = 0; i < CHUNK_TYPE_SIZE; i++) {
		final_png.p_IEND->type[i] = strips[i].p_IEND->type[i];
	}
	final_png.p_IEND->crc = htonl(strips[0].p_IEND->crc);
	

	FILE *concatenated_png;
	concatenated_png = fopen("output.png", "wb");
	final_png.p_IDAT->length = htonl(final_png.p_IDAT->length);
	unsigned char png_definition[] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, '\0' };
	fwrite(&png_definition, PNG_SIG_SIZE, 1, concatenated_png);
	fwrite(&final_png.p_IHDR->length, CHUNK_LEN_SIZE, 1, concatenated_png);
	fwrite(&final_png.p_IHDR->type, CHUNK_TYPE_SIZE, 1, concatenated_png);
	fwrite(&final_iHDR.width,  DATA_IHDR_SIZE, 1, concatenated_png);
	fwrite(&final_png.p_IHDR->crc, CHUNK_CRC_SIZE, 1, concatenated_png);
	fwrite(&final_png.p_IDAT->length, CHUNK_LEN_SIZE, 1, concatenated_png);
	fwrite(&final_png.p_IDAT->type, CHUNK_TYPE_SIZE, 1, concatenated_png);
	fwrite(final_png.p_IDAT->p_data, htonl(final_png.p_IDAT->length), 1, concatenated_png);
	fwrite(&final_png.p_IDAT->crc, CHUNK_CRC_SIZE, 1, concatenated_png);
	fwrite(&final_png.p_IEND->length, CHUNK_LEN_SIZE, 1, concatenated_png);
	fwrite(&final_png.p_IEND->type, CHUNK_TYPE_SIZE, 1, concatenated_png);
	fwrite(&final_png.p_IEND->crc, CHUNK_CRC_SIZE, 1, concatenated_png);
	free(final_png.p_IDAT->p_data);
	free(final_png.p_IHDR);
	free(final_png.p_IDAT);
	free(final_png.p_IEND);
	fclose(concatenated_png);
}
