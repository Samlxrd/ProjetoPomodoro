#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/timer.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "inc/ssd1306.h"

// Definição de constantes
#define A_BUTTON 5
#define B_BUTTON 6
#define LED_RED 13
#define LED_BLUE 12
#define LED_GREEN 11
#define JOYSTICK_BUTTON 22
#define JOYSTICK_X 26

#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C
#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 64
#define RECT_SIZE 8

uint32_t last_time;

volatile bool cor = true;
volatile bool active = false;

// Predefinições de tempo que poderão ser escolhidas no programa
static uint8_t cycles[] = { 2, 3, 4, 5 };
static uint8_t work_time[] = { 20, 25, 30, 40, 50, 60 };
static uint8_t break_time[] = { 5, 10, 15 };

// Variáveis utilizadas para salvar as configurações escolhidas pelo usuário
uint8_t ind_cycles, ind_work, ind_break;
uint8_t selected_config;
uint8_t indice;

// Variáveis para o temporizador
uint8_t work_minutes, break_minutes, cycles_remaining;

// Protótipo das Funções
void setup(void);
static void gpio_irq_handler(uint gpio, uint32_t events);
uint pwm_init_gpio(uint gpio, uint wrap);
int map(int valor, int center, int in_min, int in_max, int out_min, int out_max);
void setup_menu(ssd1306_t ssd);
bool minute_timer_callback(struct repeating_timer *t);

int main()
{
    setup(); // Configuração das portas digitais

    // Habilitando interrupção da gpio no botão do Joystick, no botão A e botão B.
    gpio_set_irq_enabled_with_callback(JOYSTICK_BUTTON, GPIO_IRQ_EDGE_FALL, 1, & gpio_irq_handler);
    gpio_set_irq_enabled(A_BUTTON, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(B_BUTTON, GPIO_IRQ_EDGE_FALL, true);

    // Configuração inicial do display ssd1306, iniciado com todos os pixels apagados.
    ssd1306_t ssd; 
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT);
    ssd1306_config(&ssd); 
    ssd1306_send_data(&ssd);
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

    char str_ciclos[20];
    char str_work[20];
    char str_break[20];
    
    while(true) {
        // Se o temporizador não estiver ativo, o usuário poderá selecionar as configurações de tempo desejadas.
        if (!active) {
            selected_config = 0;
            indice = 0;
            setup_menu(ssd);
        }

        else {
            struct repeating_timer timer;

            // Configura o temporizador para chamar a função de callback a cada minuto.
            add_repeating_timer_ms(60000, minute_timer_callback, NULL, &timer);
            cycles_remaining = cycles[ind_cycles];

            // Temporizador em atividade
            while (active) {
                sprintf(str_ciclos, "Restam %d ciclos", cycles_remaining);
                sprintf(str_work, "Tempo %d/%d", work_minutes, work_time[ind_work]);
                sprintf(str_break, "Intervalo %d/%d", break_minutes, break_time[ind_break]);
                ssd1306_fill(&ssd, !cor); // Limpa o display
                ssd1306_rect(&ssd, 3, 3, 124, 60, cor, !cor); // Desenha um retângulo
                ssd1306_draw_string(&ssd, str_ciclos, 6, 10); // Desenha uma string
                ssd1306_draw_string(&ssd, str_work, 6, 28); // Desenha uma string
                ssd1306_draw_string(&ssd, str_break, 6, 46); // Desenha uma string
                ssd1306_send_data(&ssd); // Atualiza o display
            }
        }

            
    }

    return 0;
}

// Inicialização e configuração das portas digitais
void setup(void)
{
    stdio_init_all();

    gpio_init(LED_RED);         // Inicializando LED vermelho.
    gpio_set_dir(LED_RED, GPIO_OUT);

    gpio_init(LED_GREEN);       // Inicializando LED verde.
    gpio_set_dir(LED_GREEN, GPIO_OUT);

    gpio_init(A_BUTTON);        // Inicializando Botão A.
    gpio_set_dir(A_BUTTON, GPIO_IN);
    gpio_pull_up(A_BUTTON);

    gpio_init(B_BUTTON);        // Inicializando Botão B.
    gpio_set_dir(B_BUTTON, GPIO_IN);
    gpio_pull_up(B_BUTTON);

    adc_init();                 // Inicializar o ADC
    adc_gpio_init(JOYSTICK_X);  // Configurar GPIO para eixo X

    gpio_init(JOYSTICK_BUTTON); // Inicializando o Botão do joystick
    gpio_set_dir(JOYSTICK_BUTTON, GPIO_IN);
    gpio_pull_up(JOYSTICK_BUTTON);

    // Inicialização do I2C em 400Khz.
    i2c_init(I2C_PORT, 400 * 1000);

    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C); // Configura o pino GPIO para I2C
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C); // Configura o pino GPIO para I2C
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

}

// Rotina da Interrupção
static void gpio_irq_handler(uint gpio, uint32_t events){

    uint32_t current_time = to_us_since_boot(get_absolute_time());
    if (current_time - last_time > 200000) // Delay de 200ms para debounce
    {
        last_time = current_time;

        switch (gpio) {
        case A_BUTTON: 
            break;

        case B_BUTTON:
            if (!active) {
                switch (selected_config) {
                    case 0:
                        ind_cycles = indice;
                        indice = 0;
                        selected_config += 1;
                        break;
                    case 1:
                        ind_work = indice;
                        indice = 0;
                        selected_config += 1;
                        break;
                    case 2:
                        ind_break = indice;
                        indice = 0;
                        selected_config += 1;
                        break;
                }
            }
            break;

        case JOYSTICK_BUTTON:
            break;
        
        default:
            break;
        }
    }
}

uint pwm_init_gpio(uint gpio, uint wrap) {
    gpio_set_function(gpio, GPIO_FUNC_PWM);

    uint slice_num = pwm_gpio_to_slice_num(gpio);
    pwm_set_wrap(slice_num, wrap);
    
    pwm_set_enabled(slice_num, true);  
    return slice_num;  
}

// Função para converter proporcionalmente os valores do adc para o display
int map(int value, int center, int min, int max, int out_min, int out_max) {
    return ((value - center) * (out_max - out_min) / (max - min)) + (out_min + out_max) / 2;
}

void setup_menu(ssd1306_t ssd) {
    char value[3];

    uint16_t adc_value_x;

    while(true)
    {
        if (!selected_config) {
            adc_select_input(1);
            adc_value_x = adc_read();

            if (adc_value_x > 2700) {
                if (indice == 3) { indice = 0; }
                else { indice++; }
            }

            else if (adc_value_x < 1500) {
                if (indice == 0) { indice = 3; }
                else { indice--; }
            }

            sleep_ms(100);

            sprintf(value, "%d", cycles[indice]);
            ssd1306_fill(&ssd, !cor);
            ssd1306_rect(&ssd, 3, 3, 124, 60, cor, !cor);
            ssd1306_draw_string(&ssd, "Ciclos", 40, 10);
            ssd1306_draw_string(&ssd, value, 60, 30);
            ssd1306_send_data(&ssd);
        }

        if (selected_config == 1) {
            adc_select_input(1);
            adc_value_x = adc_read();

            if (adc_value_x > 2700) {
                if (indice == 5) { indice = 0; }
                else { indice++; }
            }

            else if (adc_value_x < 1500) {
                if (indice == 0) { indice = 5; }
                else { indice--; }
            }

            sleep_ms(100);

            sprintf(value, "%d", work_time[indice]);
            ssd1306_fill(&ssd, !cor);
            ssd1306_rect(&ssd, 3, 3, 124, 60, cor, !cor);
            ssd1306_draw_string(&ssd, "Tempo", 44, 10);
            ssd1306_draw_string(&ssd, value, 56, 30);
            ssd1306_send_data(&ssd); 
        }
    
        if (selected_config == 2) {
            adc_select_input(1);
            adc_value_x = adc_read();

            if (adc_value_x > 2700) {
                if (indice == 2) { indice = 0; }
                else { indice++; }
            }

            else if (adc_value_x < 1500) {
                if (indice == 0) { indice = 2; }
                else { indice--; }
            }

            sleep_ms(100);

            sprintf(value, "%d", break_time[indice]);
            ssd1306_fill(&ssd, !cor);
            ssd1306_rect(&ssd, 3, 3, 124, 60, cor, !cor);
            ssd1306_draw_string(&ssd, "Pausa", 44, 10);
            ssd1306_draw_string(&ssd, value, 60, 30);
            ssd1306_send_data(&ssd); 
        }

        sleep_ms(100);

        if (selected_config == 3) {
            // Iniciar o programa com as configurações predefinidas
            active = true;
            break;
        }
    }
}

bool minute_timer_callback(struct repeating_timer *t) {
    if (!cycles_remaining) {
        active = false;
        return false;
    }
    
    if (break_minutes) {
        break_minutes -= 1;

        if (!break_minutes) {
            work_minutes = 0;

            // Contabiliza um ciclo e caso seja o último, reseta o programa.
            if (!--cycles_remaining) {
                active = false;
                return false;
            }
        }
        return true;
    }

    work_minutes += 1;

    // Se passou o tempo de um ciclo, ativar a pausa
    if (work_minutes == work_time[ind_work]) {
        break_minutes = break_time[ind_break];
    }

    // Retorna true para manter o temporizador repetindo. Se retornar false, o temporizador para.
    return true;
}