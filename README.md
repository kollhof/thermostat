# thermostat
ESP32 based thermostat for a DIY heating system


# gettting started


Create a factory NVS partition with hardware and homekit config

```bash
../esp-homekit-sdk/tools/factory_nvs_gen/factory_nvs_gen.py --infile ./config-nvs.csv [homekit-setup-code] [homekit-setup-id] build/factory_nvs
```

or

```bash
../esp-homekit-sdk/tools/mfg_homekit/hk_mfg_gen.py --conf mfg-config.csv --values mfg-values.csv --prefix thermo --cid 9 --outdir ./secret/mfg/ --size 0x6000
```


Create Homekit QR code

```bash
./esp-homekit-sdk/tools/setup_payload_gen/setup_payload_gen.py 9 [homekit-setup-code] [homekit-setup-id]
```



Flash it

```bash
esptool.py write_flash 0x340000 secret/mfg/bin/thermo-[homekit-setup-id].bin
```

