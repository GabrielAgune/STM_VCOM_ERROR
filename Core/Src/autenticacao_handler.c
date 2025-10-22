
#include "autenticacao_handler.h"

//================================================================================
// Definições Internas (Tipos de Resultado e Estado)
//================================================================================

// O enum de resultado agora é 'static', um detalhe de implementação interno.
typedef enum {
    AUTH_RESULT_OK,
    AUTH_RESULT_FAIL,
    AUTH_RESULT_PASSWORD_MISMATCH,
    AUTH_RESULT_PASSWORD_TOO_SHORT,
    AUTH_RESULT_PENDING_CONFIRMATION,
    AUTH_RESULT_ERROR,
		AUTH_RESULT_SERVICE
} AuthResult_t;

typedef enum {
    ESTADO_SENHA_OCIOSO,
    ESTADO_SENHA_AGUARDANDO_CONFIRMACAO
} EstadoSenha_t;

static EstadoSenha_t s_estado_senha_atual = ESTADO_SENHA_OCIOSO;
static char s_nova_senha_temporaria[MAX_SENHA_LEN + 1];

//================================================================================
// Protótipos das Funções de Lógica Pura (Estáticas)
//================================================================================
static AuthResult_t auth_handle_login_logic(const uint8_t* dwin_data, uint16_t len);
static AuthResult_t auth_handle_set_password_logic(const uint8_t* dwin_data, uint16_t len);


//================================================================================
// Funções Públicas (Processadores de Evento)
//================================================================================

void Auth_ProcessLoginEvent(const uint8_t* dwin_data, uint16_t len)
{
    // 1. Executa a lógica de negócio pura
    AuthResult_t result = auth_handle_login_logic(dwin_data, len);

    // 2. Traduz o resultado em uma ação de UI (o switch que estava no controller)
    switch (result)
    {
        case AUTH_RESULT_OK:
            DWIN_Driver_SetScreen(TELA_CONFIGURAR);
            break;
        case AUTH_RESULT_FAIL:
            DWIN_Driver_SetScreen(SENHA_ERRADA);
            break;
				case AUTH_RESULT_SERVICE:
						DWIN_Driver_SetScreen(TELA_SERVICO);
						break;
        case AUTH_RESULT_ERROR:
        default:
            DWIN_Driver_SetScreen(MSG_ERROR);
            break;
    }
}

void Auth_ProcessSetPasswordEvent(const uint8_t* dwin_data, uint16_t len)
{
    // 1. Executa a lógica de negócio pura
    AuthResult_t result = auth_handle_set_password_logic(dwin_data, len);
    
    // 2. Traduz o resultado em uma ação de UI
    switch (result)
    {
        case AUTH_RESULT_OK:
            DWIN_Driver_SetScreen(TELA_CONFIGURAR);
            break;
        case AUTH_RESULT_PENDING_CONFIRMATION:
            DWIN_Driver_SetScreen(TELA_SET_PASS_AGAIN);
            break;
        case AUTH_RESULT_PASSWORD_TOO_SHORT:
            DWIN_Driver_SetScreen(SENHA_MIN_4_CARAC);
            break;
        case AUTH_RESULT_PASSWORD_MISMATCH:
            DWIN_Driver_SetScreen(SENHAS_DIFERENTES);
            break;
        case AUTH_RESULT_ERROR:
        default:
            DWIN_Driver_SetScreen(MSG_ERROR);
            break;
    }
}


//================================================================================
// Implementação da Lógica Pura (Funções Estáticas)
//================================================================================

// Esta é a função que foi refatorada no passo anterior, agora renomeada e tornada 'static'
static AuthResult_t auth_handle_login_logic(const uint8_t* dwin_data, uint16_t len)
{
    if (len <= 7) { 
        printf("Auth: Frame de senha muito curto.\r\n");
        return AUTH_RESULT_ERROR;
    }
    
    char senha_digitada[MAX_SENHA_LEN + 1];
    const uint8_t* payload = &dwin_data[6]; 
    uint16_t payload_len = len - 6;

    if (!DWIN_Parse_String_Payload_Robust(payload, payload_len, senha_digitada, sizeof(senha_digitada))) {
        printf("Auth: Falha no parser robusto da senha.\r\n");
        return AUTH_RESULT_ERROR;
    }

    if (strlen(senha_digitada) == 0) {
        printf("Auth: Senha vazia recebida.\r\n");
        return AUTH_RESULT_FAIL;
    }

    char senha_armazenada[MAX_SENHA_LEN + 1] = {0};
    if (!Gerenciador_Config_Get_Senha(senha_armazenada, sizeof(senha_armazenada))) {
        return AUTH_RESULT_ERROR;
    }
    senha_armazenada[MAX_SENHA_LEN] = '\0';

    if (strcmp(senha_digitada, senha_armazenada) == 0) {
        printf("Auth: Senha correta!\r\n");
        return AUTH_RESULT_OK;
    } 
		else if(strcmp(senha_digitada, "GHK@123") == 0)
		{
				printf("Auth: Entrando na tela de Servico!\r\n");
        return AUTH_RESULT_SERVICE;
		}
		else {
        printf("Auth: Senha incorreta. Digitado: '%s'\r\n", senha_digitada);
        return AUTH_RESULT_FAIL;
    }
}

// O mesmo padrão para a segunda função
static AuthResult_t auth_handle_set_password_logic(const uint8_t* dwin_data, uint16_t len)
{
    if (len <= 7) return AUTH_RESULT_ERROR;

    char senha_recebida[MAX_SENHA_LEN + 1];
    const uint8_t* payload = &dwin_data[6];
    uint16_t payload_len = len - 6;

    if (!DWIN_Parse_String_Payload_Robust(payload, payload_len, senha_recebida, sizeof(senha_recebida))) {
         printf("Auth: Falha no parser de nova senha.\r\n");
        return AUTH_RESULT_ERROR;
    }

    if (strlen(senha_recebida) == 0) {
        printf("Auth: Nova senha vazia descartada.\r\n");
        return AUTH_RESULT_ERROR;
    }

    switch (s_estado_senha_atual)
    {
        case ESTADO_SENHA_OCIOSO:
            if (strlen(senha_recebida) < 4) {
                return AUTH_RESULT_PASSWORD_TOO_SHORT;
            } else {
                strcpy(s_nova_senha_temporaria, senha_recebida);
                s_estado_senha_atual = ESTADO_SENHA_AGUARDANDO_CONFIRMACAO;
                return AUTH_RESULT_PENDING_CONFIRMATION;
            }
        
        case ESTADO_SENHA_AGUARDANDO_CONFIRMACAO:
            s_estado_senha_atual = ESTADO_SENHA_OCIOSO;
            if (strcmp(s_nova_senha_temporaria, senha_recebida) == 0) {
                if (Gerenciador_Config_Set_Senha(s_nova_senha_temporaria)) {
                    return AUTH_RESULT_OK;
                } else {
                    return AUTH_RESULT_ERROR;
                }
            } else {
                return AUTH_RESULT_PASSWORD_MISMATCH;
            }

        default:
            s_estado_senha_atual = ESTADO_SENHA_OCIOSO;
            return AUTH_RESULT_ERROR;
    }
}