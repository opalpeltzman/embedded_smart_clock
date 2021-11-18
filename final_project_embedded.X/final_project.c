/*
 * File:   final_project.c
 * Author: opal peltzman
 *
 * Created on June 14, 2021, 9:36 AM
 */


#include <xc.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "p24FJ256GA705.h"
#include "System/system.h"
#include "System/delay.h"
#include "oledDriver/oledC.h"
#include "oledDriver/oledC_colors.h"
#include "oledDriver/oledC_shapes.h"

/* menu */
int menu_choice[3] = {0};
int in_menu = -1;
bool in_menu_flag = false;                  // indicates whether s1 was pressed once or for 3 sec

/* clock */
int clock_mode;                     // indicates whether clock is in digital or analog mode

/* digital clock variables */
typedef struct{
    int seconds;
    int minutes;
    int hours;
    bool long_press;            // indicates whether s2 was pressed once or for 3 sec
    bool setting;               // indicated if setting clock mode
    int indicates;              // indicated which field is changing
    // for o-led screen representation
    char zero[20];
    char h[20];
    char m[20];
    char s[20];
}digClock; 
digClock dig_clock; 

/* indicates which digital clock to display */
typedef struct{  
    bool display_small; //small clock to display in menus
    bool display_big;
}clockposition; 
clockposition clock_position;

/* all clock features */
typedef struct{
    bool analog;    //analog mode
    bool digital;   // digital mode
    bool Interval24;
    bool Interval12;
    int am_pm;
}clockDisplay; 
clockDisplay clock_display;

/* alarm object */
typedef struct{
    bool on;    // indicates if alarm is on
    int hour;
    int minute;
    bool setting_alarm; //indicates if in alarm setting mode
    bool ring;          //indicates to alarm user
    int count_time;     //counts 20 seconds to turn of alarm
}alarm_obj; 
alarm_obj alarm;

/* date object */
typedef struct{
    char str_month[20];
    char zero[20];
    int month;
    bool switch_day;
    bool display_date;
    char str_day[20];
    int day;
    bool date_setting;  // indicates if in date setting mode
}date_obj; 
date_obj date;

/* analog clock variables */
int init_clock_hands;
int prev_seconds; // seconds prev coordinates
int cor_indx;     // seconds counter & indx 

int prev_minutes; // minutes prev coordinates
int minutes_indx; // minutes counter & indx 

int prev_hours; // hours prev coordinates
int hours_indx; // hours counter & indx
int move_hours; // indicates hours change

/* converting between analog and digital variables */    
int indx_hour_setting;  // current user hour setting in set function
int updated_hour_indx;  // according to interval and converting function, indicates updated hour value
int in_setting_clock;   // indicates whether to update am-pm 

/* analog clock coordinates */    
uint8_t coordinates[60][2] = {{47,7},{51,7},{55,8},{59,9},{63,10},{67,12},{71,15},{74,17},{77,20},{79,23},{82,27},{84,31},{85,35},{86,39},{87,43},
                                    {87,47},{87,51},{86,55},{85,59},{84,63},{82,67},{79,71},{77,74},{74,77},{71,79},{67,82},{63,84},{59,85},{55,86},{51,87},
                                    {47,87},{43,87},{39,86},{35,85},{31,84},{27,82},{23,79},{20,77},{17,74},{15,71},{12,67},{10,63},{9,59},{8,55},{7,51},
                                    {7,47},{7,43},{8,39},{9,35},{10,31},{12,27},{15,23},{17,20},{20,17},{23,15},{27,12},{31,10},{35,9},{39,8},{43,7}};       
uint8_t center[] = {47, 47};
/* month of the year list */ 
int months[13] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

/* initialize variables */
static void User_Initialize(void)
{   
    oledC_setBackground(OLEDC_COLOR_BLACK); //lit up oled
    oledC_clearScreen();                    //lit up oled
    
    /* Timer configuration */
    T1CONbits.TON = 1;      //timer1 on/enable
    T1CONbits.TSIDL = 1;
    T1CONbits.TCKPS = 0b11; //pre-scaler 1:255
    T1CONbits.TCS = 0;      //internal clock
    
    /* LED1 & LED2 configuration */
    //digital input
    ANSA = 0;
    TRISAbits.TRISA11 = 1;
    TRISAbits.TRISA12 = 1; 
    //digital output
    TRISAbits.TRISA8 = 0; 
    TRISAbits.TRISA9 = 0;
    
    /* clock hands display initialize */
    init_clock_hands = 1;       //flag for clock hands initialization
    clock_display.am_pm = 1;    // pm
    
    /* digital clock initialize */ 
    dig_clock.hours = 12;
    dig_clock.minutes = 59;
    dig_clock.seconds = 50;
    dig_clock.setting = false;
    in_setting_clock = 0;
    clock_position.display_small = false;// small clock
    clock_position.display_big = true; // big clock 
    clock_display.digital = true;
    clock_display.Interval24 = false;
    clock_display.Interval12 = true;
    
    /* analog clock initialize */       
    cor_indx = 50;// seconds
    minutes_indx = 59;// minutes
    hours_indx = 4;//hours
    
    /* alarm initialize */ 
    alarm.setting_alarm = false;
    alarm.on = false;
    alarm.ring = false;
    alarm.count_time = 0;
    
    /* date initialize */ 
    date.day = 2;
    date.month = 8;
    strcpy(date.zero, "0");
    date.switch_day = false;  
    date.display_date = true;
    date.date_setting = false;
}

/* enable timer interrupts and set count limit */
void initialize_timer_ISR(){
    // initialize the system
    TMR1 = 0;               //timer count
    PR1 = 16569;            //count limit (present count)
    
    IFS0bits.T1IF = 0;      //flag for interrupt   
    IEC0bits.T1IE = 1;      //enable interrupt
//    IPC0[14:12];          //priority
}

/* Display alarm when alarm mode is on according to alarm setting */
void alarm_display(){
    oledC_DrawRing(8, 8, 5, 1, OLEDC_COLOR_CRIMSON);
    DELAY_milliseconds(200);
    if(alarm.ring){oledC_DrawRing(8, 8, 5, 1, OLEDC_COLOR_BLACK); alarm.count_time++;}
}

/* Display am-pm */
void am_pm_display(int choose, int color){
    char* am = "am";
    char* pm = "pm";
    char* output;
    if(choose == 0){strcpy(output, am);}
    else if(choose == 1){strcpy(output, pm);}
    if(color == 0){oledC_DrawString(5, 80, 1, 2, output, OLEDC_COLOR_BLACK);}
    else if(color == 1){oledC_DrawString(5, 80, 1, 2, output, OLEDC_COLOR_WHITE);}
}

/* Display clock and hour, minute, seconds hands */
void analog_clock_display(){
   
    oledC_DrawThickPoint(center[0], center[1], 2, OLEDC_COLOR_WHITE);
    am_pm_display(clock_display.am_pm, 1);
    int indx = 0;
    int i = 0;
    for(i; i < 4; i++){
        int xTemp = center[0] + (9*(coordinates[indx][0]-center[0])/10);
        int yTemp = center[1] + (9*(coordinates[indx][1]-center[1])/10);
        oledC_DrawLine(coordinates[indx][0], coordinates[indx][1],xTemp, yTemp, 3, OLEDC_COLOR_BLUEVIOLET);
        
        indx += 15;
    }
    
    indx = 5;
    i = 0;
    for(i; i < 8; i++){
        if(indx == 15 || indx == 30 || indx == 45 || indx == 50){indx += 5;}
        int xTemp = center[0] + (9*(coordinates[indx][0]-center[0])/10);
        int yTemp = center[1] + (9*(coordinates[indx][1]-center[1])/10);
        oledC_DrawLine(coordinates[indx][0], coordinates[indx][1],xTemp, yTemp, 1, OLEDC_COLOR_BLUEVIOLET);
        
        indx += 5;
    }
    
    minutes_display(coordinates[minutes_indx][0], coordinates[minutes_indx][1], coordinates[prev_minutes][0], coordinates[prev_minutes][1],0);
    hours_display(coordinates[hours_indx][0], coordinates[hours_indx][1], coordinates[prev_hours][0], coordinates[prev_hours][1],0);
}

/* display seconds according to analog clock */
void seconds_display(int x, int y, int prev_x, int prev_y, int erase){
    int xTemp;
    int yTemp;
    if(erase){
        /* previous */
        xTemp = center[0] + (8*(prev_x-center[0])/10);
        yTemp = center[1] + (8*(prev_y-center[1])/10);
        oledC_DrawLine(center[0], center[1], xTemp, yTemp, 1, OLEDC_COLOR_BLACK);
    }
    /*  current */
    xTemp = center[0] + (8*(x-center[0])/10);
    yTemp = center[1] + (8*(y-center[1])/10);
    oledC_DrawLine(center[0], center[1], xTemp, yTemp, 1, OLEDC_COLOR_WHITE);
}

/* display minutes according to analog clock */
void minutes_display(int x, int y, int prev_x, int prev_y, int erase){
    int xTemp;
    int yTemp;
    if(erase){
        /* previous */
        xTemp = prev_x - (prev_x-center[0])/5;
        yTemp = prev_y - (prev_y-center[1])/5;
        oledC_DrawLine(center[0], center[1], xTemp, yTemp, 2, OLEDC_COLOR_BLACK);
    }
    /*  current */
    xTemp = x - (x-center[0])/5;
    yTemp = y - (y-center[1])/5;
    oledC_DrawLine(center[0], center[1], xTemp, yTemp, 2, OLEDC_COLOR_WHITE);
}

/* display hours according to analog clock */
void hours_display(int x, int y, int prev_x, int prev_y, int erase){
    int xTemp;
    int yTemp;
    if(erase){
        /* previous */
        xTemp = center[0] + (prev_x-center[0])/2;
        yTemp = center[1] + (prev_y-center[1])/2;
        oledC_DrawLine(center[0], center[1], xTemp, yTemp, 3, OLEDC_COLOR_BLACK);
    }
    /*  current */
    xTemp = center[0] + (x-center[0])/2;
    yTemp = center[1] + (y-center[1])/2;
    oledC_DrawLine(center[0], center[1], xTemp, yTemp, 3, OLEDC_COLOR_WHITE);
}

/* update analog clock hands according to timer interrupt */
void update_analog(bool display, int erase, char* field){
    if(display){
        if(erase){// erase previous hand
            if(strcmp(field, "h") == 0){hours_display(coordinates[hours_indx][0], coordinates[hours_indx][1], coordinates[prev_hours][0], coordinates[prev_hours][1],1);}
            else if(strcmp(field, "m") == 0){minutes_display(coordinates[minutes_indx][0], coordinates[minutes_indx][1], coordinates[prev_minutes][0], coordinates[prev_minutes][1], 1);}
            else if(strcmp(field, "s") == 0){seconds_display(coordinates[cor_indx][0], coordinates[cor_indx][1], coordinates[prev_seconds][0], coordinates[prev_seconds][1], 1);}
        }
        else if(!erase){// do not erase previous hand
            if(strcmp(field, "h") == 0){hours_display(coordinates[hours_indx][0], coordinates[hours_indx][1], coordinates[prev_hours][0], coordinates[prev_hours][1],0);}
            else if(strcmp(field, "m") == 0){minutes_display(coordinates[minutes_indx][0], coordinates[minutes_indx][1], coordinates[prev_minutes][0], coordinates[prev_minutes][1], 0);}
            else if(strcmp(field, "s") == 0){seconds_display(coordinates[cor_indx][0], coordinates[cor_indx][1], coordinates[prev_seconds][0], coordinates[prev_seconds][1], 0);}
        }
    }
}

/* convert analog (hour) time to digital */
void convert_analog_digital(bool set){
    
    int updated_hour;
    int current_hour;
    
    if(set){current_hour = indx_hour_setting;}
    else if(!set){current_hour = hours_indx;}
    
    // if not in initialization phase switch between am and pm when returning to 01\13 //
    if(in_setting_clock == 0){
        if(move_hours == 1 && dig_clock.setting == false && hours_indx == 5 && init_clock_hands == 0 ){   
            oledC_DrawPoint(1, 1, OLEDC_COLOR_BLACK);
            if(clock_display.Interval12 && clock_position.display_big){am_pm_display(clock_display.am_pm, 0);}
            if(clock_display.am_pm == 0){clock_display.am_pm = 1;}
            else if(clock_display.am_pm == 1){clock_display.am_pm = 0;}
            if(clock_display.Interval12 && clock_position.display_big){am_pm_display(clock_display.am_pm, 1);}
        }
    }
    
    if(clock_display.Interval12 || (clock_display.Interval24 && clock_display.am_pm == 0)){
        // for 12 interval or for 24 interval and am //
        if(current_hour >= 0 && current_hour < 5){updated_hour = 12;}
        else if(current_hour >= 5 && current_hour < 10){updated_hour = 1;}
        else if(current_hour >= 10 && current_hour < 15){updated_hour = 2;}
        else if(current_hour >= 15 && current_hour < 20){updated_hour = 3;}
        else if(current_hour >= 20 && current_hour < 25){updated_hour = 4;}
        else if(current_hour >= 25 && current_hour < 30){updated_hour = 5;}
        else if(current_hour >= 30 && current_hour < 35){updated_hour = 6;}
        else if(current_hour >= 35 && current_hour < 40){updated_hour = 7;}
        else if(current_hour >= 40 && current_hour < 45){updated_hour = 8;}
        else if(current_hour >= 45 && current_hour < 50){updated_hour = 9;}
        else if(current_hour >= 50 && current_hour < 55){updated_hour = 10;}
        else if(current_hour >= 55 && current_hour < 60){updated_hour = 11;}
    }
    
    if(clock_display.Interval24 && clock_display.am_pm == 1){
        // for 24 interval and pm
        if(current_hour >= 0 && current_hour < 5){updated_hour = 24;}
        else if(current_hour >= 5 && current_hour < 10){updated_hour = 13;}
        else if(current_hour >= 10 && current_hour < 15){updated_hour = 14;}
        else if(current_hour >= 15 && current_hour < 20){updated_hour = 15;}
        else if(current_hour >= 20 && current_hour < 25){updated_hour = 16;}
        else if(current_hour >= 25 && current_hour < 30){updated_hour = 17;}
        else if(current_hour >= 30 && current_hour < 35){updated_hour = 18;}
        else if(current_hour >= 35 && current_hour < 40){updated_hour = 19;}
        else if(current_hour >= 40 && current_hour < 45){updated_hour = 20;}
        else if(current_hour >= 45 && current_hour < 50){updated_hour = 21;}
        else if(current_hour >= 50 && current_hour < 55){updated_hour = 22;}
        else if(current_hour >= 55 && current_hour < 60){updated_hour = 23;}
    }
    
    if(set){updated_hour_indx = updated_hour;}
    else if(!set){dig_clock.hours = updated_hour;}
}

/* convert digital (hour) time to analog */
void convert_digital_analog(int updated_minutes){
    
    int move_hour_indx = 0;

    if(updated_minutes >= 0 && updated_minutes < 12){move_hour_indx = 0;}
    else if(updated_minutes >= 12 && updated_minutes < 24){move_hour_indx = 1;}
    else if(updated_minutes >= 24 && updated_minutes < 36){move_hour_indx = 2; oledC_DrawRing(8, 8, 5, 1, OLEDC_COLOR_CRIMSON);}
    else if(updated_minutes >= 36 && updated_minutes < 48){move_hour_indx = 3;}
    else if(updated_minutes >= 48 && updated_minutes < 0){move_hour_indx = 4;}
    
    indx_hour_setting = 0;
  
    if(updated_hour_indx == 1 || updated_hour_indx == 13){indx_hour_setting = 5;}
    else if(updated_hour_indx == 2 || updated_hour_indx == 14){indx_hour_setting = 10;}
    else if(updated_hour_indx == 3 || updated_hour_indx == 15){indx_hour_setting = 15;}
    else if(updated_hour_indx == 4 || updated_hour_indx == 16){indx_hour_setting = 20;}
    else if(updated_hour_indx == 5 || updated_hour_indx == 17){indx_hour_setting = 25;}
    else if(updated_hour_indx == 6 || updated_hour_indx == 18){indx_hour_setting = 30;}
    else if(updated_hour_indx == 7 || updated_hour_indx == 19){indx_hour_setting = 35;}
    else if(updated_hour_indx == 8 || updated_hour_indx == 20){indx_hour_setting = 40;}
    else if(updated_hour_indx == 9 || updated_hour_indx == 21){indx_hour_setting = 45;}
    else if(updated_hour_indx == 10 || updated_hour_indx == 22){indx_hour_setting = 50;}
    else if(updated_hour_indx == 11 || updated_hour_indx == 23){indx_hour_setting = 55;}
    else if(updated_hour_indx == 12 || updated_hour_indx == 24){indx_hour_setting = 0;}

    
    indx_hour_setting += move_hour_indx;
}

/* update digital seconds */
void dig_seconds(){
    if(clock_position.display_big && dig_clock.setting == false){oledC_DrawString(70, 25, 1, 6, dig_clock.s, OLEDC_COLOR_BLACK);}
    if(clock_position.display_small){oledC_DrawString(84, 6, 1, 1, dig_clock.s, OLEDC_COLOR_BLACK);}
    sprintf(dig_clock.s, "%d", dig_clock.seconds);
    if(dig_clock.seconds < 10){strcpy(dig_clock.zero, "0"); strcat(dig_clock.zero, dig_clock.s); strcpy(dig_clock.s, dig_clock.zero);}
    if(clock_position.display_big && dig_clock.setting == false){oledC_DrawString(70, 25, 1, 6, dig_clock.s, OLEDC_COLOR_WHITE);}
    if(clock_position.display_small){oledC_DrawString(84, 6, 1, 1, dig_clock.s, OLEDC_COLOR_WHITE);}
}

/* update digital minutes */
void dig_minutes(){
    if(clock_position.display_big && dig_clock.setting == false){oledC_DrawString(40, 25, 1, 6, dig_clock.m, OLEDC_COLOR_BLACK);}
    if(clock_position.display_small){oledC_DrawString(66, 6, 1, 1, dig_clock.m, OLEDC_COLOR_BLACK);}
    sprintf(dig_clock.m, "%d", dig_clock.minutes);
    if(dig_clock.minutes < 10){strcpy(dig_clock.zero, "0"); strcat(dig_clock.zero, dig_clock.m); strcpy(dig_clock.m, dig_clock.zero);}
    if(clock_position.display_big && dig_clock.setting == false){oledC_DrawString(40, 25, 1, 6, dig_clock.m, OLEDC_COLOR_WHITE);}
    if(clock_position.display_small){oledC_DrawString(66, 6, 1, 1, dig_clock.m, OLEDC_COLOR_WHITE);}  
}

/* update digital hours */
void dig_hours(){
    
    if(clock_position.display_big && dig_clock.setting == false){oledC_DrawString(10, 25, 1, 6, dig_clock.h, OLEDC_COLOR_BLACK);}
    if(clock_position.display_small){oledC_DrawString(48, 6, 1, 1, dig_clock.h, OLEDC_COLOR_BLACK);}
    convert_analog_digital(false);
    sprintf(dig_clock.h, "%d", dig_clock.hours);
    if(dig_clock.hours < 10){strcpy(dig_clock.zero, "0"); strcat(dig_clock.zero, dig_clock.h); strcpy(dig_clock.h, dig_clock.zero);}
    if(clock_position.display_big && dig_clock.setting == false){oledC_DrawString(10, 25, 1, 6, dig_clock.h, OLEDC_COLOR_WHITE);}
    if(clock_position.display_small){oledC_DrawString(48, 6, 1, 1, dig_clock.h, OLEDC_COLOR_WHITE);}   
    
    /* update day in date to +1 when time is set to midnight */
    if(clock_display.am_pm == 1){// pm
        if(clock_display.Interval24 && dig_clock.hours == 24 && dig_clock.minutes == 0 && dig_clock.seconds == 0){date.day++; date.switch_day = true;}// 24 interval midnight = 24
        else if(clock_display.Interval12 && dig_clock.hours == 12 && dig_clock.minutes == 0 && dig_clock.seconds == 0){date.day++; date.switch_day = true;}// 12 interval midnight = 12pm
    }
}

/* update digital clock according to timer interrupt */
void update_digital(bool display, char* field){
    if(display){
        if(strcmp(field, "h") == 0){dig_hours();}
        else if(strcmp(field, "m") == 0){dig_minutes();}
        else if(strcmp(field, "s") == 0){dig_seconds();}
    }
}

/* display date */
void display_curr_date(){
    sprintf(date.str_day, "%d", date.day);
    if(date.day < 10){strcpy(date.zero, "0"); strcat(date.zero, date.str_day); strcpy(date.str_day, date.zero);}
    oledC_DrawString(66, 88, 1, 1, date.str_day, OLEDC_COLOR_WHITE);
    oledC_DrawString(78, 88, 1, 1, "/", OLEDC_COLOR_WHITE);
    sprintf(date.str_month, "%d", date.month);
    if(date.month < 10){strcpy(date.zero, "0"); strcat(date.zero, date.str_month); strcpy(date.str_month, date.zero);}
    oledC_DrawString(85, 88, 1, 1, date.str_month, OLEDC_COLOR_WHITE);  
}

/* update date */
void update_date(){
    if(date.switch_day){    // if day needs to change - happens when 00:00:00
        oledC_DrawString(66, 88, 1, 1, date.str_day, OLEDC_COLOR_BLACK);// delete previous day
        
        if(date.day > months[date.month]){ // switch month and reset day
            date.day = 1;
            oledC_DrawString(85, 88, 1, 1, date.str_month, OLEDC_COLOR_BLACK);// delete previous month
            date.month ++;
            if(date.month > 12){date.month = 1;}
            
            //update month
            sprintf(date.str_month, "%d", date.month);
            if(date.month < 10){strcpy(date.zero, "0"); strcat(date.zero, date.str_month); strcpy(date.str_month, date.zero);}
            if(date.display_date){oledC_DrawString(85, 88, 1, 1, date.str_month, OLEDC_COLOR_WHITE);} 
        }
    
        // update day
        sprintf(date.str_day, "%d", date.day);
        if(date.day < 10){strcpy(date.zero, "0"); strcat(date.zero, date.str_day); strcpy(date.str_day, date.zero);}
        if(date.display_date){oledC_DrawString(66, 88, 1, 1, date.str_day, OLEDC_COLOR_WHITE);}
    }
    date.switch_day = false;
}

/* Handle all analog & digital clock features: refresh, clock hands\fields display according to timer */
void analogDigital_clock_features(){
    /* seconds */
    int prev2;                
    move_hours = 0;
    
    cor_indx ++;
    /* seconds */
    if(cor_indx == 60){//start from 12 again
        cor_indx = 0;}
    
    if(cor_indx != 0){
        prev_seconds = cor_indx-1;
        /* seconds */
        update_analog(clock_display.analog, 1, "s");    // display only if analog
        dig_clock.seconds = cor_indx;
        update_digital(clock_display.digital, "s");     // display only if digital
    }
    else if(cor_indx == 0 && init_clock_hands == 0){         //hand on 12 
        prev_seconds = 59;
        /* seconds */
        update_analog(clock_display.analog, 1, "s");    // display only if analog
        dig_clock.seconds = cor_indx;
        update_digital(clock_display.digital, "s");     // display only if digital
        
        /* minutes */
        minutes_indx++;

        if(minutes_indx == 60){minutes_indx = 0;}
        if(minutes_indx == 0){prev_minutes = 59;}else{prev_minutes = minutes_indx-1;}
        if(minutes_indx == 12 || minutes_indx == 24 || minutes_indx == 36 || minutes_indx == 48 || minutes_indx == 0){move_hours = 1;}// move hour hand 
        
        prev2 = prev_minutes;
        prev2 -=1;
        if(prev2 == -1){prev2 = 59;}
        
        update_analog(clock_display.analog, 1, "m");    // display only if analog
        dig_clock.minutes = minutes_indx;
        update_digital(clock_display.digital, "m");     // display only if digital 
        
        /* In case minutes pass hour hand */
        if(prev_minutes == hours_indx || prev2 == hours_indx){
            update_analog(clock_display.analog, 0, "h");    // display only if analog
        }  
    }
    
    /* hours */
    if(init_clock_hands == 1){
        update_analog(clock_display.analog, 0, "h");    // display only if analog
    }
    else if(move_hours == 1){
        /* hours */
        hours_indx++;
        if(hours_indx == 60){hours_indx = 0;}
        if(hours_indx == 0){prev_hours = 59;}else{prev_hours = hours_indx-1;}
        
        prev2 = prev_hours;
        prev2 -=1;
        if(prev2 == -1){prev2 = 59;}
        
        update_analog(clock_display.analog, 1, "h");    // display only if analog
        dig_clock.hours = hours_indx;
        update_digital(clock_display.digital, "h");     // display only if digital
        
        /* In case hour pass minutes hand */
        if(prev_hours == minutes_indx || prev2 == minutes_indx){
            update_analog(clock_display.analog, 0, "m");    // display only if analog
        }
        move_hours = 0;
    }
    
    /* handle refresh */
    /* In case seconds pass minutes or hour hand */
    prev2 = prev_seconds;
    prev2 -=1;
    if(prev2 == -1){prev2 = 59;}
    if(prev_seconds == minutes_indx || prev2 == minutes_indx){
        update_analog(clock_display.analog, 0, "m");    // display only if analog
    }
    if(prev_seconds == hours_indx || prev2 == hours_indx){
        update_analog(clock_display.analog, 0, "h");    // display only if analog
    }
     
    init_clock_hands = 0;   //flag for clock hands initialization
    update_date();          // update date if needed
}

/* ISR timer handler */
void __attribute__((__interrupt__)) _T1Interrupt(void)
{
    analogDigital_clock_features();    // call timer features handle 
    IFS0bits.T1IF = 0;                  //flag for interrupt 
    
    /* handle alarm when it is on */
    if(alarm.hour == hours_indx && alarm.minute == minutes_indx){alarm.ring = true;}
    if(alarm.on){alarm_display();}
    if(alarm.count_time == 20){ //shut down alarm after 20
        alarm.on = false;
        alarm.ring = false;
        alarm.count_time = 0;   // reset count for alarm
        oledC_DrawRing(8, 8, 5, 1, OLEDC_COLOR_BLACK);
    }
}

/* handle whether s1 was pressed once or for 3 sec */
void button1Press(){
    
    //when S1 is pressed 
    if(PORTAbits.RA11 == 0){   
        PORTAbits.RA8 = 1;
        DELAY_milliseconds(400);
        if(PORTAbits.RA11 == 1){ 
            PORTAbits.RA8 = 0;
            in_menu_flag = false; 

        }
        //long S1 press 
        DELAY_milliseconds(1200);
        if(PORTAbits.RA11 == 0){
            PORTAbits.RA8 = 0;
            in_menu_flag = true; 
        }
        PORTAbits.RA8 = 0;
        DELAY_milliseconds(400);
    }
}

/* handle whether s2 was pressed once or for 3 sec */
void button2Press(){

    //when S1 is pressed 
    if(PORTAbits.RA12 == 0){   
        PORTAbits.RA9 = 1;
        DELAY_milliseconds(400);
        if(PORTAbits.RA12 == 1){ 
            PORTAbits.RA9 = 0;
            dig_clock.long_press = false; 
        }
        //long S1 press 
        DELAY_milliseconds(1200);
        if(PORTAbits.RA12 == 0){
            PORTAbits.RA9 = 0;
            dig_clock.long_press = true; 
        }
        PORTAbits.RA9 = 0;
        DELAY_milliseconds(300);
    }
}

/* digital clock display small and big clock */
void digital_clock_display(){

    if(clock_position.display_big){
        // big clock presentation
        dig_hours();
        oledC_DrawString(20, 25, 1, 6, " : ", OLEDC_COLOR_WHITE);
        dig_minutes();    
        oledC_DrawString(50, 25, 1, 6, " : ", OLEDC_COLOR_WHITE);
        dig_seconds();
        // am-pm display
        if(dig_clock.setting == false && clock_display.Interval12){am_pm_display(clock_display.am_pm, 1);}
        
    }
    
    if(clock_position.display_small){
        // small clock presentation
        dig_hours();
        oledC_DrawString(54, 6, 1, 1, " : ", OLEDC_COLOR_WHITE);
        dig_minutes();
        oledC_DrawString(72, 6, 1, 1, " : ", OLEDC_COLOR_WHITE);
        dig_seconds();
        
    }
}

/* drew rectangle in current setting field */
void rectangles(int field, int color){
    if(color == 0){//black = delete
        if(field == 0){//hours  
            oledC_DrawLine(5, 20, 25, 20, 3, OLEDC_COLOR_BLACK);
            oledC_DrawLine(5, 20, 5, 75, 3, OLEDC_COLOR_BLACK);
            oledC_DrawLine(5, 75, 25, 75, 3, OLEDC_COLOR_BLACK);
            oledC_DrawLine(25, 75, 25, 20, 3, OLEDC_COLOR_BLACK);
        }
        else if(field == 1){//minutes
            oledC_DrawLine(35, 20, 55, 20, 3, OLEDC_COLOR_BLACK);
            oledC_DrawLine(35, 20, 35, 75, 3, OLEDC_COLOR_BLACK);
            oledC_DrawLine(35, 75, 55, 75, 3, OLEDC_COLOR_BLACK);
            oledC_DrawLine(55, 75, 55, 20, 3, OLEDC_COLOR_BLACK);
        }
        else if(field == 2){//seconds
            oledC_DrawLine(65, 20, 85, 20, 3, OLEDC_COLOR_BLACK);
            oledC_DrawLine(65, 20, 65, 75, 3, OLEDC_COLOR_BLACK);
            oledC_DrawLine(65, 75, 85, 75, 3, OLEDC_COLOR_BLACK);
            oledC_DrawLine(85, 75, 85, 20, 3, OLEDC_COLOR_BLACK);
        }
    }else if(color == 1){//purple 
        if(field == 0){//hours
            oledC_DrawLine(5, 20, 25, 20, 3, OLEDC_COLOR_BLUEVIOLET);
            oledC_DrawLine(5, 20, 5, 75, 3, OLEDC_COLOR_BLUEVIOLET);
            oledC_DrawLine(5, 75, 25, 75, 3, OLEDC_COLOR_BLUEVIOLET);
            oledC_DrawLine(25, 75, 25, 20, 3, OLEDC_COLOR_BLUEVIOLET);
        }
        else if(field == 1){//minutes
            oledC_DrawLine(35, 20, 55, 20, 3, OLEDC_COLOR_BLUEVIOLET);
            oledC_DrawLine(35, 20, 35, 75, 3, OLEDC_COLOR_BLUEVIOLET);
            oledC_DrawLine(35, 75, 55, 75, 3, OLEDC_COLOR_BLUEVIOLET);
            oledC_DrawLine(55, 75, 55, 20, 3, OLEDC_COLOR_BLUEVIOLET);
        }
        else if(field == 2){//seconds
            oledC_DrawLine(65, 20, 85, 20, 3, OLEDC_COLOR_BLUEVIOLET);
            oledC_DrawLine(65, 20, 65, 75, 3, OLEDC_COLOR_BLUEVIOLET);
            oledC_DrawLine(65, 75, 85, 75, 3, OLEDC_COLOR_BLUEVIOLET);
            oledC_DrawLine(85, 75, 85, 20, 3, OLEDC_COLOR_BLUEVIOLET);
        }    
    }  
}

/* set date according to user input */
void set_date(){
    clock_position.display_big = false;
    digital_clock_display();
    oledC_DrawString(10, 25, 1, 6, date.str_day, OLEDC_COLOR_WHITE);
    oledC_DrawString(22, 25, 1, 6, " / ", OLEDC_COLOR_WHITE);
    oledC_DrawString(40, 25, 1, 6, date.str_month, OLEDC_COLOR_WHITE);

    int indx = 1;
    rectangles(indx, 1);
    // temp variables to drew numbers
    char month_field[20];
    char day_field[20];
    strcpy(month_field, date.str_month);
    strcpy(day_field, date.str_day);
    
    while(date.date_setting){
        if(PORTAbits.RA12 == 0){
            button2Press();
            if(dig_clock.long_press){  // long press on s2 (for 3 seconds ) - return to main menu
                /* back to menu */
                date.date_setting = false;     
                oledC_clearScreen();
                oledC_clearScreen();
                digital_clock_display();
                menu_display(in_menu);
                navigate(); 
            }
            else if(dig_clock.long_press == false){ // short press on s2 - scroll throw date fields
                rectangles(indx, 0);
                indx--;
                if(indx < 0){indx = 1;}
                rectangles(indx, 1);
            }
        }
        
        if(PORTAbits.RA11 == 0){            // press on s1 to change numbers
            PORTAbits.RA8 = 1;
            DELAY_milliseconds(200);
            if(indx == 1){// month
                oledC_DrawString(40, 25, 1, 6, month_field, OLEDC_COLOR_BLACK);
                date.month++;
                if(date.month > 12){date.month = 1;}
                sprintf(month_field, "%d", date.month);
                if(date.month < 10){strcpy(date.zero, "0"); strcat(date.zero, month_field); strcpy(month_field, date.zero);}
                oledC_DrawString(40, 25, 1, 6, month_field, OLEDC_COLOR_WHITE);
            }
            else if(indx == 0){// days
                oledC_DrawString(10, 25, 1, 6, day_field, OLEDC_COLOR_BLACK);
                date.day ++;
                if(date.day > months[date.month]){date.day = 1;}
                sprintf(day_field, "%d", date.day);
                if(date.day < 10){strcpy(date.zero, "0"); strcat(date.zero, day_field); strcpy(day_field, date.zero);}
                oledC_DrawString(10, 25, 1, 6, day_field, OLEDC_COLOR_WHITE);
            }
            PORTAbits.RA8 = 0;
        }
    }
}

/* digital clock user setting - time setting and alarm setting */
void digital_clock_set(){
    in_setting_clock = 1;
    clock_position.display_big = true;
    digital_clock_display();
    
    int updated_fields[3] = {0};
    // updated fields according to user //
    char hour_field[20];
    char minute_field[20];
    char second_field[20];
    int am_pm;
    
    updated_fields[2] = cor_indx;
    strcpy(second_field, dig_clock.s);
    updated_fields[1] = minutes_indx;
    strcpy(minute_field, dig_clock.m);
    indx_hour_setting = updated_fields[0] = hours_indx;
    strcpy(hour_field, dig_clock.h);
    
    am_pm = clock_display.am_pm; // copy current am-pm
        
    // while in setting digital clock mode
    dig_clock.setting = true;
    
    int indx;//field index
    if(alarm.setting_alarm == false){indx = 2;}//time setting
    else if(alarm.setting_alarm == true){indx = 1;}//alarm setting
    
    rectangles(indx, 1);
    
    // update hours according to digital display
    convert_analog_digital(true);
    updated_fields[0] = updated_hour_indx;
    
    while(dig_clock.setting){
        if(PORTAbits.RA12 == 0){
            button2Press();
            if(dig_clock.long_press){  // long press on s2 (for 3 seconds ) - return to main menu
                if(alarm.setting_alarm == false){ // if time setting
                    // update clock according to new time // - only when setting time
                    cor_indx = dig_clock.seconds = updated_fields[2];   
                    minutes_indx = dig_clock.minutes = updated_fields[1];
                    hours_indx = dig_clock.hours = indx_hour_setting;  
                    clock_display.am_pm = am_pm;
                }
                else if(alarm.setting_alarm == true){ //if alarm setting
                    alarm.minute = updated_fields[1];
                    alarm.hour = indx_hour_setting;
                    alarm.setting_alarm = false;
                    alarm.ring = false;
                    alarm.on = true;
                }
                
                //return to menu
                oledC_clearScreen();
                oledC_clearScreen();
                clock_position.display_big = false;
                clock_position.display_small = true;
                dig_clock.setting = false;
                in_setting_clock = 0;
                digital_clock_display();
                menu_display(in_menu);
                navigate(); // back to menu
            }
            else if(dig_clock.long_press == false){   // short press on s2  - scroll throw clock fields
                if(alarm.setting_alarm == false){      // setting time
                    rectangles(indx, 0);
                    indx--;
                    if(indx < 0){indx = 2;}
                    rectangles(indx, 1);
                }
                else if(alarm.setting_alarm == true){  // setting alarm
                    rectangles(indx, 0);
                    indx--;
                    if(indx < 0){indx = 1;}
                    rectangles(indx, 1);
                }
            }
        }
        
        if(PORTAbits.RA11 == 0){            // press on s1 to change numbers
            PORTAbits.RA8 = 1;
            DELAY_milliseconds(200);
            if(indx == 2){                           // if seconds
                updated_fields[indx]++;
                if(updated_fields[indx] > 59){updated_fields[indx] = 0;}
                oledC_DrawString(70, 25, 1, 6, second_field, OLEDC_COLOR_BLACK);
                sprintf(second_field, "%d", updated_fields[indx]);
                if(updated_fields[indx] < 10){strcpy(dig_clock.zero, "0"); strcat(dig_clock.zero, second_field); strcpy(second_field, dig_clock.zero);}
                oledC_DrawString(70, 25, 1, 6, second_field, OLEDC_COLOR_WHITE);
            }
            if(indx == 1){                           // if minutes
                updated_fields[indx]++;
                if(updated_fields[indx] > 59){updated_fields[indx] = 0;}
                oledC_DrawString(40, 25, 1, 6, minute_field, OLEDC_COLOR_BLACK);
                sprintf(minute_field, "%d", updated_fields[indx]);
                if(updated_fields[indx] < 10){strcpy(dig_clock.zero, "0"); strcat(dig_clock.zero, minute_field); strcpy(minute_field, dig_clock.zero);}
                oledC_DrawString(40, 25, 1, 6, minute_field, OLEDC_COLOR_WHITE);
            }
            if(indx == 0){                           // if hours
                updated_fields[indx]++;
                
                // in case 12 interval
                if(clock_display.Interval12){
                    if(updated_fields[indx] > 12){updated_fields[indx] = 1;}
                    if(updated_fields[indx] == 1){
                        oledC_DrawPoint(1, 1, OLEDC_COLOR_BLACK);
                        am_pm_display(am_pm, 0);
                        if(am_pm == 0){am_pm = 1;}
                        else if(am_pm == 1){am_pm = 0;}
                        am_pm_display(am_pm, 1);
                    }
                }
                
                // in case 24 interval
                if(clock_display.Interval24){
                    if(updated_fields[indx] > 24){updated_fields[indx] = 1;}
                    if(updated_fields[indx] >= 13 && updated_fields[indx] <= 24){am_pm = 1;}//pm
                    else if(updated_fields[indx] >= 1 && updated_fields[indx] <= 12){am_pm = 0;}//am
                }
                updated_hour_indx = updated_fields[indx];
                
                // delete previous hour display
                oledC_DrawString(10, 25, 1, 6, hour_field, OLEDC_COLOR_BLACK);
                sprintf(hour_field, "%d", updated_fields[indx]);
                if(updated_fields[indx] < 10){strcpy(dig_clock.zero, "0"); strcat(dig_clock.zero, hour_field); strcpy(hour_field, dig_clock.zero);}
                oledC_DrawString(10, 25, 1, 6, hour_field, OLEDC_COLOR_WHITE);
                convert_digital_analog(updated_fields[1]);
            }
            PORTAbits.RA8 = 0;
        }
    }
}

/* system menu display */
void menu_display(int menu_level){
   
    int choose_point[] = {23, 38, 53, 68, 83};
    int prev_choice = menu_choice[in_menu] -1;
    if(in_menu == 0 && prev_choice < 0){ prev_choice = 4;}
    if((in_menu == 1 || in_menu == 2) && prev_choice < 0){ prev_choice = 2;}
    
    if(menu_level == 0){ //main menu
        oledC_DrawThickPoint(5, choose_point[prev_choice], 4, OLEDC_COLOR_BLACK);
        oledC_DrawThickPoint(5, choose_point[menu_choice[in_menu]], 4, OLEDC_COLOR_BLUEVIOLET);
        oledC_DrawString(15, 20, 1, 1, "Display Mode", OLEDC_COLOR_WHITE);
        oledC_DrawString(15, 35, 1,1, "Set Time", OLEDC_COLOR_WHITE);
        oledC_DrawString(15, 50, 1,1, "Set Date", OLEDC_COLOR_WHITE);
        oledC_DrawString(15, 65, 1,1, "Set Alarm", OLEDC_COLOR_WHITE);
        oledC_DrawString(15, 80, 1,1, "Alarm Off", OLEDC_COLOR_WHITE);
    }
    
    else if(menu_level == 1){ //mode menu
        oledC_DrawThickPoint(5, choose_point[prev_choice], 4, OLEDC_COLOR_BLACK);
        oledC_DrawThickPoint(5, choose_point[menu_choice[in_menu]], 4, OLEDC_COLOR_BLUEVIOLET);
        oledC_DrawString(15, 20, 1, 1, "Analog", OLEDC_COLOR_WHITE);
        oledC_DrawString(15, 35, 1,1, "Digital", OLEDC_COLOR_WHITE);
        oledC_DrawString(15, 50, 1,1, "Back", OLEDC_COLOR_WHITE);
    }
    
    else if(menu_level == 2){ //Interval menu
        oledC_DrawThickPoint(5, choose_point[prev_choice], 4, OLEDC_COLOR_BLACK);
        oledC_DrawThickPoint(5, choose_point[menu_choice[in_menu]], 4, OLEDC_COLOR_BLUEVIOLET);
        oledC_DrawString(15, 20, 1, 1, "24H Interval", OLEDC_COLOR_WHITE);
        oledC_DrawString(15, 35, 1,1, "12H Interval", OLEDC_COLOR_WHITE);
        oledC_DrawString(15, 50, 1,1, "Back", OLEDC_COLOR_WHITE);
    }
}

/* handle all menu features */
void navigate(){
    //handle s1 press
    if(PORTAbits.RA11 == 0){
        button1Press();
        // main menu
        if(in_menu_flag){                 // press on s1 (for 3 seconds ) - enter menu
            clock_display.analog = false;
            date.display_date = false;   // do not display date
            dig_clock.setting = false;
            clock_position.display_big = false;
            clock_position.display_small = true;
            oledC_clearScreen();
            oledC_clearScreen();
            digital_clock_display();
            in_menu = 0;                 // indicates main menu
            menu_display(in_menu);
        }
        else if(in_menu == 0 && in_menu_flag == false){   // press on s1 in main menu - scroll in main menu
            menu_choice[in_menu] ++;
            if(menu_choice[in_menu] > 4){menu_choice[in_menu] = 0;}
            menu_display(in_menu);                  // update point in main menu
        }
        // mode menu
        else if(in_menu == 1 && in_menu_flag == false){   // press on s1 in mode menu - scroll in mode menu
            menu_choice[in_menu] ++;
            if(menu_choice[in_menu] > 2){menu_choice[in_menu] = 0;}
            menu_display(in_menu);                  // update point in mode menu
        }
        // Interval menu
        else if(in_menu == 2 && in_menu_flag == false){   // press on s1 in Interval menu - scroll in Interval menu
            menu_choice[in_menu] ++;
            if(menu_choice[in_menu] > 2){menu_choice[in_menu] = 0;}
            menu_display(in_menu);                  // update point in Interval menu
        }
    }
    //handle s2 press
    //if in main menu
    else if(in_menu == 0){    
        if(PORTAbits.RA12 == 0 && menu_choice[in_menu] == 0){       //press on s2 and Display mode choice - enter mode menu
            PORTAbits.RA9 = 1;
            oledC_clearScreen();
            oledC_clearScreen();
            digital_clock_display();
            in_menu = 1;
            menu_display(in_menu);
            PORTAbits.RA9 = 0;
        }
        else if(PORTAbits.RA12 == 0 && menu_choice[in_menu] == 1){       //press on s2 and set time choice 
            PORTAbits.RA9 = 1;
            oledC_clearScreen();
            oledC_clearScreen();
            PORTAbits.RA9 = 0;
            digital_clock_set();
        }
        else if(PORTAbits.RA12 == 0 && menu_choice[in_menu] == 2){       //press on s2 and set date choice 
            PORTAbits.RA9 = 1;
            oledC_clearScreen();
            oledC_clearScreen();
            PORTAbits.RA9 = 0;
            date.date_setting = true;
            set_date();
        }
        else if(PORTAbits.RA12 == 0 && menu_choice[in_menu] == 3){       //press on s2 and set alarm choice 
            PORTAbits.RA9 = 1;
            oledC_clearScreen();
            oledC_clearScreen();
            PORTAbits.RA9 = 0;
            alarm.setting_alarm = true; // setting alarm mode
            digital_clock_set();
        }
        else if(PORTAbits.RA12 == 0 && menu_choice[in_menu] == 4){       //press on s2 and alarm off choice 
            PORTAbits.RA9 = 1;
            alarm.on = false;
            alarm.ring = false;
            alarm.count_time = 0;   // reset count for alarm
            oledC_DrawRing(8, 8, 5, 1, OLEDC_COLOR_BLACK);
            PORTAbits.RA9 = 0;
        }
    }
    
    //if in mode menu
    else if(in_menu == 1){    
        if(PORTAbits.RA12 == 0 && menu_choice[in_menu] == 1){       //press on s2 and digital choice - enter Interval menu
            PORTAbits.RA9 = 1;
            oledC_clearScreen();
            oledC_clearScreen();
            digital_clock_display();
            in_menu = 2;
            menu_display(in_menu);
            PORTAbits.RA9 = 0;
        }
        else if(PORTAbits.RA12 == 0 && menu_choice[in_menu] == 0){       //press on s2 and Analog - display analog clock
            PORTAbits.RA9 = 1;
            oledC_clearScreen();
            clock_position.display_small = false;
            date.display_date = true;   // display date
            oledC_clearScreen();
            in_menu = 0;
            clock_display.analog = true;    // set analog mode to true
            PORTAbits.RA9 = 0;
            display_curr_date();
            analog_clock_display();
        }
        else if(PORTAbits.RA12 == 0 && menu_choice[in_menu] == 2){       //press on s2 and back - back to main menu
            PORTAbits.RA9 = 1;
            oledC_clearScreen();
            oledC_clearScreen();
            digital_clock_display();
            in_menu = 0;
            menu_display(in_menu);
            PORTAbits.RA9 = 0;
        }
    }
    //if in Interval menu
    else if(in_menu == 2){
       if(PORTAbits.RA12 == 0 && menu_choice[in_menu] == 2){       //press on s2 and back - back to mode menu
            PORTAbits.RA9 = 1;
            oledC_clearScreen();
            oledC_clearScreen();
            digital_clock_display();
            in_menu = 1;
            menu_display(in_menu);
            PORTAbits.RA9 = 0;
        }
       else if(PORTAbits.RA12 == 0 && menu_choice[in_menu] == 0){       //press on s2 and 24 interval
            PORTAbits.RA9 = 1;
            oledC_clearScreen();
            clock_position.display_small = false;
            oledC_clearScreen();
            clock_display.digital = true;
            date.display_date = true;   // display date
            clock_position.display_big = true;
            clock_display.Interval24 = true;
            clock_display.Interval12 = false;
            PORTAbits.RA9 = 0;
            display_curr_date();
            digital_clock_display();
        }
       else if(PORTAbits.RA12 == 0 && menu_choice[in_menu] == 1){       //press on s2 and 12 interval
            PORTAbits.RA9 = 1;
            oledC_clearScreen();
            clock_position.display_small = false;
            oledC_clearScreen();
            clock_display.digital = true;
            date.display_date = true;   // display date
            clock_position.display_big = true;
            clock_display.Interval24 = false;
            clock_display.Interval12 = true;
            PORTAbits.RA9 = 0;
            display_curr_date();
            digital_clock_display();
        }
    }
    
    PORTAbits.RA9 = 0;  //LED2 off
    PORTAbits.RA8 = 0;  //LED1 off
}

/* Main application */
int main(void)
{
    /* initialization */
    SYSTEM_Initialize();
    User_Initialize();
    /* enable ISR for timer interrupt - handles clock time */
    initialize_timer_ISR();
    digital_clock_display();
    display_curr_date();
    
    while(1){   
        navigate();

    }
    return 1;
}
