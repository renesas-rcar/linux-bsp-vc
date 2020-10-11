/**
    @brief  Renesas VC2 FPGA PLATFORM Device Driver and PCI memory Access Interface

    Provide APIs to read & write 64-bit words of rFabric, System RAM or internal buffer memory
    Initial Driver Load for memory Map

    @bugs
    @todo Bitfile operation still not supported, Flash ready not working
    @notes
    Read Write Function still to be provided
    Need to see if rswitch2 specific ioaddr is needed to be exported
    IOCTL to read and write SFR is there, need to see if still needed
    @author
        Asad Kamal


    Copyright (C) 2020 Renesas Electronics Corporation

    This program is free software; you can redistribute it and/or modify it
    under the terms and conditions of the GNU General Public License,
    version 2, as published by the Free Software Foundation.

    This program is distributed in the hope it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
    more details.
    You should have received a copy of the GNU General Public License along with
    this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.

    The full GNU General Public License is included in this distribution in
    the file called "COPYING".
*/


#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/etherdevice.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/netdevice.h>
#include <linux/of_device.h>
#include <linux/phy.h>
#include <linux/cache.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/ethtool.h>
#include <linux/if_vlan.h>
#include <linux/net_tstamp.h>
#include <linux/semaphore.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/pci.h>
#include <linux/gpio.h>
#include <linux/renesas_vc2_platform.h>
#include <linux/renesas_vc2_usrspace.h>

/* Macros */
#define VC2_FPGA_PLTFRM_PM_OPS	NULL

#define BUF_LEN                   20
#define MAX_FRAME_SIZE            2048
static int vendorid = 0x1172;
module_param(vendorid, int, 0000);
MODULE_PARM_DESC(vendorid, "vendor id of pci");
static int deviceid = 0xe001;
module_param(deviceid, int, 0000);
MODULE_PARM_DESC(deviceid, "device id of pci");

/* Global Variables */
DEFINE_SPINLOCK(vc2_fpga_platform_lock);
static int            vc2_fpga_pltfrm_devmajor;
static struct proc_dir_entry *root_debug_dir;
static struct class * vc2_fpga_pltfrm_devclass = NULL;
static dev_t          vc2_fpga_pltfrm_dev;
static struct cdev    vc2_fpga_pltfrm_cdev;
static struct device  vc2_fpga_pltfrm_device;
const unsigned int    fpga_flash_memory_offset = 0x04000000;
unsigned int          fpga_page1_start_address = 0x01580000;
unsigned int          fpga_update_address      = 0;
static int            minor;
char *                fpga_read_buffer;
static struct pci_driver vc2_fpgapltfrm_driver_pci;
#define VC2_FPGA_PLTFRM_CTRL_MINOR (127)

void __iomem  *ioaddr = 0;
u32  pci_id = 0;
u32  pci_sz = 0;
void __iomem  *ioaddr_pci = 0;
struct pci_dev        *pcidev;
EXPORT_SYMBOL(ioaddr);
EXPORT_SYMBOL(pci_id);
EXPORT_SYMBOL(ioaddr_pci);
EXPORT_SYMBOL(pcidev);

static const struct pci_device_id vc2_fpgapltfrm_pci_tbl[] = {
    { PCI_DEVICE(PCI_ANY_ID, PCI_ANY_ID), },
    { 0, }
};

MODULE_DEVICE_TABLE(pci, vc2_fpgapltfrm_pci_tbl);

static unsigned int bitfile_size = 0;


static void vc2_fpgapltfrm_memcleanup(void)
{
    
    if (ioaddr_pci != 0)
    {
        iounmap(ioaddr_pci);
        ioaddr_pci = 0;
        
    }

    
}

/**
    @brief  Platform Driver Remove Proc directory

    @param  void
    
    @return Return code (< 0 on error)
*/
static void vc2_fpgapltfrm_remove_proc_entry(void)
{

    remove_proc_entry(VC2_FPGA_PROC_FILE_BITFILE_VERSION, root_debug_dir);
    remove_proc_entry(VC2_FPGA_PROC_DIR, NULL);

}


/**
    @brief  Platform Driver Unload

    @param  pdev

    @return Return void
*/
static void vc2_fpgapltfrm_remove_pci(struct pci_dev *pdev)
{

     /* Remove PCI Proc Directory */
    vc2_fpgapltfrm_remove_proc_entry();
    pci_release_regions(pdev);
    pci_disable_device(pdev);
    /* Remove character device*/
    unregister_chrdev_region(vc2_fpga_pltfrm_dev, 1);
    device_del(&vc2_fpga_pltfrm_device);
    cdev_del(&vc2_fpga_pltfrm_cdev);
    class_destroy(vc2_fpga_pltfrm_devclass);
    vc2_fpgapltfrm_memcleanup();
}



/**
    @brief  Platform Driver Memory read

    @param  file
    @param  arg

    @return Return code (< 0 on error)
*/
static long vc2_fpga_pltfrm_ioctl_readmemory(struct file * file, unsigned long arg)
{
    struct vc2_fpga_pltfrm_memory    memory;
    char __user * buf = (char __user *)arg;
    int           err = 0;

    err = copy_from_user(&memory, buf, sizeof(memory));
    if (err != 0) {
        printk(KERN_ERR "vc2_fpga_pltfrm_ioctl_readmemory: copy_from_user returned %d \n", err);
        return -EFAULT;
    }
    if ((memory.address < RSWITCH2_FPGA_IP_BASE) ) {
        printk("Invalid memory address \n");
        return -EFAULT;

    }

    if(memory.raw == 1){
        memory.value = le32_to_cpu(ioread32(ioaddr_pci + ((memory.address))));
    }
    else{
        memory.value = le32_to_cpu(ioread32(ioaddr + ((memory.address - RSWITCH2_FPGA_IP_BASE))));
    }
    if ((err = copy_to_user(buf, &memory, sizeof(memory))) < 0) {
        return -EFAULT;
    }

    return 0;

}


/**
    @brief  Platform Driver Write Memory

    @param  file
    @param  arg

    @return Return code (< 0 on error)
*/
static long vc2_fpga_pltfrm_ioctl_writememory(struct file * file, unsigned long arg)
{
    struct vc2_fpga_pltfrm_memory    memory;
    char __user * buf = (char __user *)arg;
    int           err = 0;
    
    err = copy_from_user(&memory, buf, sizeof(memory));
    if ((memory.address < RSWITCH2_FPGA_IP_BASE) ) {
        printk("Invalid memory address \n");
        return -EFAULT;

    }
    if (err != 0) {
        printk(KERN_ERR "vc2_fpga_pltfrm_ioctl_writememory: copy_from_user returned %d  \n", err);
        return -EFAULT;
    }
    
    if(memory.raw == 1){
        iowrite32(cpu_to_le32(memory.value), ioaddr_pci + (memory.address));
    }
    else{
        iowrite32(cpu_to_le32(memory.value), ioaddr + (memory.address - RSWITCH2_FPGA_IP_BASE));
    }

    return 0;
}

#ifdef BFILE_SUPPORT

/**
    @brief  Platform Driver Bitfile Erase

    @param  void

    @return Return code (< 0 on error)
*/
static int erase_flash(void)
{

    int x = 0;
    int sector_count = 0;

    u32 address = 0;
    u32 value = 0;
    /* For debugging of one access only
    Register_New.address = 0x100500C;
    Register_New.value = (fpga_update_address - 0x04000000) / 0x100 + 0x02;
    Register_New.file_write_enable = 0;
    if ((ret = ioctl(gFDdebug, RSWITCH_DEBUG_IOCTL_WRITEBITFILE, &Register_New)) != 0)
        fprintf(stderr, "\nERROR: RSWITCH_DEBUG_IOCTL_WRITEMEMORY failed(%d) on address %08x: %s \n", ret, Register_New.address, strerror(errno));

    return 0;
    */

    fpga_update_address = fpga_flash_memory_offset + fpga_page1_start_address;
    printk("\tStarting Flash memory Page 1 erasing from address 0x%08x to 0x%08x\n",fpga_page1_start_address, fpga_page1_start_address * 2);
    printk("\tErasing of the flash will take around %3d minutes\n", (((2 * (fpga_page1_start_address / 0x10000)) / 60) + 1));
    for(x = (fpga_update_address - 0x04000000) / 0x100 + 0x02; x < (fpga_update_address - 0x04000000) / 0x100 * 2 + 0x02; x=x+0x100) {
        printk("\t\tErasing Sector 0x%08x",fpga_page1_start_address + sector_count * 0x10000);
        address = 0x100500C;
        value = x;
        iowrite32(value, ioaddr_pci + (address));


        msleep(2000);


        sector_count++;
        //printf(" - Sector Count %4d",sector_count);
        //printf(" - erased part so far %9d (0x%08x) bytes", sector_count * 0x10000, sector_count * 0x10000);
        //printk(" -> %3.2f %% of the requested area is erased    \r",(float) sector_count * 0x10000 / fpga_page1_start_address * 100);
        //fflush(stdout);
    }
    printk("\nErasing Flash finished\n\n");
    return 0;






}
#endif


/**
    @brief  Platform Driver Bitfile write (ioctl)

    @param  buffer
    @param  filesz
    @param  file_op
    @return Return code (< 0 on error)
*/
static int vc2_write_bitfile(char *buffer, u32 filesz, struct vc2_file_op file_op)
{

    int i = 0;
    u32 write_word = 0;
    //u32 read_word = 0;
    //erase_flash();

    if((file_op.bfile_op == VC2_IOCTL_BFILE_OP) || (file_op.file_offset == 0x0)) {
        //already stripped file?
        if (buffer[0x00] == 0x3a && buffer[0x01] == 0x65 && buffer[0x02] == 0x80 && buffer[0x03] == 0x00 &&
            buffer[0x04] == 0x20 && buffer[0x05] == 0x00 && buffer[0x06] == 0x00 && buffer[0x07] == 0x00) {
            printk( "  Detected as stripped binary\n");

            printk(KERN_INFO "  Detected as stripped binary\n");
        }
        //to be stripped
        else if (buffer[0x20] == 0x3a && buffer[0x21] == 0x65 && buffer[0x22] == 0x80 && buffer[0x23] == 0x00 &&
                 buffer[0x24] == 0x20 && buffer[0x25] == 0x00 && buffer[0x26] == 0x00 && buffer[0x27] == 0x00) {
            printk(KERN_INFO "  Detected as un-stripped binary\n");
            printk("  Detected as un-stripped binary\n");
            buffer += 0x20;
        } else {
            printk(KERN_ALERT "ERROR: Unknown binary format (expect 3a 65 80 00 20 00 00 00)\n");
            return -1;
        }
        printk("  Bitfile will be now stored at Flash memory Page 1 on address 0x%08x\n",fpga_page1_start_address);
    }


    for(i = 0; i < ((filesz+3)/4); i ++) {
        write_word =  buffer[(i*4)] | buffer[(i*4) + 1] << 8 | buffer[(i*4) + 2] << 16 | buffer[(i*4) + 3] << 24;



        //iowrite32(cpu_to_le32(write_word), ioaddr_pci + (fpga_update_address + (file_op.file_offset)+(i*4)));
        //read_word = ioread32(ioaddr_pci + (fpga_update_address + (file_op.file_offset)+(i*4)));
        //if(read_word == write_word)
        //{
        //  printk(KERN_ALERT "Data write at byte %d  failed\n", (file_op.file_offset)+(i*4));
        //return -1;

        //}
    }
    if((file_op.bfile_op == VC2_IOCTL_BFILE_OP) || (file_op.file_end == 0x1)) {
        printk("  Bitfile stored, file_op.bfile_op = %d, file_op.file_end=%d\n",file_op.bfile_op, file_op.file_end);
    }


    return 0;

}

/**
    @brief  Platform Driver Bitfilewrite(Future Use)

    @param  file    Device File
    @param  arg     User Buffer

    @return Return code (< 0 on error)
*/

static long vc2_fpga_pltfrm_ioctl_writebitfilememory(struct file * file, unsigned long arg)
{
    struct vc2_fpga_pltfrm_memory    memory;
    char __user * buf = (char __user *)arg;
    int           err = 0;
    //unsigned char *buffer;
    struct vc2_file_op file_op;
    err = copy_from_user(&memory, buf, sizeof(memory));


    if (err != 0) {
        printk(KERN_ERR "vc2_fpga_pltfrm_ioctl_writememory: copy_from_user returned %d  \n", err);
        return -EFAULT;
    }


    //buffer = (char*)kmalloc(memory.file_size * sizeof(char), GFP_KERNEL);
    //memcpy(buffer, memory.bitfile,memory.file_size );
    file_op.bfile_op = 0;
    file_op.file_offset = 0;
    file_op.file_end = 0x0;
    err = vc2_write_bitfile(memory.bitfile, memory.file_size, file_op );
    if(err < 0) {
        printk(KERN_ERR "Bitfile Write Failed \n");
        return -1;
    }
    bitfile_size = memory.file_size;

    return 0;


}

/**
    @brief  Device Open

    @return < 0 on error
*/
static int vc2_fpga_platform_open(struct inode *inode, struct file* file)
{
    minor = MINOR(file->f_inode->i_rdev);

    if (minor < 0) {
        printk("[VC-FPGA-PLATFORM] Invalid Minor Number\n");
        return -ENODEV;
    }

    //printk("[[VC-FPGA-PLATFORM] Device Opened with minor number %d \n", minor);

    return 0;
}



/**
    @brief  Device Close

    @return < 0 on error
*/
static int vc2_fpga_platform_release(struct inode *inode, struct file* file)
{
#ifdef DEBUG_PRINT
    printk("[VC-FPGA-PLATFORM] Device CLOSED \n");
#endif



    return 0;
}


/**
    @brief  IOCTLs

    @return < 0, or 0 on success
*/
static long vc2_fpga_pltfrm_ioctl(struct file * file, unsigned int cmd, unsigned long arg)
{
    int ret = 0;

    switch (cmd) {
    case VC2_FPGA_PLTFRM_IOCTL_READMEMORY:
        return vc2_fpga_pltfrm_ioctl_readmemory(file, arg);
    case VC2_FPGA_PLTFRM_IOCTL_WRITEMEMORY:
        return vc2_fpga_pltfrm_ioctl_writememory(file, arg);
    case VC2_FPGA_PLTFRM_IOCTL_WRITEBITFILE:
        return vc2_fpga_pltfrm_ioctl_writebitfilememory(file,arg);
    default:
        pr_err("[VC_FPGA_PLTFRM] IOCTL Unknown: 0x%08X\n", cmd);
        ret = -EINVAL;
    }

    return ret;
}


/**
    @brief  Send a bitfile to FPGA

    @param  filep    file pointer
    @param  buffer   pointer to the buffer holding data
    @param  len      size of the buffer
    @param  offset

    @return Number of bytes sent for transmission, or < 0 on error
*/
static ssize_t vc2_fpga_platform_write(struct file * filep, const char * buffer, size_t len, loff_t * offset)
{
    int error_count = 0;
    int ret = 0;
    char *send_data;
    unsigned long    flags;
    unsigned int i  = * offset;

    struct vc2_file_op file_op;
    send_data = kmalloc(len, GFP_ATOMIC);
    printk("vc_fpga_platform_write called \n");
    if (send_data == NULL) {
        printk(KERN_INFO "[VC-FPGA-PLATFORM] Failed to allocate %lu bytes for send buffer\n", len);
        return -ENOMEM;
    }

    memset(&file_op, 0, sizeof(struct vc2_file_op));

    /* Copy to FPGA Area */
    error_count = copy_from_user(send_data, buffer, len);
    if (error_count == 0) {
        printk(KERN_INFO "[VC-FPGA-PLATFORM] Received %ld characters from the user with offset %d, buffer[len -1]=%x\n", len, (int)*offset,buffer[len -1]);
        i +=len;
        if(buffer[len -1] == 0xFF) {
            file_op.file_end = 1;
            bitfile_size = i;

        }
        file_op.bfile_op = VC2_FILEWR_BFILE_OP;
        file_op.file_offset = *offset;
        //Put Spin lock, check if any better mechanism Critical Section start
        spin_lock_irqsave(&vc2_fpga_platform_lock, flags);
        ret = vc2_write_bitfile((char *)buffer, len, file_op);
        spin_unlock_irqrestore(&vc2_fpga_platform_lock, flags);
        //Critical section end
        if(ret < 0) {
            printk(KERN_ALERT "[VC-FPGA-PLATFORM] FPGA Write Failed \n");
            return -EFAULT;
        }
        *offset = i;
#ifdef DEBUG_PRINT
        int i = 0;
        printk(KERN_INFO "[VC-FPGA-PLATFORM] Received %d characters from the user\n", len);
        for (i = 0; i < len; i++) {
            printk("[VC-FPGA-PLATFORM] Characters Received = %0x \n", send_data[i]);
        }
#endif
    } else {
        printk(KERN_INFO "[VC-FPGA-PLATFORM] Write Failed - copy_from_user error %d\n", error_count);
        return -EFAULT;
    }

    //ret = vc2_fpga_platform_write_data(send_data, len);
    if (ret < 0) {
        printk(KERN_INFO "[VC-FPGA-PLATFORM] Cannot Write Data \n");
        return -EFAULT;
    }

    return len;
}



/**
    @brief  Read a bitfile to FPGA

    @param  filep    file pointer
    @param  buff   pointer to the buffer holding data
    @param  len      size of the buffer
    @param  offset

    @return Number of bytes read, or < 0 on error
*/
static ssize_t vc2_fpga_platform_read(struct file * filep, char * buff, size_t len, loff_t * offset)
{

    unsigned int     num_bytes_read = 0;
    int              error_count = 0;
    unsigned long    flags;
    u32 i = 0;
    u32 j = 0;
    u32 read_word = 0;

#ifdef DEBUG_PRINT
    printk("Inside Read \n");

#endif


    fpga_read_buffer = (char *) kmalloc(bitfile_size * sizeof(char), GFP_KERNEL);
    spin_lock_irqsave(&vc2_fpga_platform_lock, flags);

    for(i = 0; i < ((bitfile_size+3)/4); i ++) {

        //read_word = ioread32(ioaddr_pci + (fpga_update_address + (i*4)));
        for(j = 0; j < 4; j++) {
            fpga_read_buffer[(i*4) + j] = ((read_word >> (j*8)) & 0XFF);
        }
    }

    /* Use memcpy to fill Rx buffer from FPGA */
    //error_count = copy_to_user(buff, &fpga_read_buffer, sizeof(fpga_read_buffer));
    if (error_count == 0) {
#ifdef DEBUG_PRINT
        printk(KERN_INFO "[VC-FPGA-PLATFORM] Sent %d characters to the user\n", num_bytes_read);
#endif
    } else {
        printk(KERN_INFO "[VC-FPGA-PLATFORM] Read Failed - copy_to_user error %d\n", error_count);
        return -EFAULT;              // Failed -- return a bad address message (i.e. -14)
    }


    spin_unlock_irqrestore(&vc2_fpga_platform_lock, flags);

    return num_bytes_read;
}



const struct file_operations    vc2_fpga_pltfrm_fileops = {
    .owner            = THIS_MODULE,
    .open             = vc2_fpga_platform_open,
    .release          = vc2_fpga_platform_release,
    .unlocked_ioctl   = vc2_fpga_pltfrm_ioctl,
    .compat_ioctl     = vc2_fpga_pltfrm_ioctl,
    .read             = vc2_fpga_platform_read,
    .write            = vc2_fpga_platform_write,
};







/**
    @brief  Add character device

    @return < 0, or 0 on success
*/
static int _probe_chrdev(void)
{
    struct device *dev;
    int            ret = 0;

    /*
        Create class
    */
    vc2_fpga_pltfrm_devclass = class_create(THIS_MODULE, VC2_FPGA_PLTFRM_CLASS);
    if (IS_ERR(vc2_fpga_pltfrm_devclass)) {
        ret = PTR_ERR(vc2_fpga_pltfrm_devclass);
        pr_err("[VC-FPGA-PLTFRM] failed to create '%s' class. rc=%d\n", VC2_FPGA_PLTFRM_CLASS, ret);
        return ret;
    }

    if (vc2_fpga_pltfrm_devmajor != 0) {
        vc2_fpga_pltfrm_dev = MKDEV(vc2_fpga_pltfrm_devmajor, 0);
        ret = register_chrdev_region(vc2_fpga_pltfrm_dev, 1, VC2_FPGA_PLTFRM_CLASS);
    } else {
        ret = alloc_chrdev_region(&vc2_fpga_pltfrm_dev, 0, 1, VC2_FPGA_PLTFRM_CLASS);
    }
    if (ret < 0) {
        pr_err("[VC-FPGA-PLTFRM] failed to register '%s' character device. rc=%d\n", VC2_FPGA_PLTFRM_CLASS, ret);
        class_destroy(vc2_fpga_pltfrm_devclass);
        return ret;
    }
    vc2_fpga_pltfrm_devmajor = MAJOR(vc2_fpga_pltfrm_dev);

    cdev_init(&vc2_fpga_pltfrm_cdev, &vc2_fpga_pltfrm_fileops);
    vc2_fpga_pltfrm_cdev.owner = THIS_MODULE;

    ret = cdev_add(&vc2_fpga_pltfrm_cdev, vc2_fpga_pltfrm_dev, VC2_FPGA_PLTFRM_CTRL_MINOR + 1);
    if (ret < 0) {
        pr_err("[VC-FPGA-PLTFRM] failed to add '%s' character device. rc=%d\n", VC2_FPGA_PLTFRM_CLASS, ret);
        unregister_chrdev_region(vc2_fpga_pltfrm_dev, 1);
        class_destroy(vc2_fpga_pltfrm_devclass);
        return ret;
    }

    /* device initialize */
    dev = &vc2_fpga_pltfrm_device;
    device_initialize(dev);
    dev->class  = vc2_fpga_pltfrm_devclass;
    dev->devt   = MKDEV(vc2_fpga_pltfrm_devmajor, VC2_FPGA_PLTFRM_CTRL_MINOR);
    dev_set_name(dev, VC2_FPGA_PLTFRM_DEVICE_NAME);

    ret = device_add(dev);
    if (ret < 0) {
        pr_err("[VC-FPGA-PLTFRM] failed to add '%s' device. rc=%d\n", VC2_FPGA_PLTFRM_DEVICE_NAME, ret);
        cdev_del(&vc2_fpga_pltfrm_cdev);
        unregister_chrdev_region(vc2_fpga_pltfrm_dev, 1);
        class_destroy(vc2_fpga_pltfrm_devclass);
        return ret;
    }

    return ret;
}





/**
    @brief  Platform Driver Bitfile version Show

    @param  m
    @param  v

    @return Return code (< 0 on error)
*/
static int vc2_fpgapltfrm_bitfileversion_show(struct seq_file * m, void * v)
{
    seq_printf(m, "Platform Driver S/W Version: %s\n", VC2_FPGA_PLTFRM_DRIVER_VERSION);
    seq_printf(m, "Build on %s at %s\n", __DATE__, __TIME__);
    seq_printf(m, "Bitfile version %x\n",ioread32((ioaddr_pci) + 0x01000000 + 0x4D40));
    seq_printf(m, "RTL version %x\n",ioread32((ioaddr_pci) + 0x01000000 + 0x4D48));
    seq_printf(m, "PCI-ID  0x%x\n",pci_id);
    seq_printf(m, "PCI-SIZE  0x%x\n",pci_sz);

    return 0;
}

/**
    @brief  Platform Driver Bitfile version Open

    @param  inode
    @param  file

    @return Return code (< 0 on error)
*/
static int vc2_fpgapltfrm_driverbitfileversion_open(struct inode * inode, struct  file * file) 
{
    return single_open(file, vc2_fpgapltfrm_bitfileversion_show, NULL);
}


static const struct file_operations     vc2_fpgapltfrm_driver_fops = 
{
    .owner   = THIS_MODULE,
    .open    =  vc2_fpgapltfrm_driverbitfileversion_open,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = single_release,
};




/**
    @brief  Platform Driver Proc Initialise

    @param  void   
    

    @return Return code (< 0 on error)
*/
static int vc2_fpgapltfrm_proc_initialise(void)
{
    root_debug_dir = proc_mkdir(VC2_FPGA_PROC_DIR, NULL);
    proc_create(VC2_FPGA_PROC_FILE_BITFILE_VERSION, 0, root_debug_dir, &vc2_fpgapltfrm_driver_fops);
    return 0;
}



/**
    @brief  PCI init function

    @param  dev
    @param  id


    @return  < 0 on error
*/
static int vc2_fpgapltfrm_init_pci(struct pci_dev *dev, const struct pci_device_id *id)
{

    int rc = 0;
    const int region = 0;
    u32 *ptr = NULL;
    printk("\n[VC-FPGA-PLTFRM] (%s %s) version %s, Probing '%s' .....\n", __DATE__, __TIME__, VC2_FPGA_PLTFRM_DRIVER_VERSION, DRV_NAME);
    if((dev->vendor == vendorid) && ((dev->device == deviceid)|| ((dev->device >= 0xE000) && (dev->device <= 0xE0FF)))) {
        rc = pci_enable_device(dev);
        if (rc) {
            dev_err(&dev->dev, "pci_enable device() failed\n");
            goto err_out;
        }
        dev_info(&dev->dev, "Found and enabled PCI device with "
                 "VID 0x%04X, DID 0x%04X\n", dev->vendor, dev->device);
        if (pci_set_mwi(dev) < 0) {
            goto err_out;
        }
        pci_set_master(dev);

        /* Make sure PCI base addr 1 is MMIO */
        if (!(pci_resource_flags(dev, region) & IORESOURCE_MEM)) {
            printk(
                "region %d not an MMIO resource, aborting\n", region);
            return -ENODEV;

        }
        /*Request PCI Region for FPGA address*/
        rc = pci_request_regions(dev, DRV_NAME);
        if (rc) {
            dev_err(&dev->dev, "pci_request_regions() failed\n");
            goto err_out_pci_disable;
        }

        /* Get virtual address */
        pci_sz = pci_resource_len(dev, 0);
        ioaddr_pci = ioremap_nocache(pci_resource_start(dev, 0),
                                     pci_sz);

        if (!ioaddr_pci) {
            rc = -EIO;
            dev_err(&dev->dev, "ioremap failed\n");
            goto err_out_free_res;
        }
        
        pci_set_dma_mask(dev, DMA_BIT_MASK(64));
        /* Specific for rswitch2 */
        ioaddr = ioaddr_pci + RSWITCH2_FPGA_PCI_OFFSET;

        ptr = ioaddr_pci;
        ptr[0x1000/sizeof(u32)] = 0x00000000;
        ptr[0x1008/sizeof(u32)] = 0x40000000;
    }else {
        dev_err(&dev->dev, "This PCI device does not match "
        "VID 0x%04X, DID 0x%04X probed VID 0x%04X, DID 0x%04X\n", vendorid, deviceid, dev->vendor, dev->device);
        rc = -ENODEV;
        dev_err(&dev->dev, "PCI match failed\n");
        goto err_out;
    }

    /*
       Add character device for IOCTL operations
    */
    if ((rc = _probe_chrdev()) < 0) {
        goto err_out;
    }
    pcidev = dev;
    pci_id = dev->device;
    vc2_fpgapltfrm_proc_initialise();
    return rc;
err_out_free_res:
    pci_release_regions(dev);
err_out_pci_disable:
    pci_disable_device(dev);
err_out:
    return rc;
}












static struct pci_driver vc2_fpgapltfrm_driver_pci = {
    .name		= DRV_NAME,
    .id_table	        = vc2_fpgapltfrm_pci_tbl,
    .probe		= vc2_fpgapltfrm_init_pci,
    .remove		= vc2_fpgapltfrm_remove_pci,
    .driver.pm	        = VC2_FPGA_PLTFRM_PM_OPS,
};




/**
    @brief  Module init function

    @param  void

    @return  < 0 on error
*/
static int __init vc2_fpgapltfrm_init(void)
{
    int ret_pci;

    ret_pci = pci_register_driver(&vc2_fpgapltfrm_driver_pci);
    if ((ret_pci < 0) /*&& (ret_platform < 0)*/) {
        return ret_pci;
    }
    if(pcidev == NULL) {
        pci_unregister_driver(&vc2_fpgapltfrm_driver_pci);
        return -ENODEV;
    }
    return 0;
}

/**
    @brief  Module exit function

    @param  void

    @return  void
*/
static void __exit vc2_fpgapltfrm_cleanup(void)
{

    pci_unregister_driver(&vc2_fpgapltfrm_driver_pci);
}


module_init(vc2_fpgapltfrm_init);
module_exit(vc2_fpgapltfrm_cleanup);

MODULE_AUTHOR("Asad Kamal");
MODULE_DESCRIPTION("Renesas VC FPGA PLATFORM Driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(__DATE__"-"__TIME__);

/*
    Change History
    2020-08-10    AK  Initial Version(WIP No Bitfile Program support only PCI init)
    2020-08-10    AK  Raw Reading for Port Version 
    2020-08-28    AK  Added Proc statistics, module parameter for vendor id & device id 
    2020-09-03    AK  Added check for range of pci id, pci id to proc and export for further checking  driver name changed
    2020-09-30    AK  Module automatic unload on failed probe added
    2020-10-07    AK  Root Proc directory separated
*/


