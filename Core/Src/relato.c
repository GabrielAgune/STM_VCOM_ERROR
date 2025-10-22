#include "relato.h"
#include "gerenciador_configuracoes.h"
#include "medicao_handler.h"
#include "rtc_driver.h"
#include "cli_driver.h"
#include <stdio.h>

// REMOVIDO: A variável global foi eliminada para economizar RAM estática.
// Config_Aplicacao_t config_snapshot;

// Constantes para formatação (colocadas em Flash/ROM, não consomem RAM)
const char Ejeta[] = "================================\n\r\n\r\n\r\n\r";
const char Dupla[] = "\n\r================================\n\r";
const char Linha[] = "--------------------------------\n\r";


void Who_am_i(void)
{
    // MOVIDO: de global para local. A memória é alocada na stack e liberada ao sair.
    Config_Aplicacao_t config_snapshot;
	  Gerenciador_Config_Get_Config_Snapshot(&config_snapshot);
		
    CLI_Printf(Dupla);
    CLI_Printf("         G620_Teste_Gab\n\r");
    CLI_Printf("     (c) GEHAKA, 2004-2025\n\r");
    CLI_Printf(Linha);
    CLI_Printf("CPU      =           STM32C071RB\n\r");
    CLI_Printf("Firmware = %21s\r\n", FIRMWARE);
    CLI_Printf("Hardware = %21s\r\n", HARDWARE);
    CLI_Printf("Serial   = %21s\r\n", config_snapshot.nr_serial);
    CLI_Printf(Linha);
    CLI_Printf("Medidas  = %21d\n\r", 22);
    CLI_Printf(Ejeta);
}

void Assinatura(void)
{
    uint8_t hours, minutes, seconds;
    uint8_t day, month, year;
    // CORRIGIDO: Adicionado o 4º argumento dummy para a chamada de função.
    char weekday_dummy[4];

    printf("\n\r\n\r");
    printf(Linha);
    if (RTC_Driver_GetTime(&hours, &minutes, &seconds))
    {
        printf("Assinatura              %02d:%02d:%02d\n\r", hours, minutes, seconds);
    }
    if (RTC_Driver_GetDate(&day, &month, &year, weekday_dummy))
    {
        printf("Responsavel             %02d/%02d/%02d\n\r", day, month, year);
    }
    printf ("\n\r\n\r\n\r\n\r");
}

void Cabecalho(void)
{
    // MOVIDO: de global para local.
    Config_Aplicacao_t config_snapshot;
	Gerenciador_Config_Get_Config_Snapshot(&config_snapshot);

    printf(Dupla);
 	printf("GEHAKA            G620_Teste_Gab\n\r");
    printf(Linha);
	printf("Versao Firmware= %15s\n\r", FIRMWARE);
 	printf("Numero de Serie= %15s\n\r", config_snapshot.nr_serial);
    printf(Linha);
}

void Relatorio_Printer (void)
{
    // MOVIDO: de global para local.
    Config_Aplicacao_t config_snapshot;
    Gerenciador_Config_Get_Config_Snapshot(&config_snapshot);

    DadosMedicao_t medicao_snapshot;
    Medicao_Get_UltimaMedicao(&medicao_snapshot);

    const Config_Grao_t* dados_grao_ativo = &config_snapshot.graos[config_snapshot.indice_grao_ativo];

    Cabecalho();

    printf("Produto       = %16s\n\r",  dados_grao_ativo->nome);
  	printf("Versao Equacao= %10lu\n\r",   (unsigned long)dados_grao_ativo->id_curva);
  	printf("Validade Curva= %13s\n\r", dados_grao_ativo->validade);
  	printf("Amostra Numero= %8i\n\r",      4);
  	printf("Temp.Amostra .= %8.1f 'C\n\r", 22.0);
  	printf("Temp.Instru ..= %8.1f 'C\n\r", medicao_snapshot.Temp_Instru);
  	printf("Peso Amostra .= %8.1f g\n\r", medicao_snapshot.Peso);
  	printf("Densidade ....= %8.1f Kg/hL\n\r",  medicao_snapshot.Densidade);
    printf(Linha);
  	printf("Umidade ......= %14.*f %%\n\r", (int)config_snapshot.nr_decimals, medicao_snapshot.Umidade);
  	printf(Linha);

  	Assinatura();
}