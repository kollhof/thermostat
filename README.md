# thermostat
ESP32 based thermostat for a DIY heating system


# gettting started


Create a factory NVS partitions with hardware and homekit config:

```bash
../esp-homekit-sdk/tools/mfg_homekit/hk_mfg_gen.py --conf mfg-config.csv --values mfg-values.csv --prefix thermo --cid 9 --outdir ./secret/mfg/ --size 0x6000
```

Flash factory partitions to individual devices:

```bash
esptool.py write_flash 0x340000 secret/mfg/bin/thermo-[homekit-setup-id].bin
```


Flash firmware:

```bash
idf.py clean build flash
```


Update firmware over the air:

The Device ID can be found as serial number in Homekit once paired.
```bash
idf.py -DOTA_DEVICE_ID=[device-id] clean build flash-ota
```



# progress

This first version used an ESP32-WROVER-V4 powered by a Hi-link power supply all badly soldered to a protboard with way too thick wires.

![prototype 0](./docs/thermo0.jpg)


After reading up on powersupplies I decided to ditch the Hi-link for a Recom RAC03-05SK. It does not need any extra components to be safe which made things a bit simpler.

![prototype 1](./docs/thermo1.jpg)

The female pin headers were awful to work with and I wanted a bit more ports exposed for experimenting.

![prototype 2](./docs/thermo2.jpg)

I also started to get a tiny bit better at soldering, and my choice of wires improved as well.

![prototype 2](./docs/thermo2b.jpg)

Now I had a pretty well working little board. I used Dallas ds18b20 sensors to measure temperatur and relays to drive the electric radiators.

I needed about 5 of these boards, wiring and soldering everything got old pretty quickly. Instead of continuing to use protoboards, I decided to design my own PCB. I downloaded KiCad, looked at many ESP-32 dev board schematics for inspiration and created my own little board.

![prototype 3](./docs/thermo3.jpg)

I had them printed by https://aisler.net and few days later I had a 3 PCBs and all the parts needed. I soldered all the SMD components using a hot air blower,and everything just worked!




