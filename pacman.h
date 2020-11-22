#ifndef _PACMAN_H_
#define _PACMAN_H_

#include <linux/ioctl.h>

#define MY_MAGIC 'r'
#define NEWGAME _IOW(MY_MAGIC, 0, unsigned long)
#define GAMESTAT _IOR(MY_MAGIC, 1, unsigned long)

//
// Function prototypes
//
int my_open(struct inode *, struct file *);

int my_release(struct inode *, struct file *);

ssize_t my_read(struct file *, char *, size_t, loff_t *);

ssize_t my_write(struct file *, const char *, size_t, loff_t *);

int my_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg);

#endif
