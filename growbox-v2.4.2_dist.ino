/*
Growbox control by Giuseppe Cacciato
giuseppe.cacciato@live.it
This code use a DHT sensor for read temperature and humidity to turning on fan and vacuum for controlling it.
This code also connect with a telegram bot and it can be controlled by human 
Autocontrol can be desabled and enabled by the owner via Telegram
Enjoy


Hardware:
Wemos d1 r2 Wifi ESP8266 (Add http://arduino.esp8266.com/stable/package_esp8266com_index.json for 3d board)
Check for example here https://www.instructables.com/Programming-the-WeMos-Using-Arduino-SoftwareIDE/
Add UniversalTelegramBot to your library for using this sketch 
https://github.com/witnessmenow/Universal-Arduino-Telegram-Bot
Add DHT sensor library by Adafruit for DHT hardware
https://github.com/adafruit/DHT-sensor-library
Using the EPROM memory, check reference
https://github.com/esp8266/Arduino/blob/master/libraries/EEPROM/EEPROM.h
Library for using the WIFI reference
https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266WiFi/src/ESP8266WiFi.h
Wifi documentation on github.io and https://github.com/witnessmenow/Universal-Arduino-Telegram-Bot/blob/master/examples/ESP8266/EchoBot/EchoBot.ino
here is an example for wifi connection and basilar Telegram Bot usage.




Changelog
v1.0 Dht11 + rele (DHT22 + 4 channel rele)
v2.0 Adding support for wifi and telegram
v2.1 Adding support for manual control via telegram disabling auto control (growAUTO variable)
v2.2 Adding support of inside FAN (turnON and turnOFF)
v2.3 Adding support for manual irrigation (water)
v2.3.1 Set italian language and owner security
v2.4 Adding soil sensor 
v2.4.1 Adding emoji
v2.4.2 Adding eeprom writing and reading and updating variable via telegram (function setvar)
*/



#include <Adafruit_Sensor.h>
#include <ESP8266WiFi.h>
#include "DHT.h"
#include <EEPROM.h>


#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

#define DHTPIN D14      // Digital pin connected to the DHT sensor    
#define FAN 14         // Digital pin of FAN
#define VACUUM 13      // Digital pin for vacuum relè
#define INSIDEFAN 12      // Digital pin for local FAN
#define IRRIGATION 5    // Digital pin for irrigation
#define SOILPIN A0    // Digital pin for soil reader

//EPROM ADDRESS
#define ADDRESSTEMPMIN 0
#define ADDRESSTEMPMAX 10
#define ADDRESSHUMIDITYMIN 20
#define ADDRESSHUMIDITYMAX 30
#define ADDRESSDANGER 40

#define DELAYIRRIGATION 9000    // Delay of pump irrigation

//debug
#define debug false

//dht
//#define DHTTYPE DHT11   // DHT 11
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
//#define DHTTYPE DHT21   // DHT 21 (AM2301)

//eprom
#define ewrite false //disable after first setup
#define EEPROM_SIZE 256


// Set WiFi credentials
#define WIFI_SSID "Your_SSID"
#define WIFI_PASS "Your_SSID_PASS"

// Telegram BOT Token (Get from Botfather)
#define BOT_TOKEN "Your_Personal_Bot_Token"

const unsigned long BOT_MTBS = 1000; // mean time between scan messages

X509List cert(TELEGRAM_CERTIFICATE_ROOT);
WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
unsigned long bot_lasttime; // last time messages' scan has been done

String owner = "Your_Telegram_Name";
String master = "Another_Person"; // You can use the same as master
String owner_id = "Your_ID_Number"; // Use debug=true and send message to bot to check your ID Number
DHT dht(DHTPIN, DHTTYPE);
short tempMAX;
short tempMIN;
short humidityMAX;
short humidityMIN;
short temperatureDANGER;
float temperature = 0;
float humidity = 0;
float soil = 0;
float heatindex = 0;
short loopvar = 0;
boolean statusVACUUM = false;
boolean statusFAN = false;
boolean insideFAN = false;
boolean growAUTO = true;
boolean danger = false;

void(* resetFunc) (void) = 0; //declare reset function @ address 0

void setup() {
  Serial.begin(9600);   //Atmel
  //Serial.begin(115200); //WEMOS
  
  //Init EEPROM
  EEPROM.begin(EEPROM_SIZE);
  
  dht.begin();
  
  if (debug) {
    // attempt to connect to Wifi network:
    Serial.print("Connecting to Wifi SSID ");
    Serial.print(WIFI_SSID);
  }
  
    // Begin WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  secured_client.setTrustAnchors(&cert); // Add root certificate for api.telegram.org
  
  // Loop continuously while WiFi is not connected
  for (int x=0; WiFi.status() != WL_CONNECTED; x++)
  {
    delay(1000);
    if (debug) Serial.print(".");
    if (x>50)   resetFunc();  //call reset
  }
  
  // Connected to WiFi
  if (debug) {
    Serial.println();
    Serial.print("Connected! IP address: ");
    Serial.println(WiFi.localIP());
  }
  
  // Setting PIN 
  pinMode (VACUUM, OUTPUT); // VACUUM
  digitalWrite (VACUUM, LOW); // Default value OFF
  pinMode (FAN, OUTPUT); // FAN
  digitalWrite (FAN, LOW); // Default value OFF
  pinMode (INSIDEFAN, OUTPUT); // INSIDEFAN
  digitalWrite (INSIDEFAN, LOW); // Default value OFF
  pinMode (IRRIGATION, OUTPUT); // IRRIGATION
  digitalWrite (IRRIGATION, LOW); // Default value OFF
  // Setting Time
  if (debug) Serial.print("Retrieving time: ");
  
  configTime(0, 0, "pool.ntp.org"); // get UTC time via NTP
  time_t now = time(nullptr);
  int timecounter = 0;
  while (now < 24 * 3600)
  {
    timecounter++;
    if (debug) Serial.print(".");
    delay(100);
    now = time(nullptr);
    if (timecounter > 20) resetFunc();
  }
  delay(100);
  if (debug) Serial.println(now);
  
  if(debug) {
    Serial.print("Time : ");
    Serial.println(now);
    startupdebug();
  }
  if (ewrite) {
    tempMAX = 25;
    tempMIN = 5;
    humidityMAX = 70;
    humidityMIN = 40;
    temperatureDANGER= 35;
    writedefault(tempMAX, tempMIN, humidityMAX, humidityMIN, temperatureDANGER);   
    bot.sendMessage(owner_id, "*Attenzione!* Vengono lette le variabili di default, disabilitare ewrite!!!!\n","");
  }
  else readdefault();
  if (debug)
    bot.sendMessage(owner_id, "*Growbox* Avviata\n","");  
  EEPROM.end();  
}

void loop() {
  
  // DHT22 SENSOR READING
  
  // Wait a few seconds between measurements.
  delay(2000);
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  humidity = dht.readHumidity();
  // Read temperature as Celsius (the default)
  temperature = dht.readTemperature();
  // Check if any reads failed and exit early (to try again).
  
  if (isnan(humidity) || isnan(temperature)) {
    if(debug)
      Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }

  // Compute heat index in Celsius (isFahreheit = false)
  heatindex = dht.computeHeatIndex(temperature, humidity, false);

  // Debug with print infomation
  if(debug) debugDHT(humidity, temperature, heatindex);

  // GROWBOX RELE, AUTOCONTORL FOR TEMPERATURE, CONTORL VACUUM AND FAN
  
  // Working only with autogrow = true
  if (growAUTO) {
    if (temperature > tempMAX)
        turnON(500);
    if (temperature < tempMIN)
        turnOFF(500);
    if (temperature < tempMAX && temperature > tempMIN) {
        if (!insideFAN) {
          digitalWrite (INSIDEFAN, HIGH); 
          insideFAN = !insideFAN;
          if (debug)     Serial.println(F("Turning ON inside FAN"));
    }
    }
    if (humidity < humidityMIN && temperature > tempMIN && loopvar>100) {
        water(100);
        loopvar = 0;
    }
   else loopvar++;
  }
  if (temperature > temperatureDANGER && !danger) dangerfunction();
  if (temperature < (temperatureDANGER -5) && danger) danger=false; 

  // SOIL SENSOR

  
  for (int i = 1; i <= 100; i++) 
  { 
    soil = soil + analogRead(SOILPIN); 
    delay(1); 
  } 
  soil = soil/100.0; 
    
  if (debug) {
    Serial.print("Soil sensor:");
    Serial.println(soil); 
  }


  // Reading message from telegram
  if (millis() - bot_lasttime > BOT_MTBS)
  {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while (numNewMessages)
    {
      Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }

    bot_lasttime = millis();
  }
}

void turnON(int delayed) {
      if (!statusFAN) {
      digitalWrite (FAN, HIGH); 
      statusFAN = !statusFAN;
        if (debug)     Serial.println(F("Turning ON FAN"));
    }
    
    if (!statusVACUUM) {
      digitalWrite (VACUUM, HIGH); 
      statusVACUUM = !statusVACUUM;
        if (debug)     Serial.println(F("Turning ON VACUUM"));
    }
    
    if (!insideFAN) {
      digitalWrite (INSIDEFAN, HIGH); 
      insideFAN = !insideFAN;
      if (debug)     Serial.println(F("Turning ON inside FAN"));
    }
    delay(delayed);
}

void turnOFF(int delayed) {
      if (statusFAN) {
      digitalWrite (FAN, LOW); 
      statusFAN = !statusFAN;
        if (debug)     Serial.println(F("Turning OFF FAN"));
    }
    
    if (statusVACUUM) {
      digitalWrite (VACUUM, LOW); 
      statusVACUUM = !statusVACUUM;
        if (debug)     Serial.println(F("Turning OFF VACUUM"));
    }
    if (insideFAN) {
      digitalWrite (INSIDEFAN, LOW); 
      insideFAN = !insideFAN;
      if (debug)     Serial.println(F("Turning OFF inside FAN"));
    }
    delay(delayed);
}

void water (int delayed) 
{
      digitalWrite (IRRIGATION, HIGH); 
      delay (delayed);
      digitalWrite (IRRIGATION, LOW);
      if (debug) {
            Serial.print("Irrigation complete with ");
            Serial.print(delayed);
            Serial.println(" ms");
      }
}

void dangerfunction () {
   danger = true;
   bot.sendMessage(owner_id, "*Attenzione!* La temperatura della growbox è troppo alta!!!!\n","");
   bot.sendMessage(owner_id, "Questo messaggio non verrà ripetuto e l allert verra resettato non appena la temperatura diminuirà\n","");
   if (debug) Serial.println(F("Danger ACTIVATED!"));

}


void handleNewMessages(int numNewMessages)
{
String chat_id;
String text;
String from_name;
String botStatus;
String welcome;
String setvarmsg;
String able;
  for (int i = 0; i < numNewMessages; i++)
  {
    chat_id = bot.messages[i].chat_id;
    text = bot.messages[i].text;

    from_name = bot.messages[i].from_name;
    if (from_name == "")
      from_name = "Guest";
          if (text == "/start")
    {
      welcome = "\xF0\x9F\x98\x8F Growbox automatica gestita da: " + owner + ".\n";
      welcome += "Lista dei comandi del bot\n\n";
      welcome += "/on : Accendere ventola e aspirazione *Richiede auto=off*\n";
      welcome += "/off : Spegnere ventola e aspirazione *Richiede auto=off*\n";
      welcome += "/status : Condizioni della Growbox\n";
      welcome += "/autoon : Abilita il controllo automatico growbox\n";
      welcome += "/autooff : Disabilita il controllo automatico growbox\n";
      welcome += "/water : Abilita umidificatore temporizzato\n";
      welcome += "/setup : Menu di configurazione e test\n";
      bot.sendMessage(chat_id, welcome, "Markdown");
    }
    if (from_name == owner || from_name == master) {
        if (text == "/autoon") {
            growAUTO = true;
            bot.sendMessage(chat_id, "\xF0\x9F\x92\xBB La growbox e' adesso impostata in modalita automatica", "");
        }
        if (text == "/autooff") {
            growAUTO = false;
            bot.sendMessage(chat_id, "\xF0\x9F\x91\xB7 La growbox e' adesso impostata in modalita manuale", "");
        }
        if (text == "/water") {
            bot.sendMessage(chat_id, "\xF0\x9F\x92\xA6 Abilito la pompa d irrigazione temporizzata, attendi... ", "");
            water(DELAYIRRIGATION);
            bot.sendMessage(chat_id, "\xF0\x9F\x91\x8D Fatto!", "");
        }
        if (text == "/on") {
            if (!growAUTO) {
                turnON(2000);
                bot.sendMessage(chat_id, "\xF0\x9F\x92\xA1 Accendo le ventole interne e l aspirazione", "");
            }
            else {
                bot.sendMessage(chat_id, "\xE2\x9A\xA0 Disabilitare il controllo automatico prima di comandare manualmente", "");
            }
        }

        if (text == "/off") {
            if (!growAUTO) {
                turnOFF(2000);
                bot.sendMessage(chat_id, "\xF0\x9F\x93\xB4 Spengo le ventole interne e l aspirazione", "");
            }
            else {
                bot.sendMessage(chat_id, "\xE2\x9A\xA0 Disabilitare il controllo automatico prima di comandare manualmente", "");
            }
        }
      if (text == "/tempmax")
          bot.sendMessage(chat_id, "Temperatura massima registrata: " + (String)heatindex +"* C \n", "");
      if (text == "/var") {
          bot.sendMessage(chat_id, "tempMIN: " + (String)tempMIN +"* C \n", "");
          bot.sendMessage(chat_id, "tempMAX: " + (String)tempMAX +"* C \n", "");
          bot.sendMessage(chat_id, "humidityMIN: " + (String)humidityMIN +"\n", "");
          bot.sendMessage(chat_id, "humidityMAX: " + (String)humidityMAX +"\n", "");
          bot.sendMessage(chat_id, "DangerTemperature: " + (String)temperatureDANGER +"\n", "");
          bot.sendMessage(chat_id, "Delay Irrigation: " + (String)DELAYIRRIGATION +"\n", "");
      }
      if (text == "/test") {
        bot.sendMessage(chat_id, "Eseguo un po di test preimpostati... \n", "");
        dangerfunction();
        turnON(100);
        turnOFF(100);
        water(100);
      }

      if (text.startsWith("/tmpMIN")) {
        byte variable = setvar(text);
        if (variable == 0)
          bot.sendMessage(chat_id, "Specificare un numero compreso fra 1 e 99","");
        else {
          writedefault(variable, 0, 0, 0, 0);
          bot.sendMessage(chat_id, "Temperatura minima settata su: " + (String)variable + "");
          readdefault();
        }         
      }
      if (text.startsWith("/tmpMAX")) {
        byte variable = setvar(text);
        if (variable == 0)
          bot.sendMessage(chat_id, "Specificare un numero compreso fra 1 e 99","");
        else { 
          writedefault(0, variable, 0, 0, 0);
          bot.sendMessage(chat_id, "Temperatura massima settata su: " + (String)variable + "");
          readdefault();
        }        
      }
      if (text.startsWith("/humMIN")) {
        byte variable = setvar(text);
        if (variable == 0)
          bot.sendMessage(chat_id, "Specificare un numero compreso fra 1 e 99","");
        else {
          writedefault(0, 0, variable, 0, 0);
          bot.sendMessage(chat_id, "Umidita minima settata su: " + (String)variable + "");
          readdefault();          
        }
      }
      if (text.startsWith("/humMAX")) {
        byte variable = setvar(text);
        if (variable == 0)
          bot.sendMessage(chat_id, "Specificare un numero compreso fra 1 e 99","");
        else {
          writedefault(0, 0, 0, variable, 0);
          bot.sendMessage(chat_id, "Umidita massima settata su: " + (String)variable + "");
          readdefault();
        }
      }
      if (text.startsWith("/danger")) {
        byte variable = setvar(text);
        if (variable == 0)
          bot.sendMessage(chat_id, "Specificare un numero compreso fra 1 e 99","");
        else { 
          writedefault(0, 0, 0, 0, variable);
          bot.sendMessage(chat_id, "Temperatura DANGER settata su: " + (String)variable + "");
          readdefault();
        }
      }
      if (text=="/setup") {
        setvarmsg = "Menu di impostazione, permette di modificare le variabili di temperatura e umidita \n";
        setvarmsg += "/tmpMIN *num*, Permette di modificare la temperatura minima \n";
        setvarmsg += "/tmpMAX *num*, Permette di modificare la temperatura massima \n";
        setvarmsg += "/humMIN *num*, Permette di modificare l umidita minima \n";
        setvarmsg += "/humMAX *num*, Permette di modificare l umidita massima \n";
        setvarmsg += "/danger *num*, Permette di modificare la temperatura d allarme \n";
        setvarmsg += "*num* dev essere un numero intero compreso fra 1 e 99\n";
        setvarmsg += "\n/var Mostra le variabili impostate\n";
        setvarmsg += "/test Attiva e disattiva il rele ed il messaggio di warning temperatura elevata";
        setvarmsg += "/tempmax Mostra la temperatura massima rilevata";
        bot.sendMessage(chat_id, setvarmsg, "Markdown");
      }
    }
    else {
        bot.sendMessage(chat_id, "\xF0\x9F\x9A\xAB Solo il proprietario puo eseguire questi comandi", "");
    }
    if (text == "/status")
    {
      if (growAUTO) able= "Abilitato";
      if (!growAUTO) able="Disabilitato";
      botStatus = "Benvenuto " + (String)from_name + " \xF0\x9F\x98\x8F .\n";
      botStatus += "\U00002622 Stato della Growbox: \n\n";
      botStatus += "\xE2\x9B\x85 Temperatura interna: " + (String)temperature + ".\n";
      botStatus += "\xF0\x9F\x92\xA6 Umidita interna: " + (String)humidity + ".\n";
      botStatus += "\U00002614 Umidita' del suolo: " + (String)soil + ".\n";
      botStatus += "\xF0\x9F\x92\xBB Controllo automatico = " + (String)able + ".\n";
      bot.sendMessage(chat_id, botStatus, "Markdown");
      if (statusFAN)
      {
        bot.sendMessage(chat_id, "\xE2\x9C\x85 Ventola accesa", "");
      }
      else
      {
        bot.sendMessage(chat_id, "\U0000274c Ventola spenta", "");
      }
        if (statusVACUUM)
      {
        bot.sendMessage(chat_id, "\xE2\x9C\x85 Aspirazione accesa", "");
      }
      else
      {
        bot.sendMessage(chat_id, "\U0000274c Aspirazione spenta", "");
      }     
      if (insideFAN)
      {
        bot.sendMessage(chat_id, "\xE2\x9C\x85 Ventola interna accesa", "");
      }
      else
      {
        bot.sendMessage(chat_id, "\U0000274c Ventola esterna spenta", "");
      }   
    }
  }
      if(debug) {
        Serial.println();
        Serial.println(F("Received new message"));
        Serial.print("Message : ");
        Serial.println(text);
        Serial.print("From : ");
        Serial.println(from_name);
        Serial.print("Chat id: ");
        Serial.println(chat_id);
        Serial.print("Status : ");
        Serial.println((String)botStatus);
        Serial.print("handleNewMessages ");
        Serial.println(numNewMessages);
      }
}

byte getnumber (String text) {
  if (text.length() == 9) {
    char a = text.charAt(8);
    String b = "";
    b += (char)a; 
    return b.toInt();
  }
  if (text.length() == 10) {

    char a = text.charAt(8);
    String b = "";
    b += (char)a; 
    char c = text.charAt(9);
    String d = "";
    d += (char)c;
    if (b.toInt() != 0) return ((b.toInt()*10)+d.toInt());
    else return d.toInt();

    return 0; 
  }
}

byte setvar (String text) {
  short variable = 0;
  if (text.length() == 9 || text.length()==10)
    variable = getnumber(text);
        
  if (text.length() < 9 || text.length() >10) return 0; 
  if (debug) {
    Serial.print("New Variable are: ");
    Serial.println(variable);
  }
  return variable;
}

void writedefault (short a, short b, short c, short d, short e) {
  EEPROM.begin(EEPROM_SIZE);
  if (a != 0)  EEPROM.put(ADDRESSTEMPMIN, a); //tempMIN
  EEPROM.commit();
  delay(4);
  if (b != 0)  EEPROM.put(ADDRESSTEMPMAX, b); //tempMAX
  EEPROM.commit();
  delay(4);
  if (c != 0)  EEPROM.put(ADDRESSHUMIDITYMIN, c); //humidityMIN
  EEPROM.commit();
  delay(4);
  if (d != 0)  EEPROM.put(ADDRESSHUMIDITYMAX, d); //humidityMAX
  EEPROM.commit();
  delay(4);
  if (e != 0)  EEPROM.put(ADDRESSDANGER, e); //tempDANGER
  EEPROM.commit();
  delay(4);
  if (debug) {
    Serial.println("Writing over eprom");
    Serial.print("tempMin: ");
    Serial.print(a);
    Serial.print(" Over Address:");
    Serial.println(ADDRESSTEMPMIN);
    Serial.println("Writing over eprom");
    Serial.print("tempMax: ");
    Serial.print(b);
    Serial.print(" Over Address:");
    Serial.println(ADDRESSTEMPMAX);
    Serial.println("Writing over eprom");
    Serial.print("humiditymin: ");
    Serial.print(c);
    Serial.print(" Over Address:");
    Serial.println(ADDRESSHUMIDITYMIN);
    Serial.println("Writing over eprom");
    Serial.print("humiditymax: ");
    Serial.print(d);
    Serial.print(" Over Address:");
    Serial.println(ADDRESSHUMIDITYMAX);
    Serial.println("Writing over eprom");
    Serial.print("danger: ");
    Serial.print(e);
    Serial.print(" Over Address:");
    Serial.println(ADDRESSDANGER);
  }
  EEPROM.end();
}

void readdefault() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(ADDRESSTEMPMIN, tempMIN);
  EEPROM.get(ADDRESSTEMPMAX, tempMAX);
  EEPROM.get(ADDRESSHUMIDITYMIN, humidityMIN);
  EEPROM.get(ADDRESSHUMIDITYMAX, humidityMAX);
  EEPROM.get(ADDRESSDANGER, temperatureDANGER);
  if (debug) {
    Serial.println("Reading variable: ");
    Serial.println(tempMIN);
    Serial.println(tempMAX);
    Serial.println(humidityMIN);
    Serial.println(humidityMAX);
    Serial.println(temperatureDANGER);
  }
  EEPROM.end();
}

void updatedefault(byte address, short value) {
  if (value != 0) 
     EEPROM.put(address, value);
  delay(5);
}


/* DEBUG FUNCTION */

void startupdebug() {
    Serial.println();
    Serial.println(F("Starting Wemos Arduino with Wifi with debug"));
    // Wireless
    Serial.print("Wifi, SSID : ");
    Serial.println(WIFI_SSID);
    Serial.print("Wifi, Password : ");
    Serial.println(WIFI_PASS);
    Serial.print("Ip Address : ");
    Serial.println(WiFi.localIP());
    // Var
    Serial.println();
    Serial.print("Dht var, start value = 0 ");
    Serial.println(temperature);
    Serial.println(humidity);
    Serial.print("soil meter var, start value = 0 ");
    Serial.println(soil);
    // Telegram
    Serial.println();
    Serial.println("Telegram Library : ");
    Serial.print("Bot Token : ");
    Serial.println(BOT_TOKEN);
    Serial.print("Owner for telegram bot : ");
    Serial.println(owner);
    // Rele status
    Serial.println("Begin Status for rele: ");
    Serial.print("FAN Status : ");
    Serial.println(statusFAN);
    Serial.print("Vacuum Status : ");
    Serial.println(statusVACUUM);
    Serial.print("Inside FAN Status : ");
    Serial.println(insideFAN);
    Serial.println("Offset of temperature and humidity ");
    Serial.print("Maximum Temperature ");
    Serial.println(tempMAX);
    Serial.print("Maximum Humidity ");
    Serial.println(humidityMAX);
    Serial.print("Minimum Temperature ");
    Serial.println(tempMIN);
    Serial.print("Minimum Temperature ");
    Serial.println(humidityMIN);
    // Boolean Variable
    Serial.print("Automatic control via dht ");
    Serial.println(growAUTO);
    Serial.print("Debug ");
    Serial.println(debug);
    // Defined Variable
    Serial.println("Defined Variable and digital PIN");
    Serial.print("FAN: ");
    Serial.println(FAN);
    Serial.print("VACUUM: ");
    Serial.println(VACUUM);
    Serial.print("INSIDEFAN: ");
    Serial.println(INSIDEFAN);
    Serial.print("IRRIGATION: ");
    Serial.println(IRRIGATION);
    Serial.print("DHT PIN: ");
    Serial.println(DHTPIN);
    Serial.print("SOILPIN: ");
    Serial.println(SOILPIN);
    // EPROM
    if (ewrite) Serial.println("Default writing is true");
    else Serial.println("Default writing is false");
}

void debugDHT (float dH, float dT, float dHIC) {
  
  Serial.print(F("Humidity: "));
  Serial.print(dH);
  Serial.print(F("%  Temperature: "));
  Serial.print(dT);
  Serial.print(F("°C "));
  Serial.print(F(" - Maximum temperature: "));
  Serial.print(dHIC);
  Serial.print(F("°C "));
  Serial.println("");
  
}
      
