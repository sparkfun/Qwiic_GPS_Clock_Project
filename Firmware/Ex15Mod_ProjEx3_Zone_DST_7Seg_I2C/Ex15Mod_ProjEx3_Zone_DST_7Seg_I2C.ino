/*
  Getting the time and date in your timezone using Ublox commands
  Originally Written By: davidallenmann
  Modified By: Ho Yun "Bobby" Chan
  SparkFun Electronics
  Date: April 16th, 2019
  License: MIT. See license file for more information but you can
  basically do whatever you want with this code.

  This is a modified example that shows how to query a Ublox module for the current time and date. We also
  turn off the NMEA output on the I2C port. This decreases the amount of I2C traffic
  dramatically.

  Leave NMEA parsing behind. Now you can simply ask the module for the datums you want!

  Additionally, this code has the option to adjust the UTC date and time. The time is adjusted by manually
  entering your time zone's offset. The Daylight Savings Time is automatically calculated with the help of
  Nathan Seidle's Daylight Savings Time example [ https://github.com/nseidle/Daylight_Savings_Time_Example ].
  However, if your country does not observe DST, you can override it with the `enableDST` variable.

  The output for this example is sent through a Serial UART port. Depending on personal preference, you can view
  the time in regular 12-hour format or miltary 24-hour format. Shout out to Jim for the I2C example 7-segment
  serial display!

  Feel like supporting open source hardware?
  Buy a board from SparkFun!
  ZED-F9P RTK2: https://www.sparkfun.com/products/15136
  NEO-M8P RTK: https://www.sparkfun.com/products/15005
  SAM-M8Q: https://www.sparkfun.com/products/15106

  Hardware Connections:
  Plug a Qwiic cable into the u-Blox Qwiic-enabled GPS and the Qwiic Micro. We will use a
  7-Segment Display. Since the board was designed before the Qwiic system, you will need
  to solder wires to a Qwiic adapter before connecting it with a Qwiic cable.
  If you don't have a platform with a Qwiic connection use the SparkFun Qwiic Female Jumper (https://www.sparkfun.com/products/14425)
  Open the serial monitor at 115200 baud to see the output.
*/

#include <Wire.h> //Needed for I2C to GPS
#include "SparkFun_Ublox_Arduino_Library.h" //http://librarymanager/All#SparkFun_Ublox_GPS
// Here we'll define the I2C address of our S7S. By default it
//  should be 0x71. This can be changed, though.
const byte s7sAddress = 0x71;

SFE_UBLOX_GPS myGPS;

long lastTime = 0; //Simple local timer. Limits amount if I2C traffic to Ublox module.
long latitude = 0;
long longitude = 0;
long altitude = 0;
byte SIV = 0;

boolean DST = false; //adjust for Daylight Savings Time, this is calculated automatically. fall back = FALSE, spring forward = TRUE
boolean enableDST = true; //option to disable DST if your country does not observe DST
int zoneOffsetHour = -7; //adjust according to your standard time zone
byte DoW = 0; //needed to adjust hour for DST, or if you want to know the Day of the Week
boolean military = false; //adjust for miltary or AM/PM
boolean AM = false; //AM or PM?

// Use these variables to set the initial time: 3:03:00
int hours = 3;
int minutes = 3;
int seconds = 0;

//for 7 segment display
int time_hourTemp = 0;
int time_minuteTemp = 0;
int time_secondTemp = 0;
char tempString[10];  // Will be used with sprintf to create strings


//Tid Bit: https://www.sparkfun.com/news/2571#yearOrigin
int years = 2003; //year that SparkFun was founded!
int months = 1;  //month that SparkFun was founded!
int days = 3;    //day that SparkFun was founded!

// How fast do you want the clock to update? Set this to 1 for fun.
// Set this to 1000 to get _about_ 1 second timing.
const int CLOCK_SPEED = 1000;
unsigned long lastDraw = 0;




void setup() {
  Serial.begin(115200);
  //while (!Serial)
  //  ; //Wait for user to open terminal
  Serial.println(F("SparkFun Ublox Example"));

  Wire.begin();
  Wire.setClock(400000);   // Set clock speed to be the fastest for better communication (fast mode)


  if (myGPS.begin() == false) //Connect to the Ublox module using Wire port
  {
    Serial.println(F("Ublox GPS not detected at default I2C address. Please check wiring. Freezing."));
    while (1)
      ;
  }

  myGPS.setI2COutput(COM_TYPE_UBX); //Set the I2C port to output UBX only (turn off NMEA noise)
  myGPS.saveConfiguration();        //Save the current settings to flash and BBR

  // Clear the display, and then turn on all segments and decimals
  clearDisplayI2C();  // Clears display, resets cursor

  delay(50);  //small delay so 7-segment display has time to pick up commands or characters

  // Custom function to send four bytes via I2C
  //  The I2C.write function only allows sending of a single
  //  byte at a time.
  s7sSendStringI2C("-HI-");
  setDecimalsI2C(0b010000);  // Turn on colon

  // Flash brightness values at the beginning
  setBrightnessI2C(0);  // Lowest brightness
  delay(1500);
  setBrightnessI2C(255);  // High brightness
  delay(1500);

  // Clear the display before jumping into loop
  clearDisplayI2C();

  delay(50); //small delay so 7-segment display has time to pick up commands or characters
  setDecimalsI2C(0b010000);  // Turn on colon

}

void loop() {

  update_Time();


  displayDigital_Date_Time();

} //end loop




// Simple function to increment seconds and then increment minutes
// and hours if necessary.
void update_Time() {

  //Query module only every second. Doing it more often will just cause I2C traffic.
  //The module only responds when a new position is available
  if (millis() - lastTime > 1000) {
    lastTime = millis(); //Update the timer

    latitude = myGPS.getLatitude();
    longitude = myGPS.getLongitude();
    altitude = myGPS.getAltitude();
    SIV = myGPS.getSIV();

    years = myGPS.getYear();
    months = myGPS.getMonth();
    days = myGPS.getDay();
    hours = myGPS.getHour();
    minutes = myGPS.getMinute();
    seconds = myGPS.getSecond();

    calcZone_DST(); //adjust zone and used to check if it is Daylight Savings Time

  }
  //Serial.print(F("Lat: "));
  //Serial.print(latitude);


  //Serial.print(F(" Long: "));
  //Serial.print(longitude);
  //Serial.print(F(" (degrees * 10^-7)"));


  //Serial.print(F(" Alt: "));
  //Serial.print(altitude);
  //Serial.print(F(" (mm)"));


  //Serial.print(F(" SIV: "));
  //Serial.print(SIV);

  //Serial.println();

}

//Nate's snazzy code!
//Given a year/month/day/current UTC/local offset give me local time
void calcZone_DST() {
  //Since 2007 DST starts on the second Sunday in March and ends the first Sunday of November
  //Let's just assume it's going to be this way for awhile (silly US government!)
  //Example from: http://stackoverflow.com/questions/5590429/calculating-daylight-savings-time-from-only-date

  DoW = day_of_week(); //Get the day of the week. 0 = Sunday, 6 = Saturday
  int previousSunday = days - DoW;

  //DST = false; //Assume we're not in DST
  if (enableDST == true) {
    if (months > 3 && months < 11) DST = true; //DST is happening!

    //In March, we are DST if our previous Sunday was on or after the 8th.
    if (months == 3)
    {
      if (previousSunday >= 8) DST = true;
    }
    //In November we must be before the first Sunday to be DST.
    //That means the previous Sunday must be before the 1st.
    if (months == 11)
    {
      if (previousSunday <= 0) DST = true;
    }
  }




  //adjust time for DST here if it applies to your region
  if (DST == true) {//adjust time Daylight Savings Time
    hours = hours + 1;
  }
  else { //leave time as is for Daylight Time
  }




  //adjust time based on Time Zone
  hours = hours + zoneOffsetHour;

  //adjust for offset zones when hour is negative value
  if (hours < 0) {
    days = days - 1;

    hours = hours + 24;
  }
  else if ( hours > 23)    {
    days = days + 1;

    hours = hours - 24;
  }



  //adjust for AM/PM mode
  if (military == false) {
    if (hours >= 0 && hours <= 11) {// we are in AM
      if (hours == 0) {
        hours = 12;
      }
      AM = true;
    }
    else { // hours >= 12 && hours <= 23, therefore we are in PM!!!
      if (hours > 12  && hours <= 23) {
        hours = hours - 12;
      }
      AM = false;
    }

  }



  /*
    Serial.print("Hour: ");
    Serial.println(hour);
    Serial.print("Day of week: ");
    if(DoW == 0) Serial.println("Sunday");
    if(DoW == 1) Serial.println("Monday");
    if(DoW == 2) Serial.println("Tuesday");
    if(DoW == 3) Serial.println("Wednesday");
    if(DoW == 4) Serial.println("Thursday");
    if(DoW == 5) Serial.println("Friday!");
    if(DoW == 6) Serial.println("Saturday");
  */

}




//Given the current year/month/day
//Returns 0 (Sunday) through 6 (Saturday) for the day of the week
//From: http://en.wikipedia.org/wiki/Calculating_the_day_of_the_week
//This function assumes the month from the caller is 1-12
char day_of_week() {
  //Devised by Tomohiko Sakamoto in 1993, it is accurate for any Gregorian date:
  static int t[] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4  };
  years -= months < 3;
  return (years + years / 4 - years / 100 + years / 400 + t[months - 1] + days) % 7;
}




void displayDigital_Date_Time() {
  if (lastDraw + CLOCK_SPEED < millis())
  {
    lastDraw = millis();

    Serial.print(F("Date: "));

    if (months <= 9) {
      Serial.print(F(" "));
    }
    Serial.print(String(months) + '-');


    if (days <= 9) {
      Serial.print(F("0"));
    }
    Serial.println(String(days) + '-' + String(years));





    Serial.print(F("Time: "));


    if (hours <= 9) {
      Serial.print(' ');
      Wire.beginTransmission(s7sAddress);
      Wire.write(0x7B); // special command to control Digit 1
      Wire.write(0b00000000);
      Wire.endTransmission();
      delay(50); //small delay so 7-segment display has time to pick up commands or characters
      if (time_hourTemp != hours) {
        Wire.beginTransmission(s7sAddress);
        Wire.write(0x79); // special command to cursor control
        Wire.write(0x01);
        Wire.endTransmission();
        delay(50); //small delay so 7-segment display has time to pick up commands or characters
        // Magical sprintf creates a string for us to send to the s7s.
        //  The %1d option creates a 1-digit integer.
        sprintf(tempString, "%1d", hours);
        // This will output the tempString to the S7S
        s7sSendCharI2C(tempString);
        time_hourTemp = hours;
      }
    }
    if (time_hourTemp != hours) {
      Wire.beginTransmission(s7sAddress);
      Wire.write(0x79);
      Wire.write(0x00);
      Wire.endTransmission();
      delay(50); //small delay so 7-segment display has time to pick up commands or characters

      // Magical sprintf creates a string for us to send to the s7s.
      //  The %2d option creates a 1-digit integer.
      sprintf(tempString, "%2d", hours);
      // This will output the tempString to the S7S
      s7sSend2xCharI2C(tempString);
      time_hourTemp = hours;

    }

    Serial.print(String(hours) + ':' );


    if (minutes <= 9) {
      Serial.print(F("0"));
      Wire.beginTransmission(s7sAddress);
      Wire.write(0x7D); //special command to control Digit 3
      Wire.write(0b00111111);
      Wire.endTransmission();
      delay(50); //small delay so 7-segment display has time to pick up commands or characters
      if (time_minuteTemp != minutes) {
        Wire.beginTransmission(s7sAddress);
        Wire.write(0x79); //special command to cursor control
        Wire.write(0x03);
        Wire.endTransmission();
        delay(50); //small delay so 7-segment display has time to pick up commands or characters
        // Magical sprintf creates a string for us to send to the s7s.
        //  The %1d option creates a 1-digit integer.
        sprintf(tempString, "%1d", minutes);
        // This will output the tempString to the S7S
        s7sSendCharI2C(tempString);
        time_minuteTemp = minutes;
      }
    }
    if (time_minuteTemp != minutes) {
      Wire.beginTransmission(s7sAddress);
      Wire.write(0x79); //special command to cursor control
      Wire.write(0x02);
      Wire.endTransmission();
      delay(50); //small delay so 7-segment display has time to pick up commands or characters
      // Magical sprintf creates a string for us to send to the s7s.
      //  The %2d option creates a 1-digit integer.
      sprintf(tempString, "%2d", minutes);
      // This will output the tempString to the S7S
      s7sSend2xCharI2C(tempString);
      time_minuteTemp = minutes;
    }

    Serial.print(String(minutes) + ':');


    if (seconds <= 9) {
      Serial.print(F("0"));

    }
    Serial.print(String(seconds));


    if (military == false) {
      if (AM == true) {

        Serial.println(F(" AM"));
      }
      else {
        if (AM == false) {
          Serial.println(F(" PM"));
        }
      }
    }
    else {
      Serial.println(); //space between military time for Serial Monitor
    }

    if (myGPS.getDateValid() == false) {
      Serial.println(F("Date is invalid, not enough satellites in view!"));
    }
    if (myGPS.getTimeValid() == false) {
      Wire.beginTransmission(s7sAddress);
      Wire.write(0x7B); // Digit 1 control
      Wire.write(0b01000000); // display `-`
      Wire.write(0x7C); // Digit 2 control
      Wire.write(0b01000000); // display `-`
      Wire.write(0x7D);  // Digit 3 control
      Wire.write(0b01000000); // display `-`
      Wire.write(0x7E); // Digit 4 control
      Wire.write(0b01000000); // display `-`
      Wire.endTransmission();
      delay(50); //small delay so 7-segment display has time to pick up commands or characters
      
      Serial.println(F("Time is invalid, not enough satellites in view!"));
    }

    Serial.println();
  }

}




// This custom function works somewhat like a serial.print.
//  You can send it an array of chars (string) and it'll print
//  the first 4 characters in the array.
void s7sSendStringI2C(String toSend) {
  Wire.beginTransmission(s7sAddress);
  for (int i = 0; i < 4; i++)
  {
    Wire.write(toSend[i]);
  }
  Wire.endTransmission();
}

// This custom function works somewhat like a serial.print.
//  You can send it an array of chars (string) and it'll print
//  the first character in the array.
void s7sSendCharI2C(String toSend) {
  Wire.beginTransmission(s7sAddress);
  for (int i = 0; i < 1; i++)
  {
    Wire.write(toSend[i]);
  }
  Wire.endTransmission();
}

// This custom function works somewhat like a serial.print.
//  You can send it an array of chars (string) and it'll print
//  the first character in the array.
void s7sSend2xCharI2C(String toSend) {
  Wire.beginTransmission(s7sAddress);
  for (int i = 0; i < 2; i++)
  {
    Wire.write(toSend[i]);
  }
  Wire.endTransmission();
}


// Send the clear display command (0x76)
//  This will clear the display and reset the cursor
void clearDisplayI2C() {
  Wire.beginTransmission(s7sAddress);
  Wire.write(0x76);  // Clear display command
  Wire.endTransmission();
}




// Set the displays brightness. Should receive byte with the value
//  to set the brightness to
//  dimmest------------->brightest
//     0--------127--------255
void setBrightnessI2C(byte value) {
  Wire.beginTransmission(s7sAddress);
  Wire.write(0x7A);  // Set brightness command byte
  Wire.write(value);  // brightness data byte
  Wire.endTransmission();
}




// Turn on any, none, or all of the decimals.
//  The six lowest bits in the decimals parameter sets a decimal
//  (or colon, or apostrophe) on or off. A 1 indicates on, 0 off.
//  [MSB] (X)(X)(Apos)(Colon)(Digit 4)(Digit 3)(Digit2)(Digit1)
void setDecimalsI2C(byte decimals) {
  Wire.beginTransmission(s7sAddress);
  Wire.write(0x77);
  Wire.write(decimals);
  Wire.endTransmission();
}
