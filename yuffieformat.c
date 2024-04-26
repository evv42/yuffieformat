#include <stdio.h>
#include <stdlib.h>

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;

struct d88header {
	uint8_t name[17];
	uint8_t reserved[9];
	uint8_t protect;      /*non-zero is write protected*/
	uint8_t media;        /*0x00 = 2D, 0x10 = 2DD, 0x20 = 2HD*/
	uint32_t size;
	uint32_t tracks[164]; /*unformatted track have offset 0*/
};
typedef struct d88header d88header;

struct d88sector {
	uint8_t c;            /*cylinder*/
	uint8_t h;            /*head*/
	uint8_t r;            /*record*/
	uint8_t n;            /*size (128 << n)*/
	uint16_t sectors;     /*sectors in track*/
	uint8_t density;
	uint8_t ddam;         /*deleted data flag*/
	uint8_t fdc_status;   /*status of the fdc when the sector was read*/
	uint8_t reserved[5];
	uint16_t size;
};
typedef struct d88sector d88sector;

uint8_t chrn_table[256]; /*max sectors per track (128b sector size)*/
uint8_t sector_data[1024];/*max sector size*/

uint16_t pc98_floppyformat(uint16_t drive, uint16_t c, uint16_t h, uint16_t n, uint16_t dtl, uint16_t pattern, uint8_t* layout, uint16_t mask){
#asm
	push bp
	mov bp, sp
	push ds
	
	mov cl, [bp+6]
	mov dh, [bp+8]
	mov ch, [bp+10]
	mov bx, [bp+12]
	mov dl, [bp+14]
	mov al, [bp+4]
	mov ah, [bp+18]
	or  ah, #$1D
	or  al, #$90
	
	push ax
	mov ax, [bp+16]
	mov bp, ax
	pop ax
	
	int #$1B
	
	mov bh,ah
	lahf
	and ax, #0001
	mov ah,bh
	
	pop ds
	pop bp
#endasm
}

uint16_t pc98_floppywrite(uint16_t drive, uint16_t c, uint16_t h, uint16_t n, uint16_t dtl, uint16_t r, uint8_t* data, uint16_t mask){
#asm
	push bp
	mov bp, sp
	push ds
	
	mov cl, [bp+6]
	mov dh, [bp+8]
	mov ch, [bp+10]
	mov bx, [bp+12]
	mov dl, [bp+14]
	mov al, [bp+4]
	mov ah, [bp+18]
	or  ah, #$15
	or  al, #$90
	
	push ax
	mov ax, [bp+16]
	mov bp, ax
	pop ax
	
	int #$1B
	
	mov bh,ah
	lahf
	and ax, #0001
	mov ah,bh
	
	pop ds
	pop bp
#endasm
}

uint16_t pc98_floppyseek(uint16_t track, uint16_t drive){
#asm
	push bp
	mov bp, sp
	push ds
	
	mov cx, [bp+4]
	mov ax, [bp+6]
	mov ah, #$10
	or  al, #$90
	
	int #$1B
	
	lahf
	and ax, #0001
	
	pop ds
	pop bp
#endasm
}

void pc98_beep(uint16_t time, uint16_t frequency){/*time is n*10ms, frequency is hertz as hex (0x20 = 20Hz)(check)*/
#asm
	push bp
	mov bp, sp
	push ds
	
	mov cx, [bp+4]
	mov dx, [bp+6]
	mov ah, #$06
	
	int #$1C
	
	pop ds
	pop bp
#endasm
}

uint16_t pc98_check(){
#asm
	mov ah, #$0f
	int #$10
	cmp ah, #$0f
	je is_98
	mov ax, #1
	jmp en
	is_98:
		mov ax, #0
	en:
		nop
#endasm
}

int p_track_info(uint32_t offset, FILE* f){
	d88sector s;
	uint8_t i;
	uint16_t n, sectors;
	
	fseek(f, offset, SEEK_SET);
	
	fread(&s, sizeof(d88sector), 1, f);
	
	n = 128 << s.n;
	sectors = s.sectors;
	
	printf("(%db, %ds)c%dh%d: ", n, sectors, s.c, s.h);
	for(i=0; i<sectors; i++){
		fseek(f, offset + ((16 + n)*i), SEEK_SET);
		fread(&s, sizeof(d88sector), 1, f);
		printf("%d ", s.r);
		if(s.ddam != 0)printf("D");
	}
	printf("\n");
	
	return 0;
}

int f_track(uint32_t offset, FILE* f, uint8_t d, uint8_t rc, uint8_t rh){
	d88sector s;
	uint8_t i, r;
	uint16_t n, sectors, mask;
	
	fseek(f, offset, SEEK_SET);
	
	fread(&s, sizeof(d88sector), 1, f);
	
	n = 128 << s.n;
	sectors = s.sectors;
	
	printf("\nformat(%db)track %d(%d) head %d(%d):", n, rc, s.c, rh, s.h);
	fflush(stdout);
	for(i=0; i<sectors; i++){
		fseek(f, offset + ((16 + n)*i), SEEK_SET);
		fread(&s, sizeof(d88sector), 1, f);
		chrn_table[(i*4)+0] = s.c;
		chrn_table[(i*4)+1] = s.h;
		chrn_table[(i*4)+2] = s.r;
		chrn_table[(i*4)+3] = s.n;
	}
	/*if(s.c == 2){
		printf("\n");
		for(i=0; i<sectors; i++){
			printf("%d ", chrn_table[(i*4)+0]);
			printf("%d ", chrn_table[(i*4)+1]);
			printf("%d ", chrn_table[(i*4)+2]);
			printf("%d ", chrn_table[(i*4)+3]);
		}
		printf("\n");
	}*/
	if((n == 128) || (n == 256 && sectors <= 15)){/*TODO: more checks here*/
		mask = 0;/*fm*/
	}else{
		mask = 0x40;/*mfm*/
	}
	r = pc98_floppyformat(d, rc, rh, s.n, sectors << 2, 0xFF, &chrn_table, mask);
	
	if(r == 0){
		printf("OK !", r);
	}else{
		printf("ERROR: %x", r);
		return r;
	}
	
	return 0;
}

int w_track(uint32_t offset, FILE* f, uint8_t d, uint8_t rc, uint8_t rh){
	d88sector s;
	uint8_t i, r;
	uint16_t n, sectors, mask;
	
	fseek(f, offset, SEEK_SET);
	
	fread(&s, sizeof(d88sector), 1, f);
	
	n = 128 << s.n;
	sectors = s.sectors;
	
	printf("\nwrite(%db)track %d(%d) head %d(%d):", n, rc, s.c, rh, s.h);
	fflush(stdout);
	
	for(i=0; i<sectors; i++){
		fseek(f, offset + ((16 + n)*i), SEEK_SET);
		fread(&s, sizeof(d88sector), 1, f);
		fread(&sector_data, 1, n, f);
		printf("\nsector%d ", s.r);
		fflush(stdout);
		if((n == 128) || (n == 256 && sectors <= 15)){/*TODO: more checks here*/
			mask = 0;/*fm*/
			printf("(FM)");
		}else{
			mask = 0x40;/*mfm*/
			printf("(MFM)");
		}
		r = pc98_floppywrite(d, rc, rh, s.n, (128 << s.n), s.r, &sector_data, mask);
		if(r == 0){
			printf("OK !", r);
		}else{
			printf("ERROR: %x", r);
			return r;
		}
		fflush(stdout);
	}
	
	return 0;
}

int print_disk_info(char** file){
	FILE* f;
	uint8_t i;
	d88header h;
	int tn;
	
	f = fopen(file,"rb");
	if(f==NULL)return 2;
	
	fread(&h, sizeof(d88header), 1, f);
	
	printf("image is named \"%s\", wp:%d, type:%x, size=%ld bytes (~%ld mibs)\n", h.name, h.protect, ((h.media >> 4) & 0x07), h.size, h.size/1024/1024);
	
	printf("track layout:\n");
	tn = -1;
	for(i=0; i<164; i++){
		if(h.tracks[i] != 0)p_track_info(h.tracks[i], f);
		if(h.tracks[i] == 0 && tn == -1)tn=i;
	}
	printf("%d tracks (%d 2-head)\n", tn, tn/2);
	
	fclose(f);
	return 0;
}

int write_disk(char* file, char* drive){
	FILE* f;
	uint8_t i, d;
	d88header h;
	int tn;
	
	d = atoi(drive);
	if(d > 3){
		printf("invalid drive number\n");
		return 0;
	}
	
	f = fopen(file,"rb");
	if(f==NULL)return 2;
	
	fread(&h, sizeof(d88header), 1, f);
	
	printf("image is named \"%s\", wp:%d, type:%x, size=%ld bytes (~%ld mibs)\n", h.name, h.protect, ((h.media >> 4) & 0x07), h.size, h.size/1024/1024);
	
	printf("insert disk in drive %d and press enter", d);
	getchar();
	printf("writing...\n");
	tn = -1;
	for(i=0; i<164; i++){
		if(h.tracks[i] != 0){
			f_track(h.tracks[i], f, d, i/2, i&1);
			w_track(h.tracks[i], f, d, i/2, i&1);
		}
		if(h.tracks[i] == 0 && tn == -1)tn=i;
	}
	printf("\n%d tracks (%d 2-head) written, no errors reported by fdc\n", tn, tn/2);
	
	fclose(f);
	return 0;
}

int seek(char* drive, char* track){
	int d,t;
	uint16_t r;
	
	if(pc98_check()){
		printf("this option cannot be used on a IBM PC\n");
		return 5;
	}
	
	d = atoi(drive);
	t = atoi(track);
	
	r = pc98_floppyseek(t,d);
	
	if(r == 0)printf("seek of drive %d to sector %d was successful\n",d, t);
	
	return 0;
}

int main(int argc, char** argv){
	if(argc < 3){
		printf("%s: not enough arguments\n", argv[0]);
		return 1;
	}
	switch(argv[1][0]){
		case 'i': return print_disk_info(argv[2]);
		case 's': return seek(argv[2], argv[3]);
		case 'w': return write_disk(argv[2], argv[3]);
		default: printf("%s: bad syntax\n", argv[0]);
	}
	return 3;
}
