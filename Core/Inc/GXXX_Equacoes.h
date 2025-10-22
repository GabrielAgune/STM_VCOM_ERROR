#ifndef GXXX_EQUACOES_H
#define GXXX_EQUACOES_H

/******************************************************************************
  Curva de Calibração na ROM
******************************************************************************/
struct Produtos_ROM{
    char           *Nome[6];
    unsigned long  Nr_Equa;
    float		   Fat_A;
    float		   Fat_B;
    float		   Fat_C;
    float		   Fat_D;
    signed int     Um_Min;
    signed int 	   Um_Max;
    signed int 	   Peso_Pad;
    float 	       CT_Ganho;
    float 	       CT_Zero;
}; 

#define NR_CEREAIS 7 // Ajustado para o número real de itens na sua tabela

// CORREÇÃO: A palavra-chave 'code' foi substituída por 'const'
extern const struct Produtos_ROM Produto[];

#endif /* GXXX_EQUACOES_H */
