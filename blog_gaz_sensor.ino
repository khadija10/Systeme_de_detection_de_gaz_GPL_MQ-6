#include <FS.h> //this needs to be first, or it all crashes and burns...

#include <ESP8266HTTPClient.h>
#include <WiFiManager.h>          // https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h>          // https://github.com/bblanchon/ArduinoJson
#include <ESP8266WiFi.h>
#include <ESP_Mail_Client.h>

#define PIN_MQ6 A0
#define Buzzer  D2

int value;

bool premiereFois = false;

const int limite=650;

//Timer2

const int   watchdog2 = 60000; //  60 sec
unsigned long previousMillis2;
unsigned long currentMillis2;

//Timer1

const int   watchdog1 = 30000; //  60 sec
unsigned long previousMillis1;
unsigned long currentMillis1;


WiFiClient espClient;

bool shouldSaveConfig = false;
#define smtp_server  "smtp.gmail.com"; // le serveur SMTP

#define smtp_port 465; // le port smtp
char email_address[40] = ""; // l'adresse email du recepeteur. cet email doit etre configurer avec le WIFI

#define AUTHOR_EMAIL "mq6@gmail.com" // l'email qui de l'expeditaire
#define AUTHOR_PASSWORD "password" // mot de passe de l'email

/* L'objet Session SMTP utilisé pour l'envoi d'e-mails */
SMTPSession smtp;


#define NUL '\0'
HTTPClient http;

// WiFiManager nécessitant un rappel de sauvegarde de la configuration
void saveConfigCallback () {
  shouldSaveConfig = true;
}



// WiFiManager entrant dans le rappel du mode de configuration
void configModeCallback(WiFiManager *myWiFiManager) {
  // entré en mode de configuration

}

// Enregistrer les paramètres personnalisés dans /config.json sur SPIFFS
void save_settings() {
  DynamicJsonBuffer jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
 
  File configFile = SPIFFS.open("/config.json", "w");
  json.printTo(configFile);
  configFile.close();
}

// Charger les paramètres personnalisés de /config.json sur SPIFFS
void load_settings() {
  if (SPIFFS.begin() && SPIFFS.exists("/config.json")) {
    File configFile = SPIFFS.open("/config.json", "r");
    
    if (configFile) {
      size_t size = configFile.size();
      // Allouer un tampon pour stocker le contenu du fichier.
      std::unique_ptr<char[]> buf(new char[size]);

      configFile.readBytes(buf.get(), size);
      DynamicJsonBuffer jsonBuffer;
      JsonObject& json = jsonBuffer.parseObject(buf.get());
      
      if (json.success()) {
        strcpy(email_address, json["email_address"]);
     
      }
    }
  }
}

/* Fonction de rappel pour obtenir le statut d'envoi de l'e-mail */
void smtpCallback(SMTP_Status status);

//////////////////////////Fonction qui envoie l'email
byte sendEmail()
{
    ESP_Mail_Session session;

  /* Définir la configuration de la session */
  session.server.host_name = smtp_server;
  session.server.port = smtp_port;
  session.login.email = AUTHOR_EMAIL;
  session.login.password = AUTHOR_PASSWORD;
  session.login.user_domain = "";

  /* Déclarer la classe de message */
  SMTP_Message message;

  /* Définir les en-têtes de message */
  message.sender.name = "Alarm Mail";
  message.sender.email = AUTHOR_EMAIL;
  message.subject = "Alarm Mail";
  message.addRecipient("user", email_address);



  /*Envoyer un message HTML*/
  String htmlMsg = "<div style=\"color:#2f4468;\"><h1>L'ALARME SONNE</h1><p>Bonjour il semble qu'il ait du Gaz chez vous. L'alarme s'est declanche</p></div>";
  message.html.content = htmlMsg.c_str();
  message.html.content = htmlMsg.c_str();
  message.text.charSet = "us-ascii";
  message.html.transfer_encoding = Content_Transfer_Encoding::enc_7bit;


  /* Seconnecter au serveur avec la configuration de session */
  if (!smtp.connect(&session)){
      //Serial.println("Debug2");
      return 1;
  }

  /* Commencer à envoyer un e-mail et fermer la session */
  if (!MailClient.sendMail(&smtp, &message))
    Serial.println("Error sending Email, " + smtp.errorReason());
  
}



//////end mail



void setup() {

  Serial.begin(115200);
  WiFiManager wm;

//Definir le mode des pins Buzzer en mode sortie et PIN_MQ6 en entree
  pinMode(Buzzer, OUTPUT);
  pinMode(PIN_MQ6, INPUT);
  
  
  load_settings();
        
  WiFiManager wifiManager;

  wifiManager.setAPCallback(configModeCallback);
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  
    // paramètres personnalisés
  WiFiManagerParameter custom_email_address("email", "Email address", email_address, 40);
  wifiManager.addParameter(&custom_email_address);
    
  // essayer de se connecter ou de revenir à ESP + ChipID AP config mode.
  if ( ! wifiManager.autoConnect()) {
    // réinitialiser et réessayer, ou metter le en veille profonde
    ESP.reset();
    delay(1000);
  }
  
   // lire les paramètres

  strcpy(email_address, custom_email_address.getValue());

  // si nous sommes passés par la configuration, nous devrions enregistrer nos modifications.
  if (shouldSaveConfig) save_settings();
  
  Serial.println(email_address);


  smtp.debug(1);

  /* Définir la fonction de rappel pour obtenir les résultats d'envoi */
  smtp.callback(smtpCallback);
  
}

void loop() {
  
     currentMillis1 = millis();

     if(currentMillis1 - previousMillis1 > watchdog1){
      previousMillis1 = currentMillis1;
      //lire les donnees de capteur de gaz
      value = analogRead(PIN_MQ6);
      //afficher la valeur lue
      Serial.println("VALUE - " + String(value));

      }


   currentMillis2 = millis();
// Si la valeur lue est superieure ou egale a la limite, on active le buzzer puis on envoit le mail d'alarme
  if (value >= limite && (currentMillis2 - previousMillis2 > watchdog2)) { 
     previousMillis2 = currentMillis2;
    if(!premiereFois){
        digitalWrite(Buzzer, HIGH); 
        //Envoyer le mail
        byte ret = sendEmail();
      }
        premiereFois = true;
    }else{
      digitalWrite(Buzzer, LOW);
      premiereFois = false;
      }
}


/* Fonction de rappel pour obtenir le statut d'envoi de l'e-mail */
void smtpCallback(SMTP_Status status){
  /* Afficher l'état actuel */
  Serial.println(status.info());

  /* Afficher le résultat de l'envoi */
  if (status.success()){
    Serial.println("----------------");
    ESP_MAIL_PRINTF("Message sent success: %d\n", status.completedCount());
    ESP_MAIL_PRINTF("Message sent failled: %d\n", status.failedCount());
    Serial.println("----------------\n");
    struct tm dt;

    for (size_t i = 0; i < smtp.sendingResult.size(); i++){
      /* Recuperer l'élément de résultat */
      SMTP_Result result = smtp.sendingResult.getItem(i);
      time_t ts = (time_t)result.timestamp;
      localtime_r(&ts, &dt);

      ESP_MAIL_PRINTF("Message No: %d\n", i + 1);
      ESP_MAIL_PRINTF("Status: %s\n", result.completed ? "success" : "failed");
      ESP_MAIL_PRINTF("Date/Time: %d/%d/%d %d:%d:%d\n", dt.tm_year + 1900, dt.tm_mon + 1, dt.tm_mday, dt.tm_hour, dt.tm_min, dt.tm_sec);
      ESP_MAIL_PRINTF("Recipient: %s\n", result.recipients.c_str());
      ESP_MAIL_PRINTF("Subject: %s\n", result.subject.c_str());
    }
    Serial.println("----------------\n");
    //smtp.sendingResult.clear();
  }
}
