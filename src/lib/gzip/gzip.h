#ifndef __GZIP_H
#define __GZIP_H

#ifdef __cplusplus
extern "C" {
#endif

void gz_init(void *ptr, unsigned size, 
	     void (*_raw_write)(const char *s, size_t len));
void gz_end(void);
int  gz_open(const char *fname);
int  gz_write(const char *data, unsigned int size);
int  gz_close(void);

#ifdef __cplusplus
}
#endif

#endif

