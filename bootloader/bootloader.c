#include <avr/io.h>
#include <util/delay.h>
#include <avr/boot.h>
#include <avr/interrupt.h>

#if SPM_PAGESIZE > 128
 #define MY_SPM_PAGESIZE 128
#else
 #define MY_SPM_PAGESIZE SPM_PAGESIZE
#endif



#define RBUF_SZ		(MY_SPM_PAGESIZE+1+1+2)*2+11
static char rbuf[RBUF_SZ+1];
static uint8_t rbuf_p=0;


static void (*jump_to_app)(void) = 0x0000;

void hex2byte1 (char *hp, uint8_t *b) {
    *b=0;
    char *lp=hp+1;
    if ((*lp>='0')&&(*lp<='9')) {
    *b+=(*lp-'0');
    }
    if ((*lp>='a')&&(*lp<='f')) {
    *b+=(*lp-'a'+10);
    }
    if ((*lp>='A')&&(*lp<='F')) {
    *b+=(*lp-'A'+10);
    }
    if ((*hp>='0')&&(*hp<='9')) {
    *b+=((*hp-'0')*16);
    }
    if ((*hp>='a')&&(*hp<='f')) {
    *b+=((*hp-'a'+10)*16);
    }
    if ((*hp>='A')&&(*hp<='F')) {
    *b+=((*hp-'A'+10)*16);
    }
    return;
}

uint8_t hex2byte(char *hp) {
    uint8_t b;
    hex2byte1 (hp, &b);
    return b;
}

void byte2hex (uint8_t *b, char *hp) {
    uint8_t c=(*b&0xf0)>>4;
    if (c>9) c+='A'-10; else c+='0';
    *hp=c;
    c=*b&0x0f;
    if (c>9) c+='A'-10; else c+='0';
    *(hp+1)=c;
    return;
}

uint8_t byte2hex_hi(uint8_t b) {
    uint8_t h=(b&0xf0)>>4;
    if (h>9) h+='A'-10; else h+='0';
    return h;
}

uint8_t byte2hex_lo(uint8_t b) {
    uint8_t l=b&0x0f;
    if (l>9) l+='A'-10; else l+='0';
    return l;
}



void USARTInit(uint16_t ubrr_value) {
   UBRRL = ubrr_value;
   UBRRH = (ubrr_value>>8);
   //UCSRA = _BV(U2X);
   UCSRB=(1<<RXEN)|(1<<TXEN);
   UCSRC=(1<<URSEL)|(3<<UCSZ0);
   return;
}



void USART_TransmitByte( unsigned char data ) {
    while (!(UCSRA&(1<<UDRE)));
    UDR = data;
    return;
}

void USART_TransmitHex( unsigned char data ) {
    unsigned char h = data>>4;
    char ho = (h < 10) ? (h+'0') : (h+'A'-10);
    unsigned char l = data & 0xF;
    char lo = (l < 10) ? (l+'0') : (l+'A'-10);
    while ( !( UCSRA & (1<<UDRE)) );
    UDR = ho;
    while ( !( UCSRA & (1<<UDRE)) );
    UDR = lo;
}

void USART_TransmitText(char* data) {
    while (*data != 0) {
	while ( !( UCSRA & (1<<UDRE)) );
	UDR = *data;
	data++;
    }
}

void USART_Transmit(void* p, unsigned long int len) {
    unsigned char* buff = (unsigned char*)p;
    unsigned long int b;
    for (b = 0; b < len; b++) USART_TransmitByte(buff[b]);
}


uint8_t	overloads=5;
char USART_ReadChar() {
    do {
	if ((overloads>0)&&(TCNT1>16000)) {
	    --overloads;
	    if (overloads==0) jump_to_app();
	    USART_TransmitText("R00\r");
	    TCNT1=0;
	}
    } while(!(UCSRA & (1<<RXC)));
   return UDR;
}


static void
#if (FLASHEND > USHRT_MAX)
write_flash_page(uint32_t address, uint8_t* pagebuffer)
#else
write_flash_page(uint16_t address, uint8_t* pagebuffer)
#endif
{
    eeprom_busy_wait();
#if MY_SPM_PAGESIZE != SPM_PAGESIZE
    if (!(address % SPM_PAGESIZE))
#endif
    {
	boot_page_erase(address);
	boot_spm_busy_wait();		// Wait until the memory is erased.
    }
    uint8_t i;
    for (i = 0; i < MY_SPM_PAGESIZE; i += 2)
	boot_page_fill(address + i, *((uint16_t*)(pagebuffer + i)));
	boot_page_write(address);	// Store buffer in flash page.
	boot_spm_busy_wait();		// Wait until the memory is written.
	boot_rww_enable();
}



int main (void) {
    char data;
    USARTInit(51);
    USART_TransmitText("\rUART bootloader 0.01\r");

    TCCR1A=0x00;
    TCCR1B=0x05;
    ACSR=0x80;

    while (1) {
	data=USART_ReadChar();
	_delay_ms(1);
	USART_TransmitByte(data);
	if (rbuf_p<RBUF_SZ) { rbuf[rbuf_p++]=data; }
	if (data=='\r'||data=='\n') {
	    switch (rbuf[0]) {
		case 'S':
		    if (rbuf[1]=='0')
		    switch (rbuf[2]) {
			case '1':
			    overloads=0;
			    if (rbuf_p!=1+(1)*2+1) break;
			    rbuf[0]='R';
			    rbuf[1]='0';
			    rbuf[2]='2';
			    rbuf[3]=byte2hex_hi(MY_SPM_PAGESIZE>>8);
			    rbuf[4]=byte2hex_lo(MY_SPM_PAGESIZE>>8);
			    rbuf[5]=byte2hex_hi(MY_SPM_PAGESIZE);
			    rbuf[6]=byte2hex_lo(MY_SPM_PAGESIZE);
			    rbuf[7]='\r';
			    rbuf[8]='\0';
			    USART_TransmitText(rbuf);
			    break;
			case '3':
			    if (rbuf_p!=(1+(1+4+MY_SPM_PAGESIZE)*2+1+0))  break;
			    #if (FLASHEND > USHRT_MAX)
			    uint32_t address;
			    uint8_t *p = (uint8_t *) &address;
			    p[0]=hex2byte(rbuf+3+2*0);
			    p[1]=hex2byte(rbuf+3+2*1);
			    p[2]=hex2byte(rbuf+3+2*2);
			    p[3]=hex2byte(rbuf+3+2*3);
			    #else
			    uint16_t address;
			    uint8_t *p = (uint8_t *) &address;
			    p[0]=hex2byte(rbuf+3+2*0);
			    p[1]=hex2byte(rbuf+3+2*1);
			    #endif
			    for (uint8_t i=0;i<MY_SPM_PAGESIZE;++i) {
				rbuf[i]=hex2byte(rbuf+11+i*2);
			    }
			    write_flash_page(address, (uint8_t *)rbuf); // Пишем во флеш-память
			    USART_TransmitText("R04\r");
			    break;
			case '5':
			    jump_to_app();
			    break;
			default:
			    break;
		    }
		    break;
		default:
		    break;
	    }
        rbuf_p=0;
	}
    }
    return 0;
}
