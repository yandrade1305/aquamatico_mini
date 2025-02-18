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

const uint I2C_SDA = 14;
const uint I2C_SCL = 15;

#define LED_COUNT 25
#define LED_PIN 7
#define NUM_LEDS 25
#define BRIGHTNESS 255
#define BUTTON_A_PIN 5
#define BUTTON_B_PIN 6

struct pixel_t {
    uint8_t G, R, B;
};
typedef struct pixel_t pixel_t;
typedef pixel_t npLED_t;

npLED_t leds[LED_COUNT];

PIO np_pio;
uint sm;

void npInit(uint pin) {
    uint offset = pio_add_program(pio0, &ws2818b_program);
    np_pio = pio0;

    sm = pio_claim_unused_sm(np_pio, false);
    if (sm < 0) {
        np_pio = pio1;
        sm = pio_claim_unused_sm(np_pio, true);
    }

    ws2818b_program_init(np_pio, sm, offset, pin, 800000.f);

    for (uint i = 0; i < LED_COUNT; ++i) {
        leds[i].R = 0;
        leds[i].G = 0;
        leds[i].B = 0;
    }
}

void npSetLED(const uint index, const uint8_t r, const uint8_t g, const uint8_t b) {
    leds[index].R = r;
    leds[index].G = g;
    leds[index].B = b;
}

void npClear() {
    for (uint i = 0; i < LED_COUNT; ++i)
        npSetLED(i, 0, 0, 0);
}

void npWrite() {
    for (uint i = 0; i < LED_COUNT; ++i) {
        pio_sm_put_blocking(np_pio, sm, leds[i].G);
        pio_sm_put_blocking(np_pio, sm, leds[i].R);
        pio_sm_put_blocking(np_pio, sm, leds[i].B);
    }
    sleep_us(100);
}

int get_random_led() {
    return rand() % NUM_LEDS;
}

void set_led_color(int index, uint8_t r, uint8_t g, uint8_t b) {
    npSetLED(index, r, g, b);
}

void drop_green_dot() {
    int col = 2;
    for (int row = 4; row >= 0; row--) { 
        int index = row * 5 + col;
        if (row < 4) {
            npSetLED((row + 1) * 5 + col, 0, 0, 255);
        }
        npSetLED(index, 0, 255, 0);
        npWrite();
        sleep_ms(2000);
    }
    npSetLED(0 * 5 + col, 0, 0, 255);
    npWrite();
}

void blink_last_two_rows() {
    for (int col = 0; col < 5; col++) {
        npSetLED(4 * 5 + col, 255, 255, 255);
    }
    npWrite();
    sleep_ms(2000);

    for (int col = 0; col < 5; col++) {
        npSetLED(3 * 5 + col, 255, 255, 255);
    }
    npWrite();
    sleep_ms(2000);

    for (int col = 0; col < 5; col++) {
        npSetLED(3 * 5 + col, 0, 0, 255);
    }
    npWrite();
    sleep_ms(2000);

    for (int col = 0; col < 5; col++) {
        npSetLED(4 * 5 + col, 0, 0, 255);
    }
    npWrite();
}

int main() {
    bool botao_a_pressionado = false;
    bool botao_b_pressionado = false;
    int passo_motor = 0;
    bool bomba_ligada = false;

    stdio_init_all();

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

    ssd1306_init();

    struct render_area frame_area = {
        start_column : 0,
        end_column : ssd1306_width - 1,
        start_page : 0,
        end_page : ssd1306_n_pages - 1
    };

    calculate_render_area_buffer_length(&frame_area);

    uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, ssd1306_buffer_length);
    render_on_display(ssd, &frame_area);

    char *text[] = {
        "  Bem-vindo ao   ",
        "  Aquamatico   ",
        "     Mini!     "};

    int y = 0;
    for (uint i = 0; i < count_of(text); i++) {
        ssd1306_draw_string(ssd, 5, y, text[i]);
        y += 8; 
    }
    render_on_display(ssd, &frame_area);

    npInit(LED_PIN);
    npClear();

    for (int i = 0; i < NUM_LEDS; i++) {
        set_led_color(i, 0, 0, 255);
    }
    npWrite();

    while (true) {
        int random_led = get_random_led();

        set_led_color(random_led, 255, 165, 0);
        npWrite();
        sleep_ms(2000);
    
        set_led_color(random_led, 0, 0, 255);
        npWrite();
        sleep_ms(2000);
    
        if (!gpio_get(BUTTON_A_PIN)) {
            botao_a_pressionado = true;
            passo_motor = 1;

            uint8_t ssd[ssd1306_buffer_length];
            memset(ssd, 0, ssd1306_buffer_length);
            render_on_display(ssd, &frame_area);
        
            char *text[] = {
                "  Fazendo  ",
                "  Alimentacao   "};
        
            int y = 0;
            for (uint i = 0; i < count_of(text); i++) {
                ssd1306_draw_string(ssd, 5, y, text[i]);
                y += 8; 
            }
            render_on_display(ssd, &frame_area);
            
            drop_green_dot();
            sleep_ms(500);

            botao_a_pressionado = false;
            passo_motor = 0;
        }
    
        if (!gpio_get(BUTTON_B_PIN)) {

            botao_b_pressionado = true;
            bomba_ligada = true;
                        uint8_t ssd[ssd1306_buffer_length];
                        memset(ssd, 0, ssd1306_buffer_length);
                        render_on_display(ssd, &frame_area);

                        char *text[] = {
                            "  Fazendo  ",
                            "  TPA   "};
                    
                        int y = 0;
                        for (uint i = 0; i < count_of(text); i++) {
                            ssd1306_draw_string(ssd, 5, y, text[i]);
                            y += 8;
                        }
                        render_on_display(ssd, &frame_area);
            

            blink_last_two_rows();
            sleep_ms(500);

            botao_b_pressionado = false;
            bomba_ligada = false;
        }
    
        static uint32_t last_update_time = 0;
        if (to_ms_since_boot(get_absolute_time()) - last_update_time >= 5000) {
            last_update_time = to_ms_since_boot(get_absolute_time());
            
            char *clear_text[] = {
                "              ", 
                "              ", 
                "              "  
            };

            int y = 0;
            ssd1306_draw_string(ssd, 5, y, clear_text[0]);
            y += 10;
            ssd1306_draw_string(ssd, 5, y, clear_text[1]);
            y += 10;
            ssd1306_draw_string(ssd, 5, y, clear_text[2]);

            float temperatura = 27.0 + (rand() % 30) / 10.0;
            float pH = 6.0 + (rand() % 21) / 10.0;
            int nivel = 95 + (rand() % 6);
    
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