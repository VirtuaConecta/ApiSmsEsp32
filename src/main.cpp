#include <HTTPClient.h>
#include <DNSServer.h> 
#include <WebServer.h>
#include <WiFiManager.h> 
#include <ArduinoJson.h>


//variaveis
#define INTERVALO 5000 
#define TINY_GSM_MODEM_SIM800 //modem é um sim800
#define TINY_GSM_RX_BUFFER 1024 //rx buffer = 1k

//Usando ESP32 TTGO T-Call  
#define MODEM_RST 5
#define MODEM_PWKEY 4
#define MODEM_POWER_ON 23
#define MODEM_TX 27
#define MODEM_RX 26
#define I2C_SDA 21
#define I2C_SCL 22
#define ENABLE_DEBUG 0 // Defina como 1 para habilitar o debug, 0 para desabilitar

//as definições acima precisam ser feitas antes de chamar TinyGsmClient.h 
#include <TinyGsmClient.h>




bool serverStarted = false; // Flag para verificar se o servidor foi iniciado

uint32_t lastTimeRead = 0;
// JSON data buffer
StaticJsonDocument<250> jsonDocument;
char buffer[250];


// === configuracoes do wiFi ==================================
WiFiManager wifiManager;//Objeto de manipulação do wi-fi
 void saveConfigCallback (); 
 void configModeCallback (WiFiManager *myWiFiManager);
//=============================================================

//==== SERVIDOR api ===========================================
 WebServer server(80);
//=============================================================


#if ENABLE_DEBUG
#include <StreamDebugger.h>
#endif


// Inicializa a UART para comunicação com o modem
 HardwareSerial SerialAT(1);
#if ENABLE_DEBUG
  StreamDebugger debugger(SerialAT, Serial);
  TinyGsm modem(debugger);
#else
  TinyGsm modem(SerialAT);
#endif



//Inicialização dos métodos ================
  void conectaWiFi();
  void handlePost();
  void setup_routing(); 
  void setup_gsm();
 void sendSMS(const String& tel, const String& msg) ;

void setup()
{
  Serial.begin(115200);
  pinMode(13,INPUT);
  pinMode(25,OUTPUT); 

  //callback para quando entra em modo de configuração AP
  wifiManager.setAPCallback(configModeCallback); 
  //callback para quando se conecta em uma rede, ou seja, quando passa a trabalhar em modo estação
  wifiManager.setSaveConfigCallback(saveConfigCallback); 

     setup_gsm();

 
}

void loop()
{
 //  Serial.println("executa loop 1..");
     unsigned long now = millis();
   if(now - lastTimeRead > INTERVALO)// a cada 5 segundos verifcamos a conexão do WiFi
    {
       // Serial.println("executa loop 5..");
       conectaWiFi();

        lastTimeRead = now; 
    }
 
  if (WiFi.status() == WL_CONNECTED) {
    if (!serverStarted) {
      // Mostra o MacAddress é útil para redirecionamento na Rede
       Serial.print("MAC Address: ");
       Serial.println(WiFi.macAddress());
      setup_routing(); // Inicia o servidor apenas após a conexão WiFi bem-sucedida
      
    }
    server.handleClient(); // Trata as requisições ao servidor web
  } else {
    // Tratamento caso WiFi esteja desconectado
    if (serverStarted) {
      Serial.println("WiFi desconectado. Servidor não disponível.");
    }
  }
    delay(500);
}

  //Faz a configuração do WiFI
 void conectaWiFi(){

   //Se o botão for pressionado entra no modo de configuração da rede WiFi
   if (digitalRead(13) == LOW) {
         
 
     
      digitalWrite(25,LOW);
      wifiManager.resetSettings();       //Apaga rede salva anteriormente
      if(!wifiManager.startConfigPortal("VIRTUA-SMS", "12345678") ) //Nome da Rede e Senha gerada pela ESP
      {
        Serial.println("Falha ao conectar"); //Se caso não conectar na rede mostra mensagem de falha
        delay(2000);
        ESP.restart(); //Reinicia ESP após não conseguir conexão na rede
      }
      else{       //Se caso conectar 
        Serial.println("Conectado na Rede!!!");
        ESP.restart(); //Reinicia ESP após conseguir conexão na rede 
      } 
   }
 
   if(WiFi.status()== WL_CONNECTED){ //Se conectado na rede
      digitalWrite(25,HIGH); //Acende LED AZUL
    //  Serial.println("WiFi Conectado");

      
   }
   else{ //se não conectado na rede
      digitalWrite(32,LOW); //Apaga LED AZUL
      wifiManager.autoConnect();//Função para se autoconectar na rede
   }  




 }
 
//callback modo AP
void configModeCallback (WiFiManager *myWiFiManager) {  
   Serial.println("Entrou no modo de configuração");
  Serial.println(WiFi.softAPIP()); //imprime o IP do AP
  Serial.println(myWiFiManager->getConfigPortalSSID()); //imprime o SSID criado da rede
}
 
//Callback que indica que salvamos uma nova rede para se conectar (modo estação)
void saveConfigCallback () {
   Serial.println("Configuração salva");
}

void setup_routing() {	 	 

  server.on("/sms", HTTP_POST, handlePost);	 	 
  	 	 
  // start server	 	 
  server.begin();	 	
   serverStarted = true; 
   Serial.println("Api iniciada!");
}



void handlePost() {

  if (server.hasArg("plain") == false) {
   // Serial.println("======= Erro no Post =======");
  }
    String body = server.arg("plain");
  deserializeJson(jsonDocument, body);
  
  // Captura dados da mensagem
   String tel = jsonDocument["telefone"].as<String>(); // Garante conversão para String;
   String msg = jsonDocument["msg"].as<String>(); // Garante conversão para String;
   
   // Verifica se 'tel' e 'msg' estão definidos e não são nem vazios nem "null"
    if (!tel.isEmpty() && tel != "null" && !msg.isEmpty() && msg != "null") 
    {
      Serial.println("SMS tel:"+ tel +" MSG:"+msg);
      sendSMS(tel, msg);  
        server.send(200, "application/json", "sucesso");
    }
    else
    {
      
      // Serial.println("Falha SMS dados não recebidos");
      // server.send(400, "application/json", "falha");
    }
}

void sendSMS(const String& tel, const String& msg) {
  if(modem.sendSMS(tel, msg)) {
    Serial.println("SMS enviado com sucesso.");
    
  } else {
    Serial.println("Falha ao enviar SMS.");
    // Tratamento de falha no envio do SMS
  }
}

void setup_gsm() {
  // Configura os pinos do modem
  pinMode(MODEM_PWKEY, OUTPUT);
  pinMode(MODEM_RST, OUTPUT);
  pinMode(MODEM_POWER_ON, OUTPUT);
  digitalWrite(MODEM_PWKEY, LOW);
  digitalWrite(MODEM_RST, HIGH);
  digitalWrite(MODEM_POWER_ON, HIGH);

  // Inicia a comunicação serial com o modem
  SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(3000); // Espera o modem inicializar

  Serial.println(F("Modem inicializando..."));
  modem.restart(); // Reinicia o modem
}
