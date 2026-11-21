#ifndef BUTTON_H
#define BUTTON_H
/* The mode button: a push button between header pin 37 (GPIO 26) and
 * header pin 39 (ground). The internal pull-up holds the pin high; a
 * press pulls it low. */
#define BUTTON_GPIO 26

int button_open(void);              /* returns a file descriptor, or -1 */
int button_presses(int fd);         /* presses since the last call */
void button_close(int fd);
#endif
