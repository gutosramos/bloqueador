#!/usr/bin/env python3
"""
Gera o blocklist.bin usado pelo bloqueador de anuncios ESP8266.

Le uma lista no formato "hosts" (ex: "0.0.0.0 dominio.com" por linha),
calcula um hash FNV-1a de 64 bits pra cada dominio, remove duplicatas,
ordena, e grava tudo como um array binario de inteiros de 8 bytes
(little-endian) -- o que o sketch le com busca binaria direto da flash.

Uso:
    python3 gen_blocklist.py                        # baixa a lista padrao (StevenBlack/hosts)
    python3 gen_blocklist.py minha_lista.txt         # usa um arquivo hosts local
    python3 gen_blocklist.py minha_lista.txt saida.bin

Depois de gerar, copie o blocklist.bin para a pasta esp8266/data/
antes de fazer o upload do filesystem (LittleFS) pro ESP8266.
"""
import struct
import sys
import urllib.request

DEFAULT_URL = "https://raw.githubusercontent.com/StevenBlack/hosts/master/hosts"

IGNORED_DOMAINS = {
    "localhost",
    "localhost.localdomain",
    "local",
    "broadcasthost",
    "ip6-localhost",
    "ip6-loopback",
}


def fnv1a64(s: str) -> int:
    h = 0xCBF29CE484222325
    for b in s.encode("ascii", errors="ignore"):
        h ^= b
        h = (h * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return h


def extract_domains(lines):
    domains = set()
    for line in lines:
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split()
        if len(parts) < 2:
            continue
        ip = parts[0]
        if ip not in ("0.0.0.0", "127.0.0.1"):
            continue
        domain = parts[1].strip().lower()
        if domain in IGNORED_DOMAINS:
            continue
        domains.add(domain)
    return domains


def main():
    args = [a for a in sys.argv[1:]]

    if args and args[0] not in ("-", "--download"):
        src_path = args[0]
        with open(src_path, "r", encoding="utf-8", errors="ignore") as f:
            lines = f.readlines()
        print(f"Lendo lista local: {src_path}")
    else:
        print(f"Baixando lista de: {DEFAULT_URL}")
        with urllib.request.urlopen(DEFAULT_URL) as resp:
            lines = resp.read().decode("utf-8", errors="ignore").splitlines()

    out_path = args[1] if len(args) >= 2 else "blocklist.bin"

    domains = extract_domains(lines)
    print(f"{len(domains)} dominios encontrados na lista")

    hashes = sorted({fnv1a64(d) for d in domains})
    collisions = len(domains) - len(hashes)
    print(f"{len(hashes)} hashes unicos ({collisions} colisoes removidas)")

    with open(out_path, "wb") as f:
        for h in hashes:
            f.write(struct.pack("<Q", h))

    size_kb = (len(hashes) * 8) / 1024
    print(f"\nGravado em: {out_path} ({size_kb:.1f} KB)")
    print("Copie esse arquivo pra pasta esp8266/data/ e faca o upload do")
    print("filesystem (LittleFS) pro ESP8266 antes de gravar o sketch.")


if __name__ == "__main__":
    main()
