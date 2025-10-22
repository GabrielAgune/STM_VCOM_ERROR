#ifndef RELATO_H
#define RELATO_H

#include "main.h"
#include "gerenciador_configuracoes.h"
#include "medicao_handler.h"
#include "rtc_driver.h"
#include "stm32c0xx_hal.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

extern void Who_am_i(void);

extern void Assinatura(void);
extern void Cabecalho(void);

extern void Relatorio_Printer (void);



#endif /* RELATO_H */