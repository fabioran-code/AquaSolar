/* AquaSolar MQTT bridge
 * Browser -> EMQX Cloud over secure WebSockets.
 * The MQTT password is requested at runtime and stored only in localStorage.
 * Configure the EMQX endpoint in mqtt/emqx-config.example.js or override it
 * with window.AquaSolarMQTTConfig before this script loads.
 */
(function(){
  const external=window.AquaSolarMQTTConfig||{};
  const CFG={
    host:external.host||'YOUR_EMQX_ENDPOINT',
    port:Number(external.port||8084),
    path:external.path||'/mqtt',
    protocol:external.protocol||'wss',
    username:external.username||'AquaSolar',
    deviceId:external.deviceId||'aquasolar-device01'
  };
  const TELEMETRY=`aquasolar/${CFG.deviceId}/telemetry`;
  const STATUS=`aquasolar/${CFG.deviceId}/status`;
  const COMMAND=`aquasolar/${CFG.deviceId}/command`;
  let client=null,lastTelemetry=null,connected=false;
  window.AquaSolarMQTT={cfg:CFG,get connected(){return connected},get telemetry(){return lastTelemetry},mode:localStorage.getItem('aquasolarMode')||'simulation'};

  function addModeUI(){
    const card=document.querySelector('.status-side'); if(!card||document.getElementById('aquasolarModeSelect'))return;
    const wrap=document.createElement('div'); wrap.style.marginTop='10px';
    wrap.innerHTML='<label style="display:block;font-size:.72rem;font-weight:700;margin-bottom:4px">SOURCE</label><select id="aquasolarModeSelect" style="padding:8px;border-radius:8px;border:1px solid #d8e7e1;background:#fff;font-weight:700"><option value="simulation">🧪 SIMULATION</option><option value="mqtt">📡 MODE RÉEL MQTT</option></select><button id="aquasolarMqttConnect" style="margin-top:6px;padding:7px 10px;border:0;border-radius:8px;cursor:pointer">Connecter MQTT</button>';
    card.appendChild(wrap);
    const select=wrap.querySelector('#aquasolarModeSelect'); select.value=AquaSolarMQTT.mode;
    select.onchange=()=>{AquaSolarMQTT.mode=select.value;localStorage.setItem('aquasolarMode',select.value);if(select.value==='mqtt')connect();else disconnect();};
    wrap.querySelector('#aquasolarMqttConnect').onclick=connect;
  }
  function setModeText(){
    const el=document.getElementById('modeLabel'); if(el)el.textContent=AquaSolarMQTT.mode==='mqtt'?(connected?'RÉEL MQTT':'RÉEL — CONNEXION...'):'SIMULATION';
    const t=document.getElementById('connectionText'); if(t&&AquaSolarMQTT.mode==='mqtt')t.textContent=connected?'MQTT · EMQX Cloud':'MQTT hors ligne';
  }
  function connect(){
    AquaSolarMQTT.mode='mqtt'; localStorage.setItem('aquasolarMode','mqtt'); setModeText();
    if(CFG.host==='YOUR_EMQX_ENDPOINT'){alert('Configurez l\'endpoint EMQX Cloud dans mqtt/emqx-config.example.js.');return;}
    if(typeof mqtt==='undefined'){alert('Bibliothèque MQTT indisponible. Vérifiez la connexion Internet.');return;}
    const password=localStorage.getItem('aquasolarMqttPassword')||prompt('Mot de passe MQTT EMQX pour AquaSolar :');
    if(!password)return;
    localStorage.setItem('aquasolarMqttPassword',password);
    if(client){try{client.end(true)}catch(e){}}
    const url=`${CFG.protocol}://${CFG.host}:${CFG.port}${CFG.path}`;
    client=mqtt.connect(url,{username:CFG.username,password,clean:true,reconnectPeriod:3000,connectTimeout:8000,clientId:`AquaSolarWeb-${Math.random().toString(16).slice(2)}`});
    client.on('connect',()=>{connected=true;client.subscribe([TELEMETRY,STATUS],{qos:0});setModeText();const b=document.getElementById('aquasolarMqttConnect');if(b)b.textContent='MQTT connecté';});
    client.on('message',(topic,payload)=>{try{const d=JSON.parse(payload.toString());if(topic===TELEMETRY){lastTelemetry={...d,timestamp:d.timestamp||Date.now()};}}catch(e){console.warn('MQTT payload invalide',e);}});
    client.on('close',()=>{connected=false;setModeText();});
    client.on('error',e=>{connected=false;setModeText();console.warn('MQTT/EMQX',e);});
  }
  function disconnect(){connected=false;if(client){try{client.end(true)}catch(e){}}client=null;setModeText();}
  function publishCommand(payload){if(!client||!connected)return Promise.reject(new Error('MQTT non connecté'));client.publish(COMMAND,JSON.stringify(payload),{qos:0});return Promise.resolve();}
  window.AquaSolarMQTT.publishCommand=publishCommand;

  const nativeFetch=window.fetch.bind(window);
  window.fetch=function(input,init){
    const url=typeof input==='string'?input:(input&&input.url)||'';
    if(url.includes('/api/status')){
      if(AquaSolarMQTT.mode!=='mqtt')return Promise.reject(new Error('Simulation mode'));
      if(!connected||!lastTelemetry)return Promise.reject(new Error('MQTT telemetry unavailable'));
      return Promise.resolve(new Response(JSON.stringify(lastTelemetry),{status:200,headers:{'Content-Type':'application/json'}}));
    }
    if(url.includes('/api/config')&&AquaSolarMQTT.mode==='mqtt'){
      let body={};try{body=typeof init?.body==='string'?JSON.parse(init.body):{}}catch(e){}
      return publishCommand({type:'config',...body}).then(()=>new Response(JSON.stringify({ok:true}),{status:200,headers:{'Content-Type':'application/json'}}));
    }
    return nativeFetch(input,init);
  };
  document.addEventListener('DOMContentLoaded',()=>{addModeUI();setModeText();});
})();
