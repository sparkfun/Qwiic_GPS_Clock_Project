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

  The output on the Qwiic SerLCD is just digital. Depending on personal preference, you can view
  the time in regular 12-hour format or miltary 24-hour format.

  Feel like supporting open source hardware?
  Buy a board from SparkFun!
  ZED-F9P RTK2: https://www.sparkfun.com/products/15136
  NEO-M8P RTK: https://www.sparkfun.com/products/15005
  SAM-M8Q: https://www.sparkfun.com/products/15106

  Hardware Connections:
  Plug a Qwiic cable into the GPS, Qwiic SerLCD, and the RedBoard Qwiic.
  If you don't have a platform with a Qwiic connection use the SparkFun Qwiic Breadboard Jumper (https://www.sparkfun.com/products/14425)
  Open the serial monitor at 115200 baud to see the output.
*/

#include <Wire.h> //Needed for I2C to GPS
#include "SparkFun_Ublox_Arduino_Library.h" //http://librarymanager/All#SparkFun_Ublox_GPS
#define SerLCD_Address 0x72  //If using SerLCD with I2C

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
boolean military = false; //adjust for miltary (24-hr mode) or AM/PM (12-hr mode)
boolean AM = false; //AM or PM?

// Use these variables to set the initial time: 3:03:00
int hours = 3;
int minutes = 3;
int seconds = 0;

//Tid Bit: https://www.sparkfun.com/news/2571#yearOrigin
int years = 2020; //year that SparkFun was founded!
int months = 9;  //month that SparkFun was founded!
int days = 1;    //day that SparkFun was founded!

// How fast do you want the clock to update? Set this to 1 for fun.
// Set this to 1000 to get _about_ 1 second timing.
const int CLOCK_SPEED = 1000;
unsigned long lastDraw = 0;




void setup() {
  Serial.begin(115200);
  //while (!Serial)
  //  ; //Wait for user to open terminal
  Serial.println("SparkFun Ublox Example");

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

  Wire.beginTransmission(SerLCD_Address);
  Wire.write('|'); //Put LCD into setting mode
  Wire.write('-'); //Send clear display command

  Wire.write('|'); //Put LCD into setting mode
  Wire.write('+'); //Send the Set RGB command
  Wire.write(0xFF); //Send the red value
  Wire.write(0x00); //Send the green value
  Wire.write(0x00); //Send the blue value
  Wire.endTransmission();

}//end setup()




void loop() {

  update_Time();   //adjust UTC date/time based on time zone and DST

  displayDigital_Date_Time();  //after calculating, display the date and time on the SerLCD

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
char day_of_week()
{
  //Devised by Tomohiko Sakamoto in 1993, it is accurate for any Gregorian date:
  static int t[] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4  };
  years -= months < 3;
  return (years + years / 4 - years / 100 + years / 400 + t[months - 1] + days) % 7;
}




void displayDigital_Date_Time() {
  if (lastDraw + CLOCK_SPEED < millis()) {
    lastDraw = millis();

    Wire.beginTransmission(SerLCD_Address); // transmit to device #1
    Wire.write('|'); //Put LCD into setting mode
    Wire.write('-'); //Send clear display command
    Wire.print("Date: ");
    Serial.print("Date: ");

    //display month in same position
    if (months <= 9) {
      Wire.print(" ");
      Serial.print(" ");
    }
    Wire.print(String(months) + "-");
    Serial.print(String(months) + "-" );

    //display day and years in the same position
    if (days <= 9) {
      Wire.print("0");
      Serial.print("0");
    }
    Wire.print(String(days) + "-" + String(years));
    Wire.endTransmission(); //Stop I2C transmission
    Serial.println(String(days) + "-" + String(years));





    Wire.beginTransmission(SerLCD_Address); // transmit to device #1
    Wire.write(254); //Send command character
    Wire.write(128 + 64); //Change the position (128) of the cursor to 2nd row (64), position 9 (9)
    Wire.print("Time: ");
    Serial.print("Time: ");

    //display hours in the same position
    if (hours <= 9) {
      Wire.print(" ");
      Serial.print(" ");

    }
    Wire.print(String(hours) + ":" );
    Serial.print(String(hours) + ":" );

    //display minutes in same position
    if (minutes <= 9) {
      Wire.print("0");
      Serial.print("0");
    }
    Wire.print(String(minutes) + ":");
    Serial.print(String(minutes) + ":");

    //display seconds in same position
    if (seconds <= 9) {
      Wire.print("0");
      Serial.print("0");

    }
    Wire.print(String(seconds));
    Serial.print(String(seconds));


    if (military == false) {
      if (AM == true) {
        Wire.print(" AM");
        Serial.println(" AM");

      }
      else {
        if (AM == false) {
          Wire.print("PM");
          Serial.println(" PM");
        }
      }
    }
    else {
      Serial.println(); //space between military time for Serial Monitor
    }

    if (myGPS.getDateValid() == false) {
      Wire.write(254); //Send command character
      Wire.write(128 + 0 + 5); //Change the position (128) of the cursor to 2nd row (64), position 9 (9)
      Wire.print("X");
      Serial.println(F("Date is invalid, not enough satellites in view!"));
    }
    if (myGPS.getTimeValid() == false) {
      Wire.write(254); //Send command character
      Wire.write(128 + 64 + 5); //Change the position (128) of the cursor to 2nd row (64), position 9 (9)
      Wire.print("X");
      Serial.println(F("Time is invalid, not enough satellites in view!"));
    }

    Wire.endTransmission(); //Stop I2C transmission
    Serial.println();

  }

}



//https://learn.sparkfun.com/tutorials/avr-based-serial-enabled-lcds-hookup-guide/i2c-hardware-hookup--example-code---basic
//Given a number, i2cSendValue chops up an integer into four values and sends them out over I2C
void i2cSendValue(int value) {
  //Example of how to send value to the SerLCD
  
  Wire.beginTransmission(SerLCD_Address); // transmit to device #1

  Wire.write('|'); //Put LCD into setting mode
  Wire.write('-'); //Send clear display command

  Wire.print("Cycles: ");
  Wire.print(value);

  Wire.endTransmission(); //Stop I2C transmission
}
