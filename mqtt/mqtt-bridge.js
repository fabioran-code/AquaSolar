/* AquaSolar MQTT bridge
 * Browser: MQTT over secure WebSockets.
 * Demo broker only. Replace constants with a private authenticated broker for production.
 */
(function(){
  const BROKER='wss://test.mosquitto.org:8081/mqtt';
  const ROOT='aquasolar';
  const clientId='dashboard-'+Math.random().toString(16).slice(2,10);
  let client=null,lastTelemetry=null,lastDevice=null,ready=false;
  window.AquaSolarMQTT={getTelemetry:()=>lastTelemetry,isConnected:()=>ready,device:()=>lastDevice};
  function connect(){
    if(!window.mqtt)return setTimeout(connect,200);
    client=window.mqtt.connect(BROKER,{clientId,clean:true,connectTimeout:5000,reconnectPeriod:3000});
    client.on('connect',()=>{ready=true;client.subscribe(ROOT+'/+ /telemetry'.replace(' ','')).subscribe(ROOT+'/+/status');});
    client.on('close',()=>ready=false);
    client.on('error',()=>ready=false);
    client.on('message',(topic,payload)=>{
      try{const d=JSON.parse(payload.toString());if(topic.endsWith('/telemetry')){lastTelemetry=d;lastDevice=topic.split('/')[1];}}
      catch(e){}
    });
  }
  window.AquaSolarPublishCommand=function(device,payload){
    if(client&&ready&&device)client.publish(ROOT+'/'+device+'/command',JSON.stringify(payload),{qos:0});
  };
  connect();
  const originalFetch=window.fetch.bind(window);
  window.fetch=function(input,init){
    const url=typeof input==='string'?input:(input&&input.url)||'';
    if(url.includes('/api/status')){
      if(lastTelemetry)return Promise.resolve(new Response(JSON.stringify(lastTelemetry),{status:200,headers:{'Content-Type':'application/json'}}));
      return Promise.reject(new Error('MQTT telemetry unavailable'));
    }
    if(url.includes('/api/config')&&init&&init.method==='POST'){
      try{const body=JSON.parse(init.body||'{}');AquaSolarPublishCommand(lastDevice||'device01',{config:body});}catch(e){}
      return Promise.resolve(new Response('{"ok":true}',{status:200,headers:{'Content-Type':'application/json'}}));
    }
    return originalFetch(input,init);
  };
  // In simulation, publish the same telemetry schema for demonstration/testing.
  setInterval(()=>{
    const mode=document.getElementById('connectionMode')?.value;
    if(mode!=='simulation'||!client||!ready)return;
    const n=id=>parseFloat(document.getElementById(id)?.textContent||'0');
    const payload={ph:n('phValue'),temperature:n('tempValue'),humidity:n('humidityValue'),waterLevel:n('levelValue'),pump:(document.getElementById('pumpState')?.textContent||'').includes('FONCTIONNEMENT'),status:(document.getElementById('waterStatus')?.textContent||'').includes('PROPRE')?'NORMAL':'ANORMAL',battery:n('batteryValue'),timestamp:Date.now()};
    client.publish(ROOT+'/simulation/telemetry',JSON.stringify(payload),{qos:0});
  },4000);
})();
