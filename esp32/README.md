# Ranomadio Solar — ESP32 Phase 1

Le firmware `ranomadio_solar.ino` connecte les capteurs physiques au dashboard Ranomadio Solar.

## GPIO

| Élément | GPIO |
|---|---:|
| SEN0169-V2 A | 34 |
| DHT11 DATA | 4 |
| HC-SR04 TRIG | 5 |
| HC-SR04 ECHO | 18 via diviseur 1 kΩ / 2 kΩ |
| LED rouge | 25 |
| LED jaune | 26 |
| LED verte | 27 |
| Commande pompe | 23 |

## Mise en service

1. Installer les bibliothèques `DHT sensor library`, `ArduinoJson` et les dépendances ESP32.
2. Ouvrir `esp32/ranomadio_solar.ino` dans Arduino IDE.
3. Sélectionner une carte ESP32 compatible.
4. Téléverser le firmware.
5. Ouvrir le moniteur série à `115200 bauds`.
6. Connecter le téléphone/PC au Wi-Fi `Ranomadio-ESP32` avec le mot de passe `Ranomadio2026`.
7. Dans Ranomadio Solar > Système, saisir `http://192.168.4.1` puis tester la connexion.

## API

- `GET /api/status` retourne les mesures et l'état de la pompe.
- `POST /api/config` accepte `{"phMin":6.5,"phMax":8.5}`.

## Important

La commande de la pompe reste locale sur l'ESP32. Le GPIO 23 doit commander uniquement un MOSFET/relais adapté. La pompe 12 V ne doit jamais être branchée directement à l'ESP32. Pour le HC-SR04 classique, protéger ECHO avec un diviseur de tension avant GPIO 18.

La batterie n'est pas mesurée en Phase 1 : le dashboard affiche `N/D` jusqu'à l'ajout d'un capteur de tension batterie.

## HTTPS / hébergement

Un navigateur peut bloquer une requête `https://site...` vers `http://192.168.4.1` pour des raisons de mixed content. Pour les premiers tests locaux, ouvrir le dashboard dans un contexte local/HTTP compatible. Pour une version cloud finale, prévoir un backend/relais sécurisé entre le site HTTPS et l'ESP32.
