#include <cpsumon.h>
#include <cpsumoncli.h>


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

    fd = open_usb(argv[1]);
    if(fd == -1)
    {
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

    if (read_psu_fan_mode(fd, &i) == -1) exit(-1);
    if (i == FANMODE_AUTO) printf("Fan mode: Auto\n");
    else if (i == FANMODE_FIXED) {
      printf("Fan mode: Fixed\n");

      if (read_psu_fan_fixed_percent(fd, &i) == -1) exit(-1);
      printf("Fan setting: %d %%\n", i);
    }

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
