#ifndef LOG_H
#define LOG_H

#define Log(__level, __levelstr, ...) do{ if(__level >= logLevel){ printf("[%s] ", (__levelstr)); printf(__VA_ARGS__); puts(""); } } while(0)

#define LogD(...) Log(0, "DD", __VA_ARGS__)
#define LogV(...) Log(1, "VV", __VA_ARGS__)
#define LogI(...) Log(2, "II", __VA_ARGS__)
#define LogW(...) Log(3, "WW", __VA_ARGS__)
#define LogE(...) Log(4, "EE", __VA_ARGS__)
#define LogF(...) Log(5, "FF", __VA_ARGS__)

#define LAssert(__v, ...) if(!(__v)){ LogF(__VA_ARGS__); exit(1); };
#define LAssertWarn(__v, ...) if(!(__v)){ LogW(__VA_ARGS__); };

#endif
