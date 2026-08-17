# Firmware ESP32 — Ranomadio Solar / AquaSolar

Le fichier recommandé pour la Phase 1 est **`RanomadioSolar.ino`**. Il implémente le serveur Wi-Fi local et l'API consommée par le dashboard.

## Matériel

- ESP32 DevKit
- SEN0169-V2 (pH)
- DHT11 (température + humidité)
- HC-SR04 (niveau)
- LED verte / jaune / rouge
- relais ou MOSFET pour pompe 12 V

## Câblage

| Élément | ESP32 | Remarque |
|---|---:|---|
| SEN0169-V2 VCC | 5V | module : 3.3–5.5 V |
| SEN0169-V2 GND | GND | masse commune |
| SEN0169-V2 A | GPIO34 | entrée ADC |
| DHT11 DATA | GPIO4 | pull-up 10 kΩ si nécessaire |
| HC-SR04 TRIG | GPIO5 | sortie |
| HC-SR04 ECHO | GPIO18 | **diviseur obligatoire si ECHO = 5 V** |
| LED verte | GPIO25 | résistance 220–330 Ω |
| LED jaune | GPIO26 | résistance 220–330 Ω |
| LED rouge | GPIO27 | résistance 220–330 Ω |
| IN relais/MOSFET | GPIO23 | ne jamais brancher la pompe directement |

La pompe 12 V doit avoir sa propre alimentation. Le GPIO23 commande uniquement l'étage de puissance.

## Bibliothèques Arduino

Installer dans le Library Manager :

- DHT sensor library by Adafruit
- Adafruit Unified Sensor
- ArduinoJson

Le support ESP32 doit être installé depuis Boards Manager.

## Wi-Fi

`RanomadioSolar.ino` démarre un point d'accès :

- SSID : `Ranomadio-Solar`
- mot de passe : `ranomadio123`
- IP : `192.168.4.1`

Le dashboard peut donc appeler directement `http://192.168.4.1/api/status` lorsque le téléphone/PC est connecté au Wi-Fi de l'ESP32. L'ESP32 peut également rejoindre un routeur si `STA_SSID` et `STA_PASSWORD` sont renseignés dans le fichier.

## API compatible dashboard

### GET `/api/status`

Retourne : `ph`, `temperature`, `humidity`, `distance`, `waterLevel`, `pump`, `auto`, `status`, `timestamp`, `battery`.

### GET `/api/health`

Permet de vérifier rapidement que l'ESP32 répond.

### POST `/api/config`

```json
{"phMin":6.5,"phMax":8.5}
```

### POST `/api/pump`

```json
{"on":true}
```

Le firmware refuse l'activation si le niveau d'eau est critique.

### POST `/api/auto`

```json
{"enabled":true}
```

## Calibration SEN0169-V2

Le SEN0169-V2 fournit une sortie analogique 0–3 V. DFRobot recommande une calibration en deux points avec solutions tampons, notamment pH 7 et pH 4. Le firmware possède des commandes série :

- `ph` : afficher tension et pH
- `cal7` : mémoriser la tension actuelle comme point pH 7
- `cal4` : mémoriser la tension actuelle comme point pH 4
- `save` : enregistrer la calibration
- `info` : afficher la configuration
- `help` : afficher l'aide

Moniteur série : **115200 bauds**.

Documentation DFRobot : https://wiki.dfrobot.com/sen0169-v2/docs/24420

## Protections pompe

Le firmware arrête automatiquement la pompe si le niveau passe sous 15 %, si le pH revient dans la plage normale en mode AUTO, ou après 30 secondes de fonctionnement continu. Un délai de sécurité de 10 secondes empêche un redémarrage immédiat.

Ces valeurs sont adaptées au prototype et doivent être validées avant toute utilisation réelle.

## Test

1. Téléverser `RanomadioSolar.ino`.
2. Ouvrir le moniteur série à 115200.
3. Connecter le PC/téléphone à `Ranomadio-Solar`.
4. Ouvrir `http://192.168.4.1/api/health`.
5. Dans AquaSolar, utiliser `http://192.168.4.1` comme URL ESP32.
6. Sélectionner **LOCAL ESP32**.
7. Vérifier les mesures et l'état de la pompe.

### Attention HTTPS

Un dashboard hébergé sur Render en HTTPS peut bloquer les appels HTTP directs vers `192.168.4.1` à cause du mixed content. Pour la démonstration ESP32 réelle, utiliser le dashboard local/HTTP ou prévoir un relais backend sécurisé. Le mode SIMULATION reste disponible pour Render.
