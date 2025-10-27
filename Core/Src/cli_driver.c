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
#include <stdarg.h> // Essencial para a função CLI_Printf

// Inclui o header que contém o protótipo da função de transmissão USB
#include "ux_device_cdc_acm.h"

// Definições
#define CLI_BUFFER_SIZE 128

// Protótipos de Funções de Comando
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
static void Handle_Dwin_PIC(char* sub_args);
static void Handle_Dwin_INT(char* sub_args);
static void Handle_Dwin_INT32(char* sub_args);
static void Handle_Dwin_RAW(char* sub_args);
static uint8_t hex_char_to_value(char c);

// Variáveis Estáticas
static char s_cli_buffer[CLI_BUFFER_SIZE];
static uint16_t s_cli_buffer_index = 0;
static bool s_command_ready = false;

// Tabela de Comandos Principal
typedef struct { const char* name; void (*handler)(char* args); } cli_command_t;
static const cli_command_t s_command_table[] = {
    { "HELP", Cmd_Help }, { "?", Cmd_Help }, { "DWIN", Cmd_Dwin },
		{ "SETTIME", Cmd_SetTime }, { "SETDATE", Cmd_SetDate }, { "DATE", Cmd_Date },
		{ "PESO", Cmd_GetPeso}, {"TEMP", Cmd_GetTemp}, {"FREQ", Cmd_GetFreq},
		{ "SERVICE", Cmd_Service }, { "WHO_AM_I", Cmd_WhoAmI },
};
static const size_t NUM_COMMANDS = sizeof(s_command_table) / sizeof(s_command_table[0]);

// Tabela de Subcomandos DWIN
typedef struct { const char* name; void (*handler)(char* args); } dwin_subcommand_t;
static const dwin_subcommand_t s_dwin_table[] = {
    { "PIC", Handle_Dwin_PIC }, { "INT", Handle_Dwin_INT },
    { "INT32", Handle_Dwin_INT32 }, { "RAW", Handle_Dwin_RAW }
};
static const size_t NUM_DWIN_SUBCOMMANDS = sizeof(s_dwin_table) / sizeof(s_dwin_table[0]);

static const char HELP_TEXT[] =
"\r\n====================== CLI de Teste DWIN & RTC =========================\r\n"
    "| HELP ou ?                | Mostra esta ajuda.                        |\r\n"
    "| DWIN PIC <id>            | Muda a tela (ex: DWIN PIC 1).             |\r\n"
    "| DWIN INT <addr> <val>    | Escreve int16 (ex: DWIN INT 1500 -10).    |\r\n"
    "| DWIN INT32 <addr> <val>  | Escreve int32 (ex: DWIN INT32 1500 40500) |\r\n"
    "| DWIN RAW <hex...>        | Envia bytes hex (ex: DWIN RAW 5A A5...).  |\r\n"
    "| SETTIME HH:MM:SS         | Ajusta a hora do RTC.                     |\r\n"
    "| SETDATE DD/MM/YY         | Ajusta a data do RTC.                     |\r\n"
    "| DATE                     | Mostra a data e hora atuais.              |\r\n"
		"| SERVICE                  | Entra na tela de servico.                 |\r\n"
		"| PESO                     | Mostra a leitura atual da balanca.        |\r\n"
    "| TEMP                     | Mostra a leitura do sensor de temperatura.|\r\n"
    "| FREQ                     | Mostra a ultima leitura de frequencia.    |\r\n"
    "========================================================================\r\n";

// ============================================================================
//               IMPLEMENTAÇÃO DA NOVA FUNÇÃO CLI_Printf
// ============================================================================
void CLI_Printf(const char* format, ...)
{
    static char printf_buffer[256];
    va_list args;
    va_start(args, format);
    int len = vsnprintf(printf_buffer, sizeof(printf_buffer), format, args);
    va_end(args);
    
    if (len > 0)
    {
        uint32_t bytes_sent_dummy;
        USBD_CDC_ACM_Transmit((uint8_t*)printf_buffer, len, &bytes_sent_dummy);
    }
}

void CLI_Puts(const char* str)
{
    if (str == NULL) return;

    uint32_t len = strlen(str);
    if (len > 0)
    {
        uint32_t bytes_sent_dummy;
        USBD_CDC_ACM_Transmit((uint8_t*)str, len, &bytes_sent_dummy);
    }
}
// --- Resto do módulo usando a nova função CLI_Printf ---

void CLI_Init(void) {
    CLI_Printf("\r\nCLI Pronta. Digite 'HELP' para comandos.\r\n> ");
}

void CLI_Process(void) {
    if (s_command_ready) {
        CLI_Printf("\r\n");
        Process_Command();
        memset(s_cli_buffer, 0, CLI_BUFFER_SIZE);
        s_cli_buffer_index = 0;
        s_command_ready = false;
        CLI_Printf("\r\n> ");
    }
}

void CLI_Receive_Char(uint8_t received_char) {
    if (s_command_ready) return;

    if (received_char == '\r' || received_char == '\n') {
        if (s_cli_buffer_index > 0) {
            s_cli_buffer[s_cli_buffer_index] = '\0';
            s_command_ready = true;
        } else {
            CLI_Printf("\r\n> ");
        }
    } else if (received_char == '\b' || received_char == 127) { // Backspace
        if (s_cli_buffer_index > 0) {
            s_cli_buffer_index--;
            CLI_Printf("\b \b");
        }
    } else if (s_cli_buffer_index < (CLI_BUFFER_SIZE - 1) && isprint(received_char)) {
        s_cli_buffer[s_cli_buffer_index++] = received_char;
        CLI_Printf("%c", received_char); 
    }
}

static void Process_Command(void) {
    char* command_str = s_cli_buffer;
    char* args = NULL;

    while (isspace((unsigned char)*command_str)) command_str++;

    args = strchr(command_str, ' ');
    if (args != NULL) {
        *args = '\0';
        args++;
        while (isspace((unsigned char)*args)) args++;
        if (*args == '\0') args = NULL;
    }

    if (*command_str == '\0') return;

    for (size_t i = 0; i < NUM_COMMANDS; i++) {
        if (strcasecmp(command_str, s_command_table[i].name) == 0) {
            s_command_table[i].handler(args);
            return;
        }
    }
    CLI_Printf("Comando desconhecido: \"%s\".", command_str);
}

static void Cmd_Help(char* args) { 
    CLI_Puts(HELP_TEXT); 
}

static void Cmd_WhoAmI(char* args) {
    Who_am_i();
}

static void Cmd_Service(char* args)
{
	DWIN_Driver_SetScreen(TELA_SERVICO);
}

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

static void Cmd_SetDate(char* args) {
    if (args == NULL) {
        CLI_Printf("Erro: Faltam argumentos. Uso: SETDATE DD/MM/YY");
        return;
    }
    uint8_t d, m, a;
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

static void Cmd_Date(char* args) {
    // Estas variáveis irão guardar os valores lidos do RTC
    uint8_t h, m, s, d, mo, y;
    char weekday_str[4]; // "SEG", "TER", etc.

    // Chama as funções do seu driver RTC para obter os dados atuais
    bool time_ok = RTC_Driver_GetTime(&h, &m, &s);
    bool date_ok = RTC_Driver_GetDate(&d, &mo, &y, weekday_str);

    if (time_ok && date_ok) {
        CLI_Printf("Data/Hora: %s %02u/%02u/20%02u %02u:%02u:%02u", weekday_str, d, mo, y, h, m, s);
    } else {
        CLI_Printf("Erro: Nao foi possivel ler a data/hora do RTC.");
    }
}

static void Cmd_GetPeso(char* args) {
    DadosMedicao_t dados_atuais;
    Medicao_Get_UltimaMedicao(&dados_atuais);
    CLI_Printf("Dados da Balanca:\r\n  - Peso: %.2f g\r\n", dados_atuais.Peso);
}

static void Cmd_GetTemp(char* args) {
    float temperatura = TempSensor_GetTemperature();
    CLI_Printf("Temperatura interna do MCU: %.2f C\r\n", temperatura);
}

static void Cmd_GetFreq(char* args) {
    DadosMedicao_t dados_atuais;
    Medicao_Get_UltimaMedicao(&dados_atuais);
    CLI_Printf("Dados de Frequencia:\r\n");
    CLI_Printf("  - Pulsos (em 1s): %.1f\r\n", dados_atuais.Frequencia);
    CLI_Printf("  - Escala A (calc): %.2f\r\n", dados_atuais.Escala_A);
}


static void Cmd_Dwin(char* args) {
    if (args == NULL) { 
        CLI_Printf("Subcomando DWIN faltando. Use 'HELP'."); 
        return; 
    }
    
    char* sub_cmd = args;
    char* sub_args = NULL;
    
    sub_args = strchr(sub_cmd, ' ');
    if (sub_args != NULL) {
        *sub_args = '\0';
        sub_args++;
        while (isspace((unsigned char)*sub_args)) sub_args++;
        if (*sub_args == '\0') sub_args = NULL;
    }

    for (size_t i = 0; i < NUM_DWIN_SUBCOMMANDS; i++) {
        if (strcasecmp(sub_cmd, s_dwin_table[i].name) == 0) {
            s_dwin_table[i].handler(sub_args);
            return;
        }
    }
    CLI_Printf("Subcomando DWIN desconhecido: \"%s\"", sub_cmd);
}

static void Handle_Dwin_PIC(char* sub_args) {
    if (sub_args == NULL) { 
        CLI_Printf("Uso: DWIN PIC <id>"); 
        return; 
    }
    DWIN_Driver_SetScreen(atoi(sub_args));
    CLI_Printf("Comando enviado: Mudar para tela ID %s", sub_args);
}

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
    *val_str = '\0'; 
    val_str++;

    uint16_t vp = strtol(addr_str, NULL, 16);
    int16_t val = atoi(val_str);
    DWIN_Driver_WriteInt(vp, val);
    CLI_Printf("Escrevendo (int16) %d em 0x%04X", val, vp);
}

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
    *val_str = '\0'; 
    val_str++;

    uint16_t vp = strtol(addr_str, NULL, 16);
    int32_t val = atol(val_str);
    DWIN_Driver_WriteInt32(vp, val);
    CLI_Printf("Escrevendo (int32) %ld em 0x%04X", (long)val, vp);
}

static uint8_t hex_char_to_value(char c) {
    c = toupper((unsigned char)c);
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return 0xFF; // Erro
}

static void Handle_Dwin_RAW(char* sub_args) {
    if (sub_args == NULL) { 
        CLI_Printf("Uso: DWIN RAW <byte_hex> ..."); 
        return; 
    }
    uint8_t raw_buffer[CLI_BUFFER_SIZE / 2];
    int byte_count = 0;
    char* ptr = sub_args;
    while (*ptr != '\0' && byte_count < (CLI_BUFFER_SIZE / 2)) {
        while (isspace((unsigned char)*ptr)) ptr++;
        if (*ptr == '\0') break;
        char high_c = *ptr++;
        if (*ptr == '\0' || isspace((unsigned char)*ptr)) { 
            CLI_Printf("\nErro: Numero impar de caracteres hex."); 
            return; 
        }
        char low_c = *ptr++;
        uint8_t high_v = hex_char_to_value(high_c);
        uint8_t low_v = hex_char_to_value(low_c);
        if (high_v == 0xFF || low_v == 0xFF) { 
            CLI_Printf("\nErro: Caractere invalido na string hex."); 
            return; 
        }
        raw_buffer[byte_count++] = (high_v << 4) | low_v;
    }
    CLI_Printf("Enviando %d bytes:", byte_count);
    for(int i = 0; i < byte_count; i++) {
        CLI_Printf(" %02X", raw_buffer[i]);
    }
    DWIN_Driver_WriteRawBytes(raw_buffer, byte_count);
}