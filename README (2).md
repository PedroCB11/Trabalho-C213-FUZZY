# Trabalho Final Fuzzy — C213

Controle automático de **luminosidade** de um ambiente usando lógica fuzzy, implementado em um ESP32.

**Integrantes:** Pedro Consoli Bressan e Henrique Fonseca de Castro

## 📋 Descrição

O sistema mantém a luminosidade medida próxima de um *set point* desejado, ajustando automaticamente o brilho de um LED via PWM. A luminosidade é simulada em um sensor LDR e o controlador fuzzy roda diretamente no ESP32, em C/C++.

- **Entradas:** erro (SP − luminosidade) e variação do erro, faixa -100 a 100
- **Saída:** PWM do LED, faixa 0 a 255
- **Fuzzificação:** 5 termos por entrada/saída (Negativo Grande → Positivo Grande / Muito Baixo → Muito Alto), funções triangulares e trapezoidais
- **Base de regras:** matriz 5x5 (erro × Δerro)
- **Defuzzificação:** método do centroide

## 🔧 Hardware

| Componente | Função |
|---|---|
| ESP32 | Controlador principal |
| LDR (pino 34) | Simula a leitura de luminosidade |
| LED PWM (pino 25) | Atuador controlado pelo fuzzy |
| LEDs indicadores (26, 27, 14) | Indicam se está abaixo, OK ou acima do set point |
| Display OLED 128x64 (I2C) | Mostra luminosidade, SP, erro e PWM em tempo real |

## 🎛️ Set points testados

| SP | Valor | Interpretação |
|---|---|---|
| SP1 | 30% | Baixa |
| SP2 | 60% | Média |
| SP3 | 90% | Alta |

A troca de set point é feita via Monitor Serial, enviando `1`, `2` ou `3`.

## 📁 Arquivos

- `codeEsp32.ino` — firmware completo (fuzzificação, base de regras, defuzzificação por centroide, leitura do sensor, controle do LED, display OLED e leitura de comandos via serial)
- `relatorio_fuzzy_luminosidade_final_Pedro_e_Henrique.pdf` — relatório técnico completo, com modelagem, gráficos de pertinência, resultados experimentais e análise para os três set points

## ▶️ Como usar

1. Abra `codeEsp32.ino` na Arduino IDE com a placa ESP32 configurada
2. Monte o circuito (potenciômetro, LED PWM, LEDs indicadores e display OLED conforme os pinos definidos no código)
3. Faça o upload e abra o Monitor Serial (115200 baud)

