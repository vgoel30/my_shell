#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CAPACITY_INCREMENT 5

typedef struct arraylist{
	char **data;
	int current_elements;
	int current_capacity;

}arraylist;

void init_list(arraylist *list);
void insert_element(arraylist *list, char *to_insert);
void clear_list(arraylist *list);
void print_list(arraylist *list);
char *get_element(arraylist *list, int index);