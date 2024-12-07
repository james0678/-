import spidev
from time import sleep

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
        raise ValueError("Channel must be between 0 and 7")  # Channel validation
    adc = spi.xfer2([1, (8 + channel) << 4, 0])  # Send and receive data via SPI
    data = ((adc[1] & 3) << 8) + adc[2]  # Extract ADC value from received data
    return data

def read_adc_avg(channel, interval=0.1, duration=1):
    """
    Reads the average ADC value over a specified duration, sampling every interval.
    :param channel: The ADC channel (0-7)
    :param interval: Time interval between samples (in seconds)
    :param duration: Total duration to collect samples (in seconds)
    :return: Averaged ADC value
    """
    total_samples = int(duration / interval)
    total = 0
    for _ in range(total_samples):
        total += read_adc(channel)  # Read ADC value from the specified channel
        sleep(interval)  # Delay between each sample
    return total // total_samples  # Return the average value

try:
    print("Reading water level sensor values... Press Ctrl+C to exit.")
    while True:
        for sensor_id in range(1,5):  # Iterate through sensor channels 0, 1, 2, 3
            # Average ADC value over 1 second, sampling every 0.1 seconds
            adc_value = read_adc_avg(sensor_id, interval=0.1, duration=1)
            
            # Convert ADC value to voltage (assuming 5V reference)
            voltage = (adc_value / 1023.0) * 5.0
            
            # Determine water level status
            if voltage > 3.35:
                water_status = "Sensor mostly submerged"
            elif voltage < 2.95:
                water_status = "Sensor not submerged"
            else:
                water_status = "Sensor partially submerged"

            # Print the results for each sensor
            print(f"Sensor {sensor_id}: Raw ADC: {adc_value}, Voltage: {voltage:.2f} V, Status: {water_status}")
        
        print("-" * 50)  # Separator for clarity
        sleep(1)  # Wait for 1 second before the next reading
        
except KeyboardInterrupt:
    print("\nExiting...")
    spi.close()