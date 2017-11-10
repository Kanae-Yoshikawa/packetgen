/*  
// [cpuid命令でCPU情報を取得]
// http://linux-biyori.sakura.ne.jp/program/pr_cpuid.php
// インラインアセンブラを使用してcpuid命令を実行し
// 各レジスタに情報が設定されていることを確認する．
// cpuid命令の詳しい資料についてはintelのデータシートを参照せよ
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    unsigned int eax;
    unsigned int ebx;
    unsigned int ecx;
    unsigned int edx;
}reg_t;


/* cpuid()； cpuidを実行するインラインアセンブラのラッパー関数 */
/* 引数eaxで渡された値をレジスタeaxに設定しcpuid命令を実行.
   レジスタeax,ebx,ecx,edxに設定された値を引数で渡されたrに代入 */
void cpuid(unsigned int eax, reg_t *r)
{
    asm volatile (
            "cpuid"
            :"=a"(r->eax), "=b"(r->ebx), "=c"(r->ecx), "=d"(r->edx)
            :"a"(eax)            
            );
    return;
}


int main(void)
{
    reg_t reg;
    char vendorid[16]={0}, cpuname[48]={0};
    int cpunum = 0, L2 = 0, L3 = 0;
    unsigned int support = 0;


    //EAX=0x00000000
    /* ベンダーコード取得．
       eaxを0x00でcpuidを実行するとeaxに基本命令の使用可能な最大値
       （cpuidを使用する場合に使える引数eaxの最大値）が設定され、
       ebx,ecx,edxにベンダーコードの文字列が返される
     */
    cpuid(0x00, &reg);
    support = reg.eax;
    memcpy(vendorid, &(reg.ebx), 4);
    memcpy(&vendorid[4], &(reg.edx), 4);
    memcpy(&vendorid[8], &(reg.ecx), 4);
    printf("vendor_id\t: %s\n", vendorid);

    /* CPU情報取得 */
    /* eax=0x01としてcpuidを実行し、CPUの
       ファミリー、モデル、ステッピングやサポートしている命令等を取得 */
    if(support >= 0x01){
        //EAX=0x00000001
        cpuid(0x01, &reg);
        printf("Family\t\t: %d\n", (((reg.eax) >> 8)&0x0f));
        printf("Model\t\t: %d\n", (((reg.eax) >> 4)&0x0f));
        printf("Stepping\t: %d\n", ((reg.eax)&0x0f));
        printf("Processor Type\t: %d\n", (((reg.eax) >> 12)&0x0f));
        cpunum = ((reg.ebx) >> 16) & 0xff;
        printf("CPU cores\t: %d\n", cpunum);
        printf("flags\t\t: ");
        if(((reg.edx) >> 23) & 0x01)
            printf("MMX ");
        if(((reg.edx) >> 25) & 0x01)
            printf("SSE ");
        if(((reg.edx) >> 26) & 0x01)
            printf("SSE2 ");
        if(reg.ecx & 0x01)
            printf("SSE3 ");
        if(((reg.edx) >> 28) & 0x01)
            printf("HT ");
        printf("\n");
    }

    /*拡張命令の実行 */
    /* eaxに使用可能な拡張命令の最大値が返される */
    cpuid(0x80000000, &reg);
    support = reg.eax;
  
    
    
    if(support >= 0x80000004){
        //Processor name
        cpuid(0x80000002, &reg);
        memcpy(cpuname, &(reg.eax), 4);
        memcpy(&cpuname[4], &(reg.ebx), 4);
        memcpy(&cpuname[8], &(reg.ecx), 4);
        memcpy(&cpuname[12], &(reg.edx), 4);
        cpuid(0x80000003, &reg);
        memcpy(&cpuname[16], &(reg.eax), 4);
        memcpy(&cpuname[20], &(reg.ebx), 4);
        memcpy(&cpuname[24], &(reg.ecx), 4);
        memcpy(&cpuname[28], &(reg.edx), 4);
        cpuid(0x80000004, &reg);
        memcpy(&cpuname[32], &(reg.eax), 4);
        memcpy(&cpuname[36], &(reg.ebx), 4);
        memcpy(&cpuname[40], &(reg.ecx), 4);
        memcpy(&cpuname[44], &(reg.edx), 4);
        printf("CPU name\t: %s\n", cpuname);
    }
    if(support >= 0x80000006){
        //EAX=0x80000006(cache information)
        cpuid(0x80000006, &reg);
        L2 = (reg.ecx) >> 16;
        L3 = (reg.edx) >> 16;
        printf("L2 cache\t: %d KB\n", L2);
        printf("L3 cache\t: %d KB\n", L3);
    }
    return 0;
}
