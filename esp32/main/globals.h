#ifndef GLOBALS_H
#define GLOBALS_H

#define TAG "ENV"

#define IP_STR_LEN      16
#define HOSTNAME_LEN    32
#define SSID_LEN        33
#define PASSWORD_LEN    65
#define MAC_STR_LEN     18
#define SERVER_HOST     "www.viltstigen.se"
#define SERVER_PATH     "/esp32_bme280"
#define SERVER_PORT     "8095"
#define SERVER_URL      "https://" SERVER_HOST SERVER_PATH
#define SSID_STR        "SSID_HERE"  // Note that idf.py erase-flash needed when changing SSID or password
#define WIFI_PW_STR     "PW_HERE"
#define LOCATION_STR    "Guö"

extern char ip_str[IP_STR_LEN];
extern char hostname[HOSTNAME_LEN];
extern char current_ssid[SSID_LEN];
extern char ssid[SSID_LEN];
extern char password[PASSWORD_LEN];
extern char macstr[MAC_STR_LEN];


#endif