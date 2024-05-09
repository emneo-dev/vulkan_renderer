#ifndef SSTR_MACRO_H
#define SSTR_MACRO_H

#define LENGTH_OF(arr) (sizeof(arr) / sizeof(arr)[0])

#define SSTR_LEN(sstr) (LENGTH_OF(sstr) - 1)
#define SSTR_UNPACK(sstr) sstr, SSTR_LEN(sstr)

#endif
