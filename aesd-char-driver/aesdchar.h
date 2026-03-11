/*
 * aesdchar.h
 *
 *  Created on: Oct 23, 2019
 *      Author: Dan Walkes
 */

#ifndef AESD_CHAR_DRIVER_AESDCHAR_H_
#define AESD_CHAR_DRIVER_AESDCHAR_H_

#define AESD_DEBUG 1  //Remove comment on this line to enable debug

#include "aesd-circular-buffer.h" //to include buffer entry defintion

#undef PDEBUG             /* undef it, just in case */
#ifdef AESD_DEBUG
#  ifdef __KERNEL__
     /* This one if debugging is on, and kernel space */
#    define PDEBUG(fmt, args...) printk( KERN_DEBUG "aesdchar: " fmt, ## args)
#  else
     /* This one for user space */
#    define PDEBUG(fmt, args...) fprintf(stderr, fmt, ## args)
#  endif
#else
#  define PDEBUG(fmt, args...) /* not debugging: nothing */
#endif

/*
* Contains :- 
   1. aesd_buffer_entry :- Structure containing the pointer to the user sent string along with size
   2. aesd_circular_buffer :- Circular buffer where each string would be stored alongside the read and write pointers
   3. mutex :- Lock used to ensure read and write opeartions are exclusive for one user.
   4. cdev :- Character device structure associated with our character device
*/
struct aesd_dev
{

    struct aesd_buffer_entry buffer_entry;
    struct aesd_circular_buffer circular_buffer;
    struct mutex cb_lock;
    struct cdev cdev;     /* Char device structure      */
};


#endif /* AESD_CHAR_DRIVER_AESDCHAR_H_ */
