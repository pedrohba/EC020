#ifndef COMMAND_CTRL_H__
#define COMMAND_CTRL_H__

//Incomplete type declaration
//typedef struct command_ctrl_private command_ctrl_private;

typedef struct command_ctrl {
    //"private" data.
	//command_ctrl_private* data;

    //"class" functions
    void (*free)();
    void (*print)();
} command_ctrl;

//instatiate a new person
command_ctrl* get_instance();

//pointer to the actual person in the context
command_ctrl* __actual_command_ctrl;

//sets the actual person
command_ctrl* _(command_ctrl* obj);

#endif
