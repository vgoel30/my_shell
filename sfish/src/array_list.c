#include "../include/array_list.h"

void init_list(arraylist *list){
	list -> current_capacity = CAPACITY_INCREMENT;
	list -> data = calloc(CAPACITY_INCREMENT, sizeof(char *));
	list -> current_elements = 0;
}

void insert_element(arraylist *list, char *to_insert){
	/* For a non empty list, if there is no more room for another pointer an the /0, realloc */
	if(list -> current_elements == list -> current_capacity - 1){
		/* Realloc for more space */
		if(!((list -> data) = (realloc(list -> data, sizeof(char *) * (list -> current_elements + CAPACITY_INCREMENT))))){
			printf("REALLOC ERROR\n");
			exit(0);
		}
		/* Increase the current capacity by increment */
		(list -> current_capacity) += CAPACITY_INCREMENT;
		/* Insert the pointer */
		*((list -> data) +  (list -> current_elements)) = strdup(to_insert);
		/* Increment the total elements in the list */
		(list -> current_elements)++;
		*((list -> data) +  (list -> current_elements)) = "\0";
	}
	/* If there is space to add an element */
	else{
		*((list -> data) +  (list -> current_elements)) = strdup(to_insert); 
		(list -> current_elements)++;
		*((list -> data) +  (list -> current_elements)) = "\0";
	}
}

char *get_element(arraylist *list, int index){
	if(index >= list -> current_elements)
		return NULL;
	return *((list -> data) +  index);
}

void print_list(arraylist *list){
	int i = 0;
	while(list -> current_elements > i){
		printf("%s ",*(list -> data + i));
		i++;
	}
	printf("\n");
}

void clear_list(arraylist *list){
	/* Free all the old pointers and set the value at the index to NULL */
	for(int i = 0; i < list -> current_elements; i++){
		free(*((list -> data) +  i));
		*((list -> data) +  i) = NULL;
	}
	list -> current_elements = 0;
}

// int main(int argc, char const *argv[]){
// 	arraylist *list = malloc(sizeof(arraylist));
// 	init_list(list);
// 	insert_element(list, "Hello");
// 	insert_element(list, "World");
// 	insert_element(list, "Lit");
// 	insert_element(list, "Scuderia");
// 	clear_list(list);
// 	insert_element(list, "Hello");
// 	insert_element(list, "World");
// 	insert_element(list, "Lit");
// 	insert_element(list, "Scuderia");
// 	//print_list(list);
// 	printf("%s\n",get_element(list, 1));
// 	free(list);
// 	return 0;
// }