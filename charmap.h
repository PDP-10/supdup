#ifndef CHARMAP_H
#define CHARMAP_H

typedef struct {
    char *name;
    char *utf8;
} CharDefinition;

extern CharDefinition charmap[128];

#endif
