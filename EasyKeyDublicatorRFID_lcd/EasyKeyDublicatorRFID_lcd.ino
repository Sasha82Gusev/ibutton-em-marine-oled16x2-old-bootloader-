#include <OneWire.h>
#include "microLiquidCrystal_I2C.h"
LiquidCrystal_I2C lcd (0x27, 16, 2); // Устанавливаем дисплей

#define iButtonPin A3      // Линия data ibutton
#define R_Led 2            // RGB Led
//#define G_Led 3
//#define B_Led 4
//#define ACpinGnd 5         // Земля аналогового компаратора
#define ACpin 6            // Вход Ain0 аналогового компаратора 0.1В для EM-Marie 
#define BtnRWPin 8           // Кнопка переключения режима чтение/запись
#define BtnLSPin 9        // Кнопка переключения keyid 
#define speakerPin 10       // Спикер, он же buzzer, он же beeper
#define FreqGen 11         // генератор 125 кГц
//#define speakerPinGnd 12   // земля Спикера
//#define blueModePin A2      // Эмулятор ключа rfid
//#define rfidBitRate 2       // Скорость обмена с rfid в kbps
//#define rfidUsePWD 0        // ключ использует пароль для изменения
//#define rfidPWD 123456      // пароль для ключа

OneWire ibutton (iButtonPin);
byte addr[8];                             // временный буфер
byte keyID[8];                            // ID ключа для записи
byte rfidData[5];                         // значащие данные frid em-marine
bool readflag = false;                    // флаг сигнализирует, что данные с ключа успечно прочианы в ардуино
bool writeflag = false;                   // режим запись/чтение
int numberofkey = 0;                       //номер ключа из памяти
bool preBtnRWPinSt = HIGH;
bool preBtnLSPinSt = HIGH;
enum emRWType {rwUnknown, TM01, RW1990_1, RW1990_2, TM2004, T5557, EM4305};               // тип болванки
enum emkeyType {keyUnknown, keyDallas, keyTM2004, keyCyfral, keyMetacom, keyEM_Marie};    // тип оригинального ключа
emkeyType keyType;

void setup() {   ////Старт Загрузки
  lcd.init();
  lcd.backlight();// Включаем подсветку дисплея
  lcd.setCursor(0, 0);
  lcd.print("Hello Sanya");
  lcd.setCursor(0, 1);
  lcd.print("no key");
  pinMode(BtnRWPin, INPUT_PULLUP);                            // включаем чтение и подягиваем пин кнопки режима к +5В
  pinMode(BtnLSPin, INPUT_PULLUP);                            // подягиваем пин кнопки keyid к +5В
  //  pinMode(BtnPinGnd, OUTPUT); digitalWrite(BtnPinGnd, LOW); // подключаем второй пин кнопки к земле
  pinMode(speakerPin, OUTPUT);
  //  pinMode(speakerPinGnd, OUTPUT); digitalWrite(speakerPinGnd, LOW); // подключаем второй пин спикера к земле
  pinMode(ACpin, INPUT);                                            // Вход аналогового компаратора 3В для Cyfral
  // pinMode(ACpinGnd, OUTPUT);digitalWrite(ACpinGnd, LOW);           // подключаем второй пин аналогового компаратора Cyfral к земле
  pinMode(R_Led, OUTPUT); //pinMode(G_Led, OUTPUT);// pinMode(B_Led, OUTPUT);  //RGB-led
  // digitalWrite(blueModePin, LOW); pinMode(blueModePin, OUTPUT);
  digitalWrite(R_Led, LOW);
  pinMode(FreqGen, OUTPUT);
  //  digitalWrite(B_Led, HIGH);                                //awaiting of origin key data
  Serial.begin(9600);
  Sd_StartOK();//Пищим успешную загрузку
}   ///Конец загрузки

//void clearLed(){
//digitalWrite(R_Led, LOW);
// digitalWrite(G_Led, LOW);
//  digitalWrite(B_Led, LOW);
//}

//*************** dallas **************

emRWType getRWtype() {
  byte answer;
  // TM01 это неизвестный тип болванки, делается попытка записи TM-01 без финализации для dallas или c финализацией под cyfral или metacom
  // RW1990_1 - dallas-совместимые RW-1990, RW-1990.1, ТМ-08, ТМ-08v2
  // RW1990_2 - dallas-совместимая RW-1990.2
  // TM2004 - dallas-совместимая TM2004 в доп. памятью 1кб
  // пробуем определить RW-1990.1
  ibutton.reset(); ibutton.write(0xD1); // проуем снять флаг записи для RW-1990.1
  ibutton.write_bit(1);                 // записываем значение флага записи = 1 - отключаем запись
  delay(10); pinMode(iButtonPin, INPUT);
  ibutton.reset(); ibutton.write(0xB5); // send 0xB5 - запрос на чтение флага записи
  answer = ibutton.read();
  //Serial.print("\n Answer RW-1990.1: "); Serial.println(answer, HEX);
  if (answer == 0xFE) {
    Serial.println(" Type: dallas RW-1990.1 ");
    lcd.setCursor(0, 0);
    lcd.print("dallas RW-1990.1");




    return RW1990_1;            // это RW-1990.1
  }
  // пробуем определить RW-1990.2
  ibutton.reset(); ibutton.write(0x1D);  // пробуем установить флаг записи для RW-1990.2
  ibutton.write_bit(1);                  // записываем значение флага записи = 1 - включаем запись
  delay(10); pinMode(iButtonPin, INPUT);
  ibutton.reset(); ibutton.write(0x1E);  // send 0x1E - запрос на чтение флага записи
  answer = ibutton.read();
  if (answer == 0xFE) {
    ibutton.reset(); ibutton.write(0x1D); // возвращаем оратно запрет записи для RW-1990.2
    ibutton.write_bit(0);                 // записываем значение флага записи = 0 - выключаем запись
    delay(10); pinMode(iButtonPin, INPUT);
    Serial.println(" Type: dallas RW-1990.2 ");
    lcd.setCursor(0, 0);
    lcd.print("dallas RW-1990.2");
    return RW1990_2; // это RW-1990.2
  }
  // пробуем определить TM-2004
  ibutton.reset(); ibutton.write(0x33);                     // посылаем команду чтения ROM для перевода в расширенный 3-х байтовый режим
  for ( byte i = 0; i < 8; i++) ibutton.read();             // читаем данные ключа
  ibutton.write(0xAA);                                      // пробуем прочитать регистр статуса для TM-2004
  ibutton.write(0x00); ibutton.write(0x00);                 // передаем адрес для считывания
  answer = ibutton.read();                                  // читаем CRC комманды и адреса
  byte m1[3] = {0xAA, 0, 0};                                // вычисляем CRC комманды
  if (OneWire::crc8(m1, 3) == answer) {
    answer = ibutton.read();                                  // читаем регистр статуса
    //Serial.print(" status: "); Serial.println(answer, HEX);
    Serial.println(" Type: dallas TM2004");
    lcd.setCursor(0, 0);
    lcd.print("dallas TM2004");
    ibutton.reset();
    return TM2004; // это Type: TM2004
  }
  ibutton.reset();
  Serial.println(" Type: dallas unknown, trying TM-01! ");
  lcd.setCursor(0, 0);
  lcd.print("dallas unknown  ");
  return TM01;                              // это неизвестный тип DS1990, нужно перебирать алгоритмы записи (TM-01)
}

bool write2iBtnTM2004() {               // функция записи на TM2004
  byte answer; bool result = true;
  ibutton.reset();
  ibutton.write(0x3C);                                      // команда записи ROM для TM-2004
  ibutton.write(0x00); ibutton.write(0x00);                 // передаем адрес с которого начинается запись
  for (byte i = 0; i < 8; i++) {
    digitalWrite(R_Led, !digitalRead(R_Led));
    ibutton.write(keyID[i]);
    answer = ibutton.read();
    //if (OneWire::crc8(m1, 3) != answer){result = false; break;}     // crc не верный
    delayMicroseconds(600); ibutton.write_bit(1); delay(50);         // испульс записи
    pinMode(iButtonPin, INPUT);
    Serial.print('*');
    Sd_WriteStep();
    if (keyID[i] != ibutton.read()) {
      result = false;  //читаем записанный байт и сравниваем, с тем что должно записаться
      break;
    }
  }
  if (!result) {
    ibutton.reset();
    Serial.println(" The key copy faild");
    Sd_ErrorBeep();
    digitalWrite(R_Led, HIGH);
    return false;
  }
  ibutton.reset();
  Serial.println(" The key has copied successesfully");
  Sd_ReadOK();
  delay(500);
  digitalWrite(R_Led, HIGH);
  return true;
}
// функция записи на RW1990.1, RW1990.2, TM-01C(F)
bool write2iBtnRW1990_1_2_TM01(emRWType rwType) {             // функция записи на RW1990.1, RW1990.2, TM-01C(F)
  byte rwCmd, rwFlag = 1;
  switch (rwType) {
    case TM01: rwCmd = 0xC1; break;                   //TM-01C(F)
    case RW1990_1: rwCmd = 0xD1; rwFlag = 0; break;  // RW1990.1  флаг записи инвертирован
    case RW1990_2: rwCmd = 0x1D; break;              // RW1990.2
  }
  ibutton.reset(); ibutton.write(rwCmd);       // send 0xD1 - флаг записи
  ibutton.write_bit(rwFlag);                   // записываем значение флага записи = 1 - разрешить запись
  delay(10); pinMode(iButtonPin, INPUT);
  ibutton.reset(); ibutton.write(0xD5);        // команда на запись
  for (byte i = 0; i < 8; i++) {
    digitalWrite(R_Led, !digitalRead(R_Led));
    if (rwType == RW1990_1) BurnByte(~keyID[i]);      // запись происходит инверсно для RW1990.1
    else BurnByte(keyID[i]);
    Serial.print('*');
    Sd_WriteStep();
  }
  ibutton.write(rwCmd);                     // send 0xD1 - флаг записи
  ibutton.write_bit(!rwFlag);               // записываем значение флага записи = 1 - отключаем запись
  delay(10); pinMode(iButtonPin, INPUT);
  digitalWrite(R_Led, LOW);
  if (!dataIsBurningOK()) {         // проверяем корректность записи
    Serial.println(" The key copy faild");
    Sd_ErrorBeep();
    digitalWrite(R_Led, HIGH);
    return false;
  }
  Serial.println(" The key has copied successesfully");
  if ((keyType == keyMetacom) || (keyType == keyCyfral)) {   //переводим ключ из формата dallas
    ibutton.reset();
    if (keyType == keyCyfral) ibutton.write(0xCA);       // send 0xCA - флаг финализации Cyfral
    else ibutton.write(0xCB);                       // send 0xCA - флаг финализации metacom
    ibutton.write_bit(1);                             // записываем значение флага финализации = 1 - перевезти формат
    delay(10); pinMode(iButtonPin, INPUT);
  }
  Sd_ReadOK();
  delay(500);
  digitalWrite(R_Led, HIGH);
  return true;
}

void BurnByte(byte data) {
  for (byte n_bit = 0; n_bit < 8; n_bit++) {
    ibutton.write_bit(data & 1);
    delay(5);                        // даем время на прошивку каждого бита до 10 мс
    data = data >> 1;                // переходим к следующему bit
  }
  pinMode(iButtonPin, INPUT);
}

bool dataIsBurningOK() {
  byte buff[8];
  if (!ibutton.reset()) return false;
  ibutton.write(0x33);
  ibutton.read_bytes(buff, 8);
  byte Check = 0;
  for (byte i = 0; i < 8; i++)
    if (keyID[i] == buff[i]) Check++;      // сравниваем код для записи с тем, что уже записано в ключе.
  if (Check != 8) return false;             // если коды совпадают, ключ успешно скопирован
  return true;
}

bool write2iBtn() {
  int Check = 0;
  if (!ibutton.search(addr)) {
    ibutton.reset_search();
    return false;
  }
  Serial.print("The new key code is: ");
  for (byte i = 0; i < 8; i++) {
    Serial.print(addr[i], HEX); Serial.print(":");
    if (keyID[i] == addr[i]) Check++;    // сравниваем код для записи с тем, что уже записано в ключе.
  }
  if (Check == 8) {                     // если коды совпадают, ничего писать не нужно
    digitalWrite(R_Led, LOW);
    Serial.println(" it is the same key. Writing in not needed.");
    Sd_ErrorBeep();
    digitalWrite(R_Led, HIGH);
    delay(500);
    return false;
  }
  emRWType rwType = getRWtype(); // определяем тип RW-1990.1 или 1990.2 или TM-01
  Serial.print("\n Burning iButton ID: ");
  if (rwType == TM2004) return write2iBtnTM2004();  //шьем TM2004
  else return write2iBtnRW1990_1_2_TM01(rwType); //пробуем прошить другие форматы
}

bool searchIbutton() {
  if (!ibutton.search(addr)) {
    ibutton.reset_search();
    return false;
  }
  lcd.setCursor(0, 1);
  Serial.println();
  for (byte i = 0; i < 8; i++) {
    // Serial.print(addr[i], HEX); Serial.print(":");
    if (addr[i] < 16) {
      Serial.print("0"); Serial.print(addr[i], HEX); Serial.print(":");
      lcd.print("0"); lcd.print(addr[i], HEX);
    }
    else {
      Serial.print(addr[i], HEX); Serial.print(":");
      lcd.print(addr[i], HEX);
    }
    keyID[i] = addr[i];                               // копируем прочтенный код в ReadID
  }

  if (addr[0] == 0x01) {                         // это ключ формата dallas
    keyType = keyDallas;
    if (getRWtype() == TM2004) keyType = keyTM2004;
    if (OneWire::crc8(addr, 7) != addr[7]) {
      Serial.println("CRC is not valid!");
      Sd_ErrorBeep();
      //digitalWrite(B_Led, HIGH);
      return false;
    }
    return true;
  }
  if ((addr[0] >> 4) == 0x0E) Serial.println(" Type: unknown family dallas. May be cyfral in dallas key.");


  else Serial.println(" Type: unknown family dallas");
  keyType = keyUnknown;
  return true;
}

//************ Cyfral ***********************
unsigned long pulseAComp(bool pulse, unsigned long timeOut = 20000) { // pulse HIGH or LOW
  bool AcompState;
  unsigned long tStart = micros();
  do {
    AcompState = (ACSR >> ACO) & 1; // читаем флаг компаратора
    if (AcompState == pulse) {
      tStart = micros();
      do {
        AcompState = (ACSR >> ACO) & 1; // читаем флаг компаратора
        if (AcompState != pulse) return (long)(micros() - tStart);
      } while ((long)(micros() - tStart) < timeOut);
      return 0;                                                 //таймаут, импульс не вернуся оратно
    }             // end if
  } while ((long)(micros() - tStart) < timeOut);
  return 0;
}

void ACsetOn() {
  ACSR |= 1 << ACBG;                          // Подключаем ко входу Ain0 1.1V для Cyfral/Metacom
  ADCSRA &= ~(1 << ADEN);                     // выключаем ADC
  ADMUX = (ADMUX & 0b11110000) | 0b0011;      // подключаем к AC Линию A3
  ADCSRB |= 1 << ACME;                        // включаем мультиплексор AC
}

bool read_cyfral(byte* buf, byte CyfralPin) {///Пробуем определить ключь цыфрал
  unsigned long ti; byte j = 0;
  digitalWrite(CyfralPin, LOW); pinMode(CyfralPin, OUTPUT);  //отклчаем питание от ключа
  delay(100);
  pinMode(CyfralPin, INPUT);  // включаем пиание Cyfral
  ACsetOn();
  for (byte i = 0; i < 36; i++) { // чиаем 36 bit
    ti = pulseAComp(HIGH);
    if ((ti == 0) || (ti > 200)) break;                      // not Cyfral
    //if ((ti > 20)&&(ti < 50)) bitClear(buf[i >> 3], 7-j);
    if ((ti > 90) && (ti < 200)) bitSet(buf[i >> 3], 7 - j);
    j++; if (j > 7) j = 0;
  }
  if (ti == 0) return false;
  if ((buf[0] >> 4) != 0b1110) return false;   /// not Cyfral
  byte test;
  for (byte i = 1; i < 4; i++) {
    test = buf[i] >> 4;
    if ((test != 1) && (test != 2) && (test != 4) && (test != 8)) return false;
    test = buf[i] & 0x0F;
    if ((test != 1) && (test != 2) && (test != 4) && (test != 8)) return false;
  }
  return true;
}
///конец Пробуем определить ключь цыфрал

bool searchCyfral() {
  for (byte i = 0; i < 8; i++) addr[i] = 0;
  if (!read_cyfral(addr, iButtonPin)) return false;
  keyType = keyCyfral;
  for (byte i = 0; i < 8; i++) {
    Serial.print(addr[i], HEX); Serial.print(":");
    keyID[i] = addr[i];                               // копируем прочтенный код в ReadID
  }
  Serial.println(" Type: Cyfral ");
  return true;
}

//**********EM-Marine***************************
bool vertEvenCheck(byte* buf) {       // проверка четности столбцов с данными
  byte k;
  k = 1 & buf[1] >> 6 + 1 & buf[1] >> 1 + 1 & buf[2] >> 4 + 1 & buf[3] >> 7 + 1 & buf[3] >> 2 + 1 & buf[4] >> 5 + 1 & buf[4] + 1 & buf[5] >> 3 + 1 & buf[6] >> 6 + 1 & buf[6] >> 1 + 1 & buf[7] >> 4;
  if (k & 1) return false;
  k = 1 & buf[1] >> 5 + 1 & buf[1] + 1 & buf[2] >> 3 + 1 & buf[3] >> 6 + 1 & buf[3] >> 1 + 1 & buf[4] >> 4 + 1 & buf[5] >> 7 + 1 & buf[5] >> 2 + 1 & buf[6] >> 5 + 1 & buf[6] + 1 & buf[7] >> 3;
  if (k & 1) return false;
  k = 1 & buf[1] >> 4 + 1 & buf[2] >> 7 + 1 & buf[2] >> 2 + 1 & buf[3] >> 5 + 1 & buf[3] + 1 & buf[4] >> 3 + 1 & buf[5] >> 6 + 1 & buf[5] >> 1 + 1 & buf[6] >> 4 + 1 & buf[7] >> 7 + 1 & buf[7] >> 2;
  if (k & 1) return false;
  k = 1 & buf[1] >> 3 + 1 & buf[2] >> 6 + 1 & buf[2] >> 1 + 1 & buf[3] >> 4 + 1 & buf[4] >> 7 + 1 & buf[4] >> 2 + 1 & buf[5] >> 5 + 1 & buf[5] + 1 & buf[6] >> 3 + 1 & buf[7] >> 6 + 1 & buf[7] >> 1;
  if (k & 1) return false;
  if (1 & buf[7]) return false;
  //номер ключа, который написан на корпусе
  rfidData[0] = (0b01111000 & buf[1]) << 1 | (0b11 & buf[1]) << 2 | buf[2] >> 6;
  rfidData[1] = (0b00011110 & buf[2]) << 3 | buf[3] >> 4;
  rfidData[2] = buf[3] << 5 | (0b10000000 & buf[4]) >> 3 | (0b00111100 & buf[4]) >> 2;
  rfidData[3] = buf[4] << 7 | (0b11100000 & buf[5]) >> 1 | 0b1111 & buf[5];
  rfidData[4] = (0b01111000 & buf[6]) << 1 | (0b11 & buf[6]) << 2 | buf[7] >> 6;
  return true;
}

byte ttAComp(unsigned long timeOut = 10000) { // pulse 0 or 1 or -1 if timeout
  byte AcompState, AcompInitState;
  unsigned long tStart = micros();
  AcompInitState = (ACSR >> ACO) & 1;             // читаем флаг компаратора
  do {
    AcompState = (ACSR >> ACO) & 1;               // читаем флаг компаратора
    if (AcompState != AcompInitState) {
      delayMicroseconds(1000 / (2 * 4)); // 1/4 Period on 2 kBps = 125 mks
      AcompState = (ACSR >> ACO) & 1;             // читаем флаг компаратора
      delayMicroseconds(1000 / (2 * 2)); // 1/2 Period on 2 kBps = 250 mks
      return AcompState;
    }
  } while ((long)(micros() - tStart) < timeOut);
  return 2;                                             //таймаут, компаратор не сменил состояние
}

bool readEM_Marie(byte* buf) {
  unsigned long tStart = millis();
  byte ti; byte j = 0, k = 0;
  for (int i = 0; i < 64; i++) { // читаем 64 bit
    ti = ttAComp();
    if (ti == 2)  break;         //timeout
    //Serial.print("b ");
    if ( ( ti == 0 ) && ( i < 9)) {  // если не находим 9 стартовых единиц - начинаем сначала
      if ((long)(millis() - tStart) > 50) {
        ti = 2;  //timeout
        break;
      }
      i = -1; j = 0; continue;
    }
    if ((i > 8) && (i < 59)) {    //начиная с 9-го бита проверяем контроль четности каждой строки
      if (ti) k++;                // считаем кол-во единиц
      if ( (i - 9) % 5 == 4 ) {   // конец строки с данными из 5-и бит,
        if (k & 1) {              //если нечетно - начинаем сначала
          i = -1; j = 0; k = 0; continue;
        }
        k = 0;
      }
    }
    if (ti) bitSet(buf[i >> 3], 7 - j);
    else bitClear(buf[i >> 3], 7 - j);
    j++; if (j > 7) j = 0;
  }
  if (ti == 2) return false;         //timeout
  return vertEvenCheck(buf);
}

void rfidACsetOn() {
  //включаем генератор 125кГц
  TCCR2A = _BV(COM2A0) | _BV(COM2B1) | _BV(WGM21) | _BV(WGM20);  //Вкючаем режим Toggle on Compare Match на COM2A (pin 11) и счет таймера2 до OCR2A
  TCCR2B = _BV(WGM22) | _BV(CS20);                                // Задаем делитель для таймера2 = 1 (16 мГц)
  OCR2A = 63;                                                    // 63 тактов на период. Частота на COM2A (pin 11) 16000/64/2 = 125 кГц, Скважнось COM2A в этом режиме всегда 50%
  OCR2B = 31;                                                     // Скважность COM2B 32/64 = 50%  Частота на COM2A (pin 3) 16000/64 = 250 кГц
  // включаем компаратор
  ADCSRB &= ~(1 << ACME);         // отключаем мультиплексор AC
  ACSR &= ~(1 << ACBG);           // отключаем от входа Ain0 1.1V
}

bool searchEM_Marine( bool copyKey = true) {
  // byte gr = 0;//digitalRead(G_Led);
  bool rez = false;
  rfidACsetOn();            // включаем генератор 125кГц и компаратор
  delay(13);                //13 мс длятся переходные прцессы детектора
  if (!readEM_Marie(addr)) {
    if (!copyKey) TCCR2A &= 0b00111111;
    return rez;
  };
  rez = true;
  keyType = keyEM_Marie;
  lcd.setCursor(0, 1);
  for (byte i = 0; i < 8; i++) {
    if (copyKey) keyID[i] = addr [i];
    // Serial.print(addr[i], HEX); Serial.print(":");

    if (addr[i] < 16) {
      Serial.print("0"); Serial.print(addr[i], HEX); Serial.print(":");
      lcd.print("0"); lcd.print(addr[i], HEX);
    }
    else {
      Serial.print(addr[i], HEX); Serial.print(":");
      lcd.print(addr[i], HEX);
    }


  }
  Serial.print(" ( id ");
  Serial.print(rfidData[0]); Serial.print(" key ");
  //unsigned long keyNum = (unsigned long)rfidData[1] << 24 | (unsigned long)rfidData[2] << 16 | (unsigned long)rfidData[3] << 8 | (unsigned long)rfidData[4];
  unsigned long keyNum = (unsigned long)rfidData[1] << 24 | (unsigned long)rfidData[2] << 16 | (unsigned long)rfidData[3] << 8 | (unsigned long)rfidData[4];
  Serial.print(keyNum);
  Serial.println(" )Type: EM-Marie ");
  lcd.setCursor(0, 0);
  lcd.print("EM-Marie        ");

  if (!copyKey) TCCR2A &= 0b00111111;             //Оключить ШИМ COM2A (pin 11)
  //  digitalWrite(G_Led, gr);
  return rez;
}

void TxBitRfid(byte data) {
  if (data & 1) delayMicroseconds(54 * 8);
  else delayMicroseconds(24 * 8);
  rfidGap(19 * 8);                     //write gap
}

void TxByteRfid(byte data) {
  for (byte n_bit = 0; n_bit < 8; n_bit++) {
    TxBitRfid(data & 1);
    data = data >> 1;                   // переходим к следующему bit
  }
}

void rfidGap(unsigned int tm) {
  TCCR2A &= 0b00111111;               //Оключить ШИМ COM2A
  delayMicroseconds(tm);
  TCCR2A |= _BV(COM2A0);              // Включить ШИМ COM2A (pin 11)
}

bool T5557_blockRead(byte* buf) {
  byte ti; byte j = 0, k = 0;
  for (int i = 0; i < 33; i++) { // читаем стартовый 0 и 32 значащих bit
    ti = ttAComp(2000);
    if (ti == 2)  break;         //timeout
    if ( ( ti == 1 ) && ( i == 0)) {  // если не находим стартовый 0 - это ошибка
      ti = 2;
      Serial.print("b2 ");
      break;
    }
    if (i > 0) {    //начиная с 1-го бита пишем в буфер
      if (ti) bitSet(buf[(i - 1) >> 3], 7 - j);
      else bitClear(buf[(i - 1) >> 3], 7 - j);
      j++; if (j > 7) j = 0;
    }
  }
  if (ti == 2) return false;         //timeout
  return true;
}

bool sendOpT5557(byte opCode, unsigned long password = 0, byte lockBit = 0, unsigned long data = 0, byte blokAddr = 1) {
  TxBitRfid(opCode >> 1); TxBitRfid(opCode & 1); // передаем код операции 10
  if (opCode == 0b00) return true;
  // password
  TxBitRfid(lockBit & 1);               // lockbit 0
  if (data != 0) {
    for (byte i = 0; i < 32; i++) {
      TxBitRfid((data >> (31 - i)) & 1);
    }
  }
  TxBitRfid(blokAddr >> 2); TxBitRfid(blokAddr >> 1); TxBitRfid(blokAddr & 1);  // адрес блока для записи
  delay(4);                       // ждем пока пишутся данные
  return true;
}

bool write2rfidT5557(byte* buf) {
  bool result; unsigned long data32;
  delay(6);
  for (byte k = 0; k < 2; k++) {                                    // send key data
    data32 = (unsigned long)buf[0 + (k << 2)] << 24 | (unsigned long)buf[1 + (k << 2)] << 16 | (unsigned long)buf[2 + (k << 2)] << 8 | (unsigned long)buf[3 + (k << 2)];
    rfidGap(30 * 8);                                                 //start gap
    sendOpT5557(0b10, 0, 0, data32, k + 1);                          //передаем 32 бита ключа в blok k
    Serial.print('*'); delay(6);
  }
  delay(6);
  rfidGap(30 * 8);          //start gap
  sendOpT5557(0b00);
  result = readEM_Marie(addr);
  TCCR2A &= 0b00111111;             //Оключить ШИМ COM2A (pin 11)
  for (byte i = 0; i < 8; i++)
    if (addr[i] != keyID[i]) {
      result = false;
      break;
    }
  if (!result) {
    Serial.println(" The key copy faild");
    Sd_ErrorBeep();
  } else {
    Serial.println(" The key has copied successesfully");
    Sd_ReadOK();
    delay(1000);
  }
  digitalWrite(R_Led, HIGH);
  return result;
}

emRWType getRfidRWtype() {
  unsigned long data32, data33; byte buf[4] = {0, 0, 0, 0};
  rfidACsetOn();            // включаем генератор 125кГц и компаратор
  delay(13);                //13 мс длятся переходные прцессы детектора
  rfidGap(30 * 8);          //start gap
  sendOpT5557(0b11, 0, 0, 0, 1); //переходим в режим чтения Vendor ID
  if (!T5557_blockRead(buf)) return rwUnknown;
  data32 = (unsigned long)buf[0] << 24 | (unsigned long)buf[1] << 16 | (unsigned long)buf[2] << 8 | (unsigned long)buf[3];
  delay(4);
  rfidGap(20 * 8);          //gap
  data33 = 0b00000000000101001000000001000000 | (0 << 4);   //конфиг регистр 0b00000000000101001000000001000000
  sendOpT5557(0b10, 0, 0, data33, 0);   //передаем конфиг регистр
  delay(4);
  rfidGap(30 * 8);          //start gap
  sendOpT5557(0b11, 0, 0, 0, 1); //переходим в режим чтения Vendor ID
  if (!T5557_blockRead(buf)) return rwUnknown;
  data33 = (unsigned long)buf[0] << 24 | (unsigned long)buf[1] << 16 | (unsigned long)buf[2] << 8 | (unsigned long)buf[3];
  sendOpT5557(0b00, 0, 0, 0, 0);  // send Reset
  delay(6);
  if (data32 != data33) return rwUnknown;
  Serial.print(" The rfid RW-key is T5557. Vendor ID is ");
  Serial.println(data32, HEX);
  return T5557;
}

bool write2rfid() {
  bool Check = true;
  if (searchEM_Marine(false)) {
    for (byte i = 0; i < 8; i++)
      if (addr[i] != keyID[i]) {
        Check = false;  // сравниваем код для записи с тем, что уже записано в ключе.
        break;
      }
    if (Check) {                                          // если коды совпадают, ничего писать не нужно
      digitalWrite(R_Led, LOW);
      Serial.println(" it is the same key. Writing in not needed.");
      Sd_ErrorBeep();
      digitalWrite(R_Led, HIGH);
      delay(500);
      return false;
    }
  }
  emRWType rwType = getRfidRWtype(); // определяем тип T5557 (T5577) или EM4305
  if (rwType != rwUnknown) Serial.print("\n Burning rfid ID: ");
 // keyID[0] = 0xFF; keyID[1] = 0xA9; keyID[2] =  0x8A; keyID[3] = 0xA4; keyID[4] = 0x87; keyID[5] = 0x78; keyID[6] = 0x98; keyID[7] = 0x6A;
  switch (rwType) {
    case T5557: return write2rfidT5557(keyID); break;                    //пишем T5557
    //case EM4305: return write2rfidEM4305(keyID); break;                  //пишем EM4305
    case rwUnknown: break;
  }
  return false;
}

void loop() {
  bool BtnRWPinSt  = digitalRead(BtnRWPin);
  bool BtnLSPinSt  = digitalRead(BtnLSPin);
  bool BtnRWClick;
  bool BtnLSClick;
  if ((BtnRWPinSt == LOW) && (preBtnRWPinSt != LOW)) BtnRWClick = true;
  else BtnRWClick = false;
  if ((BtnLSPinSt == LOW) && (preBtnLSPinSt != LOW)) BtnLSClick = true;
  else BtnLSClick = false;
  preBtnRWPinSt = BtnRWPinSt;
  if (BtnRWClick) {  // переключаель режима чтение/запись
    if (readflag == true) {
      writeflag = !writeflag;
      digitalWrite(R_Led, LOW);
      if (writeflag) digitalWrite(R_Led, HIGH);
      else //digitalWrite(G_Led, HIGH);
        Serial.print("Writeflag = "); Serial.println(writeflag);
    } else {
      digitalWrite(R_Led, LOW);
      Sd_ErrorBeep();
      Serial.print("nokey ");
      //      digitalWrite(B_Led, HIGH);
    }
  }
   if (BtnLSClick){ // переключаель следующего ключа
    readflag = true;
    Sd_ReadOK();
   if (numberofkey>14) numberofkey=0;  
   numberofkey=numberofkey+1;
   Serial.print("LS Click "); Serial.println(numberofkey);
   lcd.init();
 
   if (numberofkey == 1) {
keyType = keyDallas;
keyID[0] = 0x01; keyID[1] = 0x00; keyID[2] =  0x00; keyID[3] = 0x00; keyID[4] = 0x00; keyID[5] = 0x90; keyID[6] = 0x19; keyID[7] = 0xFF;//Пишем один из универсальных ключей 
lcd.setCursor(0, 0); lcd.print("Univer1 iButton ");
lcd.setCursor(0, 1); lcd.print("01000000009019FF");
}
 if (numberofkey == 2) {
keyType = keyDallas;
//0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x14
keyID[0] = 0xFF; keyID[1] = 0xFF; keyID[2] =  0xFF; keyID[3] = 0xFF; keyID[4] = 0xFF; keyID[5] = 0xFF; keyID[6] = 0xFF; keyID[7] = 14;//Пишем один из универсальных ключей 
lcd.setCursor(0, 0); lcd.print("Univer2 iButton ");
lcd.setCursor(0, 1); lcd.print("FFFFFFFFFFFFFF14");
}
if (numberofkey == 3) {
keyType = keyDallas;
//0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x2F
keyID[0] = 0x01; keyID[1] = 0xFF; keyID[2] =  0xFF; keyID[3] = 0xFF; keyID[4] = 0xFF; keyID[5] = 0xFF; keyID[6] = 0xFF; keyID[7] = 0x2F;//Пишем один из универсальных ключей 
lcd.setCursor(0, 0); lcd.print("Univer3 iButton ");
lcd.setCursor(0, 1); lcd.print("01FFFFFFFFFFFF2F");
}

if (numberofkey == 4) {
keyType = keyDallas;
//0x01 0xB2, 0x01, 0x2E, 0x01, 0x00, 0x00, 0x57   Алексея толстого 75 Калитка
keyID[0] = 0x01; keyID[1] = 0xB2; keyID[2] =  0x01; keyID[3] = 0x2E; keyID[4] = 0x01; keyID[5] = 0x00; keyID[6] = 0x00; keyID[7] = 0x57; 
lcd.setCursor(0, 0); lcd.print("AT75 Kal iButton");
lcd.setCursor(0, 1); lcd.print("01B2012E01000057");
}

if (numberofkey == 5) {
keyType = keyEM_Marie;
//FF:FB:DE:F7:BD:EF:7B:C0: ( id 255 key 4294967295) Type: EM-Marie син
keyID[0] = 0xFF; keyID[1] = 0xFB; keyID[2] =  0xDE; keyID[3] = 0x7F; keyID[4] = 0xBD; keyID[5] = 0xEF; keyID[6] = 0x7B; keyID[7] = 0xC0;//Пишем один из универсальных ключей 
lcd.setCursor(0, 0); lcd.print("Univer1 EM-Marine");
lcd.setCursor(0, 1); lcd.print("FFFBDEF7BDEF7BC0");
}

if (numberofkey == 6) {
keyType = keyEM_Marie;
//FF:A9:8A:A0:C6:90:5F:BA: ( id 86 key 1511080126) Type: EM-Marie черн
keyID[0] = 0xFF; keyID[1] = 0xA9; keyID[2] =  0x8A; keyID[3] = 0xA0; keyID[4] = 0xC6; keyID[5] = 0x90; keyID[6] = 0x5F; keyID[7] = 0xBA;//Пишем один из универсальных ключей 
lcd.setCursor(0, 0); lcd.print("Univer2 EM-Marine");
lcd.setCursor(0, 1); lcd.print("FFA98AA0C6905FBA");
}

if (numberofkey == 7) {
keyType = keyEM_Marie;
//FF:AA:20:4:9E:FB:ED:4E ( id 86 key 1511080126) Type: EM-Marie Саша Работа
keyID[0] = 0xFF; keyID[1] = 0xAA; keyID[2] =  0x20; keyID[3] = 0x04; keyID[4] = 0x9E; keyID[5] = 0xFB; keyID[6] = 0xED; keyID[7] = 0x4E; 
lcd.setCursor(0, 0); lcd.print("SanRAB EM-Marine");
lcd.setCursor(0, 1); lcd.print("FFAA20049EFBED4E");
}

if (numberofkey == 8) {
keyType = keyEM_Marie;
//FF:B3:60:00:D3:B4:A8:04 ( id 86 key 1511080126) Type: EM-Marie Полевая 7
keyID[0] = 0xFF; keyID[1] = 0xB3; keyID[2] =  0x60; keyID[3] = 0x00; keyID[4] = 0xD3; keyID[5] = 0xB4; keyID[6] = 0xA8; keyID[7] = 0x04; 
lcd.setCursor(0, 0); lcd.print("Polev7 EM-Marine");
lcd.setCursor(0, 1); lcd.print("FFB36000D3B4A804");
}

if (numberofkey == 9) {
keyType = keyEM_Marie;
//FF:B2:E0:03:FA:FF:19:36 ( id 86 key 1511080126) Type: EM-Marie Больничная 16
keyID[0] = 0xFF; keyID[1] = 0xB2; keyID[2] =  0xE0; keyID[3] = 0x03; keyID[4] = 0xFA; keyID[5] = 0xFF; keyID[6] = 0x19; keyID[7] = 0x36; 
lcd.setCursor(0, 0); lcd.print("Boln16 EM-Marine");
lcd.setCursor(0, 1); lcd.print("FFB2E003FAFF1936");
}

if (numberofkey == 10) {
keyType = keyEM_Marie;
//FF:94:60:02:65:71:C8:1A ( id 86 key 1511080126) Type: EM-Marie Настька работа
keyID[0] = 0xFF; keyID[1] = 0x94; keyID[2] =  0x60; keyID[3] = 0x02; keyID[4] = 0x65; keyID[5] = 0x71; keyID[6] = 0xC8; keyID[7] = 0x1A; 
lcd.setCursor(0, 0); lcd.print("NasRAB EM-Marine");
lcd.setCursor(0, 1); lcd.print("FF9460026571C81A");
}

if (numberofkey == 11) {
keyType = keyEM_Marie;
//FF:A9:8A:A7:5F:DC:5C:BC Алексея Толстого 75 3 подъезд
keyID[0] = 0xFF; keyID[1] = 0xA9; keyID[2] =  0x8A; keyID[3] = 0xA7; keyID[4] = 0x5F; keyID[5] = 0xDC; keyID[6] = 0x5C; keyID[7] = 0xBC; 
lcd.setCursor(0, 0); lcd.print("AT75 3p EM_Marin");
lcd.setCursor(0, 1); lcd.print("FFA98AA75FDC5CBC");
}

if (numberofkey == 12) {
keyType = keyEM_Marie;
//FF:A9:8A:A6:FB:DE:80:D8 Азаров 
keyID[0] = 0xFF; keyID[1] = 0xA9; keyID[2] =  0x8A; keyID[3] = 0xA6; keyID[4] = 0xFB; keyID[5] = 0xDE; keyID[6] = 0x80; keyID[7] = 0xD8; 
lcd.setCursor(0, 0); lcd.print("Azarov EM_Marine");
lcd.setCursor(0, 1); lcd.print("FFA98AA6FBDE80D8");
}

if (numberofkey == 13) {
keyType = keyEM_Marie;
//FF:A9:8A:A2:87:D5:25:44 Ново-Вокзальная 146
keyID[0] = 0xFF; keyID[1] = 0xA9; keyID[2] =  0x8A; keyID[3] = 0xA2; keyID[4] = 0x87; keyID[5] = 0xD5; keyID[6] = 0x25; keyID[7] = 0x44; 
lcd.setCursor(0, 0); lcd.print("NV-146 EM_Marine");
lcd.setCursor(0, 1); lcd.print("FFA98AA287D52544");
}

if (numberofkey == 14) {
keyType = keyDallas;
//0x01  0xFC, 0xA1, 0xB9, 0x0F, 0x00, 0x00, 0x91 Карла маркса 6 боковая дверь 
keyID[0] = 0x01; keyID[1] = 0xFC; keyID[2] =  0xA1; keyID[3] = 0xB9; keyID[4] = 0x0F; keyID[5] = 0x00; keyID[6] = 0x00; keyID[7] = 0x91; 
lcd.setCursor(0, 0); lcd.print("KM-6 bok iButton");
lcd.setCursor(0, 1); lcd.print("01FCA1B90F000091");
}

if (numberofkey == 15) {
keyType = keyEM_Marie;
//FF:8D:80:00:D3:15:01:36  EmMArine Башня
keyID[0] = 0xFF; keyID[1] = 0x8D; keyID[2] =  0x80; keyID[3] = 0x00; keyID[4] = 0xD3; keyID[5] = 0x15; keyID[6] = 0x01; keyID[7] = 0x36; 
lcd.setCursor(0, 0); lcd.print("Bashnya EM_Marine");
lcd.setCursor(0, 1); lcd.print("FF8D8000D3150136");
}

}//Конец переключения следующего ключа
  if (!writeflag) {
    if (searchCyfral() || searchEM_Marine() || searchIbutton()) {           // запускаем поиск cyfral, затем поиск EM_Marine, затем поиск dallas
      //      digitalWrite(G_Led, LOW);
      Sd_ReadOK();
      readflag = true;
      digitalWrite(R_Led, LOW);// clearLed();// digitalWrite(G_Led, HIGH);
    } else {
      delay(100);   //ничего не нашлось - начинаем сначала
      return;
    }
  }
  if (writeflag && readflag) {
    if (keyType == keyEM_Marie) write2rfid();
    else write2iBtn();
  }
  delay(200);
}

//***************** звуки****************
void Sd_ReadOK() {  // звук ОК
  for (int i = 400; i < 6000; i = i * 1.5) {
    tone(speakerPin, i);
    delay(20);
  }
  noTone(speakerPin);
}

void Sd_WriteStep() { // звук "очередной шаг"
  for (int i = 2500; i < 6000; i = i * 1.5) {
    tone(speakerPin, i);
    delay(10);
  }
  noTone(speakerPin);
}

void Sd_ErrorBeep() {  // звук "ERROR"
  for (int j = 0; j < 3; j++) {
    for (int i = 1000; i < 2000; i = i * 1.1) {
      tone(speakerPin, i);
      delay(10);
    }
    delay(50);
    for (int i = 1000; i > 500; i = i * 1.9) {
      tone(speakerPin, i);
      delay(10);
    }
    delay(50);
  }
  noTone(speakerPin);
}

void Sd_StartOK() {  // звук "Успешное включение"
  Serial.println("Hello Sanya");
  delay(150);
  tone(speakerPin, 1318, 300);
  delay(300);
  tone(speakerPin, 1318, 150);
  delay(300);
  tone(speakerPin, 1046, 150);
  delay(150);
  tone(speakerPin, 1318, 300);
  delay(300);
  tone(speakerPin, 1568, 600);
  delay(600);
  tone(speakerPin, 784, 600);
  delay(600);
}
