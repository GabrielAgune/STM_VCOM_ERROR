/**
 * ============================================================================
 * @file    cli_driver.c
 * @brief   Implementação do driver de Command Line Interface (CLI).
 * @author  Gabriel Agune
 * 
 * Este arquivo contém a lógica para:
 * - Gerenciamento de um buffer de transmissão (FIFO) para logs USB.
 * - Recebimento e "echo" de caracteres.
 * - Parsing e execução de comandos de texto.
 * - Handlers para comandos específicos (DWIN, RTC, Medição, etc.).
 * ============================================================================
 */

#include "cli_driver.h"
#include "dwin_driver.h"
#include "rtc_driver.h"
#include "medicao_handler.h"
#include "temp_sensor.h"
#include "relato.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>

#include "ux_device_cdc_acm.h"

/*
==================================================
  BUFFER DE TRANSMISSÃO (FIFO)
==================================================
*/
#define CLI_TX_FIFO_SIZE 1536 // Aumente se precisar de mais logs
static uint8_t s_cli_tx_fifo[CLI_TX_FIFO_SIZE];
static volatile uint16_t s_cli_tx_head = 0;
static volatile uint16_t s_cli_tx_tail = 0;

// Referência externa para a instância do CDC-ACM.
extern UX_SLAVE_CLASS_CDC_ACM *cdc_acm; 

/*
==================================================
  DEFINIÇÕES E PROTÓTIPOS INTERNOS
==================================================
*/

#define CLI_BUFFER_SIZE 128 // Tamanho do buffer de recepção de comandos

// --- Protótipos de Funções de Comando ---
static void Process_Command(void);
static void Cmd_Help(char* args);
static void Cmd_WhoAmI(char* args);
static void Cmd_Dwin(char* args);
static void Cmd_SetTime(char* args);
static void Cmd_SetDate(char* args);
static void Cmd_Date(char* args);
static void Cmd_GetPeso(char* args);
static void Cmd_GetTemp(char* args);
static void Cmd_GetFreq(char* args);
static void Cmd_Service(char* args);

// --- Protótipos de Handlers DWIN ---
static void Handle_Dwin_PIC(char* sub_args);
static void Handle_Dwin_INT(char* sub_args);
static void Handle_Dwin_INT32(char* sub_args);
static void Handle_Dwin_RAW(char* sub_args);
static uint8_t hex_char_to_value(char c);

/*
==================================================
  VARIÁVEIS ESTÁTICAS DO MÓDULO
==================================================
*/

// Buffer para montagem do comando recebido
static char s_cli_buffer[CLI_BUFFER_SIZE];
static uint16_t s_cli_buffer_index = 0;

// Flag que indica que um comando completo foi recebido
static bool s_command_ready = false;

/*
==================================================
  TABELAS DE COMANDOS
==================================================
*/

// --- Tabela de Comandos Principal ---
typedef struct {
    const char* name;
    void (*handler)(char* args);
} cli_command_t;

static const cli_command_t s_command_table[] = {
    { "HELP", Cmd_Help },
    { "?", Cmd_Help },
    { "DWIN", Cmd_Dwin },
    { "SETTIME", Cmd_SetTime },
    { "SETDATE", Cmd_SetDate },
    { "DATE", Cmd_Date },
    { "PESO", Cmd_GetPeso },
    { "TEMP", Cmd_GetTemp },
    { "FREQ", Cmd_GetFreq },
    { "SERVICE", Cmd_Service },
    { "WHO_AM_I", Cmd_WhoAmI },
};
static const size_t NUM_COMMANDS = sizeof(s_command_table) / sizeof(s_command_table[0]);

// --- Tabela de Subcomandos DWIN ---
typedef struct {
    const char* name;
    void (*handler)(char* args);
} dwin_subcommand_t;

static const dwin_subcommand_t s_dwin_table[] = {
    { "PIC", Handle_Dwin_PIC },
    { "INT", Handle_Dwin_INT },
    { "INT32", Handle_Dwin_INT32 },
    { "RAW", Handle_Dwin_RAW }
};
static const size_t NUM_DWIN_SUBCOMMANDS = sizeof(s_dwin_table) / sizeof(s_dwin_table[0]);

/*
==================================================
  DADOS ESTÁTICOS (TEXTOS DE AJUDA)
==================================================
*/

static const char HELP_TEXT[] =
    "========================== CLI de Teste DWIN & RTC =========================\r\n"
    "| HELP ou ?                | Mostra esta ajuda.                            |\r\n"
    "| DWIN PIC <id>            | Muda a tela (ex: DWIN PIC 1).                 |\r\n"
    "| DWIN INT <addr> <val>    | Escreve int16 (ex: DWIN INT 1500 -10).        |\r\n"
    "| DWIN INT32 <addr> <val>  | Escreve int32 (ex: DWIN INT32 1500 40500)     |\r\n"
    "| DWIN RAW <hex...>        | Envia bytes hex (ex: DWIN RAW 5A A5...).      |\r\n"
    "| SETTIME HH:MM:SS         | Ajusta a hora do RTC.                         |\r\n"
    "| SETDATE DD/MM/YY         | Ajusta a data do RTC.                         |\r\n"
    "| DATE                     | Mostra a data e hora atuais.                  |\r\n"
    "| SERVICE                  | Entra na tela de servico.                     |\r\n"
    "| PESO                     | Mostra a leitura atual da balanca.            |\r\n"
    "| TEMP                     | Mostra a leitura do sensor de temperatura.    |\r\n"
    "| FREQ                     | Mostra a ultima leitura de frequencia.        |\r\n"
    "============================================================================\r\n";

/*
==================================================
  FUNÇÕES PÚBLICAS
==================================================
*/

/**
 * @brief Verifica se o host USB (PC) está conectado e a classe CDC está ativa.
 *
 * @return true se o USB está pronto para comunicação, false caso contrário.
 */
bool CLI_Is_USB_Connected(void) {
    return (cdc_acm != NULL);
}

/**
 * @brief Enfileira uma string simples para envio via CLI (FIFO).
 *
 * @param str A string terminada em nulo a ser enviada.
 */
void CLI_Puts(const char* str) {
    if (str == NULL) {
        return;
    }

    // Se o USB não está conectado, não faz nada para não encher o buffer em vão.
    if (!CLI_Is_USB_Connected()) {
        return;
    }

    uint32_t len = strlen(str);
    if (len == 0) {
        return;
    }

    // Coloca os dados no FIFO.
    for (uint32_t i = 0; i < len; i++) {
        uint16_t next_head = (s_cli_tx_head + 1) % CLI_TX_FIFO_SIZE;

        if (next_head == s_cli_tx_tail) {
            // Buffer cheio, descarta o resto da string.
            break;
        }
        s_cli_tx_fifo[s_cli_tx_head] = (uint8_t)str[i];
        s_cli_tx_head = next_head;
    }
}

/**
 * @brief Envia dados do buffer de transmissão (FIFO) para a porta USB.
 *
 * Deve ser chamado continuamente no loop principal.
 */
void CLI_TX_Pump(void) {
    // Se não há conexão ou nada para enviar, retorna.
    if (!CLI_Is_USB_Connected() || (s_cli_tx_head == s_cli_tx_tail)) {
        return;
    }

    uint32_t sent_bytes;
    uint16_t bytes_to_send = 0;

    // Calcula quantos bytes podem ser enviados de forma contígua no buffer circular
    if (s_cli_tx_head > s_cli_tx_tail) {
        bytes_to_send = s_cli_tx_head - s_cli_tx_tail;
    } else {
        // O buffer deu a volta (wrap-around)
        bytes_to_send = CLI_TX_FIFO_SIZE - s_cli_tx_tail;
    }

    // Limita ao tamanho máximo do endpoint USB (comum ser 64 bytes)
    if (bytes_to_send > 64) {
        bytes_to_send = 64;
    }

    
    if (USBD_CDC_ACM_Transmit(&s_cli_tx_fifo[s_cli_tx_tail], bytes_to_send, &sent_bytes) == 0) {
        // Atualiza o tail com o número de bytes que foram realmente aceitos para transmissão
        if (sent_bytes > 0) {
            s_cli_tx_tail = (s_cli_tx_tail + sent_bytes) % CLI_TX_FIFO_SIZE;
        }
    }
}

/**
 * @brief Envia uma string formatada para o CLI (estilo printf).
 *
 * @param format A string de formato (ex: "Valor: %d").
 * @param ...    Argumentos variáveis correspondentes ao formato.
 */
void CLI_Printf(const char* format, ...) {
    // Buffer estático para evitar alocação de stack excessiva.
    // Aumente se mensagens maiores forem necessárias.
    static char printf_buffer[256];

    va_list args;
    va_start(args, format);
    int len = vsnprintf(printf_buffer, sizeof(printf_buffer), format, args);
    va_end(args);

    if (len > 0) {
        CLI_Puts(printf_buffer);
    }
}

/**
 * @brief Processa um comando pendente que foi recebido.
 *
 * Deve ser chamado no loop principal do sistema.
 */
void CLI_Process(void) {
    if (s_command_ready) {
        CLI_Printf("\r\n"); // Nova linha antes da resposta
        Process_Command();

        // Limpa o buffer para o próximo comando
        memset(s_cli_buffer, 0, CLI_BUFFER_SIZE);
        s_cli_buffer_index = 0;
        s_command_ready = false;

        CLI_Printf("\r\n> "); // Exibe o prompt
    }
}

/**
 * @brief Recebe um único caractere da interface de comunicação (ex: USB).
 *
 * @param received_char O caractere (byte) recebido.
 */
void CLI_Receive_Char(uint8_t received_char) {
    if (s_command_ready) {
        // Ignora caracteres se um comando já está esperando processamento
        return;
    }

    // --- Tratamento de Enter (CR ou LF) ---
    if (received_char == '\r' || received_char == '\n') {
        if (s_cli_buffer_index > 0) {
            // Comando pronto
            s_cli_buffer[s_cli_buffer_index] = '\0'; // Termina a string
            s_command_ready = true;
        } else {
            // Enter com buffer vazio, apenas mostra novo prompt
            CLI_Printf("\r\n> ");
        }
    }
    // --- Tratamento de Backspace ---
    else if (received_char == '\b' || received_char == 127) {
        if (s_cli_buffer_index > 0) {
            s_cli_buffer_index--;
            CLI_Printf("\b \b"); // "Apaga" o caractere no terminal
        }
    }
    // --- Tratamento de caracteres imprimíveis ---
    else if (s_cli_buffer_index < (CLI_BUFFER_SIZE - 1) && isprint(received_char)) {
        s_cli_buffer[s_cli_buffer_index++] = received_char;

        // Echo do caractere de volta para o terminal
        uint8_t ch_to_echo[2] = { received_char, '\0' };
        CLI_Puts((const char*)ch_to_echo);
    }
}

/*
==================================================
  FUNÇÕES AUXILIARES INTERNAS (PARSING)
==================================================
*/

/**
 * @brief Processa o comando armazenado em `s_cli_buffer`.
 *
 * Separa o comando dos argumentos e chama o handler correspondente
 * na tabela de comandos.
 */
static void Process_Command(void) {
    char* command_str = s_cli_buffer;
    char* args = NULL;

    // 1. Pula espaços em branco no início
    while (isspace((unsigned char)*command_str)) {
        command_str++;
    }

    // 2. Encontra o primeiro espaço (separador de argumentos)
    args = strchr(command_str, ' ');
    if (args != NULL) {
        *args = '\0'; // Termina a string do comando
        args++;
        // Pula espaços em branco no início dos argumentos
        while (isspace((unsigned char)*args)) {
            args++;
        }
        if (*args == '\0') {
            args = NULL; // Argumentos são apenas espaços em branco
        }
    }

    if (*command_str == '\0') {
        return; // Comando vazio
    }

    // 3. Procura o comando na tabela
    for (size_t i = 0; i < NUM_COMMANDS; i++) {
        if (strcasecmp(command_str, s_command_table[i].name) == 0) {
            s_command_table[i].handler(args);
            return;
        }
    }

    CLI_Printf("Comando desconhecido: \"%s\".", command_str);
}

/*
==================================================
  HANDLERS DE COMANDO (PRINCIPAIS)
==================================================
*/

/**
 * @brief Handler do comando HELP ou ?.
 * @param args Argumentos (ignorados).
 */
static void Cmd_Help(char* args) {
    (void)args; // Evita warning de "unused parameter"
    CLI_Puts(HELP_TEXT);
}

/**
 * @brief Handler do comando WHO_AM_I.
 * @param args Argumentos (ignorados).
 */
static void Cmd_WhoAmI(char* args) {
    (void)args;
    Who_am_i(); // Chama a função externa
}

/**
 * @brief Handler do comando SERVICE.
 * @param args Argumentos (ignorados).
 */
static void Cmd_Service(char* args) {
    (void)args;
    DWIN_Driver_SetScreen(TELA_SERVICO);
}

/**
 * @brief Handler do comando SETTIME.
 * @param args String contendo a hora (formato "HH:MM:SS").
 */
static void Cmd_SetTime(char* args) {
    if (args == NULL) {
        CLI_Printf("Erro: Faltam argumentos. Uso: SETTIME HH:MM:SS");
        return;
    }

    uint8_t h, m, s;
    if (sscanf(args, "%hhu:%hhu:%hhu", &h, &m, &s) == 3 && h < 24 && m < 60 && s < 60) {
        if (RTC_Driver_SetTime(h, m, s)) {
            CLI_Printf("OK. RTC atualizado para %02u:%02u:%02u", h, m, s);
        } else {
            CLI_Printf("Erro: Falha ao setar a hora no hardware do RTC.");
        }
    } else {
        CLI_Printf("Erro: Formato ou valores invalidos. Uso: SETTIME HH(0-23):MM(0-59):SS(0-59).");
    }
}

/**
 * @brief Handler do comando SETDATE.
 * @param args String contendo a data (formato "DD/MM/YY").
 */
static void Cmd_SetDate(char* args) {
    if (args == NULL) {
        CLI_Printf("Erro: Faltam argumentos. Uso: SETDATE DD/MM/YY");
        return;
    }

    uint8_t d, m, a; // Dia, Mês, Ano
    if (sscanf(args, "%hhu/%hhu/%hhu", &d, &m, &a) == 3 && d >= 1 && d <= 31 && m >= 1 && m <= 12) {
        if (RTC_Driver_SetDate(d, m, a)) {
            CLI_Printf("OK. RTC atualizado para %02u/%02u/%02u", d, m, a);
        } else {
            CLI_Printf("Erro: Falha ao setar a data no hardware do RTC.");
        }
    } else {
        CLI_Printf("Erro: Formato ou valores invalidos. Uso: SETDATE DD(1-31)/MM(1-12)/YY(00-99).");
    }
}

/**
 * @brief Handler do comando DATE.
 * @param args Argumentos (ignorados).
 */
static void Cmd_Date(char* args) {
    (void)args;
    uint8_t h, m, s, d, mo, y;
    char weekday_str[4]; // Buffer para dia da semana (ex: "SEG")

    bool time_ok = RTC_Driver_GetTime(&h, &m, &s);
    bool date_ok = RTC_Driver_GetDate(&d, &mo, &y, weekday_str);

    if (time_ok && date_ok) {
        CLI_Printf("Data/Hora: %s %02u/%02u/20%02u %02u:%02u:%02u", weekday_str, d, mo, y, h, m, s);
    } else {
        CLI_Printf("Erro: Nao foi possivel ler a data/hora do RTC.");
    }
}

/**
 * @brief Handler do comando PESO.
 * @param args Argumentos (ignorados).
 */
static void Cmd_GetPeso(char* args) {
    (void)args;
    DadosMedicao_t dados_atuais;
    Medicao_Get_UltimaMedicao(&dados_atuais);
    CLI_Printf("Dados da Balanca:\r\n  - Peso: %.2f g\r\n", dados_atuais.Peso);
}

/**
 * @brief Handler do comando TEMP.
 * @param args Argumentos (ignorados).
 */
static void Cmd_GetTemp(char* args) {
    (void)args;
    float temperatura = TempSensor_GetTemperature();
    CLI_Printf("Temperatura interna do MCU: %.2f C\r\n", temperatura);
}

/**
 * @brief Handler do comando FREQ.
 * @param args Argumentos (ignorados).
 */
static void Cmd_GetFreq(char* args) {
    (void)args;
    DadosMedicao_t dados_atuais;
    Medicao_Get_UltimaMedicao(&dados_atuais);
    CLI_Printf("Dados de Frequencia:\r\n");
    CLI_Printf("  - Pulsos (em 1s): %.1f\r\n", dados_atuais.Frequencia);
    CLI_Printf("  - Escala A (calc): %.2f\r\n", dados_atuais.Escala_A);
}

/**
 * @brief Handler principal do comando DWIN (chama subcomandos).
 * @param args String contendo o subcomando e seus argumentos.
 */
static void Cmd_Dwin(char* args) {
    if (args == NULL) {
        CLI_Printf("Subcomando DWIN faltando. Use 'HELP'.");
        return;
    }

    char* sub_cmd = args;
    char* sub_args = NULL;

    // Separa o subcomando (ex: "PIC") do resto dos argumentos
    sub_args = strchr(sub_cmd, ' ');
    if (sub_args != NULL) {
        *sub_args = '\0'; // Termina a string do subcomando
        sub_args++;
        // Pula espaços
        while (isspace((unsigned char)*sub_args)) {
            sub_args++;
        }
        if (*sub_args == '\0') {
            sub_args = NULL;
        }
    }

    // Procura na tabela de subcomandos DWIN
    for (size_t i = 0; i < NUM_DWIN_SUBCOMMANDS; i++) {
        if (strcasecmp(sub_cmd, s_dwin_table[i].name) == 0) {
            s_dwin_table[i].handler(sub_args);
            return;
        }
    }

    CLI_Printf("Subcomando DWIN desconhecido: \"%s\"", sub_cmd);
}

/*
==================================================
  HANDLERS DE SUB-COMANDO (DWIN)
==================================================
*/

/**
 * @brief Handler do subcomando DWIN PIC.
 * @param sub_args String contendo o ID da tela.
 */
static void Handle_Dwin_PIC(char* sub_args) {
    if (sub_args == NULL) {
        CLI_Printf("Uso: DWIN PIC <id>");
        return;
    }
    DWIN_Driver_SetScreen(atoi(sub_args));
    CLI_Printf("Comando enviado: Mudar para tela ID %s", sub_args);
}

/**
 * @brief Handler do subcomando DWIN INT.
 * @param sub_args String contendo o endereço (hex) e o valor (int16).
 */
static void Handle_Dwin_INT(char* sub_args) {
    if (sub_args == NULL) {
        CLI_Printf("Uso: DWIN INT <addr_hex> <valor>");
        return;
    }

    char* val_str = NULL;
    char* addr_str = sub_args;

    val_str = strchr(addr_str, ' ');
    if (val_str == NULL) {
        CLI_Printf("Valor faltando.");
        return;
    }
    *val_str = '\0'; // Separa endereço do valor
    val_str++;

    uint16_t vp = strtol(addr_str, NULL, 16);
    int16_t val = atoi(val_str);
    DWIN_Driver_WriteInt(vp, val);
    CLI_Printf("Escrevendo (int16) %d em 0x%04X", val, vp);
}

/**
 * @brief Handler do subcomando DWIN INT32.
 * @param sub_args String contendo o endereço (hex) e o valor (int32).
 */
static void Handle_Dwin_INT32(char* sub_args) {
    if (sub_args == NULL) {
        CLI_Printf("Uso: DWIN INT32 <addr_hex> <valor>");
        return;
    }

    char* val_str = NULL;
    char* addr_str = sub_args;

    val_str = strchr(addr_str, ' ');
    if (val_str == NULL) {
        CLI_Printf("Valor faltando.");
        return;
    }
    *val_str = '\0'; // Separa endereço do valor
    val_str++;

    uint16_t vp = strtol(addr_str, NULL, 16);
    int32_t val = atol(val_str);
    DWIN_Driver_WriteInt32(vp, val);
    CLI_Printf("Escrevendo (int32) %ld em 0x%04X", (long)val, vp);
}

/**
 * @brief Converte um caractere hexadecimal para seu valor numérico.
 * @param c O caractere (ex: 'A', 'f', '3').
 * @return O valor (0-15) ou 0xFF em caso de erro.
 */
static uint8_t hex_char_to_value(char c) {
    c = toupper((unsigned char)c);
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'A' && c <= 'F') {
        return 10 + (c - 'A');
    }
    return 0xFF; // Erro
}

/**
 * @brief Handler do subcomando DWIN RAW.
 * @param sub_args String contendo os bytes em formato hexadecimal (ex: "5A A5 04 ...").
 */
static void Handle_Dwin_RAW(char* sub_args) {
    if (sub_args == NULL) {
        CLI_Printf("Uso: DWIN RAW <byte_hex> ...");
        return;
    }

    uint8_t raw_buffer[CLI_BUFFER_SIZE / 2];
    int byte_count = 0;
    char* ptr = sub_args;

    while (*ptr != '\0' && byte_count < (CLI_BUFFER_SIZE / 2)) {
        // Pula espaços em branco
        while (isspace((unsigned char)*ptr)) {
            ptr++;
        }
        if (*ptr == '\0') {
            break; // Fim da string
        }

        // Lê o nibble alto
        char high_c = *ptr++;
        if (*ptr == '\0' || isspace((unsigned char)*ptr)) {
            CLI_Printf("\nErro: Numero impar de caracteres hex.");
            return;
        }

        // Lê o nibble baixo
        char low_c = *ptr++;

        uint8_t high_v = hex_char_to_value(high_c);
        uint8_t low_v = hex_char_to_value(low_c);

        if (high_v == 0xFF || low_v == 0xFF) {
            CLI_Printf("\nErro: Caractere invalido na string hex.");
            return;
        }

        // Combina os dois nibbles em um byte
        raw_buffer[byte_count++] = (high_v << 4) | low_v;
    }

    CLI_Printf("Enviando %d bytes:", byte_count);
    for (int i = 0; i < byte_count; i++) {
        CLI_Printf(" %02X", raw_buffer[i]);
    }

    DWIN_Driver_WriteRawBytes(raw_buffer, byte_count);
}