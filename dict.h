#ifndef __DICT_H__
#define __DICT_H__

#include "packet.h"

struct packet *dict_get(char *key);
int dict_get_index(char *key);
void dict_insert(char *key, struct packet *value);
void dict_print();
void dict_print_payload();
//  GMR Vitvarservice AB

#endif
