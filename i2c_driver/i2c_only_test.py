import usb
import argparse
import time
from periphery import I2C


def main():
    # Open i2c-0 controller
    i2c = I2C("/dev/i2c-18")

    # Define the I2C address of the target device
    device_address = 0x76

    # Create a list of messages to send
    message_to_send = "TEST_123".encode('utf-8')

    flags = 0  # Replace with the appropriate flags

    messages = [
        I2C.Message(message_to_send, flags=flags),
        I2C.Message(data=[0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00], read=True, flags=0),
    ]

    i2c.transfer(device_address, messages)

    # Print sent and received data
    print("Sent data:", message_to_send)

    print("Received data:", messages[1].data)

    # Convert the received data (list of integers) to bytes
    received_data = bytes(messages[1].data)

    # Process only the relevant portion of the received data
    relevant_data = received_data[:len(message_to_send)]

    # Attempt to decode the received data as a UTF-8 string
    try:
        received_string = relevant_data.decode('utf-8')
        print("Received data as a string:", received_string)
    except UnicodeDecodeError:
        print("Received data could not be decoded as UTF-8 string")

    # Close the I2C connection
    i2c.close()


if __name__ == "__main__":
    main()
