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
void init_iHDR(struct data_IHDR *, char *, U32 *, struct simple_PNG *, int);
void init_iDAT(struct data_IHDR *, FILE *, U32 *, struct simple_PNG *, int);
void init_iEND(struct data_IHDR *, FILE *, U32 *, struct simple_PNG *, int);
U8* concatenation(const U8 *, const U8 *);
void buildPng(struct simple_PNG *, FILE *);

int main(int argc, char **argv)
{
	int success, isFirst;
	isFirst = 1;
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
	test.p_IDAT->p_data = malloc(1);
	test.p_IDAT->p_data[0] = '\0';
	test.p_IEND = malloc(sizeof(struct chunk));
	memset(test.p_IEND, 0, sizeof(struct chunk));

	concatenated_png = fopen("all.png", "w");
    
	for (int i = 1; i < argc; i++) {
		init_iHDR(&test_iHDR, argv[i], &totalHeight, &test, isFirst);
		isFirst = 0;
	}
	U8 *everything_buffer = malloc(test.p_IDAT->length + CHUNK_LEN_SIZE);
	everything_buffer = &test.p_IDAT->type;
	test.p_IDAT->crc = crc(everything_buffer, test.p_IDAT->length + CHUNK_LEN_SIZE);
	printf("i_dat crc value: %04X\n", test.p_IDAT->crc);
	//free(everything_buffer);
	U8 *everything_buffer1;
	everything_buffer1 = malloc(test.p_IHDR->length + CHUNK_LEN_SIZE);
	everything_buffer1 = &test.p_IHDR->type;
	test.p_IHDR->crc = crc(everything_buffer1, test.p_IHDR->length + CHUNK_LEN_SIZE);
	
	buildPng(&test, concatenated_png);


	fclose(concatenated_png);
	free(test.p_IHDR->p_data);
	free(test.p_IDAT->p_data);
	free(test.p_IEND->p_data);
	free(test.p_IHDR);
	free(test.p_IDAT);
	free(test.p_IEND);
	return 0;
}

void init_iHDR(struct data_IHDR *test_iHDR, char *png_name, U32 *totalHeight, struct simple_PNG *test, int isFirst) {
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
	p_buffer = malloc(PNG_SIG_SIZE +1);
	memset(p_buffer, 0, PNG_SIG_SIZE + 1);
	fread(p_buffer, 1, PNG_SIG_SIZE, pngFiles);
	p_buffer[PNG_SIG_SIZE] = '\0';
	free(p_buffer);

	p_buffer = malloc(CHUNK_LEN_SIZE); //get length of data
	memset(p_buffer, 0, CHUNK_LEN_SIZE);

	fread(p_buffer, 1, CHUNK_LEN_SIZE, pngFiles);


	U32 length_ihdr;
	memcpy(&length_ihdr, p_buffer, CHUNK_LEN_SIZE);
	length_ihdr = htonl(length_ihdr);

	test->p_IHDR->length = length_ihdr;
	
	//test->p_IHDR->p_data = malloc(DATA_IHDR_SIZE);
	//test->p_IHDR->length = DATA_IHDR_SIZE;
	free(p_buffer);
	p_buffer = malloc(sizeof(U8) * CHUNK_TYPE_SIZE + 1);
	memset(p_buffer, 0, sizeof(U8) * CHUNK_TYPE_SIZE + 1);
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
	U32 curr_chunk_height = test_iHDR->height;
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
	init_iDAT(test_iHDR, pngFiles, &curr_chunk_height, test, isFirst);
}

void init_iDAT(struct data_IHDR *test_iHDR, FILE *pngFiles,  U32 *totalHeight, struct simple_PNG *test, int isFirst) {
	
	U8 *p_buffer = NULL;  /* a buffer that contains some data to play with */
	U32 crc_val = 0;      /* CRC value                                     */
	int ret = 0;          /* return value for various routines             */
	U64 len_def = 0;      /* compressed data length                        */
	U64 len_inf = 0;      /* uncompressed data length                      */

	p_buffer = malloc(CHUNK_LEN_SIZE); //get length of data
	memset(p_buffer, 0, CHUNK_LEN_SIZE);

	fread(p_buffer, 1, CHUNK_LEN_SIZE, pngFiles);
	

	U32 chuck_length;
	memcpy(&chuck_length, p_buffer, CHUNK_LEN_SIZE);
	chuck_length = htonl(chuck_length);
	
	test->p_IDAT->length += chuck_length;
	//simple_PNG_p test = malloc(sizeof(struct simple_PNG));

	//test->p_IHDR->p_data = malloc(DATA_IHDR_SIZE);
	//test->p_IHDR->length = DATA_IHDR_SIZE;
	free(p_buffer);
	p_buffer = malloc(sizeof(U8) * CHUNK_TYPE_SIZE + 1);
	memset(p_buffer, 0, sizeof(U8) * CHUNK_TYPE_SIZE + 1);
	fread(p_buffer, 1, sizeof(U8) * CHUNK_TYPE_SIZE, pngFiles);
	for (int i = 0; i < CHUNK_TYPE_SIZE; i++) {
		test->p_IDAT->type[i] = *(p_buffer + i);
	}

	free(p_buffer);
	p_buffer = malloc(chuck_length + 1);
	memset(p_buffer, 0, chuck_length);
	fread(p_buffer, 1, chuck_length, pngFiles);
	//printf("Size of p_buffer: %02X\n", sizeof(p_buffer));
	p_buffer[chuck_length] = '\0';
	//printf("Chuck length of: %02X\n\n\n", chuck_length);
	wait(1000);
	//printf("p_buffer data: \n")
	U8 *inflated = malloc((test_iHDR->width * 4 + 1)*test_iHDR->height);
	memset(inflated, 0, (test_iHDR->width * 4 + 1)*test_iHDR->height);
	U64 lengthInf = 0;
	U64 lengthCur = 0;
	U64 deflateLength = 0;
	U8 *currData = malloc((test_iHDR->width*4 + 1) * *(totalHeight));
	
	memset(currData, 0, (test_iHDR->width * 4 + 1) * *(totalHeight));
	
	if (isFirst == 0) {
		ret = mem_inf(inflated, &lengthInf, test->p_IDAT->p_data, test->p_IDAT->length - chuck_length);
		if(ret == 0) { /* success */
			//printf("original len = %d, len_def = %lu, len_inf = %lu\n", \
				chuck_length, len_def, lengthCur);
		}
		else { /* failure */
			fprintf(stderr, "mem_def failed. ret = %d.\n", ret);
		}
	}
	ret = mem_inf(currData, &lengthCur, p_buffer, chuck_length);
	free(p_buffer);
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
		new_data = malloc(sizeof(currData));
		new_data = currData;
	}
	else
	{
		new_data = malloc(sizeof(currData) + sizeof(inflated));
		new_data = concatenation(inflated, currData);
	}
	
	free(test->p_IDAT->p_data);
	U8 *deflated_data = malloc((test_iHDR->width * 4 + 1)*test_iHDR->height);
	ret = mem_def(deflated_data, &deflateLength, new_data, lengthCur + lengthInf, Z_DEFAULT_COMPRESSION);
	//ret = mem_def(deflated_data, deflateLength, new_data, lengthCur + lengthInf, -1);
	//    ret = mem_def(gp_buf_def, &len_def, p_buffer, BUF_LEN, Z_DEFAULT_COMPRESSION);
	if (ret == 0) { /* success */
	   //printf("original len = %d, len_def = %lu\n", lengthCur + lengthInf, deflateLength);
    } else { /* failure */
       fprintf(stderr,"mem_def failed. ret = %d.\n", ret);
       return ret;
    }
	test->p_IDAT->p_data = deflated_data;
	test->p_IDAT->length = deflateLength;

	if (isFirst == 1) {
		p_buffer = malloc(CHUNK_CRC_SIZE);
		memset(p_buffer, 0, CHUNK_CRC_SIZE);
		fread(p_buffer, 1, CHUNK_CRC_SIZE, pngFiles);
		free(p_buffer);
		init_iEND(test_iHDR, pngFiles, totalHeight, test, isFirst);
	}
	else {
		fclose(pngFiles);
	}
}

void init_iEND(struct data_IHDR *test_iHDR, FILE *pngFiles, U32 *totalHeight, struct simple_PNG *test, int isFirst)
{
	U8 *p_buffer = NULL;  /* a buffer that contains some data to play with */
	U32 crc_val = 0;      /* CRC value                                     */
	int ret = 0;          /* return value for various routines             */
	U64 len_def = 0;      /* compressed data length                        */
	U64 len_inf = 0;      /* uncompressed data length                      */

	p_buffer = malloc(CHUNK_LEN_SIZE); //get length of data
	memset(p_buffer, 0, CHUNK_LEN_SIZE);

	fread(p_buffer, 1, CHUNK_LEN_SIZE, pngFiles);


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
	fread(p_buffer, 1, sizeof(U8) * CHUNK_TYPE_SIZE, pngFiles);
	for (int i = 0; i < CHUNK_TYPE_SIZE; i++) {
		test->p_IEND->type[i] = *(p_buffer + i);
	}
	free(p_buffer);
	p_buffer = malloc(chuck_length + 1);
	memset(p_buffer, 0, chuck_length);
	fread(p_buffer, 1, chuck_length, pngFiles);
	p_buffer[chuck_length] = '\0';
	test->p_IEND->p_data = p_buffer;
	char *iend_crc = malloc(CHUNK_CRC_SIZE);
	fread(p_buffer, 1, CHUNK_CRC_SIZE, pngFiles);
	memcpy(&chuck_length, p_buffer, CHUNK_CRC_SIZE);
	chuck_length = htonl(chuck_length);
	test->p_IEND->crc = chuck_length;
	fclose(pngFiles);
}

U8* concatenation(const U8 *s1, const U8 *s2) {
	//printf("First string is: %s | Second string is: %s\n", s1, s2);
	char *con = malloc(strlen(s1) + strlen(s2) + 1); /*length of s1 + length of s2 + \0 + "/" since it's added between the concatenations*/
	memset(con, 0, strlen(s1) + strlen(s2) + 1);
	strcpy(con, s1);
	con[strlen(s1)] = '\0';
	strcat(con, s2);
	con[strlen(con)] = '\0';
	return con;
}

void buildPng(struct simple_PNG *test, FILE *concatenated_png)
{
	//printf("IHDR: Length: %08X\n", test->p_IHDR->length);
	//printf("IHDR: type: ");
	//for (int i = 0; i < CHUNK_TYPE_SIZE; i++) {
	//	printf("%02X", test->p_IHDR->type[i]);
	//}
	//printf("\nIHDR: p_data: ");
	unsigned char png_definition[] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };
	fwrite(&png_definition, 8, 1, concatenated_png);
#if 0
	for (int i = 0; i < test->p_IDAT->length; i++) {
		printf("%02X\n", *(test->p_IDAT->p_data + i));
	}
#endif
	/*for (U8 i = 0; i < test->p_IHDR->length; i++) {
		printf("%02X", *(test->p_IHDR->p_data + i));
	}*/
	//printf("\nIHDR: CRC: %08X\n", test->p_IHDR->crc);
	fwrite(&test->p_IHDR->length, CHUNK_LEN_SIZE, 1, concatenated_png);
	fwrite(&test->p_IHDR->type, 1, CHUNK_TYPE_SIZE, concatenated_png);
	fwrite(test->p_IHDR->p_data, 1, test->p_IHDR->length, concatenated_png);
	fwrite(&test->p_IHDR->crc, 1, CHUNK_CRC_SIZE, concatenated_png);
	//printf("string iHDR p_data: %s\n", *test->p_IHDR->p_data);
	fwrite(&test->p_IDAT->length, 1 , CHUNK_LEN_SIZE, concatenated_png);
	fwrite(&test->p_IDAT->type, 1,  CHUNK_TYPE_SIZE, concatenated_png);
	fwrite(test->p_IDAT->p_data, 1, test->p_IDAT->length, concatenated_png);
	
	//printf("---------------------------------------------------------------------\n");
	/*printf("IDAT: Length: %08X\nstring length: %08X\n", test->p_IDAT->length, strlen(test->p_IHDR->p_data));
	printf("IDAT: type: ");*/
	//for (int i = 0; i < CHUNK_TYPE_SIZE; i++) {
	//	printf("%02X", test->p_IDAT->type[i]);
	//}
#if 0
	printf("\nIDAT: p_data: ");
	/*for (U8 i = 0; i < test->p_IDAT->length; i++) {
		printf("%02X", *(test->p_IDAT->p_data + i));
	}*/
	printf("\nIDAT: CRC: %08X\n", test->p_IDAT->crc);

	printf("total length of p_data: %X\n", sizeof(test->p_IDAT->p_data));
#endif
	fwrite(&test->p_IEND->length, 1, CHUNK_LEN_SIZE, concatenated_png);
	fwrite(&test->p_IEND->type, 1, CHUNK_TYPE_SIZE, concatenated_png);
	fwrite(test->p_IEND->p_data, 1, test->p_IHDR->length, concatenated_png);
	fwrite(&test->p_IEND->crc, 1, CHUNK_CRC_SIZE, concatenated_png);
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
//   ret = mem_inf(gp_buf_inf, &len_inf, gp_buf_def, len_def);
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