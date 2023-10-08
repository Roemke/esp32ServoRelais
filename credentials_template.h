#define mySSID "FamilyWlan"
#define myPASSWORD "geheim"

//eigener ap
#define apSSID "ESP32ServoWlan"
#define apPASSWORD "wiederGeheim"

//der broker 
#define mqttBROKER "192.168.x.x"

//power direkt abfragen nicht über mqtt (ist so einfach schneller)
#define powerHouseGet "http://192.168.x.x/cm?cmnd=status%208"
