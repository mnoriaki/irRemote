// IR reciever / transmitter
//
// 参考にしたもの：
// 赤外線のフォーマット http://elm-chan.org/docs/ir_format.html
// デコードルーチン    http://hello-world.blog.so-net.ne.jp/2011-05-19

const int IR_IN = 2;  // IR Receiver
const int LEDpin = 3; // PD3

inline void pulse() { // 38kHz
  // digitalWrite(LEDpin, HIGH);
  PORTD |= 0x08;
  delayMicroseconds(8);
  // digitalWrite(LEDpin, LOW);
  PORTD &= 0xf7;
  delayMicroseconds(17);
}

inline void pulse40() { // 40kHz
  // digitalWrite(LEDpin, HIGH);
  PORTD |= 0x08;
  delayMicroseconds(8);
  // digitalWrite(LEDpin, LOW);
  PORTD &= 0xf7;
  delayMicroseconds(16);
}

// Function prototypes
void sendAEHA(const byte *p, int len); // string
void sendAEHA(unsigned long d);
void sendAEHA2(const byte c[], int len); // raw
void sendNEC(const byte *p, int len);
void sendNEC(byte c1, byte c2, byte c3);
void sendNEC4(byte c1, byte c2, byte c3, byte c4);
void sendSony(byte dat, uint16_t adr, byte len); // Not tested
void OCR04sendON();
void OCR04sendOFF();

void decode();
void serial();
int serialReadline();
int char2int(int c);
int str2int(const byte *p);
void printAEHA2(byte *p, byte len);

#define NEC    1
#define AEHA   2
#define SONY   3

// Global variables
unsigned int irdata[ 600 ];// ATmega168だと 128くらいにしないと不安定
const int BUF_SIZE = sizeof(irdata) / sizeof(irdata[0]);
byte dat[40];
bool verbose = false;

uint32_t nec_data = 0;
uint16_t sony_data = 0, sony_adrs = 0,  sony_adrs_bit = 0;
byte *ahea_dat;
byte ahea_len = 0;
byte irFormat = 0;
bool irIsValid = false;
unsigned long lastIrMillis = 0;

unsigned long OCR04timer = 0;

const byte on[] = {0x23, 0xCB, 0x26, 0x01, 0x00, 0x20, 0x48, 0x16, // 22.5度 暖房 ON
                   0x00, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x10, 0x06, 0x00, 0x69
                  };
const byte on22[] = {0x23, 0xCB, 0x26, 0x01, 0x00, 0x20, 0x4C, 0x06,
                     0x30, 0xD8, 0x00, 0x00, 0x00, 0x00, 0x10, 0x04, 0x00, 0xA3,
                    }; // 22.0度暖房 ON, 23CB260100204C0630D800000000100400A3
const byte off22[] = {0x23, 0xCB, 0x26, 0x1, 0x0, 0x0, 0x48, 0x6,
                      0x30, 0xF8, 0x0, 0x0, 0x0, 0x0, 0x10, 0x4, 0x0, 0x9F,
                     }; // 22.0度暖房 OFF

// Setup function
void setup() {
  pinMode(LEDpin, OUTPUT);
  /*
    digitalWrite(IR_IN - 1, LOW);
    pinMode(IR_IN - 1, OUTPUT);
    digitalWrite(IR_IN - 2, HIGH);
    pinMode(IR_IN - 2, OUTPUT);
  */
  for (int i = 4; i <= 13; i++) {
    pinMode(i, OUTPUT);
    digitalWrite(i, LOW);
  }
  Serial.begin(115200);
}

// Loop function
void loop() {
  // ● 赤外線を感知するまで待つ
  if ( digitalRead( IR_IN ) == LOW )
    irRecieve();

  if (Serial.available())
    serial();

  if (OCR04timer != 0 && ((millis() - OCR04timer) > 30 * 1000)) {
    OCR04sendOFF();
    OCR04timer = 0;
  }

  if (lastIrMillis - millis() > 150)
    lastIrMillis = 0;
}

void irRecieve() {
  decode();
  if (!irIsValid)
    return;
  switch (irFormat) {
    case 0:
    case NEC:
      Serial.print( F("N "));
      printHexcode(nec_data       % 256);
      printHexcode((nec_data >> 8)  % 256);
      printHexcode((nec_data >> 16) % 256);
      printHexcode((nec_data >> 24) % 256);
      Serial.println();
      if (nec_data == 0xB54AFF00 /* # */) {
        sendAEHA2(off22, sizeof(off22));// 22.0度暖房 OFF
        OCR04timer = millis();
        if (OCR04timer == 0)
          OCR04timer = 1;
      } else if (nec_data == 0xf708FF00 /* 7 */) {
        OCR04sendON();
      } else if (nec_data == 0xbd42FF00 /* * */) {
        OCR04sendOFF();
      } else if (nec_data == 0xe31cFF00 /* 8 */) {
        sendAEHA2(on22, sizeof(on22));// 22.0度暖房 ON
      } else if (nec_data == 0xad52FF00 /* 0 */) {
        sendAEHA2(off22, sizeof(off22));// 22.0度暖房 OFF
      }
      break;
    case AEHA:
      printAEHA2(ahea_dat, ahea_len);
      break;
    case SONY:
      Serial.print(F("S "));
      printHexcode(sony_data);
      printHexcode(sony_adrs_bit);
      printHexcode(sony_adrs);
      Serial.println();
      break;
    default:
      break;
  }
}

void serial() {
  byte c = Serial.read();

  switch (c) {
    case 'A': { // AEHA format
        int l = serialReadline();
        if (l > 0 && (l % 2) == 0)
          sendAEHA(dat, l);
      }
      break;
    case 'H':
      sendAEHA2(off22, sizeof(off22));// 22.0度暖房 OFF
      break;
    case 'h':
      sendAEHA2(on22, sizeof(on22));// 22.0度暖房 ON
      break;
    case 'k':
      sendNEC(0xb8, 0x00, 0x9d); // Kenwood RC-RP0702 power button
      break;
    case 'N': { // NEC format
        int l = serialReadline();
        if (l == 6 || l == 8)
          sendNEC(dat, l);
      }
      break;
    case 'O':
      OCR04sendOFF();
      break;
    case 'o':
      OCR04sendON();
      break;
    case 'P':
      OCR05sendOFF();
      break;
    case 'p':
      OCR05sendON();
      break;
    case 'S': { // Sony format (not tested)
        int l = serialReadline();
        if (l > 0 && l == 8) {
          sendSony(str2int(dat), str2int(dat + 2) << 8 | str2int(dat + 4), str2int(dat + 6));
        }
      }
      break;
    case 'V':
      verbose = false;
      break;
    case 'v':
      verbose = true;
      break;
    default:
      break;
  }
}

int serialReadline() {
  unsigned long st = millis();
  byte *p = dat;
  int len = 0;

  do {
    if (Serial.available()) {
      *p = Serial.read();
      if (*p == '\r' || *p == '\n') {
        *p = 0;
        break;
      }
      p ++;
      len ++;
      *p = 0;
    }
  } while (millis() - st < 1000 && len < sizeof(dat));

  return len;
}

int char2int(int c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 0xa;
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 0xa;

  return -1;
}

int str2int(const byte *p) {
  int c = char2int(*p);
  if (c < 0)
    return -1;

  int c2 = char2int(*(p + 1));
  if (c2 < 0)
    return -1;
  return (c << 4) | c2;
}

void waitIr() {
  if (lastIrMillis == 0)
    return;

  while (lastIrMillis - millis() < 150)
    delay(10);
  lastIrMillis = 0;
}

//---------------------------------------------------------------------//
//                   Decode                                            //
// http://hello-world.blog.so-net.ne.jp/2011-05-19                     //
//---------------------------------------------------------------------//
void printlnHexcode(unsigned int d);
void printAEHA(byte *p, byte len);

void decode() {
  unsigned long usec;
  unsigned int irOffTime, minTime, aveCnt = 0, aveAdd = 0;
  unsigned int timeunit, leaderH, leaderL, datalen;
  unsigned int hex = 0;

  irFormat = 0;
  irIsValid = true;
  nec_data = 0;
  sony_data = sony_adrs = sony_adrs_bit = 0;
  ahea_dat = NULL;
  ahea_len = 0;

  // ● 生データの取得
  for (int i = 0; i < BUF_SIZE; ) {
    usec = micros();
    while ( digitalRead( IR_IN ) == LOW );          // IR信号がONの時間を測定
    irdata[  i] = micros() - usec;
    irdata[++i] = 0;
    usec = micros();
    while ( digitalRead( IR_IN ) == HIGH ) {        // IR信号がOFFの時間を測定
      irOffTime = micros() - usec;
      if ( irOffTime > 65000 )
        goto ir_exit;        // 信号途絶なら終了
    }
    irdata[i++] = irOffTime;
  }

ir_exit:
  // ● 基準時間を決める
  // まず最小値を確認
  minTime = irdata[0];
  for (int i = 0; irdata[i]; i++)
    minTime = min( minTime, irdata[i]);

  // 最小値の150%未満の時間幅の平均値を基準時間にする
  for (int i = 0; irdata[i] != 0; i++) {
    if ( minTime * 3 / 2 > irdata[i] ) {
      aveAdd += irdata[i] - minTime;
      aveCnt++;
    }
  }
  timeunit = aveAdd / aveCnt + minTime;

  // 基準時間が短すぎるときは異常と判断
  if ( timeunit < 300 )
    return;

  // ● 基準時間の整数倍に変換
  for (int i = 0; irdata[i]; i++)
    irdata[i] = ( irdata[i] + timeunit / 2 ) / timeunit;

  if (verbose) {
    // ● 赤外線の波形を表示
    Serial.println( F("<< IR data analyser >>") );
    for (int i = 0; irdata[i] != 0; i++)
      for (int n = 0; n < irdata[i]; n++)
        Serial.print( (i % 2) ? "_" : "|" );
    Serial.println( F(".") );
  }

  // ● 解析しやすくするためにデータを整理
  leaderH = irdata[0];                // リーダ部Highの長さ
  leaderL = irdata[1];                // リーダ部Low の長さ
  unsigned int *data = irdata + 2;    // データ部分
  for (int i = 1; ; i += 2) {             // リーダ部とリピートを除去・整理
    if ( data[i] > 10 || data[i] == 0 ) {
      data[i] = 0;
      datalen = (i + 1) / 2;
      break;
    }
  }

  // ● リーダ部の長さでフォーマットを判断
  irFormat = 0;
  if ((leaderH > 14 || leaderH < 18) && leaderL == 8 ) {
    irFormat = NEC;
  }
  else if           ( leaderH == 8  && leaderL == 4 ) {
    irFormat = AEHA;
  }
  else if           ( leaderH == 4  && leaderL == 1 ) {
    irFormat = SONY;
  }

  if (verbose) {
    // ● 各種基本データを表示
    Serial.print( F("Time Unit (usec) : "));
    Serial.println( timeunit , DEC );

    Serial.print( F("Leader  (On/Off) : "));
    Serial.print( leaderH, DEC );
    Serial.print(  F(" / " ));
    Serial.println( leaderL, DEC );

    Serial.print( F("Format           : "));
  }
  // ● フォーマット毎に結果を表示
  switch ( irFormat ) {
    case 0:        // ■ 不明な形式の場合
      if (verbose) {
        Serial.println( F("???") );
        Serial.println( F(" !! Analyzed as NEC format !!"));
      }
    case NEC:      // ■ NECフォーマット
      if (verbose) {
        Serial.println( F("NEC")  );
        Serial.print( F("Binary data      : (LSB)"));
      }
      for (int n = 0; n < datalen - 1; n++) { // ストップビット手前まで繰り返し
        if (verbose && n % 8 == 0)
          Serial.print(F(" "));
        if ( data[n * 2]   != 1) {
          if (verbose)
            Serial.print(F("?"));
          irIsValid = false;
          continue;
        }
        if ( data[n * 2 + 1] != 1 && data[n * 2 + 1] != 3) {
          if (verbose)
            Serial.print(F("?"));
          irIsValid = false;
          continue;
        }
        if (verbose)
          Serial.print((data[n * 2 + 1] == 1) ? "0" : "1" );
        nec_data |= ( (data[n * 2 + 1] == 1) ? 0UL : 1UL ) << n;
      }
      if (verbose)
        Serial.println(F(" (MSB)"));
      if ( !irIsValid )
        break;   // データに不具合があれば以降は表示しない
      if (verbose) {
        Serial.print( F("Custom code      : "));  printlnHexcode(  nec_data        % 256 );
        Serial.print( F("Custom code '    : "));  printlnHexcode( (nec_data >>  8) % 256 );
        Serial.print( F("Data code        : "));  printlnHexcode( (nec_data >> 16) % 256 );
        Serial.print( F("Data code (nega) : "));  printlnHexcode( (nec_data >> 24) % 256 );
      }
      break;
    case AEHA:     // ■ 家電協フォーマット
      if (verbose) {
        Serial.println( F("AEHA" ));
        Serial.print( F("Binary data      : (LSB)"));
      }
      for (int n = 0; n < datalen - 1; n++) { // ストップビット手前まで繰り返し
        if (verbose && n % 4 == 0)
          Serial.print(F(" "));
        if ( data[n * 2]   != 1) {
          if (verbose)
            Serial.print(F("?"));
          irIsValid = false;
          continue;
        }
        if ( data[n * 2 + 1] != 1 && data[n * 2 + 1] != 3) {
          if (verbose)
            Serial.print(F("?"));
          irIsValid = false;
          continue;
        }
        if (verbose)
          Serial.print((data[n * 2 + 1] == 1) ? "0" : "1");
      }
      if (verbose)
        Serial.println(F(" (MSB)"));
      if ( !irIsValid )
        break;   // データに不具合があれば以降は表示しない
      if (verbose)
        Serial.print( F("Hexadecimal data : (LSB) "));
      {
        byte *p = dat;
        ahea_dat = dat;
        ahea_len = 0;
        for (int n = 0; n < datalen; n++) {
          hex |= ((data[n * 2 + 1] == 1) ? 0 : 1) << (n % 4);
          if ( n % 4 == 3) {
            *(p++) = hex;
            ahea_len ++;
            if (verbose) {
              Serial.print(hex, HEX);
              Serial.print(F("    "));
            }
            hex = 0;
          }
        }
      }
      break;
    case SONY:     // ■ SONYフォーマット
      if (verbose) {
        Serial.println( F("SONY" ));
        Serial.print( F("Binary data      : (LSB) "));
      }
      for (int n = 0; n < datalen; n++) {
        if ( data[n * 2 + 1] != 0 && data[n * 2 + 1] != 1) {
          if (verbose)
            Serial.print(F("?"));
          irIsValid = false;
          continue;
        }
        if ( data[n * 2]   != 1 && data[n * 2]   != 2) {
          if (verbose)
            Serial.print(F("?"));
          irIsValid = false;
          continue;
        }
        if (verbose) {
          Serial.print( data[n * 2] - 1 );
          if ( n == 6 )
            Serial.print(F(" "));
        }
        if ( n <  7 )
          sony_data |= (data[n * 2] - 1) << n;
        else
          sony_adrs |= (data[n * 2] - 1) << sony_adrs_bit++;
      }
      if (verbose)
        Serial.println(" (MSB)");
      if ( !irIsValid )   // データに不具合があれば以降は表示しない
        break;
      if (verbose) {
        Serial.print( F("Data (7bit)      : "));
        printlnHexcode( sony_data );

        Serial.print( F("Adress bits (bit): "));
        Serial.print( sony_adrs_bit );
        Serial.println( F(" bit") );

        Serial.print( F("Adress           : "));
        printlnHexcode( sony_adrs );
      }
      break;
  }
  // ● 以上で終了
  if (verbose)
    Serial.println();
}

void printlnHexcode(unsigned int d) {    // 16進数を見栄えよく表示、改行
  Serial.print( ( d < 0x10 ) ? "0x0" : "0x" );
  Serial.print( ( d < 0x1000 && d > 0xff ) ? "0" : "" );
  Serial.println( d, HEX);
}

void printHexcode(unsigned int d) {    // 16進数を見栄えよく表示
  if (d < 0x10)
    Serial.print(0);
  Serial.print( ( d < 0x1000 && d > 0xff ) ? "0" : "" );
  Serial.print( d, HEX);
}

void printAEHA(byte *p, byte len) {
  Serial.print(F("0x"));
  Serial.print(p[1] << 4 | p[0], HEX);
  Serial.print(F(", 0x"));
  Serial.print(p[3] << 4 | p[2], HEX);
  Serial.print(F(", 0x"));
  Serial.print(p[4], HEX);
  Serial.print(F(", 0x"));
  Serial.print(p[5], HEX);
  Serial.print(F(", 0x"));

  for (int i = 6; i < len; i += 2) {
    Serial.print(p[i + 1] << 4 | p[i], HEX);
    Serial.print(F(", 0x"));
  }
}

void printAEHA2(byte *p, byte len) {
  Serial.print(F("A "));
  for (int i = 0; i < len; i += 2) {
    if (p[i + 1] == 0)
      Serial.print(0);
    Serial.print(p[i + 1] << 4 | p[i], HEX);
  }
  Serial.println();
}


//---------------------------------------------------------------------//
//                   NEC                                               //
//---------------------------------------------------------------------//
void NECLeader();
void NEC0();
void NEC1();
void NECByte(byte c);

const int NEC_T = 562;
const int NEC_N = 562 / (1000 / 38) + 1;

void sendNEC(const byte *p, int len) {
  byte c1 = str2int(p);
  p += 2;
  byte c2 = str2int(p);
  p += 2;
  byte c3 = str2int(p);

  if (len == 6) {
    sendNEC(c1, c2, c3);
    return;
  }
  p += 2;
  byte c4 = str2int(p);
  sendNEC4(c1, c2, c3, c4);
}

void sendNEC(byte c1, byte c2, byte c3) {
  sendNEC4(c1, c2, c3, ~c3);
}

void sendNEC4(byte c1, byte c2, byte c3, byte c4) {
  waitIr();

  NECLeader();
  NECByte(c1);
  NECByte(c2);
  NECByte(c3);
  NECByte(c4);
  NEC0();

  lastIrMillis = millis();
}

void NECLeader() {
  for (int i = NEC_N * 16; i > 0; i--) {
    pulse();
  }
  for (int i = 8; i > 0; i--) {
    delayMicroseconds(NEC_T);
  }
}

void NEC0() {
  for (int i = NEC_N; i > 0; i--) {
    pulse();
  }
  delayMicroseconds(NEC_T);
}

void NEC1() {
  for (int i = NEC_N; i > 0; i--) {
    pulse();
  }
  for (int i = 3; i > 0; i--)
    delayMicroseconds(NEC_T);
}

void NECByte(byte c) {
  for (int i = 8; i > 0; i--) {
    if (c & 1) {
      NEC1();
    } else {
      NEC0();
    }
    c >>= 1;
  }
}

//---------------------------------------------------------------------//
//                   AEHA                                              //
//---------------------------------------------------------------------//
void AEHALeader();
void AEHAsend0();
void AEHAsend1();
void AEHAsendByte(byte c);

const int T = 425 / (1000 / 38);

void sendAEHA(const byte *p, int len) {
  byte send[20];
  byte *q = send;
  len /= 2;

  for (int i = 0; i < len; i ++) {
    *q = str2int(p);
    q ++; p += 2;
  }
  sendAEHA2(send, len);
}

void sendAEHA2(const byte c[], int len) {
  waitIr();

  AEHALeader();
  for (int i = 0; i < len; i++) {
    AEHAsendByte(c[i]);
  }
  AEHAsend1();

  lastIrMillis = millis();
}

void sendAEHA(unsigned long d) {
  waitIr();

  AEHALeader();
  AEHAsendByte((d >> 24) & 0xff);
  AEHAsendByte((d >> 16) & 0xff);
  AEHAsendByte((d >> 8) & 0xff );
  AEHAsendByte(d & 0xff);
  AEHAsend1();

  lastIrMillis = millis();
}

void AEHALeader() {
  for (int i = 8 * T; i > 0; i--) {
    pulse();
  }
  for (int i = 4; i > 0; i--) {
    delayMicroseconds(425);
  }
}

void AEHAsend0() {
  for (int i = T; i > 0; i--) {
    pulse();
  }
  delayMicroseconds(425);
}

void AEHAsend1() {
  for (int i = T; i > 0; i--) {
    pulse();
  }
  for (int i = 3; i > 0; i--)
    delayMicroseconds(425);
}

void AEHAsendByte(byte c) {
  for (int i = 8; i > 0; i--) {
    if (c & 1) {
      AEHAsend1();
    } else {
      AEHAsend0();
    }
    c >>= 1;
  }
}

//---------------------------------------------------------------------//
//                   Sony                                               //
//---------------------------------------------------------------------//
const int sonyT = 600;
const int sonyN = sonyT / (1 / 40 * 1000);

inline void sonyLeader() {
  for (int i = sonyN * 4; i > 0; i--) {
    pulse40();
  }
}

inline void sony0() {
  delayMicroseconds(sonyT);
  for (int i = sonyN; i > 0; i--)
    pulse40();
}

inline void sony1() {
  delayMicroseconds(sonyT);
  for (int i = sonyN * 2; i > 0; i--)
    pulse40();
}

inline void sonyBits(uint16_t dat, byte len) {
  for (int i = len; i > 0; i --) {
    if (dat & 1)
      sony0();
    else
      sony1();
    dat = (dat >> 1);
  }
}

void sendSony(byte dat, uint16_t adr, byte len) { // len should be 5, 8 or 13
  waitIr();

  sonyLeader();
  sonyBits(dat, 7);
  sonyBits(adr, len);

  lastIrMillis = millis();
}

//---------------------------------------------------------------------//
//                   Ohm                                               //
//---------------------------------------------------------------------//
inline void OhmPulse() { // 42kHz
  digitalWrite(LEDpin, HIGH);
  delayMicroseconds(7);
  delayMicroseconds(2);
  digitalWrite(LEDpin, LOW);
  delayMicroseconds(7);
  delayMicroseconds(2);
}

inline void OCR04sendHL() {
  for (int i = 0; i < 32  /* 30*/; i++) { // 2ms
    OhmPulse();
  }
  delayMicroseconds(858);
}

inline void OCR04sendLH() {
  delayMicroseconds(858);
  for (int i = 0; i < 32 /* 30 */; i++) { // 2ms
    OhmPulse();
  }
}

void OCR04sendON() {
  waitIr();

  for (int i = 0; i < 97 /* 92*/ ; i++) { // 2ms
    OhmPulse();
  }
  delayMicroseconds(2610);
  OCR04sendHL();
  OCR04sendHL();
  OCR04sendHL();
  OCR04sendHL();
  OCR04sendLH();
  delayMicroseconds(1889);
  OCR04sendHL();
  OCR04sendLH();
  delayMicroseconds(1889);
  OCR04sendHL();
  OCR04sendHL();

  lastIrMillis = millis();
}


void OCR04sendOFF() {
  waitIr();

  for (int i = 0; i < 97; i++) { // 2ms
    OhmPulse();
  }
  delayMicroseconds(2610);
  OCR04sendHL();
  OCR04sendHL();
  OCR04sendHL();
  OCR04sendLH();
  OCR04sendLH();
  delayMicroseconds(1889);
  OCR04sendHL();
  OCR04sendLH();
  OCR04sendLH();
  delayMicroseconds(1889);
  OCR04sendHL();

  lastIrMillis = millis();
}


void OCR05sendON() {
  waitIr();

  for (int i = 0; i < 97 /* 92*/ ; i++) { // 2ms
    OhmPulse();
  }
  delayMicroseconds(2610);
  OCR04sendHL();
  OCR04sendHL();
  OCR04sendHL();
  OCR04sendLH();
  delayMicroseconds(1889);
  OCR04sendHL();
  OCR04sendLH();
  delayMicroseconds(1889);
  OCR04sendHL();
  OCR04sendHL();
  OCR04sendHL();

  lastIrMillis = millis();
}

void OCR05sendOFF() {
  waitIr();

  for (int i = 0; i < 97 /* 92*/ ; i++) { // 2ms
    OhmPulse();
  }
  delayMicroseconds(2610);
  OCR04sendHL();
  OCR04sendHL();
  OCR04sendLH();
  OCR04sendLH();
  OCR04sendLH();
  delayMicroseconds(1889);
  OCR04sendHL();
  OCR04sendHL();
  OCR04sendLH();
  delayMicroseconds(1889);
  OCR04sendHL();

  lastIrMillis = millis();
}

