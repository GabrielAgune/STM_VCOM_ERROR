/**
 * ============================================================================
 * @file    cli_controller.c
 * @brief   Implementação do controlador/despachante de comandos do CLI.
 *
 * Este módulo usa o driver de CLI (cli_driver) para receber linhas de texto
 * e implementa:
 *  - Parsing do comando + argumentos.
 *  - Tabelas de comandos.
 *  - Handlers para DWIN, RTC, Medição, temperatura e diagnóstico.
 * ============================================================================
 */

#include "cli_controller.h"
#include "cli_driver.h"

#include "dwin_driver.h"
#include "rtc_driver.h"
#include "medicao_handler.h"
#include "temp_sensor.h"
#include "relato.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

/* ============================================================================
 *  DECLARAÇÕES DE HANDLERS DE COMANDO
 * ========================================================================== */

typedef void (*cli_cmd_handler_t)(char* args);

typedef struct {
    const char*       name;
    cli_cmd_handler_t handler;
} cli_command_t;

static void CLI_LineHandler(const char* line);

static void Cmd_Help    (char* args);
static void Cmd_WhoAmI  (char* args);
static void Cmd_Dwin    (char* args);
static void Cmd_SetTime (char* args);
static void Cmd_SetDate (char* args);
static void Cmd_Date    (char* args);
static void Cmd_GetPeso (char* args);
static void Cmd_GetTemp (char* args);
static void Cmd_GetFreq (char* args);
static void Cmd_Service (char* args);

/* -------------------- Subcomandos DWIN -------------------- */

typedef void (*dwin_subcmd_handler_t)(char* args);

typedef struct {
    const char*           name;
    dwin_subcmd_handler_t handler;
} dwin_subcommand_t;

static void Handle_Dwin_PIC   (char* sub_args);
static void Handle_Dwin_INT   (char* sub_args);
static void Handle_Dwin_INT32 (char* sub_args);
static void Handle_Dwin_RAW   (char* sub_args);

static uint8_t hex_char_to_value(char c);

/* ============================================================================
 *  TABELAS
 * ========================================================================== */

static const cli_command_t s_command_table[] = {
    { "HELP",     Cmd_Help     },
    { "?",        Cmd_Help     },
    { "DWIN",     Cmd_Dwin     },
    { "SETTIME",  Cmd_SetTime  },
    { "SETDATE",  Cmd_SetDate  },
    { "DATE",     Cmd_Date     },
    { "PESO",     Cmd_GetPeso  },
    { "TEMP",     Cmd_GetTemp  },
    { "FREQ",     Cmd_GetFreq  },
    { "SERVICE",  Cmd_Service  },
    { "WHO_AM_I", Cmd_WhoAmI   },
};

static const size_t NUM_COMMANDS =
    sizeof(s_command_table) / sizeof(s_command_table[0]);

static const dwin_subcommand_t s_dwin_table[] = {
    { "PIC",   Handle_Dwin_PIC   },
    { "INT",   Handle_Dwin_INT   },
    { "INT32", Handle_Dwin_INT32 },
    { "RAW",   Handle_Dwin_RAW   },
};

static const size_t NUM_DWIN_SUBCOMMANDS =
    sizeof(s_dwin_table) / sizeof(s_dwin_table[0]);

/* ============================================================================
 *  TEXTO DE AJUDA
 * ========================================================================== */

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

/* ============================================================================
 *  FUNÇÕES PÚBLICAS
 * ========================================================================== */

void CLI_Controller_Init(void) {
    CLI_Init(CLI_LineHandler);
    CLI_Puts("\r\n> ");
}

/* ============================================================================
 *  CALLBACK DE LINHA (PONTO CENTRAL DO DESPACHANTE)
 * ========================================================================== */

static void CLI_LineHandler(const char* line) {
    if (!line) {
        return;
    }

    // Copia para um buffer mutável, pois vamos tokenizar
    char buffer[128];
    strncpy(buffer, line, sizeof(buffer) - 1u);
    buffer[sizeof(buffer) - 1u] = '\0';

    char* command_str = buffer;
    while (isspace((unsigned char)*command_str)) {
        command_str++;
    }

    if (*command_str == '\0') {
        // linha vazia -> apenas novo prompt
        CLI_Puts("\r\n> ");
        return;
    }

    char* args = strchr(command_str, ' ');
    if (args) {
        *args++ = '\0';
        while (isspace((unsigned char)*args)) {
            args++;
        }
        if (*args == '\0') {
            args = NULL;
        }
    }

    for (size_t i = 0; i < NUM_COMMANDS; i++) {
        if (strcasecmp(command_str, s_command_table[i].name) == 0) {
            CLI_Printf("\r\n");   // quebra de linha antes da resposta
            s_command_table[i].handler(args);
            CLI_Puts("\r\n> ");   // prompt após comando
            return;
        }
    }

    CLI_Printf("\r\nComando desconhecido: \"%s\".", command_str);
    CLI_Puts("\r\n> ");
}

/* ============================================================================
 *  HANDLERS DE COMANDOS PRINCIPAIS
 * ========================================================================== */

static void Cmd_Help(char* args) {
    (void)args;
    CLI_Puts(HELP_TEXT);
}

static void Cmd_WhoAmI(char* args) {
    (void)args;
    Who_am_i();
}

static void Cmd_Service(char* args) {
    (void)args;
    DWIN_Driver_SetScreen(TELA_SERVICO);
}

static void Cmd_SetTime(char* args) {
    if (!args) {
        CLI_Puts("Uso: SETTIME HH:MM:SS");
        return;
    }

    uint8_t h, m, s;
    if (sscanf(args, "%hhu:%hhu:%hhu", &h, &m, &s) == 3 &&
        h < 24u && m < 60u && s < 60u) {

        if (RTC_Driver_SetTime(h, m, s)) {
            CLI_Printf("OK. RTC atualizado para %02u:%02u:%02u", h, m, s);
        } else {
            CLI_Puts("Erro ao setar a hora no hardware do RTC.");
        }
    } else {
        CLI_Puts("Formato invalido. Uso: SETTIME HH(0-23):MM(0-59):SS(0-59).");
    }
}

static void Cmd_SetDate(char* args) {
    if (!args) {
        CLI_Puts("Uso: SETDATE DD/MM/YY");
        return;
    }

    uint8_t d, m, a;
    if (sscanf(args, "%hhu/%hhu/%hhu", &d, &m, &a) == 3 &&
        d >= 1u && d <= 31u &&
        m >= 1u && m <= 12u) {

        if (RTC_Driver_SetDate(d, m, a)) {
            CLI_Printf("OK. RTC atualizado para %02u/%02u/%02u", d, m, a);
        } else {
            CLI_Puts("Erro ao setar a data no hardware do RTC.");
        }
    } else {
        CLI_Puts("Formato invalido. Uso: SETDATE DD(1-31)/MM(1-12)/YY(00-99).");
    }
}

static void Cmd_Date(char* args) {
    (void)args;
    uint8_t h, m, s, d, mo, y;
    char weekday_str[4] = {0};

    const bool time_ok = RTC_Driver_GetTime(&h, &m, &s);
    const bool date_ok = RTC_Driver_GetDate(&d, &mo, &y, weekday_str);

    if (time_ok && date_ok) {
        CLI_Printf("Data/Hora: %s %02u/%02u/20%02u %02u:%02u:%02u",
                   weekday_str, d, mo, y, h, m, s);
    } else {
        CLI_Puts("Erro ao ler data/hora do RTC.");
    }
}

static void Cmd_GetPeso(char* args) {
    (void)args;
    DadosMedicao_t dados;
    Medicao_Get_UltimaMedicao(&dados);
    CLI_Printf("Peso: %.2f g\r\n", dados.Peso);
}

static void Cmd_GetTemp(char* args) {
    (void)args;
    const float temperatura = TempSensor_GetTemperature();
    CLI_Printf("Temperatura interna do MCU: %.2f C\r\n", temperatura);
}

static void Cmd_GetFreq(char* args) {
    (void)args;
    DadosMedicao_t dados;
    Medicao_Get_UltimaMedicao(&dados);
    CLI_Puts("Dados de Frequencia:\r\n");
    CLI_Printf("  Pulsos (1s): %.1f\r\n", dados.Frequencia);
    CLI_Printf("  Escala A: %.2f\r\n", dados.Escala_A);
}

/* ============================================================================
 *  COMANDO DWIN E SUBCOMANDOS
 * ========================================================================== */

static void Cmd_Dwin(char* args) {
    if (!args) {
        CLI_Puts("Uso: DWIN <PIC|INT|INT32|RAW> ... (veja HELP)");
        return;
    }

    char* sub_cmd  = args;
    char* sub_args = strchr(sub_cmd, ' ');
    if (sub_args) {
        *sub_args++ = '\0';
        while (isspace((unsigned char)*sub_args)) {
            sub_args++;
        }
        if (*sub_args == '\0') {
            sub_args = NULL;
        }
    }

    for (size_t i = 0; i < NUM_DWIN_SUBCOMMANDS; i++) {
        if (strcasecmp(sub_cmd, s_dwin_table[i].name) == 0) {
            s_dwin_table[i].handler(sub_args);
            return;
        }
    }

    CLI_Printf("Subcomando DWIN desconhecido: \"%s\"", sub_cmd);
}

/* -------------------- Subcomandos DWIN -------------------- */

static void Handle_Dwin_PIC(char* sub_args) {
    if (!sub_args) {
        CLI_Puts("Uso: DWIN PIC <id>");
        return;
    }

    const int screen_id = atoi(sub_args);
    DWIN_Driver_SetScreen((uint16_t)screen_id);
    CLI_Printf("Tela alterada para ID %d", screen_id);
}

static void Handle_Dwin_INT(char* sub_args) {
    if (!sub_args) {
        CLI_Puts("Uso: DWIN INT <addr_hex> <valor>");
        return;
    }

    char* val_str  = strchr(sub_args, ' ');
    if (!val_str) {
        CLI_Puts("Valor faltando. Uso: DWIN INT <addr_hex> <valor>");
        return;
    }

    *val_str++ = '\0';

    const uint16_t vp  = (uint16_t)strtoul(sub_args, NULL, 16);
    const int16_t  val = (int16_t)atoi(val_str);

    DWIN_Driver_WriteInt(vp, val);
    CLI_Printf("Escrevendo (int16) %d em 0x%04X", val, vp);
}

static void Handle_Dwin_INT32(char* sub_args) {
    if (!sub_args) {
        CLI_Puts("Uso: DWIN INT32 <addr_hex> <valor>");
        return;
    }

    char* val_str  = strchr(sub_args, ' ');
    if (!val_str) {
        CLI_Puts("Valor faltando. Uso: DWIN INT32 <addr_hex> <valor>");
        return;
    }

    *val_str++ = '\0';

    const uint16_t vp  = (uint16_t)strtoul(sub_args, NULL, 16);
    const int32_t  val = (int32_t)atol(val_str);

    DWIN_Driver_WriteInt32(vp, val);
    CLI_Printf("Escrevendo (int32) %ld em 0x%04X", (long)val, vp);
}

static uint8_t hex_char_to_value(char c) {
    c = (char)toupper((unsigned char)c);
    if (c >= '0' && c <= '9') {
        return (uint8_t)(c - '0');
    }
    if (c >= 'A' && c <= 'F') {
        return (uint8_t)(10 + (c - 'A'));
    }
    return 0xFFu;
}

static void Handle_Dwin_RAW(char* sub_args) {
    if (!sub_args) {
        CLI_Puts("Uso: DWIN RAW <byte_hex> ...");
        return;
    }

    uint8_t raw_buffer[64];  // 64 bytes já é bastante para debug
    int     byte_count = 0;
    char*   ptr        = sub_args;

    while (*ptr && byte_count < (int)sizeof(raw_buffer)) {

        while (isspace((unsigned char)*ptr)) {
            ptr++;
        }
        if (!*ptr) {
            break;
        }

        char high_c = *ptr++;
        if (!*ptr || isspace((unsigned char)*ptr)) {
            CLI_Puts("\r\nErro: Numero impar de caracteres hex.");
            return;
        }
        char low_c = *ptr++;

        const uint8_t high_v = hex_char_to_value(high_c);
        const uint8_t low_v  = hex_char_to_value(low_c);

        if (high_v == 0xFFu || low_v == 0xFFu) {
            CLI_Puts("\r\nErro: Caractere invalido na string hex.");
            return;
        }

        raw_buffer[byte_count++] = (uint8_t)((high_v << 4) | low_v);
    }

    CLI_Printf("Enviando %d bytes:", byte_count);
    for (int i = 0; i < byte_count; i++) {
        CLI_Printf(" %02X", raw_buffer[i]);
    }

    DWIN_Driver_WriteRawBytes(raw_buffer, (uint16_t)byte_count);
}