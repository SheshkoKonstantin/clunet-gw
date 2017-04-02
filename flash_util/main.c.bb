
#include <stdlib.h>
#include <stdio.h>   /* Стандартные объявления ввода/вывода */
#include <string.h>  /* Объявления строковых функций */
#include <unistd.h>  /* Объявления стандартных функций UNIX */
#include <fcntl.h>   /* Объявления управления файлами */
#include <errno.h>   /* Объявления кодов ошибок */
#include <termios.h> /* Объявления управления POSIX-терминалом */



#define uint8_t	unsigned char
#define rbuf_sz	256
char rbuf[rbuf_sz+1];
unsigned int rbuf_p=0;
char wbuf[rbuf_sz+1];

int fd;
int ff;


char 


void byte2hex (uint8_t *b, char *hp) {
    uint8_t c=(*b&0xf0)>>4;
    if (c>9) c+=0x61-10; else c+=0x30;
    *hp=c;
    c=*b&0x0f;
    if (c>9) c+=0x61-10; else c+=0x30;
    *(hp+1)=c;
    return;
}


void com_read_line () {
    char c;
    rbuf_p=0;
    while (1) {
	int b=read(fd, &c, 1);
	if (b>0) {
	    if ((c!='\r')&&(rbuf_p<rbuf_sz)) rbuf[rbuf_p++]=c;
	    //printf("%c\n",c);
	    //fflush (stdout);
	    if (c=='\r') {
		rbuf[rbuf_p]=0;
		//printf("%s\n",rbuf);
		break;
	    }
	}
    }

    return;
}

void write_str (char *str) {
    for (int i=0;*(str+i)!=0;++i) {
	usleep(1000);
	write (fd,str+i,1);
    }
    usleep(5000);
    return;
}

int open_port(void) {
  int fd; /* Файловый дескриптор для порта */
  fd = open("/dev/ttyUSB0", O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (fd == -1)   {
    perror("open_port: Unable to open /dev/ttyf1 - ");
  }
  else
    fcntl(fd, F_SETFL, 0);
  return (fd);
}

int open_file(void) {
  int file;
  file = open("/home/kosta/desktop/sega/switch2/clunet-demo.hex",O_RDONLY);
  if (fd == -1)   {
    perror("open_port: Unable to open hex!! ");
  }
  else
  return (file);
}

void print_hex1 (char * p, unsigned int c) {
    for (int i=0;i<c;++i)
	printf ("%02x ",p[i]);
	printf("\n");
    return;
}

void hex2byte1 (char *hp, uint8_t *b) {
    *b=0;
    char *lp=hp+1;
    if ((*lp>='0')&&(*lp<='9')) {
    *b+=(*lp-'0');
    }
    if ((*lp>='a')&&(*lp<='f')) {
    *b+=(*lp-'a'+10);
    }
    if ((*hp>='0')&&(*hp<='9')) {
    *b+=((*hp-'0')*16);
    }
    if ((*hp>='a')&&(*hp<='f')) {
    *b+=((*hp-'a'+10)*16);
    }
    return;
}

uint8_t hex2byte(char *hp) {
    uint8_t b;
    hex2byte1 (hp, &b);
    return b;
}


int page_size=64;


void wait_flash_ok () {
		// Ждем подтверждения
		while (1) {
		com_read_line();
		printf("ANS: %s\n",rbuf);
		if (rbuf_p>0 && strncmp(rbuf,"P63020104",9)==0) { 
		    printf("write ok!!!!\n");
		    //write_str ("F6308020105\r");
		    //exit(0);
		    break;
		    } //else {exit(22);}
		rbuf_p=0;
		}
    return;
}


void ppage() {
    char buf[128];
    char data[256];
    char *page=data+1+8+8+2;
    char *paddr=data+1+8+2;
    int pp=0;
    unsigned char c;
    unsigned char bytes_in_line=0;
    uint8_t	addr_l, addr_h;
    int beg_addr=0;
    int cur_addr=0;
    int page_cnt=0;
    int line_cnt=0;
    ////////////////FaappccssCCAAAAAAAA
    //strncpy(data,"F6308022a0300000000",1+8+8+2);  //16
      strncpy(data,"F6308024a0300000000",1+8+8+2);  //32
    //strncpy(data,"F6308028a0300000000",1+8+8+2);  //64
    while (1) {
	read(ff, &c, 1); // :
	if (c!=':') { perror("':' in hex! "); exit(-1);};
	read(ff, buf, 2); // bytes in line
	bytes_in_line=hex2byte(buf);
	++line_cnt;
	//printf("Bytes in %d line %d \n", line_cnt, bytes_in_line);
	read(ff, buf, 4); // address
	addr_h=hex2byte(buf);
	addr_l=hex2byte(buf+2);
	//printf("Address %d %d \n", addr_h, addr_l);
	read(ff, buf, 2); // 00 ?  ПОСЛЕДНЯЯ ЛИНИЯ?
	if (strncmp(buf,"01",2)==0) { 
	    printf("Last line!\n");
	    for (int i=0;i<bytes_in_line*2;++i) { // line bytes...
		read(ff, page+pp, 1); ++pp; ++cur_addr;
	    }
	    for (int i=pp;i<page_size*2;++i) {
		page[i]='F';
	    }
	    
	    write_str(data);
	    wait_flash_ok();
	    write_str ("F6308020105\r");
	    exit(0);
	    break;
	} else if (strncmp(buf,"00",2)!=0) {
	    printf("NOT 00!\n");
	    write_str ("F6308020105\r");
	    exit(0);
	}
	
	//if (page_cnt>40) break;
	printf ("read:");
	for (int i=0;i<bytes_in_line*2;++i) { // line bytes...
	    read(ff, page+pp, 1); printf("%c",page[pp]); ++pp; ++cur_addr;
	    if (pp>=page_size*2) {
		page[pp]=0;
		page[pp]='\r';
		page[pp+1]=0;
		printf("Data %d:\n", page_cnt);
		printf("%s\n",data);
		write_str (data);
		wait_flash_ok();
		pp=0;
		printf("BEGADDR:%02x\n",beg_addr);
		beg_addr=cur_addr/2;
		byte2hex( ((uint8_t *)&beg_addr)+0,paddr);
		byte2hex( ((uint8_t *)&beg_addr)+1,paddr+2);
		++page_cnt;
	    }
	    
	}
	printf("\n");
	
	read(ff, buf, 2); // cksum
	//printf(":::%02x:%02x\n", buf[0],buf[1]);
	read(ff, buf, 2); // \r\n
	if (strncmp(buf,"\r\n",2)!=0) { perror("'rn' in hex! "); printf("::%02x:%02x\n", buf[0],buf[1]); exit(-1); };

    }

    return;
}



void parse () {
    if (strcmp (rbuf,"P63020100")==0) {
	printf ("Обнаружен желающий!\n");
	write_str ("F6308020101\r");
	return;
    }
    
    if (strcmp(rbuf,"P6302020240")==0) {
	printf ("Желающий готов!\n");
	ppage();
	
	return;
    }
    
    return;
}


int main () {
    fd=open_port();
    ff=open_file();

    struct termios oldtio, newtio;
    
    tcgetattr(fd,&oldtio); // save current port settings 
    newtio.c_cflag = B115200 | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;
    newtio.c_lflag = 0;
    newtio.c_cc[VMIN]=1;
    newtio.c_cc[VTIME]=0;
    tcflush(fd, TCIFLUSH);
    tcsetattr(fd,TCSANOW,&newtio);


/*
    write_str("\r");
    write_str("\r");
    write_str("\r");
    write_str("cr00\r");
    write_str("cg00\r");
    write_str("cb00\r");
*/

    write_str("F6308020105\r");
    usleep(5000);
    write_str("F63080300\r");
    usleep(5000);

    //ppage();

    char c;
    while (1) {
	int b=read(fd, &c, 1);
	if (b>0) {
	    if ((c!='\r')&&(rbuf_p<rbuf_sz)) rbuf[rbuf_p++]=c;
	    if (c=='\r') {
		rbuf[rbuf_p]=0;
		printf("%s\n",rbuf);
		parse();
		rbuf_p=0;
	    }
	}
    }
    
    
    
    close(fd);


return 0;
}