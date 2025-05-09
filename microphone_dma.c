#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/timer.h"
#include "hardware/watchdog.h"
#include "neopixel.c"  // Biblioteca externa para controle dos LEDs NeoPixel

// Definições de pinos e parâmetros
#define MIC_ADC_CHANNEL     2      // Canal ADC usado pelo microfone (GPIO 28)
#define MIC_GPIO_PIN        28     // GPIO correspondente ao canal ADC 2
#define SAMPLE_COUNT        200    // Número de amostras por captura
#define NOISE_THRESHOLD     800    // Limite de volume para detectar ruído

#define LED_GPIO_PIN        7      // Pino conectado aos LEDs NeoPixel
#define LED_TOTAL_COUNT     25     // Total de LEDs a serem controlados

// Buffer para armazenar as amostras de áudio
uint16_t audio_samples[SAMPLE_COUNT];

// Configurações do canal DMA
int dma_audio_channel;
dma_channel_config dma_audio_config;

// Timer para captura repetitiva de áudio
struct repeating_timer sample_timer;

// Flags de estado
bool is_noise_detected = false;    // Indica se o ruído foi detectado
bool should_update_leds = false;  // Indica se os LEDs precisam ser atualizados

// Captura `SAMPLE_COUNT` amostras de áudio usando DMA
void capture_audio_samples() {
    adc_fifo_drain();       // Limpa FIFO
    adc_run(false);         // Garante que o ADC esteja desligado

    // Configura o DMA para transferir dados do ADC FIFO para o buffer
    dma_channel_configure(
        dma_audio_channel, &dma_audio_config,
        audio_samples,
        &adc_hw->fifo,
        SAMPLE_COUNT,
        true  // Inicia imediatamente
    );

    adc_run(true);  // Inicia conversão ADC
    dma_channel_wait_for_finish_blocking(dma_audio_channel);  // Espera DMA terminar
    adc_run(false); // Para o ADC
}

// Calcula o volume médio com base nas amostras capturadas
uint16_t calculate_average_volume() {
    uint32_t total = 0;
    for (int i = 0; i < SAMPLE_COUNT; i++) {
        total += audio_samples[i];
    }
    return total / SAMPLE_COUNT;
}


// Função chamada periodicamente pelo timer
bool sampling_timer_callback(struct repeating_timer *t) {
    capture_audio_samples();  // Captura novas amostras de áudio via DMA
    uint16_t current_volume = calculate_average_volume();  // Calcula o volume médio

    printf("Volume: %u\n", current_volume);  // Mostra o volume no terminal

    // Verifica se houve mudança no estado do ruído
    bool noise_now = (current_volume > NOISE_THRESHOLD);
    if (noise_now != is_noise_detected) {
        is_noise_detected = noise_now;
        should_update_leds = true;  // Sinaliza que os LEDs devem ser atualizados
    }

    return true;  // Mantém o timer ativo
}

// Acende todos os LEDs na cor vermelha
void light_up_all_red() {
    npClear();
    for (int i = 0; i < LED_TOTAL_COUNT; i++) {
        npSetLED(i, 255, 0, 0);  // RGB: vermelho máximo
    }
    npWrite();
}

// Desliga todos os LEDs
void turn_off_all_leds() {
    npClear();
    npWrite();
}

// Função principal
int main() {
    stdio_usb_init();  // Inicializa comunicação USB (para printf)
    sleep_ms(1500);    // Aguarda estabilização da conexão USB

    watchdog_enable(3000, 1);  // Ativa watchdog de 3 segundos

    // Inicializa LEDs NeoPixel
    npInit(LED_GPIO_PIN, LED_TOTAL_COUNT);
    turn_off_all_leds();

    // Configura ADC
    adc_gpio_init(MIC_GPIO_PIN);  // Ativa função ADC no pino GPIO 28
    adc_init();                   // Inicializa periférico ADC
    adc_select_input(MIC_ADC_CHANNEL);  // Seleciona canal ADC 2
    adc_set_clkdiv(96.f);              // Ajusta clock do ADC
    adc_fifo_setup(true, true, 1, false, false);  // Configura FIFO do ADC

    // Configura DMA para leitura do ADC
    dma_audio_channel = dma_claim_unused_channel(true);
    dma_audio_config = dma_channel_get_default_config(dma_audio_channel);
    channel_config_set_transfer_data_size(&dma_audio_config, DMA_SIZE_16);
    channel_config_set_read_increment(&dma_audio_config, false);
    channel_config_set_write_increment(&dma_audio_config, true);
    channel_config_set_dreq(&dma_audio_config, DREQ_ADC);  // Gatilho do ADC

    // Cria um timer que dispara a cada 150 ms
    add_repeating_timer_ms(150, sampling_timer_callback, NULL, &sample_timer);

    // Loop principal
    while (true) {
        watchdog_update();  // Alimenta o watchdog

        // Atualiza LEDs conforme detecção de ruído
        if (should_update_leds) {
            if (is_noise_detected)
                light_up_all_red();
            else
                turn_off_all_leds();
            should_update_leds = false;
        }

        sleep_ms(10);  // Pequeno atraso para evitar uso excessivo da CPU
    }
}
