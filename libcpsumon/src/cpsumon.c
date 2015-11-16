#include <cpsumon.h>

int open_usb(char *device)
{

    int fd = open(device, O_RDWR | O_NONBLOCK);

    if (fd < 0) {
        printf("Serial port (%s) open error.\n", device);
        return -1;
    }
    return fd;
}

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

int set_psu_fan_fixed_percent(int fd, float f) {
    char percent = f;
    unsigned char * ret = write_data_psu(fd, 0x3b, &percent, 1);
    if (!ret) return -1;
    return 0;
}

int read_psu_fan_fixed_percent(int fd, int * i) {
    unsigned char * ret = read_data_psu(fd, 0x3b, 1);
    if (!ret) return -1;
    *i = (unsigned char)*ret;
    free(ret);
    return 0;
}

int set_psu_fan_mode(int fd, int m) {
    char mode = m;
    unsigned char * ret = write_data_psu(fd, 0xf0, &mode, 1);
    if (!ret) return -1;
    free(ret);
    return 0;
}

int read_psu_fan_mode(int fd, int * m) {
    unsigned char * ret = read_data_psu(fd, 0xf0, 1);
    if (!ret) return -1;
    *m = (unsigned char)*ret;
    free(ret);
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
