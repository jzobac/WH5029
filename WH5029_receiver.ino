/*
autor    jzobac
licence   CC BY-NC-SA 4.0
copyright Copyright (c) 2017 jzobac
datum   Prosinec 2016

https://creativecommons.org/licenses/by-nc-sa/4.0/deed.cs

En
https://creativecommons.org/licenses/by-nc-sa/4.0/


Tento kód slouží k příjmu a dekódování meteostanice WH5029 na Arduinu a ESP8266.
Kód dávám k dispozici bez jakýchkoliv záruk a zříkám se veškeré odpovědnosti za jakékoliv škody.

Pro použití musíte mít nainstalován rfcontrol.

Zmen promenou const_id na sve ID, jinak se nic neprijme, v prijatem kodu je to bit 25 - 32
*/


#define DEBUG               //chceme debug na serial?



bool meteo_bits_array[88];  //88 bits of information; (192) pulzes
int rain_last = -1;
const byte const_id = xx;   //prepis na svoje ID (napr 180); v prijatem kodu z meteostanice je to bit 25 - 32

#include <RFControl.h>
 
#ifdef DEBUG
  #define debug_init() Serial.begin(115200);
  #define dprintexp(expression) { Serial.print("# "); Serial.print( #expression ); Serial.print( ": " ); Serial.println( expression ); }
  #define dprint(text) Serial.print(text);
  #define dprint2(text, format) Serial.print(text, format);
  #define dprintln(text) Serial.println(text);
  #define dwrite(text) Serial.write(text);
#else
  #define debug_init()
  #define dprintexp(expression)
  #define dprint(text)
  #define dprint2(text, format)
  #define dprintln(text)
  #define dwrite(text)
#endif


void setup() {
  debug_init();
  dprintln();
  dprintln();
  dprintln(F("Boot"));
  RFControl::startReceiving(0);
  //RFControl::startReceiving(D2);  //ESP8266
}

void loop() {
  if(RFControl::hasData()) {
    unsigned int *timings;
    unsigned int timings_size;
    RFControl::getRaw(&timings, &timings_size);
    
    unsigned int buckets[8];
    RFControl::compressTimings(buckets, timings, timings_size);


    
    dprint(timings_size);
    dprint(F(" : "));
 
  
    #ifdef DEBUG
      unsigned int pulse_length_divider = RFControl::getPulseLengthDivider();
      dprint("b: ");
      for(int i=0; i < 8; i++) {
        unsigned long bucket = buckets[i] * pulse_length_divider;
        dprint(bucket);
        dwrite(' ');
      }
      dprint("\nt: ");
    #endif

    if (timings_size == 192)
    {
      digitalWrite(LED_BUILTIN, true);  //swap true and false on ESP8266
      get_data(timings_size, timings);
      digitalWrite(LED_BUILTIN, false); //swap true and false on ESP8266
    }
    #ifdef DEBUG
      else
      {
        for(int i=0; i < timings_size; i++) {
          dwrite('0' + timings[i]);
        } //end for
      }
    #endif

    
    dwrite('\n');
    dwrite('\n');
    
    RFControl::continueReceiving();
  }
}

void get_data(unsigned int timings_size, unsigned int *timings)
{
    unsigned int pozice_pole = 0;
 

//toto je alternativni kus kodu dekodovani pulzu, bohuzel se dle mereni zda, ze je o 100ms pomalejsi
// //88bitu dat, tj 88 * 2 pulzu = 176 - 2 = 174 (pole zacina na nule, odecteme tedy 2 pulzy = 1 bit)
//   for(int i=0; i <= 174; i+=2) {
//      //Serial.print(codes[i]);
//      //Serial.print(' ');
//
//
//        if (timings[i] > timings[i+1] )
//        {
//          meteo_bits_array[pozice_pole]=  1;
//          dprint(meteo_bits_array[pozice_pole]);
//        }
//        else
//        {
//          meteo_bits_array[pozice_pole]=  0;
//          dprint(meteo_bits_array[pozice_pole]);
//        }
//      pozice_pole +=1;
//    }
/////////////////////////////////////

    bool temp;
    for(unsigned int i=0; i <= timings_size; i++) {

      
      temp = i % 2;   //zajisti nam, ze jsme prosli vzdy druhy pruchod, pokud se rovna 1 (technicky mame zbytek pri deleni)

      if (temp == 1)   //mame druhy temp == 1 a jeste pokracujem v zapisu dat - neni konec
      {
        if ((timings[i-1] == 0) and (timings[i] == 1))
        {
          meteo_bits_array[pozice_pole]=  1;
          dprint(meteo_bits_array[pozice_pole]);
        }
        else if ((timings[i-1] == 1) and (timings[i] == 0))
        {
          meteo_bits_array[pozice_pole]=  0;
          dprint(meteo_bits_array[pozice_pole]);
        }
        else if ((timings[i-1] == 0) and (timings[i] == 0))
        {
          i = timings_size;   //mame usekly retezec bez poslednich nul; na data to nema vliv, usetri strojovy cas
        }
        else if ((timings[i-1] == 0) and (timings[i] == 2))
        {
          //obcas se chytne i toto
          //192 : b: 972 488 288 4888 0 0 0 0 
          //t: 011001100110011001100110100110010110100101101010010101010101011010101010101010101001100101010101100110101001011010101010100110100110011010101010011010010110100101101010010110010010102010101013

          i = timings_size;   //mame usekly retezec bez poslednich nul; na data to nema vliv, usetri strojovy cas
        }
        
        pozice_pole +=1;
      } //end if


    } //end for
    
    dprint('\n');
    dprint('\n');

    dprintln("");


    Decode_Temp();
    
}

void Decode_Temp()
{



    //kontrola CRC, bohuzel evidentne nic neznamena
    if (!check_crc_wh5029())
    {
      dprintln(F("CRC ERR, pokracuji."));
    }

  

    if (validate_type_WH5029() == false)   //zvalidujeme typ
    {

      dprintln(F("Nesedi typ! Koncim."));
      return;
    }



    if (validate_two_complement_WH5029() == false)   //zvalidujeme two complement
    {
      //zde lze teoreticky zkusit two complement opravit a pokud se bude podobat predesle hodnote , tak jej lze pozadovat za platny
      dprintln(F("Nesedi two complement! Koncim"));
      return;
    }

    dprint(F("crc: "));
    int CRC = readBits(80, 8);


    dprint(F("id: "));
    byte id = readBits(24, 8);
    if (id != const_id)
    {
    dprintln(F("ID nesedi! Koncim."));
    return;
    }
  
    dprint(F("batt: "));
    boolean batt = meteo_bits_array[32];
    dprintln(batt);
    
    dprint(F("temp: "));
    int temp = readBits(36, 12);
    if (temp > 2048) temp -= 4096; // handle negative values


    if (temp > 600 || temp < -200)  //temp je v int, tedy hodnota je 10x vetsi, aby to nebyl float
    {
      dprintln(F("Teplota je mimo rozsah! Koncim."));
      return;
    }



    dprint(F("hum: "));
    byte hum = readBits(48, 8);

    //obcas dorazi toto, zahodit, protoze to vyresetuje dest a v pristim mereni to zapocita cele cislo, navic vlhkost 0 je take na 100% chybné
    //1010101010100101100110001111111000000000001100000000000000000000000000000000000000000000,temp:4.8,hum:0,rain:0.00,wind_speed:0,wind_dir:0,batt:1,id:254,crc:0,crc_dopocet:0x56
    
    if (hum == 0)
    {
      //musime zabranit resetu deste a vlhkost 0 je urcite chyba
      dprintln(F("Err vlhkost je 0! Koncim"));
      return;
    }
    
    dprint(F("rain: "));
    int rain;

    //pokud mame reset, musime nastavit vychozi hodnotu deste na novou "nulu", vuci ktere budem dest pocitat
    if (rain_last == -1)
    {
      rain_last = readBits(56, 12); //nacteme aktualni hodnotu co poslala meteo - to bude nova nula
      rain = NULL;
      
    }
    else
    {
      rain = readBits(56, 12);
      if (rain < rain_last) rain_last = 0; //reset of outer meteostation, rain is zero; or overflow 4095 => 0; is possible to rain_last = rain; there is potentially risk to loose some data when the reception is bad, but probably is this the best solutin
      if (rain != rain_last) //save some cpu cycles
      {
        rain -= rain_last;
        rain_last = readBits(56, 12);  
      }
      else rain = 0;
      
    }  
    dprintexp(rain);
    
    dprint(F("wind speed: "));
    byte wind_speed = readBits(68, 8);

    dprint(F("wind dir: "));
    byte wind_dir = readBits(76, 4);


    dprintln("");
    dprintln(F("Dekodovana data: "));


    dprint(F("id: "));
    dprintln(id);

    dprint(F("batt: "));
    dprintln(batt ? F("LOW") : F("OK"));
    
    
    dprint(F("temp: "));
    if (temp > 2048) temp -= 4096; // handle negative values
    dprint2((temp/10.0f),1);
    dprintln(F("C"));
    
    dprint(F("hum: "));
    if (hum > 99) hum = 99;
    dprint(hum);
    dprintln(F("%"));
    
    dprint(F("rain: "));
    dprint2((rain * 0.75),1 );
    dprintln(F("mm"));
    
    
    dprint(F("wind speed: "));
    dprint(wind_speed);
    dprintln(F("km/h"));

    dprint(F("wind dir: "));
    dprintln(wind_dir);

    dprint(F("crc: "));
    dprintln(CRC);
 
    // feelTemp calculation
    char feeltempOut[7];
    float feeltempcalc = (hum/100.0)*6.105*pow(2.71828,((17.27*(temp/10.0f))/(237.7+(temp/10.0f))));
    float feeltemp = round(((temp/10.0f)+0.33*feeltempcalc-0.7*(wind_speed/3.6)-4)*10)/10.0;
    dtostrf(feeltemp, 2, 1, feeltempOut);
    dprint(F("feelTemp: "));
    dprintln(feeltempOut);
 
    // dewpoint calculation
    float dewK = 237.7;
    float dewZ = 6.11;
    float dewC = 7.5;
    float dewd = dewC*(temp/10.0f)/(dewK+(temp/10.0f));
    float dewES = dewZ*pow(10,dewd);
    float dewE = (hum*dewES)/100.0;
    float dewf = log(dewE/dewZ);
    float dewpoint = dewf*dewK/(dewC*log(10)-dewf);
    dprint(F("dewpoint: "));
    dprintln(dewpoint);
 
}

boolean check_crc_wh5029()
{
  //1010101010100101 10011000 11111110 00001111 11111011 01000101 00000010 11100000 01001001 11110011
  //1010101010100101 10011000 11111110 00001111 11010111 01001011 00000010 11110000 00101011 10000011
  
  //1010101010100101 10011000 11111110 00001111 11110011 00111000 00000010 11110000 00111100 10010011
  //1010101010100101 10011000 11111110 00001111 11100110 00111100 00000010 11110000 00100111 01011001

  //1010101010100101 10011000 11111110 00001111 10010010 01010110 00000010 11110000 00001011 10001011
  //1010101010100101 10011000 11111110 00001111 10011100 01011001 00000010 11110000 00001011 10011010
  
  byte crc = 0;

  for(int i = 1; i < 9; i++)
  {
    crc ^= readBits((8 + i*8), 8);
  }

  crc ^= readBits(80, 8);

  dprint(F("CRC: ")); dprint2(crc, HEX); dprintln();
  
  byte crc_low_bits = crc & 0xF; //resetnem si pripadne vrchni bity
  dprint(F("crc_low_bits: ")); dprint2(crc_low_bits, HEX); dprintln();
  
  if ( crc_low_bits == 0xF)
  {
      dprint(F("CRC 0x")); dprint2(crc, HEX); dprintln(F(" OK"))
      return true;
  }
  else
  {
      dprint(F("CRC 0x")); dprint2(crc, HEX); dprintln(F(" ERR"))
      return false;
  }
}

boolean validate_type_WH5029()
{
  //10101010 10100101 10011000 1111111000001111111110110100010100000010111000000100100111110011
  
  byte bity;
  bity = 0;
  
  
  bity = readBits(16, 8);
  dprintexp(bity);
  if (bity != B10011000)
    {
      dprintln(F("Err validace typu"));
      return false;
    }

  dprintln(F("Validace typu OK"));
  return true;
}

boolean validate_two_complement_WH5029()
{
  //101010101010010110011000111111100000 1111 111110110100010100000010111000000100100111110011
  
  byte bity;
  bity = 0;
  
  bity = readBits(36, 4);
 
  dprintexp(bity);
  if (bity == B1111 || bity == B0000 || bity == B0001) //pokud je 0000 je teplota kladna, pokud 1111 je zaporna, pokud je 0001 je teplota >= 25,6C, nic jineho neni akceptovatelne
    {
      dprintln(F("Validace two complement OK"));
      return true;
    }
   else
    {
      dprintln(F("Err validace two complement"));
      return false;
    }
}

int readBits(byte start, byte count)
{
    int val = 0;
    for(byte i = start; i < start + count; i++) {
        val <<= 1;
        val |= meteo_bits_array[i];
    }
    dprintln("");
    return val;
}
