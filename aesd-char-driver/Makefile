# Debug flags
ifeq ($(DEBUG),y)
  DEBFLAGS = -O -g -DSCULL_DEBUG
else
  DEBFLAGS = -O2
endif

EXTRA_CFLAGS += $(DEBFLAGS)

ifneq ($(KERNELRELEASE),)
  # Called from kernel build system
  obj-m := aesdchar.o
  aesdchar-y := aesd-circular-buffer.o main.o
else
  PWD := $(shell pwd)

  modules:
	  $(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules

endif

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions
