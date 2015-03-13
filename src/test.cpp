#include "mcp3008Spi.h"
#include <vector>
#include <wiringPi.h>

using namespace std;

int main(void)
{
    mcp3008Spi a2d_2("/dev/spidev0.0", SPI_MODE_0, 1000000, 8);
    int a2dVal = 0;
    int a2dChannel = 0;
    unsigned char data[3];
    int chip_counter = 0;
    unsigned char channel = 0b10000000;
    bool b_switch = false;

    wiringPiSetup();

    for (int pin = 4; pin < 7; ++pin)
    {
        pinMode(pin, OUTPUT);
    }

    pinMode(1, INPUT);

    digitalWrite(4, 1);
    digitalWrite(5, 1);
    digitalWrite(6, 0);

    while(1)
    {
        chip_counter = 0;
        channel = 0b10000000;
        for (int i = 0; i < 10; i++)
        {
            chip_counter = i/8;
            if (chip_counter)
            {
                digitalWrite(6, 1);
                digitalWrite(5, 0);
            }
            else
            {
                digitalWrite(6, 0);
                digitalWrite(5, 1);
            }

            data[0] = 1;  //  first byte transmitted -> start bit
            data[1] = channel |( ((a2dChannel & 7) << 4)); // second byte transmitted -> (SGL/DIF = 1, D2=D1=D0=0)
            data[2] = 0; // third byte transmitted....don't care
    
            a2d_2.spiWriteRead(data, sizeof(data) );
 
            a2dVal = 0;
                    a2dVal = (data[1]<< 8) & 0b1100000000; //merge data[1] & data[2] to get result
                    a2dVal |=  (data[2] & 0xff);
            channel = channel + (1 << 4);
            sleep(1);
            cout << "The Result on channel " << i << " is: " << a2dVal << endl;
        }
    }
    return 0;
}
