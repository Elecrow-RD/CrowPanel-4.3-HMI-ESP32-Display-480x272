#define SERIAL_BAUD 9600
HardwareSerial cardSerial(1);//Declare the UART1
unsigned char buffer[256]; // buffer array for data recieve over serial port
int count_1 = 0;   // counter for buffer array
void clearBufferArray()              // function to clear buffer array
{
  for (int i = 0; i < count_1; i++)
  {
    buffer[i] = 0; // clear all index of array with command NULL
  }
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin( 115200 ); /*Initialize the serial port*/
  cardSerial.begin(SERIAL_BAUD, SERIAL_8N1, 18, 17);//4.3 /*初Initialize the serial port1*/18-RX\17-TX
}

void loop() {
  // put your main code here, to run repeatedly:
  if (cardSerial.available())              // if date is comming from softwareserial port ==> data is comming from SoftSerial shield
  {
    while (cardSerial.available())         // reading data into char array
    {
      buffer[count_1++] = cardSerial.read();   // writing data into array
      if (count_1 == 256)break;
    }
    Serial.write(buffer, count_1);           // if no data transmission ends, write buffer to hardware serial port
    clearBufferArray();              // call clearBufferArray function to clear the storaged data from the array
    count_1 = 0;                       // set counter of while loop to zero
  }
  if (Serial.available())            // if data is available on hardwareserial port ==> data is comming from PC or notebook
    cardSerial.write(Serial.read());       // write it to the SoftSerial shield
}