int pwm_up = 13;
int pwm_down = 12;

void setup() {
  pinMode(pwm_up, OUTPUT);
  pinMode(pwm_down, OUTPUT);
  digitalWrite(pwm_up, LOW);
  digitalWrite(pwm_down, LOW);
  
  Serial.begin(9600);
}

void loop() {
  while (Serial.available()) {
    char in_byte = Serial.read();
    int pin = -1;
    if (in_byte == 'U') {
      pin = pwm_up;
    } else if (in_byte == 'D') {
      pin = pwm_down;
    }
    if (pin != -1) {
      digitalWrite(pin, HIGH);
      delay(100);
      digitalWrite(pin, LOW);
    }
    Serial.print(in_byte);
  }
}

