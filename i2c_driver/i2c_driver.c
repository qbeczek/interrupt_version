#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/usb.h>

/* Meta Information */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("KUBA");
MODULE_DESCRIPTION("Driver for stm32 i2c device");

#define VENDOR_ID 0x2312
#define PRODUCT_ID 0xec40
#undef CTRL_MSG_TEST

#define I2C_READ 1
#define I2C_WRITE 0
/* commands via USB, must match command ids in the firmware */
#define CMD_ECHO 0
#define CMD_GET_FUNC 1
#define CMD_SET_DELAY 2
#define CMD_GET_STATUS 3

#define CMD_I2C_IO 4
#define CMD_I2C_IO_BEGIN (1 << 0)
#define CMD_I2C_IO_END (1 << 1)

#define I2C_FLAG_FIND_DEVICE 8 /* your new flag 1 */
#define I2C_FLAG_READ_BYTE 9   /* your new flag 2 */
#define I2C_FLAG_3 10          /* your new flag 3 */
#define I2C_FLAG_4 0x80000     /* your new flag 4 */
#define I2C_FLAG_5 0x100000    /* your new flag 5 */

#define CMD_I2C_FIND_DEVICE 0x5d
#define CMD_I2C_READ_BYTE 0x5e

#define CMD_READ_FROM_BUFFER 0x5c
#define CMD_WRITE_TO_BUFFER 0x5b

struct i2c_command {
    uint8_t operation;  // 1 = write, 0 = read
    uint8_t addr;       // Adres urządzenia I2C
    uint8_t reg;        // Adres rejestru
    uint8_t length;     // Długość danych
    uint8_t data[64];   // Dane do zapisu lub bufor do odczytu
};

/* i2c bit delay, default is 10us -> 100kHz */
static int delay = 10;
struct i2c_over_usb;
module_param(delay, int, 0);
MODULE_PARM_DESC(delay,
                 "bit delay in microseconds, "
                 "e.g. 10 for 100kHz (default is 100kHz)");

static int usb_read(struct i2c_adapter *adapter, uint8_t cmd,
                    struct i2c_command *i2c_cmd, size_t cmd_len, uint8_t *buf,
                    size_t buf_len);

static int usb_write(struct i2c_adapter *adapter, uint8_t cmd,
                     struct i2c_command *i2c_cmd, size_t cmd_len);

void print_buffer(__u8 *buf, size_t buf_size) {
    for (size_t i = 0; i < buf_size; i++) {
        printk("buf[%zu] = %c\n", i, buf[i]);
    }
}

static void usb_interrupt_callback(struct urb *urb) {
    struct i2c_over_usb *dev = urb->context;

    if (urb->status) {
        printk("Interrupt transfer failed: % d\n ", urb->status);
    } else {
        printk("Data received via interrupt: %*ph\n", urb->actual_length,
               urb->transfer_buffer);
        /* Process the received data */
    }

    // /* Resubmit the URB */
    int ret = usb_submit_urb(urb, GFP_ATOMIC);
    if (ret) {
        printk("Failed to resubmit interrupt URB: %d\n", ret);
    }
}

static int usb_xfer(struct i2c_adapter *adapter, struct i2c_msg *msgs,
                    int num_msgs) {
    int ret, i;
    struct i2c_command cmd;
    struct i2c_msg *msg;

    dev_info(&adapter->dev, "master xfer %d messages:\n", num_msgs);

    for (i = 0; i < num_msgs; i++) {
        msg = &msgs[i];

        cmd.operation = (msg->flags & I2C_M_RD) ? 0 : 1;
        cmd.addr = msg->addr;
        cmd.reg = msg->buf[0];
        cmd.length = 8;
        memcpy(cmd.data, &msg->buf[1], cmd.length);

        if (cmd.operation == 0) {
            // Odczyt danych
            dev_info(&adapter->dev, "Reading %d bytes from 0x%02x\n",
                     cmd.length, cmd.addr);
            ret = usb_read(adapter, CMD_READ_FROM_BUFFER, &cmd, sizeof(cmd),
                           msg->buf, msg->len);
            if (ret < 0) {
                dev_err(&adapter->dev, "failure reading data\n");
                return -EREMOTEIO;
            }
        } else {
            // Zapis danych
            dev_info(&adapter->dev, "Writing %d bytes to 0x%02x\n", cmd.length,
                     cmd.addr);

            printk("Register: %x", cmd.reg);
            ret = usb_write(adapter, CMD_WRITE_TO_BUFFER, &cmd, sizeof(cmd));
            if (ret < 0) {
                dev_err(&adapter->dev, "failure writing data\n");
                return -EREMOTEIO;
            }
        }
    }

    return num_msgs;
}

static uint32_t usb_func(struct i2c_adapter *adapter) {
    return (I2C_FUNC_I2C | I2C_FUNC_SMBUS_QUICK | I2C_FUNC_SMBUS_BYTE |
            I2C_FUNC_SMBUS_BYTE_DATA | I2C_FUNC_SMBUS_WORD_DATA |
            I2C_FUNC_SMBUS_BLOCK_DATA);
}

/* This is the actual algorithm we define */
static const struct i2c_algorithm usb_algorithm = {
    .master_xfer = usb_xfer,
    .functionality = usb_func,
};

/*-----------------------------------------------------------------------------


-------------------------------------------------------------------------------*/
static struct usb_device *usb_dev;

static struct usb_device_id i2c_over_usb_id_table[] = {
    {USB_DEVICE(VENDOR_ID, PRODUCT_ID)},
    {},
};

MODULE_DEVICE_TABLE(usb, i2c_over_usb_id_table);

/* Structure to hold all of our device specific stuff */
struct i2c_over_usb {
    struct usb_device *usb_dev;      /* the usb device for this device */
    struct usb_interface *interface; /* the interface for this device */
    struct i2c_adapter adapter;      /* i2c related things */
    struct urb *int_in_urb;
    unsigned char *int_in_buffer;
    size_t int_in_buffer_size;
    int int_in_endpointAddr;
};

static int usb_read(struct i2c_adapter *adapter, uint8_t cmd,
                    struct i2c_command *i2c_cmd, size_t cmd_len, uint8_t *buf,
                    size_t buf_len) {
    msleep(10);
    printk("USB_READ_______________________\n");
    int ret;
    int transferred;
    struct i2c_over_usb *dev = (struct i2c_over_usb *)adapter->algo_data;
    uint8_t *buffer = kzalloc(64, GFP_KERNEL);

    if (!buffer) {
        dev_err(&adapter->dev, "Failed to allocate memory\n");
        return -ENOMEM;
    }

    printk("Reading USB interrupt message:\n");

    ret = usb_interrupt_msg(dev->usb_dev, usb_rcvintpipe(dev->usb_dev, 0x81),
                            buffer, 64, &transferred, 1000);

    if (ret < 0) {
        dev_err(&adapter->dev, "USB interrupt msg receive failed: %d\n", ret);
        kfree(buffer);
        return ret;
    }

    printk("Data received: %*ph\n", 64, buffer);
    printk("Transferred after: %d\n", transferred);

    memcpy(buf, buffer, min(buf_len, (size_t)transferred));
    kfree(buffer);

    return 0;
}

static int usb_write(struct i2c_adapter *adapter, uint8_t cmd,
                     struct i2c_command *i2c_cmd, size_t cmd_len) {
    // msleep(10);

    printk("USB_WRITE_______________________");

    struct i2c_over_usb *dev = (struct i2c_over_usb *)adapter->algo_data;
    int transferred;
    dev_info(&adapter->dev, "Sending USB interrupt message:\n");
    dev_info(&adapter->dev, "cmd_len: %zu\n", cmd_len);
    dev_info(&adapter->dev,
             "i2c_cmd operation: %d, addr: 0x%02x, reg: 0x%02x, length: %d\n",
             i2c_cmd->operation, i2c_cmd->addr, i2c_cmd->reg, i2c_cmd->length);

    int ret = usb_interrupt_msg(
        dev->usb_dev, usb_sndintpipe(dev->usb_dev, 0x01), (uint8_t *)i2c_cmd,
        sizeof(cmd_len), &transferred, 1000);

    if (ret < 0) {
        dev_err(&adapter->dev, "USB interrupt msg send failed: %d\n", ret);
        return ret;
    }

    printk("Data sent: %*ph\n", (int)cmd_len, (unsigned char *)i2c_cmd);
    printk("Transferred: %d\n", transferred);

    return 0;
}

static int i2c_over_usb_probe(struct usb_interface *interface,
                              const struct usb_device_id *id) {
    struct usb_host_interface *iface_desc;
    struct usb_endpoint_descriptor *endpoint;
    struct i2c_over_usb *dev;
    int retval = -ENOMEM;
    int i;

    // Allocate memory for our device structure
    dev = kzalloc(sizeof(struct i2c_over_usb), GFP_KERNEL);
    if (!dev) {
        dev_err(&interface->dev, "Failed to allocate memory\n");
        return -ENOMEM;
    }

    dev->usb_dev = usb_get_dev(interface_to_usbdev(interface));
    dev->interface = interface;

    // Find and initialize interrupt-in endpoint
    iface_desc = interface->cur_altsetting;
    for (i = 0; i < iface_desc->desc.bNumEndpoints; ++i) {
        endpoint = &iface_desc->endpoint[i].desc;

        if (usb_endpoint_is_int_in(endpoint)) {
            dev->int_in_endpointAddr = endpoint->bEndpointAddress;
            dev->int_in_buffer_size = le16_to_cpu(endpoint->wMaxPacketSize);
            dev->int_in_buffer = kmalloc(dev->int_in_buffer_size, GFP_KERNEL);
            if (!dev->int_in_buffer) {
                dev_err(&interface->dev, "Failed to allocate int_in_buffer\n");
                retval = -ENOMEM;
                goto error;
            }
            dev->int_in_urb = usb_alloc_urb(0, GFP_KERNEL);
            if (!dev->int_in_urb) {
                dev_err(&interface->dev, "Failed to allocate int_in_urb\n");
                retval = -ENOMEM;
                goto error;
            }
            break;  // Found and initialized the interrupt-in endpoint
        }
    }

    usb_fill_int_urb(dev->int_in_urb, dev->usb_dev,
                     usb_rcvintpipe(dev->usb_dev, dev->int_in_endpointAddr),
                     dev->int_in_buffer, dev->int_in_buffer_size,
                     usb_interrupt_callback, dev, 1);

    retval = usb_submit_urb(dev->int_in_urb, GFP_KERNEL);
    if (retval) {
        dev_err(&interface->dev, "Could not submit interrupt in URB: %d\n",
                retval);
        goto error;
    }
    if (!dev->int_in_urb) {
        dev_err(&interface->dev, "Interrupt-in endpoint not found\n");
        retval = -ENODEV;
        goto error;
    }

    // Initialize and register the I2C adapter
    dev->adapter.owner = THIS_MODULE;
    dev->adapter.class = I2C_CLASS_HWMON | I2C_CLASS_SPD;
    dev->adapter.algo = &usb_algorithm;
    dev->adapter.algo_data = dev;
    dev->adapter.dev.parent = &interface->dev;
    strlcpy(dev->adapter.name, "i2c-over-usb", sizeof(dev->adapter.name));
    i2c_set_adapdata(&dev->adapter, dev);

    retval = i2c_add_adapter(&dev->adapter);
    if (retval) {
        dev_err(&interface->dev, "Failed to add I2C adapter: %d\n", retval);
        goto error;
    }

    // Set the interface data to our device structure
    usb_set_intfdata(interface, dev);

    dev_info(&interface->dev, "i2c-over-usb device now attached\n");
    return 0;

error:
    if (dev->int_in_urb) usb_free_urb(dev->int_in_urb);
    kfree(dev->int_in_buffer);
    kfree(dev);
    return retval;
}

static void i2c_over_usb_disconnect(struct usb_interface *interface) {
    struct i2c_over_usb *dev;

    dev = usb_get_intfdata(interface);
    usb_set_intfdata(interface, NULL);

    if (dev) {
        usb_kill_urb(dev->int_in_urb);
        usb_free_urb(dev->int_in_urb);
        kfree(dev->int_in_buffer);
        i2c_del_adapter(&dev->adapter);
        kfree(dev);
    }

    dev_info(&interface->dev, "i2c-over-usb now disconnected\n");
}

static struct usb_driver i2c_over_usb_driver = {
    .name = "i2c_over_usb",
    .probe = i2c_over_usb_probe,
    .disconnect = i2c_over_usb_disconnect,
    .id_table = i2c_over_usb_id_table,
};

static int __init usb_i2c_over_usb_init(void) {
    /* register this driver with the USB subsystem */
    return usb_register(&i2c_over_usb_driver);
}

static void __exit usb_i2c_over_usb_exit(void) {
    /* deregister this driver with the USB subsystem */
    usb_deregister(&i2c_over_usb_driver);
}

module_init(usb_i2c_over_usb_init);
module_exit(usb_i2c_over_usb_exit);

// module_usb_driver(i2c_over_usb_driver);
