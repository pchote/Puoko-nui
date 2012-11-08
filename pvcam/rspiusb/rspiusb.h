/* piusb.h */

#include <linux/ioctl.h>

#define to_pi_dev(d) container_of(d, struct rspiusb, kref)

#define PIUSB_MAGIC         'm'
#define PIUSB_IOCTL_BASE    192
#define PIUSB_GETVNDCMD     _IOR(PIUSB_MAGIC, PIUSB_IOCTL_BASE + 1, struct ioctl_data)
#define PIUSB_SETVNDCMD     _IOW(PIUSB_MAGIC, PIUSB_IOCTL_BASE + 2, struct ioctl_data)
#define PIUSB_WRITEPIPE     _IOW(PIUSB_MAGIC, PIUSB_IOCTL_BASE + 3, struct ioctl_data)
#define PIUSB_READPIPE      _IOR(PIUSB_MAGIC, PIUSB_IOCTL_BASE + 4, struct ioctl_data)
#define PIUSB_SETFRAMESIZE  _IOW(PIUSB_MAGIC, PIUSB_IOCTL_BASE + 5, struct ioctl_data)
#define PIUSB_WHATCAMERA    _IO(PIUSB_MAGIC,  PIUSB_IOCTL_BASE + 6)
#define PIUSB_USERBUFFER    _IOW(PIUSB_MAGIC, PIUSB_IOCTL_BASE + 7, struct ioctl_data)
#define PIUSB_ISHIGHSPEED   _IO(PIUSB_MAGIC,  PIUSB_IOCTL_BASE + 8)
#define PIUSB_UNMAP_USERBUFFER  _IOW(PIUSB_MAGIC, PIUSB_IOCTL_BASE + 9, struct ioctl_data)

/* Version Information */
#define DRIVER_VERSION "V1.0.3"
#define DRIVER_AUTHOR  "Princeton Instruments"
#define DRIVER_DESC    "PI USB2.0 Device Driver for Linux"

/* Define these values to match your devices */
#define VENDOR_ID   0x0BD7
#define ST133_PID   0xA010
#define PIXIS_PID   0xA026

/* Get a minor range for your devices from the usb maintainer */
#ifdef CONFIG_USB_DYNAMIC_MINORS
#define PIUSB_MINOR_BASE    0
#else
#define PIUSB_MINOR_BASE    192
#endif

/* prevent races between open() and disconnect() */
static DEFINE_SEMAPHORE(disconnect_sem);
static DEFINE_MUTEX(piusb_mutex);

/* Structure to hold all of our device specific stuff */
struct rspiusb {
    struct usb_device*      udev;           /* save off the usb device pointer */
    struct usb_interface*   interface;      /* the interface for this device */
    unsigned char           minor;          /* the starting minor number for this device */
    size_t                  bulk_in_size_returned;
    int                     bulk_in_byte_trk;
    struct urb***           PixelUrb;
    int                     frameIdx;
    int                     urbIdx;
    unsigned int*           maplist_numPagesMapped;
    int                     open;           /* if the port is open or not */
    int                     present;        /* if the device is not disconnected */
    int                     userBufMapped;  /* has the user buffer been mapped? */
    struct scatterlist**    sgl;            /* scatter-gather list for user buffer */
    unsigned int*           sgEntries;
    struct  kref            kref;
    int                     gotPixelData;
    int                     pendingWrite;
    char**                  pendedPixelUrbs;
    int                     iama;           /* PIXIS or ST133 */
    int                     num_frames;     /* the number of frames that will fit in the user buffer */
    int                     active_frame;
    unsigned long           frameSize;
    struct semaphore        sem;

    /* FX2 specific endpoints */
    unsigned int            hEP[8];
};

struct ioctl_data {
    unsigned char  cmd;
    unsigned long  numbytes;
    unsigned char  dir; /* 1=out; 0=in */
    int            endpoint;
    int            numFrames;
    unsigned char __user *pData;
};

static long piusb_unlocked_ioctl(struct file *f, unsigned cmd, unsigned long arg);
static int 	piusb_open          (struct inode *inode, struct file *file);
static int 	piusb_release       (struct inode *inode, struct file *file);
static int  piusb_probe         (struct usb_interface *interface, const struct usb_device_id *id);
static void piusb_disconnect    (struct usb_interface *interface);

static struct file_operations piusb_fops = {
	.owner = THIS_MODULE,
	.open = piusb_open,
	.unlocked_ioctl = piusb_unlocked_ioctl,
	.release = piusb_release,
};

/* 
 * usb class driver info in order to get a minor number from the usb core,
 * and to have the device registered with devfs and the driver core
 */
static struct usb_class_driver piusb_class = {
	.name = "usb/rspiusb%d",
	.fops = &piusb_fops,
	.minor_base = PIUSB_MINOR_BASE,
};

/* Table of devices that work with this driver */
static struct usb_device_id pi_device_table [] = {
	{USB_DEVICE(VENDOR_ID, ST133_PID)},
	{USB_DEVICE(VENDOR_ID, PIXIS_PID)},
	{}
};
MODULE_DEVICE_TABLE(usb, pi_device_table);

/* usb specific object needed to register this driver with the usb subsystem */
static struct usb_driver piusb_driver = {
	.name =		    "RSPIUSB",
	.probe =	    piusb_probe,
	.disconnect =	piusb_disconnect,
	.id_table =	    pi_device_table,
};
