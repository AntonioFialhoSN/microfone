#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/timer.h"
#include "hardware/adc.h"
#include "hardware/watchdog.h"
#include "neopixel.c"

// Define os pinos e constantes do sistema
#define PINO_MIC 2         // GPIO26 (ADC0) conectado ao microfone (Nota: comentário corrigido - GPIO26 é ADC0)
#define NIVEL_ALERTA 3000  // Limite de volume para disparar o alerta (valores ADC)
#define LED_PIN 7          // Pino de controle da matriz de LEDs NeoPixel
#define NUM_LEDS 25        // Quantidade de LEDs na matriz
#define TEMPO_WATCHDOG 1000 // Timeout do watchdog em ms (1 segundo)

// Variável global para controle de estado (não utilizada no código atual)
volatile bool alerta_sonoro = false;

// Inicializa a matriz de LEDs NeoPixel
void iniciar_matriz_led() {
    npInit(LED_PIN, NUM_LEDS);  // Configura a biblioteca NeoPixel
    npClear();                  // Apaga todos os LEDs
    npWrite();                  // Aplica a mudança
}

// Acende todos os LEDs em vermelho por 500ms (função de alerta)
void acender_leds_vermelhos() {
    npClear();  // Limpa o buffer de LEDs
    
    // Percorre todos os LEDs configurando para vermelho (R=255, G=0, B=0)
    for (int i = 0; i < NUM_LEDS; i++) {
        npSetLED(i, 255, 0, 0);
    }
    
    npWrite();          // Envia os dados para a matriz
    sleep_ms(500);      // Mantém aceso por 500ms
    npClear();          // Apaga os LEDs
    npWrite();          // Aplica o desligamento
}

// Callback do timer periódico para verificação do nível sonoro
bool verificar_som(struct repeating_timer *timer) {
    // Lê o valor atual do ADC conectado ao microfone
    adc_select_input(PINO_MIC);
    uint16_t leitura = adc_read();
    
    // Debug: exibe o valor lido via serial
    printf("Nível sonoro atual: %d\n", leitura);
    
    // Verifica se ultrapassou o limiar de alerta
    if (leitura > NIVEL_ALERTA) {
        printf("Alerta! Som alto detectado: %d\n", leitura);
        acender_leds_vermelhos();  // Dispara o efeito visual de alerta
    }
    
    watchdog_update(); // "Alimenta" o watchdog para evitar reset
    return true;      // Mantém o timer ativo
}

// Configuração inicial do hardware
void configurar_hardware() {
    stdio_init_all();  // Inicializa comunicação serial (USB)
    sleep_ms(1000);    // Delay para estabilização

    // Configura Watchdog - reinicia o sistema se travar
    watchdog_enable(TEMPO_WATCHDOG, 1);

    // Inicializa periféricos
    iniciar_matriz_led();      // Matriz de LEDs
    adc_init();                // Conversor analógico-digital
    adc_gpio_init(26);         // Habilita ADC no pino GPIO26 (Nota: pino físico)
}

// Função principal
int main() {
    configurar_hardware();
    
    // Configura timer periódico para amostragem do microfone (50ms = 20Hz)
    struct repeating_timer amostrador;
    add_repeating_timer_ms(50, verificar_som, NULL, &amostrador);
    
    // Loop principal - mantém o programa em execução
    while (1) {
        tight_loop_contents();       // Otimização de energia
        watchdog_update();            // Alimenta o watchdog periodicamente
    }
}