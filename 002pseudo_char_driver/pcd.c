#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>

#define DEV_MEM_SIZE 512

/* pseudo device's memory */
char device_buffer[DEV_MEM_SIZE];   // 假裝是硬體設備 memory area
dev_t device_number;    // This holds the device number

/* Cdev variable */
struct cdev pcd_cdev;

/* 宣告 file operations prototype */
static int pcd_open(struct inode *inode, struct file *file);
static int pcd_release(struct inode *inode, struct file *file);
static ssize_t pcd_read(struct file *file, char __user *buf, size_t count, loff_t *ppos);
static ssize_t pcd_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos);
static loff_t pcd_lseek (struct file *filp, loff_t offset, int whence);


loff_t pcd_lseek (struct file *filp, loff_t offset, int whence)
{
    loff_t temp;
    
    printk(KERN_ALERT "lseek request \n");
    printk(KERN_ALERT "current value of file position = %lld\n", filp->f_pos);

    switch(whence)
    {
        case SEEK_SET:
            if ((offset > DEV_MEM_SIZE) || (offset < 0))
                return -EINVAL;
            filp->f_pos = offset;
            break;
        case SEEK_CUR:
            temp = filp->f_pos + offset;
            if ((temp > DEV_MEM_SIZE) || (temp < 0))
                return -EINVAL;
            filp->f_pos = temp;
            break;
        case SEEK_END:
            temp = DEV_MEM_SIZE + offset;
            if ((temp > DEV_MEM_SIZE) || (temp < 0))
                return -EINVAL;
            filp->f_pos = temp;
            break;
        default:
            return -EINVAL;
    }
    
    printk(KERN_ALERT "New value of the file position = %lld\n", filp->f_pos);
    return filp->f_pos;
}

ssize_t pcd_read (struct file *filp, char __user *buff, size_t count, loff_t *f_pos)
{
    printk(KERN_ALERT "read requested for %zu bytes \n", count);
    printk(KERN_ALERT "current file position = %lld\n", *f_pos);

    /* Adjust the "count" */
    if ((*f_pos + count) > DEV_MEM_SIZE){
        count = DEV_MEM_SIZE - *f_pos;
    }

    /* copy to user */
    if (copy_to_user(buff, &device_buffer[*f_pos], count)){
        return -EFAULT;
    }

    /* update the current file position */
    *f_pos += count;
    printk(KERN_ALERT "Nmuber of bytes successfully read = %zu\n", count);
    printk(KERN_ALERT "Updated file position = %lld\n", *f_pos);

    /* return number of bytes which been successfully read */
    return count;
}

ssize_t pcd_write (struct file *filp, const char __user *buff, size_t count, loff_t *f_pos)
{
    printk(KERN_ALERT "write requested for %zu bytes \n", count);
    printk(KERN_ALERT "current file position = %lld\n", *f_pos);

    /* Adjust the "count" */
    if ((*f_pos + count) > DEV_MEM_SIZE){
        count = DEV_MEM_SIZE - *f_pos;
    }

    if (!count){
        printk(KERN_ALERT "No space left on the device \n");
        return -ENOMEM;
    }

    /* copy from user */
    if (copy_from_user(&device_buffer[*f_pos], buff, count)){
        return -EFAULT;
    }

    /* update the current file position */
    *f_pos += count;
    printk(KERN_ALERT "Nmuber of bytes successfully written = %zu\n", count);
    printk(KERN_ALERT "Updated file position = %lld\n", *f_pos);

    /* return number of bytes which been successfully written */
    return count;
}

int pcd_open (struct inode *imode, struct file *filp)
{
    printk(KERN_ALERT "open was successful \n");
    return 0;
}

int pcd_release (struct inode *inode, struct file *filp)
{
    printk(KERN_ALERT "release was successful \n");
    return 0;
}

/* file operations of the driver */
struct file_operations pcd_fops = 
{
    .open = pcd_open,
    .write = pcd_write,
    .read = pcd_read,
    .llseek = pcd_lseek,
    .release = pcd_release,
    .owner = THIS_MODULE
};

struct class *class_pcd;

struct device *device_pcd; 

static int __init pcd_driver_init(void)
{
    int ret;

    /* 1. Dynamically allocate a Device number */
    ret = alloc_chrdev_region(&device_number, 0, 1, "pcd_devices");
    if (ret < 0){
        printk(KERN_ALERT "Alloc chrdev failed\n");
        goto out;
    }
    printk(KERN_ALERT "%s: Device number <major>:<minor> = %d:%d\n", __func__,  MAJOR(device_number), MINOR(device_number));

    /* 2. Initialize the cdev structure with fops*/
    cdev_init(&pcd_cdev, &pcd_fops);

    /* 3. Register a device (cdev structure) with VFS */
    pcd_cdev.owner = THIS_MODULE;
    ret = cdev_add(&pcd_cdev, device_number, 1);
    if (ret < 0){
        printk(KERN_ALERT "Cdev add failed\n");
        goto unreg_chardev;
    }

    /* 4. create device class number /sys/class */
    class_pcd = class_create("pcd_class");
    if (IS_ERR(class_pcd)){
        printk(KERN_ALERT "Class creation failed\n");
        ret = PTR_ERR(class_pcd);
        goto cdev_del;
    }

    /* 5. populate the sysfs with device information */
    device_pcd = device_create(class_pcd, NULL, device_number, NULL, "pcd");
    if (IS_ERR(device_pcd)){
        printk(KERN_ALERT "Device create failed\n");
        ret = PTR_ERR(device_pcd);
        goto class_del;
    }

    printk(KERN_ALERT "module init successful.\n");

    return 0;

class_del:
    class_destroy(class_pcd);
cdev_del:
    cdev_del(&pcd_cdev);
unreg_chardev:
    unregister_chrdev_region(device_number, 1);
out:
    printk(KERN_ALERT "Module instion failed\n");
    return ret;
}

/* 清理時, 要照 init 反方向執行*/
static void __exit pcd_driver_cleanup(void) 
{
    device_destroy(class_pcd, device_number);
    class_destroy(class_pcd);
    cdev_del(&pcd_cdev);
    unregister_chrdev_region(device_number, 1);
    printk(KERN_ALERT "module unloaded \n");
}

module_init(pcd_driver_init);
module_exit(pcd_driver_cleanup);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Harry");
MODULE_DESCRIPTION("char device linux driver device prac.");