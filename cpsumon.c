/*
Corsair AXi Series PSU Monitor
Copyright (C) 2014 Andras Kovacs - andras@sth.sze.hu

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <math.h>

#define TYPE_AX760  0
#define TYPE_AX860  1
#define TYPE_AX1200 2
#define TYPE_AX1500 3

#define true  1
#define false 0

typedef struct rail_12v_elem_t {
    float voltage;
    float current;
    float power;
    unsigned char ocp_enabled;
    float ocp_limit;
} rail_12v_elem_t;

typedef struct rail_misc_elem_t {
    float voltage;
    float current;
    float power;
} rail_misc_elem_t;

typedef struct rail_12v_t {
    rail_12v_elem_t pcie[10];
    rail_12v_elem_t atx;
    rail_12v_elem_t peripheral;
} rail_12v_t;


typedef struct rail_misc_t {
    rail_misc_elem_t rail_5v;
    rail_misc_elem_t rail_3_3v;
} rail_misc_t;

typedef struct psu_main_power_t {
    float voltage;
    float current;
    float inputpower;
    float outputpower;
    char cabletype;
    float efficiency;
} psu_main_power_t;

static int _psu_type;
static rail_12v_t _rail12v;
static rail_misc_t _railmisc;
static psu_main_power_t _psumain;


unsigned char encode_table[16]  =
			 {0x55, 0x56, 0x59, 0x5a, 0x65, 0x66, 0x69, 0x6a, 0x95, 0x96, 0x99, 0x9a, 0xa5, 0xa6, 0xa9, 0xaa};
unsigned char decode_table[256] =
			 {0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10,
			  0x20, 0x21, 0x00, 0x12, 0x22, 0x23, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x14, 0x24,
			  0x25, 0x00, 0x16, 0x26, 0x27, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x28, 0x29, 0x00, 0x1a,
			  0x2a, 0x2b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1c, 0x2c, 0x2d, 0x00, 0x1e, 0x2e,
			  0x2f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
			  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
			  0x00
			};

int xread(int f, void * b, int s, int timeout) {
    int r, ss=s;
    time_t cstart = time(NULL);
    unsigned char * bb = (unsigned char *) b;
    do {
        do {
	    if (timeout > 0 && (time(NULL) - cstart) >=  timeout) {
    		return ss - s;
	    }
            r = read(f, bb, s);
            if (r == -1) usleep(200);
        } while (r == -1);
    bb+=r;
    s-=r;
    } while (s != 0);
return ss;
}

int xwrite(int fd, void * buffer, unsigned int len) {
    int count = 0, ret = 0;
    while (count < len) {
	errno = 0;
	ret = write(fd, buffer + count, len - count);
	if (ret <= 0) {
	    if (errno == EAGAIN || errno == EWOULDBLOCK) {
		usleep(200);
		continue;
	    }
	    return ret;
	}
	count += ret;
    }
    return count;
}


/* taken from http://www.cs.unc.edu/~dewan/242/s00/xinu-pentium/debug/hexdump.c */
#define OPL 16 /* octets printed per line */

void dump(unsigned char *buf, int dlen) {
    char c[OPL+1];
    int i, ct;

    if (dlen < 0) {
        printf("WARNING: computed dlen %d\n", dlen);
        dlen = 0;
    }

    for (i=0; i<dlen; ++i) {
        if (i == 0)
            printf("DATA: ");
        else if ((i % OPL) == 0) {
            c[OPL] = '\0';
            printf("\t|%s|\nDATA: ", c);
        }
        ct = buf[i] & 0xff;
        c[i % OPL] = (ct >= ' ' && ct <= '~') ? ct : '.';
        printf("%02x ", ct);
    }
    c[i%OPL] = '\0';
    for (; i % OPL; ++i)
        printf("   ");
    printf("\t|%s|\n", c);
}

unsigned char * decode_answer(unsigned char *data, int size, int * nsize) {
	int i, j = 0;
	int newsize = (size/2);
	if (newsize <= 0) return NULL;

	if (nsize) *nsize = newsize;

        if (((decode_table[data[0]] & 0xf) >> 1) != 7) {
	    printf("decode_answer: wrong reply data: %d (data %x)\n", ((decode_table[data[0]] & 0xf) >> 1), data[0]);
	    return NULL;
	}

	unsigned char *ret = (unsigned char*) malloc(newsize);

	if (!ret) return NULL;

	for (i = 1; i <= size; i += 2) {
	    ret[j++] = (decode_table[data[i]] & 0xf) | ((decode_table[data[i + 1]] & 0xf) << 4);
	}

	return ret;
}

unsigned char * encode_answer(unsigned char command, unsigned char *data, int size, int * nsize) {
    int i, j = 1;
    if (size <= 0) return NULL;

    int newsize = (size * 2) + 2;

    if (nsize) *nsize = newsize;

    unsigned char *ret = (unsigned char *) malloc(newsize);
    if (!ret) return NULL;

    ret[0] = encode_table[(command << 1) & 0xf] & 0xfc;
    ret[newsize - 1] = 0;

    for (i = 1; i <= size; i++) {
	ret[j++] = encode_table[data[i - 1] & 0xf];
	ret[j++] = encode_table[data[i - 1] >> 4];
    }

    return ret;
}

float convert_byte_float(unsigned char * data) {

    int p1 = (data[1] >> 3) & 31;
    if (p1 > 15) p1 -= 32;

    int p2 = ((int)data[1] & 7) * 256 + (int)data[0];
    if (p2 > 1024) p2 = -(65536 - (p2 | 63488));

    return (float) p2 * powf(2.0, (float) p1);
}

void convert_float_byte(float val, int exp, unsigned char *data) {
    int p1;
    if (val > 0.0) {
        p1 = round(val * pow(2.0, (double) exp));
        if (p1 > 1023) p1 = 1023;
    } else {
        int p2 = round(val * pow(2.0, (double) exp));
        if (p2 < -1023) p2 = -1023;
        p1 = p2 & 2047;
    }
    data[0] = (unsigned char) (p1 & 255);
    exp = exp <= 0 ? -exp : 256 - exp;
    exp = exp << 3 & 255;
    data[1] = (unsigned char) (p1 >> 8 & 255 | exp);
}

// maximum 512 bytes
unsigned char * data_read_dongle(int fd, int size, int * command) {
    unsigned char buffer[1024];
    memset(buffer, 0, 1024);

    if (size < 0) size = 512;

    size *= 2;

    char r;
    if ((r = xread(fd, buffer, size - 1, 2)) == 0) return NULL;

//    printf("read=%d, exp=%d\n", r, size);
//    dump(buffer, r);

    read(fd, buffer+r, 1); // eat optional 0x00 at the end

    buffer[r] = 0x00;

    return decode_answer(buffer, size, command);
}

int data_write_dongle(int fd, unsigned char * datain, int size) {
    int s;
    char *data = encode_answer(0, datain, size, &s);

//    dump(datain, size);
//    printf("DATA TO WRITE:\n");
//    dump(data, s);

    int ret = xwrite(fd, data, s);
    free(data);

    return (ret != s) ? -1 : 0;
}

unsigned char * read_dongle_name(int fd) {
    char d[1] = {2};

    if (data_write_dongle(fd, d, 1) != 0) return NULL;

    unsigned char * ret = data_read_dongle(fd, 512, NULL);

    return ret;
}

int read_dongle_version(int fd, float *f) {
    char d[1] = {0};

    if (data_write_dongle(fd, d, 1) != 0) return -1;

    unsigned char * ret = data_read_dongle(fd, 3, NULL);

    if (!ret) return -1;

    *f = (ret[1] >> 4) + (ret[1] & 0xf)/10.;
    free(ret);

    return 0;
}

unsigned char * read_data_psu(int fd, int reg, int len) {
    char d[7] = {19, 3, 6, 1, 7, (char) len, (char) reg};
    char d2[3] = {8, 7, (char) len};

    if (data_write_dongle(fd, d, 7) != 0) return NULL;

    unsigned char * ret = data_read_dongle(fd, 1, NULL);

    if (!ret) return NULL;
    free(ret);

    if (data_write_dongle(fd, d2, 3) != 0) return NULL;

    ret = data_read_dongle(fd, len + 1, NULL);

//    dump(ret, len + 1);

    return ret;
}

unsigned char * write_data_psu(int fd, int reg, char * data, int len) {
    char * d = (char *) malloc(len + 5);
    if (!d) return NULL;
    d[0] = 19;
    d[1] = 1;
    d[2] = 4;
    d[3] = len + 1;
    d[4] = reg;
    memcpy(d + 5, data, len);

    if (data_write_dongle(fd, d, len + 5) != 0) return NULL;

    unsigned char * ret = data_read_dongle(fd, 1, NULL);

    //dump(ret, 2);

    return ret;
}

int set_page(int fd, int main, int page) {
    int r = -1;
    unsigned char c = (unsigned char) page;
    unsigned char * ret = write_data_psu(fd, (main ? 0 : 0xe7), &c, 1);
    unsigned char * ret2 = read_data_psu(fd, (main ? 0 : 0xe7), 1);

    if (!ret || !ret2 || ret2[0] != c) {
	printf("set_page (%s): set failed: %x, %x\n", (main ? "main" : "12v"), c, (ret2 ? ret2[0] : 0));
    } else if (ret && ret2) r = 0;

    if (ret) free(ret);
    if (ret2) free(ret2);

    return r;
}

int set_12v_page(int fd, int page) {
    return set_page(fd, 0, page);
}

int set_main_page(int fd, int page) {
    return set_page(fd, 1, page);
}

int read_psu_main_power(int fd) {
    unsigned char *ret;
    float unk1;

    if (set_main_page(fd, 0) == -1) return -1;

    if ((ret = read_data_psu(fd, 0x97, 2)) == NULL) return -1;
    unk1 = convert_byte_float(ret);
    free(ret);

    if ((ret = read_data_psu(fd, 0x89, 2)) == NULL) return -1;
    _psumain.current = convert_byte_float(ret);
    free(ret);

    if ((ret = read_data_psu(fd, 0x88, 2)) == NULL) return -1;
    _psumain.voltage = convert_byte_float(ret);
    free(ret);

    if ((ret = read_data_psu(fd, 0xee, 2)) == NULL) return -1;
    _psumain.outputpower = convert_byte_float(ret);
    free(ret);

    if (_psu_type == TYPE_AX1500) {
	if ((ret = read_data_psu(fd, 0xf2, 1)) == NULL) return -1;
	_psumain.cabletype = ret[0];
	free(ret);
    }

    _psumain.inputpower = (unk1 + (_psumain.voltage * _psumain.current))/2.;

    switch (_psu_type) {
	case TYPE_AX1500:
	    if (_psumain.voltage < 170.0) {
		if (_psumain.inputpower < 259.0) {
		    _psumain.outputpower = 0.9151 * _psumain.inputpower - 8.5209;
		    break;
		} else break;
	    } else if (_psumain.inputpower < 254.0) {
		_psumain.outputpower = 0.9394 * _psumain.inputpower - 62.289;
		break;
	    }
	break;
	case TYPE_AX1200:
	    if (_psumain.voltage < 170.0) {
		if (_psumain.inputpower < 201.0) {
		    _psumain.outputpower = 0.950565 * _psumain.inputpower - 11.98481;
		    break;
		} else break;
	    } else if (_psumain.inputpower < 195.0) {
		_psumain.outputpower = 0.97254 * _psumain.inputpower - 12.93532;
		break;
	    }
	break;
	case TYPE_AX860:
	    if (_psumain.voltage < 170.0) {
		if (_psumain.inputpower < 144.0) {
		    _psumain.outputpower = 0.958796 * _psumain.inputpower - 10.80566;
		    break;
		} else break;
	    } else if (_psumain.inputpower < 141.0) {
		_psumain.outputpower = 0.969644 * _psumain.inputpower - 10.59645;
		break;
	    }
	break;
	default: // AX760i
	    if (_psumain.voltage < 170.0) {
		if (_psumain.inputpower < 126.0) {
		    _psumain.outputpower = 0.958395 * _psumain.inputpower - 10.71166;
		    break;
		} else break;
	    } else if (_psumain.inputpower < 123.0) {
		_psumain.outputpower = 0.973022 * _psumain.inputpower - 10.8746;
		break;
	    }
	break;
    }

    if (_psumain.outputpower > _psumain.inputpower * 0.99) _psumain.outputpower = _psumain.inputpower * 0.99;

    _psumain.efficiency = (_psumain.outputpower/_psumain.inputpower) * 100.;

    return 0;
}

int read_psu_rail12v(int fd) {
    int i;
    unsigned char * ret;
    float f;
    int chnnum = (_psu_type == TYPE_AX1500 ? 10 : ((_psu_type == TYPE_AX1200) ? 8 : 6));
    for (i = 0; i < chnnum + 2; i++) {
//	printf("chnnum=%d\n", i);
	if (set_main_page(fd, 0) == -1) return -1;
	if (set_12v_page(fd, (_psu_type != TYPE_AX1200 && _psu_type != TYPE_AX1500 && i >= chnnum) ? i + 2 : i) == -1) return -1;

	if ((ret = read_data_psu(fd, 0x8b, 2)) == NULL) return -1;
	    if (i == chnnum) _rail12v.atx.voltage = convert_byte_float(ret);
	    else if (i == chnnum + 1) _rail12v.peripheral.voltage = convert_byte_float(ret);
	    else _rail12v.pcie[i].voltage = convert_byte_float(ret);
	free(ret);
	if ((ret = read_data_psu(fd, 0xe8, 2)) == NULL) return -1;
	    if (i == chnnum) _rail12v.atx.current = convert_byte_float(ret);
	    else if (i == chnnum + 1) _rail12v.peripheral.current = convert_byte_float(ret);
	    else _rail12v.pcie[i].current = convert_byte_float(ret);
	free(ret);
	if ((ret = read_data_psu(fd, 0xe9, 2)) == NULL) return -1;
	    if (i == chnnum) _rail12v.atx.power = convert_byte_float(ret);
	    else if (i == chnnum + 1) _rail12v.peripheral.power = convert_byte_float(ret);
	    else _rail12v.pcie[i].power = convert_byte_float(ret);
	free(ret);
	if ((ret = read_data_psu(fd, 0xea, 2)) == NULL) return -1;
	if (ret[0] == 0xff || (f = convert_byte_float(ret)) > 40.0) {
	    f = 40.0;
	    if (i == chnnum) {
		_rail12v.atx.ocp_enabled = false;
		_rail12v.atx.ocp_limit = f;
	    }else if (i == chnnum + 1) {
		_rail12v.peripheral.ocp_enabled = false;
		_rail12v.peripheral.ocp_limit = f;
	    } else {
		_rail12v.pcie[i].ocp_enabled = false;
		_rail12v.pcie[i].ocp_limit = f;
	    }
	} else {
	    if (f < 0.0) f = 0.0;
	    if (i == chnnum) {
		_rail12v.atx.ocp_enabled = true;
		_rail12v.atx.ocp_limit = f;
	    }else if (i == chnnum + 1) {
		_rail12v.peripheral.ocp_enabled = true;
		_rail12v.peripheral.ocp_limit = f;
	    } else {
		_rail12v.pcie[i].ocp_enabled = true;
		_rail12v.pcie[i].ocp_limit = f;
	    }
	}
	free(ret);
    }
    return 0;
}

int read_psu_railmisc(int fd) {
    unsigned char * ret;
    float unk1;

    if (set_main_page(fd, 1) == -1) return -1;

    if ((ret = read_data_psu(fd, 0x96, 2)) == NULL) return -1;
    unk1 = convert_byte_float(ret);
    free(ret);

    if ((ret = read_data_psu(fd, 0x8b, 2)) == NULL) return -1;
    _railmisc.rail_5v.voltage = convert_byte_float(ret);
    free(ret);

    if ((ret = read_data_psu(fd, 0x8c, 2)) == NULL) return -1;
    _railmisc.rail_5v.current = convert_byte_float(ret);
    free(ret);

    _railmisc.rail_5v.power = (unk1 + (_railmisc.rail_5v.voltage * _railmisc.rail_5v.current))/2.0;


    if (set_main_page(fd, 2) == -1) return -1;

    if ((ret = read_data_psu(fd, 0x96, 2)) == NULL) return -1;
    unk1 = convert_byte_float(ret);
    free(ret);

    if ((ret = read_data_psu(fd, 0x8b, 2)) == NULL) return -1;
    _railmisc.rail_3_3v.voltage = convert_byte_float(ret);
    free(ret);

    if ((ret = read_data_psu(fd, 0x8c, 2)) == NULL) return -1;
    _railmisc.rail_3_3v.current = convert_byte_float(ret);
    free(ret);

    _railmisc.rail_3_3v.power = (unk1 + (_railmisc.rail_3_3v.voltage * _railmisc.rail_3_3v.current))/2.0;

    return 0;
}

int read_psu_fan_speed(int fd, float * f) {
    unsigned char * ret = read_data_psu(fd, 0x90, 2);
    if (!ret) return -1;
    *f = convert_byte_float(ret);
    free(ret);
    return 0;
}

int read_psu_temp(int fd, float * f) {
    unsigned char * ret = read_data_psu(fd, 0x8e, 2);
    if (!ret) return -1;
    *f = convert_byte_float(ret);
    free(ret);
    return 0;
}

char * dump_psu_type(int type) {
    switch (type) {
	case TYPE_AX860: return "AX860i"; break;
	case TYPE_AX1200: return "AX1200i"; break;
	case TYPE_AX1500: return "AX1500i"; break;
	default: return "AX760i"; break;
    }
}

int setup_dongle(int fd) {
    unsigned char * ret;
    char d[7] = {17, 2, 100, 0, 0, 0, 0};
    float f = 0.0;

    if ((ret = read_dongle_name(fd)) == NULL) return -1;
    printf("Dongle name: %s\n", ret);
    free(ret);

    if (data_write_dongle(fd, d, 7) != 0) return -1;

    ret = data_read_dongle(fd, 1, NULL);

    if (!ret) return -1;

    free(ret);

    if (read_dongle_version(fd, &f) == -1) return -1;

    printf("Dongle version: %0.1f\n", f);

    if ((ret = read_data_psu(fd, 0x9a, 7)) == NULL) return -1;
//    dump(ret, 7);

    if (!memcmp(ret, "AX860", 5)) _psu_type = TYPE_AX860;
    if (!memcmp(ret, "AX1200", 6)) _psu_type = TYPE_AX1200;
    if (!memcmp(ret, "AX1500", 6)) _psu_type = TYPE_AX1500;
    // AX760 = default

    free(ret);

    printf("PSU type: %s\n", dump_psu_type(_psu_type));

    return 0;
}


int main (int argc, char * argv[]) {
    int fd;
    struct termios tio;
    int i;

    _psu_type = TYPE_AX760;

    printf("Corsair AXi Series PSU Monitor\n");
    printf("(c) 2014 Andras Kovacs - andras@sth.sze.hu\n");
    printf("-------------------------------------------\n\n");

    if (argc < 2) {
	printf("usage: %s <serial port device>\n", argv[0]);
	return 0;
    }

    fd = open(argv[1], O_RDWR | O_NONBLOCK);

    if (fd < 0) {
        printf("Serial port (%s) open error.\n", argv[1]);
        return -1;
    }

    memset(&tio, 0, sizeof(tio));
    tio.c_cflag = B115200 | CS8 | CLOCAL | CREAD;
    tio.c_iflag = IGNPAR;
    tio.c_oflag = 0;
    tio.c_lflag = 0;
    tio.c_cc[VTIME]= 0;
    tio.c_cc[VMIN] = 1;
    tcsetattr(fd, TCSANOW, &tio);
    tcflush(fd, TCIOFLUSH);

    float f;

    if (setup_dongle(fd) == -1) exit(-1);
    if (read_psu_fan_speed(fd, &f) == -1) exit(-1);
    printf("Fan speed: %0.2f RPM\n", f);
    if (read_psu_temp(fd, &f) == -1) exit(-1);
    printf("Temperature: %0.2f °C\n", f);

    if (read_psu_main_power(fd) == -1) exit(-1);

    printf("Voltage: %0.2f V\n", _psumain.voltage);
    printf("Current: %0.2f A\n", _psumain.current);
    printf("Input power: %0.2f W\n", _psumain.inputpower);
    printf("Output power: %0.2f W\n", _psumain.outputpower);
    if (_psu_type == TYPE_AX1500)
	    printf("Cable type: %s\n", (_psumain.cabletype ? "20 A" : "15 A"));
    printf("Efficiency: %0.2f %%\n", _psumain.efficiency);

    if (read_psu_rail12v(fd) == -1) exit(-1);

    int chnnum = (_psu_type == TYPE_AX1500 ? 10 : ((_psu_type == TYPE_AX1200) ? 8 : 6));
    for (i = 0; i < chnnum; i++) {
	printf("PCIe %02d Rail:    %0.2f V, %0.2f A, %0.2f W, OCP %s (Limit: %0.2f A)\n", i, _rail12v.pcie[i].voltage,
	_rail12v.pcie[i].current, _rail12v.pcie[i].power, (_rail12v.pcie[i].ocp_enabled ? "enabled " : "disabled"), _rail12v.pcie[i].ocp_limit);
    }

    printf("ATX Rail:        %0.2f V, %0.2f A, %0.2f W, OCP %s (Limit: %0.2f A)\n", _rail12v.atx.voltage,
	_rail12v.atx.current, _rail12v.atx.power, (_rail12v.atx.ocp_enabled ? "enabled " : "disabled"), _rail12v.atx.ocp_limit);

    printf("Peripheral Rail: %0.2f V, %0.2f A, %0.2f W, OCP %s (Limit: %0.2f A)\n", _rail12v.peripheral.voltage,
	_rail12v.peripheral.current, _rail12v.peripheral.power, (_rail12v.peripheral.ocp_enabled ? "enabled " : "disabled"), _rail12v.peripheral.ocp_limit);

    if(read_psu_railmisc(fd) == -1) exit(-1);

    printf("5V Rail:         %0.2f V, %0.2f A, %0.2f W\n", _railmisc.rail_5v.voltage, _railmisc.rail_5v.current, _railmisc.rail_5v.power);
    printf("3.3V Rail:       %0.2f V, %0.2f A, %0.2f W\n", _railmisc.rail_3_3v.voltage, _railmisc.rail_3_3v.current, _railmisc.rail_3_3v.power);

    close(fd);
    return 0;
}