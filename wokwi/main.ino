#include <Wire.h>
#include <Adafruit_BMP085.h>
#include "DHT.h"
#include <LiquidCrystal_I2C.h>

#define I2C_ADDR    0x27
#define LCD_COLUMNS 20
#define LCD_LINES   4
LiquidCrystal_I2C lcd(I2C_ADDR, LCD_COLUMNS, LCD_LINES);

#define DHTPIN 2     // Pin donde está conectado el sensor
#define DHTTYPE DHT22   // Sensor DHT22
DHT dht(DHTPIN, DHTTYPE);

Adafruit_BMP085 bmp;

#define PIN_TRIG 3
#define PIN_ECHO 4
#define multDist 0.017132877
int altura = 500;
  
void setup() {
  Serial.begin(9600);

  if (!bmp.begin()) {
    Serial.println("Could not find a valid BMP085 sensor, check wiring!");
    while (1) {}
  }

  dht.begin();

  lcd.init();
  lcd.backlight();
  // you can now interact with the LCD, e.g.:
  lcd.print("Iniciando...");
  delay(1500);

  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
}
  
void loop() {
    String temperature = "Temp = ";
    float temp = bmp.readTemperature();
    temperature += temp;
    lcd.clear();
    lcd.print(temperature);
    lcd.write(223);
    lcd.print("C");
    
    String pressure = "Pre = ";
    pressure += bmp.readPressure();
    pressure += " Pa";
    lcd.setCursor(0, 1);
    lcd.print(pressure);

    String humidity = "Hum = ";
    humidity += dht.readHumidity();
    humidity += "%";
    lcd.setCursor(0, 2);
    lcd.print(humidity);

    // Start a new measurement:
    digitalWrite(PIN_TRIG, LOW);  //para generar un pulso limpio ponemos a LOW 4us
    delayMicroseconds(4);
    digitalWrite(PIN_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_TRIG, LOW);

    // Read the result:
    int duration = pulseIn(PIN_ECHO, HIGH);
    lcd.setCursor(0, 3);
    lcd.print("Nivel = ");
    lcd.print((altura - (duration * multDist)));
    lcd.print(" cm");

    int buzzed = 0;
    if((altura - duration * multDist) <= 100.0){
      tone(8, 580, 1000);
    }else if(temp >= 35.0){
      tone(8, 440, 500);
      delay(600);
      tone(8, 440, 500);
      buzzed = 600;
    }

    delay(2500-buzzed);

    // These constants should match the photoresistor's "gamma" and "rl10" attributes
    const float GAMMA = 0.7;
    const float RL10 = 50;

    // Convert the analog value into lux value:
    int analogValue = analogRead(A0);
    float voltage = analogValue / 1024. * 5;
    float resistance = 2000 * voltage / (1 - voltage / 5);
    float lux = pow(RL10 * 1e3 * pow(10, GAMMA) / resistance, (1 / GAMMA));
    lcd.clear();
    lcd.print("Lux = ");
    lcd.print(lux);
    lcd.print(" lux");

    delay(2500);

    
    /*float h = dht.readHumidity(); //Leemos la Humedad
    float t = dht.readTemperature(); //Leemos la temperatura en grados Celsius
    float f = dht.readTemperature(true); //Leemos la temperatura en grados Fahrenheit
    //--------Enviamos las lecturas por el puerto serial-------------
    Serial.print("Humedad ");
    Serial.print(h);
    Serial.print(" %t");
    Serial.print("Temperatura: ");
    Serial.print(t);
    Serial.print(" *C ");
    Serial.print(f);
    Serial.println(" *F");*/
}
