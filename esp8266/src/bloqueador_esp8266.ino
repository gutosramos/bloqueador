// Bloqueador de Anuncios DNS para ESP8266 (Wemos/Lolin D1 Mini e compativeis)
//
// Funciona como um DNS sinkhole: consultas para dominios da lista de
// bloqueio recebem uma resposta "vazia" (0.0.0.0); todo o resto e
// encaminhado normalmente para um DNS de verdade (upstream).
//
// A lista de bloqueio fica em /blocklist.bin na flash (LittleFS), como
// um array ordenado de hashes de 64 bits (8 bytes cada), e a busca e
// feita por busca binaria direto na flash — nao precisa caber tudo na
// RAM, entao da pra ter uma lista bem maior do que a RAM permitiria
// sozinha. Gere esse arquivo com tools/gen_blocklist.py.
//
// Bibliotecas necessarias (todas ja vem com o core ESP8266 do Arduino):
//   ESP8266WiFi, ESP8266WebServer, WiFiUdp, LittleFS

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <LittleFS.h>
#include "secrets.h" // copie secrets.h.example -> secrets.h e preencha

// ==================== CONFIGURACAO — AJUSTE EM secrets.h ====================

IPAddress LOCAL_IP(LOCAL_IP_A, LOCAL_IP_B, LOCAL_IP_C, LOCAL_IP_D);
IPAddress GATEWAY(GATEWAY_A, GATEWAY_B, GATEWAY_C, GATEWAY_D);
IPAddress SUBNET(SUBNET_A, SUBNET_B, SUBNET_C, SUBNET_D);

// DNS "de verdade" pra onde mandamos tudo que nao esta bloqueado.
IPAddress UPSTREAM_DNS(1, 1, 1, 1); // Cloudflare; troque por 8.8.8.8 (Google) se preferir

// IP devolvido para dominios bloqueados (0.0.0.0 = sinkhole padrao)
IPAddress SINKHOLE_IP(0, 0, 0, 0);

const char* BLOCKLIST_FILE = "/blocklist.bin"; // gerado pelo gen_blocklist.py
const uint16_t DNS_PORT = 53;
const unsigned long UPSTREAM_TIMEOUT_MS = 3000;

// ======================================================================

WiFiUDP dnsServer;
WiFiUDP upstreamClient;
ESP8266WebServer webServer(80);
File blocklistFile;

uint32_t blocklistCount = 0;
uint32_t queryCount = 0;
uint32_t blockedCount = 0;

// ---- hash FNV-1a 64 bits (tem que bater com o gen_blocklist.py) ----
uint64_t fnv1a64(const char* s) {
  uint64_t hash = 14695981039346656037ULL;
  while (*s) {
    hash ^= (uint8_t)(*s++);
    hash *= 1099511628211ULL;
  }
  return hash;
}

void toLowerInPlace(char* s) {
  for (; *s; s++) {
    if (*s >= 'A' && *s <= 'Z') *s += 32;
  }
}

// Busca binaria no arquivo de hashes ordenado (8 bytes por entrada, little-endian)
bool isBlocked(const char* domain) {
  if (blocklistCount == 0 || !blocklistFile) return false;

  char buf[256];
  strncpy(buf, domain, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = 0;
  toLowerInPlace(buf);
  uint64_t target = fnv1a64(buf);

  int32_t lo = 0, hi = (int32_t)blocklistCount - 1;
  uint8_t rec[8];
  while (lo <= hi) {
    int32_t mid = lo + (hi - lo) / 2;
    blocklistFile.seek((uint32_t)mid * 8, SeekSet);
    blocklistFile.read(rec, 8);
    uint64_t val = 0;
    for (int i = 7; i >= 0; i--) val = (val << 8) | rec[i];
    if (val == target) return true;
    if (val < target) lo = mid + 1; else hi = mid - 1;
  }
  return false;
}

// Le o QNAME a partir do byte 12 do pacote DNS (formato: [tamanho]rotulo...[0]).
// Devolve o tamanho da secao de pergunta inteira (QNAME+QTYPE+QCLASS), ou 0 se invalido.
int parseQuestionName(const uint8_t* pkt, int len, char* outName, int outNameSize) {
  int pos = 12;
  int outPos = 0;
  bool first = true;
  while (pos < len) {
    uint8_t labelLen = pkt[pos];
    if (labelLen == 0) { pos++; break; }
    if ((labelLen & 0xC0) == 0xC0) return 0; // nao esperamos compressao na pergunta
    pos++;
    if (pos + labelLen > len) return 0;
    if (!first && outPos < outNameSize - 1) outName[outPos++] = '.';
    first = false;
    for (int i = 0; i < labelLen && outPos < outNameSize - 1; i++) {
      outName[outPos++] = (char)pkt[pos + i];
    }
    pos += labelLen;
  }
  outName[outPos] = 0;
  pos += 4; // QTYPE (2) + QCLASS (2)
  if (pos - 12 < 5 || pos > len) return 0;
  return pos - 12;
}

void sendBlockedResponse(const uint8_t* query, int qEnd, IPAddress clientIP, uint16_t clientPort, uint8_t qtype) {
  uint8_t resp[300];
  int rlen = 0;

  resp[0] = query[0]; resp[1] = query[1]; // ID (mesmo da pergunta)
  resp[2] = 0x81; resp[3] = 0x80;         // QR=1 RD=1 RA=1, resto 0 (NOERROR por padrao)
  resp[4] = query[4]; resp[5] = query[5]; // QDCOUNT (mesmo da pergunta)
  resp[8] = 0x00; resp[9] = 0x00;         // NSCOUNT = 0
  resp[10] = 0x00; resp[11] = 0x00;       // ARCOUNT = 0
  rlen = 12;

  int qsecLen = qEnd - 12;
  memcpy(resp + rlen, query + 12, qsecLen);
  rlen += qsecLen;

  if (qtype == 1) { // tipo A: devolve 0.0.0.0
    resp[6] = 0x00; resp[7] = 0x01; // ANCOUNT = 1
    resp[rlen++] = 0xC0; resp[rlen++] = 0x0C; // nome comprimido apontando pro offset 12
    resp[rlen++] = 0x00; resp[rlen++] = 0x01; // TYPE A
    resp[rlen++] = 0x00; resp[rlen++] = 0x01; // CLASS IN
    resp[rlen++] = 0x00; resp[rlen++] = 0x00; resp[rlen++] = 0x00; resp[rlen++] = 0x3C; // TTL 60s
    resp[rlen++] = 0x00; resp[rlen++] = 0x04; // RDLENGTH 4
    resp[rlen++] = SINKHOLE_IP[0]; resp[rlen++] = SINKHOLE_IP[1];
    resp[rlen++] = SINKHOLE_IP[2]; resp[rlen++] = SINKHOLE_IP[3];
  } else {
    // Qualquer outro tipo (AAAA, etc.): NXDOMAIN, sem resposta
    resp[6] = 0x00; resp[7] = 0x00; // ANCOUNT = 0
    resp[3] = 0x83;                 // RCODE = 3 (NXDOMAIN)
  }

  dnsServer.beginPacket(clientIP, clientPort);
  dnsServer.write(resp, rlen);
  dnsServer.endPacket();
}

void forwardToUpstream(const uint8_t* query, int len, IPAddress clientIP, uint16_t clientPort) {
  upstreamClient.beginPacket(UPSTREAM_DNS, 53);
  upstreamClient.write(query, len);
  upstreamClient.endPacket();

  unsigned long start = millis();
  while (millis() - start < UPSTREAM_TIMEOUT_MS) {
    int respLen = upstreamClient.parsePacket();
    if (respLen > 0) {
      uint8_t resp[512];
      if (respLen > (int)sizeof(resp)) respLen = sizeof(resp);
      upstreamClient.read(resp, respLen);
      dnsServer.beginPacket(clientIP, clientPort);
      dnsServer.write(resp, respLen);
      dnsServer.endPacket();
      return;
    }
    delay(1);
  }
  // timeout: nao responde nada; o cliente tenta de novo ou desiste sozinho
}

void handleStatus() {
  String html = "<html><head><meta charset='utf-8'>"
                 "<title>Bloqueador de Anuncios</title></head><body>"
                 "<h1>Bloqueador de Anuncios (ESP8266)</h1><ul>";
  html += "<li>IP: " + WiFi.localIP().toString() + "</li>";
  html += "<li>Dominios na lista: " + String(blocklistCount) + "</li>";
  html += "<li>Consultas totais: " + String(queryCount) + "</li>";
  html += "<li>Bloqueadas: " + String(blockedCount) + "</li>";
  html += "<li>Uptime: " + String(millis() / 1000) + "s</li>";
  html += "<li>Memoria livre: " + String(ESP.getFreeHeap()) + " bytes</li>";
  html += "</ul></body></html>";
  webServer.send(200, "text/html; charset=utf-8", html);
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== Bloqueador de Anuncios ESP8266 ===");

  if (!LittleFS.begin()) {
    Serial.println("ERRO: falha ao montar LittleFS.");
    Serial.println("Rode 'ESP8266 LittleFS Data Upload' (ou equivalente) antes de continuar.");
  } else {
    blocklistFile = LittleFS.open(BLOCKLIST_FILE, "r");
    if (blocklistFile) {
      blocklistCount = blocklistFile.size() / 8;
      Serial.printf("Lista de bloqueio carregada: %u dominios\n", blocklistCount);
    } else {
      Serial.println("AVISO: blocklist.bin nao encontrado. Nada sera bloqueado por enquanto.");
    }
  }

  WiFi.mode(WIFI_STA);
  WiFi.config(LOCAL_IP, GATEWAY, SUBNET);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Conectando ao Wi-Fi");

  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    // Depois de 30s sem conectar, reinicia o ESP e tenta de novo do zero
    // (em vez de ficar preso pra sempre -- util se o roteador reiniciar
    // ou o sinal cair momentaneamente durante o boot).
    if (millis() - wifiStart > 30000) {
      Serial.println("\nNao conectou em 30s, reiniciando...");
      ESP.restart();
    }
  }
  Serial.println();
  Serial.print("Conectado! IP: ");
  Serial.println(WiFi.localIP());

  dnsServer.begin(DNS_PORT);
  upstreamClient.begin(0); // porta local qualquer

  webServer.on("/", handleStatus);
  webServer.begin();

  Serial.println("Servidor DNS ativo na porta 53. Painel de status em http://<ip>/");
}

void loop() {
  // Se cair o Wi-Fi em algum momento, reinicia sozinho depois de 30s
  // desconectado em vez de ficar servindo DNS sem rede.
  static unsigned long disconnectedSince = 0;
  if (WiFi.status() != WL_CONNECTED) {
    if (disconnectedSince == 0) disconnectedSince = millis();
    else if (millis() - disconnectedSince > 30000) ESP.restart();
  } else {
    disconnectedSince = 0;
  }

  webServer.handleClient();

  int len = dnsServer.parsePacket();
  if (len <= 0) return;

  uint8_t query[512];
  if (len > (int)sizeof(query)) {
    // pacote maior que o esperado: descarta
    uint8_t dump[512];
    while (len > 0) { int n = dnsServer.read(dump, sizeof(dump)); if (n <= 0) break; len -= n; }
    return;
  }
  dnsServer.read(query, len);

  if (len < 12) return; // pacote invalido, ignora

  IPAddress clientIP = dnsServer.remoteIP();
  uint16_t clientPort = dnsServer.remotePort();

  char domain[256];
  int qsecLen = parseQuestionName(query, len, domain, sizeof(domain));
  if (qsecLen <= 0) {
    forwardToUpstream(query, len, clientIP, clientPort);
    return;
  }

  int qtypeOffset = 12 + (qsecLen - 4); // QTYPE = 2 bytes antes do fim da secao
  uint8_t qtype = query[qtypeOffset + 1];

  queryCount++;

  if (isBlocked(domain)) {
    blockedCount++;
    Serial.printf("[BLOQUEADO] %s\n", domain);
    sendBlockedResponse(query, 12 + qsecLen, clientIP, clientPort, qtype);
  } else {
    forwardToUpstream(query, len, clientIP, clientPort);
  }
}
