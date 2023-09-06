from onl.platform.base import *
from onl.platform.netweb import *

class OnlPlatform_x86_64_netweb_tyrone_g48c210r_r0(OnlPlatformNetweb,
                                              OnlPlatformPortConfig_48x1_4x25_2x100):

    PLATFORM='x86-64-netweb-tyrone-g48c210r-r0'
    MODEL="TYRONE_G48C210R"
    SYS_OBJECT_ID=".48.210.1"

    def baseconfig(self):
        os.system("modprobe i2c-ismt")
        self.insmod('optoe')
        self.insmod('ym2651y')
        for m in [ 'cpld', 'psu', 'leds' ]:
            self.insmod("x86-64-netweb-tyrone-g48c210r-%s.ko" % m)

        ########### initialize I2C bus 0 ###########
        self.new_i2c_devices([
            # initialize multiplexer (PCA9548)
            ('pca9548', 0x77, 1),
            ('pca9548', 0x71, 2),
            ('pca9548', 0x70, 3),
            #initiate CPLD
            ('g48c210r_cpld', 0x60, 3)
            ])

        self.new_i2c_devices([
            # inititate LM77
            ('lm77', 0x48, 14),
            # inititate LM75
            ('lm75', 0x4a, 25),
            ('lm75', 0x4b, 24)
            ])

        self.new_i2c_devices([
            # initiate PSU-1
            ('g48c210r_psu1', 0x50, 10),
            ('ym1921', 0x58, 10),
            # initiate PSU-2
            ('g48c210r_psu2', 0x51, 11),
            ('ym1921', 0x59, 11),
            ])

        # initialize SFP port 49~52
        for port in range(49, 53):
            self.new_i2c_device('optoe2', 0x50, port-31)
            subprocess.call('echo port%d > /sys/bus/i2c/devices/%d-0050/port_name' % (port, port-31), shell=True)

        # initialize SFP port 49~52
        for port in range(53, 55):
            self.new_i2c_device('optoe1', 0x50, port-31)
            subprocess.call('echo port%d > /sys/bus/i2c/devices/%d-0050/port_name' % (port, port-31), shell=True)

        self.new_i2c_device('24c02', 0x57, 1)
        return True
