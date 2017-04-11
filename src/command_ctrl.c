#include "command_ctrl.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

//private data... visible only by the functions below
/*struct command_ctrl_private
{
	command_ctrl* instance;
	char* name;
    int age;
};*/

//a "manual destructor"
void free_()
{
    //free(__actual_command_ctrl->data->name);
    //free(__actual_command_ctrl->data);
    free(__actual_command_ctrl);

        puts("command_ctrl sucessfuly freed!\nBye");
}

//prints
/*void print_person()
{
    printf("%s -: %d\n", __actual_person->data->name,
         __actual_person->data->age);
}*/

command_ctrl* get_instance()
{

    //Allocate the object
	if (__actual_command_ctrl == NULL) {
		__actual_command_ctrl = (command_ctrl*)malloc(sizeof(command_ctrl));
	}
//    new->data = (person_private*)malloc(sizeof(person_private));

    //Initialize the data
    /*new->data->name = (char*)malloc(strlen(name) * sizeof(char) + 1);
    strcpy(new->data->name, name);*/

//    new->data->age = age;

    //Set the functions pointers
    __actual_command_ctrl->free = free_;

    puts("command_ctrl sucessfuly created!\nBye");
    return __actual_command_ctrl;
}

//must call the objects with this function
command_ctrl* _(command_ctrl* obj)
{
	__actual_command_ctrl = obj;
    return obj;
}
