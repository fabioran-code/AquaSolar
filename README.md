# AquaSolar

Dashboard Web responsive de surveillance et de traitement de l'eau.

## Fonctionnalités
- Dashboard temps réel
- pH, température, humidité, niveau du réservoir
- États rouge / jaune / vert
- Automatisation locale de la pompe par ESP32
- Mode SIMULATION avec fausses données
- Mode réel MQTT
- Historique et export CSV
- Alertes
- Configuration des seuils pH
- Interface mobile responsive
- Manifest PWA

## Architecture MQTT
Le mode réel utilise **EMQX Cloud** comme broker MQTT :

`ESP32 → MQTT/TLS 8883 → EMQX Cloud → MQTT/WSS 8084 → Dashboard AquaSolar`

Topics utilisés :
- `aquasolar/<deviceId>/telemetry`
- `aquasolar/<deviceId>/status`
- `aquasolar/<deviceId>/command`

## Configuration EMQX
1. Créer le déploiement EMQX Cloud.
2. Créer un credential MQTT pour `AquaSolar`.
3. Récupérer l'endpoint exact du déploiement.
4. Remplacer `YOUR_EMQX_ENDPOINT` dans `mqtt/emqx-config.example.js`.
5. Utiliser le port `8084` avec `/mqtt` pour le dashboard Web.
6. Utiliser le port `8883` pour l'ESP32.
7. Vérifier les règles d'authentification/ACL pour les topics AquaSolar.

Ne jamais publier le vrai mot de passe MQTT dans GitHub.

## ESP32
Le firmware MQTT est dans `esp32/ranomadio_solar_mqtt.ino`.

Avant le flash, renseigner :
- `WIFI_SSID`
- `WIFI_PASSWORD`
- `MQTT_HOST`
- `MQTT_USER`
- `MQTT_PASSWORD`

Le firmware conserve la logique de traitement local : l'ESP32 peut continuer à gérer les capteurs, les LEDs et la sécurité de la pompe même si le dashboard ou Internet est indisponible.

## Lancer le dashboard
Ouvrir `index.html` ou héberger le projet sur un serveur statique/Render.

Le mode SIMULATION fonctionne sans ESP32. Pour le mode réel, le navigateur doit pouvoir établir une connexion WebSocket sécurisée vers EMQX Cloud.