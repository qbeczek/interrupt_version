import periphery

i2c = periphery.I2C("/dev/i2c-17")


# # Define custom flags
# MY_CUSTOM_FLAG_1 = 0x0010
# MY_CUSTOM_FLAG_2 = 0x0200
device_adr = 0x50
# # Create an I2C message
# # Replace [0x00, 0x01, 0x02] with your data bytes
message_to_send = "TEST".encode('utf-8')
messages = [
    # I2C.Message(message_to_send, flags=flags),
    # I2C.Message(message_to_send, flags=0),
    periphery.I2C.Message(message_to_send, flags=0),
    periphery.I2C.Message(data=[0x00, 0x00, 0x00, 0x00], read=True, flags=0),
]

# Set custom flags in the flags field
# pmsg.flags |= MY_CUSTOM_FLAG_1
# pmsg.flags |= MY_CUSTOM_FLAG_2

# Open the I2C device
# Replace "/dev/i2c-0" with the appropriate I2C bus device

# Send the I2C message
i2c.transfer(device_adr, messages)

# Close the I2C device
i2c.close()
