#include <ESP8266WiFi.h>
#include <MySQL_Connection.h>
#include <MySQL_Cursor.h>

//defines
#define SSID_REDE     "Triplex do Lula"  //Nome da Rede Wifi
#define SENHA_REDE    "felipe1675"       //Senha

IPAddress server_addr(192,168,1,100);   // IP do banco e dados
char user[] = "root";                   // Usuário do banco de dados
char password[] = "1234";               // Senha do banco de dados

//constantes e variáveis globais
long lastConnectionTime; //Ultima Conexão
long intervaloRegistro;  //Tempo de intervalo de Envio
int rele = D1; // pino do rele
char status_rele; //Status do rele L = Ligado, D = Desligado

WiFiClient client;                          //Habilita Wifi Cliente
WiFiServer server(80);                      //Habilita Wifi WebServer
MySQL_Connection conn((Client *)&client);   //Conexão com banco de dados


//Procedimentos
void  EnviaInformacoesBanco(int umidade);     //Grava umidade no banco
void  FazConexaoWiFi(void);                   //Conecta com Wifi
float FazLeituraUmidade(void);                //Faz leitura da Umidade 
void  VerificaIntervaloEnvio(void);           //Busca intervalo de envio no banco de dados
void  EnviaInformacaWebServer(int umidade);   //Envia umidade para WebServer
void  Analisa_Umidade(int umidade);
int  Retornar_Id_Area(void);

void  Insere_Irrigacao(int id_area); 
void  Atualiza_Irrigacao(int id_area);

/*** Implementações ***/

void FazConexaoWiFi(void)
{
    //WIFI CLIENTE
    client.stop();
    Serial.println("Conectando-se à rede WiFi...");
    Serial.println();  
    delay(1000);
    WiFi.begin(SSID_REDE, SENHA_REDE);

    /*Define um IP Fixo para Wifi Cliente*/
    IPAddress ip(192,168,1,150);   
    IPAddress gateway(192,168,1,1);   
    IPAddress subnet(255,255,255,0);   
    WiFi.config(ip, gateway, subnet);

    while (WiFi.status() != WL_CONNECTED) 
    {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connectado com sucesso!");  
    Serial.println("IP obtido: ");
    Serial.println(WiFi.localIP());

    //VERIFICA CONEXAO COM BANCO DE DADOS
    if (conn.connect(server_addr, 3306, user, password) != true)
    {
        while (conn.connect(server_addr, 3306, user, password) != true) {
            Serial.println("DB - Connecting...");
        } 
    }
     
    /*Inicia WebServer*/
    server.begin(); 
    
    delay(1000);
}

void EnviaInformacaWebServer(int umidade)
{

    WiFiClient client = server.available();
    if (!client) {
      return;
    }
  
    while(!client.available()){
      delay(1);
    }
   
    client.print(umidade);
    client.flush();
        
}

float FazLeituraUmidade(void)
{
    int ValorADC;
    float UmidadePercentual;

     ValorADC = analogRead(0);   //978 -> 3,3V
     Serial.print("[Leitura ADC]");
     Serial.println(ValorADC);

     //Quanto maior o numero lido do ADC, menor a umidade.
     //Sendo assim, calcula-se a porcentagem de umidade por:
     //      
     //   Valor lido                 Umidade percentual
     //      _    0                           _ 100
     //      |                                |   
     //      |                                |   
     //      -   ValorADC                     - UmidadePercentual 
     //      |                                |   
     //      |                                |   
     //     _|_  978                         _|_ 0
     //
     //   (UmidadePercentual-0) / (100-0)  =  (ValorADC - 978) / (-978)
     //      Logo:
     //      UmidadePercentual = 100 * ((978-ValorADC) / 978)  
     
     UmidadePercentual = 100 * ((978-(float)ValorADC) / 978);

     
     Serial.print("[Umidade Percentual] ");
     Serial.print(UmidadePercentual);
     Serial.println("%");

     return UmidadePercentual;
}

void VerificaIntervaloEnvio(void)
{      
      /*RETORNANDO TEMPO DE REGISTRO*/
      char query[] = "select tempo_registro from db_soil_analyser.parametros";

      row_values *row = NULL;
      
      // Initiate the query class instance
      MySQL_Cursor *cur_mem = new MySQL_Cursor(&conn);
      
      // Execute the query
      cur_mem->execute(query);
      
      // Fetch the columns (required) but we don't use them.
      column_names *columns = cur_mem->get_columns();
      
      // Read the row (we are only expecting the one)
      do {
      row = cur_mem->get_next_row();
        if (row != NULL) {
        intervaloRegistro = atol(row->values[0]);
        }
      } while (row != NULL);
      
      //Limpa memória
      delete cur_mem;
}

void EnviaInformacoesBanco(int umidade)
{
      /* Verifica o ultimo envio foi maior ou igual ao tempo de intervalo de gravação(Parametro no Sitema) */
      if ((millis() - lastConnectionTime) >= (intervaloRegistro * 60000))
      {
        char INSERT_SQL[100];
       
        sprintf(INSERT_SQL, "insert into db_soil_analyser.umidades(valor, id_sensor) values(%d, 1)", umidade);        
        Serial.println("Gravando umidade...");

       MySQL_Cursor *cur_mem = new MySQL_Cursor(&conn);
       
       // Execute the query
       cur_mem->execute(INSERT_SQL);
       
       // Deleting the cursor also frees up memory used
       delete cur_mem; 

       lastConnectionTime = 0;
       //grava momento do insert, para proxima verificação.               
       lastConnectionTime = millis();
       
      }          
}

int Retornar_Id_Area(void)
{
  int id_area;
  
    /*RETORNANDO ID DA AREA*/
    char query[] = "select areas.id_area from db_soil_analyser.areas where id_sensor = 1";

    row_values *row = NULL;
    
    long head_count = 0;
    
    // Initiate the query class instance
    MySQL_Cursor *cur_mem = new MySQL_Cursor(&conn);
    
    // Execute the query
    cur_mem->execute(query);
    
    // Fetch the columns (required) but we don't use them.
    column_names *columns = cur_mem->get_columns();
    
    // Read the row (we are only expecting the one)
    do {
    row = cur_mem->get_next_row();
      if (row != NULL) {
      head_count = atol(row->values[0]);
      }
    } while (row != NULL);      

    id_area = head_count;
  
    //Limpa memória do cursor
    delete cur_mem;   

    return id_area;
}

void Insere_Irrigacao(int id_area)
{
     char INSERT_SQL[100];
     
     sprintf(INSERT_SQL, "insert into db_soil_analyser.irrigacoes(id_area) values(%d)", id_area);

      if (id_area > 0)
      {
         Serial.println("Inserindo Irrigação...");
  
         MySQL_Cursor *cur_mem = new MySQL_Cursor(&conn);
         
         // Execute the query
         cur_mem->execute(INSERT_SQL);
         
         // Deleting the cursor also frees up memory used
         delete cur_mem; 
       
      }      
}

void Atualiza_Irrigacao(int id_area)
{
       char INSERT_SQL[300];
     
       sprintf(INSERT_SQL, "update db_soil_analyser.irrigacoes set irrigacoes.irrigando = 'N' where irrigacoes.id_area = %d and irrigacoes.data_fim is null", id_area);
 
      if (id_area > 0)
      {
         Serial.println("Finalizando Irrigação...");
  
         MySQL_Cursor *cur_mem = new MySQL_Cursor(&conn);
         
         // Execute the query
         cur_mem->execute(INSERT_SQL);
         
         // Deleting the cursor also frees up memory used
         delete cur_mem; 
       
      }  
}

void Analisa_Umidade(int umidade)
{
      int nivel_ideal;
      int area = Retornar_Id_Area();
      char INSERT_SQL[100];
      char UPDATE_SQL[100];
  
      
      /*RETORNANDO NIVEL IDEAL PARA ESTE SENSOR*/
      char query[] = "select areas.ideal_menor from db_soil_analyser.areas where id_sensor = 1";

      row_values *row = NULL;
      
      long head_count = 0;
      
      // Initiate the query class instance
      MySQL_Cursor *cur_mem = new MySQL_Cursor(&conn);
      
      // Execute the query
      cur_mem->execute(query);
      
      // Fetch the columns (required) but we don't use them.
      column_names *columns = cur_mem->get_columns();
      
      // Read the row (we are only expecting the one)
      do {
      row = cur_mem->get_next_row();
        if (row != NULL) {
        head_count = atol(row->values[0]);
        }
      } while (row != NULL);      

      nivel_ideal = head_count;
      
      //Limpa memória do cursor
      delete cur_mem;


      if ((umidade < nivel_ideal) && (status_rele == 'D'))
      {
        Serial.print("Ideal: ");
        Serial.println(nivel_ideal);
        Serial.print("Umidade lida: ");
        Serial.println(umidade);
                       
         //Liga o rele e muda o status para L
         digitalWrite(rele, LOW);
         status_rele = 'L';

         Insere_Irrigacao(area);
         
         Serial.print("Abaixo do Ideal ...  bomba d'agua LIGADA");

      }
      
      if ((umidade >= nivel_ideal) && (status_rele == 'L'))
      {
        Serial.print("Ideal: ");
        Serial.println(nivel_ideal);
        Serial.print("Umidade lida: ");
        Serial.println(umidade);                
               
         //desliga o rele e muda o status para D
         digitalWrite(rele, HIGH);
         status_rele = 'D';
  
         Atualiza_Irrigacao(area);
  
         Serial.print("Umidade Ideal ... bomba d'agua DESLIGADA");                        
                
      }

      

}


void setup()
{  
    Serial.begin(9600);
    
    lastConnectionTime = 0;
    
    intervaloRegistro = -1;
    
    FazConexaoWiFi();

    pinMode(rele, OUTPUT);
    
    //desliga o rele e muda o status para D
    digitalWrite(rele, HIGH);
    
    status_rele = 'D';
}

//loop principal
void loop()
{
             
    //verifica se está conectado no WiFi
    if(WiFi.status() == WL_CONNECTED)
    {
      Serial.println("Tudo OK com a conexão WIFI!");
      Serial.println(); 

      //conecta com o banco
      while (conn.connect(server_addr, 3306, user, password) != true) {
          Serial.println("DB - Connecting...");
      } 


      float UmidadePercentualLida;
      int   UmidadePercentualTruncada;
      char  FieldUmidade[11];  
  
      UmidadePercentualLida = FazLeituraUmidade();
  
      if (UmidadePercentualLida < 0)
      {
        UmidadePercentualTruncada = 0;
        EnviaInformacaWebServer(UmidadePercentualTruncada);
      }
      else
      {
        UmidadePercentualTruncada = (int)UmidadePercentualLida; //trunca umidade como número inteiro
        EnviaInformacaWebServer(UmidadePercentualTruncada);
      }
      
      sprintf(FieldUmidade,"field1=%d",UmidadePercentualTruncada);

               
      VerificaIntervaloEnvio();
      EnviaInformacoesBanco(UmidadePercentualTruncada);   
      Analisa_Umidade(UmidadePercentualTruncada);                                         
    }
    else
    {
      Serial.println("Falha na conexão com Wifi, reconfigurando...aguarde...");
      Serial.println(); 
      FazConexaoWiFi(); //Caso não esteja conectado, tenta fazer conexão novamente.
    }    

     delay(1000);
}
