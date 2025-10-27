
#include "rtc_handler.h"
#include "controller.h"

//================================================================================
// Definições Internas
//================================================================================

typedef enum {
    RTC_SET_OK,
    RTC_SET_FAIL_PARSE,
    RTC_SET_FAIL_HW
} RtcSetResult_t;

typedef struct {
    uint8_t day;
    uint8_t month;
    uint8_t year;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} RtcData_t;


//================================================================================
// Protótipos das Funções de Lógica Pura (Estáticas)
//================================================================================

static RtcSetResult_t rtc_handle_set_date_and_time_logic(const uint8_t* dwin_data, uint16_t len, RtcData_t* out_data);
static RtcSetResult_t rtc_handle_set_time_logic(const uint8_t* dwin_data, uint16_t len, RtcData_t* out_data);

//================================================================================
// Funções Públicas (Processadores de Evento)
//================================================================================

void RTC_Handle_Set_Time(const uint8_t* dwin_data, uint16_t len, uint16_t received_value)
{
		if (received_value == 0x0050)
		{
				Controller_SetScreen(TELA_SET_JUST_TIME);
		}
		else
		{
				RtcData_t parsed_data;

				// 1. Chama a NOVA função de lógica que só mexe na hora
				RtcSetResult_t result = rtc_handle_set_time_logic(dwin_data, len, &parsed_data);

				// 2. Age sobre o resultado
				if (result == RTC_SET_OK) {
						printf("RTC Handler: HORA atualizada com sucesso. Atualizando display.\r\n");
				} else {
						printf("RTC Handler: Falha ao atualizar HORA. Nenhum feedback para o usuario.\r\n");
				}
		}
}

void RTC_Handle_Set_Date_And_Time(const uint8_t* dwin_data, uint16_t len, uint16_t received_value)
{

		if (received_value == 0x0050)
		{
				Controller_SetScreen(TELA_ADJUST_TIME);
		}
		else
		{
				RtcData_t parsed_data;
		
				RtcSetResult_t result = rtc_handle_set_date_and_time_logic(dwin_data, len, &parsed_data);

				if (result == RTC_SET_OK)
				{
						printf("RTC Handler: RTC atualizado com sucesso. Atualizando display.\r\n");
				}
				else
				{
						printf("RTC Handler: Falha ao atualizar RTC. Nenhum feedback para o usuario.\r\n");
				}
		}
}


//================================================================================
// Implementação da Lógica Pura e Ações de UI (Funções Estáticas)
//================================================================================

static RtcSetResult_t rtc_handle_set_date_and_time_logic(const uint8_t* dwin_data, uint16_t len, RtcData_t* out_data)
{
    char parsed_string[32] = {0};
    
    // 1. Extrair a string do payload DWIN
    const uint8_t* payload = &dwin_data[6];
    uint16_t payload_len = len - 6;
    if (!DWIN_Parse_String_Payload_Robust(payload, payload_len, parsed_string, sizeof(parsed_string))) {
        printf("RTC Logic: Falha ao extrair string.\r\n");
        return RTC_SET_FAIL_PARSE;
    }
    printf("RTC Logic: Recebido string '%s'\r\n", parsed_string);

    // 2. Tentar extrair data e hora da string
    uint8_t d=0, m=0, y=0, h=0, min=0, s=0;
		char weekday_dummy[4];
    bool date_found = false;
    bool time_found = false;

    if (sscanf(parsed_string, "%hhu/%hhu/%hhu %hhu:%hhu:%hhu", &d, &m, &y, &h, &min, &s) == 6) {
        date_found = true; time_found = true;
    } else if (sscanf(parsed_string, "%hhu/%hhu/%hhu", &d, &m, &y) == 3) {
        date_found = true;
    } else if (sscanf(parsed_string, "%hhu:%hhu:%hhu", &h, &min, &s) == 3) {
        time_found = true;
    }

    if (!date_found && !time_found) {
        printf("RTC Logic: Formato de string irreconhecivel.\r\n");
        return RTC_SET_FAIL_PARSE;
    }

    // 3. Se a atualização for parcial, busca os dados atuais do hardware
    if (!time_found) { RTC_Driver_GetTime(&h, &min, &s); }
    if (!date_found) { RTC_Driver_GetDate(&d, &m, &y, weekday_dummy); }

    // 4. Aplica as novas configurações ao hardware
    if (date_found) {
        if (!RTC_Driver_SetDate(d, m, y)) return RTC_SET_FAIL_HW;
    }
    if (time_found) {
        if (!RTC_Driver_SetTime(h, min, s)) return RTC_SET_FAIL_HW;
    }
    
    // 5. Preenche a struct de saída para a camada de UI usar
    out_data->day = d; out_data->month = m; out_data->year = y;
    out_data->hour = h; out_data->minute = min; out_data->second = s;
    
    return RTC_SET_OK;
}

static RtcSetResult_t rtc_handle_set_time_logic(const uint8_t* dwin_data, uint16_t len, RtcData_t* out_data)
{
    char parsed_string[32] = {0};

    // CORREÇÃO: O payload da string começa no índice 8.
    const uint8_t* payload = &dwin_data[8];
    uint16_t payload_len = len - 8;
    if (!DWIN_Parse_String_Payload_Robust(payload, payload_len, parsed_string, sizeof(parsed_string))) {
        printf("RTC Logic (TimeOnly): Falha ao extrair string.\r\n");
        return RTC_SET_FAIL_PARSE;
    }
    printf("RTC Logic (TimeOnly): Recebido string '%s'\r\n", parsed_string);

    uint8_t h=0, min=0, s=0;
		

    // Tenta extrair APENAS a hora da string
    if (sscanf(parsed_string, "%hhu:%hhu:%hhu", &h, &min, &s) != 3) {
        printf("RTC Logic (TimeOnly): Formato de string invalido. Esperado HH:MM:SS.\r\n");
        return RTC_SET_FAIL_PARSE;
    }

    // Aplica a nova hora ao hardware
    if (!RTC_Driver_SetTime(h, min, s)) {
        return RTC_SET_FAIL_HW;
    }

    // Busca a data atual do hardware para poder atualizar o display corretamente
    uint8_t d, m, y;
		char weekday_dummy[4];
    if (!RTC_Driver_GetDate(&d, &m, &y, weekday_dummy)) {
        return RTC_SET_FAIL_HW; // Se não conseguir ler a data, retorna erro.
    }
    
    // Preenche a struct de saída com a data atual e a nova hora
    out_data->day = d; out_data->month = m; out_data->year = y;
    out_data->hour = h; out_data->minute = min; out_data->second = s;

    return RTC_SET_OK;
}