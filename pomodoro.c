#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/timer.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "inc/ssd1306.h"

// Definição de constantes
#define B_BUTTON 6
#define JOYSTICK_X 26
#define A_BUZZER 21
#define B_BUZZER 10

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
volatile bool sound = false;

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
void setup_menu(ssd1306_t ssd);
bool minute_timer_callback(struct repeating_timer *t);
void nota(uint32_t frequencia, uint32_t tempo_ms);
void timer_sound();

int main()
{
    setup(); // Configuração das portas digitais

    // Habilitando interrupção da gpio no botão B.
    gpio_set_irq_enabled_with_callback(B_BUTTON, GPIO_IRQ_EDGE_FALL, 1, & gpio_irq_handler);

    // Configuração inicial do display ssd1306, iniciado com todos os pixels apagados.
    ssd1306_t ssd; 
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT);
    ssd1306_config(&ssd); 
    ssd1306_send_data(&ssd);
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

    // Variáveis para armazenar os textos do display
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

            // Define a quantidade de ciclos
            cycles_remaining = cycles[ind_cycles];

            // Temporizador em atividade
            while (active) {
                sprintf(str_ciclos, "Restam %d ciclos", cycles_remaining);
                sprintf(str_work, "  Tempo   %d %d", work_minutes, work_time[ind_work]);
                sprintf(str_break, "Intervalo %d %d", break_minutes, break_time[ind_break]);
                ssd1306_fill(&ssd, !cor);
                ssd1306_rect(&ssd, 3, 3, 124, 60, cor, !cor);
                ssd1306_draw_string(&ssd, str_ciclos, 6, 10);
                ssd1306_draw_string(&ssd, str_work, 6, 28);
                ssd1306_draw_string(&ssd, str_break, 6, 46);
                ssd1306_send_data(&ssd);

                // Se a flag de emitir som for ativa, emite o som e desliga a flag
                if (sound) {
                    timer_sound();
                    sound = false;
                }
            }
        }   
    }

    return 0;
}

// Inicialização e configuração das portas digitais
void setup(void)
{
    stdio_init_all();

    gpio_init(B_BUTTON);        // Inicializando Botão B.
    gpio_set_dir(B_BUTTON, GPIO_IN);
    gpio_pull_up(B_BUTTON);

    adc_init();                 // Inicializar o ADC
    adc_gpio_init(JOYSTICK_X);  // Configurar GPIO para eixo X

    gpio_init(A_BUZZER); // Inicializa o pino do buzzer A
    gpio_set_dir(A_BUZZER, GPIO_OUT);

    gpio_init(B_BUZZER); // Inicializa o pino do buzzer B
    gpio_set_dir(B_BUZZER, GPIO_OUT);

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

        case B_BUTTON:
            if (!active) {
                switch (selected_config) {
                    case 0: // Salva o índice da quantidade de ciclos selecionada
                        ind_cycles = indice;
                        indice = 0;
                        selected_config += 1;
                        break;
                    case 1: // Salva o índice dos minutos de trabalho selecionado
                        ind_work = indice;
                        indice = 0;
                        selected_config += 1;
                        break;
                    case 2: // Salva o índice da dos minutos de intervalo selecionado
                        ind_break = indice;
                        indice = 0;
                        selected_config += 1;
                        break;
                }
            }
            break;
        
        default:
            break;
        }
    }
}

void setup_menu(ssd1306_t ssd) {
    char value[3];
    uint16_t adc_value_x;

    while(true)
    {
        // Seleção da configuração 1: quantidade de clicos
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

        // Seleção da configuração 2: Minutos de trabalho
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
        
        // Seleção da configuração 3: Minutos de intervalo
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

        sleep_ms(200);

        // Após selecionar as 3 configurações, inicia o temporizador
        if (selected_config == 3) {
            active = true;
            break;
        }
    }
}

bool minute_timer_callback(struct repeating_timer *t) {
    // Se não houver mais ciclos, o programa volta ao menu inicial
    if (!cycles_remaining) {
        active = false;
        return false;
    }
    
    // Verifica se está em um intervalo
    if (break_minutes) {
        break_minutes -= 1;

        // Se acabou o intervalo, reseta o tempo de trabalho e ativa a flag de emitir som
        if (!break_minutes) {
            work_minutes = 0;

            sound = true;

            // Contabiliza um ciclo e caso seja o último, reseta o programa.
            if (!--cycles_remaining) {
                active = false;
                return false;
            }
        }
        return true;
    }

    work_minutes += 1;

    // Se passou o tempo de um ciclo, ativa o intervalo e a flag de emitir som
    if (work_minutes == work_time[ind_work]) {
        break_minutes = break_time[ind_break];
        sound = true;
    }

    // Retorna true para manter o temporizador repetindo. Se retornar false, o temporizador para.
    return true;
}

void nota(uint32_t frequencia, uint32_t tempo_ms) {
    uint32_t delay = 1000000 / (frequencia * 2);  // Calcula o tempo de atraso em microssegundos para gerar a onda quadrada
    uint32_t ciclo = frequencia * tempo_ms / 1000;  // Calcula o número de ciclos necessários para a duração desejada

    // Loop para gerar a onda quadrada no buzzer
    for (uint32_t i = 0; i < ciclo; i++) {
        gpio_put(A_BUZZER, 1);
        gpio_put(B_BUZZER, 1);
        sleep_us(delay);
        gpio_put(A_BUZZER, 0);
        gpio_put(B_BUZZER, 0);
        sleep_us(delay); // Espera novamente pelo tempo de atraso para completar o ciclo
    }
}

// Efeito sonoro de 3 beeps
void timer_sound() {
    nota(132, 200);
    sleep_ms(100);
    nota(165, 200);
    sleep_ms(100);
    nota(247, 200);
}