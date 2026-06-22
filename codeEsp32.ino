#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

// =========================
// PINOS DO PROJETO
// =========================
#define POT_PIN       34   // Potenciômetro simulando luminosidade
#define LED_PWM_PIN   25   // LED principal controlado por PWM

#define LED_ABAIXO    26   // Luminosidade abaixo do set point
#define LED_OK        27   // Luminosidade próxima do set point
#define LED_ACIMA     14   // Luminosidade acima do set point

#define SDA_PIN       21
#define SCL_PIN       22

// =========================
// CONFIGURACAO PWM ESP32
// =========================
const int canalPWM = 0;
const int freqPWM = 5000;
const int resolucaoPWM = 8; // 8 bits: 0 a 255


// =========================
// DISPLAY OLED 128x64 I2C
// =========================
// Primeiro tente este modelo:
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// Se o display não mostrar nada, comente a linha acima e use esta:
// U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// =========================
// VARIÁVEIS DE CONTROLE
// =========================
float setPoint = 60.0;     // SP inicial: 60%
float erroAnterior = 0.0;

unsigned long tempoAnterior = 0;
const unsigned long intervalo = 300; // ms

// =========================
// FUNÇÕES DE PERTINÊNCIA
// =========================
float trimf(float x, float a, float b, float c) {
  if (x <= a || x >= c) return 0.0;
  if (x == b) return 1.0;
  if (x < b) return (x - a) / (b - a);
  return (c - x) / (c - b);
}

float trapmf(float x, float a, float b, float c, float d) {
  if (x <= a || x >= d) return 0.0;
  if (x >= b && x <= c) return 1.0;
  if (x > a && x < b) return (x - a) / (b - a);
  return (d - x) / (d - c);
}

// Entrada: erro e variação do erro, faixa -100 a 100
float pertinenciaEntrada(int termo, float x) {
  switch (termo) {
    case 0: return trapmf(x, -100, -100, -70, -35); // Negativo Grande
    case 1: return trimf(x, -70, -35, 0);           // Negativo Pequeno
    case 2: return trimf(x, -15, 0, 15);            // Zero
    case 3: return trimf(x, 0, 35, 70);             // Positivo Pequeno
    case 4: return trapmf(x, 35, 70, 100, 100);     // Positivo Grande
  }
  return 0.0;
}

// Saída: PWM do LED, faixa 0 a 255
float pertinenciaSaida(int termo, float x) {
  switch (termo) {
    case 0: return trapmf(x, 0, 0, 20, 60);          // Muito Baixo
    case 1: return trimf(x, 30, 80, 130);            // Baixo
    case 2: return trimf(x, 100, 150, 200);          // Médio
    case 3: return trimf(x, 170, 210, 245);          // Alto
    case 4: return trapmf(x, 220, 245, 255, 255);    // Muito Alto
  }
  return 0.0;
}

// =========================
// CONTROLADOR FUZZY
// =========================
int controladorFuzzy(float erro, float deltaErro) {
  /*
    Termos:
    0 = Negativo Grande / Muito Baixo
    1 = Negativo Pequeno / Baixo
    2 = Zero / Médio
    3 = Positivo Pequeno / Alto
    4 = Positivo Grande / Muito Alto

    Interpretação:
    erro positivo  -> ambiente abaixo do desejado -> aumentar LED
    erro negativo  -> ambiente acima do desejado  -> reduzir LED
  */

  int regras[5][5] = {
    // deltaErro: NG, NP, Z,  PP, PG
    {0, 0, 0, 1, 1}, // erro NG
    {0, 1, 1, 2, 2}, // erro NP
    {1, 1, 2, 2, 3}, // erro Z
    {2, 2, 3, 3, 4}, // erro PP
    {3, 3, 4, 4, 4}  // erro PG
  };

  float numerador = 0.0;
  float denominador = 0.0;

  // Defuzzificação pelo centroide
  for (int pwm = 0; pwm <= 255; pwm++) {
    float agregacao = 0.0;

    for (int i = 0; i < 5; i++) {
      float muErro = pertinenciaEntrada(i, erro);

      for (int j = 0; j < 5; j++) {
        float muDelta = pertinenciaEntrada(j, deltaErro);
        float ativacaoRegra = min(muErro, muDelta);

        int termoSaida = regras[i][j];
        float muSaida = pertinenciaSaida(termoSaida, pwm);

        float valorRegra = min(ativacaoRegra, muSaida);
        agregacao = max(agregacao, valorRegra);
      }
    }

    numerador += pwm * agregacao;
    denominador += agregacao;
  }

  if (denominador == 0) return 0;

  int saidaPWM = numerador / denominador;
  return constrain(saidaPWM, 0, 255);
}

// =========================
// LEITURA DO POTENCIÔMETRO
// =========================
float lerLuminosidade() {
  int leituraADC = analogRead(POT_PIN);

  // Conversao simples: 0 a 4095 para 0% a 100%
  float luminosidade = (leituraADC / 4095.0) * 100.0;

  // Se a leitura ficar invertida, descomente a linha abaixo:
  // luminosidade = 100.0 - luminosidade;

  return luminosidade;
}

// =========================
// ATUALIZA OS LEDS INDICADORES
// =========================
void atualizarIndicadores(float erro) {
  digitalWrite(LED_ABAIXO, LOW);
  digitalWrite(LED_OK, LOW);
  digitalWrite(LED_ACIMA, LOW);

  if (erro > 5) {
    digitalWrite(LED_ABAIXO, HIGH);
  } else if (erro < -5) {
    digitalWrite(LED_ACIMA, HIGH);
  } else {
    digitalWrite(LED_OK, HIGH);
  }
}

// =========================
// ATUALIZA DISPLAY
// =========================
void atualizarDisplay(float luminosidade, float erro, float deltaErro, int pwm) {
  char linha[32];

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);

  u8g2.drawStr(0, 10, "Controle Luz Fuzzy");

  snprintf(linha, sizeof(linha), "Lum: %.1f %%", luminosidade);
  u8g2.drawStr(0, 25, linha);

  snprintf(linha, sizeof(linha), "SP:  %.1f %%", setPoint);
  u8g2.drawStr(0, 37, linha);

  snprintf(linha, sizeof(linha), "Erro: %.1f", erro);
  u8g2.drawStr(0, 49, linha);

  snprintf(linha, sizeof(linha), "PWM: %d", pwm);
  u8g2.drawStr(0, 61, linha);

  u8g2.sendBuffer();
}
// =========================
// LER COMANDO SERIAL
// =========================

void lerComandoSerial() {
  if (Serial.available() > 0) {
    char comando = Serial.read();

    if (comando == '1') {
      setPoint = 30.0;
      Serial.println("Set Point alterado para SP1 = 30%");
    } 
    else if (comando == '2') {
      setPoint = 60.0;
      Serial.println("Set Point alterado para SP2 = 60%");
    } 
    else if (comando == '3') {
      setPoint = 90.0;
      Serial.println("Set Point alterado para SP3 = 90%");
    }
  }
}

// =========================
// SETUP
// =========================
void setup() {
  Serial.begin(115200);

  pinMode(POT_PIN, INPUT);

  pinMode(LED_PWM_PIN, OUTPUT);
  ledcSetup(canalPWM, freqPWM, resolucaoPWM);
  ledcAttachPin(LED_PWM_PIN, canalPWM);
  
  pinMode(LED_ABAIXO, OUTPUT);
  pinMode(LED_OK, OUTPUT);
  pinMode(LED_ACIMA, OUTPUT);

  

  analogReadResolution(12);
  analogSetPinAttenuation(POT_PIN, ADC_11db);

  Wire.begin(SDA_PIN, SCL_PIN);
  u8g2.begin();

  Serial.println("Sistema iniciado.");
  Serial.println("Girando o potenciometro, a luminosidade simulada muda.");
}

// =========================
// LOOP
// =========================
void loop() {
  lerComandoSerial();
  
  unsigned long tempoAtual = millis();

  if (tempoAtual - tempoAnterior >= intervalo) {
    tempoAnterior = tempoAtual;

    float luminosidade = lerLuminosidade();

    float erro = setPoint - luminosidade;
    float deltaErro = erro - erroAnterior;
    erroAnterior = erro;

    int saidaPWM = controladorFuzzy(erro, deltaErro);

    ledcWrite(canalPWM, saidaPWM);

    atualizarIndicadores(erro);
    atualizarDisplay(luminosidade, erro, deltaErro, saidaPWM);

    Serial.print("Luminosidade: ");
    Serial.print(luminosidade);
    Serial.print("% | SP: ");
    Serial.print(setPoint);
    Serial.print("% | Erro: ");
    Serial.print(erro);
    Serial.print(" | DeltaErro: ");
    Serial.print(deltaErro);
    Serial.print(" | PWM: ");
    Serial.println(saidaPWM);
  }
}
