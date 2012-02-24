
#ifndef DECRYPTION_H
#define DECRYPTION_H

#ifdef __cplusplus
extern "C" {
#endif


// from libdvdcss:
typedef struct dvdcss_s* dvdcss_t;
#define DVDCSS_BLOCK_SIZE      2048
#define DVDCSS_NOFLAGS         0
#define DVDCSS_READ_DECRYPT    (1 << 0)
#define DVDCSS_SEEK_MPEG       (1 << 0)
#define DVDCSS_SEEK_KEY        (1 << 1)


#ifdef __OBJC__
@class NSString;

// return 0 on success
int InitDecryption(NSString *tryPath, NSString **libpath);
#endif

extern dvdcss_t (*dvdcss_open)(char *);
extern int (*dvdcss_close)(dvdcss_t);
extern int (*dvdcss_seek)(dvdcss_t, int, int);
extern int (*dvdcss_read)(dvdcss_t, void *, int, int);

#ifdef __cplusplus
}
#endif

#endif /* DECRYPTION_H  */

