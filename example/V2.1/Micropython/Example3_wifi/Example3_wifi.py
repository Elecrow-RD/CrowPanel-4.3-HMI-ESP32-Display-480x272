import network
import time

def connect():
    ssid = 'elecrow888'
    password = 'elecrow2014'
    
    wlan = network.WLAN(network.STA_IF)  # Create a WLAN object in station mode
    wlan.active(True)  # Activate the network interface
    wlan.connect(ssid, password)  # Connect to the specified WiFi network
    
    while not wlan.isconnected():  # Wait for the connection to be established
        print('Waiting for connection...')
        time.sleep(1)
    
    print('Connected on {ip}'.format(ip=wlan.ifconfig()[0]))  # Print the IP address

connect()
