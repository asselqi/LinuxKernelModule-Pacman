/* pacman.c: Example char device module.
 *
 */
/* Kernel Programming */
#define MODULE
#define LINUX
#define __KERNEL__

#include <linux/kernel.h>   
#include <linux/module.h>
#include <linux/fs.h>
#include<linux/slab.h>
#include <asm/uaccess.h>
#include <linux/errno.h> 
#include <linux/limits.h> 
#include <asm/segment.h>

#include "pacman.h"

#define MY_DEVICE "pacman"


#define NUM_OF_ROWS 30
#define NUM_OF_COLS 101

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anonymous");

/* globals */
int my_major = 0; /* will hold the major # of my device driver */
int i = 0;
int j = 0;

struct file_operations my_fops = {
    .open = my_open,
    .release = my_release,
    .read = my_read,
    .write = my_write,
    .ioctl = my_ioctl
};

//char device private data struct
typedef struct chardev_t {
   int minor;
   int ref_count;
   int write;
   int is_board_init;
   int is_game_finished;
   int score_count;
   int food_count;
   char* game_board;
   struct chardev_t* next_data;
} *CharDev;

CharDev chardev_list = NULL; //global list of all char device's private data structures

/***************************************/
/*
    List implementation-helper functions
*/
/**************************************/


void add_chardev(CharDev dev) {
    if (!chardev_list) {
        chardev_list = dev;
        return;
    } else {
    	CharDev temp = chardev_list;
    	chardev_list = dev;
    	chardev_list->next_data = temp;
    }
}

CharDev search_chardev(int minor) {
    CharDev head = chardev_list;
    while(head) {
        if (head->minor == minor)
            return head;
        head = head->next_data;
    }
    return NULL;
}

void free_chardev_list(void) {
    CharDev head = chardev_list;
    while (head) {
        CharDev temp = head->next_data;
        kfree(head->game_board);
        kfree(head);
        head = temp;
    }
}

void delete_chardev(int minor) {
    CharDev chardev_to_delete = search_chardev(minor);

    if (!chardev_to_delete)
        return;

    CharDev head = chardev_list;
    while (head) {
        //head of the list is the chardev to delete
        if (head == chardev_to_delete) {
            //adjusting the list
            chardev_list = head->next_data;
            //deallocating game board
            kfree(chardev_to_delete->game_board);
            kfree(chardev_to_delete);
            return;
        } else if (head->next_data == chardev_to_delete) {
            //adjusting the list
            head->next_data = chardev_to_delete->next_data;
            //deallocating game board
            kfree(chardev_to_delete->game_board);
            kfree(chardev_to_delete);
            return;
        }
        head = head->next_data;
    }
} 

/********************/
/*
 * Module initialization function
 */
/********************/
int init_module(void) {
    my_major = register_chrdev(my_major, MY_DEVICE, &my_fops);

    if (my_major < 0) {
        printk(KERN_WARNING "can't get dynamic major\n");
        return my_major;
    }
    return 0;
}

/*******************************************************************************************/
/*******************************Helper functions implementation*****************************/
/*******************************************************************************************/

struct file *file_open(const char *path) {
    struct file *filp = NULL;
    mm_segment_t oldfs;
    int err = 0;

    oldfs = get_fs();
    set_fs(get_ds());
    filp = filp_open(path, O_RDWR, O_APPEND);
    set_fs(oldfs);
    if (IS_ERR(filp)) {
        err = PTR_ERR(filp);
        return NULL;
    }
    return filp;
}

/*******************************************************************************************/
/*********************End of helper functions implementation********************************/
/*******************************************************************************************/

void file_close(struct file *file) {
    filp_close(file, NULL);
}


int file_read(struct file *file, unsigned long long offset, unsigned char *data, unsigned int size) {
    mm_segment_t oldfs;
    int ret;

    oldfs = get_fs();
    set_fs(get_ds());

    ret = kernel_read(file, offset, data, size);

    set_fs(oldfs);
    return ret;
}

/********************/
/*
 * Module Clean up function
 */
/********************/
void cleanup_module(void) {
    unregister_chrdev(my_major, MY_DEVICE);
    free_chardev_list();
    return;
}

/********************/
/*
 * File opening function
 */
/********************/
int my_open(struct inode *inode, struct file *filp) {
    CharDev chardev = search_chardev(MINOR(inode->i_rdev));
    if (!chardev) {
        chardev = (CharDev) kmalloc(sizeof(struct chardev_t), GFP_KERNEL);
        if (!chardev)
            return -ENOMEM;

        //alocate the game board
        chardev->game_board = (char*) kmalloc(sizeof(char) * (NUM_OF_ROWS * NUM_OF_COLS), GFP_KERNEL);
        if (!chardev->game_board) {
            kfree(chardev->game_board);
            kfree(chardev);
            return -ENOMEM;
        }
     
        //initializing fields
        chardev->minor = (int)(MINOR(inode->i_rdev));
        chardev->ref_count = 1;
        chardev->write = 0;
        chardev->is_board_init = 0;
        chardev->is_game_finished = 0;
        chardev->score_count = 0;
        chardev->food_count = 0;
        chardev->next_data = NULL;
        filp->private_data = (int*) kmalloc(sizeof(int), GFP_KERNEL);

        //filp->private_data allocation fail
        if (!filp->private_data) {
            kfree(chardev->game_board);
            kfree(chardev);
            return -ENOMEM;
        }

        filp->private_data = chardev;
        //adding chardev to chardev_list
        add_chardev(chardev);
    } else {
        chardev->ref_count++;
        filp->private_data = (int*) kmalloc(sizeof(int), GFP_KERNEL);

        //filp->private_data allocation fail
        if (!filp->private_data) {
            kfree(chardev->game_board);
            kfree(chardev);
            return -ENOMEM;
        }
        
        filp->private_data = chardev;
    }
    return 0;
}

/********************/
/*
 * File closing function
 */
/********************/
int my_release(struct inode *inode, struct file *filp) {
    if (!inode || !filp)
        return -EINVAL;
    
    int minor = (int) (MINOR(inode->i_rdev));
    CharDev chardev = search_chardev(minor);

    if (chardev) {
        chardev->ref_count--;
        if (!chardev->ref_count) {
            //all refrences to chardev has been disposed
            delete_chardev(minor);
            filp->private_data = NULL;
        } else {
            //not all refrences to chardev has benn disposed, so we don't deallocate the game board and status
            filp->private_data = NULL;
        }
    }

    return 0;
}

/********************/
/*
 * Read from file function -Return number of bytes read.
 */
/********************/
ssize_t my_read(struct file *filp, char *buf, size_t count, loff_t *f_pos) {
    int bytes_to_read = count;
    CharDev chardev = (CharDev) filp->private_data;

    if (!bytes_to_read)
        return 0;

    if (!chardev->is_board_init)
        return -EINVAL;
    
    if (bytes_to_read >= NUM_OF_ROWS * NUM_OF_COLS) {
        if (copy_to_user(buf, chardev->game_board, NUM_OF_ROWS * NUM_OF_COLS) != 0)
            return -EBADF;
        return NUM_OF_ROWS * NUM_OF_COLS;
    } else if (bytes_to_read < NUM_OF_ROWS * NUM_OF_COLS) {
        if (copy_to_user(buf, chardev->game_board, bytes_to_read) != 0)
            return -EBADF;
        return bytes_to_read;        
    }
    return -EFAULT; 
}


/********************/
/*
 * Write to file function -Return number of bytes written.
 */
/********************/
ssize_t my_write(struct file *filp, const char* buf, size_t count, loff_t *f_pos) {
    int bytes_to_write = strlen_user((const char*) buf) - 1;
    printk(KERN_DEBUG "bytes_to_write: %d\n", bytes_to_write);
    CharDev chardev = (CharDev) filp->private_data;
    int currRow, currCol;

    if (!bytes_to_write)
        return 0;

    if (!chardev->is_board_init)
        return -EINVAL;

    char* write_buf = (char*) kmalloc(sizeof(char) * bytes_to_write, GFP_KERNEL);
    if (!write_buf) {
        printk(KERN_ERR "Didn't allocate successfully.");
        kfree(write_buf);
        return -EFAULT;
    }
    int bytes_did_not_copy_from_user = copy_from_user(write_buf, buf, bytes_to_write);
    if (bytes_did_not_copy_from_user > 0){
        kfree(write_buf);       
        return -EFAULT;
    }

    //getting current location from game board
    for (i = 0; i < NUM_OF_ROWS; i++) {
        for (j = 0; j < NUM_OF_COLS; j++) {
            if (chardev->game_board[(i * NUM_OF_COLS) + j] == 'x') {
                currRow = i;
                currCol = j;
            }
        }
    }

    //playing the steps on the board
    for (i = 0; i < bytes_to_write; i++) {
        switch(write_buf[i]) {
            case 'U':
                if (currRow != 0 && chardev->game_board[(currRow - 1) * NUM_OF_COLS + currCol] != '#') {
                    chardev->score_count++;
                    chardev->game_board[(currRow * NUM_OF_COLS) + currCol] = ' ';
                    if (chardev->game_board[(currRow - 1) * NUM_OF_COLS + currCol] == '*')
                        chardev->food_count--;
                    currRow--;
                    chardev->game_board[(currRow * NUM_OF_COLS) + currCol] = 'x';
                    if (!chardev->food_count){
                    	//finish the game with success
                        chardev->is_game_finished = 1;
                        return bytes_to_write;
                    }
                }
                break;
            case 'D':
                if (currRow != (NUM_OF_ROWS - 1) && chardev->game_board[(currRow + 1) * NUM_OF_COLS + currCol] != '#') {
                    chardev->score_count++;
                    chardev->game_board[currRow * NUM_OF_COLS + currCol] = ' ';
                    if (chardev->game_board[(currRow + 1) * NUM_OF_COLS + currCol] == '*')
                        chardev->food_count--;
                    currRow++;
                    chardev->game_board[currRow * NUM_OF_COLS + currCol] = 'x';
                    if (!chardev->food_count) {
                        //finish the game with success
                        chardev->is_game_finished = 1;
                        return bytes_to_write;
                    }
                }
                break;
            case 'R':
                if (currCol != (NUM_OF_COLS - 1) && chardev->game_board[currRow * NUM_OF_COLS + currCol + 1] != '#') {
                    chardev->score_count++;
                    chardev->game_board[currRow * NUM_OF_COLS + currCol] = ' ';
                    if (chardev->game_board[currRow * NUM_OF_COLS + currCol + 1] == '*')
                        chardev->food_count--;
                    currCol++;
                    chardev->game_board[currRow * NUM_OF_COLS + currCol] = 'x';
                    if (!chardev->food_count) {
                        //finish the game with success
                        chardev->is_game_finished = 1;
                        return bytes_to_write;
                    }
                }
                break;
            case 'L':
                if (currCol != 0 && chardev->game_board[currRow * NUM_OF_COLS + currCol - 1] != '#') {
                    chardev->score_count++;
                    chardev->game_board[currRow * NUM_OF_COLS + currCol] = ' ';
                    if (chardev->game_board[currRow * NUM_OF_COLS + currCol - 1] == '*')
                        chardev->food_count--;
                    currCol--;
                    chardev->game_board[currRow * NUM_OF_COLS + currCol] = 'x';
                    if (!chardev->food_count) {
                        //finish the game with success
                        chardev->is_game_finished = 1;
                        return bytes_to_write;
                    }
                }
                break;
            default:
                kfree(write_buf);
                return -EFAULT;
        }
    }
    kfree(write_buf);
    return bytes_to_write;
}

/********************/
/*
 * IOCTL functions
 */
/********************/
int my_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg) {
    CharDev chardev = search_chardev(MINOR(inode->i_rdev));
    if (!chardev)
        return 0;

    switch(cmd) {
    case NEWGAME:
        //check wether the argument we recieved is NULL 
        if (!(const char*) arg) {
            chardev->score_count = 0;
            break;
        }
        //create and read the path from the argument
        int path_len = strnlen_user((const char*) arg, PATH_MAX);
        char* path = (char*) kmalloc(sizeof(char) * path_len, GFP_KERNEL);

        if (!path) 
            return -EFAULT;
        if (copy_from_user(path, (const char*) arg, path_len)) {
            kfree(path);
            return -EFAULT;
        }

        //open the file
        struct file* file = (struct file*) file_open(path);
        if (!file) {
            kfree(path);
            return -ENOENT;
        }

        //read the file contents and copy them to the game_board
        int bytes_read = file_read(file, 0, (char*) chardev->game_board, NUM_OF_ROWS * NUM_OF_COLS);
        if (bytes_read != NUM_OF_ROWS * NUM_OF_COLS) {
            file_close(file);
            kfree(path);
            return -EFAULT;
        }

        //initialize game
        chardev->is_board_init = 1;
        chardev->score_count = 0;
        for (i = 0; i < NUM_OF_ROWS; i++) {
            for (j = 0; j < NUM_OF_COLS; j++) {
                if (chardev->game_board[i * NUM_OF_COLS + j] == '*')
                    chardev->food_count++;
            }
        }
        break;

    case GAMESTAT:
        //check if game_board has already been initialized
        if (!chardev->is_board_init)
            return -EINVAL;
        //if the board has no food and we didn't move
        if (!chardev->food_count)
        	chardev->is_game_finished = 1;
        //constructing the 32 bit result
        int result = 0;
        result = chardev->is_game_finished << 31 ;
        result |= chardev->score_count;
        put_user(result, (unsigned int*) arg);
        break;

    default:
       return -ENOTTY;
    }
    return 0;
}
