#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "inc/ssd1306.h"
#include "ws2818b.pio.h"

// Definições para o display OLED
const uint I2C_SDA = 14;
const uint I2C_SCL = 15;

// Definições para a matriz de LEDs
#define LED_COUNT 25
#define LED_PIN 7
#define NUM_LEDS 25
#define BRIGHTNESS 255
#define BUTTON_A_PIN 5
#define BUTTON_B_PIN 6

// Definição de pixel GRB
struct pixel_t {
    uint8_t G, R, B;
};
typedef struct pixel_t pixel_t;
typedef pixel_t npLED_t;

// Declaração do buffer de pixels que formam a matriz.
npLED_t leds[LED_COUNT];

// Variáveis para uso da máquina PIO.
PIO np_pio;
uint sm;

/**
 * Inicializa a máquina PIO para controle da matriz de LEDs.
 */
void npInit(uint pin) {
    // Cria programa PIO.
    uint offset = pio_add_program(pio0, &ws2818b_program);
    np_pio = pio0;

    // Toma posse de uma máquina PIO.
    sm = pio_claim_unused_sm(np_pio, false);
    if (sm < 0) {
        np_pio = pio1;
        sm = pio_claim_unused_sm(np_pio, true); // Se nenhuma máquina estiver livre, panic!
    }

    // Inicia programa na máquina PIO obtida.
    ws2818b_program_init(np_pio, sm, offset, pin, 800000.f);

    // Limpa buffer de pixels.
    for (uint i = 0; i < LED_COUNT; ++i) {
        leds[i].R = 0;
        leds[i].G = 0;
        leds[i].B = 0;
    }
}

/**
 * Atribui uma cor RGB a um LED.
 */
void npSetLED(const uint index, const uint8_t r, const uint8_t g, const uint8_t b) {
    leds[index].R = r;
    leds[index].G = g;
    leds[index].B = b;
}

/**
 * Limpa o buffer de pixels.
 */
void npClear() {
    for (uint i = 0; i < LED_COUNT; ++i)
        npSetLED(i, 0, 0, 0);
}

/**
 * Escreve os dados do buffer nos LEDs.
 */
void npWrite() {
    // Escreve cada dado de 8-bits dos pixels em sequência no buffer da máquina PIO.
    for (uint i = 0; i < LED_COUNT; ++i) {
        pio_sm_put_blocking(np_pio, sm, leds[i].G);
        pio_sm_put_blocking(np_pio, sm, leds[i].R);
        pio_sm_put_blocking(np_pio, sm, leds[i].B);
    }
    sleep_us(100); // Espera 100us, sinal de RESET do datasheet.
}

// Função para gerar um número aleatório entre 0 e 24 (índices dos LEDs).
int get_random_led() {
    return rand() % NUM_LEDS;
}

// Função para definir a cor de um LED no buffer.
void set_led_color(int index, uint8_t r, uint8_t g, uint8_t b) {
    npSetLED(index, r, g, b);
}

// Função para fazer um pontinho verde cair corretamente
void drop_green_dot() {
    int col = 2; // Coluna fixa (2)
    for (int row = 4; row >= 0; row--) { // Cai da linha 4 até a linha 0
        int index = row * 5 + col;
        if (row < 4) {
            npSetLED((row + 1) * 5 + col, 0, 0, 255); // Apaga o LED anterior (volta ao azul)
        }
        npSetLED(index, 0, 255, 0); // Acende o LED atual (verde)
        npWrite();
        sleep_ms(2000);
    }
    npSetLED(0 * 5 + col, 0, 0, 255); // Apaga o último LED (volta ao azul)
    npWrite();
}

// Função para piscar as linhas corretamente
void blink_last_two_rows() {
    // Linha 4 branca
    for (int col = 0; col < 5; col++) {
        npSetLED(4 * 5 + col, 255, 255, 255);
    }
    npWrite();
    sleep_ms(2000);

    // Linha 3 branca
    for (int col = 0; col < 5; col++) {
        npSetLED(3 * 5 + col, 255, 255, 255);
    }
    npWrite();
    sleep_ms(2000);

    // Linha 3 volta ao azul
    for (int col = 0; col < 5; col++) {
        npSetLED(3 * 5 + col, 0, 0, 255);
    }
    npWrite();
    sleep_ms(2000);

    // Linha 4 volta ao azul
    for (int col = 0; col < 5; col++) {
        npSetLED(4 * 5 + col, 0, 0, 255);
    }
    npWrite();
}

int main() {
    // Inicializa o sistema.
    stdio_init_all();

    // Inicialização do i2c para o display OLED
    i2c_init(i2c1, ssd1306_i2c_clock * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    gpio_init(BUTTON_A_PIN);
    gpio_init(BUTTON_B_PIN);
    gpio_set_dir(BUTTON_A_PIN, GPIO_IN);
    gpio_set_dir(BUTTON_B_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_A_PIN);
    gpio_pull_up(BUTTON_B_PIN);



    // Processo de inicialização completo do OLED SSD1306
    ssd1306_init();

    // Preparar área de renderização para o display (ssd1306_width pixels por ssd1306_n_pages páginas)
    struct render_area frame_area = {
        start_column : 0,
        end_column : ssd1306_width - 1,
        start_page : 0,
        end_page : ssd1306_n_pages - 1
    };

    calculate_render_area_buffer_length(&frame_area);

    // Zera o display inteiro
    uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, ssd1306_buffer_length);
    render_on_display(ssd, &frame_area);

    // Exibe a mensagem no display OLED
    char *text[] = {
        "  Bem-vindo ao   ",
        "  Aquamatico   ",
        "     Mini!     "};

    int y = 0;
    for (uint i = 0; i < count_of(text); i++) {
        ssd1306_draw_string(ssd, 5, y, text[i]);
        y += 8; // Incrementa a posição vertical para a próxima linha
    }
    render_on_display(ssd, &frame_area);

    // Inicializa a matriz de LEDs
    npInit(LED_PIN);
    npClear(); // Garante que todos os LEDs comecem apagados.

    // Define todos os LEDs como brancos.
    for (int i = 0; i < NUM_LEDS; i++) {
        set_led_color(i, 0, 0, 255); // Branco (R=0, G=0, B=255).
    }
    npWrite(); // Envia os valores do buffer para os LEDs físicos.

    while (true) {
        // Escolhe um LED aleatório.
        int random_led = get_random_led();
    
        // Define o LED escolhido como laranja (R=255, G=165, B=0).
        set_led_color(random_led, 255, 165, 0);
        npWrite(); // Atualiza os LEDs.
    
        // Mantém o LED laranja aceso por 2 segundos.
        sleep_ms(2000);
    
        // Volta o LED escolhido para branco.
        set_led_color(random_led, 0, 0, 255);
        npWrite(); // Atualiza os LEDs.
    
        // Aguarda 2 segundos antes de escolher outro LED.
        sleep_ms(2000);
    
        // Verifica se o botão A foi pressionado
        if (!gpio_get(BUTTON_A_PIN)) {
            drop_green_dot(); // Faz o pontinho verde cair
            sleep_ms(500); // Debounce
        }
    
        // Verifica se o botão B foi pressionado
        if (!gpio_get(BUTTON_B_PIN)) {
            blink_last_two_rows(); // Pisca as duas últimas linhas
            sleep_ms(500); // Debounce
        }
    
        // A cada 5 segundos, exibe temperatura e pH no monitor OLED
        static uint32_t last_update_time = 0;
        if (to_ms_since_boot(get_absolute_time()) - last_update_time >= 5000) {
            last_update_time = to_ms_since_boot(get_absolute_time());
            
            // Limpa o display desenhando espaços em branco sobre a área dos textos
            char *clear_text[] = {
                "              ", // Espaços para limpar a linha da temperatura
                "              ", // Espaços para limpar a linha do pH
                "              "  // Espaços para limpar a linha onde "Mini" estava
            };

            int y = 0;
            ssd1306_draw_string(ssd, 5, y, clear_text[0]); // Limpa a linha da temperatura
            y += 10;
            ssd1306_draw_string(ssd, 5, y, clear_text[1]); // Limpa a linha do pH
            y += 10;
            ssd1306_draw_string(ssd, 5, y, clear_text[2]); // Limpa a linha onde o "Mini" estava

        // Agora você pode continuar com a exibição da temperatura e pH

    
    
            // Simula valores de temperatura e pH
            float temperatura = 27.0 + (rand() % 30) / 10.0; // Entre 27.0 e 30.0
            float pH = 6.0 + (rand() % 21) / 10.0; // Entre 6.0 e 8.0
            int nivel = 95 + (rand() % 6);
    
            // Exibe no monitor OLED
            char *text[] = {
                " Temperatura: ",
                " pH: ",
                " Niv.agua: "
            };
    
            char temp_buffer[16];
            char ph_buffer[16];
            char nivel_buffer[16];
            snprintf(temp_buffer, sizeof(temp_buffer), " %.1f C", temperatura);
            snprintf(ph_buffer, sizeof(ph_buffer), " %.1f", pH);
            snprintf(nivel_buffer, sizeof(nivel_buffer), " %d", nivel);
    
            y = 0;
            ssd1306_draw_string(ssd, 5, y, text[0]);
            ssd1306_draw_string(ssd, 80, y, temp_buffer);
            y += 10;
            ssd1306_draw_string(ssd, 5, y, text[1]);
            ssd1306_draw_string(ssd, 80, y, ph_buffer);
            y += 10;
            ssd1306_draw_string(ssd, 5, y, text[2]);
            ssd1306_draw_string(ssd, 80, y, nivel_buffer);
    
            render_on_display(ssd, &frame_area);
        }
    }
    
    
    return 0;
}