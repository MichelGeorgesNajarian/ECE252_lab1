/**
 * @biref To demonstrate how to use zutil.c and crc.c functions
 */

#include <stdio.h>    /* for printf(), perror()...   */
#include <stdlib.h>   /* for malloc()                */
#include <errno.h>    /* for errno                   */
#include "crc.h"      /* for crc()                   */
#include "zutil.h"    /* for mem_def() and mem_inf() */
#include "lab_png.h"  /* simple PNG data structures  */
#include <sys/types.h>/* for data types*/
#include <sys/stat.h> /* stats of data i.e. last access , READ MAN*/
#include <unistd.h>   /* for standard symbolic constants and types*/
#include <string.h>

/******************************************************************************
 * DEFINED MACROS 
 *****************************************************************************/
#define BUF_LEN  (256*16)
#define BUF_LEN2 (256*32)

/******************************************************************************
 * GLOBALS 
 *****************************************************************************/
U8 gp_buf_def[BUF_LEN2]; /* output buffer for mem_def() */
U8 gp_buf_inf[BUF_LEN2]; /* output buffer for mem_inf() */

/******************************************************************************
 * FUNCTION PROTOTYPES 
 *****************************************************************************/

void init_data(U8 *buf, int len);

/******************************************************************************
 * FUNCTIONS 
 *****************************************************************************/

/**
 * @brief initialize memory with 256 chars 0 - 255 cyclically 
 */
void init_data(U8 *buf, int len)
{
    int i;
    for ( i = 0; i < len; i++) {
        buf[i] = i%256;
    }
}

int isPng(char *);
void init_iHDR(struct data_IHDR *, char *, U32 *, struct simple_PNG *);
void init_iDAT( FILE *, char *, U32 *, struct simple_PNG *);
void init_iEND();
char* concatenation(const char *, const char *);

int main(int argc, char **argv)
{
	int success;
	U32 totalHeight = 0;
	for (int i = 1; i < argc; i++) {
		success = isPng(argv[i]);
		if (success == 0) {
			printf("Please enter the correct path to a valid PNG file\n");
			return -1;
		}

	}
	FILE *concatenated_png;
	struct data_IHDR test_iHDR;
	struct simple_PNG test;
	test.p_IHDR = malloc(sizeof(struct chunk));
	memset(test.p_IHDR, 0, sizeof(struct chunk));
	test.p_IHDR->p_data = malloc(DATA_IHDR_SIZE);
	memset(test.p_IHDR->p_data, 0, DATA_IHDR_SIZE);
	test.p_IHDR->length = DATA_IHDR_SIZE;
	test.p_IDAT = malloc(sizeof(struct chunk));
	memset(test.p_IDAT, 0, sizeof(struct chunk));
	test.p_IDAT->length = 0;
	test.p_IEND = malloc(sizeof(struct chunk));
	memset(test.p_IEND, 0, sizeof(struct chunk));

	concatenated_png = fopen("all.png", "w");
    
	for (int i = 1; i < argc; i++) {
		init_iHDR(&test_iHDR, argv[i], &totalHeight, &test);
	}


	fclose(concatenated_png);
	free(test.p_IHDR);
	free(test.p_IDAT);
	free(test.p_IEND);
	free(test.p_IHDR->p_data);
	return 0;
}

void init_iHDR(struct data_IHDR *test_iHDR, char *png_name, U32 *totalHeight, struct simple_PNG *test) {
	FILE *pngFiles;
	U8 *p_buffer = NULL;  /* a buffer that contains some data to play with */
	U32 crc_val = 0;      /* CRC value                                     */
	int ret = 0;          /* return value for various routines             */
	U64 len_def = 0;      /* compressed data length                        */
	U64 len_inf = 0;      /* uncompressed data length                      */

	/* Step 1: Initialize some data in a buffer */
	/* Step 1.1: Allocate a dynamic buffer */
	p_buffer = malloc(BUF_LEN);
	if (p_buffer == NULL) {
		perror("malloc");
		return errno;
	}
	pngFiles = fopen(png_name, "rb");
	p_buffer = malloc(PNG_SIG_SIZE);
	memset(p_buffer, 0, PNG_SIG_SIZE);
	fread(p_buffer, 1, PNG_SIG_SIZE, pngFiles);

	free(p_buffer);

	p_buffer = malloc(CHUNK_LEN_SIZE); //get length of data
	memset(p_buffer, 0, CHUNK_LEN_SIZE);
	fread(p_buffer, 1, CHUNK_LEN_SIZE, pngFiles);
	//simple_PNG_p test = malloc(sizeof(struct simple_PNG));
	
	//test->p_IHDR->p_data = malloc(DATA_IHDR_SIZE);
	//test->p_IHDR->length = DATA_IHDR_SIZE;
	free(p_buffer);
	p_buffer = malloc(sizeof(U8) * CHUNK_TYPE_SIZE);
	memset(p_buffer, 0, sizeof(U8) * CHUNK_TYPE_SIZE);
	fread(p_buffer, 1, sizeof(U8) * CHUNK_TYPE_SIZE, pngFiles);
	for (int i = 0; i < CHUNK_TYPE_SIZE; i++) {
		test->p_IHDR->type[i] = *(p_buffer + i);
	}
	free(p_buffer);
	p_buffer = malloc(test->p_IHDR->length);
	memset(p_buffer, 0, test->p_IHDR->length);
	fread(p_buffer, 1, test->p_IHDR->length, pngFiles);
	for (int i = 0; i < test->p_IHDR->length; i++) {
		*(test->p_IHDR->p_data + i) = *(p_buffer + i);
	}
	free(p_buffer);

	int incrementation = 0;

	//doing width
	memcpy(&test_iHDR->width, test->p_IHDR->p_data, sizeof(test_iHDR->width));
	incrementation += sizeof(test_iHDR->width);
	test_iHDR->width = htonl(test_iHDR->width);

	//doing height
	memcpy(&test_iHDR->height, test->p_IHDR->p_data + incrementation, sizeof(test_iHDR->height));
	test_iHDR->height = htonl(test_iHDR->height);
	*(totalHeight) += test_iHDR->height;
	test_iHDR->height = *(totalHeight); //updating max height
	test_iHDR->height = htonl(test_iHDR->height);
	memcpy(test->p_IHDR->p_data + incrementation, &test_iHDR->height, sizeof(test_iHDR->height));
	//printf("------------------ Height from test: %02X%02X%02X%02X\n", *(test->p_IHDR->p_data + incrementation), *(test->p_IHDR->p_data + incrementation + 1), *(test->p_IHDR->p_data + incrementation + 2), *(test->p_IHDR->p_data + incrementation + 3));
	test_iHDR->height = htonl(test_iHDR->height);
	incrementation += sizeof(test_iHDR->height);

	//doing	bit depth
	memcpy(&test_iHDR->bit_depth, test->p_IHDR->p_data + incrementation, sizeof(test_iHDR->bit_depth));
	incrementation += sizeof(test_iHDR->bit_depth);
	//test_iHDR->bit_depth = htonl(test_iHDR->height);

	//doing color type
	memcpy(&test_iHDR->color_type, test->p_IHDR->p_data + incrementation, sizeof(test_iHDR->color_type));
	incrementation += sizeof(test_iHDR->color_type);
	//test_iHDR->color_type = htonl(test_iHDR->color_type);

	//doing compression
	memcpy(&test_iHDR->compression, test->p_IHDR->p_data + incrementation, sizeof(test_iHDR->compression));
	incrementation += sizeof(test_iHDR->compression);
	//test_iHDR->compression = htonl(test_iHDR->compression);

	//doing filter
	memcpy(&test_iHDR->filter, test->p_IHDR->p_data + incrementation, sizeof(test_iHDR->filter));
	incrementation += sizeof(test_iHDR->filter);
	//test_iHDR->filter = htonl(test_iHDR->filter);

	//doing interlace
	memcpy(&test_iHDR->interlace, test->p_IHDR->p_data + incrementation, sizeof(test_iHDR->interlace));
	incrementation += sizeof(test_iHDR->interlace);
	
	//test_iHDR->interlace = htonl(test_iHDR->interlace);
	p_buffer = malloc(CHUNK_CRC_SIZE);
	memset(p_buffer, 0, CHUNK_CRC_SIZE);
	fread(p_buffer, 1, CHUNK_CRC_SIZE, pngFiles);
	free(p_buffer);
	init_iDAT(pngFiles, png_name, totalHeight, test);
}

void init_iDAT(FILE *pngFiles, char *png_name, U32 *totalHeight, struct simple_PNG *test) {
	
	U8 *p_buffer = NULL;  /* a buffer that contains some data to play with */
	U32 crc_val = 0;      /* CRC value                                     */
	int ret = 0;          /* return value for various routines             */
	U64 len_def = 0;      /* compressed data length                        */
	U64 len_inf = 0;      /* uncompressed data length                      */

	p_buffer = malloc(CHUNK_LEN_SIZE +1); //get length of data
	memset(p_buffer, 0, CHUNK_LEN_SIZE);

	fread(p_buffer, 1, CHUNK_LEN_SIZE, pngFiles);
	printf("from pointer: ");
	for (int i = 0; i < CHUNK_LEN_SIZE; i++) {
		printf("%X", *(p_buffer + i));
	}
	printf("\n");

	
	memcpy(&chuck_length, p_buffer + CHUNK_LEN_SIZE, -4);
	printf("before htonl: %04X \n", chuck_length);
	chuck_length = htonl(chuck_length);
	printf("after htonl: %04X \n", chuck_length);

	test->p_IDAT->length += chuck_length;

	printf("total length %X \n", test->p_IDAT->length);

	//simple_PNG_p test = malloc(sizeof(struct simple_PNG));

	//test->p_IHDR->p_data = malloc(DATA_IHDR_SIZE);
	//test->p_IHDR->length = DATA_IHDR_SIZE;
	free(p_buffer);
	p_buffer = malloc(sizeof(U8) * CHUNK_TYPE_SIZE);
	memset(p_buffer, 0, sizeof(U8) * CHUNK_TYPE_SIZE);
	fread(p_buffer, 1, sizeof(U8) * CHUNK_TYPE_SIZE, pngFiles);
	for (int i = 0; i < CHUNK_TYPE_SIZE; i++) {
		test->p_IDAT->type[i] = *(p_buffer + i);
	}
	printf("\n\n");
	for (int i = 0; i < CHUNK_TYPE_SIZE; i++) {
		printf("%02X", test->p_IDAT->type[i]);
	}
	printf("\n\n");
	free(p_buffer);
}

void init_iEND()
{
}

char* concatenation(const char *s1, const char *s2) {
	//printf("First string is: %s | Second string is: %s\n", s1, s2);
	char *con = malloc(strlen(s1) + strlen(s2) + 1); /*length of s1 + length of s2 + \0 + "/" since it's added between the concatenations*/
	memset(con, 0, strlen(s1) + strlen(s2) + 1);
	strcpy(con, s1);
	con[strlen(s1)] = '\0';
	strcat(con, s2);
	con[strlen(con)] = '\0';
	return con;
}

    /* Step 1.2: Fill the buffer with some data */
//    init_data(p_buffer, BUF_LEN);
//
//    /* Step 2: Demo how to use zlib utility */
//    ret = mem_def(gp_buf_def, &len_def, p_buffer, BUF_LEN, Z_DEFAULT_COMPRESSION);
//    if (ret == 0) { /* success */
//        printf("original len = %d, len_def = %lu\n", BUF_LEN, len_def);
//    } else { /* failure */
//        fprintf(stderr,"mem_def failed. ret = %d.\n", ret);
//        return ret;
//    }
//    
//    ret = mem_inf(gp_buf_inf, &len_inf, gp_buf_def, len_def);
//    if (ret == 0) { /* success */
//        printf("original len = %d, len_def = %lu, len_inf = %lu\n", \
//               BUF_LEN, len_def, len_inf);
//    } else { /* failure */
//        fprintf(stderr,"mem_def failed. ret = %d.\n", ret);
//    }
//
//    /* Step 3: Demo how to use the crc utility */
//    crc_val = crc(gp_buf_def, len_def); // down cast the return val to U32
//    printf("crc_val = %u\n", crc_val);
//    
//    /* Clean up */
//    fclose(pngFiles);
//    free(p_buffer); /* free dynamically allocated memory */
//
//    return 0;
//}

int isPng(char *fullPath) {
    //printf("isPng path: %s\n",fullPath);
    FILE *png_file;
    int trueFalse = 0;
    unsigned char *bufferSize = malloc(8+1); // 8 bytes + 1 for the \0
	memset(bufferSize, 0, 8 + 1);
    png_file = fopen(fullPath, "r");
    if ((png_file = fopen(fullPath, "r"))){
        fread(bufferSize, 1, 4, png_file);
        bufferSize[9] = '\0';
        fclose(png_file);
        if (bufferSize[1] == 'P' && bufferSize[2] == 'N' && bufferSize[3] == 'G') {
            trueFalse = 1;
        }
    } else {
        printf("Inexistant file: \"%s\". Please try again\n",fullPath);
        exit(2);
    }
    free(bufferSize);
    return trueFalse;
}