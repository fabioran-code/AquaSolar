/* AquaSolar MQTT bridge
 * Browser -> HiveMQ Cloud over secure WebSockets.
 * The MQTT password is requested at runtime and stored only in localStorage.
 * For production, use a dedicated low-privilege HiveMQ credential/ACL.
 */
(function(){
  const CFG={host:'dd3778f4cb2143faba675c1b1bc30546.s1.eu.hivemq.cloud',port:8884,path:'/mqtt',username:'AquaSolar',deviceId:'aquasolar-device01'};
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
    const t=document.getElementById('connectionText'); if(t&&AquaSolarMQTT.mode==='mqtt')t.textContent=connected?'MQTT · HiveMQ Cloud':'MQTT hors ligne';
  }
  function connect(){
    AquaSolarMQTT.mode='mqtt'; localStorage.setItem('aquasolarMode','mqtt'); setModeText();
    if(typeof mqtt==='undefined'){alert('Bibliothèque MQTT indisponible. Vérifiez la connexion Internet.');return;}
    const password=localStorage.getItem('aquasolarMqttPassword')||prompt('Mot de passe MQTT HiveMQ pour AquaSolar :');
    if(!password)return;
    localStorage.setItem('aquasolarMqttPassword',password);
    if(client){try{client.end(true)}catch(e){}}
    client=mqtt.connect(`wss://${CFG.host}:${CFG.port}${CFG.path}`,{username:CFG.username,password,clean:true,reconnectPeriod:3000,connectTimeout:8000,clientId:`AquaSolarWeb-${Math.random().toString(16).slice(2)}`});
    client.on('connect',()=>{connected=true;client.subscribe([TELEMETRY,STATUS],{qos:0});setModeText();document.getElementById('aquasolarMqttConnect').textContent='MQTT connecté';});
    client.on('message',(topic,payload)=>{try{const d=JSON.parse(payload.toString());if(topic===TELEMETRY){lastTelemetry={...d,timestamp:d.timestamp||Date.now()};} }catch(e){console.warn('MQTT payload invalide',e)}});
    client.on('close',()=>{connected=false;setModeText();}); client.on('error',e=>console.warn('MQTT',e));
  }
  function disconnect(){connected=false;if(client){try{client.end(true)}catch(e){}}client=null;setModeText();}
  function publishCommand(payload){if(!client||!connected)return Promise.reject(new Error('MQTT non connecté'));client.publish(COMMAND,JSON.stringify(payload),{qos:0});return Promise.resolve();}
  window.AquaSolarMQTT.publishCommand=publishCommand;

  // Intercept the existing app's /api/status and /api/config calls without breaking its UI.
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
