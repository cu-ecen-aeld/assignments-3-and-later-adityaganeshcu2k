/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include "aesdchar.h"
#include "aesd_ioctl.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Aditya Ganesh"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    /**
     * TODO: handle open
     */
     filp->private_data = container_of(inode->i_cdev , struct aesd_dev , cdev);
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release
     */
    filp->private_data = NULL;
    return 0;
}

loff_t aesd_llseek(struct file *filp, loff_t offset , int whence)
{

   struct aesd_dev *dev = filp->private_data;
   int ret = 0;
   //Check if file pointer is null
   if(!filp)
   {
     return -EINVAL;
   }
   
   switch(whence)
   {
     //Set file position to user defined offset
     case SEEK_SET:
     {
       ret = filp->f_pos;
       break;
     }
    
     case SEEK_CUR:
     {
       ret = filp->f_pos + offset;
       filp->f_pos = ret;
       break;
     }
     
     //Set file position relative to the end 
     case SEEK_END:
     {
        ret = mutex_lock_interruptible(&dev->cb_lock);
	if(ret !=0)
	{
	   ret = -ERESTART;
	   PDEBUG("Error: Unable to acquire mutex");
	   goto error;
	}
	
	int total_size = 0;
	
	//Calculate the total size of the buffer
	for (int i = dev->circular_buffer.out_offs;
	     i != dev->circular_buffer.in_offs;
	     i = (i + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED)
	{
	    total_size += dev->circular_buffer.entry[i].size;
	}
	
	
	mutex_unlock(&dev->cb_lock);
	ret = total_size-1+offset;
       break;
     }
     defult:
     {
       ret = -EINVAL;
       goto error;
     }
   }
   
   filp->f_pos = ret;
   

error:
    return ret;   
   

}

long aesd_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) 
{
  long retval = 0;
  int total_length = 0;
  if(!filp)
  {
    retval = -EINVAL;
    goto error;
  }
  
  struct aesd_dev *dev = filp->private_data;
  struct aesd_seekto seek_param;
  
  if(cmd == AESDCHAR_IOCSEEKTO)
  {
    //Copy from user space to kernel space
    if (copy_from_user(&seek_param, (struct aesd_seekto __user *)arg, sizeof(seek_param)))
    {
        retval = -EFAULT;
        goto error;
    }
    
    //Check if write_cmd greater than max write operations
    if(seek_param.write_cmd > AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED)
    {
        retval = -EINVAL;
        goto error; 
    }
    
    seek_param.write_cmd = (seek_param.write_cmd+dev->circular_buffer.out_offs)% AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;

    if(seek_param.write_cmd_offset > dev->circular_buffer.entry[seek_param.write_cmd].size)
    {
        retval = -EINVAL;
        goto error; 
    }
    //Lock mutex
    retval = mutex_lock_interruptible(&dev->cb_lock);
    if(retval !=0)
    {
        retval = -ERESTART;
        goto error;
    }
    // Calculate the total length
    for(int i=dev->circular_buffer.out_offs;i!=seek_param.write_cmd; i = (i+1)%AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;)
    {
        total_length+=dev->circular_buffer.entry[i].size;

    }
    filp->f_pos = total_length + seek_param.write_cmd_offset;
    mutex_unlock(&dev->cb_lock);
  }
  
  
  
  error:
    return retval;
}

ssize_t aesd_read(struct file *filp, char __user *buf,
                  size_t count, loff_t *f_pos)
{
    ssize_t retval = 0;
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
 
    /*Input validation*/
    if(!filp || !buf || !f_pos || *f_pos<0)
    {
       PDEBUG("Input validation failed");
       return -EINVAL;
    
    }
    
    struct aesd_dev *dev = filp->private_data;
    size_t entry_offset = 0;
    size_t bytes_to_copy = 0;
    
    retval = mutex_lock_interruptible(&dev->cb_lock);
    if(retval != 0){
    	PDEBUG("Error: Unable to acquire mutex");
    	return -ERESTART;
    }
    
    struct aesd_buffer_entry *temp = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->circular_buffer, *f_pos,&entry_offset);
    if(!temp){
    	PDEBUG("Error: Entry for given position not found");
    	retval = 0;
    	goto lock_error;
    }
    
    bytes_to_copy = temp->size - entry_offset;
    if(bytes_to_copy > count){
    	bytes_to_copy = count;
    }
    
    retval = copy_to_user(buf, temp->buffptr + entry_offset , bytes_to_copy);
    if(retval != 0){
      bytes_to_copy -= retval;
      PDEBUG("Error: Copying data to userspace failed");
      retval = -EFAULT;
      goto lock_error;
    }
    
    *f_pos += bytes_to_copy;
    retval = bytes_to_copy;
    

lock_error:
    mutex_unlock(&dev->cb_lock);
    PDEBUG("Error: Mutex unlocked"); 
    
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    char *kbuf = NULL;
    const char *newline_indx = NULL;
    size_t size_wc = 0;
    struct aesd_dev *dev = NULL;
    
    if(!filp || !buf || count <= 0 || !f_pos || *f_pos<0){
      PDEBUG("Error : Input validation failed");
      return - EINVAL;
    }
    
    //Get the aesd device structure
    dev = filp->private_data;
    if(dev == NULL){
      	PDEBUG("Error: aesd device structure pointer is NULL");
        return - EINVAL;
    }
    
    //allocate memory in kernel for the size requested by user
    kbuf = kmalloc(count, GFP_KERNEL);
    if(kbuf == NULL){
    	PDEBUG("Error: Memory allocation failed");
    	return -ENOMEM;
    }
    
    //copy userspace data to kernel
    retval = copy_from_user(kbuf,buf,count);
    if(retval){
        PDEBUG("Error: Failed to copy user space data to kernel");
        retval = -EFAULT;
        goto copy_error;
        
    }
    
    //see if we find a newline character in the buffer
    newline_indx = memchr(kbuf, '\n', count);
    
    //size of write command if newline character is found
    if(newline_indx){
    	
    	size_wc = newline_indx - kbuf + 1;
    
    }
    else{
     	size_wc = 0;
    }
    
    //get the mutex to add onto the circular buffer
    retval = mutex_lock_interruptible(&dev->cb_lock);
    if(retval != 0){
    	retval = -ERESTART;
        PDEBUG("Error: Failed to acquire a lock");
        goto copy_error;
    }
    
    if(size_wc>0){
    
    	//reallocating more memory for the entry
    	dev->buffer_entry.buffptr = krealloc(dev->buffer_entry.buffptr, dev->buffer_entry.size+size_wc, GFP_KERNEL);
    	if(dev->buffer_entry.buffptr == NULL){
    	   PDEBUG("Error: Reallocation failed");
    	   retval = -ENOMEM;
    	   goto lock_error;
    	}
    	
    	memcpy(dev->buffer_entry.buffptr + dev->buffer_entry.size, kbuf, size_wc);
    	dev->buffer_entry.size += size_wc;
    	
    	const char *ptr = aesd_circular_buffer_add_entry(&dev->circular_buffer, &dev->buffer_entry);
    	if(ptr){
    	  kfree(ptr);
    	}
    	
    	dev->buffer_entry.size = 0;
    	dev->buffer_entry.buffptr = NULL;
    } 
    else
    {
    	dev->buffer_entry.buffptr = krealloc(dev->buffer_entry.buffptr, dev->buffer_entry.size + count, GFP_KERNEL);
    	if(dev->buffer_entry.buffptr == NULL)
    	{
    	  PDEBUG("Error: Reallocation failed");
    	  retval = -ENOMEM;
    	  goto lock_error;
    	}
    	memcpy(dev->buffer_entry.buffptr + dev->buffer_entry.size, kbuf,count);
    	dev->buffer_entry.size += count;
    }
    
    retval = count;
    
    
lock_error:
    mutex_unlock(&dev->cb_lock); 
copy_error:
    kfree(kbuf);	
    	    
    return retval;
}
struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
    .llseek = aesd_llseek,
    .unlocked_ioctl = aesd_unlocked_ioctl,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}



int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    /**
     * TODO: initialize the AESD specific portion of the device
     */
    aesd_circular_buffer_init(&aesd_device.circular_buffer);
    mutex_init(&aesd_device.cb_lock);
    
    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */
     
    struct aesd_buffer_entry *entry;
    uint8_t index = 0;
    
    /*Freeing all the circular buffers*/
    AESD_CIRCULAR_BUFFER_FOREACH(entry, &aesd_device.circular_buffer, index){
    if(entry->buffptr != NULL){
    	kfree(entry->buffptr);
    }
    }
    
    /*Destroy the mutex*/
    mutex_destroy(&aesd_device.cb_lock);

    /*Unregister the character device module*/
    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
