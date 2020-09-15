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

  The output on the Qwiic microOLED can be in digital or analog form. Depending on personal preference, you can view
  the time in regular 12-hour format or miltary 24-hour format. Shout out to Jim for the initial analog clock example code
  for 12-hour format! Due to the size of the RAM on the RedBoard Qwiic's ATmega328P, the output to the 
  Arduino serial monitor is disabled. You can upgrade to the Qwiic Micro with SAMD21 which has been tested
  to work with the both. The Qwiic Micro has a bigger RAM so it can handle both outputs. 
  

  Feel like supporting open source hardware?
  Buy a board from SparkFun!
  ZED-F9P RTK2: https://www.sparkfun.com/products/15136
  NEO-M8P RTK: https://www.sparkfun.com/products/15005
  SAM-M8Q: https://www.sparkfun.com/products/15106

  Hardware Connections:
  Plug a Qwiic cable into the GPS, Qwiic microOLED, and the RedBoard Qwiic.
  If you don't have a platform with a Qwiic connection use the SparkFun Qwiic Breadboard Jumper (https://www.sparkfun.com/products/14425)
*/

#include <Wire.h> //Needed for I2C to GPS
#include "SparkFun_Ublox_Arduino_Library.h" //http://librarymanager/All#SparkFun_Ublox_GPS
#include <SFE_MicroOLED.h>   // http://librarymanager/All#Sparkfun_micro_oled_breakout

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

#define PIN_RESET 7          // A pin needs to be declared even though we are not physically connecting to the Qwiic micro OLED via I2C
#define DC_JUMPER 1
MicroOLED oled(PIN_RESET, DC_JUMPER);    // I2C declaration

// Use these variables to set the initial time: 3:03:00
int hours = 3;
int minutes = 3;
int seconds = 0;

//Tid Bit: https://www.sparkfun.com/news/2571#yearOrigin
int years = 2003; //year that SparkFun was founded!
int months = 1;  //month that SparkFun was founded!
int days = 3;    //day that SparkFun was founded!

// How fast do you want the clock to spin? Set this to 1 for fun.
// Set this to 1000 to get _about_ 1 second timing.
const int CLOCK_SPEED = 1000;
unsigned long lastDraw = 0;

// Global variables to help draw the clock face:
const int MIDDLE_Y = oled.getLCDHeight() / 2;
const int MIDDLE_X = oled.getLCDWidth() / 2;

int CLOCK_RADIUS;
int POS_12_X, POS_12_Y;
int POS_3_X, POS_3_Y;
int POS_6_X, POS_6_Y;
int POS_9_X, POS_9_Y;
int S_LENGTH;
int M_LENGTH;
int H_LENGTH;





void setup(){
  //Serial.begin(115200);
  //while (!Serial)
  //  ; //Wait for user to open terminal
  //Serial.println(F("SparkFun Ublox Example"));

  Wire.begin();
  Wire.setClock(400000);   // Set clock speed to be the fastest for better communication (fast mode)


  if (myGPS.begin() == false) //Connect to the Ublox module using Wire port
  {
    //Serial.println(F("Ublox GPS not detected at default I2C address. Please check wiring. Freezing."));
    while (1)
      ;
  }

  myGPS.setI2COutput(COM_TYPE_UBX); //Set the I2C port to output UBX only (turn off NMEA noise)
  myGPS.saveConfiguration();        //Save the current settings to flash and BBR

  oled.begin();    // Initialize the OLED
  oled.clear(ALL); // Clear the display's internal memory
  oled.display();  // Display what's in the buffer (splashscreen)
  delay(1000);     // Delay 1000 ms
  oled.clear(PAGE); // Clear the buffer.

  initClockVariables();

  oled.clear(ALL);
  drawFace();
  drawArms(hours, minutes, seconds);
  oled.display(); // display the memory buffer drawn

}




void loop(){

  update_Time();   //adjust UTC date/time based on time zone and DST

  displayDigital_Date_Time();  //after calculating, display the date and time
  
  //displayAnalog_Time(); //display time

} //end loop




// Simple function to increment seconds and then increment minutes
// and hours if necessary.
void update_Time(){

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

    oled.clear(PAGE);          // Clear the display, this is already called before we enter this function

    oled.setCursor(0, 0);        // Set cursor to top-left
    oled.setFontType(0);         // Smallest font
    oled.print(F(" GPS Clock "));   // Print
    //Serial.print(F("Date: "));

    oled.setCursor(0, 14);

    if (months <= 9) {
      oled.print(F(" "));             // Print
      //Serial.print(F(" "));
    }
    oled.print(String(months) + '/');
    //Serial.print(String(months) + '-');


    if (days <= 9) {
      oled.print(F("0"));             // Print
      //Serial.print(F("0"));
    }
    oled.print(String(days) + '/' + String(years));
    //Serial.println(String(days) + '-' + String(years));





    //Serial.print(F("Time: "));

    oled.setCursor(3, 28);
    oled.print(" ");
    if (hours <= 9) {
      oled.print(' ');
      //Serial.print(' ');

    }
    oled.print( String(hours) + ':' );
    //Serial.print(String(hours) + ':' );

    if (minutes <= 9) {
      oled.print(F("0"));
      //Serial.print(F("0"));
    }
    oled.print(String(minutes) + ':' );
    //Serial.print(String(minutes) + ':');


    if (seconds <= 9) {
      oled.print(F("0"));
      //Serial.print(F("0"));

    }
    oled.print(String(seconds));
    //Serial.print(String(seconds));


    oled.setCursor(27, 40);

    if (military == false) {
      if (AM == true) {
        oled.print(F("AM"));
        //Serial.println(F(" AM"));
      }
      else {
        if (AM == false) {
          oled.print(F("PM"));
          //Serial.println(F(" PM"));
        }
      }
    }
    else {
      //Serial.println(F("")); //space between military time for Serial Monitor
    }


    if (myGPS.getDateValid() == false) {
      oled.setCursor(0, 40);
      oled.print(F("!D"));
      //Serial.println(F("Date is invalid, not enough satellites in view!"));
    }
    if (myGPS.getTimeValid() == false) {
      oled.setCursor(53, 40);
      oled.print(F("!T"));
      //Serial.println(F("Time is invalid, not enough satellites in view!"));
    }


    oled.display();            // Update the display
    //Serial.println();
  }

}




void initClockVariables() {
  // Calculate constants for clock face component positions:
  oled.setFontType(0);
  CLOCK_RADIUS = min(MIDDLE_X, MIDDLE_Y) - 1;
  POS_12_X = MIDDLE_X - oled.getFontWidth();
  POS_12_Y = MIDDLE_Y - CLOCK_RADIUS + 2;
  POS_3_X  = MIDDLE_X + CLOCK_RADIUS - oled.getFontWidth() - 1;
  POS_3_Y  = MIDDLE_Y - oled.getFontHeight() / 2;
  POS_6_X  = MIDDLE_X - oled.getFontWidth() / 2;
  POS_6_Y  = MIDDLE_Y + CLOCK_RADIUS - oled.getFontHeight() - 1;
  POS_9_X  = MIDDLE_X - CLOCK_RADIUS + oled.getFontWidth() - 2;
  POS_9_Y  = MIDDLE_Y - oled.getFontHeight() / 2;

  // Calculate clock arm lengths
  S_LENGTH = CLOCK_RADIUS - 2;
  M_LENGTH = S_LENGTH * 0.7;
  H_LENGTH = S_LENGTH * 0.5;
}




// Draw the clock's three arms: seconds, minutes, hours.
void drawArms(int h, int m, int s) {
  double midHours;  // this will be used to slightly adjust the hour hand
  static int hx, hy, mx, my, sx, sy;

  // Adjust time to shift display 90 degrees ccw
  // this will turn the clock the same direction as text:
  if (military == true) {
    h -= 6;
  }
  else {//military == false, i.e. 12 mode
    h -= 3;
  }
  m -= 15;
  s -= 15;

  if (h <= 0) {
    if (military == true) {
      h += 24;
    }
    else {
      h += 12;
    }
  }


  if (m < 0)
    m += 60;
  if (s < 0)
    s += 60;

  // Calculate and draw new lines:
  s = map(s, 0, 60, 0, 360);  // map the 0-60, to "360 degrees"
  sx = S_LENGTH * cos(PI * ((float)s) / 180);  // woo trig!
  sy = S_LENGTH * sin(PI * ((float)s) / 180);  // woo trig!
  // draw the second hand:
  oled.line(MIDDLE_X, MIDDLE_Y, MIDDLE_X + sx, MIDDLE_Y + sy);

  m = map(m, 0, 60, 0, 360);  // map the 0-60, to "360 degrees"
  mx = M_LENGTH * cos(PI * ((float)m) / 180);  // woo trig!
  my = M_LENGTH * sin(PI * ((float)m) / 180);  // woo trig!
  // draw the minute hand
  oled.line(MIDDLE_X, MIDDLE_Y, MIDDLE_X + mx, MIDDLE_Y + my);

  if (military == true) {
    midHours = minutes / 6.0; // midHours is used to set the hours hand to middling levels between whole hours
    h *= 10;  // Get hours and midhours to the same scale
    h += midHours;  // add hours and midhours
    h = map(h, 0, 24, 0, 360);  // map the 0-24, to "360 degrees"
    h = h / 10;

  }
  else {
    midHours = minutes / 6.0; // midHours is used to set the hours hand to middling levels between whole hours
    h *= 10;  // Get hours and midhours to the same scale
    h += midHours;  // add hours and midhours
    h = map(h, 0, 12, 0, 360);  // map the 0-12, to "360 degrees"
    h = h / 10; //scale back down

  }

  hx = H_LENGTH * cos(PI * ((float)h) / 180);  // woo trig!
  hy = H_LENGTH * sin(PI * ((float)h) / 180);  // woo trig!
  // draw the hour hand:
  oled.line(MIDDLE_X, MIDDLE_Y, MIDDLE_X + hx, MIDDLE_Y + hy);
}




// Draw an analog clock face
void drawFace() {
  // Draw the clock border
  oled.circle(MIDDLE_X, MIDDLE_Y, CLOCK_RADIUS);

  if (military == false) {
    // Draw the clock numbers for 12-hour mode
    oled.setFontType(0); // set font type 0, please see declaration in SFE_MicroOLED.cpp
    oled.setCursor(POS_12_X, POS_12_Y); // points cursor to x=27 y=0
    oled.print(12);
    oled.setCursor(POS_6_X, POS_6_Y);
    oled.print(6);
    oled.setCursor(POS_9_X, POS_9_Y);
    oled.print(9);
    oled.setCursor(POS_3_X, POS_3_Y);
    oled.print(3);
  }
  else {
    // Draw the clock numbers for 24-hour mode
    oled.setFontType(0); // set font type 0, please see declaration in SFE_MicroOLED.cpp
    oled.setCursor(POS_12_X, POS_12_Y); // points cursor to x=27 y=0
    oled.print(24);
    oled.setCursor(POS_6_X - 3, POS_6_Y);
    oled.print(12);
    oled.setCursor(POS_9_X, POS_9_Y);
    oled.print(18);
    oled.setCursor(POS_3_X - 3, POS_3_Y);
    oled.print(F("6"));
  }
}




void displayAnalog_Time() {
  // Check if we need to update seconds, minutes, hours:
  if (lastDraw + CLOCK_SPEED < millis())
  {
    lastDraw = millis();

    // Draw the clock:
    oled.clear(PAGE);  // Clear the buffer

    drawFace();  // Draw the face to the buffer
    drawArms(hours, minutes, seconds);  // Draw arms to the buffer


    oled.setFontType(0);         // Smallest font
    oled.setCursor(52, 40);

    if (military == false) {
      if (AM == true) {
        oled.print(F("AM"));
        //Serial.println(F(" AM"));
      }
      else {
        if (AM == false) {
          oled.print(F("PM"));
          //Serial.println(F(" PM"));
        }
      }
    }


    if (myGPS.getTimeValid() == false) {
      oled.setCursor(0, 40);
      oled.print(F("!T"));
      //Serial.println(F("Time is invalid, not enough satellites in view!"));
    }

    
    oled.display(); // Draw the memory buffer
  }
}
