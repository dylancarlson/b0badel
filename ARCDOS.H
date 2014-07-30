/*
 * arc header structure for MSDOS.  This is the form an arc header is
 * stored on disk.
 */

struct arcH {
#pragma pack(1)
    char  a_name[AFLEN];
    long  a_size;
    long  a_timestamp;
    short a_crc;
    long  a_length;
#pragma pack()
} ;
