#include "gpio_uart.h"
//#include "stm32f401xc.h"
#include "wb32f3g71xx.h"
#include "string.h"

#ifdef UART_DEBUG_TRACE

#ifndef DEBUG_PORT
#define DEBUG_PORT  GPIOA
#endif

#ifndef DEBUG_PIN
#define DEBUG_PIN   13
#endif


#pragma GCC push_options
#pragma GCC optimize ("O2")

#define GPIOUART_PIN_L  ((GPIO_TypeDef *)DEBUG_PORT)->BSRR = (1<<(DEBUG_PIN+16))
#define GPIOUART_PIN_H  ((GPIO_TypeDef *)DEBUG_PORT)->BSRR = (1<<DEBUG_PIN)

char bt_state_list[][13]  = { "Reset", "Init", "Disconnected", "Connected", "Pairing", "Connecting", "Suspend" };
char xport[][10]  = { "NONE", "USB", "BT", "NONE", "2.4G", "max" };

static int gpiouart_putc(char byte);

static void xputs(const char* str);

static void myxvprintf (
    const char*    fmt,    /* Pointer to the format string */
    va_list arp            /* Pointer to arguments */
);

void gpiouart_init(void)
{
    uint32_t t = DEBUG_PORT->MODER;
    t &=  ~(3<<(DEBUG_PIN*2));
    t |= (1<<((DEBUG_PIN*2)));
    DEBUG_PORT->MODER = t;
    DEBUG_PORT->OSPEEDR|= (3<<((DEBUG_PIN*2)));
    DEBUG_PORT->PUPDR = (DEBUG_PORT->PUPDR & (~(3<<(DEBUG_PIN*2))))  | (0x01<<(DEBUG_PIN*2));
    GPIOUART_PIN_H;
}

/*
 feed *3 :    800K bps
*/
void gpiouart_delay(void)
{
    asm volatile("nop" ::: "memory");
    asm volatile("nop" ::: "memory");
    asm volatile("nop" ::: "memory");
    asm volatile("nop" ::: "memory");

    asm volatile("nop" ::: "memory");
    asm volatile("nop" ::: "memory");
	
    asm volatile("nop" ::: "memory");
    asm volatile("nop" ::: "memory");
#if 0

    asm volatile("nop" ::: "memory");
    asm volatile("nop" ::: "memory");
    asm volatile("nop" ::: "memory");
    asm volatile("nop" ::: "memory");


    asm volatile("nop" ::: "memory");
    asm volatile("nop" ::: "memory");
    asm volatile("nop" ::: "memory");
    asm volatile("nop" ::: "memory");
#endif
}

int gpiouart_putc(char byte) //GPIO simulate uart print func
{
    unsigned char j;
    unsigned char tmp_bit0 = 0;
    unsigned char tmp_bit1 = 1;
    unsigned char bit[10] = {0};

    bit[0] = tmp_bit0;
    bit[1] = (byte & 0x01)? tmp_bit1 : tmp_bit0;
    bit[2] = ((byte>>1) & 0x01)? tmp_bit1 : tmp_bit0;
    bit[3] = ((byte>>2) & 0x01)? tmp_bit1 : tmp_bit0;
    bit[4] = ((byte>>3) & 0x01)? tmp_bit1 : tmp_bit0;
    bit[5] = ((byte>>4) & 0x01)? tmp_bit1 : tmp_bit0;
    bit[6] = ((byte>>5) & 0x01)? tmp_bit1 : tmp_bit0;
    bit[7] = ((byte>>6) & 0x01)? tmp_bit1 : tmp_bit0;
    bit[8] = ((byte>>7) & 0x01)? tmp_bit1 : tmp_bit0;
    bit[9] = tmp_bit1;

    __disable_irq();

    for(j = 0;j<10;j++)
    {
        if(bit[j])
            GPIOUART_PIN_H;
            //gpiouart_output_h();
        else
            GPIOUART_PIN_L;
            //gpiouart_output_l();
        gpiouart_delay();
    }
    __enable_irq();

    return byte;
}

void log_uint8(unsigned char val)
{
    myxprintf("%d ",val);
}

void log_uint8_arrary(unsigned char *val ,unsigned char len)
{
    unsigned char i=0;

    myxprintf("%04x: ",i);

    for(i=0;i<len;i++)
        {
        if(i%16==0)
            myxprintf("%04x: ",i);

        myxprintf("%02x ",val[i]);
    }
}

void log_char(unsigned char val)
{
    gpiouart_putc(val);
}

void log_trace(const char* str)
{
    xputs(str);
}

void xputc(char c)
{
    gpiouart_putc(c);
}

void xputs(const char* str)
{
    unsigned char len = strlen(str);
    unsigned char i=0;

    if((len==0)||(len>128))
        return ;

    while(len--)
        {
        gpiouart_putc(*(str+i));
        i++;
    }
}

/*----------------------------------------------*/
/* Formatted string output                      */
/*----------------------------------------------*/
/*  xprintf("%d", 1234);            "1234"
    xprintf("%6d,%3d%%", -200, 5);    "  -200,  5%"
    xprintf("%-6u", 100);            "100   "
    xprintf("%ld", 12345678L);        "12345678"
    xprintf("%04x", 0xA3);            "00a3"
    xprintf("%08LX", 0x123ABC);        "00123ABC"
    xprintf("%016b", 0x550F);        "0101010100001111"
    xprintf("%s", "String");        "String"
    xprintf("%-4s", "abc");            "abc "
    xprintf("%4s", "abc");            " abc"
    xprintf("%c", 'a');                "a"
    xprintf("%f", 10.0);            <xprintf lacks floating point support>
*/
static
void myxvprintf (
    const char*    fmt,    /* Pointer to the format string */
    va_list arp            /* Pointer to arguments */
)
{
    unsigned int r, i, j, w, f;
    unsigned long v;
    char s[16], c, d, *p;

    for (;;) {
        c = *fmt++;                    /* Get a char */
        if (!c) break;                /* End of format? */
        if (c != '%') {                /* Pass through it if not a % sequense */
            xputc(c); continue;
        }
        f = 0;
        c = *fmt++;                    /* Get first char of the sequense */
        if (c == '0') {                /* Flag: '0' padded */
            f = 1; c = *fmt++;
        } else {
            if (c == '-') {            /* Flag: left justified */
                f = 2; c = *fmt++;
            }
        }
        for (w = 0; c >= '0' && c <= '9'; c = *fmt++)    /* Minimum width */
            w = w * 10 + c - '0';
        if (c == 'l' || c == 'L') {    /* Prefix: Size is long int */
            f |= 4; c = *fmt++;
        }
        if (!c) break;                /* End of format? */
        d = c;
        if (d >= 'a') d -= 0x20;
        switch (d) {                /* Type is... */
        case 'S' :                    /* String */
            p = va_arg(arp, char*);
            for (j = 0; p[j]; j++) ;
            while (!(f & 2) && j++ < w) xputc(' ');
            xputs(p);
            while (j++ < w) xputc(' ');
            continue;
        case 'C' :                    /* Character */
            xputc((char)va_arg(arp, int)); continue;
        case 'B' :                    /* Binary */
            r = 2; break;
        case 'O' :                    /* Octal */
            r = 8; break;
        case 'D' :                    /* Signed decimal */
        case 'U' :                    /* Unsigned decimal */
            r = 10; break;
        case 'X' :                    /* Hexdecimal */
            r = 16; break;
        default:                    /* Unknown type (passthrough) */
            xputc(c); continue;
        }

        /* Get an argument and put it in numeral */
        v = (f & 4) ? va_arg(arp, long) : ((d == 'D') ? (long)va_arg(arp, int) : (long)va_arg(arp, unsigned int));
        if (d == 'D' && (v & 0x80000000)) {
            v = 0 - v;
            f |= 8;
        }
        i = 0;
        do {
            d = (char)(v % r); v /= r;
            if (d > 9) d += (c == 'x') ? 0x27 : 0x07;
            s[i++] = d + '0';
        } while (v && i < sizeof(s));
        if (f & 8) s[i++] = '-';
        j = i; d = (f & 1) ? '0' : ' ';
        while (!(f & 2) && j++ < w) xputc(d);
        do xputc(s[--i]); while(i);
        while (j++ < w) xputc(' ');
    }
}

void myxprintf (            /* Put a formatted string to the default device */
    const char*    fmt,    /* Pointer to the format string */
    ...                    /* Optional arguments */
)
{
    va_list arp;
    va_start(arp, fmt);
    myxvprintf(fmt, arp);
    va_end(arp);
}

#pragma GCC pop_options

#endif



