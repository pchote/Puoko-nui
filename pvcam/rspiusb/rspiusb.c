/*
 * rspiusb.c
 *
 * Copyright (C) 2005, 2006 Princeton Instruments
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation version 2 of the License
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/vmalloc.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/completion.h>
#include <asm/uaccess.h>
#include <linux/usb.h>
#include <asm/scatterlist.h>
#include <linux/mm.h>
#include <linux/pci.h> //for scatterlist macros
#include <linux/pagemap.h>

#if HAVE_UNLOCKED_IOCTL
#include <linux/mutex.h>
#else
#include <linux/smp_lock.h>
#endif

#include "rspiusb.h"

static void piusb_delete(struct kref *);
static int  piusb_getvndcmd(struct rspiusb *dev, struct ioctl_data __user *arg);
static int  piusb_setvndcmd(struct rspiusb *dev, struct ioctl_data __user *arg);
static int  piusb_writepipe(struct rspiusb *dev, struct ioctl_data __user *arg);
static int  piusb_readpipe(struct rspiusb *dev, struct ioctl_data __user *arg);
static int  piusb_setframesize(struct rspiusb *dev, struct ioctl_data __user *arg);
static int  piusb_unmap_user_buffer(struct rspiusb *);
static int  piusb_map_user_buffer(struct rspiusb *dev, struct ioctl_data __user *arg);
static void piusb_write_bulk_callback(struct urb *);
static void piusb_read_pixel_callback(struct urb *);

static int lastErr = 0;
static int errCnt = 0;


/**
 *  piusb_probe
 *
 *  Called by the usb core when a new device is connected that it thinks
 *  this driver might be interested in.
 */
static int piusb_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
    struct rspiusb *dev = NULL;
    struct usb_host_interface *iface_desc;
    struct usb_endpoint_descriptor *endpoint;
    int i;
    int retval = -ENOMEM;

    pr_debug("RSPIUSB: Looking for PI USB Hardware\n");

    dev = kmalloc(sizeof(struct rspiusb), GFP_KERNEL);
    if (dev == NULL)
    {
        dev_err(&interface->dev, "Out of memory\n");
        goto error;
    }

    memset(dev, 0x00, sizeof(*dev));
    kref_init(&dev->kref);
    dev->udev = usb_get_dev(interface_to_usbdev(interface));
    dev->interface = interface;
    iface_desc = interface->cur_altsetting;

    /* See if the device offered us matches what we can accept */
    if ((dev->udev->descriptor.idVendor != VENDOR_ID) || (dev->udev->descriptor.idProduct != PIXIS_PID &&
                                                          dev->udev->descriptor.idProduct != ST133_PID))
    {
        return -ENODEV;
    }
    dev->iama = dev->udev->descriptor.idProduct;

    if (dev->iama == PIXIS_PID)
        dev_dbg(&interface->dev, "Pixis Camera Found\n");
    else
        dev_dbg(&interface->dev, "ST133 USB Controller Found\n");

    if (dev->udev->speed == USB_SPEED_HIGH)
        dev_dbg(&interface->dev, "Highspeed(USB2.0) Device Attached\n");
    else
        dev_dbg(&interface->dev, "Lowspeed (USB1.1) Device Attached\n");

    for (i = 0; i < iface_desc->desc.bNumEndpoints; ++i)
    {
        endpoint = &iface_desc->endpoint[i].desc;

        if ((endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) == USB_ENDPOINT_XFER_BULK)
        {
            if (endpoint->bEndpointAddress & USB_DIR_IN)
                dev->hEP[i] = usb_rcvbulkpipe(dev->udev, endpoint->bEndpointAddress);
            else
                dev->hEP[i] = usb_sndbulkpipe(dev->udev, endpoint->bEndpointAddress);
        }
    }

    usb_set_intfdata(interface, dev);
    retval = usb_register_dev(interface, &piusb_class);
    if (retval)
    {
        dev_err(&interface->dev, "Not able to get a minor for this device.\n");
        usb_set_intfdata( interface, NULL );
        goto error;
    }
    dev->present = 1;

    /* We can register the device now, as it is ready */
    dev->minor = interface->minor;

    /* Let the user know what node this device is now attached to */
    dev_dbg(&interface->dev, "PI USB2.0 device now attached to rspiusb-%d\n", dev->minor);
    return 0;

error:
    if (dev)
        kref_put(&dev->kref, piusb_delete);
    return retval;
}

static void piusb_delete(struct kref *kref)
{
    struct rspiusb *dev = to_pi_dev(kref);

    usb_put_dev(dev->udev);
    kfree(dev);
}

static int piusb_open(struct inode *inode, struct file *file)
{
    struct rspiusb *dev = NULL;
    struct usb_interface *interface;
    int subminor;

    subminor = iminor(inode);
    interface = usb_find_interface(&piusb_driver, subminor);
    if (!interface)
    {
        pr_err("RSPIUSB: Can't find device for minor %d\n", subminor);
        return -ENODEV;
    }

    dev = usb_get_intfdata(interface);
    if (!dev)
        return -ENODEV;

    dev->frameIdx = 0;
    dev->urbIdx = 0;
    dev->gotPixelData = 0;
    dev->pendingWrite = 0;
    dev->frameSize = 0;
    dev->num_frames = 0;
    dev->active_frame = 0;
    dev->bulk_in_byte_trk = 0;
    dev->userBufMapped = 0;
    dev->pendedPixelUrbs = NULL;
    dev->sgEntries = NULL;
    dev->sgl = NULL;
    dev->maplist_numPagesMapped = NULL;
    dev->PixelUrb = NULL;
    dev->bulk_in_size_returned = 0;

    /* Increment our usage count for the device */
    kref_get(&dev->kref);

    /* Save our object in the file's private structure */
    file->private_data = dev;

    return 0;
}

static int piusb_release(struct inode *inode, struct file *file)
{
    struct rspiusb *dev;
    int retval = 0;

    dev = (struct rspiusb *)file->private_data;
    if (dev == NULL)
        return -ENODEV;

    /* Decrement the count on our device */
    kref_put(&dev->kref, piusb_delete);
    return retval;
}

static int piusb_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
    struct rspiusb *dev;
    int retval = 0;
    struct ioctl_data ctrl;

    dev = (struct rspiusb *)file->private_data;

    /* Verify that the device wasn't unplugged */
    if (!dev->present)
    {
        printk(KERN_ERR "No Device Present\n");
        return -ENODEV;
    }

    switch (cmd)
    {
        case PIUSB_GETVNDCMD:
            return piusb_getvndcmd(dev, (struct ioctl_data __user *)arg);

        case PIUSB_SETVNDCMD:
            return piusb_setvndcmd(dev, (struct ioctl_data __user *)arg);

        case PIUSB_ISHIGHSPEED:
            return ((dev->udev->speed == USB_SPEED_HIGH) ? 1 : 0);

        case PIUSB_WRITEPIPE:
            /* Return value ignored */
            piusb_writepipe(dev, (struct ioctl_data __user *)arg);
            return 0;

        case PIUSB_USERBUFFER:
            return piusb_map_user_buffer(dev, (struct ioctl_data __user *)arg);

        case PIUSB_UNMAP_USERBUFFER:
            /* Return value ignored */
            piusb_unmap_user_buffer(dev);
            return 0;

        case PIUSB_READPIPE:
            return piusb_readpipe(dev, (struct ioctl_data __user *)arg);

        case PIUSB_WHATCAMERA:
            return dev->iama;

        case PIUSB_SETFRAMESIZE:
            if (copy_from_user(&ctrl, (void __user*)arg, sizeof(ctrl)))
            {
                dev_err(&dev->interface->dev, "PIUSB_SETFRAMESIZE: copy_from_user failed\n");
                return -EFAULT;
            }

            dev->frameSize = ctrl.numbytes;
            dev->num_frames = ctrl.numFrames;
            if (!dev->sgl)
                dev->sgl = kmalloc(sizeof(struct scatterlist *) * dev->num_frames, GFP_KERNEL);
            if (!dev->sgEntries)
                dev->sgEntries = kmalloc(sizeof(unsigned int) * dev->num_frames, GFP_KERNEL);
            if (!dev->PixelUrb)
                dev->PixelUrb = kmalloc(sizeof(struct urb **) * dev->num_frames, GFP_KERNEL);
            if (!dev->maplist_numPagesMapped)
                dev->maplist_numPagesMapped = vmalloc(sizeof(unsigned int) * dev->num_frames);
            if (!dev->pendedPixelUrbs)
                dev->pendedPixelUrbs = kmalloc(sizeof(char *) * dev->num_frames, GFP_KERNEL);

            return 0;

        default:
            dev_dbg(&dev->interface->dev, "Unknown ioctl: %d\n", cmd);
            break;
    }

    /* Unknown ioctl */
    dev_err(&dev->interface->dev, "Returning -ENOTTY\n");
    return -ENOTTY;
}

/**
 *  piusb_disconnect
 *
 *  Called by the usb core when the device is removed from the system.
 *
 *  This routine guarantees that the driver will not submit any more urbs
 *  by clearing dev->udev.  It is also supposed to terminate any currently
 *  active urbs.  Unfortunately, usb_bulk_msg(), used in piusb_read(), does
 *  not provide any way to do this.  But at least we can cancel an active
 *  write.
 */
static void piusb_disconnect(struct usb_interface *interface)
{
    struct rspiusb *dev;
    int minor = interface->minor;

#if HAVE_UNLOCKED_IOCTL
    mutex_lock(&piusb_mutex);
#else
    lock_kernel();
#endif
    dev = usb_get_intfdata(interface);
    usb_set_intfdata(interface, NULL);

    /* Give back our minor */
    usb_deregister_dev(interface, &piusb_class);

#if HAVE_UNLOCKED_IOCTL
    mutex_unlock(&piusb_mutex);
#else
    unlock_kernel();
#endif

    /* Prevent device read, write and ioctl */
    dev->present = 0;
    kref_put(&dev->kref, piusb_delete);
    pr_debug("RSPIUSB: PI USB2.0 device #%d now disconnected\n", minor);
}

/**
 *  piusb_init
 */
static int __init piusb_init(void)
{
    int result;

    result = usb_register(&piusb_driver);
    if (result)
    {
        pr_err("RSPIUSB: usb_register failed. Error number %d\n", result);
        return result;
    }

    pr_info("RSPIUSB: %s %s", DRIVER_DESC, DRIVER_VERSION);
    return 0;
}

/**
 *  piusb_exit
 */
static void __exit piusb_exit(void)
{
    usb_deregister(&piusb_driver);
}

static int piusb_getvndcmd(struct rspiusb *dev, struct ioctl_data __user *arg)
{
    unsigned long devRB = 0;
    struct ioctl_data ctrl;

    if (copy_from_user(&ctrl, arg, sizeof(ctrl)))
    {
        dev_err(&dev->interface->dev, "PIUSB_GETVNDCMD: copy_from_user failed\n");
        return -EFAULT;
    }

    dev_dbg(&dev->interface->dev, "Get Vendor Command = %x\n", ctrl.cmd);
    usb_control_msg(dev->udev, usb_rcvctrlpipe(dev->udev, 0), ctrl.cmd,
                    USB_DIR_IN, 0, 0, &devRB, ctrl.numbytes, HZ*10);

    return devRB;
}

static int piusb_setvndcmd(struct rspiusb *dev, struct ioctl_data __user *arg)
{
    char dummyCtlBuf[] = {0,0,0,0,0,0,0,0};
    struct ioctl_data ctrl;
    unsigned short controlData = 0;
    unsigned char data[2];

    if (copy_from_user(&ctrl, arg, sizeof(ctrl)))
    {
        dev_err(&dev->interface->dev, "PIUSB_SETVNDCMD: copy_from_user failed\n");
        return -EFAULT;
    }

    dev_dbg(&dev->interface->dev, "Set Vendor Command = %x\n", ctrl.cmd);
    if (copy_from_user(&data, ctrl.pData, sizeof(data)))
    {
        dev_err(&dev->interface->dev, "PIUSB_SETVNDCMD: copy_from_user pData failed\n");
        return -EFAULT;
    }
    controlData = data[0];
    controlData |= ( data[1] << 8 );

    dev_dbg(&dev->interface->dev, "Vendor Data = %d\n", controlData);
    return usb_control_msg(dev->udev,
                           usb_sndctrlpipe(dev->udev, 0),
                           ctrl.cmd,
                           (USB_DIR_OUT | USB_TYPE_VENDOR ),/* | USB_RECIP_ENDPOINT), */
                           controlData,
                           0,
                           &dummyCtlBuf,
                           ctrl.numbytes,
                           HZ*10);
}

static int piusb_writepipe(struct rspiusb *dev, struct ioctl_data __user *arg)
{
    struct ioctl_data ctrl;
    struct urb *urb = NULL;
    int err = 0;
    unsigned char *kbuf = NULL;

    if (copy_from_user(&ctrl, arg, sizeof(ctrl)))
    {
        dev_err(&dev->interface->dev, "PIUSB_WRITEPIPE: copy_from_user failed\n");
        return -EFAULT;
    }

    urb = usb_alloc_urb(0, GFP_KERNEL);
    if (urb != NULL)
    {
        kbuf = kmalloc(ctrl.numbytes, GFP_KERNEL);
        if (kbuf == NULL)
        {
            dev_err(&dev->interface->dev, "kmalloc failed for pisub_output\n");
            return -ENOMEM;
        }

        if (copy_from_user(kbuf, ctrl.pData, ctrl.numbytes))
        {
            dev_err(&dev->interface->dev, "copy_from_user failed for pisub_output\n");
            return -EFAULT;
        }

        usb_fill_bulk_urb(urb, dev->udev, dev->hEP[ctrl.endpoint], kbuf, ctrl.numbytes, piusb_write_bulk_callback, dev);

        err = usb_submit_urb(urb, GFP_KERNEL);
        if (err)
            dev_err(&dev->interface->dev, "WRITE ERROR:submit urb error = %d\n", err);

        dev->pendingWrite = 1;
        usb_free_urb(urb);
    }

    return -EINPROGRESS;
}

static int piusb_readpipe(struct rspiusb *dev, struct ioctl_data __user *arg)
{
    struct ioctl_data ctrl;
    unsigned char *uBuf;
    int numbytes = 0;
    int i = 0;

    if (copy_from_user(&ctrl, (void __user*)arg, sizeof(ctrl)))
    {
        dev_err(&dev->interface->dev, "PIUSB_READPIPE: copy_from_user failed\n");
        return -EFAULT;
    }

    switch (ctrl.endpoint)
    {
        case 0: /* ST133 Pixel Data or PIXIS IO */
            if (dev->iama == PIXIS_PID)
            {
                unsigned int numToRead = 0;
                unsigned int totalRead = 0;
                uBuf = kmalloc(ctrl.numbytes, GFP_KERNEL);
                if (!uBuf)
                {
                    dev_err(&dev->interface->dev, "Alloc for uBuf failed\n");
                    return 0;
                }

                numbytes = ctrl.numbytes;
                numToRead = numbytes;
                dev_dbg(&dev->interface->dev, "numbytes to read = %d\n", numbytes);
                dev_dbg(&dev->interface->dev, "endpoint # %d\n", ctrl.endpoint);

                if (copy_from_user(uBuf, ctrl.pData, ctrl.numbytes))
                {
                    dev_err(&dev->interface->dev, "copying ctrl.pData to dummyBuf failed\n");
                    return 0;
                }

                do
                {
                    i = usb_bulk_msg(dev->udev, dev->hEP[ctrl.endpoint], (uBuf + totalRead),
                                     (numToRead > 64) ? 64 : numToRead, &numbytes, HZ*10); /* EP0 can only handle 64 bytes at a time */
                    if (i)
                    {
                        dev_dbg(&dev->interface->dev, "CMD = %s, Address = 0x%02X\n",
                                ((uBuf[3] == 0x02) ? "WRITE" : "READ"), uBuf[1]);
                        dev_dbg(&dev->interface->dev, "Number of bytes Attempted to read = %d\n", (int)ctrl.numbytes);
                        dev_dbg(&dev->interface->dev, "Blocking Read I/O Failed with status %d\n", i);
                        kfree(uBuf);
                        return -1;
                    }
                    else
                    {
                        dev_dbg(&dev->interface->dev, "Pixis EP0 Read %d bytes\n", numbytes);
                        totalRead += numbytes;
                        numToRead -= numbytes;
                    }
                } while (numToRead);

                if (copy_to_user(ctrl.pData, uBuf, totalRead))
                {
                    dev_err(&dev->interface->dev, "uBuf copy_to_user failed\n");
                    return 0;
                }

                dev_dbg(&dev->interface->dev, "Total Bytes Read from PIXIS EP0 = %d\n", totalRead);
                ctrl.numbytes = totalRead;

                if (copy_to_user(arg, &ctrl, sizeof(ctrl)))
                {
                    dev_err(&dev->interface->dev, "ioctl_ctrl copy_to_user failed\n");
                    return 0;
                }

                kfree(uBuf);
                return ctrl.numbytes;
            }
            else /* ST133 Pixel Data */
            {
                if (!dev->gotPixelData)
                    return 0;
                else
                {
                    dev->gotPixelData = 0;
                    ctrl.numbytes = dev->bulk_in_size_returned;
                    dev->bulk_in_size_returned -= dev->frameSize;
                    for (i=0; i < dev->maplist_numPagesMapped[dev->active_frame]; i++)
                        SetPageDirty(sg_page(&(dev->sgl[dev->active_frame][i])));
                    dev->active_frame = ((dev->active_frame + 1) % dev->num_frames);
                    return ctrl.numbytes;
                }
            }
            break;

        case 1: /* ST133 I/O */
        case 4: /* PIXIS I/O */
            uBuf = kmalloc(ctrl.numbytes, GFP_KERNEL);
            if (!uBuf)
            {
                dev_err(&dev->interface->dev, "Alloc for uBuf failed\n");
                return 0;
            }

            numbytes = ctrl.numbytes;
            if (copy_from_user(uBuf, ctrl.pData, numbytes))
            {
                dev_err(&dev->interface->dev, "copying ctrl.pData to dummyBuf failed\n");
                return 0;
            }

            i = usb_bulk_msg(dev->udev, dev->hEP[ctrl.endpoint], uBuf,
                             numbytes, &numbytes, HZ*10);

            if (i)
            {
                dev_err(&dev->interface->dev, "Blocking Read I/O Failed with status %d\n", i);
                kfree(uBuf);
                return -1;
            }
            else
            {
                ctrl.numbytes = numbytes;
                if (copy_to_user(ctrl.pData, uBuf, numbytes))
                {
                    dev_err(&dev->interface->dev, "uBuf copy_to_user failed\n");
                    return 0;
                }

                if (copy_to_user(arg, &ctrl, sizeof(ctrl)))
                {
                    dev_err(&dev->interface->dev, "ioctl_ctrl copy_to_user failed\n");
                    return 0;
                }

                kfree(uBuf);
                return ctrl.numbytes;
            }
            break;

        case 2: /* PIXIS Ping */
        case 3: /* PIXIS Pong */
            if (!dev->gotPixelData)
                return 0;
            else
            {
                dev->gotPixelData = 0;
                ctrl.numbytes = dev->bulk_in_size_returned;
                dev->bulk_in_size_returned -= dev->frameSize;

                for (i=0; i < dev->maplist_numPagesMapped[dev->active_frame]; i++)
                    SetPageDirty(sg_page(&(dev->sgl[dev->active_frame][i])));

                dev->active_frame = ((dev->active_frame + 1) % dev->num_frames);
                return ctrl.numbytes;
            }
            break;
    }

    return 0;
}

static int piusb_unmap_user_buffer(struct rspiusb *dev)
{
    int i = 0;
    int k = 0;
    unsigned int epAddr;

    for (k = 0; k < dev->num_frames; k++)
    {
        dev_dbg(&dev->interface->dev, "Killing Urbs for Frame %d\n", k);
        for (i = 0; i < dev->sgEntries[k]; i++)
        {
            usb_kill_urb(dev->PixelUrb[k][i]);
            usb_free_urb(dev->PixelUrb[k][i]);
            dev->pendedPixelUrbs[k][i] = 0;
        }
        dev_dbg(&dev->interface->dev, "Urb error count = %d\n", errCnt);
        errCnt = 0;
        dev_dbg(&dev->interface->dev, "Urbs free'd and Killed for Frame %d\n", k);
    }

    for (k = 0; k < dev->num_frames; k++)
    {
        if (dev->iama == PIXIS_PID) /* If so, which EP should we map this frame to */
        {
            if (k % 2) /* Check to see if this should use EP4(PONG) */
                epAddr = dev->hEP[3]; /* PONG, odd frames */
            else
                epAddr = dev->hEP[2]; /* PING, even frames and zero */
        }
        else /* ST133 only has 1 endpoint for Pixel data transfer */
            epAddr = dev->hEP[0];

        dma_unmap_sg(&dev->udev->dev, dev->sgl[k], dev->maplist_numPagesMapped[k],
                     epAddr ? DMA_FROM_DEVICE : DMA_TO_DEVICE);

        for (i = 0; i < dev->maplist_numPagesMapped[k]; i++)
            page_cache_release(sg_page(&(dev->sgl[k][i])));

        kfree(dev->sgl[k]);
        kfree(dev->PixelUrb[k]);
        kfree(dev->pendedPixelUrbs[k]);
        dev->sgl[k] = NULL;
        dev->PixelUrb[k] = NULL;
        dev->pendedPixelUrbs[k] = NULL;
    }

    kfree(dev->sgEntries);
    vfree(dev->maplist_numPagesMapped);
    dev->sgEntries = NULL;
    dev->maplist_numPagesMapped = NULL;
    kfree(dev->sgl);
    kfree(dev->pendedPixelUrbs);
    kfree(dev->PixelUrb);
    dev->sgl = NULL;
    dev->pendedPixelUrbs = NULL;
    dev->PixelUrb = NULL;

    return 0;
}

/* piusb_map_user_buffer
 * Inputs:
 *    struct rspiusb *dev - the PIUSB device extension
 *    struct ioctl_data *io - structure containing user address, frame #, and size
 *
 * Returns:
 *    int - status of the task
 *
 * Notes:
 *    Maps a buffer passed down through an ioctl.  The user buffer is Page Aligned by the app
 *    and then passed down.  The function get_free_pages(...) does the actual mapping of the buffer from user space to
 *    kernel space.  From there a scatterlist is created from all the pages.  The next function called is to usb_buffer_map_sg
 *    which allocated DMA addresses for each page, even coalescing them if possible.  The DMA address is placed in the scatterlist
 *    structure.  The function returns the number of DMA addresses.  This may or may not be equal to the number of pages that
 *    the user buffer uses.  We then build an URB for each DMA address and then submit them.
 */
static int piusb_map_user_buffer(struct rspiusb *dev, struct ioctl_data __user *arg)
{
    struct ioctl_data ctrl;
    unsigned long uaddr;
    unsigned long numbytes;
    int frameInfo; /* Which frame we're mapping */
    unsigned int epAddr = 0;
    unsigned long count =0;
    int i = 0;
    int k = 0;
    int err = 0;
    int ret = 0;
    struct page **maplist_p;
    int numPagesRequired;

    if (copy_from_user(&ctrl, arg, sizeof(ctrl)))
    {
        dev_err(&dev->interface->dev, "PIUSB_USERBUFFER: copy_from_user failed\n");
        return -EFAULT;
    }

    frameInfo = ctrl.numFrames;
    uaddr = (unsigned long)ctrl.pData;
    numbytes = ctrl.numbytes;

    if (dev->iama == PIXIS_PID) /* If so, which EP should we map this frame to */
    {
        if (frameInfo % 2) /* check to see if this should use EP4(PONG) */
            epAddr = dev->hEP[3]; /* PONG, odd frames */
        else
            epAddr = dev->hEP[2]; /* PING, even frames and zero */

        dev_dbg(&dev->interface->dev, "Pixis Frame #%d: EP=%d\n",
                frameInfo, (epAddr==dev->hEP[2]) ? 2 : 4);
    }
    else /* ST133 only has 1 endpoint for Pixel data transfer */
    {
        epAddr = dev->hEP[0];
        dev_dbg(&dev->interface->dev, "ST133 Frame #%d: EP=2\n", frameInfo);
    }

    count = numbytes;
    dev_dbg(&dev->interface->dev, "UserAddress = 0x%08lX\n", uaddr);
    dev_dbg(&dev->interface->dev, "numbytes = %d\n", (int)numbytes);

    /* Number of pages to map the entire user space DMA buffer */
    numPagesRequired = ((uaddr & ~PAGE_MASK) + count + ~PAGE_MASK) >> PAGE_SHIFT;
    dev_dbg(&dev->interface->dev, "Number of pages needed = %d\n", numPagesRequired);
    maplist_p = vmalloc(numPagesRequired * sizeof(struct page*));//, GFP_ATOMIC);
    if (!maplist_p)
    {
        dev_dbg(&dev->interface->dev, "Can't Allocate Memory for maplist_p\n");
        return -ENOMEM;
    }

    /* Map the user buffer to kernel memory */
    down_write(&current->mm->mmap_sem);
    dev->maplist_numPagesMapped[frameInfo] = get_user_pages(current,
                                                            current->mm,
                                                            (uaddr & PAGE_MASK),
                                                            numPagesRequired,
                                                            WRITE,
                                                            0, /* Don't Force */
                                                            maplist_p,
                                                            NULL);
    up_write(&current->mm->mmap_sem);
    dev_dbg(&dev->interface->dev, "Number of pages mapped = %d\n",
            dev->maplist_numPagesMapped[frameInfo]);

    for (i=0; i < dev->maplist_numPagesMapped[frameInfo]; i++)
        flush_dcache_page(maplist_p[i]);

    if (!dev->maplist_numPagesMapped[frameInfo])
    {
        dev_err(&dev->interface->dev, "get_user_pages() failed\n");
        vfree(maplist_p);
        return -ENOMEM;
    }

    /* Need to create a scatterlist that spans each frame that can fit into the mapped buffer */
    dev->sgl[frameInfo] = kmalloc((dev->maplist_numPagesMapped[frameInfo] * sizeof(struct scatterlist)), GFP_ATOMIC);
    if (!dev->sgl[frameInfo])
    {
        vfree(maplist_p);
        dev_err(&dev->interface->dev, "can't allocate mem for sgl\n");
        return -ENOMEM;
    }

    sg_init_table(dev->sgl[frameInfo], dev->maplist_numPagesMapped[frameInfo]);
    sg_assign_page(&(dev->sgl[frameInfo][0]), maplist_p[0]);
    dev->sgl[frameInfo][0].offset = uaddr & ~PAGE_MASK;

    if (dev->maplist_numPagesMapped[frameInfo] > 1)
    {
        dev->sgl[frameInfo][0].length = PAGE_SIZE - dev->sgl[frameInfo][0].offset;
        count -= dev->sgl[frameInfo][0].length;
        for (k=1; k < dev->maplist_numPagesMapped[frameInfo] ; k++)
        {
            sg_assign_page(&(dev->sgl[frameInfo][k]), maplist_p[k]);
            dev->sgl[frameInfo][k].offset = 0;
            dev->sgl[frameInfo][k].length = (count < PAGE_SIZE) ? count : PAGE_SIZE;
            count -= PAGE_SIZE; //example had PAGE_SIZE here;
        }
    }
    else
        dev->sgl[frameInfo][0].length = count;

    ret = dma_map_sg(&dev->udev->dev, dev->sgl[frameInfo], dev->maplist_numPagesMapped[frameInfo],
                     epAddr ? DMA_FROM_DEVICE : DMA_TO_DEVICE) ? : -ENOMEM;

    if (ret < 0)
    {
        vfree(maplist_p);
        dev_err(&dev->interface->dev, "dma_map_sg failed\n");

        return -EFAULT;
    }

    dev_dbg(&dev->interface->dev, "number of sgEntries = %u\n",
            dev->sgEntries[frameInfo]);
    vfree(maplist_p);

    dev->sgEntries[frameInfo] = ret;
    dev->userBufMapped = 1;

    /* Create and Send the URB's for each s/g entry */
    dev->PixelUrb[frameInfo] = kmalloc(dev->sgEntries[frameInfo] * sizeof(struct urb *), GFP_KERNEL);
    if (!dev->PixelUrb[frameInfo])
    {
        dev_err(&dev->interface->dev, "Can't Allocate Memory for Urb\n");
        return -ENOMEM;
    }

    for (i = 0; i < dev->sgEntries[frameInfo]; i++)
    {
        dev->PixelUrb[frameInfo][i] = usb_alloc_urb(0, GFP_KERNEL); /* 0 because we're using BULK transfers */
        usb_fill_bulk_urb( dev->PixelUrb[frameInfo][i],
                          dev->udev,
                          epAddr,
                          (void *)(unsigned long)sg_dma_address(&dev->sgl[frameInfo][i]),
                          sg_dma_len(&dev->sgl[frameInfo][i]),
                          piusb_read_pixel_callback,
                          (void *)dev);
        dev->PixelUrb[frameInfo][i]->transfer_dma = sg_dma_address(&dev->sgl[frameInfo][i]);
        dev->PixelUrb[frameInfo][i]->transfer_flags = URB_NO_TRANSFER_DMA_MAP | URB_NO_INTERRUPT;
    }

    dev->PixelUrb[frameInfo][--i]->transfer_flags &= ~URB_NO_INTERRUPT;  /* Only interrupt when last URB completes */
    dev->pendedPixelUrbs[frameInfo] = kmalloc((dev->sgEntries[frameInfo] * sizeof(char)), GFP_KERNEL);
    if (!dev->pendedPixelUrbs[frameInfo])
        dev_err(&dev->interface->dev, "Can't allocate Memory for pendedPixelUrbs\n");

    for (i = 0; i < dev->sgEntries[frameInfo]; i++)
    {
        err = usb_submit_urb(dev->PixelUrb[frameInfo][i], GFP_ATOMIC);
        if (err)
        {
            dev_err(&dev->interface->dev, "submit urb error = %d\n", err);
            dev->pendedPixelUrbs[frameInfo][i] = 0;
            return err;
        }
        else
            dev->pendedPixelUrbs[frameInfo][i] = 1;
    }

    return 0;
}

static long piusb_unlocked_ioctl(struct file *f, unsigned cmd, unsigned long arg)
{
    long ret;
    mutex_lock(&piusb_mutex);
    ret = piusb_ioctl(f->f_dentry->d_inode, f, cmd, arg);
    mutex_unlock(&piusb_mutex);
    return ret;
}

/**
 *  piusb_write_bulk_callback
 */
static void piusb_write_bulk_callback(struct urb *urb)
{
    struct rspiusb *dev = (struct rspiusb *)urb->context;

    /* sync/async unlink faults aren't errors */
    if (urb->status && !(urb->status == -ENOENT || urb->status == -ECONNRESET))
        dev_err(&dev->interface->dev, "nonzero write bulk status received: %d\n", urb->status);

    dev->pendingWrite = 0;
    kfree(urb->transfer_buffer);
}

static void piusb_read_pixel_callback(struct urb *urb)
{
    int i = 0;
    struct rspiusb *dev = (struct rspiusb *) urb->context;

    if (urb->status && !(urb->status == -ENOENT || urb->status == -ECONNRESET))
    {
        dev_err(&dev->interface->dev, "Nonzero read bulk status received: %d\n", urb->status);
        dev_err(&dev->interface->dev, "Error in read EP2 callback\n");
        dev_err(&dev->interface->dev, "FrameIndex = %d\n", dev->frameIdx);
        dev_err(&dev->interface->dev, "Bytes received before problem occurred = %d\n", dev->bulk_in_byte_trk);
        dev_err(&dev->interface->dev, "Urb Idx = %d\n", dev->urbIdx);
        dev->pendedPixelUrbs[dev->frameIdx][dev->urbIdx] = 0;
    }
    else
    {
        dev->bulk_in_byte_trk += urb->actual_length;

        /* Resubmit the URB */
        i = usb_submit_urb(urb, GFP_ATOMIC);
        if (i)
        {
            errCnt++;
            if (i != lastErr)
            {
                dev_dbg(&dev->interface->dev, "submit urb in callback failed with error code %d\n", i);
                lastErr = i;
            }
        }
        else
        {
            /* Point to next URB when we callback */
            dev->urbIdx++;
            if (dev->bulk_in_byte_trk >= dev->frameSize)
            {
                dev->bulk_in_size_returned = dev->bulk_in_byte_trk;
                dev->bulk_in_byte_trk = 0;
                dev->gotPixelData = 1;
                dev->frameIdx = (dev->frameIdx + 1) % dev->num_frames;
                dev->urbIdx = 0;
            }
        }
    }
}

module_init(piusb_init);
module_exit(piusb_exit);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL v2");
