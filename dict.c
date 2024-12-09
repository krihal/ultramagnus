#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "dict.h"
#include "packet.h"

#define DICT_MAX_SIZE 1000000

int dict_size = 0;
char dict_keys[DICT_MAX_SIZE][100];
struct packet *dict_values[DICT_MAX_SIZE];

void dict_print_keys()
{
    for (int i = dict_size; i > 0; i--) {
	printf("%s\n", dict_keys[i]);
    }
}

int dict_get_index(char *key)
{
    int i = 0;
    
    // Since we're most lokely to look for the last element, we start
    // from the end and go backwards.
    for (i = dict_size - 1; i > -1; i--) {
	if (strcmp(dict_keys[i], key) == 0) {
	    return i;
	}
    }

    return -1;
}

void dict_insert(char *key, struct packet *value)
{
    int index = 0;
    size_t payload_len = 0;

    index = dict_get_index(key);

    if (index == -1) {
	strcpy(dict_keys[dict_size], key);
	dict_values[dict_size] = value;
	dict_size++;
    } else {
	payload_len = strlen(dict_values[index]->payload);

	// Append trailing ',' to the payload
	snprintf(dict_values[index]->payload + payload_len, 2, ",");

	// After the trailing ',' append the new value
	snprintf(dict_values[index]->payload + payload_len + 1,
		 strlen(value->payload) + 1,
		 "%s", value->payload);
	
	//dict_values[index]->payload = value;
    }
}

struct packet *dict_get(char *key)
{
    int index = dict_get_index(key);

    if (index == -1) {
	return (struct packet *)NULL;
    }

    return dict_values[index];
}

void dict_print()
{
    int i = 0;

    for (i = dict_size - 1; i > -1; i--) {
	printf("%d=%s\n", i, dict_keys[i]);
    }
}

void dict_print_payload()
{
    int i = 0;
    
    for (i = dict_size - 1; i > -1; i--) {
	if (strcmp(dict_values[i]->payload, "0,1,2") != 0) {
	    printf("[%d] %s:%d -> %s:%d: %s (OOO)\n", i, dict_values[i]->source, dict_values[i]->src_port, dict_values[i]->dest, dict_values[i]->dst_port, dict_values[i]->payload);
	}

	/*
	else
	    printf("[%d] %s:%d -> %s:%d: %s\n", i, dict_values[i]->source, dict_values[i]->src_port, dict_values[i]->dest, dict_values[i]->dst_port, dict_values[i]->payload);
	*/
    }

    printf("\nTotal number of prefixes received: %d\n", dict_size);
}
