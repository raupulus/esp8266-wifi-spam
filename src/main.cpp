#include <Arduino.h>

int it = 0;

void setup()
{
  Serial.begin(9600);
}

void loop()
{
  // put your main code here, to run repeatedly:

  it++;

  Serial.print("Iteration");
  Serial.println(it);

  delay(500);
}
