#ifndef LOG_H
#define LOG_H

#define Log(__levelstr, ...) do{ printf("[%s] ", (__levelstr)); printf(__VA_ARGS__); puts(""); } while(0)

#define LogD(...) Log("DD", __VA_ARGS__)
#define LogV(...) Log("VV", __VA_ARGS__)
#define LogI(...) Log("II", __VA_ARGS__)
#define LogW(...) Log("WW", __VA_ARGS__)
#define LogE(...) Log("EE", __VA_ARGS__)
#define LogF(...) Log("FF", __VA_ARGS__)

#define LAssert(__v, ...) if(!(__v)){ LogF(__VA_ARGS__); exit(1); };

#endif
