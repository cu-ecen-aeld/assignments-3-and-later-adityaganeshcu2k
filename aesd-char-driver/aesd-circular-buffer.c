/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer imlementation
 *
 * @author Dan Walkes
 * @date 2020-03-01
 * @copyright Copyright (c) 2020
 *
 */

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif
#include "aesd-circular-buffer.h"

/**
 * @param buffer the buffer to search for corresponding offset.  Any necessary locking must be performed by caller.
 * @param char_offset the position to search for in the buffer list, describing the zero referenced
 *      character index if all buffer strings were concatenated end to end
 * @param entry_offset_byte_rtn is a pointer specifying a location to store the byte of the returned aesd_buffer_entry
 *      buffptr member corresponding to char_offset.  This value is only set when a matching char_offset is found
 *      in aesd_buffer.
 * @return the struct aesd_buffer_entry structure representing the position described by char_offset, or
 * NULL if this position is not available in the buffer (not enough data is written).
 */
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(
    struct aesd_circular_buffer *buffer,
    size_t char_offset,
    size_t *entry_offset_byte_rtn)
{
    // Reset output
    *entry_offset_byte_rtn = 0;

    // Empty buffer?
    if (!buffer->full && buffer->in_offs == buffer->out_offs) {
        return NULL;
    }

    size_t total_entries_checked = 0;
    uint8_t idx = buffer->out_offs;

    while (total_entries_checked < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED) {

        struct aesd_buffer_entry *entry = &buffer->entry[idx];

        // If this entry is large enough to contain the char_offset
        if (char_offset < entry->size) {
            *entry_offset_byte_rtn = char_offset;
            return entry;
        }

        // Otherwise, move to the next entry
        char_offset -= entry->size;
        idx = (idx + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
        total_entries_checked++;

        // Stop if buffer not full and we've reached the write position
        if (!buffer->full && idx == buffer->in_offs) {
            break;
        }
    }

    // Offset not found
    *entry_offset_byte_rtn = 0;
    return NULL;
}

/**
* Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
* If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
* new start location.
* Any necessary locking must be handled by the caller
* Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
*/
const char* aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{
   const char* ret = NULL; //this pointer is returned so that we can free the oldest entry malloced buffer
   /*null check on user input*/
   if((NULL == add_entry) || (NULL == buffer)) return NULL;
   

  
  /*If buffer already full we remove the oldest data*/

    if (buffer->full) {
        
        ret = buffer->entry[buffer->out_offs].buffptr;//return the oldest entry buffer so that it can be freed
        buffer->out_offs = (buffer->out_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    }
    
    /*Insert new entry*/
    buffer->entry[buffer->in_offs] = *add_entry;
    /* Advance write index */
    buffer->in_offs = (buffer->in_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;

    /* Check if buffer became full */
    if (buffer->in_offs == buffer->out_offs) {
	 buffer->full = true;
    }
    
    return ret;

}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer,0,sizeof(struct aesd_circular_buffer));
}
