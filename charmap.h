#ifndef CHARMAP_H
#define CHARMAP_H

typedef struct {
    char *name;
    char *utf8;
} CharDefinition;

#ifndef IN_CHARMAP
#define EXTERN
#else
#define EXTERN extern
#endif

EXTERN CharDefinition charmap[128];

#endif
