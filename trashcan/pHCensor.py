import spidev
from time import sleep
from collections import deque

# SPI Initialization
spi = spidev.SpiDev()
spi.open(0, 0)  # Bus 0, Device 0
spi.max_speed_hz = 1350000

def read_adc(channel):
    """
    Reads data from an MCP3008 ADC channel.
    :param channel: The ADC channel (0-7)
    :return: ADC value (0-1023)
    """
    if channel < 0 or channel > 7:
        raise ValueError("Channel must be between 0 and 7")
    adc = spi.xfer2([1, (8 + channel) << 4, 0])
    data = ((adc[1] & 3) << 8) + adc[2]
    return data

def read_adc_avg(channel, samples=50):
    """
    Reads the average ADC value over multiple samples.
    :param channel: The ADC channel (0-7)
    :param samples: Number of samples to average
    :return: Averaged ADC value
    """
    total = 0
    for _ in range(samples):
        total += read_adc(channel)
        sleep(0.002)  # Small delay between samples
    return total // samples

def convert_to_ph(voltage, offset=4.2, sensitivity=0.18):
    """
    Converts voltage to pH value.
    :param voltage: Analog voltage (V)
    :param offset: Voltage at pH 7 (default: 4.2V)
    :param sensitivity: Voltage change per pH unit (default: 0.18V/pH)
    :return: pH value
    """
    return 7 + (voltage - offset) / sensitivity

class MovingAverage:
    """
    Moving Average Filter for smoothing pH values.
    """
    def __init__(self, size):
        self.queue = deque(maxlen=size)

    def add(self, value):
        self.queue.append(value)
        return sum(self.queue) / len(self.queue)

# Moving average filter initialization
ph_filter = MovingAverage(size=10)

try:
    print("Reading pH sensor values... Press Ctrl+C to exit.")
    while True:
        # Read the average ADC value from channel 0
        adc_value = read_adc_avg(0, samples=50)
        
        # Convert ADC value to voltage
        voltage = (adc_value / 1023.0) * 5.0  # 5V reference
        
        # Convert voltage to pH
        ph_value = convert_to_ph(voltage, offset=4.2, sensitivity=0.18)
        
        # Apply moving average filter
        smoothed_ph = ph_filter.add(ph_value)

        # Print raw and smoothed pH values
        print(f"Raw ADC: {adc_value}, Voltage: {voltage:.2f} V, Smoothed pH: {smoothed_ph:.2f}")
        
        # Delay for stability
        sleep(1)

except KeyboardInterrupt:
    print("\nExiting...")
    spi.close()