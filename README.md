# Steam Punk Clock

## Rest API
La base de l'URL est http://esp-home.local
Ci dessous la liste des API disponible : 

   - GET URL/api/v1/jsonTime : permet de lire l'heure courante. Renvoi un json comme ceci
    ```
     {
        "hours" : 12,
        "minutes" : 24
     }
     ```

   - POST URL/api/v1/jsonTime : Permet de seter l'heure courante. Prend une Pay load en Json
    ```
     {
        "hours" : 12,
        "minutes" : 24
     }
    ```
    - POST URL/api/v1/time?hours=12&minutes=24 : permet de fixer l'heure avec des Query params
    Peut être testé avec Curl : 
    ```
    curl -X POST "http://esp-home.local/api/v1/time?hours=12&minutes=24"
    ```

    peut se tester avec 
    - POST URL/api/v1/filtering?alpha=80 : permet de fixer le coefficiant du filtre passe haut
    ```
    curl -X POST "http://esp-home.local/api/v1/filtering?alpha=50"
    ```

# MDNS

L'utilisation de MDNS permet de faire une resolution de nom de l'esp32.
Ici l'esp est rendu visible en tant que esp-home.local
Les api rest sont : GETou SET /api/v1/time
Exemple : esp-home.local/api/v1/time

La method Get Time renvoie un JSon {hour:12, minutes:25}
La method Set Time prend en payload un Json identique 

# HTTP Restful API Server Example

(See the README.md file in the upper level 'examples' directory for more information about examples.)

## Overview


#### Pin Assignment:

Only if you deploy the website to SD card, then the following pin connection is used in this example.

| ESP32  | SD Card   |
| ------ | -------   |
| GPI36  | Analog in |


### Configure the project

Open the project configuration menu (`idf.py menuconfig`).

In the `Example Connection Configuration` menu:

* Choose the network interface in `Connect using`  option based on your board. Currently we support both Wi-Fi and Ethernet.
* If you select the Wi-Fi interface, you also have to set:
  * Wi-Fi SSID and Wi-Fi password that your esp32 will connect to.
* If you select the Ethernet interface, you also have to set:
  * PHY model in `Ethernet PHY` option, e.g. IP101.
  * PHY address in `PHY Address` option, which should be determined by your board schematic.
  * EMAC Clock mode, GPIO used by SMI.

In the `Example Configuration` menu:

* Set the domain name in `mDNS Host Name` option.
* Choose the deploy mode in `Website deploy mode`, currently we support deploy website to host PC, SD card and SPI Nor flash.
  * If we choose to `Deploy website to host (JTAG is needed)`, then we also need to specify the full path of the website in `Host path to mount (e.g. absolute path to web dist directory)`.
* Set the mount point of the website in `Website mount point in VFS` option, the default value is `/www`.

### Build and Flash

After the webpage design work has been finished, you should compile them by running following commands:

```bash
cd path_to_this_example/front/web-demo
npm install
npm run build
```
> **_NOTE:_** This example needs `nodejs` version `v10.19.0`

After a while, you will see a `dist` directory which contains all the website files (e.g. html, js, css, images).

Run `idf.py -p PORT flash monitor` to build and flash the project..

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) for full steps to configure and use ESP-IDF to build projects.

### Extra steps to do for deploying website by semihost

We need to run the latest version of OpenOCD which should support semihost feature when we test this deploy mode:

```bash
openocd-esp32/bin/openocd -s openocd-esp32/share/openocd/scripts -f board/esp32-wrover-kit-3.3v.cfg
```

## Example Output

### Render webpage in browser

In your browser, enter the URL where the website located (e.g. `http://esp-home.local`). You can also enter the IP address that ESP32 obtained if your operating system currently don't have support for mDNS service.

Besides that, this example also enables the NetBIOS feature with the domain name `esp-home`. If your OS supports NetBIOS and has enabled it (e.g. Windows has native support for NetBIOS), then the URL `http://esp-home` should also work.

![esp_home_local](https://dl.espressif.com/dl/esp-idf/docs/_static/esp_home_local.gif)

### ESP monitor output

In the *Light* page, after we set up the light color and click on the check button, the browser will send a post request to ESP32, and in the console, we just print the color value.

```bash
I (6115) example_connect: Connected to Ethernet
I (6115) example_connect: IPv4 address: 192.168.2.151
I (6325) esp-home: Partition size: total: 1920401, used: 1587575
I (6325) esp-rest: Starting HTTP Server
I (128305) esp-rest: File sending complete
I (128565) esp-rest: File sending complete
I (128855) esp-rest: File sending complete
I (129525) esp-rest: File sending complete
I (129855) esp-rest: File sending complete
I (137485) esp-rest: Light control: red = 50, green = 85, blue = 28
```

## Troubleshooting

1. Error occurred when building example: `...front/web-demo/dist doesn't exit. Please run 'npm run build' in ...front/web-demo`.
   * When you choose to deploy website to SPI flash, make sure the `dist` directory has been generated before you building this example.

(For any technical queries, please open an [issue](https://github.com/espressif/esp-idf/issues) on GitHub. We will get back to you as soon as possible.)
